#include "common.hpp"

#include <iostream>
#include <ranges>

XBT_LOG_NEW_DEFAULT_CATEGORY(common, "Messages specific to this module.");

std::string common_get_str_from_clock_frequency_type(clock_frequency_type_t type)
{
    switch (type) {
        case CommonClockFrequencyType::DYNAMIC_CLOCK_FREQUENCY: return "dynamic";
        case CommonClockFrequencyType::STATIC_CLOCK_FREQUENCY: return "static";
        case CommonClockFrequencyType::ARRAY_CLOCK_FREQUENCY: return "array";
        default: return "unknown";
    }
}

simgrid_execs_t common_read_dag_from_dot(const std::string &file_name)
{
    simgrid_execs_t execs;
    // for (auto &activity : simgrid::s4u::create_DAG_from_DAX(file_name.c_str()))
    for (auto &activity : simgrid::s4u::create_DAG_from_dot(file_name.c_str()))
        if (auto *exec = dynamic_cast<simgrid::s4u::Exec *>(activity.get()))
            execs.push_back((simgrid_exec_t *)exec);

    /* DELETE ENTRY/EXIT TASK */
    // TODO: The last tasks still have end as dependency.
    //  We need to remove this
    simgrid_exec_t *root = execs.front();

    for (const auto &succ_ptr : root->get_successors())
        (succ_ptr.get())->complete(simgrid::s4u::Activity::State::FINISHED);
    root->complete(simgrid::s4u::Activity::State::FINISHED);
    execs.erase(execs.begin());

    execs.pop_back();

    return execs;
}

simgrid_execs_t common_get_ready_tasks(const simgrid_execs_t &execs)
{
    simgrid_execs_t ready_execs;
    for (auto &exec : execs)
        if (exec->dependencies_solved() && not exec->is_assigned())
            ready_execs.push_back(exec);

    return ready_execs;
}

std::vector<int> common_get_avail_core_ids(const common_t *common)
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

void common_set_core_id_avail_unitl(common_t *common, unsigned int core_id, double duration)
{
    common->core_avail_until[core_id] = duration;
}

double common_get_core_id_avail_unitl(const common_t *common, unsigned int core_id)
{
    return common->core_avail_until.at(core_id);
}

void common_set_core_id_as_avail(common_t *common, unsigned int core_id)
{
    common->core_avail[core_id] = true;
}

void common_set_core_id_as_unavail(common_t *common, unsigned int core_id)
{
    common->core_avail[core_id] = false;
}

void common_increment_active_threads_counter(common_t *common)
{
    pthread_mutex_lock(&(common->mutex));
    common->active_threads += 1;
    pthread_mutex_unlock(&(common->mutex));
}

void common_decrement_active_threads_counter(common_t *common)
{
    pthread_mutex_lock(&(common->mutex));
    common->active_threads -= 1;
    if (common->active_threads == 0)
        pthread_cond_signal(&(common->cond)); // Signal the main thread
    pthread_mutex_unlock(&(common->mutex));
}

void common_wait_active_threads(common_t *common)
{
    // Wait for all threads to finish
    pthread_mutex_lock(&(common->mutex));
    while (common->active_threads > 0)
        pthread_cond_wait(&(common->cond), &(common->mutex)); // Wait until active_threads == 0
    pthread_mutex_unlock(&(common->mutex));
}

name_to_time_range_payload_t common_filter_name_to_time_range_payload(
    const common_t *common, const std::string &name, time_range_payload_type_t type, comm_name_match_t comm_part_to_match)
{
    name_to_time_range_payload_t matches;
    name_to_time_range_payload_t map;

    switch (type)
    {
    case COMM_READ_TIMESTAMPS:
        map = common->comm_name_to_r_ts_range_payload;
        break;
    case COMM_WRITE_TIMESTAMPS:
        map = common->comm_name_to_w_ts_range_payload;
        break;
    case COMPUTE_TIMESTAMPS:
        map = common->exec_name_to_c_ts_range_payload;
        break;
    case COMM_READ_OFFSETS:
        map = common->comm_name_to_r_time_offset_payload;
        break;
    case COMM_WRITE_OFFSETS:
        map = common->comm_name_to_w_time_offset_payload;
        break;
    case COMPUTE_OFFSETS:
        map = common->exec_name_to_c_time_offset_payload;
        break;
    default:
        map = common->exec_name_to_c_ts_range_payload;
        break;
    }

    for (const auto &[key, value] : map)
    {
        auto [left, right] = common_split(key, "->");

        std::string name_to_match = left.empty() ? name : (comm_part_to_match == COMM_SRC) ? left : right;
        if (name_to_match == name)
        {
            matches[key] = value;
        }
    }

    return matches;
}

double common_earliest_start_time(const common_t *common, const std::string &exec_name, unsigned int core_id)
{
    // EST(n_i,p_i) = max{ avail[j], max_{n_{m} e pred(n_i)}( AFT(n_{m}) + c_{m,i} ) };
    // - avail[j]  => earliest time which processor j will be ready for task execution.
    // - pred(n_i) => set of immediate predecessor tasks of task n_{i}.

    // In bare-metal mode, core_id_avail_until is always 0, so it has no effect  
    // in the estimation of the earliest start time.  
    // In simulation mode, core_id_avail_until represents the time at which the core becomes available.

    // Match all communication (Task1->Task2) where this task_name is the destination.
    double max_pred_actual_finish_time = 0.0;
    for (const auto &[comm_name, time_range_payload] : common_filter_name_to_time_range_payload(common, exec_name, COMM_WRITE_OFFSETS, COMM_DST))
    {
        auto [pred_exec_name, self_exec_name] = common_split(comm_name, "->");
        max_pred_actual_finish_time = std::max(
            max_pred_actual_finish_time, std::get<1>(common->exec_name_to_rcw_time_offset_payload.at(pred_exec_name)));
    }

    double core_id_avail_until = common_get_core_id_avail_unitl(common, core_id);
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

double common_compute_time(const common_t *common, double flops, double processor_speed_flops_per_second)
{
    // Calculate the compute time in microseconds.
    return (flops / processor_speed_flops_per_second) * 1000000;
}

std::vector<std::vector<double>> common_read_distance_matrix_from_file(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    int n;
    file >> n; // Read matrix dimensions from the first line

    std::vector<std::vector<double>> matrix(n, std::vector<double>(n));

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

double common_get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000 + tv.tv_usec;
}

std::string common_get_timestamped_filename(const std::string &base_name, const std::string &format = "%Y%m%d_%H%M%S")
{
    std::time_t now = std::time(nullptr);
    char buf[100];

    if (!format.empty() && std::strftime(buf, sizeof(buf), format.c_str(), std::localtime(&now)))
    {
        return base_name + buf + ".yaml";
    }
    return base_name + ".yaml";
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

std::pair<std::string, std::string> common_split(const std::string &input, std::string delimiter = "->")
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

void common_print_distance_matrix(const distance_matrix_t &matrix, const std::string &key, std::ostream &out)
{
    if (matrix.empty()) return;

    out << key << ":\n";
    for (const auto &row : matrix)
    {
        out << "  - [";
        for (size_t i = 0; i < row.size(); ++i)
        {
            out << row[i] << (i != row.size() - 1 ? ", " : "");
        }
        out << "]\n";
    }
    out << std::endl;
}

void common_print_name_to_numa_ids(const name_to_numa_ids_t &mapping, const std::string header, std::ostream &out)
{
    if (mapping.empty()) return;

    out << header << ":\n";
    for (const auto &[key, values] : mapping)
    {
        out << "  " << key << ": {numa_ids: [";
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

void common_print_name_to_thread_locality(const name_to_thread_locality_t &mapping, std::ostream &out)
{
    if (mapping.empty()) return;

    out << "name_to_thread_locality:\n";
    for (const auto &[key, loc] : mapping)
    {
        out << "  " << key << ": {numa_id: " << loc.numa_id << ", core_id: " << loc.core_id
            << ", voluntary_cs: " << loc.voluntary_context_switches
            << ", involuntary_cs: " << loc.involuntary_context_switches << ", core_migrations: " << loc.core_migrations
            << "}\n";
    }
    out << std::endl;
}

void common_print_name_to_time_range_payload(const name_to_time_range_payload_t &mapping, const std::string &header,
                                             std::ostream &out)
{
    if (mapping.empty()) return;

    out << header << ":\n";
    for (const auto &[key, value] : mapping)
    {
        auto [start, end, bytes] = value;
        out << "  " << key << ": {start: " << start << ", end: " << end << ", payload: " << bytes << "}\n";
    }
    out << std::endl;
}

void common_print_name_to_address(const name_to_address_t &mapping, std::ostream &out)
{
    if (mapping.empty()) return;

    out << "name_to_address:\n";
    for (const auto &[key, address] : mapping)
    {
        out << "  " << key << ": {address: " << static_cast<void *>(address) << "}\n";
    }
    out << std::endl;
}

void common_print_metadata(const common_t *common, std::ostream &out)
{
    out << "checksum: " << common->checksum << "\n";
    out << "active_threads: " << common->active_threads << "\n";
    out << "flops_per_cycle: " << common->flops_per_cycle << "\n";
    out << "clock_frequency_type: " << common_get_str_from_clock_frequency_type(common->clock_frequency_type) << "\n";
    out << "clock_frequency_hz: " << common->clock_frequency_hz << "\n";
    out << std::endl;

    if (!common->clock_frequencies_hz.empty())
    {
        out << "clock_frequencies_hz:\n";
        for (size_t i = 0; i < common->core_avail.size(); ++i)
        {
            out << "  " << i << ": " << common->clock_frequencies_hz[i] << "\n";
        }
        out << std::endl;
    }

    if (!common->core_avail.empty())
    {
        out << "core_availability:\n";
        for (size_t i = 0; i < common->core_avail.size(); ++i)
        {
            if (common->core_avail[i])  // Print only available cores
            {
                out << "  " << i << ": {avail_until: " << common->core_avail_until[i] << "}" << "\n";
            }        
        }
        out << std::endl;
    }
}

void common_print_common_structure(const common_t *common)
{
    std::string file_name = common_get_timestamped_filename(common->log_base_name, common->log_date_format);
    std::ofstream out(file_name);

    common_print_metadata(common, out);
    common_print_distance_matrix(common->distance_lat_ns, "distance_latency_ns", out);
    common_print_distance_matrix(common->distance_bw_gbps, "distance_bandwidth_gbs", out);
    common_print_name_to_numa_ids(common->comm_name_to_numa_ids_w, "numa_mappings_write", out);
    common_print_name_to_numa_ids(common->comm_name_to_numa_ids_r, "numa_mappings_read", out);
    common_print_name_to_thread_locality(common->exec_name_to_thread_locality, out);
    common_print_name_to_address(common->comm_name_to_address, out);
    common_print_name_to_time_range_payload(common->comm_name_to_r_ts_range_payload, "comm_name_read_timestamps", out);
    common_print_name_to_time_range_payload(common->comm_name_to_w_ts_range_payload, "comm_name_write_timestamps", out);
    common_print_name_to_time_range_payload(common->exec_name_to_c_ts_range_payload, "exec_name_compute_timestamps",
                                            out);
    common_print_name_to_time_range_payload(common->comm_name_to_r_time_offset_payload, "comm_name_read_offsets", out);
    common_print_name_to_time_range_payload(common->comm_name_to_w_time_offset_payload, "comm_name_write_offsets", out);
    common_print_name_to_time_range_payload(common->exec_name_to_c_time_offset_payload, "exec_name_compute_offsets",
                                            out);
    common_print_name_to_time_range_payload(common->exec_name_to_rcw_time_offset_payload, "exec_name_total_offsets",
                                            out);

    out.close();
}
