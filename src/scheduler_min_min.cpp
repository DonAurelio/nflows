#include "scheduler_min_min.hpp"


MIN_MIN_Scheduler::MIN_MIN_Scheduler(std::string dot_file_path)
{
    const char* file_path = dot_file_path.c_str();
    this->dag = common_read_dag_from_dot(file_path);
}

std::tuple<std::string, unsigned int, unsigned long> MIN_MIN_Scheduler::next()
{
    simgrid_exec_t *selected_exec = nullptr;
    unsigned int selected_core_id = std::numeric_limits<unsigned int>::max();
    unsigned long estimated_finish_time = std::numeric_limits<unsigned long>::max();

    for (simgrid_exec_t *exec : common_get_ready_tasks(this->dag))
    {
        int core_id;
        double finish_time;

        std::tie(core_id, finish_time) = get_best_core_id(this->common, exec);

        if (finish_time < estimated_finish_time)
        {
            selected_exec = exec;
            selected_core_id = core_id;
            estimated_finish_time = finish_time;
        }
    }

    return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
}

/**
 * @brief Find the best core to execute a given task in a task dependecy graph.
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
 * ### Parameters:
 * @param common_data A pointer to the global data structure containing information such as latency and bandwidth
 * matrices.
 * @param exec The task for which the best hardware core is being selected.
 *
 * ### Notes:
 * Control dependencies with size -1 or 0, as defined in SimGrid, are not supported and may introduce unexpected behavior.
 */
std::tuple<unsigned int, unsigned long> get_best_core_id(const simgrid_exec_t *exec)
{
    unsigned int best_core_id = std::numeric_limits<unsigned int>::max();
    unsigned double earliest_finish_time_us = std::numeric_limits<unsigned double>::max();

    // Estimate exec earliest_finish_time for every hwloc_core_id.
    for (int core_id : common_get_avail_core_ids(this->common))
    {
        // Match all communication names (Task1->Task2) where this task_name is the destination.
        comm_name_time_ranges_t read_comm_name_time_ranges = common_get_name_ts_range_payload(this->common, exec->get_name());

        /* 1. ESTIMATE EARLIEST_START_TIME(n_i). */

        // ASSUMPTION:
        // EST(n_i,p_i) = max{ avail[j], max_{n_{m} e pred(n_i)}( AFT(n_{m}) + c_{m,i} ) };
        // - avail[j]  => earliest time which processor j will be ready for task execution.
        // - pred(n_i) => set of immediate predecessor tasks of task n_{i}.

        // This model do not consider avail[j] as the selection of the best core is performed
        // with the set of current available cores.

        // Since we already have the time at which communication all dependent communication finished:
        // EST(n_i) = max_{n_{m} e pred(n_i)} { AFT(c_{m,i}) ) };

        uint64_t earliest_start_time_us = 0;

        for (const auto &[comm_name, _start_time, _end_time, bytes_to_read] : read_comm_name_time_ranges)
        {
            auto [pred_exec_name, self_exec_name] = split_by_arrow(std::string(comm_name));
            uint64_t pred_exec_actual_finish_time = std::get<1>((*common_data->exec_name_to_time)[pred_exec_name]);
            earliest_start_time_us = std::max(earliest_start_time_us, pred_exec_actual_finish_time);
        }

        /* 2. ESTIMATE READ_TIME(EXEC) */

        // ASSUMPTION:
        // While readings are performed sequentially, the timing calculations assume they are 
        // executed in parallel. Specifically, the read time of a task is determined as the 
        // maximum finish time among the reading operations.

        double estimated_read_time = 0;

        for (const auto &[comm_name, _start_time, _end_time, bytes_to_read] : read_comm_name_time_ranges)
        {
            char *read_buffer = (*common_data->comm_name_to_ptr)[comm_name];

            // ASSUMPTION:
            // The pages of the data to be read can be distributed across multiple NUMA nodes,
            // depending on the memory allocation policy. For this estimation, the worst-case scenario is assumed.
            // Specifically, it is assumed that all data pages are located in a single NUMA node—the one with the
            // highest read time relative to the NUMA node of the requesting core. This approach provides an 
            // upper-bound estimate of the reading operation.

            // Retrieve the NUMA nodes where the data pages to be read are allocated.
            std::vector<int> hwloc_read_src_numa_ids = get_hwloc_numa_ids_from_ptr(common_data->topology, read_buffer, (size_t)bytes_to_read);

            // Determine the NUMA node corresponding to the core that will perform the reading operation.
            int hwloc_read_dst_numa_id = get_hwloc_numa_id_from_hwloc_core_id(common_data->topology, hwloc_core_id);

            // Identify the source NUMA node representing the worst-case scenario, i.e.,
            // the node from which reading will incur the highest time cost, assuming all pages
            // are allocated on this NUMA node.

            for (int i : hwloc_read_src_numa_ids)
            {
                double read_latency = (*(common_data->latency_matrix))[i][hwloc_read_dst_numa_id];
                double read_bandwidth = (*(common_data->bandwidth_matrix))[i][hwloc_read_dst_numa_id];

                // TODO: Validate the consistency of bandwidth and latency units.
                estimated_read_time = std::max(estimated_read_time, read_latency + (bytes_to_read / read_bandwidth));
            }
        }

        /* 3. ESTIMATE COMPUTE_TIME(EXEC). */

        // TODO: Check units of core speed (it must be in FLOPS).
        double estimated_compute_time = exec->get_remaining() / get_current_core_speed_from_hwloc_core_id(common_data->topology, hwloc_core_id);

        /* 4. ESTIMATE WRITE_TIME(EXEC) */

        // ASSUMPTION:
        // While readings are performed sequentially, the timing calculations assume they are 
        // executed in parallel. Specifically, the read time of a task is determined as the 
        // maximum finish time among the readings to be performed.

        double estimated_write_time = 0;

        for (const auto &succ : exec->get_successors())
        {
            const simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            double bytes_to_write = succ_comm->get_remaining();

            // Retrieve the NUMA node that will perform the write operation.
            int hwloc_write_src_numa_id = get_hwloc_numa_id_from_hwloc_core_id(common_data->topology, hwloc_core_id);

            // Determine the NUMA node that will handle the write operation.
            // ASSUMPTION:
            // Since it is uncertain which NUMA node will handle the write operations for this task,
            // the worst-case scenario is assumed: the data is written to the farthest NUMA node,
            // i.e., the one with the highest write time relative to the specified hwloc core ID.

            for (size_t i = 0; i < (*(common_data->latency_matrix))[0].size(); ++i)
            {
                double write_latency = (*(common_data->latency_matrix))[hwloc_write_src_numa_id][i];
                double write_bandwidth = (*(common_data->bandwidth_matrix))[hwloc_write_src_numa_id][i];

                // TODO: Validate the consistency of bandwidth and latency units.
                estimated_write_time = std::max(estimated_write_time, write_latency + (bytes_to_write / write_bandwidth));
            }
        }

        double finish_time_us = earliest_start_time_us + estimated_read_time + estimated_compute_time + estimated_write_time;
        best_hwloc_core_id = (finish_time_us < earliest_finish_time_us) ? hwloc_core_id : best_hwloc_core_id;
        earliest_finish_time_us = std::min(finish_time_us, earliest_finish_time_us);
    }

    return std::make_pair(best_hwloc_core_id, earliest_finish_time_us);
}
