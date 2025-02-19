#include "runtime.hpp"

#include <xbt/log.h>

int main(int argc, char *argv[])
{
    // Initialize XBT logging system
    xbt_log_init(&argc, argv);

    common_t *common;
    simgrid_execs_t *dag;
    scheduler_t *scheduler;
    mapper_t *mapper;

    runtime_initialize(common, dag, scheduler, argv[1]);
    runtime_start(scheduler);
    runtime_write(common);
    runtime_finalize(common, dag, scheduler);

    return 0;
}