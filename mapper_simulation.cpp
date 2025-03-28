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
    XBT_INFO("Start mapper_simulation");
    simgrid_exec_t *selected_exec;
    int selected_core_id;
    double estimated_completion_time;

    // Mandatory previous to initiate any scheduling activity.
    this->scheduler.initialize();

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

        // Task execution must be performed first to ensure that earliest_start_time  
        // is calculated correctly. Then, core availability must be set. 
        this->thread_func_ptr(data);
    }

    // Workaround to properly finalize SimGrid resources.
    simgrid::s4u::Engine *e = simgrid::s4u::Engine::get_instance();
    e->run();

    XBT_INFO("End mapper_simulation");
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
    thread_data_t* data = ((thread_data_t *) arg);

    common_t *common = data->common;
    simgrid_exec_t *exec = data->exec;
    int assigned_core_id = data->assigned_core_id;
    int assigned_core_numa_id = hardware_hwloc_numa_id_get_by_core_id(common, assigned_core_id);

    XBT_INFO("Task ID: %s, Core ID: %d => message: started.", exec->get_cname(), assigned_core_id);

    double earliest_start_time_us = common_earliest_start_time(common, exec->get_name(), assigned_core_id);

    /* SIMULATE MEMORY READING */

    double read_start_timestamp_us = earliest_start_time_us;
    double max_read_end_timestamp_us = 0.0;
    
    // Match all communication (Task1->Task2) where this task_name is the destination.
    name_to_time_range_payload_t matches = common_comm_name_to_w_time_offset_payload_filter(common, exec->get_name());

    for (const auto &[comm_name, time_range_payload] : matches)
    {
        double read_payload_bytes = std::get<2>(time_range_payload);

        // ASSUMPTION:
        // The simulation assumes that the entire data item is stored in a single memory domain.
        // Future implementations may consider data pages being spread, NUMA balancing (page migration).
        // Future implmenetations may also consider reproduce a data access pattern collected from a real execution.

        int read_src_numa_id = common_comm_name_to_numa_ids_w_get(common, comm_name).front();      

        double read_time_us = common_communication_time(common, read_src_numa_id, assigned_core_numa_id, read_payload_bytes);

        double read_end_timestamp_us = read_start_timestamp_us + read_time_us;

        max_read_end_timestamp_us = std::max(max_read_end_timestamp_us, read_end_timestamp_us);

        // Save read data locality.
        common_comm_name_to_numa_ids_r_create(common, comm_name, {read_src_numa_id});

        // Save read time offset.
        time_range_payload_t read_of_payload = time_range_payload_t(read_start_timestamp_us, read_end_timestamp_us, read_payload_bytes);
        common_comm_name_to_r_time_offset_payload_create(common, comm_name, read_of_payload);

        XBT_INFO("Task ID: %s, Core ID: %d => read: %s, payload (bytes): %f", exec->get_cname(), assigned_core_id, comm_name.c_str(), read_payload_bytes);

        common_reads_active_increment(common, comm_name);
    }

    /* SIMULATE COMPUTATION */

    double exec_start_timestamp_us = std::max(earliest_start_time_us, max_read_end_timestamp_us);

    double flops = exec->get_remaining();
    double clock_frequency_hz = hardware_hwloc_core_id_get_clock_frequency(common, assigned_core_id);
    double compute_time_us = common_compute_time(common, flops, clock_frequency_hz);
    
    double exec_end_timestamp_us = exec_start_timestamp_us + compute_time_us;

    // Save compute offsets.
    time_range_payload_t exec_of_range_payload = time_range_payload_t{exec_start_timestamp_us, exec_end_timestamp_us, flops};

    common_exec_name_to_c_time_offset_payload_create(common, exec->get_name(), exec_of_range_payload);

    common_execs_active_increment(common, exec->get_name());

    /* SIMULATE MEMORY WRITTING */

    double write_start_timestamp_us = exec_end_timestamp_us;
    double max_end_write_timestamp_us = 0.0;

    for (const auto &succ_ptr : exec->get_successors())
    {
        const simgrid_activity_t *succ = succ_ptr.get();
        auto [exec_name, succ_exec_name] = common_split(succ->get_name());

        // Skipt all task_i->end communications.
        if (succ_exec_name == std::string("end")) continue;

        // ASSUMPTION:
        // Writes will follow the first-touch policy, i.e., data will be saved in 
        // the numa node that share locality with the core_id.
        double write_payload_bytes = succ->get_remaining();
        double write_time_us = common_communication_time(common, assigned_core_numa_id, assigned_core_numa_id, write_payload_bytes);

        // Compute write time, assuming reads are carried out in parallel.
        // The total read time is determined by the longest individual read time.
        double write_end_timestamp_us = write_start_timestamp_us + write_time_us;
        max_end_write_timestamp_us = std::max(max_end_write_timestamp_us, write_end_timestamp_us);

        // Save address for subsequent reading.
        common_comm_name_to_address_create(common, succ->get_name(), nullptr);

        // Save data locality.
        common_comm_name_to_numa_ids_w_create(common, succ->get_name(), {assigned_core_numa_id});

        // Save write offsets.
        time_range_payload_t write_of_range_payload = time_range_payload_t(write_start_timestamp_us, write_end_timestamp_us, write_payload_bytes);

        common_comm_name_to_w_time_offset_payload_create(common, succ->get_name(), write_of_range_payload);

        common_writes_active_increment(common, succ->get_name());

        XBT_INFO("Task ID: %s, Core ID: %d => write: %s, payload (bytes): %f.", exec->get_cname(), assigned_core_id, succ->get_cname(), write_payload_bytes);
    }

    // Save read + compute + write offsets
    double actual_finish_time_us = std::max(exec_end_timestamp_us, max_end_write_timestamp_us);
    time_range_payload_t rcw_of_range_payload = time_range_payload_t(read_start_timestamp_us, actual_finish_time_us, flops);
    common_exec_name_to_rcw_time_offset_payload_create(common, exec->get_name(), rcw_of_range_payload);

    // Save thread locality.
    thread_locality_t thread_locality = {assigned_core_numa_id, assigned_core_id, 0, 0, 0};
    common_exec_name_to_thread_locality_create(common, exec->get_name(), thread_locality);

    /* CLEAN UP */

    // Mark successors as completed.
    for (const auto &succ_ptr : data->exec->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);

    // Mark the selected hwloc_core_id as available.
    // common_core_id_set_avail(common, assigned_core_id, true);

    // Update core availability
    common_core_id_set_avail_until(common, assigned_core_id, actual_finish_time_us);

    XBT_INFO("Task ID: %s, Core ID: %d => message: finished.", exec->get_cname(), assigned_core_id);

    // this pointer was created in the thread caller 'assign_exec'
    free(data);

    return NULL;
}
