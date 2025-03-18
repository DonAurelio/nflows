"""
IA Genarated code:
I need a Python script that takes a workflow represented in JSON and parses it into a dot_file. These are the steps to be performed in the algorithm.
1. Parse the tasks field and create all the nodes in the graph using a networkx digraph. Use the property name of every task as a node identifier. Use the runtimeInSeconds property, and convert it into flops when considering a hypothetical core processing speed. Use this property as the size of the node.
2. Use the children's property in every task to create the dependencies. The size of the dependencies is calculated according to the file property in the parent and child task. If the parent and child share the same file name, the size will be the sum of the sizeInBytes property of the files they share.
3. Add a root node in the graph.
4. Add dependencies between the root node and the tasks whose parents' property is an empty list. The size of these dependencies is calculated from the file property of these tasks.  Sum the sizeInBytes of the input files and set this value as the dependency size. 
5. Add an end node in the graph.
6. Add dependencies between the leave nodes and the end node. Leave nodes are those whose children's properties are an empty list. The size of these dependencies is calculated from the file property of these tasks. Sum the sizeInBytes of the output files and set this value as the dependency size.

This is a sample of the JSON file 

    "tasks": [
        {
            "name": "mProject_00000001",
            "type": "compute",
            "command": {
                "program": "mProject",
                "arguments": []
            },
            "parents": [],
            "children": [
                "mDiffFit_00000005",
                "mDiffFit_00000006",
                "mDiffFit_00000007",
                "mBackground_00000013"
            ],
            "files": [
                {
                    "link": "output",
                    "name": "c9f8c3fe-ae8b-423b-9584-4cd376a934ec.fits",
                    "sizeInBytes": 36622657
                },
                {
                    "link": "input",
                    "name": "9bfd3dc8-c95b-421f-986f-d96272d3b417.fits",
                    "sizeInBytes": 13153745
                },
                {
                    "link": "input",
                    "name": "4f8fde8e-4a5b-47a4-98e3-52380a7cc796.hdr",
                    "sizeInBytes": 190
                }
            ],
            "runtimeInSeconds": 1389.829,
            "cores": 1,
            "id": "00000001",
            "category": "mProject",
            "startedAt": "2024-08-01T08:48:41.938613-05:00"
        },
        {
            "name": "mProject_00000002",
            "type": "compute",
            "command": {
                "program": "mProject",
                "arguments": []
            },
            "parents": [],
            "children": [
                "mDiffFit_00000005",
                "mDiffFit_00000008",
                "mDiffFit_00000009",
                "mBackground_00000014"
            ],
            "files": [
                {
                    "link": "output",
                    "name": "c2d9c925-5afc-4c6e-a6e2-2da6c95415e3.fits",
                    "sizeInBytes": 42993996
                },
                {
                    "link": "input",
                    "name": "4da312e9-a061-4e7c-a8e9-9eef872ccedb.fits",
                    "sizeInBytes": 7727699
                },
                {
                    "link": "input",
                    "name": "bd881837-1b67-4c81-8191-0c37329d133b.hdr",
                    "sizeInBytes": 306
                }
            ],
            "runtimeInSeconds": 1389.829,
            "cores": 1,
            "id": "00000002",
            "category": "mProject",
            "startedAt": "2024-08-01T08:48:41.938806-05:00"
        }
    ]
"""

import json
import networkx as nx
import pydot
from networkx.drawing.nx_pydot import write_dot

# Load JSON
with open('montage-workflow.json') as file:
    data = json.load(file)
    workflow = data['workflow']['tasks']

# Hypothetical core processing speed (in flops per second)
core_processing_speed = 1e9  # 1 GFLOPS

# Create a directed graph
G = nx.DiGraph()

# Add root node
G.add_node("root", size=0)

# Add tasks as nodes and edges
for task in data['workflow']['tasks']:
    task_name = task['name']
    task_runtime_flops = task['runtimeInSeconds'] * core_processing_speed
    G.add_node(task_name, size=task_runtime_flops)
    
    # Add dependencies (edges)
    for child in task['children']:
        # Calculate dependency size
        # TODO: Not sure if the calculation is correct. 
        dependency_size = sum(file['sizeInBytes'] for file in task['files'] if any(file['name'] == f['name'] for f in workflow[0]['files']))
        G.add_edge(task_name, child, size=dependency_size)
    
    # Add edges from root node to tasks with no parents
    if not task['parents']:
        input_size = sum(file['sizeInBytes'] for file in task['files'] if file['link'] == 'input')
        G.add_edge("root", task_name, size=input_size)

# Add end node
G.add_node("end", size=0)

# Add edges from leaf nodes to end node
for task in workflow:
    if not task['children']:
        output_size = sum(file['sizeInBytes'] for file in task['files'] if file['link'] == 'output')
        G.add_edge(task['name'], "end", size=output_size)

# Write to dot file
write_dot(G, "workflow.dot")