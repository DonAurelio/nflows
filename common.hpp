#include <hwloc.h>

#include <simgrid/s4u.hpp>
#include <iomanip>
#include <map>
#include <string>
#include <tuple>
#include <vector>


struct thread_locality_s
{
    int numa_id;
    int core_id;
    long voluntary_context_switches;
    long involuntary_context_switches;
    long core_migrations;
};
typedef struct thread_locality_s thread_locality_t;

// Maps data item names (e.g., "task1->task2") to their virtual addresses.
typedef std::unordered_map<std::string, char *> name_to_address_t;
// Maps data item names (e.g., "task1->task2") to the NUMA nodes where their pages reside.
typedef std::unordered_map<std::string, std::vector<int>> name_to_numa_ids_t;
// Maps task names to their executing thread locality information.
typedef std::unordered_map<std::string, thread_locality_t> name_to_thread_locality_t;

// Time ranges can be absolute (timestamps) or relative (durations or offsets).
typedef std::tuple<uint64_t, uint64_t, uint64_t> time_range_payload_t;
typedef std::unordered_map<std::string, time_range_payload_t> name_to_time_range_payload_t;

typedef simgrid::s4u::Exec simgrid_exec_t;
typedef simgrid::s4u::Comm simgrid_comm_t;
typedef simgrid::s4u::Host simgrid_host_t;
typedef simgrid::s4u::Activity simgrid_activity_t;
typedef simgrid::s4u::ActivityPtr simgrid_activity_ptr_t;
typedef std::vector<simgrid_activity_ptr_t> simgrid_activity_ptrs_t;
typedef std::vector<simgrid_exec_t *> simgrid_execs_t;
typedef std::vector<std::vector<double>> matrix_t;
typedef std::vector<std::vector<double>> matrix_t;
typedef std::vector<bool> hwloc_core_ids_availability_t;
typedef std::vector<int> hwloc_core_ids_t;

typedef std::pair<int, double> hwloc_core_id_completion_time_t;
typedef std::tuple<simgrid_exec_t *, int, double> exec_hwloc_core_id_completion_time_t;

enum MatchPart
{
    FIRST,
    SECOND
};

struct common_data_s
{
    hwloc_topology_t *topology;

    int *active_threads;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;

    int *numa_nodes_num;
    matrix_t *latency_matrix;
    matrix_t *bandwidth_matrix;

    unsigned long flops_per_cycle;
    unsigned long clock_frequency_hz;

    comm_name_to_ptr_t *comm_name_to_ptr;
    comm_name_to_numa_id_t *comm_name_to_numa_id;
    comm_name_to_numa_id_t *comm_name_to_numa_id_read;
    exec_name_to_locality_t *exec_name_to_locality;

    comm_name_to_time_t *comm_name_to_read_time;
    exec_name_to_compute_time_t *exec_name_to_compute_time;
    comm_name_to_time_t *comm_name_to_write_time;

    // Example: Root->Task1
    // Root: start = 0, finish = read_time + compute_time + write_time.
    // Task1: start = finish(Root), finish = read_time + compute_time + write_time.
    exec_name_to_time_t *exec_name_to_time;

    hwloc_core_ids_availability_t *hwloc_core_ids_availability;
};
typedef struct common_data_s common_data_t;