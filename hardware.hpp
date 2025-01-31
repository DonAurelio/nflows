#include <hwloc.h>
#include <sys/resource.h> // For getrusage


struct locality_s
{
    int numa_id;
    int core_id;
    long voluntary_context_switches;
    long involuntary_context_switches;
    long core_migrations;
};
typedef struct locality_s locality_t;

unsigned long get_current_core_speed_from_hwloc_core_id(common_t *common, int hwloc_core_id);
int get_hwloc_core_id_from_os_pu_id(common_t* common, int os_pu_id);
std::vector<int> get_hwloc_numa_ids_from_ptr(hwloc_topology_t *topology, char *address, size_t size);  // Hwloc (logical) NUMA id from memory address.
