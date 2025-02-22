#include "mapper_base.hpp"

Mapper_Base::Mapper_Base(common_t *common, scheduler_t &scheduler) : common(common), scheduler(scheduler)
{
    /* CREATE DUMMY HOST (CORE) */
    this->dummy_net_zone = simgrid::s4u::create_full_zone("zone0");
    this->dummy_host = dummy_net_zone->create_host("host0", "1Gf")->seal();
    this->dummy_net_zone->seal();
}

void Mapper_Base::set_thread_func_ptr(void *(*func)(void *))
{
    this->thread_func_ptr = func;
}
