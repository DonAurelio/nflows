#!/usr/bin/env python3

import os
import pandas as pd
import numpy as np
import argparse


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generates sample data for 'generate_aggreg_plot'.")
    parser.add_argument("output_folder", type=str, help="Path to the folder where files will be saved.")
    args = parser.parse_args()

    # Define configurations and sizes
    categories = ["base_line", "one_node", "two_nodes"]
    sizes = [4, 8, 16]
    fields = [
        "workflow_makespan_us",
        "total_compute_time_us",
        "total_read_time_us",
        "total_write_time_us",
        "comp_to_comm_ratio",
        "comm_to_comp_ratio"
    ]

    os.makedirs(args.output_folder, exist_ok=True)

    # Generate CSV files
    for category in categories:
        for size in sizes:
            filename = f"aggreg_{category}_{size}.csv"
            filepath = os.path.join(args.output_folder, filename)
            
            data = {}
            for field in fields:
                mean_value = np.random.uniform(1e6, 1e8)  # Random mean value
                std_value = np.random.uniform(1e5, 1e7)  # Random standard deviation
                variance_value = std_value ** 2  # Variance = std^2
                
                data[f"{field}_mean"] = mean_value
                data[f"{field}_std"] = std_value
                data[f"{field}_variance"] = variance_value
            
            # Convert to DataFrame and save
            df = pd.DataFrame(list(data.items()))
            df.to_csv(filepath, index=False, header=False)

    print(f"Sample CSVs generated in '{args.output_folder}' directory.")
