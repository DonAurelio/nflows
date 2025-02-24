#include "hardware.hpp"

int get_hwloc_core_id_by_pu_id(const common_t *common, int os_pu_id)
{
    hwloc_topology_t topology = common->topology;

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

int get_hwloc_numa_id_by_core_id(const common_t *common, int hwloc_core_id)
{
    hwloc_topology_t topology = common->topology;
    hwloc_obj_t core_obj;
    hwloc_cpuset_t cpuset;

    int hwloc_numa_id = -1;

    // Traverse through the topology to find the core object with the given core_id
    core_obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, hwloc_core_id);
    if (core_obj == NULL)
    {
        fprintf(stderr, "Error: Core ID %d not found.\n", hwloc_core_id);
        return -1;
    }

    // Get the CPU set (cpuset) of the core
    cpuset = hwloc_bitmap_dup(core_obj->cpuset);

    // Use hwloc_get_next_obj_covering_cpuset_by_type to find the NUMA node
    hwloc_obj_t numa_node = NULL;
    while ((numa_node = hwloc_get_next_obj_covering_cpuset_by_type(topology, cpuset, HWLOC_OBJ_NUMANODE, numa_node)) !=
           NULL)
    {
        if (hwloc_bitmap_isincluded(cpuset, numa_node->cpuset))
            hwloc_numa_id = numa_node->logical_index;
    }

    // Cleanup
    hwloc_bitmap_free(cpuset);

    return hwloc_numa_id;
}

double get_hwloc_core_performance_by_id(const common_t *common, int hwloc_core_id)
{
    hwloc_topology_t topology = common->topology;

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

        double frequency_khz = 0;
        freq_file >> frequency_khz;
        freq_file.close();

        // Convert frequency to Hertz.
        double frequency_hz = frequency_khz * 1000;

        return common->flops_per_cycle * frequency_hz;
    }

    // # of flops the system can perform in one second.
    return common->flops_per_cycle * common->clock_frequency_hz;
}

std::vector<int> get_hwloc_numa_ids_by_address(const common_t *common, char *address, size_t size)
{
    hwloc_topology_t topology = common->topology;

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
    int ret = hwloc_get_area_memlocation(topology, address, size, nodeset, HWLOC_MEMBIND_BYNODESET);
    if (ret == 0)
    {
        int node;
        hwloc_bitmap_foreach_begin(node, nodeset)
        {
            numa_nodes.push_back(node); // Add NUMA node ID to the vector
        }
        hwloc_bitmap_foreach_end();
    }
    else
    {
        std::cerr << "Error: Not able to retrieving memory binding.\n";
    }

    // Cleanup
    hwloc_bitmap_free(nodeset);

    return numa_nodes;
}

std::string get_hwloc_thread_mem_policy(const common_t *common)
{
    hwloc_topology_t topology = common->topology;
    hwloc_bitmap_t nodeset;
    hwloc_membind_policy_t policy;
    std::ostringstream output; // String stream to build the output

    // Allocate nodeset
    nodeset = hwloc_bitmap_alloc();

    // Get the NUMA memory binding for the current thread
    int ret = hwloc_get_membind(topology, nodeset, &policy, HWLOC_MEMBIND_THREAD);
    if (ret == 0)
    {
        // Add memory policy to the output string
        switch (policy)
        {
        case HWLOC_MEMBIND_DEFAULT:
            output << "DEFAULT ";
            break;
        case HWLOC_MEMBIND_FIRSTTOUCH:
            output << "FIRSTTOUCH ";
            break;
        case HWLOC_MEMBIND_BIND:
            output << "BIND  ";
            break;
        case HWLOC_MEMBIND_INTERLEAVE:
            output << "INTERLEAVE ";
            break;
        case HWLOC_MEMBIND_NEXTTOUCH:
            output << "NEXTTOUCH ";
            break;
        case HWLOC_MEMBIND_MIXED:
            output << "MIXED ";
            break;
        default:
            output << "UNKNOWN ";
            break;
        }
        output << "memory policy - ";

        // Add NUMA nodes the thread is bound to
        output << "thread is bound to NUMA nodes ";
        int found = 0; // Track if any NUMA node is found
        int node;
        hwloc_bitmap_foreach_begin(node, nodeset)
        {
            hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(topology, nodeset, HWLOC_OBJ_NUMANODE, node);
            if (obj)
            {
                output << obj->logical_index << " "; // Add NUMA node ID
                found = 1;
            }
        }
        hwloc_bitmap_foreach_end();

        if (!found)
        {
            output << "None (not bound to any specific NUMA node)";
        }
    }
    else
    {
        output << "Error: retrieving memory policy.\n";
    }

    // Clean up
    hwloc_bitmap_free(nodeset);

    std::string result = output.str();
    if (!result.empty())
        result.pop_back(); // Remove the last space

    return result; // Return the constructed string
}

thread_locality_t get_hwloc_thread_locality(const common_t *common)
{
    hwloc_topology_t topology = common->topology;
    hwloc_bitmap_t cpuset;
    hwloc_obj_t obj;
    int numa_node, core_id;
    long core_migrations = -1; // Core migrations initialized to -1 to handle errors

    // Get the current thread's CPU binding
    cpuset = hwloc_bitmap_alloc();
    hwloc_get_last_cpu_location(topology, cpuset, HWLOC_CPUBIND_THREAD);

    // Get the NUMA node on which the current thread is running
    hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
    hwloc_cpuset_to_nodeset(topology, cpuset, nodeset);
    numa_node = hwloc_bitmap_first(nodeset); // Get the first NUMA node

    // Get the core object corresponding to the CPU binding
    obj = hwloc_get_obj_covering_cpuset(topology, cpuset);

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

void bind_exec_to_thread(thread_data_t *data)
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
        std::cerr << "Core " << data->assigned_core_id << " not found!" << std::endl;
        std::exit(EXIT_FAILURE);
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
    {
        CPU_SET(pu, &cpuset); // Set the corresponding processing unit in the CPU set
    }

    // Free the hwloc cpuset object as it's no longer needed
    hwloc_bitmap_free(hwloc_cpuset);

    // Set thread affinity
    int ret = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
    if (ret != 0)
    {
        std::cerr << "Error in pthread_attr_setaffinity_np: " << ret << std::endl;
        std::exit(EXIT_FAILURE); // Exit with failure status
    }

    // Mark the selected hwloc_core_id as unavailable (the thread will release it upon completion).
    common_set_core_id_as_unavail(data->common, data->assigned_core_id);

    if (pthread_create(&thread, &attr, data->thread_function, data) == 0)
    {
        // Increment the active thread counter (inside mutex)
        common_increment_active_threads_counter(data->common);
    }
    else
    {
        std::cerr << "Error: Unable to create thread for '" << data->exec->get_cname() << "'." << std::endl;
        std::exit(EXIT_FAILURE); // Exit with failure status
    }

    // Destroy the attributes object since it's no longer needed.
    // Destroy the pthread_attr_t immediately after creating the thread:
    // This is safe because the thread's attributes are copied into the new
    // thread when pthread_create is called. After that, the pthread_attr_t
    // object is no longer required.

    // You do not need to wait for the thread to finish in order to call pthread_attr_destroy.
    // The attributes object can be destroyed right after thread creation without affecting the thread itself.
    pthread_attr_destroy(&attr);

    // Optionally, join or detach the thread depending on your needs
    pthread_detach(thread); // or pthread_join(thread, NULL);

    // Clean up
    // thread_data is freed in thread_function.
}
