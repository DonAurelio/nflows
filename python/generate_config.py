#!/bin/python3

import json
import itertools
import argparse
import os

def generate_combinations(params):
    keys = params.keys()
    values = (params[key] for key in keys)
    return [dict(zip(keys, combination)) for combination in itertools.product(*values)]

def generate_json_files(template_file, output_dir, **params):
    with open(template_file, 'r') as f:
        template = json.load(f)
    
    os.makedirs(output_dir, exist_ok=True)
    combinations = generate_combinations(params)

    for i, combination in enumerate(combinations):
        modified_template = template.copy()
        modified_template.update(combination)
        #combination_name = "_".join(f"{k}_{v}" for k, v in combination.items()).lower()
        output_path = os.path.join(output_dir, f"config_{i}.json")
        with open(output_path, 'w') as f:
            json.dump(modified_template, f, indent=4)
        print(f"Generated: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate JSON configurations from a template and parameter variations.")
    parser.add_argument("--template", required=True, help="Path to the JSON template file.")
    parser.add_argument("--output_dir", required=True, help="Directory to save generated JSON files.")
    parser.add_argument("--params", nargs='+', required=True, help="Named parameters in key=value1,value2,... format.")
    
    args = parser.parse_args()
    
    params_dict = {}
    for param in args.params:
        key, values = param.split("=")
        params_dict[key] = [v.strip() for v in values.split(",")]
    
    generate_json_files(args.template, args.output_dir, **params_dict)
