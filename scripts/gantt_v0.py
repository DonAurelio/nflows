import yaml
import matplotlib.pyplot as plt
import numpy as np

def scale_value(value, scale):
    scales = {'us': 1, 'ms': 1e-3, 's': 1e-6, 'min': 1e-6 / 60}
    return value * scales.get(scale, 1)

def scale_payload(value, scale):
    scales = {'B': 1, 'KB': 1e-3, 'MB': 1e-6, 'GB': 1e-9}
    return value * scales.get(scale, 1)

def plot_gantt(yaml_data, y_axis='NUMA', time_scale='us', payload_scale='B'):
    data = yaml.safe_load(yaml_data)
    tasks = data['exec_name_total_offsets']
    locality = data['name_to_thread_locality']
    
    y_mapping = {}
    y_labels = []
    
    for task, loc in locality.items():
        key = loc['NUMA ID'] if y_axis == 'NUMA' else loc['Core ID']
        if key not in y_mapping:
            y_mapping[key] = len(y_mapping)
            y_labels.append(key)
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for task, times in tasks.items():
        start = scale_value(times['Start'], time_scale)
        end = scale_value(times['End'], time_scale)
        payload = scale_payload(times['Payload'], payload_scale)
        duration = end - start
        y_pos = y_mapping[locality[task]['NUMA ID'] if y_axis == 'NUMA' else locality[task]['Core ID']]
        ax.barh(y_pos, duration, left=start, label=f"{task} ({payload} {payload_scale})")
    
    ax.set_yticks(np.arange(len(y_labels)))
    ax.set_yticklabels(y_labels)
    ax.set_xlabel(f'Time ({time_scale})')
    ax.set_ylabel(y_axis)
    ax.legend()
    plt.title("Gantt Chart for Task Execution")
    plt.show()

# Example usage
yaml_data = """
exec_name_total_offsets:
  Task2: {Start: 9025204, End: 19694894, Payload: 1000000000}
  Task3: {Start: 9025204, End: 17764537, Payload: 1000000000}
  Task1: {Start: 0, End: 9025204, Payload: 1000000000}

name_to_thread_locality:
  Task2: {NUMA ID: 1, Core ID: 47, Voluntary CS: 9, Involuntary CS: 17, Core Migrations: 0}
  Task3: {NUMA ID: 0, Core ID: 0, Voluntary CS: 0, Involuntary CS: 39, Core Migrations: 0}
  Task1: {NUMA ID: 0, Core ID: 0, Voluntary CS: 0, Involuntary CS: 60, Core Migrations: 0}
"""

plot_gantt(yaml_data, y_axis='NUMA', time_scale='ms', payload_scale='GB')