#!/bin/bash

# Usage: ./generate_workflows.sh <input_json_folder> <output_folder>

# Check for required arguments
if [ $# -ne 2 ]; then
  echo "Usage: $0 <input_json_folder> <output_folder>"
  exit 1
fi

INPUT_FOLDER="$1"
OUTPUT_FOLDER="$2"

# Create the output directory if it doesn't exist
mkdir -p "$OUTPUT_FOLDER"

# Iterate over all .json files in the input directory
for json_file in "$INPUT_FOLDER"/*.json; do
  # Extract the base name (e.g., montage-chameleon-2mass-01d-001)
  filename=$(basename "$json_file" .json)

  # Define the output paths
  dot_C="$OUTPUT_FOLDER/C_${filename}.dot"
  dot_S="$OUTPUT_FOLDER/S_${filename}.dot"
  dot_L="$OUTPUT_FOLDER/L_${filename}.dot"

  # Generate the workflows
  python3 generate_dot.py "$json_file" "$dot_C" --dep_constant 40000000 --flops_constant 10000000
  python3 generate_dot.py "$json_file" "$dot_S" --dep_scale_range 4e7 5e7 --flops_constant 10000000
  python3 generate_dot.py "$json_file" "$dot_L" --dep_scale_range 4e7 1e8 --flops_constant 10000000
done

echo "Workflows generated in $OUTPUT_FOLDER"
