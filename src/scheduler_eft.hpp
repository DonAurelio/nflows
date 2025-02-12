#pragma once

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_base.hpp"

class EFT_Scheduler : public Base_Scheduler
{
  protected:
    std::tuple<int, double> get_best_core_id(const simgrid_exec_t *exec) override;

  public:
    EFT_Scheduler(const common_t *common, simgrid_execs_t &dag);
    ~EFT_Scheduler();
};