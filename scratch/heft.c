#include "scheduler.h"

double avg_computation_cost(const common_data_t *common_data, const simgrid_exec_t *exec);
double upward_rank(const common_data_t *common_data, const simgrid_exec_t* exec);


int main(int argc, char *argv[])
{
    /* HWLOC TOPOLOGY */
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    /* TRACK ACTIVE THREADS. */
    int active_threads = 0;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    /* TRACK THREADS EXECUTION TIME AND TASK/DATA LOCALITY. */
    comm_name_to_ptr_t comm_name_to_ptr;
    comm_name_to_numa_id_t comm_name_to_numa_id;
    comm_name_to_numa_id_t comm_name_to_numa_id_read;
    exec_name_to_locality_t exec_name_to_locality;

    comm_name_to_time_t comm_name_to_read_time;
    exec_name_to_compute_time_t exec_name_to_compute_time;
    comm_name_to_time_t comm_name_to_write_time;

    exec_name_to_time_t exec_name_to_time;

    // hwloc_core_ids_availability_t hwloc_core_ids_availability(get_all_hwloc_core_ids(&topology).size(), true);
    // hwloc_core_ids_availability_t hwloc_core_ids_availability(24, true);
    std::vector<bool> v = {
            true, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, true,
    };

    hwloc_core_ids_availability_t hwloc_core_ids_availability(v);

    /* NUMA BANDWIDTHS AND LATENCIES. */
    matrix_t NUMALatency;
    matrix_t NUMABandwidth;

    const std::string latency_file = "latency_matrix.txt";
    const std::string bandwidth_file = "bandwidth_matrix.txt";

    read_matrix_from_file(latency_file, NUMALatency);
    read_matrix_from_file(bandwidth_file, NUMABandwidth);

    int numa_nodes_num = NUMALatency.size();

    /* INITIALIZE COMMON DATA. */
    common_data_t *common_data = (common_data_t *)malloc(sizeof(common_data_t));

    common_data->topology = &topology;

    common_data->active_threads = &active_threads;
    common_data->mutex = &mutex;
    common_data->cond = &cond;

    common_data->numa_nodes_num = &numa_nodes_num;
    common_data->latency_matrix = &NUMALatency;
    common_data->bandwidth_matrix = &NUMABandwidth;

    common_data->comm_name_to_ptr = &comm_name_to_ptr;
    common_data->comm_name_to_numa_id = &comm_name_to_numa_id;
    // Since data migration may take place the numa node ids from which data 
    // is read be a thread may change.
    common_data->comm_name_to_numa_id_read = &comm_name_to_numa_id_read;
    common_data->exec_name_to_locality = &exec_name_to_locality;

    common_data->comm_name_to_read_time = &comm_name_to_read_time;
    common_data->exec_name_to_compute_time = &exec_name_to_compute_time;
    common_data->comm_name_to_write_time = &comm_name_to_write_time;

    common_data->exec_name_to_time = &exec_name_to_time;

    common_data->hwloc_core_ids_availability = &hwloc_core_ids_availability;
 
    /* READ DAG FROM FILE. */
    simgrid_execs_t execs = read_workflow(argv[1]);

    simgrid_execs_t ready_execs = get_ready_execs(execs);
    printf("There are %ld ready tasks\n", ready_execs.size());

    for (simgrid_exec_t* exec : ready_execs) {
        printf("Exec '%s' average completion time (us): %f\n", exec->get_cname(), avg_computation_cost(common_data, exec));
    }

 
    return 0;
}

double avg_computation_cost(const common_data_t *common_data, const simgrid_exec_t *exec)
{
    double sum_expected_completion_time = 0.0;
    std::vector<int> available_core_ids = get_hwloc_core_ids_by_availability(*(common_data->hwloc_core_ids_availability), true);

    // Estimate exec completion time for every hwloc_core_id.
    for (int hwloc_core_id : available_core_ids)
    {
        // Match all communication names (Task1->Task2) where this task_name is the destination.
        comm_name_time_ranges_t matched_comms = find_matching_time_ranges(common_data->comm_name_to_write_time, exec->get_name(), SECOND);

        uint64_t estimated_start_time_us = 0;
        double max_estimated_time_from_read_operations_us = 0.0;

        for (const auto &[comm_name, _start_time, _end_time, bytes_to_read] : matched_comms)
        {
            /* 1. ESTIMATE EXEC START TIME. */
            auto [parent_exec_name, self_exec_name] = split_by_arrow(std::string(comm_name));
            uint64_t prev_exec_finish_time = std::get<1>((*common_data->exec_name_to_time)[parent_exec_name]);
            if (prev_exec_finish_time > estimated_start_time_us)
                estimated_start_time_us = prev_exec_finish_time;

            /* 2. ESTIMATE EXEC READ TIME
            The start time of the current exec (task) will be
            start_time = estimated_start_time_us + read_max_estimated_time;
            */

            char *read_buffer = (*common_data->comm_name_to_ptr)[comm_name];

            // ASSUMPTION:
            // The pages of the data to be read can be distributed across multiple NUMA nodes,
            // depending on the memory allocation policy. For this estimation, the worst-case scenario is assumed.
            // Specifically, it is assumed that all data pages are located in a single NUMA node—the one with the
            // highest read time relative to the NUMA node of the requesting core. This approach provides an
            // upper-bound estimate of the reading operation.

            // Retrieve the NUMA nodes where the data pages to be read are allocated.
            std::vector<int> hwloc_read_src_numa_ids = get_hwloc_numa_ids_from_ptr(common_data->topology, read_buffer, (size_t)bytes_to_read);

            // Determine the NUMA node corresponding to the core that will perform the reading operation.
            int hwloc_read_dst_numa_id = get_hwloc_numa_id_from_hwloc_core_id(common_data->topology, hwloc_core_id);

            // Identify the source NUMA node representing the worst-case scenario, i.e.,
            // the node from which reading will incur the highest time cost, assuming all pages
            // are allocated on this NUMA node.
            double estimated_read_time = -1;

            for (int i : hwloc_read_src_numa_ids)
            {
                double read_latency = (*(common_data->latency_matrix))[i][hwloc_read_dst_numa_id];
                double read_bandwidth = (*(common_data->bandwidth_matrix))[i][hwloc_read_dst_numa_id];

                // TODO: Validate the consistency of bandwidth and latency units.
                double estimated_read_time = (bytes_to_read / read_bandwidth) + read_latency;
            }

            if (estimated_read_time > max_estimated_time_from_read_operations_us)
                max_estimated_time_from_read_operations_us = estimated_read_time;
        }

        /* 3. ESTIMATE COMPUTE TIME. */

        // TODO: Check units of core speed (it must be in FLOPS).
        double estimated_compute_time = exec->get_remaining() / get_current_core_speed_from_hwloc_core_id(common_data->topology, hwloc_core_id);

        /* 4. MAXIMUM DATA WRITE TIME (ESTIMATED).
        The finish time of the task is bound by the write operation taking the most.
        */

        // ASSUMPTION:
        // Since it is uncertain which NUMA node will handle the write operations for this task,
        // the worst-case scenario is assumed: the data is written to the farthest NUMA node,
        // i.e., the one with the highest latency relative to the specified hwloc core ID.
        int hwloc_write_src_numa_id = get_hwloc_numa_id_from_hwloc_core_id(common_data->topology, hwloc_core_id);
        int hwloc_write_dst_numa_id = -1;
        double max_write_latency = -1;

        for (size_t i = 0; i < (*(common_data->latency_matrix))[0].size(); ++i)
        {
            double w_latency = (*(common_data->latency_matrix))[hwloc_write_src_numa_id][i];
            if (w_latency > max_write_latency)
            {
                max_write_latency = w_latency;
                hwloc_write_dst_numa_id = i;
            }
        }

        if (hwloc_write_src_numa_id < 0 || hwloc_write_dst_numa_id < 0)
        {
            fprintf(stderr, "Error: invalid index hwloc_write_src_numa_id: '%d', hwloc_write_dst_numa_id: '%d'.\n", hwloc_write_src_numa_id, hwloc_write_dst_numa_id);
            std::exit(EXIT_FAILURE); // Exit with failure status
        }

        double write_latency = (*(common_data->latency_matrix))[hwloc_write_src_numa_id][hwloc_write_dst_numa_id];
        double write_bandwidth = (*(common_data->bandwidth_matrix))[hwloc_write_src_numa_id][hwloc_write_dst_numa_id];

        double max_estimated_time_from_write_operations_us = 0.0;
        for (const auto &parent : exec->get_successors())
        {
            const simgrid_comm_t *out_comm = dynamic_cast<simgrid_comm_t *>(parent.get());
            double bytes_to_write = out_comm->get_remaining();

            // TODO: Need to check bandwidth and latency units.
            // Bandwidth can be defined in Gbips or GBytes/s.
            double write_time = (bytes_to_write / write_bandwidth) + write_latency;

            if (write_time > max_estimated_time_from_write_operations_us)
                max_estimated_time_from_write_operations_us = write_time;
        }

        double estimated_completion_time_for_hwloc_core_id = estimated_start_time_us + max_estimated_time_from_read_operations_us + estimated_compute_time + max_estimated_time_from_write_operations_us;

        sum_expected_completion_time += estimated_completion_time_for_hwloc_core_id;

    }

    return sum_expected_completion_time / ((double) available_core_ids.size());
}

double upward_rank(const common_data_t *common_data, const simgrid_exec_t* exec)
{
    double task_avg_computation_cost = avg_computation_cost(common_data, exec);
    double max_succ_rank = 0.0;

    for (const auto &succ : exec->get_successors())
    {
        const simgrid_comm_t *succ_comm = dynamic_cast<simgrid_comm_t *>(succ.get());
        const simgrid_exec_t *succ_exec = dynamic_cast<simgrid_exec_t *>(succ_comm->get_successors().front().get());
        max_succ_rank = std::max(max_succ_rank, upward_rank(common_data, succ_exec));
    }

    return max_succ_rank;
}