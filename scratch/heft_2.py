import networkx as nx
import heapq

class Task:
    def __init__(self, task_id, computation_cost):
        self.task_id = task_id
        self.computation_cost = computation_cost
        self.rank = 0

    def __lt__(self, other):
        return self.rank > other.rank  # Higher rank tasks come first

def calculate_upward_rank(task, graph, computation_costs):
    successors = list(graph.successors(task))
    if not successors:
        return computation_costs[task]

    max_successor_rank = max(
        calculate_upward_rank(successor, graph, computation_costs) + graph.edges[task, successor]['weight']
        for successor in successors
    )
    return computation_costs[task] + max_successor_rank

def heft_scheduler(graph, computation_costs):
    tasks = {task: Task(task, computation_costs[task]) for task in graph.nodes()}

    for task in tasks.values():
        task.rank = calculate_upward_rank(task.task_id, graph, computation_costs)

    sorted_tasks = sorted(tasks.values(), key=lambda t: t.rank, reverse=True)

    schedule = []
    for task in sorted_tasks:
        schedule.append(task.task_id)

    return schedule

# Create a task dependency graph and save it as a DOT file
def create_sample_graph(dot_file_path):
    graph = nx.DiGraph()
    graph.add_edge('A', 'B', weight=2)
    graph.add_edge('A', 'C', weight=3)
    graph.add_edge('B', 'D', weight=1)
    graph.add_edge('C', 'D', weight=2)
    
    nx.drawing.nx_pydot.write_dot(graph, dot_file_path)
    return graph

# Read a DOT file to generate the graph
def read_graph_from_dot(dot_file_path):
    return nx.drawing.nx_pydot.read_dot(dot_file_path)

# Example usage
if __name__ == "__main__":
    dot_file = "task_graph.dot"

    # Create a sample graph and save it to a DOT file
    sample_graph = create_sample_graph(dot_file)

    # Define computation costs for each task
    computation_costs = {
        'A': 5,
        'B': 2,
        'C': 3,
        'D': 4
    }

    # Read the graph from the DOT file
    graph = read_graph_from_dot(dot_file)

    # Perform HEFT scheduling
    schedule = heft_scheduler(graph, computation_costs)

    print("Scheduled Task Order:", schedule)
