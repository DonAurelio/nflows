#include "scheduler.h"

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
    std::vector<bool> hwloc_cores_availability = {
            true, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, false,
            false, false, false, false, false, true,
    };

    hwloc_core_ids_availability_t hwloc_core_ids_availability(hwloc_cores_availability);

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
    int num_tasks = execs.size();

    print_workflow(execs);

    /* CREATE DUMMY HOST (CORE) */
    simgrid::s4u::NetZone *dummy_net_zone = simgrid::s4u::create_full_zone("zone0");
    simgrid::s4u::Host *dummy_host = dummy_net_zone->create_host("host0", "1Gf")->seal();
    dummy_net_zone->seal();

    print_cores_availability(hwloc_core_ids_availability, true, "Available Core IDs");

    while (execs.size() != 0)
    {
        simgrid_execs_t ready_execs = get_ready_execs(execs);
        printf("Ready tasks: %ld\n", ready_execs.size());
        

        if (not ready_execs.empty())
        {
            /* For each ready exec:
            - Get the core that minimizes the completion time.
            - Select the exec that has the minimum completion time on its best core.
            - Schedule the exec in the selected core.
            */
            simgrid_exec_t *selected_exec;
            int selected_hwloc_core_id;
            double min_expected_completion_time;

            uint64_t selection_start_time_us = get_time_us();
            std::tie(selected_exec, selected_hwloc_core_id, min_expected_completion_time) = get_min_exec(common_data, ready_execs);
            uint64_t selection_end_time_us = get_time_us();

            if (selected_hwloc_core_id != -1)
            {

                uint64_t assigment_start_time_us = get_time_us();

                // Initialize thread data.
                // exec_data is free'd by the thread.
                exec_data_t *exec_data = (exec_data_t *)malloc(sizeof(exec_data_t));

                exec_data->exec = selected_exec;
                exec_data->assigned_hwloc_core_id = selected_hwloc_core_id;
                exec_data->common_data = common_data;

                // Remove the task from the list of pending executions.
                execs.erase(std::remove(execs.begin(), execs.end(), selected_exec), execs.end());

                // Assign the execution task to the specified dummy host.
                assign_exec(exec_data, dummy_host);

                uint64_t assigment_end_time_us = get_time_us();

                printf("Assigned: %s, hwloc_core_id: %d, earliest_finish_time (us): %f, selection_time (us): %ld, assigment_time (us): %ld, progress: %ld/%d.\n", 
                    selected_exec->get_cname(), selected_hwloc_core_id, min_expected_completion_time, (selection_end_time_us-selection_start_time_us),
                    (assigment_end_time_us - assigment_start_time_us), num_tasks - execs.size(), num_tasks);
            }
            else
            {
                printf("There are not available cores, waiting 5 seconds....\n");
                sleep(5);
            }
        }
        else
        {
            printf("There are not ready tasks, waiting 5 seconds....\n");
            sleep(5);
        }
    }

    // Wait for all threads to finish
    pthread_mutex_lock(&mutex);
    while (active_threads > 0)
        pthread_cond_wait(&cond, &mutex); // Wait until active_threads == 0
    pthread_mutex_unlock(&mutex);

    /* Print statistics. */
    // std::cout << std::endl;
    // Generate timestamped file name
    std::string filename = generate_timestamped_filename("output");
    std::ofstream file(filename);
    // Latency in seconds, and bandwidth in bytes per second.
    print_matrix(NUMALatency, "numa_latency", file);
    print_matrix(NUMABandwidth, "numa_bandwidth", file);
    print_comm_name_to_numa_id(comm_name_to_numa_id, "comm_name_to_numa_id_write", file);
    print_comm_name_to_numa_id(comm_name_to_numa_id_read, "comm_name_to_numa_id_read", file);
    print_exec_name_to_locality(exec_name_to_locality, file);
    print_comm_name_to_read_time(comm_name_to_read_time, file);
    print_exec_name_to_compute_time(exec_name_to_compute_time, file);
    print_comm_name_to_write_time(comm_name_to_write_time, file);
    print_exec_name_to_time(exec_name_to_time, file);
    file.close();

    /* CLEAN UP */
    // Cleanup hwloc resources
    hwloc_topology_destroy(topology);

    free(common_data);

    return 0;
}