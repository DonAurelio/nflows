#include <hwloc.h>
#include <sched.h>
#include <stdint.h>

#include <iomanip>
#include <map>
#include <simgrid/s4u.hpp>
#include <string>
#include <tuple>
#include <vector>

#include "utils.h"

typedef std::unordered_map<std::string, char *> comm_name_to_ptr_t;
typedef std::unordered_map<std::string, int> comm_name_to_numa_id_t;

// start_time, end_time, bytes/flops.
typedef std::tuple<std::string, uint64_t, uint64_t, uint64_t> comm_name_time_range_t;
typedef std::vector<comm_name_time_range_t> comm_name_time_ranges_t;

typedef std::tuple<uint64_t, uint64_t, uint64_t> time_range_t;
typedef std::unordered_map<std::string, time_range_t> comm_name_to_time_t;
typedef std::unordered_map<std::string, time_range_t> exec_name_to_compute_time_t;

typedef std::tuple<uint64_t, uint64_t> time_interval_t;
typedef std::unordered_map<std::string, locality_t> exec_name_to_locality_t;
typedef std::unordered_map<std::string, time_interval_t> exec_name_to_time_t;

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
    matrix_t *bandwdith_matrix;

    comm_name_to_ptr_t *comm_name_to_ptr;
    comm_name_to_numa_id_t *comm_name_to_numa_id;
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

struct exec_data_s
{
    simgrid_exec_t *exec;
    common_data_t *common_data;
    int assigned_hwloc_core_id;
};
typedef struct exec_data_s exec_data_t;

simgrid_execs_t read_workflow(char *file_path);
simgrid_execs_t get_ready_execs(const simgrid_execs_t &execs);

exec_hwloc_core_id_completion_time_t get_min_exec(const common_data_t *common_data, simgrid_execs_t &execs);
hwloc_core_id_completion_time_t get_best_hwloc_core_id(const common_data_t *common_data, const simgrid_exec_t *exec);
hwloc_core_ids_t get_hwloc_core_ids_by_availability(const hwloc_core_ids_availability_t &hwloc_core_ids_availability, bool available);

int assign_exec(exec_data_t *exec_data, simgrid_host_t *dummy_host);
void *thread_function(void *arg);
void update_active_threads_counter(const common_data_t *common_data, int value);
void update_hwloc_core_availability(const common_data_t *common_data, int hwloc_core_id, bool availability);

comm_name_time_ranges_t find_matching_time_ranges(const comm_name_to_time_t *map, const std::string &name, MatchPart partToMatch);

void read_matrix_from_file(const std::string &filename, matrix_t &matrix);
void print_workflow(const simgrid_execs_t &execs);
void print_matrix(const matrix_t &matrix, const std::string &label);
void print_comm_name_to_numa_id(const comm_name_to_numa_id_t &mapping, std::ostream &out = std::cout);
void print_exec_name_to_locality(const exec_name_to_locality_t &mapping, std::ostream &out = std::cout);
void print_comm_name_to_read_time(const comm_name_to_time_t &mapping, std::ostream &out = std::cout);
void print_exec_name_to_compute_time(const exec_name_to_compute_time_t &mapping, std::ostream &out = std::cout);
void print_comm_name_to_write_time(const comm_name_to_time_t &mapping, std::ostream &out = std::cout);
void print_exec_name_to_time(const exec_name_to_time_t &mapping, std::ostream &out = std::cout);
void print_unavailable_cores(const hwloc_core_ids_availability_t &core_ids_availability);
