#include "utils.h"

uint64_t get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

unsigned long get_current_core_speed_from_hwloc_core_id(common_t *common, int hwloc_core_id)
{
    hwloc_topology_t topology = *(common->topology);

    // Retrieve the core object.
    hwloc_obj_t core_obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, hwloc_core_id);

    // Iterate over the PUs (logical processing units) inside the core to get the OS index
    int os_core_id = -1;
    for (unsigned i = 0; i < core_obj->arity; ++i)
    {
        hwloc_obj_t pu_obj = core_obj->children[i];
        if (pu_obj->type == HWLOC_OBJ_PU)
        {
            os_core_id = pu_obj->os_index;
            break;
        }
    }

    // Compute core_speed.
    if (common->clock_frequency_hz == 0)
    {
        // Path to the current frequency file for the given os core id.
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(os_core_id) + "/cpufreq/scaling_cur_freq";
        std::ifstream freq_file(path);

        if (!freq_file.is_open())
        {
            std::cerr << "Error: Unable to open file for core " << os_core_id << std::endl;
            return 0;
        }

        unsigned long frequency_khz = 0;
        freq_file >> frequency_khz;
        freq_file.close();

        // Convert frequency to Hertz.
        unsigned long frequency_hz = frequency_khz * 1000;

        return common->flops_per_cycle * frequency_hz;
    }
   
    return common->flops_per_cycle * data->clock_frequency_hz;
}

int get_hwloc_core_id_from_os_pu_id(common_t *common, int os_pu_id)
{
    hwloc_topology_t topology = *(common->topology);

    // Get the PU object by its OS index (sched_getcpu() index)
    hwloc_obj_t pu_obj = hwloc_get_pu_obj_by_os_index(topology, os_pu_id);
    if (pu_obj == nullptr)
    {
        std::cerr << "Error: PU object not found for OS CPU index " << os_pu_id << std::endl;
        return -1;
    }

    // Get the core that contains this PU
    hwloc_obj_t core_obj = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_CORE, pu_obj);
    if (core_obj == nullptr)
    {
        std::cerr << "Error: Core object not found for PU " << os_pu_id << std::endl;
        return -1;
    }

    // Output the core ID
    int hwloc_core_id = core_obj->logical_index;

    return hwloc_core_id;
}

std::vector<int> get_hwloc_numa_ids_from_ptr(hwloc_topology_t *topology, char *address, size_t size)
{
    /* Get data locality (NUMA nodes) were data pages are allocated */
    hwloc_nodeset_t nodeset = hwloc_bitmap_alloc();
    std::vector<int> numa_nodes;

    // Get memory binding for the buffer
    // Get the NUMA nodes where memory identified by (addr, len ) is physically
    // allocated. The bitmap set (previously allocated by the caller) is filled
    // according to the NUMA nodes where the memory area pages are physically
    // allocated. If no page is actually allocated yet, set may be empty.

    // If pages spread to multiple nodes, it is not specified whether they spread
    // equitably, or whether most of them are on a single node, etc.
    int ret = hwloc_get_area_memlocation(*topology, address, size, nodeset, HWLOC_MEMBIND_BYNODESET);
    if (ret == 0)
    {
        int node;
        hwloc_bitmap_foreach_begin(node, nodeset) {
            numa_nodes.push_back(node); // Add NUMA node ID to the vector
        }
        hwloc_bitmap_foreach_end();
    }
    else
    {
        std::cerr << "Error retrieving memory binding.\n";
    }

    // Cleanup
    hwloc_bitmap_free(nodeset);

    return numa_nodes;
}

int get_hwloc_numa_id_from_hwloc_core_id(hwloc_topology_t *topology, int hwloc_core_id)
{
    hwloc_obj_t core_obj;
    hwloc_cpuset_t cpuset;

    int hwloc_numa_id = -1;

    // Traverse through the topology to find the core object with the given core_id
    core_obj = hwloc_get_obj_by_type(*topology, HWLOC_OBJ_CORE, hwloc_core_id);
    if (core_obj == NULL)
    {
        fprintf(stderr, "Error: Core ID %d not found.\n", hwloc_core_id);
        return -1;
    }

    // Get the CPU set (cpuset) of the core
    cpuset = hwloc_bitmap_dup(core_obj->cpuset);

    // Use hwloc_get_next_obj_covering_cpuset_by_type to find the NUMA node
    hwloc_obj_t numa_node = NULL;
    while ((numa_node = hwloc_get_next_obj_covering_cpuset_by_type(*topology, cpuset, HWLOC_OBJ_NUMANODE, numa_node)) != NULL)
    {
        if (hwloc_bitmap_isincluded(cpuset, numa_node->cpuset))
            hwloc_numa_id = numa_node->logical_index;
    }

    // Cleanup
    hwloc_bitmap_free(cpuset);

    return hwloc_numa_id;
}

std::string get_hwloc_thread_mem_policy(hwloc_topology_t *topology)
{
    hwloc_bitmap_t nodeset;
    hwloc_membind_policy_t policy;
    std::ostringstream output; // String stream to build the output

    // Allocate nodeset
    nodeset = hwloc_bitmap_alloc();

    // Get the NUMA memory binding for the current thread
    int ret = hwloc_get_membind(*topology, nodeset, &policy, HWLOC_MEMBIND_THREAD);
    if (ret == 0) {
        // Add memory policy to the output string
        output << "memory policy ";
        switch (policy) {
            case HWLOC_MEMBIND_DEFAULT:
                output << "DEFAULT (system default allocation policy) - ";
                break;
            case HWLOC_MEMBIND_FIRSTTOUCH:
                output << "FIRSTTOUCH (memory allocated on first-touch NUMA node) - ";
                break;
            case HWLOC_MEMBIND_BIND:
                output << "BIND (memory allocated on specific NUMA nodes only) - ";
                break;
            case HWLOC_MEMBIND_INTERLEAVE:
                output << "INTERLEAVE (memory interleaved across specific NUMA nodes) - ";
                break;
            case HWLOC_MEMBIND_NEXTTOUCH:
                output << "NEXTTOUCH (memory moved to the NUMA node of the next access) - ";
                break;
            case HWLOC_MEMBIND_MIXED:
                output << "MIXED (multiple memory policies apply) - ";
                break;
            default:
                output << "UNKNOWN - ";
                break;
        }

        // Add NUMA nodes the thread is bound to
        output << "thread is bound to NUMA nodes ";
        int found = 0; // Track if any NUMA node is found
        int node;
        hwloc_bitmap_foreach_begin(node, nodeset) {
            hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(
                *topology, nodeset, HWLOC_OBJ_NUMANODE, node
            );
            if (obj) {
                output << obj->logical_index << " "; // Add NUMA node ID
                found = 1;
            }
        }
        hwloc_bitmap_foreach_end();

        if (!found) {
            output << "None (not bound to any specific NUMA node)";
        }

    } else {
        output << "Error retrieving memory policy.\n";
    }

    // Clean up
    hwloc_bitmap_free(nodeset);

    std::string result = output.str();
    if (!result.empty()) result.pop_back(); // Remove the last space

    return result; // Return the constructed string
}

// Function to concatenate vector elements into a single string
std::string join_vector_elements(const std::vector<int>& vec, const std::string& delimiter)
{
    std::ostringstream oss;

    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i != vec.size() - 1) { // Avoid adding a delimiter after the last element
            oss << delimiter;
        }
    }

    return oss.str();
}

locality_t get_hwloc_locality_info(hwloc_topology_t *topology)
{
    hwloc_bitmap_t cpuset;
    hwloc_obj_t obj;
    int numa_node, core_id;
    long core_migrations = -1; // Core migrations initialized to -1 to handle errors

    // Get the current thread's CPU binding
    cpuset = hwloc_bitmap_alloc();
    hwloc_get_last_cpu_location(*topology, cpuset, HWLOC_CPUBIND_THREAD);

    // Get the NUMA node on which the current thread is running
    hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
    hwloc_cpuset_to_nodeset(*topology, cpuset, nodeset);
    numa_node = hwloc_bitmap_first(nodeset); // Get the first NUMA node

    // Get the core object corresponding to the CPU binding
    obj = hwloc_get_obj_covering_cpuset(*topology, cpuset);

    // Traverse up the object hierarchy to find the core object
    while (obj && obj->type != HWLOC_OBJ_CORE)
    {
        obj = obj->parent;
    }

    if (obj)
    {
        core_id = obj->logical_index; // Get the logical core ID
    }
    else
    {
        core_id = -1; // Handle error case where core is not found
    }

    // Get context switch information using getrusage
    struct rusage usage;
    getrusage(RUSAGE_THREAD, &usage);

    long voluntary_context_switches = usage.ru_nvcsw;    // Voluntary context switches
    long involuntary_context_switches = usage.ru_nivcsw; // Involuntary context switches

    // Retrieve core migration information from /proc/self/sched
    std::ifstream sched_file("/proc/self/sched");
    std::string line;
    while (std::getline(sched_file, line))
    {
        if (line.find("nr_migrations") != std::string::npos)
        {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> core_migrations; // Extract migration count
            break;
        }
    }
    sched_file.close();

    // Cleanup
    hwloc_bitmap_free(cpuset);
    hwloc_bitmap_free(nodeset);

    // Return locality and context switch information along with core migrations
    return {numa_node, core_id, voluntary_context_switches, involuntary_context_switches, core_migrations};
}

std::pair<std::string, std::string> split_by_arrow(const std::string &input)
{
    std::string delimiter = "->";
    size_t pos = input.find(delimiter);
    if (pos == std::string::npos)
    {
        // If "->" is not found, return empty strings or handle as needed
        return {"", ""};
    }

    std::string first = input.substr(0, pos);
    std::string second = input.substr(pos + delimiter.length());
    return {first, second};
}

// Helper function to generate a timestamped filename
std::string generate_timestamped_filename(const std::string &base_name)
{
    std::time_t now = std::time(nullptr);
    char buf[100];
    if (std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&now)))
    {
        return base_name + "_" + buf + ".yml";
    }
    return base_name + ".yml";
}
