#!/usr/bin/env python3
"""
Detect circular dependencies in VoidLight-Framework using DFS
"""

import sys
import os
from collections import defaultdict

def read_graph(graph_file):
    """Read dependency graph from file"""
    graph = defaultdict(list)
    all_nodes = set()

    try:
        with open(graph_file, 'r') as f:
            for line in f:
                line = line.strip()
                if '->' in line:
                    parts = line.split(' -> ')
                    if len(parts) == 2:
                        source, target = parts[0].strip(), parts[1].strip()
                        graph[source].append(target)
                        all_nodes.add(source)
                        all_nodes.add(target)
    except Exception as e:
        print(f"Error reading graph: {e}")
        sys.exit(1)

    return graph, all_nodes

def find_cycles_dfs(graph):
    """Find all cycles using DFS with recursion stack"""
    visited = set()
    rec_stack = set()
    cycles = []
    seen_cycles = set()

    def dfs(node, path):
        if node in rec_stack:
            # Found a cycle
            cycle_start = path.index(node)
            cycle = path[cycle_start:] + [node]

            # Normalize cycle (start from lexicographically smallest)
            min_idx = cycle.index(min(cycle[:-1]))
            normalized = tuple(cycle[min_idx:-1] + cycle[:min_idx] + [cycle[min_idx]])

            if normalized not in seen_cycles:
                seen_cycles.add(normalized)
                cycles.append(cycle)
            return

        if node in visited:
            return

        visited.add(node)
        rec_stack.add(node)
        path.append(node)

        for neighbor in graph.get(node, []):
            dfs(neighbor, path[:])

        rec_stack.remove(node)

    # Start DFS from all nodes
    all_nodes = set(graph.keys())
    for neighbor_list in graph.values():
        all_nodes.update(neighbor_list)

    for node in sorted(all_nodes):
        if node not in visited:
            dfs(node, [])

    return cycles

def main():
    if len(sys.argv) < 2:
        print("Usage: detect_cycles.py <graph_file>")
        sys.exit(1)

    graph_file = sys.argv[1]
    graph, all_nodes = read_graph(graph_file)

    print(f"Analyzing {len(all_nodes)} nodes, {sum(len(v) for v in graph.values())} edges...")
    print()

    cycles = find_cycles_dfs(graph)

    # Persist the cycle count so downstream tools (calc_health_score.py) can
    # consume an actual measured value instead of a hardcoded assumption.
    summary_file = os.path.join(os.path.dirname(graph_file), 'circular_dependencies.txt')
    try:
        with open(summary_file, 'w') as f:
            f.write(f"circular_dependencies={len(cycles)}\n")
            for i, cycle in enumerate(cycles, 1):
                f.write(f"Cycle {i}: " + " -> ".join(cycle) + "\n")
    except Exception:
        pass

    if not cycles:
        print("✅ NO CIRCULAR DEPENDENCIES DETECTED")
        print()
        print("All include hierarchies are acyclic.")
        sys.exit(0)
    else:
        print(f"🔴 FOUND {len(cycles)} CIRCULAR DEPENDENCIES:")
        print()

        for i, cycle in enumerate(cycles, 1):
            print(f"Cycle {i}:")
            print("  " + " -> ".join(cycle))
            print()

        print(f"🔴 CIRCULAR DEPENDENCIES BLOCK COMPILATION")
        print("Action: Break cycles using forward declarations or interface extraction")
        sys.exit(1)

if __name__ == '__main__':
    main()
