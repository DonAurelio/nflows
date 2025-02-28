#!/bin/python3

import yaml
import sys

def validate_yaml(output_path, expected_path, check_order=False):
    with open(output_path, 'r') as output_file, open(expected_path, 'r') as expected_file:
        output_data = yaml.safe_load(output_file)
        expected_data = yaml.safe_load(expected_file)
    
    def compare(expected, actual, path="", depth=0):
        if isinstance(expected, dict):
            expected_keys = list(expected.keys())
            actual_keys = list(actual.keys()) if isinstance(actual, dict) else []
            
            if check_order and depth == 1 and expected_keys != actual_keys[:len(expected_keys)]:
                print(f"Key order mismatch at {path}: expected {expected_keys}, got {actual_keys[:len(expected_keys)]}")
                return False

            for key, value in expected.items():
                new_path = f"{path}.{key}" if path else key
                if key not in actual:
                    print(f"Missing key: {new_path}")
                    return False
                if not compare(value, actual[key], new_path, depth + 1):
                    return False
        elif isinstance(expected, list):
            if not isinstance(actual, list) or len(expected) != len(actual):
                print(f"Mismatch in list length at {path}")
                return False
            for i, (exp_item, act_item) in enumerate(zip(expected, actual)):
                if not compare(exp_item, act_item, f"{path}[{i}]", depth + 1):
                    return False
        else:
            if expected != actual:
                print(f"Mismatch at {path}: expected {expected}, got {actual}")
                return False
        return True
    
    return compare(expected_data, output_data)

if __name__ == "__main__":
    if len(sys.argv) not in [3, 4]:
        print("Usage: python validate_yaml.py <output_yaml> <expected_yaml> [--check-order]")
        sys.exit(1)
    
    output_file = sys.argv[1]
    expected_file = sys.argv[2]
    check_order = len(sys.argv) == 4 and sys.argv[3] == "--check-order"
    
    if validate_yaml(output_file, expected_file, check_order):
        print("Validation successful.")
    else:
        print("Validation failed.")
