#pragma once

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_base.hpp"
#include "scheduler_eft.hpp"
#include "scheduler_heft.hpp"
#include "scheduler_min_min.hpp"

#include <xbt/log.h>

class Mapper_Bare_Metal
{
  private:
    common_t *common;
    simgrid_host_t *dummy_host;
    simgrid_netzone_t *dummy_net_zone;
    scheduler_t &scheduler;

  public:
    Mapper_Bare_Metal(common_t *common, scheduler_t &scheduler);
    ~Mapper_Bare_Metal();

    void start();
};

typedef Mapper_Bare_Metal mapper_t;

void *thread_function(void *arg);
