#include "mapper.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(mapper_bare_metal, "Messages specific to this module.");

Mapper_Bare_Metal::Mapper_Bare_Metal(common_t *common, scheduler_t &scheduler) : Mapper_Base(common, scheduler)
{
    this->set_thread_func_ptr(mapper_bare_metal_thread_function);
}

Mapper::~Mapper()
{
}

void Mapper::set_thread_func_ptr(void *(*func)(void *))
{
    this->thread_func_ptr = func;
}

void Mapper::start()
{
    simgrid_exec_t *selected_exec;
    int selected_core_id;
    unsigned long estimated_completion_time;

    while (this->scheduler.has_next())
    {
        std::tie(selected_exec, selected_core_id, estimated_completion_time) = this->scheduler.next();

        if (!selected_exec)
        {
            XBT_INFO("There are not ready tasks, waiting 5 seconds.");
            // printf("There are not ready tasks, waiting 5 seconds.\n");
            sleep(5);
            continue;
        }

        if (selected_core_id == -1)
        {
            XBT_INFO("There are not available cores, waiting 5 seconds.");
            // printf("There are not available cores, waiting 5 seconds.\n");
            sleep(5);
            continue;
        }

        // Initialize thread data.
        // data is free'd by the thread at the end of the execution.
        thread_data_t *data = (thread_data_t *)malloc(sizeof(thread_data_t));
        data->exec = selected_exec;
        data->assigned_core_id = selected_core_id;
        data->common = this->common;
        data->thread_function = this->thread_func_ptr;

        // Set as assigned.
        selected_exec->set_host(this->dummy_host);
        bind_exec_to_thread(data);
    }

    common_wait_active_threads(this->common);

    // Workaround to properly finalize SimGrid resources.
    simgrid::s4u::Engine *e = simgrid::s4u::Engine::get_instance();
    e->run();
}

/**
 * @brief Emulate the activities involved in executing a workflow task.
 *
 * 1. Read the required data from memory by accessing a previously created address.
 *    Reads are performed sequentially, but the time recorded assumes they were performed in parallel.
 *
 * 2. Perform the FMA (Fused Multiply-Add) operation to emulate computation.
 *
 * 3. Write the required data to memory, creating a pointer to be accessed by successor tasks.
 *    Writes are performed sequentially, but the time recorded assumes they were performed in parallel.
 *
 * 4. Record the read, write, and execution times.
 *
 * 5. Save thread locality information, including the last NUMA node, core ID, and context switches.
 *
 * @param arg Structure used to collect thread execution data.
 * @return void*
 *
 * @note This method does not support SimGrid control dependencies (0, -1).
 */
void *mapper_bare_metal_thread_function(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: started.", getpid(), gettid(),
             data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()));

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: %s.", getpid(), gettid(),
             data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()),
             get_hwloc_thread_mem_policy(data->common).c_str());

    // Match all communication (Task1->Task2) where this task_name is the destination.
    name_to_time_range_payload_t name_to_ts_range_payload =
        common_filter_name_to_time_range_payload(data->common, data->exec->get_name(), COMM_WRITE_TIMESTAMPS, DST);

    /* FOR THE COMPUTATON OF TIME OFFSETS (DURATIONS) */

    uint64_t actual_start_time_us = 0;

    for (const auto &[comm_name, time_range_payload] : name_to_ts_range_payload)
    {
        auto [parent_exec_name, self_exec_name] = common_split(comm_name, "->");
        uint64_t parent_exec_actual_finish_time_us =
            std::get<1>(data->common->exec_name_to_rcw_time_offset_payload.at(parent_exec_name));
        actual_start_time_us = std::max(actual_start_time_us, parent_exec_actual_finish_time_us);
    }

    /* EMULATE MEMORY READING */

    uint64_t actual_read_time_us = 0;

    for (const auto &[comm_name, time_range_payload] : name_to_ts_range_payload)
    {
        char *read_buffer = data->common->comm_name_to_address.at(comm_name);
        size_t read_payload_bytes = (size_t)std::get<2>(time_range_payload);

        // Used to check data (pages) migration.
        std::vector<int> numa_locality_before_read =
            get_hwloc_numa_ids_by_address(data->common, read_buffer, read_payload_bytes);

        uint64_t read_start_timestemp_us = common_get_time_us();

        size_t checksum = 0;
        for (size_t i = 0; i < read_payload_bytes; i++)
        {
            checksum += read_buffer[i]; // Access each byte in the buffer (simulates reading)
        }

        uint64_t read_end_timestemp_us = common_get_time_us();

        // Track reading consistency.
        data->common->checksum += checksum;

        // Read time offset per dependecy.
        data->common->comm_name_to_r_time_offset_payload[comm_name] = time_range_payload_t(
            actual_start_time_us, actual_start_time_us + (read_end_timestemp_us - read_start_timestemp_us),
            read_payload_bytes);

        // Used to check data (pages) migration. Migration is trigered once the data is being read.
        std::vector<int> numa_locality_after_read =
            get_hwloc_numa_ids_by_address(data->common, read_buffer, read_payload_bytes);

        // Save data locality.
        data->common->comm_name_to_numa_ids_r[comm_name] = numa_locality_after_read;

        // Save read timestamps.
        data->common->comm_name_to_r_ts_range_payload[comm_name] =
            time_range_payload_t(read_start_timestemp_us, read_end_timestemp_us, read_payload_bytes);

        actual_read_time_us = std::max(actual_read_time_us, read_end_timestemp_us - read_start_timestemp_us);

        XBT_INFO(
            "Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => dependency: %s, read (bytes): %zu, checksum: "
            "%ld, numa_locality_before_read: %s, numa_locality_after_read: %s, data (pages) migration: %s",
            getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()),
            std::get<0>(common_split(comm_name, "->")).c_str(), read_payload_bytes, checksum,
            common_join(numa_locality_before_read, " ").c_str(), common_join(numa_locality_after_read, " ").c_str(),
            numa_locality_before_read != numa_locality_after_read ? "yes" : "no");

        // Clean up.
        free(read_buffer);

        read_buffer = NULL;
    }

    /* EMULATE COMPUTATION */

    uint64_t flops = (uint64_t)data->exec->get_remaining();

    // The volatile keyword is used to prevent the compiler from optimizing away the floating-point operations.
    volatile double a = 1.0, b = 2.0, c = 0.0;

    uint64_t exec_start_timestamp_us = common_get_time_us();

    // FMA (Fused Multiply-Add) operation. This is a common pattern in scientific computing
    // benchmarks (e.g., LINPACK)
    for (uint64_t i = 0; i < flops; ++i)
        c = a * b + c;

    uint64_t exec_end_timestamp_us = common_get_time_us();

    // Calculate the compute time in microseconds.
    uint64_t compute_time_us = exec_end_timestamp_us - exec_start_timestamp_us;

    // Compute time offset
    data->common->exec_name_to_c_time_offset_payload[data->exec->get_cname()] = time_range_payload_t(
        actual_start_time_us + actual_read_time_us,
        actual_start_time_us + actual_read_time_us + (exec_end_timestamp_us - exec_start_timestamp_us), flops);

    // Save compute timestamps.
    data->common->exec_name_to_c_ts_range_payload[data->exec->get_cname()] =
        time_range_payload_t{exec_start_timestamp_us, exec_end_timestamp_us, flops};

    /* EMULATE MEMORY WRITTING */

    uint64_t actual_write_time_us = 0;
    for (const auto &succ_ptr : data->exec->get_successors())
    {
        const simgrid_activity_t *succ = succ_ptr.get();
        auto [exec_name, succ_exec_name] = common_split(succ->get_cname(), "->");

        if (succ_exec_name == std::string("end"))
        {
            XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => successor: %s, message: skipt writing "
                     "operation for end task.",
                     getpid(), gettid(), data->exec->get_cname(),
                     get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()),
                     std::get<1>(common_split(succ->get_cname(), "->")).c_str());

            continue;
        }

        size_t write_payload_bytes = (size_t)succ->get_remaining();

        // Emulate memory writting by saving data into memory.
        char *write_buffer = (char *)malloc(write_payload_bytes);

        if (!write_buffer)
        {
            XBT_ERROR("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => successor: %s, message: unable to "
                      "create write buffer.",
                      getpid(), gettid(), data->exec->get_cname(),
                      get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()),
                      std::get<1>(common_split(succ->get_cname(), "->")).c_str());

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

        uint64_t write_start_timestamp_us = common_get_time_us();

        memset(write_buffer, 0, write_payload_bytes);

        uint64_t write_end_timestamp_us = common_get_time_us();

        // Compute write time, assuming reads are carried out in parallel.
        // The total read time is determined by the longest individual read time.
        actual_write_time_us = std::max(actual_write_time_us, write_end_timestamp_us - write_start_timestamp_us);

        // Compute time offset
        data->common->comm_name_to_w_time_offset_payload[succ->get_cname()] = time_range_payload_t(
            actual_start_time_us + actual_read_time_us + (exec_end_timestamp_us - exec_start_timestamp_us),
            actual_start_time_us + actual_read_time_us + (exec_end_timestamp_us - exec_start_timestamp_us) +
                (write_end_timestamp_us - write_start_timestamp_us),
            write_payload_bytes);

        // Save write timestamps.
        data->common->comm_name_to_w_ts_range_payload[succ->get_cname()] =
            time_range_payload_t{write_start_timestamp_us, write_end_timestamp_us, write_payload_bytes};

        // Save address for subsequent reading.
        data->common->comm_name_to_address[succ->get_cname()] = write_buffer;

        std::vector<int> numa_locality_after_write =
            get_hwloc_numa_ids_by_address(data->common, write_buffer, write_payload_bytes);

        // Save data locality.
        data->common->comm_name_to_numa_ids_w[succ->get_cname()] = numa_locality_after_write;

        XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => successor: %s, write (bytes): %zu, "
                 "numa_locality_after_write: %s.",
                 getpid(), gettid(), data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()),
                 std::get<1>(common_split(succ->get_cname(), "->")).c_str(), write_payload_bytes,
                 common_join(numa_locality_after_write, " ").c_str());
    }

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: finished.", getpid(), gettid(),
             data->exec->get_cname(), get_hwloc_core_id_by_pu_id(data->common, sched_getcpu()));

    // Save thread locality.
    data->common->exec_name_to_thread_locality[data->exec->get_cname()] = get_hwloc_thread_locality(data->common);

    /* TIME OFFSETS */

    uint64_t actual_finish_time_us =
        actual_start_time_us + actual_read_time_us + compute_time_us + actual_write_time_us;
    data->common->exec_name_to_rcw_time_offset_payload[data->exec->get_cname()] =
        time_range_payload_t(actual_start_time_us, actual_finish_time_us, flops);

    /* CLEAN UP */

    // Mark successors as completed.
    for (const auto &succ_ptr : data->exec->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);

    // In the previous version, this worked, but in the current version, exec is a null pointer.
    // As a result, the task itself is not marked as completed, but it was assigned,
    // and its dependencies were marked as completed.
    // data->exec->complete(simgrid::s4u::Activity::State::FINISHED);

    // Decrement the active thread counter and signal if no more threads
    common_decrement_active_threads_counter(data->common);

    // Mark the selected hwloc_core_id as available.
    common_set_core_id_as_avail(data->common, data->assigned_core_id);

    // this pointer was created in the thread caller 'assign_exec'
    free(data);

    return NULL;
}
