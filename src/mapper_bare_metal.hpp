#pragma once

#include "common.hpp"
#include "hardware.hpp"
#include "scheduler_min_min.hpp"

#include <xbt/log.h>


class Mapper_Bare_Metal {
    private:
        const common_t *common;
        const MIN_MIN_Scheduler& scheduler;

        simgrid_host_t *dummy_host;
        simgrid_netzone_t *dummy_net_zone;
    public:
        Mapper_Bare_Metal(const MIN_MIN_Scheduler& scheduler, const common_t *common);
        ~Mapper_Bare_Metal();
        
        void start();
};