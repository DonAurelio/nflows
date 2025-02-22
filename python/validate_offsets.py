#!/bin/python3

import yaml
import argparse

# Parse command line arguments
parser = argparse.ArgumentParser(description="Validate execution offsets from YAML file.")
parser.add_argument("yaml_path", type=str, help="Path to the YAML file")
args = parser.parse_args()

# Load data from YAML file
with open(args.yaml_path, "r") as file:
    data = yaml.safe_load(file)

comm_name_read_offsets = data["comm_name_read_offsets"]
comm_name_write_offsets = data["comm_name_write_offsets"]
exec_name_compute_offsets = data["exec_name_compute_offsets"]
exec_name_total_offsets = data["exec_name_total_offsets"]

# Identify root, intermediate, and end keys
right_hand_in_read = {key.split("->")[1] for key in comm_name_read_offsets.keys()}
left_hand_in_write = {key.split("->")[0] for key in comm_name_write_offsets.keys()}

# Validate each key
for key, total in exec_name_total_offsets.items():
    start, end = total["start"], total["end"]
    computed_sum = 0

    # Determine type of key
    if key not in right_hand_in_read:
        key_type = "root"
    elif key not in left_hand_in_write:
        key_type = "end"
    else:
        key_type = "intermediate"

    max_read_time = 0
    if key_type in ["intermediate", "end"]:
        for read_key, read in comm_name_read_offsets.items():
            _, right = read_key.split("->")
            if right == key:
                max_read_time = max(max_read_time, read["end"] - read["start"])

    compute = exec_name_compute_offsets[key]
    compute_time = (compute["end"] - compute["start"])

    # Compute the expected sum
    max_write_time = 0
    if key_type in ["root", "intermediate"]:
        for write_key, write in comm_name_write_offsets.items():
            left, _ = write_key.split("->")
            if left == key:
                max_write_time = max(max_write_time, write["end"] - write["start"])

    computed_sum += max_read_time + compute_time + max_write_time

    # Validate sum
    if start + computed_sum != end:
        print(f"Validation failed for {key}: expected {end}, got {start + computed_sum}")
    # else:
    #     print(f"Validation passed for {key}")
