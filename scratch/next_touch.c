#include <hwloc.h>
#include <iostream>
#include <cstring>

void check_memory_binding(hwloc_topology_t topology, void *buffer, size_t size) {
    hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
    hwloc_membind_policy_t policy; // Variable to store the memory binding policy

    // Get memory binding for the buffer
    int ret = hwloc_get_area_membind(topology, buffer, size, nodeset, &policy, HWLOC_MEMBIND_BYNODESET);
    if (ret == 0) {
        printf("Memory resides on NUMA nodes: ");
        int node;
        hwloc_bitmap_foreach_begin(node, nodeset) {
            printf("%d ", node);
        }
        hwloc_bitmap_foreach_end();
        printf("\n");
        printf("Memory binding policy: %d\n", policy); // Optionally display the policy
    } else {
        fprintf(stderr, "Error retrieving memory binding.\n");
    }

    hwloc_bitmap_free(nodeset);
}

int main() {
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    size_t size = 1024 * 1024; // 1 MB buffer
    hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
    hwloc_bitmap_set(nodeset, 0); // Allocate on NUMA node 0

    // Allocate memory on NUMA node 0
    void *buffer = hwloc_alloc_membind(topology, size, nodeset, HWLOC_MEMBIND_BIND, 0);
    if (!buffer) {
        std::cerr << "Failed to allocate memory on NUMA node 0.\n";
        return 1;
    }

    // Check initial memory location
    std::cout << "Before access:\n";
    check_memory_binding(topology, buffer, size);

    // Pin thread to NUMA node 1
    hwloc_bitmap_t cpu_set = hwloc_bitmap_alloc();
    hwloc_bitmap_set(cpu_set, 1); // Bind to CPU on NUMA node 1
    hwloc_set_cpubind(topology, cpu_set, HWLOC_CPUBIND_THREAD);

    // Access memory to trigger migration
    memset(buffer, 0, size);

    // Check memory location after access
    std::cout << "After access:\n";
    check_memory_binding(topology, buffer, size);

    // Clean up
    hwloc_bitmap_free(nodeset);
    hwloc_bitmap_free(cpu_set);
    hwloc_free(topology, buffer, size);
    hwloc_topology_destroy(topology);

    return 0;
}
