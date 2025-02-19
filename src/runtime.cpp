#include "runtime.hpp"

void runtime_initialize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper,
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
    *common = new common_t;
    common_t *cmn = *common;

    /* HWLOC TOPOLOGY */
    hwloc_topology_init(&cmn->topology);
    hwloc_topology_load(cmn->topology);

    /* TRACK ACTIVE THREADS. */
    cmn->active_threads = 0;
    cmn->cond = PTHREAD_COND_INITIALIZER;
    cmn->mutex = PTHREAD_MUTEX_INITIALIZER;

    /* CHECK READING OPERATION CONSISTENCY */
    cmn->checksum = 0;

    cmn->flops_per_cycle = data["flops_per_cycle"];
    cmn->clock_frequency_hz = data["clock_frequency_hz"]; // Dynamic (0) clock frequency.

    cmn->log_suffix = data["log_suffix"];

    // Latency (ns), Bandwidth (GB/s).
    cmn->distance_lat_ns = common_read_distance_matrix_from_file(data["distance_matrices"]["latency"]);
    cmn->distance_bw_gbps = common_read_distance_matrix_from_file(data["distance_matrices"]["bandwidth"]);

    // Set available cores.
    uint64_t mask = std::stoull(data["core_avail_mask"].get<std::string>(), nullptr, 16);

    int core_count = data["core_count"];
    cmn->core_avail.resize(core_count, false);

    for (int i = 0; i < core_count; ++i)
    {
        if (mask & (1ULL << i))
        {
            cmn->core_avail[i] = true;
        }
    }

    /* DAG */
    *dag = new simgrid_execs_t(common_read_dag_from_dot(data["dag_file"]));

    /* SCHEDULER */
    std::string scheduler_type = data["scheduler_type"];

    if (scheduler_type == "MIN_MIN")
    {
        *scheduler = new min_min_scheduler_t(cmn, **dag);
    }
    else if (scheduler_type == "HEFT")
    {
        *scheduler = new heft_scheduler_t(cmn, **dag);
    }
    else if (scheduler_type == "FIFO")
    {
        *scheduler = new fifo_scheduler_t(cmn, **dag);
    }
    else
    {
        std::cerr << "Unsupported scheduler '" << scheduler_type << "'." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    /* MAPPER */
    *mapper = new mapper_bare_metal_t(cmn, **scheduler);
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
