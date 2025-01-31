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
typedef std::vector<simgrid_exec_t *> simgrid_execs_t;

typedef std::vector<std::vector<double>> distance_matrix_t;

struct common_s
{
    // Harware locality config.
    hwloc_topology_t *topology;

    // Pthreads config.
    int *active_threads;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;

    // To be initialized by the user.
    unsigned long flops_per_cycle;
    unsigned long clock_frequency_hz;

    distance_matrix_t *latency_matrix;
    distance_matrix_t *bandwidth_matrix;

    std::vector<bool> *hwloc_core_avail;

    // Locality information collections.
    name_to_address_t *comm_name_to_address;
    name_to_numa_ids_t *comm_name_to_numa_ids_r;
    name_to_numa_ids_t *comm_name_to_numa_ids_w;
    name_to_thread_locality_t *exec_name_to_thread_locality;

    // Timing (absolute timestamp) information collections.
    name_to_time_range_payload_t *comm_name_to_r_ts_range_payload;
    name_to_time_range_payload_t *comm_name_to_w_ts_range_payload;
    name_to_time_range_payload_t *exec_name_to_c_ts_range_payload;

    // Timing (relative durations/offset) information collections.
    name_to_time_range_payload_t *exec_name_to_time_offset_payload;
    name_to_time_range_payload_t *comm_name_to_time_offset_payload;
};
typedef struct common_s common_t;

simgrid_execs_t common_read_dag_from_dot(const char *file_path);
simgrid_execs_t common_get_ready_tasks(const simgrid_execs_t &execs);
