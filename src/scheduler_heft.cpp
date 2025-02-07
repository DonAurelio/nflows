#include "scheduler_heft.hpp"


XBT_LOG_NEW_DEFAULT_CATEGORY(heft_scheduler, "Messages specific to this module.");

HEFT_Scheduler::HEFT_Scheduler(const common_t *common, simgrid_execs_t &dag) : common(common), dag(dag)
{
    this->initialize_compute_and_communication_cost();
}

HEFT_Scheduler::~HEFT_Scheduler()
{

}

void HEFT_Scheduler::initialize_compute_and_communication_cost()
{
    size_t sum_comp_cost = 0;
    size_t sum_comm_cost = 0;
    size_t comm_count = 0;

    for (const simgrid_exec_t *exec: this->dag)
    {
        sum_comp_cost += exec->get_remaining();
        
        for (const auto &succ : exec->get_successors())
        {
            const simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            sum_comm_cost += (size_t) succ_comm->get_remaining();
            comm_count += 1;
        }
    }

    double average_comp_cost = sum_comp_cost / this->dag.size();
    double average_comm_cost = sum_comm_cost / comm_count;

    for (simgrid_exec_t *exec: this->dag)
    {
        exec->set_flops_amount(average_comp_cost);
        
        for (auto &succ : exec->get_successors())
        {
            simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            succ_comm->set_payload_size(average_comm_cost);
        }
    }
}

