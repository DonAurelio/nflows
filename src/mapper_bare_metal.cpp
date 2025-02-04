#include "mapper_bare_metal.hpp"


Mapper_Bare_Metal::Mapper_Bare_Metal(const common_t *common, MIN_MIN_Scheduler& scheduler) : common(common), scheduler(scheduler)
{
    /* CREATE DUMMY HOST (CORE) */
    this->dummy_net_zone = simgrid::s4u::create_full_zone("zone0");
    this->dummy_host = dummy_net_zone->create_host("host0", "1Gf")->seal();
    this->dummy_net_zone->seal();
}

Mapper_Bare_Metal::~Mapper_Bare_Metal()
{

}

void Mapper_Bare_Metal::start()
{
    simgrid_exec_t* task_name;
    unsigned int core_id;
    unsigned long estimated_completion_time;
    std::tie(task_name, core_id, estimated_completion_time) = this->scheduler.next();

    std::cout << "Task: " << task_name->get_cname() << std::endl;
    std::cout << "Core ID: " << core_id << std::endl;
    std::cout << "ECT (us): " << estimated_completion_time << std::endl;
    std::cout << "has_next: " << this->scheduler.has_next() << std::endl;
    this->set_as_assigned(task_name);
    this->set_as_completed(task_name);
}

void Mapper_Bare_Metal::set_as_assigned(simgrid_exec_t *exec)
{
    exec->set_host(this->dummy_host);
}

void Mapper_Bare_Metal::set_as_completed(simgrid_exec_t *exec)
{
    for (const auto &succ_ptr : exec->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);
    exec->complete(simgrid::s4u::Activity::State::FINISHED);
}
