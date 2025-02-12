#include "mapper_bare_metal.hpp"
#include "scheduler_base.hpp"
#include "scheduler_heft.hpp"
#include "scheduler_min_min.hpp"

#include <iostream>
#include <xbt/log.h>

int main(int argc, char *argv[])
{
    // Initialize XBT logging system
    xbt_log_init(&argc, argv);

    /* INITIALIZE COMMON DATA. */
    common_t *common = new common_t;

    /* HWLOC TOPOLOGY */
    hwloc_topology_init(&common->topology);
    hwloc_topology_load(common->topology);

    /* TRACK ACTIVE THREADS. */
    common->active_threads = 0;
    common->cond = PTHREAD_COND_INITIALIZER;
    common->mutex = PTHREAD_MUTEX_INITIALIZER;

    /* CHECK READING OPERATION CONSISTENCY */
    common->checksum = 0;

    // Reference: https://www.intel.la/content/www/xl/es/architecture-and-technology/avx-512-overview.html
    // Applications can pack 32 double precision and 64 single precision floating point operations per clock
    // cycle within the 512-bit vectors.
    common->flops_per_cycle = 32;
    common->clock_frequency_hz = 10; // Dynamic (0) clock frequency.

    // Latency (ns), Bandwidth (GB/s).
    common->distance_lat_ns = common_read_distance_matrix_from_file(
        "/home/cc/runtime_workflow_scheduler/sample/distances/latency_matrix.txt");
    common->distance_bw_gbps = common_read_distance_matrix_from_file(
        "/home/cc/runtime_workflow_scheduler/sample/distances/bandwidth_matrix.txt");

    common->core_avail = {
        true,  false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, true,
    };

    simgrid_execs_t dag =
        common_read_dag_from_dot("/home/cc/runtime_workflow_scheduler/sample/workflows/0.distribution.dot");

    MIN_MIN_Scheduler scheduler(common, dag);
    Mapper_Bare_Metal min_min_mapper_bare_metal(common, scheduler);
    min_min_mapper_bare_metal.start();

    std::string min_min_filename = common_get_timestamped_filename("min_min_output");
    std::ofstream min_min_file(min_min_filename);
    common_print_common_structure(common, min_min_file);
    min_min_file.close();

    // HEFT_Scheduler scheduler(common, dag);
    // Mapper_Bare_Metal heft_mapper_bare_metal(common, scheduler);
    // heft_mapper_bare_metal.start();

    // std::string heft_filename = common_get_timestamped_filename("heft_output");
    // std::ofstream heft_file(heft_filename);
    // common_print_common_structure(common, heft_file);
    // heft_file.close();

    delete common;

    return 0;
}