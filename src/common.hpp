#pragma once

#include <hwloc.h>

#include <iomanip>
#include <map>
#include <simgrid/s4u.hpp>
#include <string>
#include <tuple>
#include <vector>

#include <ctime> // For timestamp
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <sys/time.h>

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
typedef simgrid::s4u::NetZone simgrid_netzone_t;
typedef simgrid::s4u::Activity simgrid_activity_t;
typedef simgrid::s4u::ActivityPtr simgrid_activity_ptr_t;
typedef std::vector<simgrid_exec_t *> simgrid_execs_t;

typedef std::vector<std::vector<double>> distance_matrix_t;

struct common_s
{
    // Harware locality config.
    hwloc_topology_t topology;

    // Pthreads config.
    int active_threads;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    // To be initialized by the user.
    unsigned long flops_per_cycle;
    unsigned long clock_frequency_hz;

    // Units are aligned with the reporting units used by Intel Memory Checker.
    // Latency (ns), Bandwidth (GB/s).
    distance_matrix_t distance_lat_ns;
    distance_matrix_t distance_bw_gbps;

    std::vector<bool> core_avail;

    // Locality information collections.
    name_to_address_t comm_name_to_address;
    name_to_numa_ids_t comm_name_to_numa_ids_r;
    name_to_numa_ids_t comm_name_to_numa_ids_w;
    name_to_thread_locality_t exec_name_to_thread_locality;

    // Timing (absolute timestamp) information collections.
    name_to_time_range_payload_t comm_name_to_r_ts_range_payload;
    name_to_time_range_payload_t comm_name_to_w_ts_range_payload;
    name_to_time_range_payload_t exec_name_to_c_ts_range_payload;

    // Timing (relative durations/offset) information collections.
    name_to_time_range_payload_t comm_name_to_r_time_offset_payload;
    name_to_time_range_payload_t comm_name_to_w_time_offset_payload;
    name_to_time_range_payload_t exec_name_to_c_time_offset_payload;
    name_to_time_range_payload_t exec_name_to_rcw_time_offset_payload;
};
typedef struct common_s common_t;

simgrid_execs_t common_read_dag_from_dot(const std::string &filename);
simgrid_execs_t common_get_ready_tasks(const simgrid_execs_t &dag);
std::vector<std::vector<double>> common_read_distance_matrix_from_file(const std::string &filename);

enum CommonVectorType
{
    COMM_READ,
    COMM_WRITE,
    COMPUTE
};

enum CommonCommNameMatch
{
    SRC,
    DST
};

std::vector<int> common_get_avail_core_ids(const common_t *common);
name_to_time_range_payload_t common_filter_name_ts_range_payload(const common_t *common, const std::string &name,
                                                                 CommonVectorType type, CommonCommNameMatch comm_name);

uint64_t common_get_time_us();
std::string common_get_timestamped_filename(const std::string &base_name);
std::string common_join(const std::vector<int> &vec, const std::string &delimiter);
std::pair<std::string, std::string> common_split(const std::string &input, std::string delimiter);

typedef void *(*thread_function_t)(void *);

struct thread_data_s
{
    common_t *common;
    simgrid_exec_t *exec;
    thread_function_t thread_function;
    int assigned_core_id;
};
typedef struct thread_data_s thread_data_t;

void common_set_core_id_as_avail(common_t *common, unsigned int core_id);
void common_set_core_id_as_unavail(common_t *common, unsigned int core_id);
void common_increment_active_threads_counter(common_t *common);
void common_decrement_active_threads_counter(common_t *common);
void common_wait_active_threads(common_t *common);

void common_print_distance_matrix(const distance_matrix_t &matrix, const std::string &key, std::ostream &out);
void common_print_name_to_numa_ids(const name_to_numa_ids_t &mapping, std::string header, std::ostream &out);
void common_print_name_to_thread_locality(const name_to_thread_locality_t &mapping, std::ostream &out);
void common_print_name_to_time_range_payload(const name_to_time_range_payload_t &mapping, const std::string &header,
                                             std::ostream &out);
void common_print_name_to_address(const name_to_address_t &mapping, std::ostream &out);
void common_print_metadata(const common_t *common, std::ostream &out);
void common_print_common_structure(const common_t *common, std::ostream &out);