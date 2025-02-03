#include "scheduler_min_min.hpp"

#include <iostream>


int main (int argc, char* argv[])
{
    /* INITIALIZE COMMON DATA. */
    common_t *common = new common_t;

    /* HWLOC TOPOLOGY */
    hwloc_topology_init(&common->topology);
    hwloc_topology_load(common->topology);

    /* TRACK ACTIVE THREADS. */
    common->active_threads = 0;
    common->cond = PTHREAD_COND_INITIALIZER;
    common->mutex = PTHREAD_MUTEX_INITIALIZER;

    // Reference: https://www.intel.la/content/www/xl/es/architecture-and-technology/avx-512-overview.html
    // Applications can pack 32 double precision and 64 single precision floating point operations per clock
    // cycle within the 512-bit vectors.
    common->flops_per_cycle = 32;
    common->clock_frequency_hz = 0; // Dynamic (0) clock frequency.

    // Latency (ns), Bandwidth (GB/s).
    common->distance_lat_ns = common_read_distance_matrix_from_file(
        "/home/cc/runtime_workflow_scheduler/data/latency_matrix.txt");
    common->distance_bw_gbps = common_read_distance_matrix_from_file(
        "/home/cc/runtime_workflow_scheduler/data/bandwidth_matrix.txt");

    common->core_avail = {
        true, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false, false, false, false, true,
    };

    simgrid_execs_t dag = common_read_dag_from_dot("/home/cc/runtime_workflow_scheduler/data/test/0.distribution.dot");
    MIN_MIN_Scheduler scheduler(dag, common);

    std::string task_name;
    unsigned int core_id;
    unsigned long estimated_completion_time;
    std::tie(task_name, core_id, estimated_completion_time) = scheduler.next();

    std::cout << "Task: " << task_name << std::endl;
    std::cout << "Core ID: " << core_id << std::endl;
    std::cout << "ECT (us): " << estimated_completion_time << std::endl;

    delete common;

    return 0;
}