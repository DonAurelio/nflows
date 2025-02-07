#pragma once

#include "common.hpp"
#include "hardware.hpp"

#include <xbt/log.h>


class HEFT_Scheduler {
    private:
        const common_t *common;
        simgrid_execs_t& dag;

        void initialize_compute_and_communication_cost();
        std::tuple<int, unsigned long> get_best_core_id(const simgrid_exec_t *exec);
    public:
        HEFT_Scheduler(const common_t *common, simgrid_execs_t &dag);
        ~HEFT_Scheduler();
};