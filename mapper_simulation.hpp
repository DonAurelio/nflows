#pragma once

#include "mapper_base.hpp"

#include <xbt/log.h>

class Mapper_Simulation : public Mapper_Base
{
  public:
    Mapper_Simulation(common_t *common, scheduler_t &scheduler);
    ~Mapper_Simulation();

    void start() override;
};

typedef Mapper_Simulation mapper_simulation_t;

void *mapper_simulation_thread_function(void *arg);
