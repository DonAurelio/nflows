#include <simgrid/s4u.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <limits>

namespace sg4 = simgrid::s4u;

struct TaskInfo {
    std::string name;
    double computation_cost;
    int assigned_processor;
    double start_time;
    double finish_time;
};

using TaskMap = std::unordered_map<std::string, TaskInfo>;
using DependencyMap = std::unordered_map<std::pair<std::string, std::string>, double, 
                                         boost::hash<std::pair<std::string, std::string>>>;

// Read the task graph from a DOT file using SimGrid
auto read_dot_file(const std::string& dot_file_path) {
    auto task_graph = sg4::create_DAG_from_dot(dot_file_path);
    return task_graph;
}

// HEFT Scheduling algorithm
TaskMap heft_scheduling(const sg4::DAG& task_graph, int num_processors) {
    TaskMap task_schedule;
    std::unordered_map<std::string, double> upward_rank;

    // Calculate upward rank
    std::function<double(const std::string&)> calculate_upward_rank = [&](const std::string& task_name) {
        if (upward_rank.find(task_name) != upward_rank.end()) {
            return upward_rank[task_name];
        }

        auto task = task_graph.get_task(task_name);
        double avg_computation_cost = task->get_avg_computation_cost();
        double max_successor_rank = 0.0;

        for (const auto& successor : task->get_successors()) {
            auto comm_cost = task_graph.get_communication_cost(task_name, successor);
            max_successor_rank = std::max(max_successor_rank, calculate_upward_rank(successor) + comm_cost);
        }

        upward_rank[task_name] = avg_computation_cost + max_successor_rank;
        return upward_rank[task_name];
    };

    for (const auto& task : task_graph.get_tasks()) {
        calculate_upward_rank(task->get_name());
    }

    // Sort tasks by decreasing order of upward rank
    std::vector<std::string> sorted_tasks;
    for (const auto& task : task_graph.get_tasks()) {
        sorted_tasks.push_back(task->get_name());
    }

    std::sort(sorted_tasks.begin(), sorted_tasks.end(), [&](const std::string& a, const std::string& b) {
        return upward_rank[a] > upward_rank[b];
    });

    // Schedule tasks on processors
    std::vector<double> processor_availability(num_processors, 0.0);

    for (const auto& task_name : sorted_tasks) {
        auto task = task_graph.get_task(task_name);
        double earliest_finish_time = std::numeric_limits<double>::infinity();
        int selected_processor = -1;
        double selected_start_time = 0.0;

        for (int processor = 0; processor < num_processors; ++processor) {
            double earliest_start_time = processor_availability[processor];

            for (const auto& predecessor : task->get_predecessors()) {
                int pred_processor = task_schedule[predecessor].assigned_processor;
                if (pred_processor != processor) {
                    earliest_start_time = std::max(earliest_start_time, 
                        task_schedule[predecessor].finish_time + 
                        task_graph.get_communication_cost(predecessor, task_name));
                } else {
                    earliest_start_time = std::max(earliest_start_time, 
                        task_schedule[predecessor].finish_time);
                }
            }

            double finish_time = earliest_start_time + task->get_computation_cost(processor);
            if (finish_time < earliest_finish_time) {
                earliest_finish_time = finish_time;
                selected_processor = processor;
                selected_start_time = earliest_start_time;
            }
        }

        task_schedule[task_name] = {
            task_name,
            task->get_computation_cost(selected_processor),
            selected_processor,
            selected_start_time,
            earliest_finish_time
        };
        processor_availability[selected_processor] = earliest_finish_time;
    }

    return task_schedule;
}

int main(int argc, char* argv[]) {
    sg4::Engine e(&argc, argv);
    e.load_platform(argv[1]);

    std::string dot_file_path = argv[2];
    int num_processors = std::stoi(argv[3]);

    auto task_graph = read_dot_file(dot_file_path);
    auto schedule = heft_scheduling(task_graph, num_processors);

    std::cout << "Task Scheduling:\n";
    for (const auto& [task_name, info] : schedule) {
        std::cout << "Task " << task_name
                  << ": Processor " << info.assigned_processor
                  << ", Start Time " << info.start_time
                  << ", Finish Time " << info.finish_time << std::endl;
    }

    return 0;
}
