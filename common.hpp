#pragma once

#include <hwloc.h>
#include <sys/time.h>

#include <nlohmann/json.hpp>
#include <simgrid/s4u.hpp>

#include <iomanip>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include <ranges>
#include <ctime>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <bitset>

enum CommonClockFrequencyType
{
    COMMON_DYNAMIC_CLOCK_FREQUENCY,
    COMMON_STATIC_CLOCK_FREQUENCY,
    COMMON_ARRAY_CLOCK_FREQUENCY,
};
typedef CommonClockFrequencyType clock_frequency_type_t;

enum CommonSchedulerType
{
    COMMON_SCHED_TYPE_MIN_MIN,
    COMMON_SCHED_TYPE_HEFT,
    COMMON_SCHED_TYPE_FIFO,
};
typedef CommonSchedulerType scheduler_type_t;

enum CommonMapperType
{
    COMMON_MAPPER_BARE_METAL,
    COMMON_MAPPER_SIMULATION,
};
typedef CommonMapperType mapper_type_t;

enum CommonTimeRangePayloadType
{
    COMMON_COMM_READ_TIMESTAMPS,
    COMMON_COMM_WRITE_TIMESTAMPS,
    COMMON_COMPUTE_TIMESTAMPS,

    COMMON_COMM_READ_OFFSETS,
    COMMON_COMM_WRITE_OFFSETS,
    COMMON_COMPUTE_OFFSETS,
};
typedef CommonTimeRangePayloadType time_range_payload_type_t;

enum CommonCommNameMatch
{
    COMMON_COMM_SRC,
    COMMON_COMM_DST,
};
typedef CommonCommNameMatch comm_name_match_t;

struct thread_locality_s
{
    int numa_id;
    int core_id;
    long voluntary_context_switches;
    long involuntary_context_switches;
    long core_migrations;
};
typedef struct thread_locality_s thread_locality_t;

typedef std::unordered_map<std::string, char *> name_to_address_t;
typedef std::unordered_map<std::string, unsigned int> name_to_count_t;
typedef std::unordered_map<std::string, std::vector<int>> name_to_numa_ids_t;
typedef std::unordered_map<std::string, thread_locality_t> name_to_thread_locality_t;

typedef std::tuple<double, double, double> time_range_payload_t;
typedef std::unordered_map<std::string, time_range_payload_t> name_to_time_range_payload_t;

typedef simgrid::s4u::Exec simgrid_exec_t;
typedef std::vector<simgrid_exec_t *> simgrid_execs_t;

typedef simgrid::s4u::Comm simgrid_comm_t;
typedef simgrid::s4u::Host simgrid_host_t;
typedef simgrid::s4u::NetZone simgrid_netzone_t;
typedef simgrid::s4u::Activity simgrid_activity_t;
typedef simgrid::s4u::ActivityPtr simgrid_activity_ptr_t;

typedef std::vector<std::vector<double>> distance_matrix_t;
typedef std::unordered_map<std::string, std::string> scheduler_params_t;

typedef void *(*mapper_thread_function_t)(void *);

struct common_s
{
    // User-defined.
    double flops_per_cycle;
    double clock_frequency_hz;

    std::vector<double> clock_frequencies_hz;
    clock_frequency_type_t clock_frequency_type;

    std::string out_file_name;

    // Units are aligned with the reporting units used by Intel Memory Checker.
    // Latency (ns), Bandwidth (GB/s).
    distance_matrix_t distance_lat_ns;
    distance_matrix_t distance_bw_gbps;

    std::vector<bool> core_avail;
    std::vector<double> core_avail_until;

    scheduler_type_t scheduler_type;
    scheduler_params_t scheduler_params;

    mapper_type_t mapper_type;
    hwloc_membind_policy_t mapper_mem_policy_type;
    std::vector<unsigned> mapper_mem_bind_numa_node_ids;

    // Runtime system status.
    hwloc_topology_t topology;

    size_t threads_checksum;
    unsigned int threads_active;
    pthread_mutex_t threads_mutex;
    pthread_cond_t threads_cond;

    // Counters
    name_to_count_t execs_active;
    name_to_count_t reads_active;
    name_to_count_t writes_active;
    std::mutex counters_mutex;  // Protects execs_active, reads_active, writes_active

    // Communication mappings
    name_to_address_t comm_name_to_address;
    name_to_numa_ids_t comm_name_to_numa_ids_r;
    name_to_numa_ids_t comm_name_to_numa_ids_w;
    std::mutex comm_maps_mutex;  // Protects the 3 maps above

    // Execution mappings
    name_to_thread_locality_t exec_name_to_thread_locality;
    std::mutex exec_locality_mutex;

    // Timestamp mappings
    name_to_time_range_payload_t comm_name_to_r_ts_range_payload;
    name_to_time_range_payload_t comm_name_to_w_ts_range_payload;
    name_to_time_range_payload_t exec_name_to_c_ts_range_payload;
    std::mutex timestamp_mutex;  // Protects all timestamp-related maps

    // Offset mappings
    name_to_time_range_payload_t comm_name_to_r_time_offset_payload;
    name_to_time_range_payload_t comm_name_to_w_time_offset_payload;
    name_to_time_range_payload_t exec_name_to_c_time_offset_payload;
    name_to_time_range_payload_t exec_name_to_rcw_time_offset_payload;
    std::mutex offset_mutex;  // Protects all offset-related maps
};
typedef struct common_s common_t;

struct thread_data_s
{
    int assigned_core_id;
    common_t *common;
    simgrid_exec_t *exec;
    mapper_thread_function_t thread_function;
};
typedef struct thread_data_s thread_data_t;


/* UTILS */
double common_get_time_us();
std::string common_join(const std::vector<int> &vec, const std::string &delimiter);
std::pair<std::string, std::string> common_split(const std::string &input, std::string delimiter);

/* USER */
simgrid_execs_t common_dag_read_from_dot(const std::string &dot_file);
simgrid_execs_t common_dag_get_ready_execs(const simgrid_execs_t &dag);

clock_frequency_type_t common_clock_frequency_str_to_type(std::string &type);
std::string common_clock_frequency_type_to_str(clock_frequency_type_t &type);

scheduler_type_t common_scheduler_str_to_type(std::string &type);
std::string common_scheduler_type_to_str(scheduler_type_t &type);

std::string common_scheduler_param_get(const common_t *common, const std::string &key);

mapper_type_t common_mapper_str_to_type(std::string &type);
std::string common_mapper_type_to_str(mapper_type_t &type);

hwloc_membind_policy_t common_mapper_mem_policy_str_to_type(std::string &type);
std::string common_mapper_mem_policy_type_to_str(hwloc_membind_policy_t &type);

distance_matrix_t common_distance_matrix_read_from_txt(const std::string &txt_file);

std::vector<int> common_core_id_get_avail(const common_t *common);
void common_core_id_set_avail(common_t *common, unsigned int core_id, bool avail);

double common_core_id_get_avail_until(const common_t *common, unsigned int core_id);
void common_core_id_set_avail_until(common_t *common, unsigned int core_id, double duration);

name_to_time_range_payload_t common_comm_name_to_w_time_offset_payload_filter(const common_t *common, const std::string &dst_name);

/* USER UTILS */
double common_earliest_start_time(const common_t *common, const std::string &exec_name, unsigned int core_id);
double common_communication_time(const common_t *common, unsigned int src_numa_id, unsigned int dst_numa_id, double payload);
double common_compute_time(const common_t *common, double flops, double processor_speed_flops_per_second);

/* RUNTIME */
void common_threads_checksum_update(common_t *common, size_t checksum);
void common_threads_active_increment(common_t *common);
void common_threads_active_decrement(common_t *common);
void common_threads_active_wait(common_t *common);

void common_execs_active_increment(common_t *common, const std::string &name);
void common_reads_active_increment(common_t *common, const std::string &name);
void common_writes_active_increment(common_t *common, const std::string &name);

void common_comm_name_to_address_create(common_t *common, const std::string& comm_name, char* write_buffer);
void common_comm_name_to_numa_ids_r_create(common_t *common, const std::string& comm_name, const std::vector<int>& memory_bindings);
void common_comm_name_to_numa_ids_w_create(common_t *common, const std::string& comm_name, const std::vector<int>& memory_bindings);
void common_exec_name_to_thread_locality_create(common_t *common, const std::string& exec_name,const thread_locality_t& locality);

void common_comm_name_to_r_ts_range_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload);
void common_comm_name_to_w_ts_range_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload);
void common_exec_name_to_c_ts_range_payload_create(common_t *common, const std::string& exec_name, const time_range_payload_t& time_range_payload);

void common_comm_name_to_r_time_offset_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload);
void common_comm_name_to_w_time_offset_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload);
void common_exec_name_to_c_time_offset_payload_create(common_t *common, const std::string& exec_name, const time_range_payload_t& time_range_payload);
void common_exec_name_to_rcw_time_offset_payload_create(common_t *common, const std::string& exec_name, const time_range_payload_t& time_range_payload);

/* OUTPUT */
void common_print_common_structure(const common_t *common, int indent);
void common_print_user(const common_t *common, std::ostream &out, int indent);
void common_print_distance_matrix(const distance_matrix_t &matrix, const std::string &key, std::ostream &out, int indent);
void common_print_workflow(const common_t *common, std::ostream &out, int indent);
void common_print_runtime(const common_t *common, std::ostream &out, int indent);
void common_print_trace(const common_t *common, std::ostream &out, int indent);
void common_print_name_to_thread_locality(const name_to_thread_locality_t &mapping, std::ostream &out, int indent);
void common_print_name_to_numa_ids(const name_to_numa_ids_t &mapping, const std::string header, std::ostream &out, int indent);
void common_print_name_to_time_range_payload(const name_to_time_range_payload_t &mapping, const std::string &header, std::ostream &out, int indent);

/* OUTPUT UTILS */
size_t common_name_to_count_get(const name_to_count_t &mapping);
