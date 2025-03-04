#pragma once

#include <xbt/log.h>

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_eft.hpp"

class FIFO_Scheduler : public Base_Scheduler
{
  private:
    std::deque<simgrid_exec_t*> queue;

    double compute_data_locality_score(simgrid_exec_t *task);
    std::unordered_map<std::string, double> get_data_locality_scores(simgrid_execs_t &execs);

  protected:
    std::tuple<int, double> get_best_core_id(const simgrid_exec_t *exec) override;

  public:
    FIFO_Scheduler(const common_t *common, simgrid_execs_t &dag);
    ~FIFO_Scheduler();

    std::tuple<simgrid_exec_t *, int, double> next() override;
};

typedef FIFO_Scheduler fifo_scheduler_t;
