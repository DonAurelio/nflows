#include "mapper_bare_metal.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(mapper_bare_metal, "Messages specific to this module.");

Mapper_Bare_Metal::Mapper_Bare_Metal(common_t *common, scheduler_t &scheduler) : Mapper_Base(common, scheduler)
{
    this->set_thread_func_ptr(mapper_bare_metal_thread_function);
}

Mapper_Bare_Metal::~Mapper_Bare_Metal()
{
}

void Mapper_Bare_Metal::start()
{
    XBT_INFO("Start Mapper_Bare_Metal");
    simgrid_exec_t *selected_exec;
    int selected_core_id;
    unsigned long estimated_completion_time;

    while (this->scheduler.has_next())
    {
        std::tie(selected_exec, selected_core_id, estimated_completion_time) = this->scheduler.next();

        if (!selected_exec)
        {
            XBT_INFO("There are not ready tasks, waiting 5 seconds.");
            sleep(5);
            continue;
        }

        if (selected_core_id == -1)
        {
            XBT_INFO("There are not available cores, waiting 5 seconds.");
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
        hardware_hwloc_thread_bind_to_core_id(data);
    }

    common_threads_active_wait(this->common);

    // Workaround to properly finalize SimGrid resources.
    simgrid::s4u::Engine *e = simgrid::s4u::Engine::get_instance();
    e->run();

    XBT_INFO("End Mapper_Bare_Metal");
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
 * 5. Save thread locality information, including the NUMA node ids, core ID, and context switches.
 *
 * @param arg Structure used to collect thread execution data.
 * @return void*
 *
 * @note This method does not support SimGrid control dependencies (0, -1).
 */
void *mapper_bare_metal_thread_function(void *arg)
{
    hardware_hwloc_thread_mem_policy_set(((thread_data_t *) arg)->common);

    thread_data_t* data = ((thread_data_t *) arg);

    common_t *common = data->common;
    simgrid_exec_t *exec = data->exec;
    int assigned_core_id = data->assigned_core_id;
    
    std::vector<int> mem_bind_numa_ids;
    hwloc_membind_policy_t mem_bind_policy = hardware_hwloc_thread_mem_policy_get_from_os(common, mem_bind_numa_ids);
    std::string thread_mem_policy = common_mapper_mem_policy_type_to_str(mem_bind_policy) + " [" + common_join(mem_bind_numa_ids) + "]";

    int thread_core_id = hardware_hwloc_core_id_get_by_pu_id(common, sched_getcpu());
    int thread_pid = getpid();
    int thread_tid = gettid();

    if (thread_core_id != assigned_core_id)
    {
        XBT_ERROR("thread_core_id (%d) != assigned_core_id (%d)", thread_core_id, assigned_core_id);
        std::runtime_error("thread_core_id != assigned_core_id");
    }

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: started.", thread_pid, thread_tid, exec->get_cname(), thread_core_id);
    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => thread_mem_policy: %s.", thread_pid, thread_tid, exec->get_cname(), thread_core_id, thread_mem_policy.c_str());

    double earliest_start_time_us = common_earliest_start_time(common, exec->get_name(), assigned_core_id);

    /* EMULATE MEMORY READING */
    double actual_read_time_us = 0.0;

    // Match all communication (Task1->Task2) where this task_name is the destination.
    name_to_time_range_payload_t matches = common_comm_name_to_w_time_offset_payload_filter(common, exec->get_name());

    for (const auto &[comm_name, time_range_payload] : matches)
    {
        char *read_buffer = common_comm_name_to_address_get(common, comm_name);
        
        double read_payload_bytes = std::get<2>(time_range_payload);

        // Used to check data (pages) migration.
        std::vector<int> nlbr = hardware_hwloc_numa_id_get_by_address(common, read_buffer, read_payload_bytes);

        double read_start_timestemp_us = common_get_time_us();

        size_t checksum = 0;
        for (size_t i = 0; i < read_payload_bytes; i++)
            checksum += read_buffer[i]; // Access each byte in the buffer (simulates reading)

        double read_end_timestemp_us = common_get_time_us();

        XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => read: %s, payload (bytes): %f, checksum: %ld", 
            thread_pid, thread_tid, exec->get_cname(), thread_core_id, comm_name.c_str(), read_payload_bytes, checksum);

        // Used to check data (pages) migration. Migration is trigered once the data is being read.
        std::vector<int> nlar = hardware_hwloc_numa_id_get_by_address(common, read_buffer, read_payload_bytes);

        // Track reading consistency.
        common_threads_checksum_update(common, checksum);

        // Save read data locality.
        common_comm_name_to_numa_ids_r_create(common, comm_name, nlar); 

        // Save read timestamps.
        time_range_payload_t read_ts_range_payload = time_range_payload_t(read_start_timestemp_us, read_end_timestemp_us, read_payload_bytes);
        common_comm_name_to_r_ts_range_payload_create(common, comm_name, read_ts_range_payload);

        // Save read time offset.
        time_range_payload_t read_of_payload = time_range_payload_t(
            earliest_start_time_us, earliest_start_time_us + (read_end_timestemp_us - read_start_timestemp_us), read_payload_bytes);
        common_comm_name_to_r_time_offset_payload_create(common, comm_name, read_of_payload);

        actual_read_time_us = std::max(actual_read_time_us, read_end_timestemp_us - read_start_timestemp_us);
        
        XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => read: %s, numa_locality_before_read: [%s], numa_locality_after_read: [%s], pages_migration: %s",
            thread_pid, thread_tid, exec->get_cname(), thread_core_id, comm_name.c_str(), common_join(nlbr).c_str(), common_join(nlar).c_str(), nlbr != nlar ? "yes" : "no");

        common_reads_active_increment(common, comm_name);

        // Clean up.
        free(read_buffer);
    }

    /* EMULATE COMPUTATION */
    double flops = exec->get_remaining();

    // The volatile keyword is used to prevent the compiler from optimizing away the floating-point operations.
    volatile double a = 1.0, b = 2.0, c = 0.0;

    double exec_start_timestamp_us = common_get_time_us();

    // FMA (Fused Multiply-Add) operation. This is a common pattern in scientific computing
    // benchmarks (e.g., LINPACK)
    for (uint64_t i = 0; i < flops; ++i) c = a * b + c;

    double exec_end_timestamp_us = common_get_time_us();

    // Calculate the compute time in microseconds.
    double compute_time_us = exec_end_timestamp_us - exec_start_timestamp_us;

    // Save compute timestamps.
    time_range_payload_t exec_ts_range_payload = time_range_payload_t{exec_start_timestamp_us, exec_end_timestamp_us, flops};
    common_exec_name_to_c_ts_range_payload_create(common, exec->get_name(), exec_ts_range_payload);

    // Save compute offsets.
    time_range_payload_t exec_of_range_payload = time_range_payload_t{
        earliest_start_time_us + actual_read_time_us, 
        earliest_start_time_us + actual_read_time_us + compute_time_us, 
        flops
    };

    common_exec_name_to_c_time_offset_payload_create(common, exec->get_name(), exec_of_range_payload);

    common_execs_active_increment(common, exec->get_name());

    /* EMULATE MEMORY WRITTING */
    double actual_write_time_us = 0.0;

    for (const auto &succ_ptr : exec->get_successors())
    {
        const simgrid_activity_t *succ = succ_ptr.get();
        auto [exec_name, succ_exec_name] = common_split(succ->get_cname(), "->");

        if (succ_exec_name == std::string("end")) continue;

        double write_payload_bytes = succ->get_remaining();

        // Emulate memory writting by saving data into memory.
        char *write_buffer = (char *)malloc(write_payload_bytes);

        if (!write_buffer)
        {
            XBT_ERROR("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => write: %s, message: unable to create write buffer.",
                thread_pid, thread_tid, exec->get_cname(), thread_core_id, succ->get_cname());

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

        double write_start_timestamp_us = common_get_time_us();

        memset(write_buffer, 0, write_payload_bytes);

        double write_end_timestamp_us = common_get_time_us();

        // Save address for subsequent reading.
        common_comm_name_to_address_create(common, succ->get_name(), write_buffer);

        // Get data numa locality.
        std::vector<int> nlaw = hardware_hwloc_numa_id_get_by_address(data->common, write_buffer, write_payload_bytes);

        // Save data locality.
        common_comm_name_to_numa_ids_w_create(common, succ->get_name(), nlaw);

        // Compute write time, assuming reads are carried out in parallel.
        // The total read time is determined by the longest individual read time.
        actual_write_time_us = std::max(actual_write_time_us, write_end_timestamp_us - write_start_timestamp_us);

        // Save write timestamps.
        time_range_payload_t write_ts_range_payload = time_range_payload_t{write_start_timestamp_us, write_end_timestamp_us, write_payload_bytes};

        common_comm_name_to_w_ts_range_payload_create(common, succ->get_name(), write_ts_range_payload);

        // Save write offsets.
        time_range_payload_t write_of_range_payload = time_range_payload_t(
            earliest_start_time_us + actual_read_time_us + compute_time_us,
            earliest_start_time_us + actual_read_time_us + compute_time_us + (write_end_timestamp_us - write_start_timestamp_us),
            write_payload_bytes);

        common_comm_name_to_w_time_offset_payload_create(common, succ->get_name(), write_of_range_payload);

        common_writes_active_increment(common, succ->get_name());

        XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => write: %s, payload (bytes): %f, numa_locality_after_write: [%s].",
            thread_pid, thread_tid, exec->get_cname(), thread_core_id, succ->get_cname(), write_payload_bytes,  common_join(nlaw).c_str());
    }

    // Save read + compute + write offsets
    double actual_finish_time_us = earliest_start_time_us + actual_read_time_us + compute_time_us + actual_write_time_us;
    time_range_payload_t rcw_of_range_payload = time_range_payload_t(earliest_start_time_us, actual_finish_time_us, flops);
    common_exec_name_to_rcw_time_offset_payload_create(common, exec->get_name(), rcw_of_range_payload);

    // Save thread locality.
    thread_locality_t thread_locality = hardware_hwloc_thread_get_locality_from_os(common);
    common_exec_name_to_thread_locality_create(common, exec->get_name(), thread_locality);

    /* CLEAN UP */

    // Mark successors as completed.
    for (const auto &succ_ptr : exec->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);

    // In the previous version, this worked, but in the current version, exec is a null pointer.
    // As a result, the task itself is not marked as completed, but it was assigned,
    // and its dependencies were marked as completed.
    // data->exec->complete(simgrid::s4u::Activity::State::FINISHED);

    // Decrement the active thread counter and signal if no more threads
    common_threads_active_decrement(common);

    // Mark the selected hwloc_core_id as available.
    common_core_id_set_avail(common, assigned_core_id, true);

    // Update core availability
    common_core_id_set_avail_until(common, assigned_core_id, actual_finish_time_us);

    XBT_INFO("Process ID: %d, Thread ID: %d, Task ID: %s, Core ID: %d => message: finished.", thread_pid, thread_tid, exec->get_cname(), thread_core_id);

    // this pointer was created in the thread caller 'assign_exec'
    free(data);

    return NULL;
}
