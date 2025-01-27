import numpy as np

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

    # Step 1: Calculate Upward Ranks
    num_tasks = len(tasks)
    avg_computation_cost = np.mean(computation_costs, axis=1)
    upward_rank = [0] * num_tasks

    def calculate_upward_rank(task):
        if upward_rank[task] > 0:
            return upward_rank[task]
        successors = [t for t in range(num_tasks) if communication_costs[task][t] > 0]
        if not successors:
            upward_rank[task] = avg_computation_cost[task]
        else:
            upward_rank[task] = avg_computation_cost[task] + max(
                communication_costs[task][s] + calculate_upward_rank(s) for s in successors
            )
        return upward_rank[task]

    for task in tasks:
        calculate_upward_rank(task)

    print("Upward Ranks:", upward_rank)

    # Step 2: Rank Tasks by Upward Rank in Descending Order
    ranked_tasks = sorted(tasks, key=lambda t: upward_rank[t], reverse=True)
    print("Ranked Tasks:", ranked_tasks)

    # Step 3: Initialize Processor Availability and Task Schedules
    processor_availability = {p: 0 for p in processors}  # Earliest available time for each processor
    task_schedule = {}  # Task to (processor, start time, finish time)

    # Step 4: Schedule Each Task in Order
    for task in ranked_tasks:
        earliest_finish_time = float('inf')
        best_processor = -1
        best_start_time = 0

        for processor in processors:
            # Calculate Earliest Start Time (EST) for this task on this processor
            est = processor_availability[processor]
            for pred in [t for t in tasks if communication_costs[t][task] > 0]:
                pred_finish_time = task_schedule[pred][2]
                if task_schedule[pred][0] != processor:  # Different processors
                    est = max(est, pred_finish_time + communication_costs[pred][task])
                else:
                    est = max(est, pred_finish_time)

            # Calculate Earliest Finish Time (EFT)
            eft = est + computation_costs[task][processor]

            # Update the best processor if EFT is smaller
            if eft < earliest_finish_time:
                earliest_finish_time = eft
                best_processor = processor
                best_start_time = est

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
