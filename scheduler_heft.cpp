#include "scheduler_heft.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(heft_scheduler, "Messages specific to this module.");

HEFT_Scheduler::HEFT_Scheduler(const common_t *common, simgrid_execs_t &dag) : EFT_Scheduler(common, dag)
{
}

HEFT_Scheduler::~HEFT_Scheduler()
{
}

void HEFT_Scheduler::initialize()
{
    this->initialize_compute_and_communication_costs();
    this->initialize_all_upward_ranks();
}

void HEFT_Scheduler::initialize_compute_and_communication_costs()
{
    std::vector<int> core_avail = common_core_id_get_avail(this->common);

    // Initialize average computation costs.
    for (simgrid_exec_t *exec : this->dag)
    {
        if (exec->get_name() == "end") continue; // Skip this exec

        double flops = exec->get_remaining();
        double estimated_compute_time_avg_seconds = 0.0;

        for (int core_id : core_avail)
        {
            double clock_frequency_hz = hardware_hwloc_core_id_get_clock_frequency(this->common, core_id);
            estimated_compute_time_avg_seconds += common_compute_time(this->common, flops, clock_frequency_hz);
        }

        estimated_compute_time_avg_seconds = estimated_compute_time_avg_seconds / ((double) core_avail.size());
        this->name_to_cost_seconds[exec->get_cname()] = estimated_compute_time_avg_seconds;
    }

    // Average lat and bw.
    double latency_avg_ns = 0.0;
    double bandwidth_avg_gbps = 0.0;

    for (int src_core_id : core_avail)
    {
        for (int dst_core_id : core_avail)
        {
            int src_numa_id = hardware_hwloc_numa_id_get_by_core_id(this->common, src_core_id);
            int dst_numa_id = hardware_hwloc_numa_id_get_by_core_id(this->common, dst_core_id);

            latency_avg_ns += this->common->distance_lat_ns[src_numa_id][dst_numa_id];
            bandwidth_avg_gbps += this->common->distance_bw_gbps[src_numa_id][dst_numa_id];
        }
    }

    latency_avg_ns = latency_avg_ns / (double) core_avail.size();
    bandwidth_avg_gbps = bandwidth_avg_gbps / (double) core_avail.size();

    // Initialize average communication costs.
    for (simgrid_exec_t *exec : this->dag)
    {
        for (const auto &succ : exec->get_successors())
        {
            const simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            const simgrid_exec_t *succ_exec = dynamic_cast<simgrid_exec_t *>(succ_comm->get_successors().front().get());
            if (succ_exec && succ_exec->get_name() != "end")
            {
                double payload_bytes = succ_comm->get_remaining();
                this->name_to_cost_seconds[succ_comm->get_cname()] =
                    (latency_avg_ns / 1000000000) + (payload_bytes / bandwidth_avg_gbps);
            }
        }
    }
}

double HEFT_Scheduler::compute_upward_rank(simgrid_exec_t *exec)
{
    // If already computed, return cached value
    if (upward_ranks.find(exec->get_name()) != upward_ranks.end())
        return upward_ranks[exec->get_name()];

    double exec_cost = name_to_cost_seconds[exec->get_name()];

    // Compute max successor rank
    double max_successor_rank = 0.0;
    for (auto &succ : exec->get_successors())
    {
        simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());

        simgrid_exec_t *succ_exec = dynamic_cast<simgrid_exec_t *>(succ_comm->get_successors().front().get());
        double comm_cost = succ_exec->get_name() == "end" ? 0.0 : name_to_cost_seconds[succ_comm->get_name()];
        double succ_rank = succ_exec->get_name() == "end" ? 0.0 : compute_upward_rank(succ_exec);
        max_successor_rank = std::max(max_successor_rank, comm_cost + succ_rank);
    }

    // Compute and cache upward rank
    double rank = exec_cost + max_successor_rank;
    upward_ranks[exec->get_name()] = rank;
    return rank;
}

void HEFT_Scheduler::initialize_all_upward_ranks()
{
    for (simgrid_exec_t *task : dag)
        compute_upward_rank(task);
}

std::tuple<simgrid_exec_t *, int, double> HEFT_Scheduler::next()
{
    int selected_core_id = -1;
    double estimated_finish_time = 0.0;
    simgrid_exec_t *selected_exec = nullptr;

    // Sort by upward rank (descending order)
    simgrid_execs_t ready_execs = common_dag_get_ready_execs(this->dag);

    if (ready_execs.empty())
        return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);

    std::sort(ready_execs.begin(), ready_execs.end(), [this](simgrid_exec_t *a, simgrid_exec_t *b) {
        return this->upward_ranks[a->get_name()] > this->upward_ranks[b->get_name()]; // Higher rank first
    });

    for (simgrid_exec_t *exec : ready_execs) {
        XBT_DEBUG("priority_queued_task: %s, upward_rank: %f", exec->get_cname(), this->upward_ranks[exec->get_name()]);
    }

    selected_exec = ready_execs.front();

    std::tie(selected_core_id, estimated_finish_time) = this->get_best_core_id(selected_exec);

    XBT_DEBUG("selected_task: %s, selected_core_id: %d, estimated_finish_time: %f", selected_exec->get_cname(), selected_core_id, estimated_finish_time);

    return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
}
