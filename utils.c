#include "utils.h"

uint64_t get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

std::vector<int> get_all_hwloc_core_ids(hwloc_topology_t *topology)
{
    hwloc_obj_t obj;
    std::vector<int> core_ids; // Vector to store core IDs

    // Get the depth for core objects
    int depth = hwloc_get_type_depth(*topology, HWLOC_OBJ_CORE);

    // Get the number of cores
    int n_cores = hwloc_get_nbobjs_by_depth(*topology, depth);

    // Loop through each core and store the logical index (core ID) in the vector
    for (int i = 0; i < n_cores; i++)
    {
        obj = hwloc_get_obj_by_depth(*topology, depth, i);
        core_ids.push_back(obj->logical_index); // Add core ID to vector
    }

    return core_ids; // Return the vector of core IDs
}

int get_os_core_id_from_hwloc_core_id(hwloc_topology_t *topology, int hwloc_core_id)
{
    // Retrieve the core object from hwloc using HWLOC_OBJ_CORE
    hwloc_obj_t core_obj = hwloc_get_obj_by_type(*topology, HWLOC_OBJ_CORE, hwloc_core_id);
    if (core_obj == nullptr)
    {
        std::cerr << "Error: Core ID not found in hwloc topology!" << std::endl;
        hwloc_topology_destroy(*topology);
        return -1;
    }

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

    if (os_core_id == -1)
    {
        std::cerr << "Error: No PUs found for core " << hwloc_core_id << std::endl;
        return -1;
    }

    return os_core_id;
}

int get_hwloc_core_id_from_os_core_id(hwloc_topology_t *topology, int os_core_id)
{
    // Get the total number of cores in the topology
    int num_cores = hwloc_get_nbobjs_by_type(*topology, HWLOC_OBJ_CORE);

    // Iterate over the cores to find the one with the specified OS core ID
    for (int hwloc_core_id = 0; hwloc_core_id < num_cores; ++hwloc_core_id)
    {
        // Retrieve the core object
        hwloc_obj_t core_obj = hwloc_get_obj_by_type(*topology, HWLOC_OBJ_CORE, hwloc_core_id);
        if (core_obj == nullptr)
        {
            std::cerr << "Error: Core object not found!" << std::endl;
            continue;
        }

        // Check if the core's OS index matches the os_core_id
        if (core_obj->os_index == os_core_id)
            return hwloc_core_id;
    }

    // If no matching core is found
    std::cerr << "Error: No core found with OS core ID " << os_core_id << std::endl;

    return -1;
}

int get_hwloc_core_id_from_os_pu_id(hwloc_topology_t *topology, int os_pu_id)
{
    // Step 1: Get the PU object by its OS index (sched_getcpu() index)
    hwloc_obj_t pu_obj = hwloc_get_pu_obj_by_os_index(*topology, os_pu_id);
    if (pu_obj == nullptr)
    {
        std::cerr << "Error: PU object not found for OS CPU index " << os_pu_id << std::endl;
        return -1;
    }

    // Step 2: Get the core that contains this PU
    hwloc_obj_t core_obj = hwloc_get_ancestor_obj_by_type(*topology, HWLOC_OBJ_CORE, pu_obj);
    if (core_obj == nullptr)
    {
        std::cerr << "Error: Core object not found for PU " << os_pu_id << std::endl;
        return -1;
    }

    // Output the core ID
    int hwloc_core_id = core_obj->logical_index;

    return hwloc_core_id;
}

unsigned long get_cpu_frequency_from_os_core_id(int os_core_id)
{
    // Path to the current frequency file for the given core
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

    unsigned long frequency_hz = frequency_khz * 1000;

    return frequency_hz;
}

unsigned long get_current_core_speed_from_hwloc_core_id(hwloc_topology_t *topology, int hwloc_core_id)
{
    // Reference: https://www.intel.la/content/www/xl/es/architecture-and-technology/avx-512-overview.html
    // Applications can pack 32 double precision and 64 single precision floating point operations per clock
    // cycle within the 512-bit vectors.

    // Get the current frequency for the OS core ID
    int flops_per_cycle = 32;
    int os_core_id = get_os_core_id_from_hwloc_core_id(topology, hwloc_core_id);

    unsigned long frequency_hz = get_cpu_frequency_from_os_core_id(os_core_id);
    unsigned long current_core_speed = flops_per_cycle * frequency_hz;

    return current_core_speed;
}

int get_hwloc_numa_id_from_ptr(hwloc_topology_t *topology, char *address, size_t size)
{
    /* Get data locality (NUMA node) were the data was allocated */
    hwloc_nodeset_t nodeset;
    int numa_node;

    // Get the NUMA node(s) on which the memory is located.
    nodeset = hwloc_bitmap_alloc();

    if (hwloc_get_area_memlocation(*topology, address, size, nodeset, 0) != 0)
    {
        fprintf(stderr, "Failed to get memory location\n");
        hwloc_bitmap_free(nodeset);
        hwloc_topology_destroy(*topology);
        numa_node = -2;
    }
    else
    {
        // If -1, memory is not bound to any specify NUMA node.
        numa_node = hwloc_bitmap_first(nodeset);
    }

    // Cleanup
    hwloc_bitmap_free(nodeset);

    return numa_node;
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
