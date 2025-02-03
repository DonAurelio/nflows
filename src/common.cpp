#include "common.hpp"

#include <ranges>
#include <iostream>


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
        if (value) avail_core_ids.push_back(i);
        ++i;
    }

    return avail_core_ids;
}

name_to_time_range_payload_t common_filter_name_ts_range_payload(
    const common_t *common,
    const std::string &name,
    CommonVectorType type,
    CommonCommNameMatch comm_part_to_match)
{
    name_to_time_range_payload_t matches;
    name_to_time_range_payload_t map;

    switch (type) {
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
        auto [left, right] = common_split(key,"->");

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
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    int n;
    file >> n; // Read matrix dimensions from the first line

    std::vector<std::vector<double>> matrix(n, std::vector<double>(n));

    // Read matrix values
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
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

std::string common_join(const std::vector<int>& vec, const std::string& delimiter)
{
    std::ostringstream oss;

    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i != vec.size() - 1) { // Avoid adding a delimiter after the last element
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
