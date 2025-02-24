#include "scheduler_eft.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(scheduler_eft, "Messages specific to this module.");

EFT_Scheduler::EFT_Scheduler(const common_t *common, simgrid_execs_t &dag) : Base_Scheduler(common, dag)
{
}

EFT_Scheduler::~EFT_Scheduler()
{
}

std::tuple<int, double> EFT_Scheduler::get_best_core_id(const simgrid_exec_t *exec)
{
    int best_core_id = -1;
    double earliest_finish_time_us = std::numeric_limits<double>::max();

    // Estimate exec earliest_finish_time for every core_id.
    for (int core_id : common_get_avail_core_ids(this->common))
    {
        /* 1. ESTIMATE EARLIEST_START_TIME(n_i). */
        double earliest_start_time_us = common_earliest_start_time(this->common, exec->get_name(), core_id);

        /* 2. ESTIMATE READ_TIME(EXEC) */

        double estimated_read_time_us = 0.0;

        // Determine the NUMA node corresponding to the core that will perform the reading operation.
        int read_dst_numa_id = get_hwloc_numa_id_by_core_id(this->common, core_id);

        // Match all communication (Task1->Task2) where this task_name is the destination.
        for (const auto &[comm_name, time_range_payload] : common_filter_name_to_time_range_payload(this->common, exec->get_name(), COMM_WRITE_OFFSETS, DST))
        {
            double read_payload_bytes = (double) std::get<2>(time_range_payload);

            // ASSUMPTION:
            // For the read time estimation, we assume that the entire data item is stored in a single memory domain, the first one.
            int read_src_numa_id = this->common->comm_name_to_numa_ids_w.at(comm_name).front();

            estimated_read_time_us = std::max(
                estimated_read_time_us, common_communication_time(this->common, read_src_numa_id, read_dst_numa_id, read_payload_bytes));
        }

        /* 3. ESTIMATE COMPUTE_TIME(EXEC). */

        double flops = exec->get_remaining();
        double processor_speed_flops_per_second = (double) get_hwloc_core_performance_by_id(this->common, core_id);
        double estimated_compute_time_us = common_compute_time(this->common, flops, processor_speed_flops_per_second);

        /* 4. ESTIMATE WRITE_TIME(EXEC) */

        double estimated_write_time_us = 0.0;

        // Retrieve the NUMA node that will perform the write operation.
        int write_src_numa_id = get_hwloc_numa_id_by_core_id(this->common, core_id);

        for (const auto &succ : exec->get_successors())
        {
            const simgrid_comm_t *comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            double write_payload_bytes = comm->get_remaining();

            // Skipt all task_i->end communications.
            auto [exec_name, succ_exec_name] = common_split(comm->get_cname(), "->");
            if (succ_exec_name == std::string("end")) continue;

            // Determine the NUMA node that will handle the write operation.
            // ASSUMPTION:
            // Since it is uncertain which NUMA node will handle the write operations for this task,
            // writes will follow the first-touch policy, i.e., data will be saved in 
            // the numa node that share locality with the core_id.

            estimated_write_time_us = std::max(
                estimated_write_time_us, common_communication_time(
                    this->common, write_src_numa_id, write_src_numa_id, write_payload_bytes));
        }

        double finish_time_us =
            earliest_start_time_us + estimated_read_time_us + estimated_compute_time_us + estimated_write_time_us;
            
        if (finish_time_us < earliest_finish_time_us) {
            best_core_id = core_id;
            earliest_finish_time_us = finish_time_us;
        }
    }

    return {best_core_id, earliest_finish_time_us};
}
