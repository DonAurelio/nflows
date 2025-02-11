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

    std::tuple<simgrid_exec_t *, int, unsigned long> next() override;
};