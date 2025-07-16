#include "runtime.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(runtime, "Messages specific to this module.");

void runtime_start(mapper_t **mapper)
{
    XBT_INFO("Start runtime.");
    (*mapper)->start();
}

void runtime_stop(common_t **common)
{
    XBT_INFO("End runtime.");
    common_print_common_structure(*common, 0);
}

void runtime_initialize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper, const std::string &config_path)
{
    XBT_INFO("Initialize runtime.");
    nlohmann::json data = common_config_file_read(config_path);
    simgrid_execs_t execs = common_dag_read_from_dot(data["dag_file"]);

    *dag = new simgrid_execs_t(execs);
    *common = new common_t();

    // Runtime system status.
    if (hwloc_topology_init(&((*common)->topology)) != 0)
        throw std::runtime_error("Failed to initialize topology.");

    if (hwloc_topology_load((*common)->topology) != 0)
        throw std::runtime_error("Failed to load topology.");

    (*common)->threads_active = 0;
    (*common)->threads_checksum = 0;

    (*common)->threads_cond = PTHREAD_COND_INITIALIZER;
    (*common)->threads_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Counters
    for (const simgrid_exec_t *exec :(**dag))
    {
        (*common)->execs_active[exec->get_name()] = 0;

        for (const auto &succ_ptr : exec->get_dependencies())
            (*common)->reads_active[(succ_ptr.get())->get_name()] = 0;

        for (const auto &succ_ptr : exec->get_successors())
            // Skipt all task_i->end communications.
            if (common_split((succ_ptr.get())->get_cname(), "->").second != std::string("end"))
                (*common)->writes_active[(succ_ptr.get())->get_name()] = 0;
    }

    // User-defined.
    (*common)->flops_per_cycle = data["flops_per_cycle"];
    (*common)->clock_frequency_type = common_clock_frequency_str_to_type(data["clock_frequency_type"]);

    switch ((*common)->clock_frequency_type) {
        case COMMON_DYNAMIC_CLOCK_FREQUENCY:
            (*common)->clock_frequency_hz = 0;
            break;
            
        case COMMON_STATIC_CLOCK_FREQUENCY:
            (*common)->clock_frequency_hz = data["clock_frequency_hz"];
            break;
            
        case COMMON_ARRAY_CLOCK_FREQUENCY:
            (*common)->clock_frequencies_hz = data["clock_frequencies_hz"].get<std::vector<double>>();
            break;
            
        default:
            // This should never happen if the input validation was proper
            XBT_ERROR("Invalid clock frequency type enum value");
            throw std::runtime_error("Invalid clock frequency type enum value");
    }

    const char* nflows_output_file_name = std::getenv("NFLOWS_OUTPUT_FILE_NAME");

    if (nflows_output_file_name) {
        (*common)->out_file_name = nflows_output_file_name;
    } else {
        (*common)->out_file_name = data["out_file_name"];
    }

    (*common)->distance_lat_ns = common_distance_matrix_read_from_txt(data["distance_matrices"]["latency_ns"]);
    (*common)->distance_bw_gbps = common_distance_matrix_read_from_txt(data["distance_matrices"]["bandwidth_gbps"]);

    std::string core_avail_mask = data["core_avail_mask"].get<std::string>();
    if (!core_avail_mask.empty()) {
        size_t core_count;
        (*common)->core_avail = common_core_avail_mask_to_vect(std::stoull(data["core_avail_mask"].get<std::string>(), nullptr, 16), core_count);
        (*common)->core_avail_until.resize(core_count, 0.0);
    } else {
        std::vector<unsigned> core_avail_ids = data["core_avail_ids"].get<std::vector<unsigned>>();
        (*common)->core_avail = common_core_avail_ids_to_vect(core_avail_ids);
        (*common)->core_avail_until.resize(((*common)->core_avail).size(), 0.0);
    }

    *scheduler = nullptr;
    const std::string scheduler_type = data["scheduler_type"];
    switch (common_scheduler_str_to_type(scheduler_type)) {
        case COMMON_SCHED_TYPE_MIN_MIN:
            *scheduler = new min_min_scheduler_t((*common), (**dag)); break;
        case COMMON_SCHED_TYPE_HEFT:
            *scheduler = new heft_scheduler_t((*common), (**dag)); break;
        case COMMON_SCHED_TYPE_FIFO:
            *scheduler = new fifo_scheduler_t((*common), (**dag)); break;
        case COMMON_SCHED_TYPE_UNKNOWN:
            XBT_ERROR("Invalid scheduler type: '%s'.", scheduler_type.c_str());
            throw std::runtime_error("Invalid scheduler type.");
            // break not needed here due to the throw
    }

    for (const auto& param : data["scheduler_params"].get<std::vector<std::string>>()) {
        auto pos = param.find('=');
        if (pos != std::string::npos) {
            (*common)->scheduler_params[param.substr(0, pos)] = param.substr(pos + 1);
        }
    }

    (*common)->mapper_mem_policy_type = common_mapper_mem_policy_str_to_type(data["mapper_mem_policy_type"]);
    (*common)->mapper_mem_bind_numa_node_ids = data["mapper_mem_bind_numa_node_ids"].get<std::vector<unsigned>>();

    *mapper = nullptr;
    std::string mapper_type = data["mapper_type"];
    (*common)->mapper_type = common_mapper_str_to_type(mapper_type);

    switch ((*common)->mapper_type) {
        case COMMON_MAPPER_BARE_METAL:
            *mapper = new mapper_bare_metal_t((*common), (**scheduler)); 
            break;
        case COMMON_MAPPER_SIMULATION:
            *mapper = new mapper_simulation_t((*common), (**scheduler)); break;
        default:
            XBT_ERROR("Invalid mapper type '%s'", 
                common_mapper_type_to_str((*common)->mapper_type).c_str());
            throw std::runtime_error("Invalid mapper type enum value.");
            // No break needed after throw (unreachable)
    }
}

void runtime_finalize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper) {

    XBT_INFO("Finalize runtime.");
    auto safe_delete = [](auto **ptr) {
        if (ptr && *ptr) {
            delete *ptr;
            *ptr = nullptr;
        }
    };

    if (common && (*common)->topology) hwloc_topology_destroy((*common)->topology);

    safe_delete(mapper);
    safe_delete(scheduler);
    safe_delete(dag);
    safe_delete(common);
}
