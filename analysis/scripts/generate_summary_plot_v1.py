#!/usr/bin/env python3

import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import argparse


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a plot with aggregated data.")
    parser.add_argument("input_folder", type=str, help="Path to the folder containing CSV files")
    parser.add_argument("output_file", type=str, help="Path to the folder where results will be saved")
    parser.add_argument("--field_name", type=str, default="workflow_makespan_us", help="Time unit for scaling.")
    args = parser.parse_args()
    
    # Define the folder where the CSVs are stored
    data_folder = args.input_folder
    output_file = args.output_file

    # Define the field to plot
    field_name = args.field_name

    # Initialize a dictionary to store data
    data = {}

    # Read and process all CSV files
    for filename in os.listdir(data_folder):
        if filename.endswith(".csv"):
            # Extract category and size from filename
            parts = filename.split("_")
            category = "_".join(parts[1:-1])  # Extract base_line, one_node, two_nodes, etc.
            size = int(parts[-1].split(".")[0])  # Extract the numerical size

            # Read CSV as a pandas Series
            filepath = os.path.join(data_folder, filename)
            df = pd.read_csv(filepath, header=None).dropna()

            # Convert to dictionary for easy lookup
            df_dict = dict(zip(df[0], df[1]))

            # Extract mean and std for the chosen field
            mean_key = f"{field_name}_mean"
            std_key = f"{field_name}_std"

            if mean_key in df_dict and std_key in df_dict:
                mean_val = df_dict[mean_key]
                std_val = df_dict[std_key]

                # Store in a structured dictionary
                if size not in data:
                    data[size] = {}
                data[size][category] = {"mean": float(mean_val), "std": float(std_val)}

    # Extract sorted sizes and unique categories
    sizes = sorted(data.keys())
    categories = sorted({cat for size in data for cat in data[size]})

    # Prepare bar chart data
    means = []
    stds = []
    bar_labels = []

    for size in sizes:
        for category in categories:
            if category in data[size]:
                means.append(data[size][category]["mean"])
                stds.append(data[size][category]["std"])
                bar_labels.append(f"{category} ({size})")
            else:
                means.append(0)  # Placeholder for missing data
                stds.append(0)

    # Plot settings
    x = np.arange(len(bar_labels))  # X-axis positions
    width = 0.3  # Bar width

    # Define a larger color palette
    base_colors = [
        "blue", "orange", "green", "red", "purple", "brown", "pink", "gray", "olive", "cyan"
    ]  # At least 10 colors
    num_colors_needed = min(len(categories), len(base_colors))
    colors = base_colors[:num_colors_needed]  # Select only as many as needed

    # Expand the colors to match the number of bars, keeping categories consistent
    category_to_color = {cat: colors[i % num_colors_needed] for i, cat in enumerate(categories)}
    bar_colors = [category_to_color[label.split(" (")[0]] for label in bar_labels]

    fig, ax = plt.subplots(figsize=(10, 6))
    bars = ax.bar(x, means, yerr=stds, capsize=5, color=bar_colors, alpha=0.7)

    ax.set_xlabel("Configuration (Size)")
    ax.set_ylabel(f"{field_name} (Mean ± Std)")
    ax.set_title(f"Comparison of {field_name} across different configurations")
    ax.set_xticks(x)
    ax.set_xticklabels(bar_labels, rotation=45, ha="right")

    plt.grid(axis="y", linestyle="--", alpha=0.6)
    plt.tight_layout()
    # plt.show()
    plt.savefig(output_file, format=output_file.split('.')[-1])
    plt.close()
