#!/usr/bin/env python3

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
    return float(value) / float(scale_factors[unit])

def scale_payload(value, unit):
    scale_factors = {'B': 1, 'KB': 1e3, 'MB': 1e6, 'GB': 1e9}
    return float(value) / float(scale_factors[unit])

def get_unique_color(existing_colors):
    while True:
        color = (random.random(), random.random(), random.random())
        if color not in existing_colors:
            existing_colors.add(color)
            return color

def compute_fig_size(yaml_data, time_unit, use_numa):
    """Computes the figure size dynamically based on task duration and number of resources."""
    
    # Get max execution time
    max_time = max(
        scale_time(times['end'], time_unit)
        for times in yaml_data['exec_name_compute_offsets'].values()
    )

    # Extract and sort resource IDs
    resource_ids = sorted(set(
        locality['numa_id'] if use_numa else locality['core_id']
        for locality in yaml_data['name_to_thread_locality'].values()
    ))
    num_resources = len(resource_ids)

    # Count communication events
    num_comms = len(yaml_data['comm_name_write_offsets']) + len(yaml_data['comm_name_read_offsets'])

    # Compute figure dimensions with constraints
    fig_width = max(8, min(20, max_time / 1e6))  # Width scales with time
    fig_height = max(6, min(15, num_resources * 1.2))  # Adjusted height scaling

    return fig_width, fig_height, resource_ids


def plot_gantt(yaml_data, output_file, time_unit='us', payload_unit='B', use_numa=True, title=None, xlabel=None, ylabel=None, resource_label=None):
    """Generates a Gantt chart from the given YAML data."""
    
    # Compute figure size and get ordered resource IDs
    fig_width, fig_height, resource_ids = compute_fig_size(yaml_data, time_unit, use_numa)
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))


    # Map resources to y-positions in sorted order
    resource_map = {resource_id: i for i, resource_id in enumerate(resource_ids)}

    # Adjust spacing to prevent overlap
    min_bar_height = 0.4  # Ensure readability
    y_spacing = max(2.0, 1.5 * min_bar_height)  # Adaptive spacing per resource

    existing_colors = set()
    task_offset_map = {r: 0 for r in resource_map}  # Task offset tracker per resource
    write_comm_offset_map = {r: 0 for r in resource_map}  # Write comm offset tracker per resource
    read_comm_offset_map = {r: 0 for r in resource_map}  # Read comm offset tracker per resource

    # Plot tasks
    for task, times in yaml_data['exec_name_compute_offsets'].items():
        locality = yaml_data['name_to_thread_locality'][task]
        resource_id = locality['numa_id'] if use_numa else locality['core_id']
        y_position = resource_map[resource_id] * y_spacing + task_offset_map[resource_id] * min_bar_height

        task_offset_map[resource_id] += 1  # Move next task to avoid overlap
        start = scale_time(times['start'], time_unit)
        end = scale_time(times['end'], time_unit)

        color = get_unique_color(existing_colors)
        ax.barh(y_position, end - start, left=start, color=color, edgecolor='black', alpha=0.7, height=min_bar_height)
        ax.text((start + end) / 2, y_position, f"{task}", ha='center', va='center', fontsize=8, color='black', weight='bold')

    # Plot write communications
    for comm, times in yaml_data['comm_name_write_offsets'].items():
        task_src, task_dest = comm.split('->')
        resource_id = yaml_data['name_to_thread_locality'][task_src]['numa_id'] if use_numa else yaml_data['name_to_thread_locality'][task_src]['core_id']
        y_position = resource_map[resource_id] * y_spacing + write_comm_offset_map[resource_id] * min_bar_height + y_spacing / 3

        write_comm_offset_map[resource_id] += 1  # Move next communication to avoid overlap
        start = scale_time(times['start'], time_unit)
        end = scale_time(times['end'], time_unit)

        color = get_unique_color(existing_colors)
        ax.barh(y_position, end - start, left=start, color=color, edgecolor='black', alpha=0.7, height=min_bar_height, hatch='\\')
        ax.text((start + end) / 2, y_position, f"{comm}", ha='center', va='center', fontsize=8, color='black', weight='bold')

    # Plot read communications
    for comm, times in yaml_data['comm_name_read_offsets'].items():
        _, task_dest = comm.split('->')
        resource_id = yaml_data['name_to_thread_locality'][task_dest]['numa_id'] if use_numa else yaml_data['name_to_thread_locality'][task_dest]['core_id']
        y_position = resource_map[resource_id] * y_spacing + read_comm_offset_map[resource_id] * min_bar_height + 2 * (y_spacing / 3)

        read_comm_offset_map[resource_id] += 1  # Move next communication to avoid overlap
        start = scale_time(times['start'], time_unit)
        end = scale_time(times['end'], time_unit)

        color = get_unique_color(existing_colors)
        ax.barh(y_position, end - start, left=start, color=color, edgecolor='black', alpha=0.7, height=min_bar_height, hatch='//')
        ax.text((start + end) / 2, y_position, f"{comm}", ha='center', va='center', fontsize=8, color='black', weight='bold')

    # Draw horizontal guidelines per resource
    for resource_id in resource_ids:
        y_position = resource_map[resource_id] * y_spacing + y_spacing / 2
        ax.axhline(y_position, color='gray', linestyle='--', alpha=0.6)

    # Set y-axis labels
    ax.set_yticks([resource_map[r] * y_spacing + y_spacing / 2 for r in resource_ids])
    ax.set_yticklabels([f"{resource_label} {r}" for r in resource_ids] if resource_label else [f"Resource {r}" for r in resource_ids])

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

    # Example usage
    data = load_yaml(args.yaml_file)
    plot_gantt(data, args.output_file, args.time_unit, args.payload_unit, args.use_numa, args.title, args.xlabel, args.ylabel, args.resource_label)
