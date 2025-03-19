#!/usr/bin/env python3

"""
@authors: ChatGPT
@edited_by: Aurelio Vivas
@promt:

I need a Python script that takes a workflow represented in JSON and parses it into a dot_file. These are the steps to be performed in the algorithm.

The tasks are defined in workflow.specification.tasks.
The files are defined in workflow.specification.files.

1. Parse the tasks field and create all the nodes in the graph using a networkx digraph. 
2. Use the property id of every task as a node identifier.
3. Look at workflow.execution.tasks and use the runtimeInSeconds property to calculate the size of the node. The size of the node is calculated by converting the runtimeInSeconds property into flops when considering a hypothetical core processing speed. Use this value as the size property of the node.
4. Use the children's property in every task to create the dependencies. The size of the dependencies is calculated according to the file property in the parent and child task. If the parent and child share the same file name, the size will be the sum of the sizeInBytes property of the files they share.
5. Add a root node in the graph.
6. Add dependencies between the root node and the tasks whose parents' property is an empty list. The size of these dependencies is calculated from the file property of these tasks. Sum the sizeInBytes of the input files and set this value as the dependency size.
7. Add an end node in the graph.
8. Add dependencies between the leave nodes and the end node. Leave nodes are those whose children's properties are an empty list. The size of these dependencies is calculated from the file property of these tasks. Sum the sizeInBytes of the output files and set this value as the dependency size.

This is an example of a json file
"""

import json
import networkx as nx
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a DOT file from a JSON in wfformat_v1.5.")
    parser.add_argument("input_file", help="Path to the JSON file in wfformat_v1.5.")
    parser.add_argument("output_file", help="Path to the DOT output file.")
    parser.add_argument("--cpu_speed", default=1e9, help="CPU speed in FLOPS (default: 1e9).")

    args = parser.parse_args()

    # Load the JSON data
    with open(args.input_file) as f:
        data = json.load(f)

    # Extract tasks and files
    tasks = data['workflow']['specification']['tasks']
    files = data['workflow']['specification']['files']
    execution_tasks = data['workflow']['execution']['tasks']

    # Create a directed graph
    G = nx.DiGraph()

    # Create a dictionary to map file IDs to their sizes
    file_sizes = {file['id']: file['sizeInBytes'] for file in files}

    # Create a dictionary to map task IDs to their runtime in seconds
    task_runtimes = {task['id']: task['runtimeInSeconds'] for task in execution_tasks}

    # Add nodes to the graph
    for task in tasks:
        task_id = task['id']
        # Calculate the size of the node based on runtimeInSeconds
        # Assuming a hypothetical core processing speed of 1e9 FLOPS (1 GFLOPS)
        flops = int(task_runtimes[task_id] * float(args.cpu_speed))
        G.add_node(task_id, size=flops)

    # Add edges to the graph based on children and parents
    for task in tasks:
        task_id = task['id']
        children = task['children']
        parents = task['parents']
        
        # Add edges from parents to the current task
        for parent in parents:
            # Calculate the size of the dependency based on shared files
            shared_files = set(task['inputFiles']).intersection(set(next(t for t in tasks if t['id'] == parent)['outputFiles']))
            dependency_size = sum(file_sizes[file] for file in shared_files)
            G.add_edge(parent, task_id, size=dependency_size)
        
        # Add edges from the current task to its children
        for child in children:
            # Calculate the size of the dependency based on shared files
            shared_files = set(task['outputFiles']).intersection(set(next(t for t in tasks if t['id'] == child)['inputFiles']))
            dependency_size = sum(file_sizes[file] for file in shared_files)
            G.add_edge(task_id, child, size=dependency_size)

    # Add a root node and connect it to tasks with no parents
    root_node = 'root'
    G.add_node(root_node, size=2)
    for task in tasks:
        if not task['parents']:
            # Calculate the size of the dependency based on input files
            dependency_size = sum(file_sizes[file] for file in task['inputFiles'])
            G.add_edge(root_node, task['id'], size=dependency_size)

    # Add an end node and connect it to tasks with no children
    end_node = 'end'
    G.add_node(end_node, size=2)
    for task in tasks:
        if not task['children']:
            # Calculate the size of the dependency based on output files
            dependency_size = sum(file_sizes[file] for file in task['outputFiles'])
            G.add_edge(task['id'], end_node, size=dependency_size)

    # Write the graph to a DOT file
    nx.drawing.nx_pydot.write_dot(G, args.output_file)

    print(f"DOT file created: '{args.output_file}'.")
