#include <sys/resource.h> // For getrusage

#include "common.hpp"

int get_hwloc_core_id_by_pu_id(const common_t *common, int os_pu_id);
int get_hwloc_numa_id_by_core_id(const common_t *common, int hwloc_core_id);
unsigned long get_hwloc_core_performance_by_id(const common_t *common, int hwloc_core_id);
std::vector<int> get_hwloc_numa_ids_by_address(const common_t *common, char *address, size_t size);

std::string get_hwloc_thread_mem_policy(const common_t *common);
thread_locality_t get_hwloc_thread_locality(const common_t *common);

void bind_exec_to_thread(thread_data_t *data);
