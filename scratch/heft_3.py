# Import necessary libraries
import numpy as np

# Define the HEFT algorithm
def heft_scheduling(tasks, processors, computation_costs, communication_costs):
    """
    Implements the HEFT (Heterogeneous Earliest Finish Time) algorithm.
    
    Parameters:
    - tasks: List of task IDs in the DAG (e.g., [0, 1, 2, 3]).
    - processors: List of processor IDs (e.g., [0, 1, 2]).
    - computation_costs: Matrix (tasks x processors) with computation times.
    - communication_costs: Matrix (tasks x tasks) with communication costs.
    
    Returns:
    - A schedule mapping tasks to processors and their start/finish times.
    """

    # Step 1: Calculate Average Computation Costs (Bottom Level)
    avg_computation_cost = np.mean(computation_costs, axis=1)
    print("Average Computation Costs:", avg_computation_cost)

    # Step 2: Calculate Bottom Levels for Each Task
    num_tasks = len(tasks)
    bottom_levels = [0] * num_tasks

    def calculate_bottom_level(task):
        if bottom_levels[task] > 0:
            return bottom_levels[task]
        successors = [t for t in range(num_tasks) if communication_costs[task][t] > 0]
        if not successors:
            bottom_levels[task] = avg_computation_cost[task]
        else:
            bottom_levels[task] = avg_computation_cost[task] + max(
                communication_costs[task][s] + calculate_bottom_level(s) for s in successors
            )
        return bottom_levels[task]

    for task in tasks:
        calculate_bottom_level(task)

    print("Bottom Levels:", bottom_levels)

    # Step 3: Rank Tasks by Bottom Level in Descending Order
    ranked_tasks = sorted(tasks, key=lambda t: bottom_levels[t], reverse=True)
    print("Ranked Tasks:", ranked_tasks)

    # Step 4: Initialize Processor Availability and Task Schedules
    processor_availability = {p: 0 for p in processors}  # Earliest available time for each processor
    task_schedule = {}  # Task to (processor, start time, finish time)

    # Step 5: Schedule Each Task in Order
    for task in ranked_tasks:
        earliest_finish_time = float('inf')
        best_processor = -1
        best_start_time = 0

        for processor in processors:
            # Find the earliest start time for this task on the processor
            earliest_start_time = processor_availability[processor]
            for pred in [t for t in tasks if communication_costs[t][task] > 0]:
                pred_finish_time = task_schedule[pred][2]
                if task_schedule[pred][0] != processor:  # Different processors
                    earliest_start_time = max(
                        earliest_start_time, pred_finish_time + communication_costs[pred][task]
                    )
                else:
                    earliest_start_time = max(earliest_start_time, pred_finish_time)

            # Calculate the finish time for this processor
            finish_time = earliest_start_time + computation_costs[task][processor]
            if finish_time < earliest_finish_time:
                earliest_finish_time = finish_time
                best_processor = processor
                best_start_time = earliest_start_time

        # Assign the task to the best processor
        task_schedule[task] = (best_processor, best_start_time, earliest_finish_time)
        processor_availability[best_processor] = earliest_finish_time

    # Return the final schedule
    return task_schedule


# Example Usage
if __name__ == "__main__":
    # Define tasks and processors
    tasks = [0, 1, 2, 3]  # Task IDs
    processors = [0, 1]  # Processor IDs

    # Computation costs matrix (tasks x processors)
    computation_costs = np.array([
        [14, 16],  # Task 0
        [13, 19],  # Task 1
        [11, 13],  # Task 2
        [13, 8]    # Task 3
    ])

    # Communication costs matrix (tasks x tasks)
    communication_costs = np.array([
        [0, 18, 0, 0],  # Task 0
        [0, 0, 7, 9],   # Task 1
        [0, 0, 0, 11],  # Task 2
        [0, 0, 0, 0]    # Task 3
    ])

    # Run HEFT
    schedule = heft_scheduling(tasks, processors, computation_costs, communication_costs)

    # Print the schedule
    print("\nTask Schedule:")
    for task, (processor, start, finish) in schedule.items():
        print(f"Task {task}: Processor {processor}, Start {start}, Finish {finish}")
