#pragma once

#include "common.hpp"
#include "hardware.hpp"

class EFT_Scheduler
{
  protected:
    const common_t *common;
    simgrid_execs_t &dag;

    std::tuple<int, unsigned long> get_best_core_id(const simgrid_exec_t *exec);

  public:
    EFT_Scheduler(const common_t *common, simgrid_execs_t &dag);
    ~EFT_Scheduler();

    virtual bool has_next();
    virtual std::tuple<simgrid_exec_t *, int, unsigned long> next() = 0;
};