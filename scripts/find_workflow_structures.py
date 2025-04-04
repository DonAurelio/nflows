#!/usr/bin/env python3

"""
@authors: DeepSeek
@edited_by: Aurelio Vivas
@promt:
"""

import argparse
import pandas as pd
import networkx as nx
import matplotlib.pyplot as plt
from tabulate import tabulate

def find_data_distribution_nodes(digraph, min_fanout=2):
    """
    Find nodes that distribute data to multiple destinations (one-to-many).
    Returns a list of (source, [targets]) tuples.
    """
    distributors = []
    for node in digraph.nodes():
        successors = list(digraph.successors(node))
        if len(successors) >= min_fanout:
            distributors.append((node, successors))
    return distributors

def find_data_redistribution_nodes(digraph, min_degree=2):
    """
    Find data redistribution points with their origins and destinations.
    Returns a list of tuples: (redist_node, [origin_nodes], [destination_nodes])
    """
    redistributions = []
    
    for node in digraph.nodes():
        in_degree = digraph.in_degree(node)
        out_degree = digraph.out_degree(node)
        
        if in_degree >= min_degree and out_degree >= min_degree:
            origins = list(digraph.predecessors(node))
            destinations = list(digraph.successors(node))
            redistributions.append((node, origins, destinations))
    
    return redistributions

def find_data_aggregation_nodes(digraph, min_fanin=2):
    """
    Find nodes that aggregate data from multiple sources (many-to-one).
    Returns a list of (target, [sources]) tuples.
    """
    aggregators = []
    for node in digraph.nodes():
        predecessors = list(digraph.predecessors(node))
        if len(predecessors) >= min_fanin:
            aggregators.append((node, predecessors))
    return aggregators

def find_pipelines(digraph, min_length=3):
    """
    Find linear pipeline structures (chain of nodes with single input/output).
    Returns list of paths representing pipelines.
    """
    pipelines = []
    # Find potential start nodes (in_degree <= 1)
    start_nodes = [n for n in digraph.nodes() if digraph.in_degree(n) <= 1]
    
    for node in start_nodes:
        path = [node]
        current = node
        # Follow single-successor paths
        while digraph.out_degree(current) == 1:
            next_node = next(digraph.successors(current))
            if digraph.in_degree(next_node) == 1:
                path.append(next_node)
                current = next_node
            else:
                break
        if len(path) >= min_length:
            pipelines.append(path)
    
    return pipelines

def find_process_components(digraph, min_size=3):
    """
    Find strongly connected components that represent self-contained processes.
    Returns list of components (sets of nodes).
    """
    # Get all strongly connected components
    sccs = list(nx.strongly_connected_components(digraph))
    
    # Filter by minimum size and some process-like characteristics
    processes = []
    for component in sccs:
        if len(component) >= min_size:
            subgraph = digraph.subgraph(component)
            # Additional checks could be added here
            processes.append(component)
    
    return processes

def find_data_structures(digraph, patterns=None, **kwargs):
    """
    Find specified data processing microstructures in a graph.
    
    Parameters:
    -----------
    digraph : DiGraph
        The input directed graph
    patterns : list of str, optional
        List of patterns to find. If None, finds all patterns.
        Possible values: "distribution", "redistribution", "aggregation", "pipeline", "process"
    **kwargs : 
        Additional parameters to pass to individual pattern finders:
        - min_fanout: For distribution patterns (default: 2)
        - min_degree: For redistribution patterns (default: 2)
        - min_fanin: For aggregation patterns (default: 2)
        - min_length: For pipeline patterns (default: 3)
        - min_size: For process patterns (default: 3)
        
    Returns:
    --------
    dict
        Dictionary containing found patterns (only requested patterns are included)
    """
    # Define all available pattern finders
    pattern_finders = {
        "distribution": lambda: find_data_distribution_nodes(
            digraph, 
            min_fanout=kwargs.get('min_fanout', 2)
        ),
        "redistribution": lambda: find_data_redistribution_nodes(
            digraph, 
            min_degree=kwargs.get('min_degree', 2)
        ),
        "aggregation": lambda: find_data_aggregation_nodes(
            digraph, 
            min_fanin=kwargs.get('min_fanin', 2)
        ),
        "pipeline": lambda: find_pipelines(
            digraph, 
            min_length=kwargs.get('min_length', 3)
        ),
        "process": lambda: find_process_components(
            digraph, 
            min_size=kwargs.get('min_size', 3)
        )
    }
    
    # If no patterns specified, use all available patterns
    if patterns is None:
        patterns = pattern_finders.keys()
    
    # Find requested patterns
    results = {}
    for pattern in patterns:
        results[pattern] = pattern_finders[pattern]()

    return results

def remove_outgoing_edges(digraph, nodes):
    new_graph = digraph.copy()

    for node in nodes:
        # Get all successors (nodes with outgoing edges from this node)
        successors = list(new_graph.successors(node))
        
        # Remove all outgoing edges
        for successor in successors:
            new_graph.remove_edge(node, successor)
    
    return new_graph

def get_dataframe(pattern_results, pattern_type):
    """
    Create a DataFrame from pattern results with node, origin_count, and destination_count.
    
    Parameters:
    -----------
    pattern_results : list
        Results from one of the pattern finding functions
    pattern_type : str
        Type of pattern ('distribution', 'redistribution', 'aggregation', 'pipeline', 'process')
        
    Returns:
    --------
    pd.DataFrame
        DataFrame with columns: node, origin_count, destination_count
    """
    rows = []
    
    if pattern_type == 'distribution':
        for node, destinations in pattern_results:
            rows.append({
                'node': node,
                'in': 0,  # Distribution nodes have no origins in results
                'out': len(destinations)
            })
            
    elif pattern_type == 'redistribution':
        for node, origins, destinations in pattern_results:
            rows.append({
                'node': node,
                'in': len(origins),
                'out': len(destinations)
            })
            
    elif pattern_type == 'aggregation':
        for node, origins in pattern_results:
            rows.append({
                'node': node,
                'in': len(origins),
                'out': 0  # Aggregation nodes have no destinations in results
            })
            
    elif pattern_type == 'pipeline':
        for pipeline in pattern_results:
            # For pipelines, we'll count connections between nodes
            for i, node in enumerate(pipeline):
                origin_count = 1 if i > 0 else 0
                destination_count = 1 if i < len(pipeline)-1 else 0
                rows.append({
                    'node': node,
                    'in': origin_count,
                    'out': destination_count
                })
                
    elif pattern_type == 'process':
        for component in pattern_results:
            # For processes (SCCs), count connections within component
            subgraph = digraph.subgraph(component)
            for node in component:
                rows.append({
                    'node': node,
                    'in': subgraph.in_degree(node),
                    'out': subgraph.out_degree(node)
                })
                
    else:
        raise ValueError(f"Unknown pattern type: {pattern_type}")
    
    # Create DataFrame and fill any missing values with 0
    df = pd.DataFrame(rows)
    
    return df

def print_dataframe(data, title):
    print(f"\n{title.replace('_', ' ').title()}: {data.shape[0]} found")
    table_str = tabulate(data, headers=data.columns, tablefmt="grid", showindex=True)
    print("\n" + "\n".join(f"  {line}" for line in table_str.split("\n")))

def analyze_structures(graph, patterns):
    """Analyze the graph for specified patterns."""
    return find_data_structures(graph, patterns=patterns)

def display_structures(results, sort_by=None, sort_by_ascending=False):
    """Display analysis results in the console."""
    for pattern_type, pattern_results in results.items():
        if pattern_results:
            df = get_dataframe(pattern_results, pattern_type).sort_values(by=sort_by, ascending=sort_by_ascending).reset_index(drop=True)
            print_dataframe(df, title=pattern_type)

def save_structure(graph, results, pattern, node, output_file):
    """Save the specified pattern and node to an output file."""
    structures = results[pattern]
    selected_structure = None
    file_format = output_file.split('.')[-1]

    # Find the structure containing the specified node
    for structure in structures:
        if node in structure[0]:
            selected_structure = structure
            break
    else:
        raise ValueError(f"Node '{node}' not found in pattern '{pattern}'")
    
    nodes = [selected_structure[0], *selected_structure[1]]
    if len(selected_structure) > 2:
        nodes += selected_structure[2]

    # Create subgraph for the selected structure
    digraph = graph.subgraph(nodes)
    
    # Special handling for redistribution pattern
    if pattern == 'redistribution':
        digraph = remove_outgoing_edges(graph, selected_structure[2])
    
    # Save in appropriate format
    if file_format == 'dot':
        nx.write_dot(digraph, output_file)
    else:
        subgraph = digraph.subgraph(nodes)
        pos = nx.spring_layout(subgraph)
        nx.draw(subgraph, pos, with_labels=True, node_color='lightblue', node_size=800, arrowsize=20)
        plt.savefig(output_file, format=output_file.split('.')[-1])
        print(f"Figure created: '{output_file}'.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analyze graph microstructures.")
    parser.add_argument("input_dot_file", help="Path to input DOT file")
    parser.add_argument("--sort_by", default='node' , choices=['node', 'in', 'out'], help="Path to output file (DOT, PNG, PDF, etc.)")
    parser.add_argument("--sort_by_ascending", action="store_true", help="Sort values in ascending order (default: False).")
    parser.add_argument("--output_file", help="Path to output file (DOT, PNG, PDF, etc.)")
    parser.add_argument("--patterns", nargs='+', choices=['distribution', 'redistribution', 'aggregation', 'pipeline', 'process'], help="Patterns to analyze (default: all)")
    parser.add_argument("--pattern", choices=['distribution', 'redistribution', 'aggregation', 'pipeline', 'process'], help="Single pattern for output (required with --output_file)")
    parser.add_argument("--node", type=str, help="Specific node to visualize (required with --output_file)")

    args = parser.parse_args()

    if args.output_file and (not args.pattern or not args.node):
        parser.error("--output_file requires both --pattern and --node arguments")
    
    # Load and analyze the graph
    graph = nx.nx_pydot.read_dot(args.input_dot_file)
    results = analyze_structures(graph, [args.pattern] if args.pattern else args.patterns)

     # Handle output based on arguments
    if not args.output_file:
        display_structures(results, args.sort_by, args.sort_by_ascending)
    else:
        save_structure(graph, results, args.pattern, args.node, args.output_file)
