#!/bin/python3

import yaml
import random
import numpy as np
import matplotlib.pyplot as plt
import argparse

def load_yaml(filename):
    with open(filename, 'r') as file:
        return yaml.safe_load(file)

def scale_time(value, unit):
    scale_factors = {'us': 1, 'ms': 1e3, 's': 1e6, 'min': 6e7}
    return value / scale_factors[unit]

def scale_payload(value, unit):
    scale_factors = {'B': 1, 'KB': 1e3, 'MB': 1e6, 'GB': 1e9}
    return float(value) / scale_factors[unit]

def get_unique_color(existing_colors):
    while True:
        color = (random.random(), random.random(), random.random())
        if color not in existing_colors:
            existing_colors.add(color)
            return color

def plot_gantt(yaml_data, output_file, time_unit='us', payload_unit='B', use_numa=True, title=None, xlabel=None, ylabel=None, resource_label=None):
    fig, ax = plt.subplots(figsize=(12, 6))
    
    resource_map = {}
    existing_colors = set()
    intervals = []

    comm_name_reads = {}
    for comm, times in yaml_data['comm_name_read_offsets'].items():
        _, task_dest = comm.split('->')
        if task_dest not in comm_name_reads:
            comm_name_reads[task_dest] = 1
        else:
            comm_name_reads[task_dest] += 1

    max_read_key, max_read_value = max(comm_name_reads.items(), key=lambda item: item[1])

    comm_name_writes = {}
    for comm, times in yaml_data['comm_name_write_offsets'].items():
        task_src, _ = comm.split('->')
        if task_src not in comm_name_writes:
            comm_name_writes[task_src] = 1
        else:
            comm_name_writes[task_src] += 1

    max_write_key, max_write_value = max(comm_name_writes.items(), key=lambda item: item[1])

    offset = max_read_value + max_write_value + 1

    for task, locality in yaml_data['name_to_thread_locality'].items():
        resource_id = locality['numa_id'] if use_numa else locality['core_id']
        if resource_id not in resource_map:
            resource_map[resource_id] = len(resource_map)

    for task, times in yaml_data['exec_name_compute_offsets'].items():
        locality = yaml_data['name_to_thread_locality'][task]
        resource_id = locality['numa_id'] if use_numa else locality['core_id']
        y_position = resource_map[resource_id] * offset
        start = scale_time(times['start'], time_unit)
        end = scale_time(times['end'], time_unit)
        color = get_unique_color(existing_colors)
        payload = scale_payload(times['payload'], payload_unit)
        ax.barh(y_position, end - start, left=start, color=color, edgecolor='black', alpha=0.7)
        ax.text((start + end) / 2, y_position, f"{task}", ha='center', va='center', fontsize=8, color='black', weight='bold')

    ax.set_yticks(np.arange(len(resource_map)) * offset)
    ax.set_yticklabels([f"{resource_label} {r}" for r in resource_map.keys()] if resource_label else [f"Resource {r}" for r in resource_map.keys()])
    ax.set_xlabel(xlabel if xlabel else f"Time ({time_unit})")
    ax.set_ylabel(ylabel if ylabel else "Resources")
    ax.set_title(title if title else "Gantt Chart of Task Execution and Communication")
    plt.grid(axis='x', linestyle='--', alpha=0.6)
    plt.savefig(output_file, format=output_file.split('.')[-1])
    plt.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a Gantt chart from a YAML file.")
    parser.add_argument("yaml_file", type=str, help="Path to the YAML file containing scheduling data.")
    parser.add_argument("output_file", type=str, help="Output file (PNG or PDF).")
    parser.add_argument("--time_unit", type=str, choices=['us', 'ms', 's', 'min'], default='us', help="Time unit for scaling.")
    parser.add_argument("--payload_unit", type=str, choices=['B', 'KB', 'MB', 'GB'], default='B', help="Payload unit for scaling.")
    parser.add_argument("--use_numa", action="store_true", help="Use NUMA instead of core ID for resource mapping.")
    parser.add_argument("--title", type=str, help="Title of the plot.")
    parser.add_argument("--xlabel", type=str, help="Label for the x-axis.")
    parser.add_argument("--ylabel", type=str, help="Label for the y-axis.")
    parser.add_argument("--resource_label", type=str, help="Label for resource names.")
    
    args = parser.parse_args()
    
    data = load_yaml(args.yaml_file)
    plot_gantt(data, args.output_file, args.time_unit, args.payload_unit, args.use_numa, args.title, args.xlabel, args.ylabel, args.resource_label)
