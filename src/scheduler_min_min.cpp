#include "scheduler_min_min.hpp"


MIN_MIN_Scheduler::MIN_MIN_Scheduler(const simgrid_execs_t &dag, const common_t *common)
{
    this->dag = dag;
    this->common = common;
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

    // Estimate exec earliest_finish_time for every core_id.
    for (int core_id : common_get_avail_core_ids(this->common))
    {
        // Match all communication (Task1->Task2) where this task_name is the destination.
        comm_name_time_ranges_t read_comm_name_time_ranges = common_get_name_ts_range_payload(
            this->common, exec->get_name(), COMM_WRITE, DST);
    }

    return {0,0}
}
