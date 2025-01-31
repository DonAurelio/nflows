#include "common.hpp"


simgrid_execs_t common_read_dag_from_dot(const char *file_path)
{
    simgrid_execs_t execs;
    for (auto &activity : simgrid::s4u::create_DAG_from_dot(file_path))
        if (auto *exec = dynamic_cast<simgrid::s4u::Exec *>(activity.get()))
            execs.push_back((simgrid_exec_t *)exec);

    /* DELETE ENTRY/EXIT TASK */
    // TODO: The last tasks still have end as dependency.
    //  We need to remove this
    simgrid_exec_t *root = execs.front();

    for (const auto &succ_ptr : root->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);
    root->complete(simgrid::s4u::Activity::State::FINISHED);
    execs.erase(execs.begin());

    // simgrid_exec_t *end = execs.back();
    // for (const auto &ances_ptr : end->get_dependencies())
    //     (ances_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);
    // end->complete(simgrid::s4u::Activity::State::FINISHED);
    execs.pop_back();

    return execs;
}

simgrid_execs_t common_get_ready_tasks(const simgrid_execs_t &execs)
{
    simgrid_execs_t ready_execs;
    for (auto &exec : execs)
        if (exec->dependencies_solved() && not exec->is_assigned())
            ready_execs.push_back(exec);

    return ready_execs;
}

std::vector<int> common_get_avail_core_ids(commot_t *common)
{
    std::vector<int> avail_core_ids;
    for (auto i, value : std::views::enumerate(common->core_avail))
        if (value) avail_core_ids.push_back(i);

    return avail_core_ids;
}

std::vector<name_to_time_range_payload_t> common_get_name_ts_range_payload(common_t *common)
{

}

comm_name_time_ranges_t find_matching_time_ranges(const comm_name_to_time_t *map, const std::string &name, MatchPart partToMatch)
{
    comm_name_time_ranges_t matches;

    for (const auto &entry : *map)
    {
        std::string key = entry.first; // Convert to std::string for easy manipulation
        const time_range_t &timeRange = entry.second;

        auto [firstName, secondName] = split_by_arrow(key);

        // Check if the specified part matches the name
        if ((partToMatch == FIRST && firstName == name) || (partToMatch == SECOND && secondName == name))
        {
            matches.push_back(std::make_tuple(key.c_str(), std::get<0>(timeRange), std::get<1>(timeRange), std::get<2>(timeRange)));
        }
    }
    return matches;
}