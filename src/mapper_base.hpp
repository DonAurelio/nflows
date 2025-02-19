#pragma once

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_base.hpp"
#include "scheduler_eft.hpp"
#include "scheduler_heft.hpp"
#include "scheduler_min_min.hpp"

#include <xbt/log.h>

class Mapper_Base
{
  protected:
    common_t *common;
    simgrid_host_t *dummy_host;
    simgrid_netzone_t *dummy_net_zone;
    scheduler_t &scheduler;

  public:
    Mapper_Base(common_t *common, scheduler_t &scheduler);
    virtual ~Mapper_Base() = default;

    virtual void start();
};

typedef Mapper_Base mapper_t;
