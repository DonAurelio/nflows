#include "mapper_simulation.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(mapper_simulation, "Messages specific to this module.");

Mapper_Simulation::Mapper_Simulation(common_t *common, scheduler_t &scheduler) : Mapper_Base(common, scheduler)
{
    this->set_thread_func_ptr(mapper_simulation_thread_function);
}

Mapper_Simulation::~Mapper_Simulation()
{
}

void Mapper_Simulation::start()
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
        data->thread_function = nullptr;

        // Set as assigned.
        selected_exec->set_host(this->dummy_host);
        common_set_core_id_avail_unitl(common, selected_core_id, estimated_completion_time);
        this->thread_func_ptr(data);
    }

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
void *mapper_simulation_thread_function(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    XBT_INFO("Task ID: %s, Core ID: %d => message: started.", data->exec->get_cname(), data->assigned_core_id);

    // Match all communication (Task1->Task2) where this task_name is the destination.
    name_to_time_range_payload_t name_to_ts_range_payload =
        common_filter_name_to_time_range_payload(data->common, data->exec->get_name(), COMM_WRITE_OFFSETS, DST);

    /* FOR THE COMPUTATON OF TIME OFFSETS (DURATIONS) */

    double actual_start_time_us = 0.0;

    for (const auto &[comm_name, time_range_payload] : name_to_ts_range_payload)
    {
        auto [parent_exec_name, self_exec_name] = common_split(comm_name, "->");
        actual_start_time_us = std::max(actual_start_time_us, (double) std::get<1>(time_range_payload));
    }

    /* SIMULATE MEMORY READING */

    double actual_read_time_us = 0.0;

    int assigned_core_numa_id = get_hwloc_numa_id_by_core_id(data->common, data->assigned_core_id);

    for (const auto &[comm_name, time_range_payload] : name_to_ts_range_payload)
    {
        double read_payload_bytes = (double)std::get<2>(time_range_payload);

        double read_start_timestamp_us = actual_start_time_us;

        // ASSUMPTION:
        // The simulation assumes that the entire data item is stored in a single memory domain.
        // Future implementations may consider data pages being spread, NUMA balancing (page migration).

        if (data->common->comm_name_to_numa_ids_w.at(comm_name).size() != 1)
        {
            std::cerr << "Error: Simulation mode does not support data pages spread across multiple memory domains." << std::endl;
            exit(EXIT_FAILURE);
        }

        int read_src_numa_id = data->common->comm_name_to_numa_ids_w.at(comm_name).front();

        double read_latency_ns = data->common->distance_lat_ns[read_src_numa_id][assigned_core_numa_id];
        double read_bandwidth_gbps = data->common->distance_bw_gbps[read_src_numa_id][assigned_core_numa_id];

        double read_latency_us = read_latency_ns / 1000;         // To microseconds
        double read_bandwidth_bpus = read_bandwidth_gbps * 1000; // To B/us

        double read_time_us = read_latency_us + (read_payload_bytes / read_bandwidth_bpus);

        double read_end_timestamp_us = read_start_timestamp_us + read_time_us;

        // Read time offset per dependecy.
        data->common->comm_name_to_r_time_offset_payload[comm_name] = time_range_payload_t(
            read_start_timestamp_us, read_end_timestamp_us, read_payload_bytes);

        actual_read_time_us = std::max(actual_read_time_us, read_end_timestamp_us);

        std::string comm_name_left = std::get<0>(common_split(comm_name, "->")).c_str();

        XBT_INFO("Task ID: %s, Core ID: %d => dependency: %s, read (bytes): %f",
            data->exec->get_cname(), data->assigned_core_id, comm_name_left.c_str(), read_payload_bytes);
    }

    /* SIMULATE COMPUTATION */

    double flops = data->exec->get_remaining();

    double exec_start_timestamp_us = actual_read_time_us;

    // Core clock frequency must be static (set to ther value but 0).
    double processor_speed_flops_per_second = (double) get_hwloc_core_performance_by_id(data->common, data->assigned_core_id);

    // Calculate the compute time in microseconds.
    double compute_time_us = (flops / processor_speed_flops_per_second) * 1000000;
    
    XBT_INFO("flops: %f, processor_speed_flops: %f, op: %f, compute_time_us: %f", flops, processor_speed_flops_per_second, flops / processor_speed_flops_per_second, compute_time_us);

    double exec_end_timestamp_us = exec_start_timestamp_us + compute_time_us;

    // Compute time offset
    data->common->exec_name_to_c_time_offset_payload[data->exec->get_cname()] = time_range_payload_t(
        exec_start_timestamp_us, exec_end_timestamp_us, flops);

    /* SIMULATE MEMORY WRITTING */

    double actual_write_time_us = 0;

    for (const auto &succ_ptr : data->exec->get_successors())
    {
        const simgrid_activity_t *succ = succ_ptr.get();
        auto [exec_name, succ_exec_name] = common_split(succ->get_cname(), "->");

        if (succ_exec_name == std::string("end"))
        {
            XBT_INFO("Task ID: %s, Core ID: %d => successor: %s, message: skipt writing operation for end task.",
                     data->exec->get_cname(), data->assigned_core_id, succ_exec_name.c_str());

            continue;
        }

        double write_payload_bytes = succ->get_remaining();

        double write_start_timestamp_us = exec_end_timestamp_us;

        // ASSUMPTION:
        // Writes will follow the first-touch policy.
        int write_dst_numa_id = get_hwloc_numa_id_by_core_id(data->common, data->assigned_core_id);

        double write_latency_ns = data->common->distance_lat_ns[assigned_core_numa_id][write_dst_numa_id];
        double write_bandwidth_gbps = data->common->distance_bw_gbps[assigned_core_numa_id][write_dst_numa_id];

        double write_latency_us = write_latency_ns / 1000;         // To microseconds
        double write_bandwidth_bpus = write_bandwidth_gbps * 1000; // To B/us
        
        double write_time_us = write_latency_us + (write_payload_bytes / write_bandwidth_bpus);

        double write_end_timestamp_us = write_start_timestamp_us + write_time_us;

        // Compute write time, assuming reads are carried out in parallel.
        // The total read time is determined by the longest individual read time.
        actual_write_time_us = std::max(actual_write_time_us, write_end_timestamp_us);

        // Compute time offset
        data->common->comm_name_to_w_time_offset_payload[succ->get_cname()] = time_range_payload_t(
            write_start_timestamp_us, write_end_timestamp_us, write_payload_bytes);

        // Preserve data locality.
        data->common->comm_name_to_numa_ids_w[succ->get_cname()] = {assigned_core_numa_id};

        XBT_INFO("Task ID: %s, Core ID: %d => successor: %s, write (bytes): %f, ", 
            data->exec->get_cname(), data->assigned_core_id, succ_exec_name.c_str(), write_payload_bytes);
    }

    XBT_INFO("Task ID: %s, Core ID: %d => message: finished.", data->exec->get_cname(), data->assigned_core_id);

    /* TIME OFFSETS */

    double actual_finish_time_us = actual_read_time_us + compute_time_us + actual_write_time_us;
    data->common->exec_name_to_rcw_time_offset_payload[data->exec->get_cname()] =
        time_range_payload_t(actual_start_time_us, actual_finish_time_us, flops);

    /* CLEAN UP */

    // Mark successors as completed.
    for (const auto &succ_ptr : data->exec->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);

    // Mark the selected hwloc_core_id as available.
    common_set_core_id_as_avail(data->common, data->assigned_core_id);

    // this pointer was created in the thread caller 'assign_exec'
    free(data);

    return NULL;
}
