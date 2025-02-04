#pragma once

#include "common.hpp"
#include "hardware.hpp"

#include <xbt/log.h>


class MIN_MIN_Scheduler {
    private:
        const common_t *common;
        simgrid_execs_t& dag;

        std::tuple<unsigned int, unsigned long> get_best_core_id(const simgrid_exec_t *exec);
    public:
        MIN_MIN_Scheduler(const common_t *common, simgrid_execs_t &dag);
        ~MIN_MIN_Scheduler();

        bool has_next();
        std::tuple<simgrid_exec_t*, unsigned int, unsigned long> next();
};