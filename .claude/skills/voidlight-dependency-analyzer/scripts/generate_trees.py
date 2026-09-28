#!/usr/bin/env python3
"""
Generate dependency trees for key components
"""

import sys
from collections import defaultdict

def read_graph(graph_file):
    """Read dependency graph"""
    graph = defaultdict(list)

    with open(graph_file, 'r') as f:
        for line in f:
            line = line.strip()
            if '->' in line:
                parts = line.split(' -> ')
                if len(parts) == 2:
                    source, target = parts[0].strip(), parts[1].strip()
                    graph[source].append(target)

    return graph

def print_tree(graph, node, prefix="", visited=None, max_depth=3, current_depth=0):
    """Print dependency tree recursively"""
    if visited is None:
        visited = set()

    # Prevent infinite loops
    if node in visited:
        return [f"{prefix}└── {node} (circular)"]

    # Max depth limit
    if current_depth >= max_depth:
        return [f"{prefix}└── {node} (...)"]

    lines = [f"{prefix}└── {node}"]
    visited.add(node)

    # Get dependencies
    deps = graph.get(node, [])

    if deps:
        for i, dep in enumerate(deps):
            is_last = i == len(deps) - 1
            new_prefix = prefix + ("    " if is_last else "│   ")

            sub_lines = print_tree(graph, dep, new_prefix, visited.copy(), max_depth, current_depth + 1)
            lines.extend(sub_lines)

    return lines

def main():
    if len(sys.argv) < 2:
        print("Usage: generate_trees.py <graph_file>")
        sys.exit(1)

    graph_file = sys.argv[1]
    graph = read_graph(graph_file)

    print("=== Dependency Trees ===")
    print()

    # Key components to visualize
    key_components = [
        'GameEngine.hpp',
        'AIManager.hpp',
        'CollisionManager.hpp',
        'EventManager.hpp',
        'WorldManager.hpp'
    ]

    for component in key_components:
        if component in graph:
            print(f"{component} Dependency Tree:")
            print()

            tree_lines = print_tree(graph, component, "", set(), max_depth=3)
            for line in tree_lines:
                print(line)

            print()

    # Save trees to file
    output_file = 'test_results/dependency_analysis/dependency_trees.txt'
    with open(output_file, 'w') as f:
        f.write("Dependency Trees\n")
        f.write("=" * 60 + "\n\n")

        for component in key_components:
            if component in graph:
                f.write(f"{component} Dependency Tree:\n\n")

                tree_lines = print_tree(graph, component, "", set(), max_depth=4)
                for line in tree_lines:
                    f.write(line + "\n")

                f.write("\n")

    print(f"Dependency trees saved to: {output_file}")

if __name__ == '__main__':
    main()
