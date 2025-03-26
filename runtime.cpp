#include "runtime.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(runtime, "Messages specific to this module.");

void runtime_start(mapper_t **mapper)
{
    try {
        (*mapper)->start();
    }
    catch (const std::out_of_range &e) {
        XBT_ERROR("Error (out of range): %s", e.what());
        std::exit(EXIT_FAILURE);
    } 
    catch (const std::bad_alloc &e) {
        XBT_ERROR("Error (memory allocation failed): %s", e.what());
        std::exit(EXIT_FAILURE);
    } 
    catch (const std::runtime_error &e) {
        XBT_ERROR("Runtime Error: %s", e.what());
        std::exit(EXIT_FAILURE);
    } 
    catch (const std::exception &e) {
        XBT_ERROR("Standard Exception: %s", e.what());
        std::exit(EXIT_FAILURE);
    } 
    catch (...) {
        XBT_ERROR("Unknown Error: An unexpected exception occurred.");
        std::exit(EXIT_FAILURE);
    }
}

void runtime_stop(common_t **common)
{
    common_print_common_structure(*common, 0);
}

void runtime_initialize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper, const std::string &config_path)
{
    nlohmann::json data = common_config_file_read(config_path);
    simgrid_execs_t execs = common_dag_read_from_dot(data["dag_file"]);

    *dag = new simgrid_execs_t(execs);
    simgrid_execs_t dag_tmp = (**dag);

    *common = new common_t();
    common_t *common_tmp = (*common);

    // User-defined.
    common_tmp->flops_per_cycle = data["flops_per_cycle"];
    common_tmp->clock_frequency_type = common_clock_frequency_str_to_type(data["clock_frequency_type"]);

    switch (common_tmp->clock_frequency_type) {
        case COMMON_DYNAMIC_CLOCK_FREQUENCY:
            common_tmp->clock_frequency_hz = 0;
            break;
            
        case COMMON_STATIC_CLOCK_FREQUENCY:
            common_tmp->clock_frequency_hz = data["clock_frequency_hz"];
            break;
            
        case COMMON_ARRAY_CLOCK_FREQUENCY:
            common_tmp->clock_frequencies_hz = data["clock_frequencies_hz"].get<std::vector<double>>();
            break;
            
        default:
            // This should never happen if the input validation was proper
            XBT_ERROR("Invalid clock frequency type enum value");
            std::exit(EXIT_FAILURE);
    }

    common_tmp->out_file_name = data["out_file_name"];

    common_tmp->distance_lat_ns = common_distance_matrix_read_from_txt(data["distance_matrices"]["latency_ns"]);
    common_tmp->distance_bw_gbps = common_distance_matrix_read_from_txt(data["distance_matrices"]["bandwidth_gbps"]);

    size_t core_count;
    common_tmp->core_avail = common_core_avail_mask_to_vect(std::stoull(data["core_avail_mask"].get<std::string>(), nullptr, 16), core_count);
    common_tmp->core_avail_until.resize(core_count, 0.0);

    *scheduler = nullptr;
    switch (common_scheduler_str_to_type(data["scheduler_type"])) {
        case COMMON_SCHED_TYPE_MIN_MIN:
            *scheduler = new min_min_scheduler_t(common_tmp, dag_tmp);
        case COMMON_SCHED_TYPE_HEFT:
            *scheduler = new heft_scheduler_t(common_tmp, dag_tmp);
        case COMMON_SCHED_TYPE_FIFO:
            *scheduler = new fifo_scheduler_t(common_tmp, dag_tmp);
        default:
            XBT_ERROR("Invalid scheduler type enum");
            std::exit(EXIT_FAILURE);
    }

    for (const auto& param : data["scheduler_params"].get<std::vector<std::string>>()) {
        auto pos = param.find('=');
        if (pos != std::string::npos) {
            common_tmp->scheduler_params[param.substr(0, pos)] = param.substr(pos + 1);
        }
    }

    common_tmp->mapper_mem_policy_type = common_mapper_mem_policy_str_to_type(data["mapper_mem_policy_type"]);
    common_tmp->mapper_mem_bind_numa_node_ids = data["mapper_mem_bind_numa_node_id"].get<std::vector<unsigned>>();

    *mapper = nullptr;
    switch (common_mapper_str_to_type(data["mapper_type"])) {
        case COMMON_MAPPER_BARE_METAL:
            *mapper = new mapper_bare_metal_t(common_tmp, (**scheduler));
        case COMMON_MAPPER_SIMULATION:
            *mapper = new mapper_simulation_t(common_tmp, (**scheduler));
        default:
            XBT_ERROR("Unexpected mapper type");
            std::exit(EXIT_FAILURE);
    }

    // Runtime system status.
    hwloc_topology_init(&(common_tmp->topology));
    hwloc_topology_load(common_tmp->topology);

    common_tmp->threads_active = 0;
    common_tmp->threads_checksum = 0;

    common_tmp->threads_cond = PTHREAD_COND_INITIALIZER;
    common_tmp->threads_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Counters
    for (const simgrid_exec_t *exec : dag_tmp)
    {
        common_tmp->execs_active[exec->get_name()] = 0;

        for (const auto &succ_ptr : exec->get_dependencies())
            common_tmp->reads_active[(succ_ptr.get())->get_name()] = 0;

        for (const auto &succ_ptr : exec->get_successors())
            // Skipt all task_i->end communications.
            if (common_split((succ_ptr.get())->get_cname(), "->").second != std::string("end"))
                common_tmp->writes_active[(succ_ptr.get())->get_name()] = 0;
    }
}

void runtime_finalize(common_t **common, simgrid_execs_t **dag, scheduler_t **scheduler, mapper_t **mapper) {
    auto safe_delete = [](auto **ptr) {
        if (*ptr) {
            delete *ptr;
            *ptr = nullptr;
        }
    };

    safe_delete(mapper);
    safe_delete(scheduler);
    safe_delete(dag);

    if (*common) {
        hwloc_topology_destroy((*common)->topology); // Release hwloc resources
        safe_delete(common);
    }
}
