import yaml
import matplotlib.pyplot as plt
import numpy as np
import itertools

def scale_value(value, scale):
    scales = {'us': 1, 'ms': 1e-3, 's': 1e-6, 'min': 1e-6 / 60}
    return value * scales.get(scale, 1)

def scale_payload(value, scale):
    scales = {'B': 1, 'KB': 1e-3, 'MB': 1e-6, 'GB': 1e-9}
    return value * scales.get(scale, 1)

def plot_gantt(yaml_data, y_axis='NUMA', time_scale='us', payload_scale='B'):
    data = yaml.safe_load(yaml_data)
    tasks = data['exec_name_compute_offsets']
    reads = data['comm_name_read_offsets']
    writes = data['comm_name_write_offsets']
    locality = data['name_to_thread_locality']
    
    y_mapping = {}
    y_labels = []
    
    for task, loc in locality.items():
        key = loc['NUMA ID'] if y_axis == 'NUMA' else loc['Core ID']
        if key not in y_mapping:
            y_mapping[key] = len(y_mapping) * 3  # Space out to prevent overlap
            y_labels.append(key)
    
    fig, ax = plt.subplots(figsize=(10, 6))
    compute_colors = itertools.cycle(['blue', 'purple', 'orange'])
    read_colors = itertools.cycle(['green', 'lime', 'teal'])
    write_colors = itertools.cycle(['red', 'brown', 'pink'])
    label_colors = {}
    
    def get_color(label, category):
        if label not in label_colors:
            if category == 'compute':
                label_colors[label] = next(compute_colors)
            elif category == 'read':
                label_colors[label] = next(read_colors)
            elif category == 'write':
                label_colors[label] = next(write_colors)
        return label_colors[label]
    
    offsets = {'compute': 0, 'read': 1, 'write': 2}  # Avoid bar overlap
    
    for task, times in tasks.items():
        start = scale_value(times['Start'], time_scale)
        end = scale_value(times['End'], time_scale)
        payload = scale_payload(times['Payload'], payload_scale)
        duration = end - start
        y_pos = y_mapping[locality[task]['NUMA ID'] if y_axis == 'NUMA' else locality[task]['Core ID']] + offsets['compute']
        ax.barh(y_pos, duration, left=start, color=get_color(task, 'compute'), label=f"{task} Compute ({payload} {payload_scale})")
    
    for comm, times in reads.items():
        start = scale_value(times['Start'], time_scale)
        end = scale_value(times['End'], time_scale)
        payload = scale_payload(times['Payload'], payload_scale)
        duration = end - start
        y_pos = y_mapping[locality[comm.split('->')[0]]['NUMA ID'] if y_axis == 'NUMA' else locality[comm.split('->')[0]]['Core ID']] + offsets['read']
        ax.barh(y_pos, duration, left=start, color=get_color(comm, 'read'), label=f"{comm} Read ({payload} {payload_scale})")
    
    for comm, times in writes.items():
        start = scale_value(times['Start'], time_scale)
        end = scale_value(times['End'], time_scale)
        payload = scale_payload(times['Payload'], payload_scale)
        duration = end - start
        y_pos = y_mapping[locality[comm.split('->')[0]]['NUMA ID'] if y_axis == 'NUMA' else locality[comm.split('->')[0]]['Core ID']] + offsets['write']
        ax.barh(y_pos, duration, left=start, color=get_color(comm, 'write'), label=f"{comm} Write ({payload} {payload_scale})")
    
    ax.set_yticks(np.arange(len(y_labels)) * 3 + 1)
    ax.set_yticklabels(y_labels)
    ax.set_xlabel(f'Time ({time_scale})')
    ax.set_ylabel(y_axis)
    ax.legend()
    plt.title("Gantt Chart for Task Execution with Read/Write Operations")
    plt.show()

# Example usage
yaml_data = """
name_to_thread_locality:
  Task2: {NUMA ID: 1, Core ID: 47, Voluntary CS: 9, Involuntary CS: 17, Core Migrations: 0}
  Task3: {NUMA ID: 0, Core ID: 0, Voluntary CS: 0, Involuntary CS: 39, Core Migrations: 0}
  Task1: {NUMA ID: 0, Core ID: 0, Voluntary CS: 0, Involuntary CS: 60, Core Migrations: 0}

comm_name_read_offsets:
  Task1->Task2: {Start: 9025204, End: 7848171, Payload: 13107200000}
  Task1->Task3: {Start: 9025204, End: 5917366, Payload: 13107200000}

comm_name_write_offsets:
  Task1->Task3: {Start: 2825749, End: 5928611, Payload: 13107200000}
  Task1->Task2: {Start: 2825749, End: 6199455, Payload: 13107200000}

exec_name_compute_offsets:
  Task2: {Start: 7848171, End: 2821519, Payload: 1000000000}
  Task3: {Start: 5917366, End: 2821967, Payload: 1000000000}
  Task1: {Start: 0, End: 2825749, Payload: 1000000000}
"""

plot_gantt(yaml_data, y_axis='NUMA', time_scale='ms', payload_scale='GB')
