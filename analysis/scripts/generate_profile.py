#!/usr/bin/env python3

import yaml
import pandas as pd
import numpy as np
import argparse

from tabulate import tabulate
from colorama import Fore, Style


def scale_time(value, unit):
    scale_factors = {'us': 1, 'ms': 1e3, 's': 1e6, 'min': 6e7}
    return float(value) / float(scale_factors[unit])

def process_accesses(data):
    # Extract relevant data
    name_to_thread_locality = data["name_to_thread_locality"]
    numa_mappings_write = data["numa_mappings_write"]
    numa_mappings_read = data["numa_mappings_read"]

    # Initialize an empty list to store rows for the DataFrame
    rows = []

    # Helper function to process operations
    def process_access(data_item, task_name, cpu_node, mem_nodes, core_id, access_type):
        for mem_node in mem_nodes:
            rows.append([data_item, task_name, cpu_node, mem_node, core_id, access_type])

    # Process write access operations from numa_mappings_write
    for comm_name, mapping in numa_mappings_write.items():
        mem_nodes = mapping["numa_ids"]
        for task_name, locality in name_to_thread_locality.items():
            cpu_node = locality["numa_id"]
            core_id = locality["core_id"]
            if task_name in comm_name:
                if comm_name.split("->")[0] == task_name:  # Write access (task_name on the left)
                    process_access(comm_name, task_name, cpu_node, mem_nodes, core_id, "write")

    # Process read access operations from numa_mappings_read
    for comm_name, mapping in numa_mappings_read.items():
        mem_nodes = mapping["numa_ids"]
        for task_name, locality in name_to_thread_locality.items():
            cpu_node = locality["numa_id"]
            core_id = locality["core_id"]
            if task_name in comm_name:
                if comm_name.split("->")[1] == task_name:  # Read access (task_name on the right)
                    process_access(comm_name, task_name, cpu_node, mem_nodes, core_id, "read")

    # Create a DataFrame
    return pd.DataFrame(rows, columns=["data_item", "task_name", "cpu_node", "mem_node", "core_id", "access_type"])

def aggregation_matrix(df, equal=None):
    """
    Creates a matrix aggregating counts based on cpu_node vs mem_node.
    
    Parameters:
        df (pd.DataFrame): The input DataFrame containing 'cpu_node' and 'mem_node' columns.
        equal (bool or None): 
            - If True, aggregate where cpu_node == mem_node.
            - If False, aggregate where cpu_node != mem_node.
            - If None, aggregate all rows regardless of equality.
    
    Returns:
        pd.DataFrame: A matrix with aggregated counts (cpu_node vs mem_node).
    """
    # Ensure cpu_node and mem_node are treated as integers for matrix aggregation
    df['cpu_node'] = df['cpu_node'].astype(int)
    df['mem_node'] = df['mem_node'].astype(int)
    
    # Apply filtering based on the equality parameter
    if equal is True:
        filtered_df = df[df['cpu_node'] == df['mem_node']]
    elif equal is False:
        filtered_df = df[df['cpu_node'] != df['mem_node']]
    else:  # equal is None
        filtered_df = df
    
    # Create a pivot table for the matrix (cpu_node vs mem_node)
    matrix = filtered_df.pivot_table(
        index='cpu_node',
        columns='mem_node',
        values='task_name',
        aggfunc='count',
        fill_value=0
    )
    
    return matrix

def machine_average_numa_factor(data):
    """
    Computes the average NUMA factor (ratio of remote to local memory access latency)
    for matrices of size n x n.
    """

    matrix = data["distance_latency_ns"]

    # Convert the matrix into a Pandas DataFrame
    df = pd.DataFrame(matrix)

    # Extract the diagonal (local latencies)
    local_latencies = pd.Series(df.values.diagonal())

    # Compute the remote-to-local latency ratios
    ratios = []
    for i in range(df.shape[0]):  # Iterate through rows
        for j in range(df.shape[1]):  # Iterate through columns
            if i != j:  # Only consider off-diagonal elements (remote latencies)
                remote_latency = df.iloc[i, j]
                local_latency = local_latencies[i]
                ratios.append(remote_latency / local_latency)

    # Compute the average NUMA factor
    average_numa_factor = sum(ratios) / len(ratios)

    return average_numa_factor

def data_access_pattern_performance(t_accesses_matrix, d_relatve_latencies):
    """
    Computes the NUMA metric based on the provided matrices.
    
    Parameters:
        accesses_matrix (pd.DataFrame): Matrix of task accesses.
        relatve_latencies (pd.DataFrame): Matrix of distances (hwloc/numactl relative latencies).
        
    Returns:
        float: The calculated NUMA metric.
    """
    # Step 2: Compute q_distance_matrix (d_distance_matrix with diagonal set to 0)
    q_distance_matrix = d_relatve_latencies.copy()
    np.fill_diagonal(q_distance_matrix.values, 0)

    # Step 3: Compute T and Q
    T = t_accesses_matrix.values.sum()  # Sum of elements in t_accesses_matrix
    Q = q_distance_matrix.values.sum()  # Sum of elements in q_distance_matrix

    # Step 4: Compute weighted_sum
    weighted_sum = (t_accesses_matrix * q_distance_matrix).values.sum()

    # Step 5: Compute the metric
    numa_metric = (1 / (T * Q)) * weighted_sum if T > 0 and Q > 0 else 0

    return numa_metric

def compute_durations(data):
    read_time = sum(float(entry['end']) - float(entry['start']) for entry in data['comm_name_read_offsets'].values())
    write_time = sum(float(entry['end']) - float(entry['start']) for entry in data['comm_name_write_offsets'].values())
    compute_time = sum(float(entry['end']) - float(entry['start']) for entry in data['exec_name_compute_offsets'].values())
    
    comp_to_comm_ratio = compute_time / (read_time + write_time) if (read_time + write_time) > 0 else float('inf')
    comm_to_comp_ratio = (read_time + write_time) / compute_time if compute_time > 0 else float('inf')
    
    return read_time, write_time, compute_time, comp_to_comm_ratio, comm_to_comp_ratio

def compute_makespan(data):
    return max(float(entry['end']) for entry in data['exec_name_total_offsets'].values())

def print_profile(data, matrix_relative_latencies, time_unit):
    df = process_accesses(data)

    checksum = data.get("checksum", None)
    active_threads = data.get("active_threads", None)

    print(f"{Fore.CYAN}System Validation Flags:{Style.RESET_ALL}")
    print("")
    print(f"  {Fore.YELLOW}Checksum:{Style.RESET_ALL} {checksum}")
    print(f"  {Fore.YELLOW}Active Threads:{Style.RESET_ALL} {active_threads}")

    average_numa_factor = machine_average_numa_factor(data)
    print(f"\n{Fore.CYAN}Machine AVG NUMA Factor: {Fore.YELLOW}{average_numa_factor:.2f}{Style.RESET_ALL}")

    matrix_total_accesses_none = aggregation_matrix(df, equal=None)
    data_access_performance = data_access_pattern_performance(matrix_total_accesses_none, matrix_relative_latencies)

    read_time, write_time, compute_time, comp_to_comm_ratio, comm_to_comp_ratio = compute_durations(data)
    makespan = compute_makespan(data)

    print(f"\n{Fore.CYAN}Workflow Execution Metrics:{Style.RESET_ALL}")
    print("")
    print(f"  {Fore.YELLOW}Total Read Time ({time_unit}):{Style.RESET_ALL} {scale_time(read_time, time_unit):.2f}")
    print(f"  {Fore.YELLOW}Total Write Time ({time_unit}):{Style.RESET_ALL} {scale_time(write_time, time_unit):.2f}")
    print(f"  {Fore.YELLOW}Total Compute Time ({time_unit}):{Style.RESET_ALL} {scale_time(compute_time, time_unit):.2f}")
    print(f"  {Fore.YELLOW}Workflow Makespan ({time_unit}):{Style.RESET_ALL} {scale_time(makespan, time_unit):.2f}")
    print(f"  {Fore.YELLOW}Computation-to-Communication Ratio:{Style.RESET_ALL} {comp_to_comm_ratio:.2f}")
    print(f"  {Fore.YELLOW}Communication-to-Computation Ratio:{Style.RESET_ALL} {comm_to_comp_ratio:.2f}")
    print(f"  {Fore.YELLOW}Data Access Pattern Performance:{Style.RESET_ALL} {data_access_performance:.2f}")

    total_accesses = matrix_total_accesses_none.values.sum()
    print(f"\n{Fore.CYAN}Data Access Pattern: {Fore.YELLOW}{total_accesses.sum():.2f}{Fore.CYAN} accesses.{Style.RESET_ALL}")
    table_str = tabulate(df[["task_name", "core_id", "cpu_node", "mem_node", "data_item", "access_type"]], headers="keys", tablefmt="grid", showindex=False)
    print("\n" + "\n".join(f"  {line}" for line in table_str.split("\n")))
    # print(tabulate(df[["task_name", "core_id", "cpu_node", "mem_node", "data_item", "access_type"]], headers="keys", tablefmt="grid", showindex=False))
    # print("")

    matrix_local_accesses_equal = aggregation_matrix(df, equal=True)
    print(f"\n{Fore.GREEN}Local Accesses: {Fore.YELLOW}{matrix_local_accesses_equal.sum().sum()}{Style.RESET_ALL}")
    table_str = tabulate(matrix_local_accesses_equal, tablefmt="grid", showindex=False)
    print("\n" + "\n".join(f"  {line}" for line in table_str.split("\n")))
    # print(tabulate(matrix_local_accesses_equal, tablefmt="grid", showindex=False))

    local_accesses_percentage = matrix_local_accesses_equal / total_accesses
    print(f"\n{Fore.GREEN}Local Accesses (%): {Fore.YELLOW}{local_accesses_percentage.sum().sum() * 100:.2f}%{Style.RESET_ALL}")
    table_str = tabulate(local_accesses_percentage, tablefmt="grid", showindex=False)
    print("\n" + "\n".join(f"  {line}" for line in table_str.split("\n")))
    # print(tabulate(local_accesses_percentage, tablefmt="grid", showindex=False))

    matrix_remote_accesses_different = aggregation_matrix(df, equal=False)
    print(f"\n{Fore.BLUE}Remote Accesses: {Fore.YELLOW}{matrix_remote_accesses_different.sum().sum()}{Style.RESET_ALL}")
    table_str = tabulate(matrix_remote_accesses_different, tablefmt="grid", showindex=False)
    print("\n" + "\n".join(f"  {line}" for line in table_str.split("\n")))
    # print(tabulate(matrix_remote_accesses_different, tablefmt="grid", showindex=False))

    remote_accesses_percentage = matrix_remote_accesses_different / total_accesses
    print(f"\n{Fore.BLUE}Remote Accesses (%): {Fore.YELLOW}{remote_accesses_percentage.sum().sum() * 100:.2f}%{Style.RESET_ALL}")
    table_str = tabulate(remote_accesses_percentage, tablefmt="grid", showindex=False)
    print("\n" + "\n".join(f"  {line}" for line in table_str.split("\n")))
    # print(tabulate(remote_accesses_percentage, tablefmt="grid", showindex=False))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Process NUMA access data.")
    parser.add_argument("input_file", help="Path to input YAML file")
    parser.add_argument("input_file_rel_lat", help="Path to input TXT file")
    parser.add_argument("--time_unit", type=str, choices=['us', 'ms', 's', 'min'], default='us', help="Time unit for scaling.")
    args = parser.parse_args()
    
    with open(args.input_file, "r") as file:
        data = yaml.load(file, Loader=yaml.FullLoader)

    with open(args.input_file_rel_lat, 'r') as file:
        dimension = int(file.readline().strip())
        rel_lat_matrix = pd.DataFrame([list(map(float, file.readline().split())) for _ in range(dimension)])

    print_profile(data, rel_lat_matrix, args.time_unit)
