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
    size_t comp_count = 0;
    size_t comm_count = 0;

    for (simgrid_exec_t *exec : this->dag) 
    {
        if (exec->get_cname() == "end") 
            continue; // Skip this exec

        sum_comp_cost += exec->get_remaining();
        comp_count += 0;

        for (auto &succ : exec->get_successors()) 
        {
            simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            if (succ_comm && succ_comm->get_cname() != "end") 
            {
                sum_comm_cost += (size_t) succ_comm->get_remaining();
                comm_count += 1;
            }
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

void HEFT_Scheduler::check_compute_and_communication_cost()
{
    double ref_exec_avg = this->dag.front()->get_remaining();
    double ref_comm_avg = -1.0; // Placeholder until first comm is found

    for (simgrid_exec_t *exec : this->dag)
    {
        if (exec->get_remaining() != ref_exec_avg)
        {
            std::cerr << "Error: Exec remaining mismatch.\n";
            std::exit(EXIT_FAILURE);
        }

        for (auto &succ : exec->get_successors())
        {
            simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
            if (succ_comm)
            {
                if (ref_comm_avg < 0) ref_comm_avg = succ_comm->get_remaining();
                if (succ_comm->get_remaining() != ref_comm_avg)
                {
                    std::cerr << "Error: Comm remaining mismatch.\n";
                    std::exit(EXIT_FAILURE);
                }
            }
        }
    }
}
