import json
import argparse
import os

def generate_json_file(template_file, output_file, **params):
    with open(template_file, 'r') as f:
        template = json.load(f)
    
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    template.update(params)
    
    with open(output_file, 'w') as f:
        json.dump(template, f, indent=4)
    print(f"Generated: {output_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a JSON configuration from a template and parameter variations.")
    parser.add_argument("--template", required=True, help="Path to the JSON template file.")
    parser.add_argument("--output_file", required=True, help="Full path for the output JSON file, without extension.")
    parser.add_argument("--params", nargs='+', required=True, help="Named parameters in key=value format.")
    
    args = parser.parse_args()
    
    params_dict = {}
    for param in args.params:
        key, value = param.split("=")
        params_dict[key] = value
    
    generate_json_file(args.template, args.output_file, **params_dict)
