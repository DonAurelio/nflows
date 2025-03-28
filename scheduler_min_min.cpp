#include "scheduler_min_min.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(min_min_scheduler, "Messages specific to this module.");

MIN_MIN_Scheduler::MIN_MIN_Scheduler(const common_t *common, simgrid_execs_t &dag) : EFT_Scheduler(common, dag)
{
}

MIN_MIN_Scheduler::~MIN_MIN_Scheduler()
{
}

void MIN_MIN_Scheduler::initialize()
{
}

std::tuple<simgrid_exec_t *, int, double> MIN_MIN_Scheduler::next()
{
    int selected_core_id = -1;
    double estimated_finish_time = std::numeric_limits<double>::max();
    simgrid_exec_t *selected_exec = nullptr;

    for (simgrid_exec_t *exec : common_dag_get_ready_execs(this->dag))
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

    return std::make_tuple(selected_exec, selected_core_id, estimated_finish_time);
}
