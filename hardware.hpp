#include <sys/resource.h> // For getrusage

#include "common.hpp"

int hardware_hwloc_core_id_get_by_pu_id(const common_t *common, int os_pu_id);
double hardware_hwloc_core_id_get_clock_frequency(const common_t *common, int hwloc_core_id);
double hardware_hwloc_core_id_get_dynamic_clock_frequency(const common_t *common, int hwloc_core_id);

int hardware_hwloc_numa_id_get_by_core_id(const common_t *common, int hwloc_core_id);
std::vector<int> hardware_hwloc_numa_id_get_by_address(const common_t *common, char *address, size_t size);

void hardware_hwloc_thread_mem_policy_set(const common_t *common);
hwloc_membind_policy_t hardware_hwloc_thread_mem_policy_get_from_os(const common_t *common, std::vector<int> &out_numa_ids);

thread_locality_t hardware_hwloc_thread_get_locality_from_os(const common_t *common);

void hardware_hwloc_thread_bind_to_core_id(thread_data_t *data);
