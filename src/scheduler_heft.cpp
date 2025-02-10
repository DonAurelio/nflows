#include "scheduler_heft.hpp"


XBT_LOG_NEW_DEFAULT_CATEGORY(heft_scheduler, "Messages specific to this module.");

HEFT_Scheduler::HEFT_Scheduler(const common_t *common, simgrid_execs_t &dag) : common(common), dag(dag)
{
    this->initialize_compute_and_communication_costs();
}

HEFT_Scheduler::~HEFT_Scheduler()
{

}

void HEFT_Scheduler::initialize_compute_and_communication_costs()
{
    std::vector<int> core_avail = common_get_avail_core_ids(this->common);
 
    // Initialize average computation costs.
    for (simgrid_exec_t *exec : this->dag)
    {
        if (exec->get_name() == "end") continue; // Skip this exec

        size_t flops = (size_t) exec->get_remaining(); // # of Float operations to be performed.
        double avg_estimated_compute_time_seconds = 0;
        size_t exec_count = 0;
        for (int core_id : core_avail)
        {
            // # of flops the system can perform in one second.
            unsigned long flops_per_second = get_hwloc_core_performance_by_id(this->common, core_id);
            avg_estimated_compute_time_seconds += (flops / flops_per_second);
            exec_count += 1;
        }

        this->name_to_cost_seconds[exec->get_cname()] = avg_estimated_compute_time_seconds / exec_count;
    }

    // Average lat and bw.
    double avg_latency_ns = 0.0;
    double avg_bandwidth_gbps = 0.0;

    for (int src_core_id : core_avail)
    {
        for (int dst_core_id : core_avail)
        {
            int src_numa_id = get_hwloc_numa_id_by_core_id(this->common, src_core_id);
            int dst_numa_id = get_hwloc_numa_id_by_core_id(this->common, dst_core_id);

            avg_latency_ns += this->common->distance_lat_ns[src_numa_id][dst_numa_id];
            avg_bandwidth_gbps += this->common->distance_bw_gbps[src_numa_id][dst_numa_id];
        }
    }

    avg_latency_ns = avg_latency_ns / core_avail.size();
    avg_bandwidth_gbps = avg_bandwidth_gbps / core_avail.size();

    // Initialize average communication costs.
    for (simgrid_exec_t *exec : this->dag)
    {
        for (const auto &succ : exec->get_successors())
        {
            const simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            const simgrid_exec_t *succ_exec = dynamic_cast<simgrid_exec_t *>(succ_comm->get_successors().front().get());
            if (succ_exec && succ_exec->get_name() != "end")
            {
                size_t payload_bytes = (size_t) succ_comm->get_remaining();
                this->name_to_cost_seconds[succ_comm->get_cname()] = (avg_latency_ns / 1000000000) + (payload_bytes / avg_bandwidth_gbps);
            }
        }
    }

    for (const auto& [key, value] : this->name_to_cost_seconds) {
        std::cout << key << " : " << value << " seconds\n";
    }
}
