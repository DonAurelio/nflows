#pragma once

#include <xbt/log.h>

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_eft.hpp"



class HEFT_Scheduler : public EFT_Scheduler {
    private:
        std::unordered_map<std::string, double> upward_ranks;
        std::unordered_map<std::string, double> name_to_cost_seconds;

        void initialize_compute_and_communication_costs();
        double compute_upward_rank(simgrid_exec_t* task);
        void initialize_all_upward_ranks();

    public:
        HEFT_Scheduler(const common_t *common, simgrid_execs_t &dag);
        ~HEFT_Scheduler();

        void print();

        std::tuple<simgrid_exec_t*, int, unsigned long> next() override;
};