#pragma once

#include <xbt/log.h>

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_eft.hpp"

class FIFO_Scheduler : public Base_Scheduler
{
  protected:
    std::tuple<int, double> get_best_core_id(const simgrid_exec_t *exec) override;

  public:
    FIFO_Scheduler(const common_t *common, simgrid_execs_t &dag);
    ~FIFO_Scheduler();

    std::tuple<simgrid_exec_t *, int, double> next() override;
};
