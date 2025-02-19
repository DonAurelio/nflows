#include "mapper_bare_metal.hpp"
#include "scheduler_base.hpp"
#include "scheduler_heft.hpp"
#include "scheduler_min_min.hpp"
#include "scheduler_fifo.hpp"

#include <iostream>
#include <xbt/log.h>

int main(int argc, char *argv[])
{
    // Initialize XBT logging system
    xbt_log_init(&argc, argv);

    common_t *common;
    simgrid_execs_t *dag;
    scheduler_t *scheduler;

    common_initialize(common, dag, scheduler, argv[1]);

    scheduler.start();

    common_print_common_structure(common);
    common_finalize(common, dag, scheduler);

    return 0;
}