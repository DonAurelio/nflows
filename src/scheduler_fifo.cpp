#include "scheduler_fifo.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(fifo_scheduler, "Messages specific to this module.");

FIFO_Scheduler::FIFO_Scheduler(const common_t *common, simgrid_execs_t &dag) : Base_Scheduler(common, dag)
{
}

FIFO_Scheduler::~FIFO_Scheduler()
{
}

std::tuple<int, double> FIFO_Scheduler::get_best_core_id(const simgrid_exec_t *exec)
{
    int best_core_id = -1;
    double earliest_finish_time_us = 0.0;
    std::vector<int> avail_core_ids = common_get_avail_core_ids(this->common);

    if (avail_core_ids.empty())
    {
        return {best_core_id, earliest_finish_time_us};
    }

    /* COUNT THE AMOUNT OF DATA TO BE READ BY THE EXEC PER NUMA DOMAIN. */
    std::unordered_map<int, double> numa_id_to_payload;

    // Get all writtings (data items) where this exec is the destionation.
    name_to_time_range_payload_t comm_name_to_time_range_payload =
        common_filter_name_ts_range_payload(this->common, exec->get_name(), COMM_WRITE, DST);

    for (const auto &[comm_name, time_range_payload] : comm_name_to_time_range_payload)
    {
        // ASSUMPTION:
        // If data item pages are spread across multiple NUMA domains, we assume
        // the data is evenly distributed, i.e., all data pages have the same size.
        std::vector<int> numa_ids = this->common->comm_name_to_numa_ids_w.at(comm_name);
        for (int numa_id : numa_ids)
        {
            numa_id_to_payload[numa_id] = std::get<2>(time_range_payload) / numa_ids.size();
        }
    }

    /* SORT NUMA DOMAIN IDS BY AMOUNT OF DATA TO BE READ BY THE EXEC. */
    std::vector<int> ordered_numa_ids;
    ordered_numa_ids.reserve(numa_id_to_payload.size());

    // Copy keys into the vector
    for (const auto &[key, _] : numa_id_to_payload)
        ordered_numa_ids.push_back(key);

    // Sort keys based on their corresponding values in the map
    std::sort(ordered_numa_ids.begin(), ordered_numa_ids.end(),
              [numa_id_to_payload](int a, int b) { return numa_id_to_payload.at(a) < numa_id_to_payload.at(b); });

    /* ASSIGN THE EXECUTOR TO THE CORE WHOSE LOCAL MEMORY (NUMA DOMAIN) CONTAINS THE MOST DATA. */
    for (const auto &ordered_numa_id : ordered_numa_ids)
    {
        for (const auto &avail_core_id : avail_core_ids)
        {
            if (get_hwloc_numa_id_by_core_id(this->common, avail_core_id) == ordered_numa_id)
            {
                best_core_id = avail_core_id;
            }
        }
    }

    // IF NONE OF THE AVAILABLE CORES CORRESPOND TO ANY OF THE NUMA DOMAINS
    // HOLDING THE DATA REQUIRED BY THE EXEC, ASSIGN THE FIRST AVAILABLE CORE.
    best_core_id = (best_core_id == -1) ? avail_core_ids.front() : best_core_id;

    return {best_core_id, earliest_finish_time_us};
}

std::tuple<simgrid_exec_t *, int, double> FIFO_Scheduler::next_b()
{
    int selected_core_id = -1;
    double estimated_finish_time = 0.0;
    simgrid_exec_t *selected_exec = nullptr;
    simgrid_execs_t ready_execs = common_get_ready_tasks(this->dag);

    if (ready_execs.empty())
    {
        return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
    }

    std::unordered_map<std::string, uint64_t> name_to_data_locality_scores =
        this->get_data_locality_scores(ready_execs);

    std::sort(ready_execs.begin(), ready_execs.end(), [&](simgrid_exec_t *a, simgrid_exec_t *b) {
        return name_to_data_locality_scores[a->get_name()] >
               name_to_data_locality_scores[b->get_name()]; // Higher score first
    });

    selected_exec = ready_execs.front();
    std::tie(selected_core_id, estimated_finish_time) = this->get_best_core_id(selected_exec);

    XBT_DEBUG("selected_task: %s, selected_core_id: %d, estimated_finish_time: %f", selected_exec->get_cname(),
              selected_core_id, estimated_finish_time);

    return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
}

std::tuple<simgrid_exec_t *, int, double> FIFO_Scheduler::next_s()
{
}

uint64_t FIFO_Scheduler::compute_data_locality_score(simgrid_exec_t *exec)
{
    uint64_t exec_data_locality_score_bytes = 0;

    // Get all writtings (data items) where this exec is the destionation.
    name_to_time_range_payload_t comm_name_to_time_range_payload =
        common_filter_name_ts_range_payload(this->common, exec->get_name(), COMM_WRITE, DST);

    for (const auto &[comm_name, time_range_payload] : comm_name_to_time_range_payload)
    {
        exec_data_locality_score_bytes += std::get<2>(time_range_payload);
    }

    return exec_data_locality_score_bytes;
}

std::unordered_map<std::string, uint64_t> FIFO_Scheduler::get_data_locality_scores(simgrid_execs_t &execs)
{
    std::unordered_map<std::string, uint64_t> name_to_data_locality_scores;
    for (simgrid_exec_t *exec : execs)
    {
        name_to_data_locality_scores[exec->get_name()] = this->compute_data_locality_score(exec);
    }

    return name_to_data_locality_scores;
}
