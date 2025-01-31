#include "common.hpp"


simgrid_execs_t read_dag_from_dot(const char *file_path)
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

simgrid_execs_t get_ready_tasks(const simgrid_execs_t &execs)
{
    simgrid_execs_t ready_execs;
    for (auto &exec : execs)
        if (exec->dependencies_solved() && not exec->is_assigned())
            ready_execs.push_back(exec);

    return ready_execs;
}