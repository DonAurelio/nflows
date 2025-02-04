#pragma once

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_min_min.hpp"

#include <xbt/log.h>


class Mapper_Bare_Metal {
    private:
        const common_t *common;
        simgrid_host_t *dummy_host;
        simgrid_netzone_t *dummy_net_zone;
        MIN_MIN_Scheduler& scheduler;

        void assign_exec(simgrid_exec_t *exec);
        void set_as_assigned(simgrid_exec_t *exec);
        void set_as_completed(simgrid_exec_t *exec);
    public:
        Mapper_Bare_Metal(const common_t *common, MIN_MIN_Scheduler& scheduler);
        ~Mapper_Bare_Metal();

        void start();
};