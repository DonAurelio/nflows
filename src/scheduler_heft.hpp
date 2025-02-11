#pragma once

#include "common.hpp"
#include "hardware.hpp"

#include <xbt/log.h>


class HEFT_Scheduler {
    private:
        const common_t *common;
        simgrid_execs_t& dag;

        std::unordered_map<std::string, double> upward_ranks;
        std::unordered_map<std::string, double> name_to_cost_seconds;

        void initialize_compute_and_communication_costs();
        double compute_upward_rank(simgrid_exec_t* task);
        void initialize_all_upward_ranks();
        std::tuple<int, unsigned long> get_best_core_id(const simgrid_exec_t *exec);

    public:
        HEFT_Scheduler(const common_t *common, simgrid_execs_t &dag);
        ~HEFT_Scheduler();

        bool has_next();
        std::tuple<simgrid_exec_t*, int, unsigned long> next();
        void print();
};