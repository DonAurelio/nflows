#include "scheduler_eft.hpp"

EFT_Scheduler::EFT_Scheduler(const common_t *common, simgrid_execs_t &dag) : common(common), dag(dag)
{
}

EFT_Scheduler::~EFT_Scheduler()
{
}

bool EFT_Scheduler::has_next()
{
    bool has_unassigned = std::any_of(this->dag.begin(), this->dag.end(),
                                      [](const simgrid::s4u::Exec *exec) { return !exec->is_assigned(); });

    return has_unassigned;
}

std::tuple<int, unsigned long> EFT_Scheduler::get_best_core_id(const simgrid_exec_t *exec)
{
    int best_core_id = -1;
    double earliest_finish_time_us = std::numeric_limits<double>::max();

    // Match all communication (Task1->Task2) where this task_name is the destination.
    name_to_time_range_payload_t name_to_ts_range_payload =
        common_filter_name_ts_range_payload(this->common, exec->get_name(), COMM_WRITE, DST);

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
            auto [parent_exec_name, self_exec_name] = common_split(comm_name, "->");
            uint64_t parent_exec_actual_finish_time =
                std::get<1>(this->common->exec_name_to_rcw_time_offset_payload.at(parent_exec_name));
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
            std::vector<int> read_src_numa_ids =
                get_hwloc_numa_ids_by_address(this->common, read_buffer, (size_t)(std::get<2>(time_range_payload)));

            // Determine the NUMA node corresponding to the core that will perform the reading operation.
            int read_dst_numa_id = get_hwloc_numa_id_by_core_id(this->common, core_id);

            // Identify the source NUMA node representing the worst-case scenario, i.e.,
            // the node from which reading will incur the highest time cost, assuming all pages
            // are allocated on this NUMA node.

            for (int i : read_src_numa_ids)
            {
                double read_latency_ns = this->common->distance_lat_ns[i][read_dst_numa_id];
                double read_bandwidth_gbps = this->common->distance_bw_gbps[i][read_dst_numa_id];

                double read_latency_us = read_latency_ns / 1000;         // To microseconds
                double read_bandwidth_bpus = read_bandwidth_gbps * 1000; // To B/us
                size_t read_payload_bytes = (size_t)std::get<2>(time_range_payload);

                estimated_read_time_us =
                    std::max(estimated_read_time_us, read_latency_us + (read_payload_bytes / read_bandwidth_bpus));
            }
        }

        /* 3. ESTIMATE COMPUTE_TIME(EXEC). */
        size_t flops = (size_t)exec->get_remaining(); // # of Float operations to be performed.
        unsigned long flops_per_second =
            get_hwloc_core_performance_by_id(this->common, core_id); // # of flops the system can perform in one second.

        double estimated_compute_time_us = (flops / flops_per_second) * 1000000;

        /* 4. ESTIMATE WRITE_TIME(EXEC) */

        // ASSUMPTION:
        // While readings are performed sequentially, the timing calculations assume they are
        // executed in parallel. Specifically, the read time of a task is determined as the
        // maximum finish time among the readings to be performed.

        double estimated_write_time_us = 0;

        for (const auto &succ : exec->get_successors())
        {
            const simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            size_t write_payload_bytes = (size_t)succ_comm->get_remaining();

            // Retrieve the NUMA node that will perform the write operation.
            int write_src_numa_id = get_hwloc_numa_id_by_core_id(this->common, core_id);

            // Determine the NUMA node that will handle the write operation.
            // ASSUMPTION:
            // Since it is uncertain which NUMA node will handle the write operations for this task,
            // the worst-case scenario is assumed: the data is written to the farthest NUMA node,
            // i.e., the one with the highest write time relative to the specified hwloc core ID.

            for (size_t i = 0; i < this->common->distance_lat_ns.size(); ++i)
            {
                double write_latency_ns = this->common->distance_lat_ns[write_src_numa_id][i];
                double write_bandwidth_gbps = this->common->distance_bw_gbps[write_src_numa_id][i];

                double write_latency_us = write_latency_ns / 1000;         // To microseconds
                double write_bandwidth_bpus = write_bandwidth_gbps * 1000; // To B/us

                estimated_write_time_us =
                    std::max(estimated_write_time_us, write_latency_us + (write_payload_bytes / write_bandwidth_bpus));
            }
        }

        double finish_time_us =
            earliest_start_time_us + estimated_read_time_us + estimated_compute_time_us + estimated_write_time_us;
        best_core_id = (finish_time_us < earliest_finish_time_us) ? core_id : best_core_id;
        earliest_finish_time_us = std::min(finish_time_us, earliest_finish_time_us);
    }

    return {best_core_id, earliest_finish_time_us};
}