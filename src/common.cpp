#include "common.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(common, "Messages specific to this module.");

/* UTILS */
double common_get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000 + tv.tv_usec;
}

std::string common_join(const std::vector<int> &vec, const std::string &delimiter)
{
    std::ostringstream oss;

    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << vec[i];
        if (i != vec.size() - 1)
        { // Avoid adding a delimiter after the last element
            oss << delimiter;
        }
    }

    return oss.str();
}

std::pair<std::string, std::string> common_split(const std::string &input, std::string delimiter)
{
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

std::vector<bool> common_core_avail_mask_to_vect(uint64_t mask, size_t &core_count)
{
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

nlohmann::json common_config_file_read(const std::string& config_path)
{
    // 1. Open file
    std::ifstream config_file(config_path);
    if (!config_file) {
        XBT_ERROR("Cannot open config file: %s", config_path.c_str());
        throw std::runtime_error("Config file error");
    }

    // 2. Parse JSON
    nlohmann::json data;
    try {
        config_file >> data;
    } catch (const nlohmann::json::exception& e) {
        XBT_ERROR("Invalid config file (json) in %s: %s", config_path.c_str(), e.what());
        // throw;  // Rethrow the same exception
    }

    return data;
}

/* USER */
simgrid_execs_t common_dag_read_from_dot(const std::string &file_name)
{
    simgrid_execs_t execs;
    for (auto &activity : simgrid::s4u::create_DAG_from_dot(file_name.c_str()))
        if (auto *exec = dynamic_cast<simgrid::s4u::Exec *>(activity.get()))
            execs.push_back((simgrid_exec_t *)exec);

    /* DELETE ENTRY TASK */  
    simgrid_exec_t *root = execs.front();
    
    // Mark comms as completed to release dependent execs.
    for (const auto &succ_ptr : root->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);

    // Mark exec as completed.
    root->complete(simgrid::s4u::Activity::State::FINISHED);
    execs.erase(execs.begin());

    /* DELETE EXIT TASK */  
    // TODO: The final tasks still include 'end' as a dependency.  
    // This is currently handled in other functions, but it is not a practical solution.
    execs.pop_back();

    return execs;
}

simgrid_execs_t common_dag_get_ready_execs(const simgrid_execs_t &execs)
{
    simgrid_execs_t ready_execs;
    for (auto &exec : execs)
        if (exec->dependencies_solved() && not exec->is_assigned())
            ready_execs.push_back(exec);

    return ready_execs;
}

clock_frequency_type_t common_clock_frequency_str_to_type(const std::string &type)
{
    if (type.compare("dynamic") == 0) return COMMON_DYNAMIC_CLOCK_FREQUENCY;
    if (type.compare("static") == 0) return COMMON_STATIC_CLOCK_FREQUENCY;
    if (type.compare("array") == 0) return COMMON_ARRAY_CLOCK_FREQUENCY;

    return COMMON_UNKNOWN_CLOCK_FREQUENCY;
}

std::string common_clock_frequency_type_to_str(const clock_frequency_type_t &type)
{
    switch (type) {
        case COMMON_DYNAMIC_CLOCK_FREQUENCY: return "dynamic";
        case COMMON_STATIC_CLOCK_FREQUENCY: return "static";
        case COMMON_ARRAY_CLOCK_FREQUENCY: return "array";
        case COMMON_UNKNOWN_CLOCK_FREQUENCY: return "unknown";
        default: return ""; 
    }
}

scheduler_type_t common_scheduler_str_to_type(const std::string &type)
{
    if (type.compare("min-min") == 0) return COMMON_SCHED_TYPE_MIN_MIN;
    if (type.compare("heft") == 0) return COMMON_SCHED_TYPE_HEFT;
    if (type.compare("fifo") == 0) return COMMON_SCHED_TYPE_FIFO;
    
    return COMMON_SCHED_TYPE_UNKNOWN;
}

std::string common_scheduler_type_to_str(const scheduler_type_t &type)
{
    switch (type) {
        case COMMON_SCHED_TYPE_MIN_MIN: return "min-min";
        case COMMON_SCHED_TYPE_HEFT: return "heft";
        case COMMON_SCHED_TYPE_FIFO: return "fifo";
        case COMMON_SCHED_TYPE_UNKNOWN: return "unknown";
        default: return "";
    }
}

std::string common_scheduler_param_get(const common_t *common, const std::string &key)
{
    auto it = common->scheduler_params.find(key);
    if (it != common->scheduler_params.end())
        return it->second;
    return "";
}

mapper_type_t common_mapper_str_to_type(const std::string &type)
{
    if (type.compare("bare-metal") == 0) return COMMON_MAPPER_BARE_METAL;
    if (type.compare("simulation") == 0) return COMMON_MAPPER_SIMULATION;

    return COMMON_MAPPER_UNKNOWN;
}

std::string common_mapper_type_to_str(const mapper_type_t &type)
{
    switch (type) {
        case COMMON_MAPPER_BARE_METAL: return "bare-metal";
        case COMMON_MAPPER_SIMULATION: return "simulation";
        case COMMON_MAPPER_UNKNOWN: return "unknown";
        default: return "";
    }
}

hwloc_membind_policy_t common_mapper_mem_policy_str_to_type(const std::string &type)
{
    if (type.compare("default") == 0) return HWLOC_MEMBIND_DEFAULT;
    if (type.compare("first-touch") == 0) return HWLOC_MEMBIND_FIRSTTOUCH;
    if (type.compare("bind") == 0) return HWLOC_MEMBIND_BIND;
    if (type.compare("interleave") == 0) return HWLOC_MEMBIND_INTERLEAVE;
    if (type.compare("next-touch") == 0) return HWLOC_MEMBIND_NEXTTOUCH;
    if (type.compare("mixed") == 0) return HWLOC_MEMBIND_MIXED;

    XBT_ERROR("Unsupported memory policy type '%s'.", type.c_str());
    throw std::runtime_error("Unsupported memory policy type '" + type + "'.");
}

std::string common_mapper_mem_policy_type_to_str(const hwloc_membind_policy_t &type)
{
    switch (type) {
        case HWLOC_MEMBIND_DEFAULT: return "default";
        case HWLOC_MEMBIND_FIRSTTOUCH: return "first-touch";
        case HWLOC_MEMBIND_BIND: return "bind";
        case HWLOC_MEMBIND_INTERLEAVE: return "interleave";
        case HWLOC_MEMBIND_NEXTTOUCH: return "next-touch";
        case HWLOC_MEMBIND_MIXED: return "mixed";
    }

    XBT_ERROR("Unsupported memory policy type.");
    throw std::runtime_error("Unsupported memory policy type.");
}

distance_matrix_t common_distance_matrix_read_from_txt(const std::string &txt_file)
{
    std::ifstream file(txt_file);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file " << txt_file << std::endl;
        exit(EXIT_FAILURE);
    }

    int n;
    file >> n; // Read matrix dimensions from the first line

    distance_matrix_t matrix(n, std::vector<double>(n));

    // Read matrix values
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            file >> matrix[i][j];
        }
    }
    file.close();

    return matrix;
}

std::vector<int> common_core_id_get_avail(const common_t *common)
{
    unsigned int i = 0;
    std::vector<int> avail_core_ids;
    for (auto value : common->core_avail)
    {
        if (value)
            avail_core_ids.push_back(i);
        ++i;
    }

    return avail_core_ids;
}

void common_core_id_set_avail(common_t *common, unsigned int core_id, bool avail)
{
    common->core_avail[core_id] = avail;
}

double common_core_id_get_avail_until(const common_t *common, unsigned int core_id)
{
    try {
        return common->core_avail_until.at(core_id);
    } catch (const std::out_of_range& e) {
        XBT_ERROR("Not found core_id: %d", core_id);
        throw;
    }
}

void common_core_id_set_avail_until(common_t *common, unsigned int core_id, double duration)
{
    common->core_avail_until[core_id] = duration;
}

name_to_time_range_payload_t common_comm_name_to_w_time_offset_payload_filter(const common_t *common, const std::string &dst_name)
{
    name_to_time_range_payload_t matches;

    for (const auto &[key, value] : common->comm_name_to_w_time_offset_payload)
    {
        auto [left, right] = common_split(key, "->");
        if (dst_name == right)
            matches[key] = value;
    }

    return matches;
}

/* USER UTILS */
double common_earliest_start_time(const common_t *common, const std::string &exec_name, unsigned int core_id)
{
    // EST(n_i,p_i) = max{ avail[j], max_{n_{m} e pred(n_i)}( AFT(n_{m}) + c_{m,i} ) };
    // - avail[j]  => earliest time which processor j will be ready for task execution.
    // - pred(n_i) => set of immediate predecessor tasks of task n_{i}.

    // Match all communication (Task1->Task2) where this task_name is the destination.
    double max_pred_actual_finish_time = 0.0;

    name_to_time_range_payload_t matches = common_comm_name_to_w_time_offset_payload_filter(common, exec_name);

    for (const auto &[comm_name, time_range_payload] : matches)
    {
        auto [pred_exec_name, self_exec_name] = common_split(comm_name, "->");
        time_range_payload_t rcw_time_offset_payload = common_exec_name_to_rcw_time_offset_payload_get(common, pred_exec_name);
        double pred_exec_name_end_time_offset = std::get<1>(rcw_time_offset_payload);
        max_pred_actual_finish_time = std::max(max_pred_actual_finish_time, pred_exec_name_end_time_offset);
    }

    double core_id_avail_until = common_core_id_get_avail_until(common, core_id);
    double earliest_start_time_us = std::max(core_id_avail_until, max_pred_actual_finish_time);
    
    return earliest_start_time_us;
}

double common_communication_time(const common_t *common, unsigned int src_numa_id, unsigned int dst_numa_id, double payload)
{
    double latency_ns = common->distance_lat_ns[src_numa_id][dst_numa_id];
    double bandwidth_gbps = common->distance_bw_gbps[src_numa_id][dst_numa_id];

    double latency_us = latency_ns / 1000;          // To microseconds
    double bandwidth_bpus = bandwidth_gbps * 1000;  // To B/us

    return latency_us + (payload / bandwidth_bpus);
}

double common_compute_time(const common_t *common, double flops, double clock_frequency_hz)
{
    // Calculate the compute time in microseconds.
    return (flops / (common->flops_per_cycle * clock_frequency_hz)) * 1000000;
}

/* RUNTIME */
void common_threads_checksum_update(common_t *common, size_t checksum)
{
    pthread_mutex_lock(&(common->threads_mutex));
    common->threads_checksum += checksum;
    pthread_mutex_unlock(&(common->threads_mutex));
}

void common_threads_active_increment(common_t *common)
{
    pthread_mutex_lock(&(common->threads_mutex));
    common->threads_active += 1;
    pthread_mutex_unlock(&(common->threads_mutex));
}

void common_threads_active_decrement(common_t *common)
{
    pthread_mutex_lock(&(common->threads_mutex));
    common->threads_active -= 1;
    if (common->threads_active == 0)
        pthread_cond_signal(&(common->threads_cond)); // Signal the main thread
    pthread_mutex_unlock(&(common->threads_mutex));
}

void common_threads_active_wait(common_t *common)
{
    // Wait for all threads to finish
    pthread_mutex_lock(&(common->threads_mutex));
    while (common->threads_active > 0)
        pthread_cond_wait(&(common->threads_cond), &(common->threads_mutex)); // Wait until active_threads == 0
    pthread_mutex_unlock(&(common->threads_mutex));
}

void common_execs_active_increment(common_t *common, const std::string &name)
{
    std::lock_guard<std::mutex> lock(common->counters_mutex);
    common->execs_active[name] += 1;
}

void common_reads_active_increment(common_t *common, const std::string &name)
{
    std::lock_guard<std::mutex> lock(common->counters_mutex);
    common->reads_active[name] += 1;
}

void common_writes_active_increment(common_t *common, const std::string &name)
{
    std::lock_guard<std::mutex> lock(common->counters_mutex);
    common->writes_active[name] += 1;
}

void common_comm_name_to_address_create(common_t *common, const std::string& comm_name, char* write_buffer)
{
    std::lock_guard<std::mutex> lock(common->comm_maps_mutex);
    common->comm_name_to_address[comm_name] = write_buffer;
}

char* common_comm_name_to_address_get(const common_t *common, const std::string& comm_name)
{
    try {
        return common->comm_name_to_address.at(comm_name);
    } catch (const std::out_of_range& e) {
        XBT_ERROR("Not found comm_name: %s", comm_name.c_str());
        throw;
    }
}

void common_comm_name_to_numa_ids_r_create(common_t *common, const std::string& comm_name, const std::vector<int>& memory_bindings) {
    std::lock_guard<std::mutex> lock(common->comm_maps_mutex);
    common->comm_name_to_numa_ids_r[comm_name] = memory_bindings;
}

void common_comm_name_to_numa_ids_w_create(common_t *common, const std::string& comm_name, const std::vector<int>& memory_bindings)
{
    std::lock_guard<std::mutex> lock(common->comm_maps_mutex);
    common->comm_name_to_numa_ids_w[comm_name] = memory_bindings;
}

std::vector<int> common_comm_name_to_numa_ids_w_get(const common_t *common, const std::string& comm_name)
{
    // Assuming common has a mutex member named 'mutex'
    std::lock_guard<std::mutex> lock(common->comm_maps_mutex);
    try {
        return common->comm_name_to_numa_ids_w.at(comm_name);
    } catch (const std::out_of_range& e) {
        XBT_ERROR("Not found comm_name: %s", comm_name.c_str());
        throw;
    }
}

void common_exec_name_to_thread_locality_create(common_t *common, const std::string& exec_name, const thread_locality_t& locality) {
    std::lock_guard<std::mutex> lock(common->exec_locality_mutex);
    common->exec_name_to_thread_locality[exec_name] = locality;
}

void common_comm_name_to_r_ts_range_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload) {
    std::lock_guard<std::mutex> lock(common->timestamp_mutex);
    common->comm_name_to_r_ts_range_payload[comm_name] = time_range_payload;
}

void common_comm_name_to_w_ts_range_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload) {
    std::lock_guard<std::mutex> lock(common->timestamp_mutex);
    common->comm_name_to_w_ts_range_payload[comm_name] = time_range_payload;
}

void common_exec_name_to_c_ts_range_payload_create(common_t *common, const std::string& exec_name, const time_range_payload_t& time_range_payload) {
    std::lock_guard<std::mutex> lock(common->timestamp_mutex);
    common->exec_name_to_c_ts_range_payload[exec_name] = time_range_payload;
}

void common_comm_name_to_r_time_offset_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload) {
    std::lock_guard<std::mutex> lock(common->offset_mutex);
    common->comm_name_to_r_time_offset_payload[comm_name] = time_range_payload;
}

void common_comm_name_to_w_time_offset_payload_create(common_t *common, const std::string& comm_name, const time_range_payload_t& time_range_payload) {
    std::lock_guard<std::mutex> lock(common->offset_mutex);
    common->comm_name_to_w_time_offset_payload[comm_name] = time_range_payload;
}

void common_exec_name_to_c_time_offset_payload_create(common_t *common, const std::string& exec_name, const time_range_payload_t& time_range_payload) {
    std::lock_guard<std::mutex> lock(common->offset_mutex);
    common->exec_name_to_c_time_offset_payload[exec_name] = time_range_payload;
}

void common_exec_name_to_rcw_time_offset_payload_create(common_t *common, const std::string& exec_name, const time_range_payload_t& time_range_payload) {
    std::lock_guard<std::mutex> lock(common->offset_mutex);
    common->exec_name_to_rcw_time_offset_payload[exec_name] = time_range_payload;
}

time_range_payload_t common_exec_name_to_rcw_time_offset_payload_get(const common_t *common, const std::string& exec_name)
{
    std::lock_guard<std::mutex> lock(common->offset_mutex);
    try {
        return common->exec_name_to_rcw_time_offset_payload.at(exec_name);
    } catch (const std::out_of_range& e) {
        XBT_ERROR("Not found exec_name: %s", exec_name.c_str());
        throw;
    }
}

/* OUTPUT */
void common_print_common_structure(const common_t *common, int indent = 0)
{
    std::string indent_str(indent, ' ');
    std::ofstream out(common->out_file_name);

    common_print_user(common, out, indent);
    common_print_workflow(common, out, indent);
    common_print_runtime(common, out, indent);
    common_print_trace(common, out, indent);

    out.close();
}

void common_print_user(const common_t *common, std::ostream &out, int indent = 0)
{
    std::string indent_str(indent, ' ');
    std::string indent_str1(indent + 2, ' ');
    std::string indent_str2(indent + 4, ' ');
    
    out << indent_str << "user" << ":\n";
    out << indent_str1 << "flops_per_cycle: " << common->flops_per_cycle << "\n";
    out << indent_str1 << "clock_frequency_type: " << common_clock_frequency_type_to_str(common->clock_frequency_type) << "\n";

    if (!common->clock_frequencies_hz.empty())
    {
        out << indent_str1 << "clock_frequencies_hz:\n";
        for (size_t i = 0; i < common->core_avail.size(); ++i)
            out << indent_str2 << i << ": " << common->clock_frequencies_hz[i] << "\n";
    } else {
        out << indent_str1 << "clock_frequency_hz: " << common->clock_frequency_hz << "\n";
    }

    common_print_distance_matrix(common->distance_lat_ns, "distance_lat_ns", out, indent + 2);
    common_print_distance_matrix(common->distance_bw_gbps, "distance_bw_gbps", out, indent + 2);

    out << std::endl;
}

void common_print_distance_matrix(const distance_matrix_t &matrix, const std::string &key, std::ostream &out, int indent = 0)
{
    if (matrix.empty()) return;

    std::string indent_str(indent, ' ');
    std::string indent_str1(indent + 2, ' ');
    
    out << indent_str << key << ":\n";
    for (const auto &row : matrix)
    {
        out << indent_str1 << "- [";
        for (size_t i = 0; i < row.size(); ++i)
        {
            out << row[i] << (i != row.size() - 1 ? ", " : "");
        }
        out << "]\n";
    }
}

void common_print_workflow(const common_t *common, std::ostream &out, int indent = 0)
{
    std::string indent_str(indent, ' ');
    std::string indent_str1(indent + 2, ' ');
    
    out << indent_str << "workflow" << ":\n";
    out << indent_str1 << "execs_count: " << common->execs_active.size() << "\n";
    out << indent_str1 << "reads_count: " << common->reads_active.size() << "\n";
    out << indent_str1 << "writes_count: " << common->writes_active.size() << "\n";
    out << std::endl;
}

void common_print_runtime(const common_t *common, std::ostream &out, int indent = 0)
{
    std::string indent_str(indent, ' ');
    std::string indent_str1(indent + 2, ' ');
    std::string indent_str2(indent + 4, ' ');
    
    out << indent_str << "runtime" << ":\n";
    out << indent_str1 << "threads_checksum: " << common->threads_checksum << "\n";
    out << indent_str1 << "threads_active: " << common->threads_active << "\n";
    out << indent_str1 << "tasks_active_count: " << common_name_to_count_get(common->execs_active) << "\n";
    out << indent_str1 << "reads_active_count: " << common_name_to_count_get(common->reads_active) << "\n";
    out << indent_str1 << "writes_active_count: " << common_name_to_count_get(common->writes_active) << "\n";

    if (!common->core_avail.empty())
    {
        out << indent_str1 << "core_availability:\n";
        for (size_t i = 0; i < common->core_avail.size(); ++i)
        {
            if (common->core_avail[i])  // Print only available cores
            {
                out << indent_str2 << i << ": {avail_until: " << common->core_avail_until[i] << "}" << "\n";
            }        
        }
    }
    out << std::endl;
}

void common_print_trace(const common_t *common, std::ostream &out, int indent = 0)
{
    std::string indent_str(indent, ' ');
    
    out << indent_str << "trace" << ":\n";
    common_print_name_to_thread_locality(common->exec_name_to_thread_locality, out, indent + 2);
    common_print_name_to_numa_ids(common->comm_name_to_numa_ids_w, "numa_mappings_write", out, indent + 2);
    common_print_name_to_numa_ids(common->comm_name_to_numa_ids_r, "numa_mappings_read", out, indent + 2);
    common_print_name_to_time_range_payload(common->comm_name_to_r_ts_range_payload, "comm_name_read_timestamps", out, indent + 2);
    common_print_name_to_time_range_payload(common->comm_name_to_w_ts_range_payload, "comm_name_write_timestamps", out, indent + 2);
    common_print_name_to_time_range_payload(common->exec_name_to_c_ts_range_payload, "exec_name_compute_timestamps", out, indent + 2);
    common_print_name_to_time_range_payload(common->comm_name_to_r_time_offset_payload, "comm_name_read_offsets", out, indent + 2);
    common_print_name_to_time_range_payload(common->comm_name_to_w_time_offset_payload, "comm_name_write_offsets", out, indent + 2);
    common_print_name_to_time_range_payload(common->exec_name_to_c_time_offset_payload, "exec_name_compute_offsets", out, indent + 2);
    common_print_name_to_time_range_payload(common->exec_name_to_rcw_time_offset_payload, "exec_name_total_offsets", out, indent + 2);
}

void common_print_name_to_thread_locality(const name_to_thread_locality_t &mapping, std::ostream &out, int indent = 0)
{
    if (mapping.empty()) return;

    std::string indent_str(indent, ' ');
    std::string indent_str1(indent + 2, ' ');
    
    out << indent_str << "name_to_thread_locality:\n";
    for (const auto &[key, loc] : mapping)
    {
        out << indent_str1 << key << ": {numa_id: " << loc.numa_id << ", core_id: " << loc.core_id
            << ", voluntary_cs: " << loc.voluntary_context_switches
            << ", involuntary_cs: " << loc.involuntary_context_switches << ", core_migrations: " << loc.core_migrations
            << "}\n";
    }
    out << std::endl;
}

void common_print_name_to_numa_ids(const name_to_numa_ids_t &mapping, const std::string header, std::ostream &out, int indent = 0)
{
    if (mapping.empty()) return;

    std::string indent_str(indent, ' ');
    std::string indent_str1(indent + 2, ' ');
    
    out << indent_str << header << ":\n";
    for (const auto &[key, values] : mapping)
    {
        out << indent_str1 << key << ": {numa_ids: [";
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it != values.begin())
                out << ", ";
            out << *it;
        }
        out << "]}\n";
    }
    out << std::endl;
}

void common_print_name_to_time_range_payload(const name_to_time_range_payload_t &mapping, const std::string &header, std::ostream &out, int indent = 0)
{
    if (mapping.empty()) return;

    std::string indent_str(indent, ' ');
    std::string indent_str1(indent + 2, ' ');
    
    out << indent_str << header << ":\n";
    for (const auto &[key, value] : mapping)
    {
        auto [start, end, bytes] = value;
        out << indent_str1 << key << ": {start: " << start << ", end: " << end << ", payload: " << bytes << "}\n";
    }
    out << std::endl;
}

/* OUTPUT UTILS */
size_t common_name_to_count_get(const name_to_count_t &mapping) {
    size_t total = 0;
    for (const auto &[name, count] : mapping) {
        total += count;
    }
    return total;
}
