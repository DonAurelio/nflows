#pragma once

#include <xbt/log.h>

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_eft.hpp"

class MIN_MIN_Scheduler : public EFT_Scheduler
{
  public:
    MIN_MIN_Scheduler(const common_t *common, simgrid_execs_t &dag);
    ~MIN_MIN_Scheduler();

    void initialize() override;

    std::tuple<simgrid_exec_t *, int, double> next() override;
};

typedef MIN_MIN_Scheduler min_min_scheduler_t;
