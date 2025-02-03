#include "mapper_bare_metal.hpp"


Mapper_Bare_Metal::Mapper_Bare_Metal(const MIN_MIN_Scheduler& scheduler, const common_t *common) : scheduler(scheduler), common(common)
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
    std::string task_name;
    unsigned int core_id;
    unsigned long estimated_completion_time;
    std::tie(task_name, core_id, estimated_completion_time) = this->scheduler.next();

    std::cout << "Task: " << task_name << std::endl;
    std::cout << "Core ID: " << core_id << std::endl;
    std::cout << "ECT (us): " << estimated_completion_time << std::endl;
    std::cout << "has_next: " << this->scheduler.has_next() << std::endl;
}