#include "scheduler_base.hpp"

Base_Scheduler::Base_Scheduler(const common_t *common, simgrid_execs_t &dag) : common(common), dag(dag)
{
}

Base_Scheduler::~Base_Scheduler()
{
}

bool Base_Scheduler::has_next()
{
    bool has_unassigned = std::any_of(this->dag.begin(), this->dag.end(),
                                      [](const simgrid::s4u::Exec *exec) { return !exec->is_assigned(); });

    return has_unassigned;
}