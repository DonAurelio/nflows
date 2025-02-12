#include "scheduler_fifo.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(fifo_scheduler, "Messages specific to this module.");

std::tuple<int, double> FIFO_Scheduler::get_best_core_id(const simgrid_exec_t *exec)
{
    int best_core_id = -1;
    double earliest_finish_time_us = 0.0;
    std::vector<int> avail_core_ids = common_get_avail_core_ids(this->common);

    if (!avail_core_ids.empty()) {
        best_core_id = avail_core_ids.front();
    }

    return {best_core_id, earliest_finish_time_us};
}

std::tuple<simgrid_exec_t *, int, double> FIFO_Scheduler::next()
{
    int selected_core_id = -1;
    double estimated_finish_time = 0.0;
    simgrid_exec_t *selected_exec = nullptr;
    simgrid_execs_t ready_execs = common_get_ready_tasks(this->dag);

    if (ready_execs.empty())
    {
        return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
    }

    selected_exec = ready_execs.front();
    std::tie(selected_core_id, estimated_finish_time) = this->get_best_core_id(selected_exec);

    XBT_DEBUG("selected_task: %s, selected_core_id: %d, estimated_finish_time: %f", selected_exec->get_cname(),
              selected_core_id, estimated_finish_time);

    return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
}
