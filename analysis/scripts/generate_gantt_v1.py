import yaml
import random
import numpy as np
import matplotlib.pyplot as plt
import argparse
import os

def load_yaml(filename):
    with open(filename, 'r') as file:
        return yaml.safe_load(file)

def scale_time(value, unit):
    scale_factors = {'us': 1, 'ms': 1e3, 's': 1e6, 'min': 6e7}
    return value / scale_factors[unit]

def scale_payload(value, unit):
    scale_factors = {'B': 1, 'KB': 1e3, 'MB': 1e6, 'GB': 1e9}
    return value / scale_factors[unit]

def get_unique_color(existing_colors):
    while True:
        color = (random.random(), random.random(), random.random())
        if color not in existing_colors:
            existing_colors.add(color)
            return color

def plot_gantt(yaml_data, time_unit, payload_unit, use_numa, output_file):
    fig, ax = plt.subplots(figsize=(12, 6))
    resource_map = {}
    existing_colors = set()
    intervals = []

    comm_name_reads = {task_dest: 0 for task_dest in yaml_data['name_to_thread_locality']}
    for comm in yaml_data['comm_name_read_offsets']:
        _, task_dest = comm.split('->')
        comm_name_reads[task_dest] += 1
    max_read_value = max(comm_name_reads.values())

    comm_name_writes = {task_src: 0 for task_src in yaml_data['name_to_thread_locality']}
    for comm in yaml_data['comm_name_write_offsets']:
        task_src, _ = comm.split('->')
        comm_name_writes[task_src] += 1
    max_write_value = max(comm_name_writes.values())

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
        ax.barh(y_position, end - start, left=start, color=color, edgecolor='black', alpha=0.7)
        ax.text((start + end) / 2, y_position, f"{task}", ha='center', va='center', fontsize=8, color='black', weight='bold')

    comm_name_offset = {r: 1 for r in resource_map}
    for comm, times in yaml_data['comm_name_write_offsets'].items():
        task_src, task_dest = comm.split('->')
        resource_id = yaml_data['name_to_thread_locality'][task_src]['numa_id'] if use_numa else yaml_data['name_to_thread_locality'][task_src]['core_id']
        y_position = (resource_map[resource_id] * offset) + comm_name_offset[resource_id]
        comm_name_offset[resource_id] += 1
        start = scale_time(times['start'], time_unit)
        end = scale_time(times['end'], time_unit)
        color = get_unique_color(existing_colors)
        ax.barh(y_position, end - start, left=start, color=color, edgecolor='black', alpha=0.7, hatch='\\')
        ax.text((start + end) / 2, y_position, f"{comm}", ha='center', va='center', fontsize=8, color='black', weight='bold')

    for comm, times in yaml_data['comm_name_read_offsets'].items():
        _, task_dest = comm.split('->')
        resource_id = yaml_data['name_to_thread_locality'][task_dest]['numa_id'] if use_numa else yaml_data['name_to_thread_locality'][task_dest]['core_id']
        y_position = (resource_map[resource_id] * offset) + comm_name_offset[resource_id]
        comm_name_offset[resource_id] += 1
        start = scale_time(times['start'], time_unit)
        end = scale_time(times['end'], time_unit)
        color = get_unique_color(existing_colors)
        ax.barh(y_position, end - start, left=start, color=color, edgecolor='black', alpha=0.7, hatch='//')
        ax.text((start + end) / 2, y_position, f"{comm}", ha='center', va='center', fontsize=8, color='black', weight='bold')
    
    ax.set_yticks(np.arange(len(resource_map)) * offset)
    ax.set_yticklabels([f"Resource {r}" for r in resource_map.keys()])
    ax.set_xlabel(f"Time ({time_unit})")
    ax.set_ylabel("Resources")
    ax.set_title("Gantt Chart of Task Execution and Communication")
    plt.grid(axis='x', linestyle='--', alpha=0.6)

    file_ext = os.path.splitext(output_file)[1].lower()
    if file_ext in ['.png', '.pdf']:
        plt.savefig(output_file, bbox_inches='tight')
    else:
        print("Invalid output format. Use PNG or PDF.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot Gantt chart from YAML data.")
    parser.add_argument("yaml_file", type=str, help="Path to the YAML file")
    parser.add_argument("output_file", type=str, help="Output file (PNG or PDF)")
    parser.add_argument("--time_unit", type=str, default='us', choices=['us', 'ms', 's', 'min'], help="Time unit")
    parser.add_argument("--payload_unit", type=str, default='B', choices=['B', 'KB', 'MB', 'GB'], help="Payload unit")
    parser.add_argument("--use_numa", action='store_true', help="Use NUMA node instead of core ID")
    args = parser.parse_args()

    yaml_data = load_yaml(args.yaml_file)
    plot_gantt(yaml_data, args.time_unit, args.payload_unit, args.use_numa, args.output_file)
