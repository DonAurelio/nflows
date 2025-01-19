#include <stdio.h>
#include <hwloc.h>

int main() {
    hwloc_topology_t topology;
    hwloc_bitmap_t nodeset;
    hwloc_membind_policy_t policy;

    // Initialize the topology
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    // Get the process memory binding as a nodeset
    nodeset = hwloc_bitmap_alloc();

    // Get the NUMA memory binding for the current thread
    int ret = hwloc_get_membind(topology, nodeset, &policy, HWLOC_MEMBIND_THREAD);
    if (ret == 0) {
        // Print the memory policy
        printf("Memory policy: ");
        switch (policy) {
            case HWLOC_MEMBIND_DEFAULT:
                printf("DEFAULT (system default allocation policy)\n");
                break;
            case HWLOC_MEMBIND_FIRSTTOUCH:
                printf("FIRSTTOUCH (memory allocated on first-touch NUMA node)\n");
                break;
            case HWLOC_MEMBIND_BIND:
                printf("BIND (memory allocated on specific NUMA nodes only)\n");
                break;
            case HWLOC_MEMBIND_INTERLEAVE:
                printf("INTERLEAVE (memory interleaved across specific NUMA nodes)\n");
                break;
            case HWLOC_MEMBIND_NEXTTOUCH:
                printf("NEXTTOUCH (memory moved to the NUMA node of the next access)\n");
                break;
            case HWLOC_MEMBIND_MIXED:
                printf("MIXED (multiple memory policies apply)\n");
                break;
            default:
                printf("UNKNOWN\n");
                break;
        }

        // Print the NUMA nodes the thread is bound to
        printf("Thread is bound to NUMA nodes: ");
        int found = 0; // Track if any NUMA node is found
        int node; 
        hwloc_bitmap_foreach_begin(node, nodeset) {
            hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(
                topology, nodeset, HWLOC_OBJ_NUMANODE, node
            );
            if (obj) {
                printf("%d ", obj->logical_index); // Print NUMA node ID
                found = 1;
            }
        }
        hwloc_bitmap_foreach_end();

        if (!found) {
            printf("None (not bound to any specific NUMA node)");
        }
        printf("\n");
    } else {
        printf("Error retrieving memory policy.\n");
    }

    // Clean up
    hwloc_bitmap_free(nodeset);
    hwloc_topology_destroy(topology);

    return 0;
}
