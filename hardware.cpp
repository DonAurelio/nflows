#include "hardware.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(hardware, "Messages specific to this module.");

int hardware_hwloc_core_id_get_by_pu_id(const common_t *common, int os_pu_id)
{
    // Get the PU object by its OS index (sched_getcpu() index)
    hwloc_obj_t pu_obj = hwloc_get_pu_obj_by_os_index(common->topology, os_pu_id);
    if (pu_obj == nullptr)
    {
        XBT_ERROR("hwloc_pu_object not found for os_pu_id: %d", os_pu_id);
        throw std::runtime_error("hwloc_pu_object not found for os_pu_id: " + std::to_string(os_pu_id));
    }

    // Get the core that contains this PU
    hwloc_obj_t core_obj = hwloc_get_ancestor_obj_by_type(common->topology, HWLOC_OBJ_CORE, pu_obj);
    if (core_obj == nullptr)
    {
        XBT_ERROR("hwloc_core_object not found for os_pu_id: %d", os_pu_id);
        throw std::runtime_error("hwloc_core_object not found for os_pu_id: " + std::to_string(os_pu_id));
    }

    int hwloc_core_id = core_obj->logical_index;

    return hwloc_core_id;
}

double hardware_hwloc_core_id_get_clock_frequency(const common_t *common, int hwloc_core_id)
{
    switch (common->clock_frequency_type) {
        case COMMON_DYNAMIC_CLOCK_FREQUENCY: 
            return hardware_hwloc_core_id_get_dynamic_clock_frequency(common, hwloc_core_id);
        case COMMON_ARRAY_CLOCK_FREQUENCY:
            return common->clock_frequencies_hz[hwloc_core_id];
        case COMMON_STATIC_CLOCK_FREQUENCY:
            return common->clock_frequency_hz;
        default:
            XBT_ERROR("Invalid clock frequency type: %s.", 
                common_clock_frequency_type_to_str(common->clock_frequency_type).c_str());
            throw std::runtime_error("Invalid clock frequency type enum value");
    }
}

double hardware_hwloc_core_id_get_dynamic_clock_frequency(const common_t *common, int hwloc_core_id)
{
    hwloc_obj_t core_obj = hwloc_get_obj_by_type(common->topology, HWLOC_OBJ_CORE, hwloc_core_id);

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

    // Path to the current frequency file for the given os core id.
    std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(os_core_id) + "/cpufreq/scaling_cur_freq";
    std::ifstream freq_file(path);

    if (!freq_file.is_open())
    {
        XBT_ERROR("unable to open: %s", path.c_str());
        throw std::runtime_error("unable to open: " + path);
    }

    double frequency_khz = 0.0;
    freq_file >> frequency_khz;
    freq_file.close();

    // Convert frequency to Hertz.
    return frequency_khz * 1000;
}

int hardware_hwloc_numa_id_get_by_core_id(const common_t *common, int hwloc_core_id)
{
    hwloc_obj_t core_obj;
    hwloc_cpuset_t cpuset;

    // Traverse through the topology to find the core object with the given core_id
    core_obj = hwloc_get_obj_by_type(common->topology, HWLOC_OBJ_CORE, hwloc_core_id);

    if (core_obj == nullptr)
    {
        XBT_ERROR("hwloc_core_id not found: %d ", hwloc_core_id);
        throw std::runtime_error("hwloc_core_id not found: " + std::to_string(hwloc_core_id));
    }

    // Get the CPU set (cpuset) of the core
    cpuset = hwloc_bitmap_dup(core_obj->cpuset);

    // Use hwloc_get_next_obj_covering_cpuset_by_type to find the NUMA node
    hwloc_obj_t numa_node = NULL;
    int hwloc_numa_id = -1;
    while ((numa_node = hwloc_get_next_obj_covering_cpuset_by_type(common->topology, cpuset, HWLOC_OBJ_NUMANODE, numa_node)) != NULL)
        if (hwloc_bitmap_isincluded(cpuset, numa_node->cpuset))
            hwloc_numa_id = numa_node->logical_index;

    // Cleanup
    hwloc_bitmap_free(cpuset);

    if (hwloc_numa_id == -1)
    {
        XBT_ERROR("hwloc_numa_id not found for hwloc_core_id: %d", hwloc_core_id);
        throw std::runtime_error("hwloc_numa_id not found for hwloc_core_id: " + std::to_string(hwloc_core_id));
    }

    return hwloc_numa_id;
}

std::vector<int> hardware_hwloc_numa_id_get_by_address(const common_t *common, char *address, size_t size)
{
    /* Get data locality (NUMA nodes were data pages are allocated) */
    hwloc_nodeset_t nodeset = hwloc_bitmap_alloc();
    std::vector<int> numa_nodes;

    // Get memory binding for the buffer
    // Get the NUMA nodes where memory identified by (addr, len ) is physically
    // allocated. The bitmap set (previously allocated by the caller) is filled
    // according to the NUMA nodes where the memory area pages are physically
    // allocated. If no page is actually allocated yet, set may be empty.

    // If pages spread to multiple nodes, it is not specified whether they spread
    // equitably, or whether most of them are on a single node, etc.
    if (hwloc_get_area_memlocation(common->topology, address, size, nodeset, HWLOC_MEMBIND_BYNODESET) != 0)
    {
        XBT_ERROR("failed to retrieve memory binding for address: %p", address);
        throw std::runtime_error(std::string("failed to retrieve memory binding for address: ") + address);    
    }

    int node;
    hwloc_bitmap_foreach_begin(node, nodeset)
    {
        numa_nodes.push_back(node); // Add NUMA node ID to the vector
    }
    hwloc_bitmap_foreach_end();

    // Cleanup
    hwloc_bitmap_free(nodeset);

    return numa_nodes;
}

void hardware_hwloc_thread_mem_policy_set(const common_t *common)
{
    if (common->mapper_mem_policy_type == HWLOC_MEMBIND_DEFAULT) return;

    hwloc_bitmap_t current_nodeset = hwloc_bitmap_alloc();
    hwloc_membind_policy_t current_mem_policy;

    if (hwloc_get_membind(common->topology, current_nodeset, &current_mem_policy, HWLOC_MEMBIND_THREAD) != 0)
    {
        XBT_ERROR("failed to get memory binding:: %s (errno: %d)", strerror(errno), errno);
        hwloc_bitmap_free(current_nodeset);
        throw std::runtime_error("failed to get memory binding: " + std::string(strerror(errno)));
    }

    hwloc_bitmap_t new_nodeset = hwloc_bitmap_dup(current_nodeset);

    if (common->mapper_mem_policy_type == HWLOC_MEMBIND_BIND)
    {
        hwloc_bitmap_zero(new_nodeset);
        for (int numa_id : common->mapper_mem_bind_numa_node_ids)
            hwloc_bitmap_set(new_nodeset, numa_id);
    }

    char *nodeset_c_str;
    char *nodeset_n_str;
    
    hwloc_bitmap_asprintf(&nodeset_c_str, current_nodeset);
    hwloc_bitmap_asprintf(&nodeset_n_str, new_nodeset);
    
    XBT_DEBUG("nodeset (current): %s, nodeset (new): %s.", nodeset_c_str, nodeset_n_str);

    free(nodeset_c_str);
    free(nodeset_n_str);
    
    if (hwloc_set_membind(common->topology, new_nodeset, common->mapper_mem_policy_type, HWLOC_MEMBIND_THREAD) != 0)
    {
        XBT_ERROR("set memory binding failed: %s (errno: %d)", strerror(errno), errno);
        throw std::runtime_error("set memory binding failed: " + std::string(strerror(errno)));
    }

    hwloc_bitmap_free(new_nodeset);
    hwloc_bitmap_free(current_nodeset);
}

hwloc_membind_policy_t hardware_hwloc_thread_mem_policy_get_from_os(const common_t *common, std::vector<int> &out_numa_ids)
{
    hwloc_membind_policy_t policy;
    hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();

    // Get the NUMA memory binding for the current thread
    if (hwloc_get_membind(common->topology, nodeset, &policy, HWLOC_MEMBIND_THREAD) != 0)
    {
        hwloc_bitmap_free(nodeset);
        XBT_ERROR("get memory binding: %s (errno: %d)", strerror(errno), errno);
        throw std::runtime_error("get memory binding failed: " + std::string(strerror(errno)));
    }

    int node;
    hwloc_bitmap_foreach_begin(node, nodeset)
    {
        hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(common->topology, nodeset, HWLOC_OBJ_NUMANODE, node);
        if (obj) out_numa_ids.push_back(obj->logical_index);
    }
    hwloc_bitmap_foreach_end();
 
    hwloc_bitmap_free(nodeset);

    return policy;
}

thread_locality_t hardware_hwloc_thread_get_locality_from_os(const common_t *common)
{
    // Get the current thread's CPU binding
    hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
    hwloc_get_last_cpu_location(common->topology, cpuset, HWLOC_CPUBIND_THREAD);

    // Get the NUMA node on which the current thread is running
    hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
    hwloc_cpuset_to_nodeset(common->topology, cpuset, nodeset);

    int numa_id = hwloc_bitmap_first(nodeset); // Get the first NUMA node

    // Get the core object corresponding to the CPU binding
    hwloc_obj_t obj = hwloc_get_obj_covering_cpuset(common->topology, cpuset);

    // Cleanup
    hwloc_bitmap_free(cpuset);
    hwloc_bitmap_free(nodeset);

    // Traverse up the object hierarchy to find the core object
    while (obj && obj->type != HWLOC_OBJ_CORE) obj = obj->parent;

    if (!obj)
    {
        XBT_ERROR("failed to get core object.");
        throw std::runtime_error("failed to get core object.");
    }

    int core_id = obj->logical_index; // Get the logical core ID

    // Retrieve core migration information from /proc/self/sched
    std::ifstream sched_file("/proc/self/sched");

    if (!sched_file.is_open()) {
        XBT_ERROR("failed to open /proc/self/sched: %s", strerror(errno));
        throw std::runtime_error("failed to open process scheduling information.");
    }

    std::string line;
    int core_migrations;
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

    // Get context switch information using getrusage
    struct rusage usage;
    getrusage(RUSAGE_THREAD, &usage);

    // Return locality and context switch information along with core migrations
    return {numa_id, core_id, usage.ru_nvcsw, usage.ru_nivcsw, core_migrations};
}

void hardware_hwloc_thread_bind_to_core_id(thread_data_t *data)
{
    int pu;
    pthread_t thread;
    pthread_attr_t attr;

    cpu_set_t cpuset;

    hwloc_obj_t core;
    hwloc_cpuset_t hwloc_cpuset;

    // Initialize thread attributes.
    pthread_attr_init(&attr);

    // Retrieve the core object based on the core index.
    core = hwloc_get_obj_by_type(data->common->topology, HWLOC_OBJ_CORE, data->assigned_core_id);
    if (!core)
    {
        XBT_ERROR("assigned_core_id not found in topology: %d", data->assigned_core_id);
        throw std::runtime_error("assigned_core_id not found in topology: " + std::to_string(data->assigned_core_id));
    }

    // Get the cpuset of the core (includes all PUs associated with the core)
    hwloc_cpuset = hwloc_bitmap_dup(core->cpuset);

    // Remove hyperthreads (keep only the first PU in each core)
    hwloc_bitmap_singlify(hwloc_cpuset);

    // Clear the CPU set
    CPU_ZERO(&cpuset);

    // Convert hwloc cpuset to Linux cpu_set_t using glibc function
    // Manually iterate over the cpuset and set the corresponding bits in the Linux CPU set
    for (pu = hwloc_bitmap_first(hwloc_cpuset); pu != -1; pu = hwloc_bitmap_next(hwloc_cpuset, pu))
        CPU_SET(pu, &cpuset); // Set the corresponding processing unit in the CPU set

    // Free the hwloc cpuset object as it's no longer needed
    hwloc_bitmap_free(hwloc_cpuset);

    // Set thread affinity
    if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset) != 0)
    {
        XBT_ERROR("set thread affinity failed for assigned_core_id: %d.", data->assigned_core_id);
        throw std::runtime_error("set thread affinity failed for assigned_core_id: " + std::to_string(data->assigned_core_id));
    }

    // Mark the selected hwloc_core_id as unavailable (the thread will release it upon completion).
    common_core_id_set_avail(data->common, data->assigned_core_id, false);

    if (pthread_create(&thread, &attr, data->thread_function, data) != 0)
    {
        XBT_ERROR("unable to create thread for assigned_core_id: %d", data->assigned_core_id);
        throw std::runtime_error("unable to create thread for assigned_core_id: " + data->assigned_core_id);
    }

    common_threads_active_increment(data->common);

    pthread_attr_destroy(&attr);

    pthread_detach(thread);

    // Clean up
    // thread_data is freed in thread_function.
}
