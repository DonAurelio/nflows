#pragma once

#include "common.hpp"
#include "hardware.hpp"

class Base_Scheduler
{
  protected:
    const common_t *common;
    simgrid_execs_t &dag;

    virtual std::tuple<int, double> get_best_core_id(const simgrid_exec_t *exec) = 0;

  public:
    Base_Scheduler(const common_t *common, simgrid_execs_t &dag);
    virtual ~Base_Scheduler() = default; // Ensures proper destructor chaining

    virtual bool has_next();
    virtual std::tuple<simgrid_exec_t *, int, double> next() = 0;
};

typedef Base_Scheduler scheduler_t;
