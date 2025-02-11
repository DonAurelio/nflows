#include "common.hpp"

#include <iostream>
#include <ranges>

simgrid_execs_t common_read_dag_from_dot(const std::string &file_name)
{
    simgrid_execs_t execs;
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

name_to_time_range_payload_t common_filter_name_ts_range_payload(const common_t *common, const std::string &name,
                                                                 CommonVectorType type,
                                                                 CommonCommNameMatch comm_part_to_match)
{
    name_to_time_range_payload_t matches;
    name_to_time_range_payload_t map;

    switch (type)
    {
    case COMM_READ:
        map = common->comm_name_to_r_ts_range_payload;
        break;
    case COMM_WRITE:
        map = common->comm_name_to_w_ts_range_payload;
        break;
    default:
        map = common->exec_name_to_c_ts_range_payload;
        break;
    }

    for (const auto &[key, value] : map)
    {
        auto [left, right] = common_split(key, "->");

        std::string name_to_match = left.empty() ? name : (comm_part_to_match == SRC) ? left : right;
        if (name_to_match == name)
        {
            matches[key] = value;
        }
    }

    return matches;
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

uint64_t common_get_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

std::string common_get_timestamped_filename(const std::string &base_name)
{
    std::time_t now = std::time(nullptr);
    char buf[100];
    if (std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&now)))
    {
        return base_name + "_" + buf + ".yml";
    }
    return base_name + ".yml";
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
    out << "name_to_address:\n";
    for (const auto &[key, address] : mapping)
    {
        out << "  " << key << ": {address: " << static_cast<void *>(address) << "}\n";
    }
    out << std::endl;
}

void common_print_metadata(const common_t *common, std::ostream &out)
{
    out << "common_metadata:\n";
    out << "  active_threads: " << common->active_threads << "\n";
    out << "  flops_per_cycle: " << common->flops_per_cycle << "\n";
    out << "  clock_frequency_hz: " << common->clock_frequency_hz << "\n\n";
}

void common_print_common_structure(const common_t *common, std::ostream &out)
{
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
}
