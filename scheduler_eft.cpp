#include "scheduler_eft.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(eft_scheduler, "Messages specific to this module.");

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
    std::vector<int> core_id_avail = common_core_id_get_avail(this->common);

    for (int core_id : core_id_avail)
    {
        /* 1. ESTIMATE EARLIEST_START_TIME(n_i). */
        double earliest_start_time_us = common_earliest_start_time(this->common, exec->get_name(), core_id);

        XBT_DEBUG("task: %s, core_id: %d, earliest_start_time_us: %f", exec->get_cname(), core_id, earliest_start_time_us);

        /* 2. ESTIMATE READ_TIME(EXEC) */

        double estimated_read_time_us = 0.0;

        // Determine the NUMA node corresponding to the core that will perform the reading operation.
        int read_dst_numa_id = hardware_hwloc_numa_id_get_by_core_id(this->common, core_id);

        // Match all communication (Task1->Task2) where this task_name is the destination.
        name_to_time_range_payload_t matches = common_comm_name_to_w_time_offset_payload_filter(this->common, exec->get_name());

        for (const auto &[comm_name, time_range_payload] : matches)
        {
            double read_payload_bytes = (double) std::get<2>(time_range_payload);

            // ASSUMPTION:
            // For the read time estimation, we assume that the entire data item is stored in a single memory domain, the first one.
            int read_src_numa_id = common_comm_name_to_numa_ids_w_get(this->common, comm_name).front();
            
            double read_time_us = common_communication_time(this->common, read_src_numa_id, read_dst_numa_id, read_payload_bytes);
            
            estimated_read_time_us = std::max(estimated_read_time_us, read_time_us);
            
            XBT_DEBUG("task: %s, core_id: %d, comm_name: %s, read_time_us: %f, payload: %f, estimated_read_time_us: %f",
                exec->get_cname(), core_id, comm_name.c_str(), read_time_us, read_payload_bytes, estimated_read_time_us);
        }

        /* 3. ESTIMATE COMPUTE_TIME(EXEC). */

        double flops = exec->get_remaining();
        double clock_frequency_hz = hardware_hwloc_core_id_get_clock_frequency(this->common, core_id);
        double estimated_compute_time_us = common_compute_time(this->common, flops, clock_frequency_hz);

        XBT_DEBUG("task: %s, core_id: %d, estimated_compute_time_us: %f", exec->get_cname(), core_id, estimated_compute_time_us);

        /* 4. ESTIMATE WRITE_TIME(EXEC) */

        double estimated_write_time_us = 0.0;

        // Retrieve the NUMA node that will perform the write operation.
        int write_src_numa_id = hardware_hwloc_numa_id_get_by_core_id(this->common, core_id);

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

            double write_time_us = common_communication_time(this->common, write_src_numa_id, write_src_numa_id, write_payload_bytes);
            estimated_write_time_us = std::max(estimated_write_time_us, write_time_us);

            XBT_DEBUG("task: %s, core_id: %d, comm_name: %s, write_time_us: %f, payload: %f, estimated_read_time_us: %f",
                exec->get_cname(), core_id, comm->get_cname(), write_time_us, write_payload_bytes, estimated_read_time_us);
        }

        double finish_time_us =
            earliest_start_time_us + estimated_read_time_us + estimated_compute_time_us + estimated_write_time_us;

        XBT_DEBUG("task: %s, core_id: %d, finish_time_us: %f", exec->get_cname(), core_id, finish_time_us);
            
        if (finish_time_us < earliest_finish_time_us) {
            best_core_id = core_id;
            earliest_finish_time_us = finish_time_us;
        }
    }

    XBT_DEBUG("task: %s, best_core_id: %d, earliest_finish_time_us: %f", exec->get_cname(), best_core_id, earliest_finish_time_us);

    return {best_core_id, earliest_finish_time_us};
}
