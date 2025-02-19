#include "runtime.hpp"

void runtime_initialize(common_t *common, simgrid_execs_t *dag, scheduler_t *scheduler, mapper_t *mapper,
                        const std::string &config_path)
{
    std::ifstream config_file(config_path);

    if (!config_file)
    {
        std::cerr << "Unable to open config file '" << config_path << "'." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    nlohmann::json data;
    config_file >> data;
    config_file.close();

    /* INITIALIZE COMMON DATA. */
    common = new common_t;

    /* HWLOC TOPOLOGY */
    hwloc_topology_init(&common->topology);
    hwloc_topology_load(common->topology);

    /* TRACK ACTIVE THREADS. */
    common->active_threads = 0;
    common->cond = PTHREAD_COND_INITIALIZER;
    common->mutex = PTHREAD_MUTEX_INITIALIZER;

    /* CHECK READING OPERATION CONSISTENCY */
    common->checksum = 0;

    common->flops_per_cycle = data["flops_per_cycle"];
    common->clock_frequency_hz = data["clock_frequency_hz"]; // Dynamic (0) clock frequency.

    common->log_suffix = data["log_suffix"];

    // Latency (ns), Bandwidth (GB/s).
    common->distance_lat_ns = common_read_distance_matrix_from_file(data["distance_matrices"]["latency"]);
    common->distance_bw_gbps = common_read_distance_matrix_from_file(data["distance_matrices"]["bandwidth"]);

    // Set available cores.
    uint64_t mask = std::stoull(data["core_avail_mask"].get<std::string>(), nullptr, 16);

    int core_count = data["core_count"];
    common->core_avail.resize(core_count, false);

    for (int i = 0; i < core_count; ++i)
    {
        if (mask & (1ULL << i))
        {
            common->core_avail[i] = true;
        }
    }

    /* DAG */
    dag = new simgrid_execs_t(data["dag_file"]);

    /* SCHEDULER */
    if (data["scheduler_type"] == "MIN_MIN")
    {
        scheduler = new min_min_scheduler_t(common, *dag);
    }
    else if (data["scheduler_type"] == "HEFT")
    {
        scheduler = new heft_scheduler_t(common, *dag);
    }
    else if (data["scheduler_type"] == "FIFO")
    {
        scheduler = new fifo_scheduler_t(common, *dag);
    }
    else
    {
        std::cerr << "Unsupported scheduler '" << data["scheduler_type"] << "'." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    /* MAPPER */
    mapper = new mapper_t(common, *dag);
}

void runtime_finalize(common_t *common, simgrid_execs_t *dag, scheduler_t *scheduler, mapper_t *mapper)
{
    delete common;
    delete dag;
    delete scheduler;
    delete mapper;
}

void runtime_start(mapper_t *mapper)
{
    mapper->start();
}

void runtime_write(common_t *common)
{
    common_print_common_structure(common);
}
