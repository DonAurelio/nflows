#include <hwloc.h>
#include <sys/resource.h> // For getrusage
#include <sys/time.h>

#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime> // For timestamp

struct locality_s
{
    int numa_id;
    int core_id;
    long voluntary_context_switches;
    long involuntary_context_switches;
    long core_migrations;
};
typedef struct locality_s locality_t;

uint64_t get_time_us();

unsigned long get_current_core_speed_from_hwloc_core_id(hwloc_topology_t *topology, int hwloc_core_id); // Speed in flops (float point operations per second).


int get_hwloc_core_id_from_os_core_id(hwloc_topology_t *topology, int os_core_id);
int get_hwloc_core_id_from_os_pu_id(hwloc_topology_t *topology, int os_pu_id);

unsigned long get_cpu_frequency_from_os_core_id(int os_core_id);                                        // Physical core frequency in Hz.

// TODO: data pages can be allocated in different numa nodes, so this function must be updated.
std::vector<int> get_hwloc_numa_ids_from_ptr(hwloc_topology_t *topology, char *address, size_t size);  // Hwloc (logical) NUMA id from memory address.
int get_hwloc_numa_id_from_hwloc_core_id(hwloc_topology_t *topology, int hwloc_core_id); // Hwloc (logical) NUMA id associated with the given core.
std::string get_hwloc_thread_mem_policy(hwloc_topology_t *topology);

std::string join_vector_elements(const std::vector<int>& vec, const std::string& delimiter = " ");

locality_t get_hwloc_locality_info(hwloc_topology_t *topology);

std::pair<std::string, std::string> split_by_arrow(const std::string &input);

std::string generate_timestamped_filename(const std::string &base_name);
