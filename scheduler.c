#include "scheduler.h"

simgrid_execs_t read_workflow(char *file_path)
{
    simgrid_execs_t execs;
    for (auto &activity : simgrid::s4u::create_DAG_from_dot(file_path))
        if (auto *exec = dynamic_cast<simgrid::s4u::Exec *>(activity.get()))
            execs.push_back((simgrid_exec_t *)exec);

    /* DELETE ENTRY/EXIT TASK */
    // TODO: The last tasks still have end as dependency.
    //  We need to remove this
    simgrid_exec_t *root = execs.front();

    for (const auto &succ_ptr : root->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);
    root->complete(simgrid::s4u::Activity::State::FINISHED);
    execs.erase(execs.begin());

    // simgrid_exec_t *end = execs.back();
    // for (const auto &ances_ptr : end->get_dependencies())
    //     (ances_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);
    // end->complete(simgrid::s4u::Activity::State::FINISHED);
    execs.pop_back();

    return execs;
}

simgrid_execs_t get_ready_execs(const simgrid_execs_t &execs)
{
    simgrid_execs_t ready_execs;
    for (auto &exec : execs)
        if (exec->dependencies_solved() && not exec->is_assigned())
            ready_execs.push_back(exec);

    return ready_execs;
}

exec_hwloc_core_id_completion_time_t get_min_exec(const common_data_t *common_data, simgrid_execs_t &execs)
{
    simgrid_exec_t *selected_exec = nullptr;
    int selected_hwloc_core_id = -1;
    double min_expected_completion_time = std::numeric_limits<double>::max();

    for (simgrid_exec_t *exec : execs)
    {
        int hwloc_core_id;
        double finish_time;

        std::tie(hwloc_core_id, finish_time) = get_best_hwloc_core_id(common_data, exec);

        if (finish_time < min_expected_completion_time)
        {
            min_expected_completion_time = finish_time;
            selected_exec = exec;
            selected_hwloc_core_id = hwloc_core_id;
        }
    }

    return std::make_tuple(selected_exec, selected_hwloc_core_id, min_expected_completion_time);
}

/**
 * @brief Find the best core to execute a given workflow task.
 *
 * The min-min scheduling algorithm operates by selecting tasks based
 * on their "minimum expected completion time" across all available resources
 * and then assigning each selected task to the resource that can execute it
 * the fastest.
 *
 * This approach typically results in faster tasks being scheduled earlier,
 * leaving longer tasks for later assignment. The algorithm is straightforward
 * but can suffer from load imbalance if there are tasks with significantly
 * different exeution times.
 *
 * The completion time of any task `T_i` is estimated as:
 * \f[
 * T_i = \text{start time} + \max(\text{read time}) + \text{compute time} + \max(\text{write time})
 * \f]
 *
 * ### Key Assumptions:
 * - Reads and writes are performed in parallel. However, in practice, the emulation performs them sequentially.
 * - The task's start time depends on the completion time of its predecessor and the maximum read/write completion time.
 * - Write time is calculated assuming the farthest NUMA node handles the write, representing the worst-case latency.
 *
 * ### Parameters:
 * @param common_data A pointer to the global data structure containing information such as latency and bandwidth
 * matrices.
 * @param exec The task for which the best hardware core is being selected.
 * @return A pair of:
 * - `best_hwloc_core_id` (int): The ID of the selected core.
 * - `min_expected_completion_time` (double): The estimated minimum completion time for the task on the selected core.
 *
 * ### Notes:
 * - This function assumes task dependencies are resolved before execution.
 * - Control dependencies with size = -1 or 0, as defined in SimGrid, are not handled.
 * - Units of bandwidth and core speed should be consistent (e.g., FLOPs and Gbps).
 */
hwloc_core_id_completion_time_t get_best_hwloc_core_id(const common_data_t *common_data, const simgrid_exec_t *exec)
{
    int best_hwloc_core_id = -1;
    double min_expected_completion_time = std::numeric_limits<double>::max();

    // Estimate exec completion time for every hwloc_core_id.
    for (int hwloc_core_id : get_hwloc_core_ids_by_availability(*(common_data->hwloc_core_ids_availability), true))
    {
        // Match all communication names (Task1->Task2) where this task_name is the destination.
        comm_name_time_ranges_t matched_comms = find_matching_time_ranges(common_data->comm_name_to_write_time, exec->get_name(), SECOND);

        uint64_t estimated_start_time_us = 0;
        double max_estimated_time_from_read_operations_us = 0.0;

        for (const auto &[comm_name, _start_time, _end_time, bytes_to_read] : matched_comms)
        {
            /* 1. ESTIMATE EXEC START TIME. */
            auto [parent_exec_name, self_exec_name] = split_by_arrow(std::string(comm_name));
            uint64_t prev_exec_finish_time = std::get<1>((*common_data->exec_name_to_time)[parent_exec_name]);
            if (prev_exec_finish_time > estimated_start_time_us)
                estimated_start_time_us = prev_exec_finish_time;

            /* 2. ESTIMATE EXEC READ TIME
            The start time of the current exec (task) will be
            start_time = estimated_start_time_us + read_max_estimated_time;
            */

            char *read_buffer = (*common_data->comm_name_to_ptr)[comm_name];

            // Get NUMA node where data is allocated.
            int hwloc_read_src_numa_id = get_hwloc_numa_id_from_ptr(common_data->topology, read_buffer, (size_t)bytes_to_read);
            // Get NUMA node of the code that will perform the reading operation.
            int hwloc_read_dst_numa_id = get_hwloc_numa_id_from_hwloc_core_id(common_data->topology, hwloc_core_id);

            double read_latency = (*(common_data->latency_matrix))[hwloc_read_src_numa_id][hwloc_read_dst_numa_id];
            double read_bandwidth = (*(common_data->latency_matrix))[hwloc_read_src_numa_id][hwloc_read_dst_numa_id];

            // TODO: Need to check bandwidth and latency units.
            double estimated_read_time = (bytes_to_read / read_bandwidth) + read_latency;

            if (estimated_read_time > max_estimated_time_from_read_operations_us)
                max_estimated_time_from_read_operations_us = estimated_read_time;
        }

        /* 3. ESTIMATE COMPUTE TIME. */

        // TODO: Check units of core speed (it must be in FLOPS).
        double estimated_compute_time = exec->get_remaining() / get_current_core_speed_from_hwloc_core_id(common_data->topology, hwloc_core_id);

        /* 4. MAXIMUM DATA WRITE TIME (ESTIMATED).
        The finish time of the task is bound by the write operation taking the most.
        */

        // ASSUMPTION:
        // Since it is uncertain which NUMA node will handle the write operations for this task,
        // the worst-case scenario is assumed: the data is written to the farthest NUMA node,
        // i.e., the one with the highest latency relative to the specified hwloc core ID.
        int hwloc_write_src_numa_id = get_hwloc_numa_id_from_hwloc_core_id(common_data->topology, hwloc_core_id);
        int hwloc_write_dst_numa_id = -1;
        double max_write_latency = -1;

        for (size_t i = 0; i < (*(common_data->latency_matrix))[0].size(); ++i)
        {
            double latency = (*(common_data->latency_matrix))[hwloc_write_src_numa_id][i];
            if (latency > max_write_latency)
            {
                max_write_latency = latency;
                hwloc_write_dst_numa_id = i;
            }
        }

        if (hwloc_write_src_numa_id < 0 || hwloc_write_dst_numa_id < 0)
        {
            fprintf(stderr, "Error: invalid index hwloc_write_src_numa_id: '%d', hwloc_write_dst_numa_id: '%d'.\n", hwloc_write_src_numa_id, hwloc_write_dst_numa_id);
            std::exit(EXIT_FAILURE); // Exit with failure status
        }

        double write_latency = (*(common_data->latency_matrix))[hwloc_write_src_numa_id][hwloc_write_dst_numa_id];
        double write_bandwidth = (*(common_data->bandwdith_matrix))[hwloc_write_src_numa_id][hwloc_write_dst_numa_id];

        double max_estimated_time_from_write_operations_us = 0.0;
        for (const auto &parent : exec->get_successors())
        {
            const simgrid_comm_t *out_comm = dynamic_cast<simgrid_comm_t *>(parent.get());
            double bytes_to_write = out_comm->get_remaining();

            // TODO: Need to check bandwidth and latency units.
            // Bandwidth can be defined in Gbips or GBytes/s.
            double write_time = (bytes_to_write / write_bandwidth) + write_latency;

            if (write_time > max_estimated_time_from_write_operations_us)
                max_estimated_time_from_write_operations_us = write_time;
        }

        double estimated_completion_time_for_hwloc_core_id = estimated_start_time_us + max_estimated_time_from_read_operations_us + estimated_compute_time + max_estimated_time_from_write_operations_us;

        if (estimated_completion_time_for_hwloc_core_id < min_expected_completion_time)
        {
            min_expected_completion_time = estimated_completion_time_for_hwloc_core_id;
            best_hwloc_core_id = hwloc_core_id;
        }
    }

    return std::make_pair(best_hwloc_core_id, min_expected_completion_time);
}

hwloc_core_ids_t get_hwloc_core_ids_by_availability(const hwloc_core_ids_availability_t &hwloc_core_ids_availability, bool available)
{
    hwloc_core_ids_t available_cores;
    for (size_t i = 0; i < hwloc_core_ids_availability.size(); ++i)
        if (hwloc_core_ids_availability[i] == available)
            available_cores.push_back(static_cast<int>(i));

    return available_cores;
}

int assign_exec(exec_data_t *exec_data, simgrid_host_t *dummy_host)
{
    int pu;
    pthread_t thread;
    pthread_attr_t attr;

    cpu_set_t cpuset;

    hwloc_obj_t core;
    hwloc_cpuset_t hwloc_cpuset;

    // Initialize thread attributes
    pthread_attr_init(&attr);

    // Retrieve the core object based on the core index
    core = hwloc_get_obj_by_type(*(exec_data->common_data->topology), HWLOC_OBJ_CORE, exec_data->assigned_hwloc_core_id);
    if (!core)
    {
        fprintf(stderr, "Core %d not found!\n", exec_data->assigned_hwloc_core_id);
        std::exit(EXIT_FAILURE); // Exit with failure status
    }

    // Get the cpuset of the core (includes all PUs associated with the core)
    hwloc_cpuset = hwloc_bitmap_dup(core->cpuset);

    // Remove hyperthreads (keep only the first PU in each core)
    hwloc_bitmap_singlify(hwloc_cpuset);

    // Clear the CPU set
    CPU_ZERO(&cpuset);

    // Convert hwloc cpuset to Linux cpu_set_t using glibc function
    // Manually iterate over the cpuset and set the corresponding bits in the Linux CPU set
    for (pu = hwloc_bitmap_first(hwloc_cpuset); pu != -1; pu = hwloc_bitmap_next(hwloc_cpuset, pu))
    {
        CPU_SET(pu, &cpuset); // Set the corresponding processing unit in the CPU set
    }

    // Free the hwloc cpuset object as it's no longer needed
    hwloc_bitmap_free(hwloc_cpuset);

    // Set thread affinity
    int ret = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
    if (ret != 0)
    {
        perror("pthread_attr_setaffinity_np");
        std::exit(EXIT_FAILURE); // Exit with failure status
    }

    exec_data->exec->set_host(dummy_host);

    // Mark the selected hwloc_core_id as unavailable (the thread will release it upon completion).
    update_hwloc_core_availability(exec_data->common_data, exec_data->assigned_hwloc_core_id, false);

    if (pthread_create(&thread, &attr, thread_function, exec_data) == 0)
    {
        // Increment the active thread counter (inside mutex)
        update_active_threads_counter(exec_data->common_data, 1);
    }
    else
    {
        fprintf(stderr, "Error: Unable to create thread for '%s'.\n", exec_data->exec->get_cname());
        std::exit(EXIT_FAILURE); // Exit with failure status
    }

    // Destroy the attributes object since it's no longer needed.
    // Destroy the pthread_attr_t immediately after creating the thread:
    // This is safe because the thread's attributes are copied into the new
    // thread when pthread_create is called. After that, the pthread_attr_t
    // object is no longer required.

    // You do not need to wait for the thread to finish in order to call pthread_attr_destroy.
    // The attributes object can be destroyed right after thread creation without affecting the thread itself.
    pthread_attr_destroy(&attr);

    // Optionally, join or detach the thread depending on your needs
    pthread_detach(thread); // or pthread_join(thread, NULL);

    // Clean up
    // exec_data is freed in thread_function.

    return 0;
}

/**
 * @brief Emulate the activities involved in executing a workflow task.
 *
 * 1. Read the required data from memory by accessing a previously created address.
 *    Reads are performed sequentially. But the time is taken they were performed in parallel.
 *
 * 2. Perform the FMA (Fused Multiply-Add) operation to emulate computation.
 *
 * 3. Write the required data to memory, creating a pointer to be accessed by successor tasks.
 *    Writes are performed sequentially. But the time is taken they were performed in parallel.
 *
 * 4. Record read, write, and execution times.
 *
 * 5. Save thread locality information, including the last NUMA node, core ID, and context switches.
 *
 * @param arg Structure used to collect thread execution data.
 * @return void*
 *
 * @note This method does not support SimGrid control dependencies (0, -1).
 */
void *thread_function(void *arg)
{
    exec_data_t *data = (exec_data_t *)arg;

    printf("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d: started.\n", getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_from_os_pu_id(data->common_data->topology, sched_getcpu()));
    
    std::string mem_policy = get_hwloc_thread_mem_policy(data->common_data->topology);
    printf("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => %s.\n", getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_from_os_pu_id(data->common_data->topology, sched_getcpu()), mem_policy.c_str());

    /* 1. READ FROM MEMORY. */

    // Retrieve writings (dependencies) where this execution is the destination.
    comm_name_time_ranges_t matched_comms = find_matching_time_ranges(data->common_data->comm_name_to_write_time, data->exec->get_name(), SECOND);

    // TODO: Investigate if parallel reads are possible when accessing `matched_comms` entries.
    uint64_t max_time_from_read_operations_us = 0;
    std::vector<std::string> dependency_names;
    for (const auto &[comm_name, _start_time, _end_time, bytes_to_read] : matched_comms)
    {
        if (!data->common_data->comm_name_to_ptr->contains(comm_name))
        {
            fprintf(stderr, "Process ID: %d, Thread ID: %d, Task ID: %s => dependency (%s): unable to read,  pointer not found\n", getpid(), gettid(), data->exec->get_cname(), comm_name.c_str());
            return NULL;
        }

        auto [left, right] = split_by_arrow(std::string(comm_name));
        dependency_names.push_back(left);

        // Emulate memory reading by accessing the buffer.
        // TODO: Achieving maximum throughput depends on code efficiency.
        char *read_buffer = (*data->common_data->comm_name_to_ptr)[comm_name];

        uint64_t read_start_time_us = get_time_us();

        size_t sum = 0;
        for (size_t i = 0; i < bytes_to_read; i++)
        {
            sum += read_buffer[i]; // Access each byte in the buffer (simulates reading)
        }

        uint64_t read_end_time_us = get_time_us();

        // Compute read time, assuming reads are carried out in parallel.
        // The total read time is determined by the longest individual read time.
        uint64_t read_time_us = read_end_time_us - read_start_time_us;
        if (read_time_us > max_time_from_read_operations_us)
            max_time_from_read_operations_us = read_time_us;

        // Save read time.
        (*data->common_data->comm_name_to_read_time)[comm_name] = time_range_t{read_start_time_us, read_end_time_us, bytes_to_read};

        // Clean up.
        free(read_buffer);

        printf("Process ID: %d, Thread ID: %d, Task ID: %s => dependency (%s): read %zu bytes from memory, sum (%ld)\n", getpid(), gettid(), data->exec->get_cname(), left.c_str(), bytes_to_read, sum);
    }

    /* 2. PERFORM CPU INTENSIVE COMPUTATION. */

    int64_t flops = (int64_t)data->exec->get_remaining();

    // The volatile keyword is used to prevent the compiler from optimizing away the floating-point operations.
    volatile double a = 1.0, b = 2.0, c = 0.0;

    uint64_t exec_start_time_us = get_time_us();

    // FMA (Fused Multiply-Add) operation. This is a common pattern in scientific computing
    // benchmarks (e.g., LINPACK)
    for (int64_t i = 0; i < flops; ++i)
        c = a * b + c;

    uint64_t exec_end_time_us = get_time_us();

    // Calculate the compute time in microseconds.
    uint64_t compute_time_us = exec_end_time_us - exec_start_time_us;

    // Save compute time.
    (*data->common_data->exec_name_to_compute_time)[data->exec->get_cname()] = time_range_t{exec_start_time_us, exec_end_time_us, flops};

    /* 3. WRITE TO MEMORY. */

    uint64_t max_time_from_write_operations_us = 0;
    for (const auto &succ_ptr : data->exec->get_successors())
    {
        const simgrid_activity_t *succ = succ_ptr.get();
        auto [exec_name, succ_exec_name] = split_by_arrow(std::string{succ->get_cname()});

        if (succ_exec_name != std::string{"end"})
        {
            size_t bytes_to_write = (size_t)succ->get_remaining();

            // Emulate memory writting by saving data into memory.
            char *write_buffer = (char *)malloc(bytes_to_write);

            if (!write_buffer)
            {
                fprintf(stderr, "Process ID: %d, Thread ID: %d, Task ID: %s => successor (%s): unable to create write buffer.\n", getpid(), gettid(), data->exec->get_cname(), succ->get_cname());

                return NULL;
            }

            /*
            memset:
            Fills a block of memory with a specified value, typically used to initialize memory, such as setting
            all elements of an array to zero or preparing data structures.

            Parameters:
                ptr   - Pointer to the memory block to fill.
                value - The value to set, interpreted as an unsigned char (0-255). Although passed as an int,
                        the value is converted to an unsigned char to fill the memory block.
                num   - The number of bytes to set in the memory block.

            Description:
            memset sets each byte of the memory block starting at ptr to the given value. When an integer is
            provided, it is truncated to the least significant 8 bits (one byte), and this byte is used to
            fill the entire block.
            */

            uint64_t write_start_time_us = get_time_us();
            memset(write_buffer, 0, bytes_to_write);
            uint64_t write_end_time_us = get_time_us();

            // Compute write time, assuming reads are carried out in parallel.
            // The total read time is determined by the longest individual read time.
            uint64_t write_time_us = write_end_time_us - write_start_time_us;
            if (write_time_us > max_time_from_write_operations_us)
                max_time_from_write_operations_us = write_time_us;

            // Save write time.
            (*data->common_data->comm_name_to_write_time)[succ->get_cname()] = time_range_t{write_start_time_us, write_end_time_us, bytes_to_write};

            // Save address for subsequent reading.
            (*data->common_data->comm_name_to_ptr)[succ->get_cname()] = write_buffer;

            printf("Process ID: %d, Thread ID: %d, Task ID: %s => wrote %zu bytes to memory\n", getpid(), gettid(), data->exec->get_cname(), bytes_to_write);
        }
        else
        {
            printf("Process ID: %d, Thread ID: %d, Task ID: %s => skipt writing operation for end task\n", getpid(), gettid(), data->exec->get_cname());
        }
    }

    /* 5. SAVE EXEC START TIME, AND COMPLETION TIME */
    uint64_t start_time_us = 0;
    for (auto &prev_exec_name : dependency_names)
    {
        uint64_t prev_exec_finish_time = std::get<1>((*data->common_data->exec_name_to_time)[prev_exec_name]);
        if (prev_exec_finish_time > start_time_us)
            start_time_us = prev_exec_finish_time;
    }

    uint64_t finish_time_us = start_time_us + max_time_from_read_operations_us + compute_time_us + max_time_from_write_operations_us;

    (*data->common_data->exec_name_to_time)[data->exec->get_cname()] = time_interval_t{start_time_us, finish_time_us};

    /* 5. SAVE DATA LOCALITY INFO. */
    for (const auto &succ_ptr : data->exec->get_successors())
    {
        const simgrid_activity_t *succ = succ_ptr.get();
        auto [exec_name, succ_exec_name] = split_by_arrow(std::string{succ->get_cname()});

        if (succ_exec_name != std::string{"end"})
        {
            char *address = (*data->common_data->comm_name_to_ptr)[succ->get_cname()];
            size_t size = (size_t)succ->get_remaining();
            (*data->common_data->comm_name_to_numa_id)[succ->get_cname()] = get_hwloc_numa_id_from_ptr(data->common_data->topology, address, size);
        }
    }

    (*data->common_data->exec_name_to_locality)[data->exec->get_cname()] = get_hwloc_locality_info(data->common_data->topology);

    printf("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d: finished.\n", getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_from_os_pu_id(data->common_data->topology, sched_getcpu()));

    /* 7. SET EXEC AS COMPLETED. */
    for (const auto &succ_ptr : data->exec->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);
    data->exec->complete(simgrid::s4u::Activity::State::FINISHED);

    /* 8. CLEAN UP */

    // Decrement the active thread counter and signal if no more threads
    update_active_threads_counter(data->common_data, -1);
    // Mark the selected hwloc_core_id as available.
    update_hwloc_core_availability(data->common_data, data->assigned_hwloc_core_id, true);
    // this pointer was created in the thread caller 'assign_exec'
    free(data);

    return NULL;
}

void update_active_threads_counter(const common_data_t *common_data, int value)
{
    pthread_mutex_lock(common_data->mutex);
    (*(common_data->active_threads)) += value;
    if (((*common_data->active_threads)) == 0)
        pthread_cond_signal(common_data->cond); // Signal the main thread
    pthread_mutex_unlock(common_data->mutex);
}

void update_hwloc_core_availability(const common_data_t *common_data, int hwloc_core_id, bool availability)
{
    (*(common_data->hwloc_core_ids_availability))[hwloc_core_id] = availability;
}

comm_name_time_ranges_t find_matching_time_ranges(const comm_name_to_time_t *map, const std::string &name, MatchPart partToMatch)
{
    comm_name_time_ranges_t matches;

    for (const auto &entry : *map)
    {
        std::string key = entry.first; // Convert to std::string for easy manipulation
        const time_range_t &timeRange = entry.second;

        auto [firstName, secondName] = split_by_arrow(key);

        // Check if the specified part matches the name
        if ((partToMatch == FIRST && firstName == name) || (partToMatch == SECOND && secondName == name))
        {
            matches.push_back(std::make_tuple(key.c_str(), std::get<0>(timeRange), std::get<1>(timeRange), std::get<2>(timeRange)));
        }
    }
    return matches;
}

void read_matrix_from_file(const std::string &filename, std::vector<std::vector<double>> &matrix)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    int n;
    file >> n; // Read matrix dimensions from the first line

    // Resize the matrix based on dimensions
    matrix.resize(n, std::vector<double>(n));

    // Read matrix values
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            file >> matrix[i][j];
        }
    }
    file.close();
}

void print_workflow(const simgrid_execs_t &execs)
{
    std::cout << "workflow_details: \n";
    for (const auto &exec : execs)
    {
        std::cout << "  Name: " << exec->get_cname() << ", Size: " << exec->get_remaining() << ", Dependencies: " << exec->get_dependencies().size() << ", Successors: " << exec->get_successors().size() << "\n";
    }
    std::cout << std::endl;
}

void print_matrix(const matrix_t &matrix, const std::string &label)
{
    std::cout << label << ":\n";
    for (const auto &row : matrix)
    {
        for (const auto &value : row)
        {
            std::cout << std::setw(10) << value << " ";
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

// Function to print comm_name_to_numa_id mapping
void print_comm_name_to_numa_id(const comm_name_to_numa_id_t &mapping, std::ostream &out)
{
    out << "comm_name_to_numa_id:\n";
    for (const auto &[key, value] : mapping)
    {
        out << "  " << key << ": NUMA ID = " << value << "\n";
    }
    out << std::endl;
}

// Function to print exec_name_to_locality mapping
void print_exec_name_to_locality(const exec_name_to_locality_t &mapping, std::ostream &out)
{
    out << "exec_name_to_locality:\n";
    for (const auto &[key, loc] : mapping)
    {
        out << "  " << key << ": NUMA ID = " << loc.numa_id << ", Core ID = " << loc.core_id
            << ", Voluntary CS = " << loc.voluntary_context_switches << ", Involuntary CS = " << loc.involuntary_context_switches
            << ", Core Migrations = " << loc.core_migrations << "\n";
    }
    out << std::endl;
}

// Function to print comm_name_to_read_time mapping
void print_comm_name_to_read_time(const comm_name_to_time_t &mapping, std::ostream &out)
{
    out << "comm_name_to_read_time:\n";
    for (const auto &[key, value] : mapping)
    {
        auto [start, end, bytes] = value;
        out << "  " << key << ": Start = " << start << ", End = " << end << ", Bytes = " << bytes << "\n";
    }
    out << std::endl;
}

// Function to print exec_name_to_compute_time mapping
void print_exec_name_to_compute_time(const exec_name_to_compute_time_t &mapping, std::ostream &out)
{
    out << "exec_name_to_compute_time:\n";
    for (const auto &[key, value] : mapping)
    {
        auto [start, end, flops] = value;
        out << "  " << key << ": Start = " << start << ", End = " << end << ", FLOPS = " << flops << "\n";
    }
    out << std::endl;
}

// Function to print comm_name_to_write_time mapping
void print_comm_name_to_write_time(const comm_name_to_time_t &mapping, std::ostream &out)
{
    out << "comm_name_to_write_time:\n";
    for (const auto &[key, value] : mapping)
    {
        auto [start, end, bytes] = value;
        out << "  " << key << ": Start = " << start << ", End = " << end << ", Bytes = " << bytes << "\n";
    }
    out << std::endl;
}

// Function to print exec_name_to_time mapping
void print_exec_name_to_time(const exec_name_to_time_t &mapping, std::ostream &out)
{
    out << "exec_name_to_time:\n";
    for (const auto &[key, value] : mapping)
    {
        auto [start, end] = value;
        out << "  " << key << ": Start = " << start << ", End = " << end << "\n";
    }
    out << std::endl;
}

void print_unavailable_cores(const hwloc_core_ids_availability_t &hwloc_core_ids_availability)
{
    std::vector<int> unavailable_cores = get_hwloc_core_ids_by_availability(hwloc_core_ids_availability, false);

    if (unavailable_cores.empty())
    {
        std::cout << "All cores are available." << std::endl;
    }
    else
    {
        std::cout << "Unavailable cores: ";
        for (size_t i = 0; i < unavailable_cores.size(); ++i)
        {
            std::cout << unavailable_cores[i];
            if (i != unavailable_cores.size() - 1)
            {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }
}