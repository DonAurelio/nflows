import networkx as nx

def read_dot_file(dot_file):
    """Read a task graph from a DOT file."""
    return nx.read_dot(dot_file)

def calculate_upward_rank(task_graph):
    """Calculate the upward rank for all tasks."""
    upward_rank = {}

    def compute_rank(task):
        if task in upward_rank:
            return upward_rank[task]
        
        avg_comp_cost = sum(map(float, task_graph.nodes[task]['computation_cost'].split(','))) / len(task_graph.graph['processors'])
        max_successor_rank = 0

        for _, successor, data in task_graph.out_edges(task, data=True):
            comm_cost = float(data['communication_cost'])
            max_successor_rank = max(max_successor_rank, compute_rank(successor) + comm_cost)

        upward_rank[task] = avg_comp_cost + max_successor_rank
        return upward_rank[task]

    for task in task_graph.nodes:
        compute_rank(task)

    return upward_rank

def heft(task_graph, num_processors):
    """HEFT scheduling algorithm."""
    upward_rank = calculate_upward_rank(task_graph)
    sorted_tasks = sorted(task_graph.nodes, key=lambda x: upward_rank[x], reverse=True)

    processor_availability = [0] * num_processors
    schedule = {}

    for task in sorted_tasks:
        computation_costs = list(map(float, task_graph.nodes[task]['computation_cost'].split(',')))
        earliest_finish_time = float('inf')
        best_processor = None
        best_start_time = None

        for proc in range(num_processors):
            earliest_start_time = processor_availability[proc]

            for pred, _, data in task_graph.in_edges(task, data=True):
                pred_proc = schedule[pred]['processor']
                if pred_proc != proc:
                    comm_cost = float(data['communication_cost'])
                    earliest_start_time = max(earliest_start_time, schedule[pred]['finish_time'] + comm_cost)
                else:
                    earliest_start_time = max(earliest_start_time, schedule[pred]['finish_time'])

            finish_time = earliest_start_time + computation_costs[proc]
            if finish_time < earliest_finish_time:
                earliest_finish_time = finish_time
                best_processor = proc
                best_start_time = earliest_start_time

        schedule[task] = {
            'processor': best_processor,
            'start_time': best_start_time,
            'finish_time': earliest_finish_time
        }
        processor_availability[best_processor] = earliest_finish_time

    return schedule

def print_schedule(schedule):
    """Print the generated schedule."""
    print("Task Scheduling:")
    for task, details in schedule.items():
        print(f"Task {task}: Processor {details['processor']}, Start Time {details['start_time']:.2f}, Finish Time {details['finish_time']:.2f}")

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print("Usage: python heft.py <dot_file> <num_processors>")
        sys.exit(1)

    dot_file = sys.argv[1]
    num_processors = int(sys.argv[2])

    task_graph = read_dot_file(dot_file)
    task_graph.graph['processors'] = [f"P{i}" for i in range(num_processors)]  # Add processor count to graph metadata

    schedule = heft(task_graph, num_processors)
    print_schedule(schedule)
