#include "runtime.hpp"


void runtime_start(mapper_t **mapper)
{
    (*mapper)->start();
}

void runtime_write(common_t **common)
{
    common_print_common_structure(*common);
}

void runtime_initialize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper, const std::string &config_path)
{
    std::ifstream config_file(config_path);
    if (!config_file) {
        throw std::runtime_error("Unable to open config file: " + config_path);
    }

    nlohmann::json data;
    config_file >> data;
    config_file.close();

    *common = new common_t();
    initialize_common(*common, data);

    *dag = new simgrid_execs_t(common_read_dag_from_dot(data["dag_file"]));

    *scheduler = create_scheduler(data["scheduler_type"], *common, **dag);
    *mapper = create_mapper(data["mapper_type"], *common, **scheduler);
}

void runtime_finalize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper) {
    if (*mapper) {
        delete *mapper;
        *mapper = nullptr;
    }

    if (*scheduler) {
        delete *scheduler;
        *scheduler = nullptr;
    }

    if (*dag) {
        delete *dag;
        *dag = nullptr;
    }

    if (*common) {
        hwloc_topology_destroy((*common)->topology); // Release hwloc resources
        delete *common;
        *common = nullptr;
    }
}

void initialize_common(common_t *common, const nlohmann::json &data) {
    hwloc_topology_init(&(common->topology));
    hwloc_topology_load(common->topology);

    common->active_threads = 0;
    common->cond = PTHREAD_COND_INITIALIZER;
    common->mutex = PTHREAD_MUTEX_INITIALIZER;
    common->checksum = 0;

    size_t core_count;
    common->core_avail = parse_core_avail_mask(std::stoull(data["core_avail_mask"].get<std::string>(), nullptr, 16), core_count);
    common->core_avail_until.resize(core_count, 0.0);

    common->flops_per_cycle = data["flops_per_cycle"];
    std::string clock_frequency_type = data["clock_frequency_type"];

    if (clock_frequency_type == "dynamic") {
        common->clock_frequency_type = DYNAMIC_CLOCK_FREQUENCY;
        common->clock_frequency_hz = 0;
    } else if (clock_frequency_type == "static") {
        common->clock_frequency_type = STATIC_CLOCK_FREQUENCY;
        common->clock_frequency_hz = data["clock_frequency_hz"];
    } else if (clock_frequency_type == "array") {
        common->clock_frequency_type = ARRAY_CLOCK_FREQUENCY;
        common->clock_frequencies_hz = data["clock_frequencies_hz"].get<std::vector<double>>();
    } else {
        throw std::runtime_error("Unsupported clock frequency type: " + clock_frequency_type);
    }

    common->log_base_name = data["log_base_name"];
    common->log_date_format = data["log_date_format"];

    common->distance_lat_ns = common_read_distance_matrix_from_file(data["distance_matrices"]["latency"]);
    common->distance_bw_gbps = common_read_distance_matrix_from_file(data["distance_matrices"]["bandwidth"]);
}

std::vector<bool> parse_core_avail_mask(uint64_t mask, size_t &core_count) {
    core_count = 0;
    uint64_t temp_mask = mask;
    while (temp_mask) {
        core_count++;
        temp_mask >>= 1;
    }

    std::vector<bool> core_avail(core_count, false);
    for (size_t i = 0; i < core_count; ++i) {
        if (mask & (1ULL << i)) {
            core_avail[i] = true;
        }
    }
    return core_avail;
}

scheduler_t* create_scheduler(const std::string &type, common_t *common, simgrid_execs_t &dag) {
    if (type == "min_min") return new min_min_scheduler_t(common, dag);
    if (type == "heft") return new heft_scheduler_t(common, dag);
    if (type == "fifo") return new fifo_scheduler_t(common, dag);
    throw std::runtime_error("Unsupported scheduler: " + type);
}

mapper_t* create_mapper(const std::string &type, common_t *common, scheduler_t &scheduler) {
    if (type == "bare_metal") return new mapper_bare_metal_t(common, scheduler);
    if (type == "simulation") return new mapper_simulation_t(common, scheduler);
    throw std::runtime_error("Unsupported mapper: " + type);
}
