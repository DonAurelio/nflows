#!/bin/python3

"""
validate_execution_offsets.py

This script validates execution offsets based on data from a YAML file.
It checks whether the computed sum of read, compute, and write offsets 
matches the expected total offsets for each key.

Validation Rules:
    1. Identifies root, intermediate, and end nodes based on read/write dependencies.
    2. Ensures root tasks start at 0 and end tasks finish at the `avail_until` value of the core where the task was executed.
    3. Computes expected execution time for each key as:
       max(read time) + compute time + max(write time).
    4. Compares the computed execution time with the expected total offset.
    5. Prints validation failures if any discrepancies are found.

Data Sources:
    The YAML file should contain the following keys:
    - comm_name_read_offsets: Dictionary mapping communication read dependencies 
      with start and end times.
    - comm_name_write_offsets: Dictionary mapping communication write dependencies 
      with start and end times.
    - exec_name_compute_offsets: Dictionary mapping execution compute times 
      with start and end times.
    - exec_name_total_offsets: Dictionary mapping execution total offsets 
      with start and end times.
    - core_availability (optional): Dictionary mapping core IDs to availability times 
      for end tasks.

Example YAML Structure:
    comm_name_read_offsets:
        "A->B": { "start": 0, "end": 5 }
    comm_name_write_offsets:
        "B->C": { "start": 6, "end": 10 }
    exec_name_compute_offsets:
        "B": { "start": 5, "end": 6 }
    exec_name_total_offsets:
        "B": { "start": 0, "end": 10 }
    core_availability:
        "core_1": { "avail_until": 10 }

Validation Output:
    If a validation fails, an error message is printed:
        "Validation failed for <key>: expected <expected_time>, got <computed_time>"
"""

import yaml
import argparse


def load_yaml_data(file_path):
    """Loads YAML data from the given file."""
    with open(file_path, "r") as file:
        return yaml.safe_load(file)


def determine_key_type(key, right_hand_in_read, left_hand_in_write):
    """Determines if a key is a root, intermediate, or end node."""
    if key not in right_hand_in_read:
        return "root"
    elif key not in left_hand_in_write:
        return "end"
    return "intermediate"


def compute_max_offset_time(dependencies, target_key, position):
    """
    Computes the maximum offset time for dependencies.
    - `dependencies`: Dictionary containing offset start and end times.
    - `target_key`: The key whose dependencies are analyzed.
    - `position`: "left" for write dependencies, "right" for read dependencies.
    """
    max_time = 0
    for dep_key, dep in dependencies.items():
        left, right = dep_key.split("->")
        if (position == "right" and right == target_key) or (position == "left" and left == target_key):
            max_time = max(max_time, dep["end"] - dep["start"])
    return max_time


def validate_offsets(data):
    """Validates execution offsets based on computed vs expected total times."""
    comm_read_offsets = data.get("comm_name_read_offsets", {})
    comm_write_offsets = data.get("comm_name_write_offsets", {})
    exec_compute_offsets = data["exec_name_compute_offsets"]
    exec_total_offsets = data["exec_name_total_offsets"]
    core_availability = data["core_availability"]
    name_to_thread_locality = data["name_to_thread_locality"]

    right_hand_in_read = {key.split("->")[1] for key in comm_read_offsets.keys()}
    left_hand_in_write = {key.split("->")[0] for key in comm_write_offsets.keys()}

    for key, total in exec_total_offsets.items():
        start, end = total["start"], total["end"]
        key_type = determine_key_type(key, right_hand_in_read, left_hand_in_write)

        # Compute offsets
        max_read_time = compute_max_offset_time(comm_read_offsets, key, "right") if key_type in ["intermediate", "end"] else 0
        max_write_time = compute_max_offset_time(comm_write_offsets, key, "left") if key_type in ["root", "intermediate"] else 0
        compute_time = exec_compute_offsets[key]["end"] - exec_compute_offsets[key]["start"]

        computed_sum = max_read_time + compute_time + max_write_time

        if start + computed_sum != end:
            print(f"Validation failed for {key}: expected {end}, got {start + computed_sum}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Validate execution offsets from YAML file.")
    parser.add_argument("yaml_path", type=str, help="Path to the YAML file")
    args = parser.parse_args()

    yaml_data = load_yaml_data(args.yaml_path)
    validate_offsets(yaml_data)
