#include "scheduler_min_min.hpp"


MIN_MIN_Scheduler::MIN_MIN_Scheduler(const simgrid_execs_t &dag, const common_t *common) : dag(dag), common(common)
{

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

        std::tie(core_id, finish_time) = this->get_best_core_id(exec);

        if (finish_time < estimated_finish_time)
        {
            selected_exec = exec;
            selected_core_id = core_id;
            estimated_finish_time = finish_time;
        }
    }

    return std::make_tuple(selected_exec->get_cname(), selected_core_id, estimated_finish_time);
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
std::tuple<unsigned int, unsigned long> MIN_MIN_Scheduler::get_best_core_id(const simgrid_exec_t *exec)
{
    unsigned int best_core_id = std::numeric_limits<unsigned int>::max();
    double earliest_finish_time_us = std::numeric_limits<double>::max();

    // Match all communication (Task1->Task2) where this task_name is the destination.
    name_to_time_range_payload_t name_to_ts_range_payload = common_filter_name_ts_range_payload(
        this->common, exec->get_name(), COMM_WRITE, DST);

    // Estimate exec earliest_finish_time for every core_id.
    for (int core_id : common_get_avail_core_ids(this->common))
    {
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

        for (const auto &[comm_name, time_range_payload] : name_to_ts_range_payload)
        {
            auto [parent_exec_name, self_exec_name] = common_split(comm_name,"->");
            uint64_t parent_exec_actual_finish_time = std::get<1>(this->common->exec_name_to_time_offset_payload.at(parent_exec_name));
            earliest_start_time_us = std::max(earliest_start_time_us, parent_exec_actual_finish_time);
        }

        /* 2. ESTIMATE READ_TIME(EXEC) */

        // ASSUMPTION:
        // While readings are performed sequentially, the timing calculations assume they are 
        // executed in parallel. Specifically, the read time of a task is determined as the 
        // maximum finish time among the reading operations.

        double estimated_read_time_us = 0;

        for (const auto &[comm_name, time_range_payload] : name_to_ts_range_payload)
        {
            char *read_buffer = this->common->comm_name_to_address.at(comm_name);

            // ASSUMPTION:
            // The pages of the data to be read can be distributed across multiple NUMA nodes,
            // depending on the memory allocation policy. For this estimation, the worst-case scenario is assumed.
            // Specifically, it is assumed that all data pages are located in a single NUMA node—the one with the
            // highest read time relative to the NUMA node of the requesting core. This approach provides an 
            // upper-bound estimate of the reading operation.

            // Retrieve the NUMA nodes where the data pages to be read are allocated.
            std::vector<int> read_src_numa_ids = get_hwloc_numa_ids_by_address(
                this->common, read_buffer, (size_t)(std::get<2>(time_range_payload)));

            // Determine the NUMA node corresponding to the core that will perform the reading operation.
            int read_dst_numa_id = get_hwloc_numa_id_by_core_id(this->common, core_id);

            // Identify the source NUMA node representing the worst-case scenario, i.e.,
            // the node from which reading will incur the highest time cost, assuming all pages
            // are allocated on this NUMA node.

            for (int i : read_src_numa_ids)
            {
                double read_latency_ns = this->common->distance_lat_ns[i][read_dst_numa_id];
                double read_bandwidth_gbps = this->common->distance_bw_gbps[i][read_dst_numa_id];

                double read_latency_us = read_latency_ns / 1000; // To microseconds
                double read_bandwidth_bpus = read_bandwidth_gbps * 1000; // To B/us
                size_t payload_bytes = (size_t) std::get<2>(time_range_payload);

                estimated_read_time_us = std::max(estimated_read_time_us, read_latency_us + (payload_bytes / read_bandwidth_bpus));
            }
        }


    }

    return {0,0};
}
