#!/usr/bin/env python3
"""
Calculate dependency depth for compile time impact analysis
"""

import sys
from collections import defaultdict

def read_graph(graph_file):
    """Read dependency graph"""
    graph = defaultdict(list)
    all_nodes = set()

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

    return graph, all_nodes

def calculate_depth(graph, node, memo=None, visiting=None):
    """Calculate max dependency depth using DFS with memoization"""
    if memo is None:
        memo = {}
    if visiting is None:
        visiting = set()

    if node in memo:
        return memo[node]

    # Detect cycles
    if node in visiting:
        return 0

    if node not in graph or not graph[node]:
        memo[node] = 0
        return 0

    visiting.add(node)
    max_depth = 0

    for neighbor in graph[node]:
        depth = calculate_depth(graph, neighbor, memo, visiting)
        max_depth = max(max_depth, depth + 1)

    visiting.remove(node)
    memo[node] = max_depth
    return max_depth

def main():
    if len(sys.argv) < 2:
        print("Usage: calc_depth.py <graph_file>")
        sys.exit(1)

    graph_file = sys.argv[1]
    graph, all_nodes = read_graph(graph_file)

    print("=== Dependency Depth Analysis ===")
    print()
    print("Calculating dependency depth for all nodes...")
    print("(Higher depth = more cascading recompilation)")
    print()

    # Calculate depth for all nodes
    depths = {}
    for node in all_nodes:
        depths[node] = calculate_depth(graph, node)

    # Sort by depth
    sorted_depths = sorted(depths.items(), key=lambda x: x[1], reverse=True)

    print("Top 20 Headers by Dependency Depth:")
    print()

    for node, depth in sorted_depths[:20]:
        if depth > 10:
            status = "🔴 VERY HIGH"
        elif depth > 7:
            status = "⚠️  HIGH"
        elif depth > 4:
            status = "🟡 MODERATE"
        else:
            status = "✅ LOW"

        print(f"  {node:45s} depth={depth:2d} {status}")

    print()
    print("Note: High depth = changing this header causes cascading recompilation")
    print()

    # Calculate statistics
    total_depth = sum(depths.values())
    avg_depth = total_depth / len(depths) if depths else 0
    max_depth = max(depths.values()) if depths else 0

    print("=== Compile Time Impact Estimation ===")
    print()
    print(f"  Total dependency depth: {total_depth}")
    print(f"  Average depth per header: {avg_depth:.1f}")
    print(f"  Maximum depth: {max_depth}")
    print()

    # Classify headers by depth
    very_high = sum(1 for d in depths.values() if d > 10)
    high = sum(1 for d in depths.values() if 7 < d <= 10)
    moderate = sum(1 for d in depths.values() if 4 < d <= 7)
    low = sum(1 for d in depths.values() if d <= 4)

    print("Distribution:")
    print(f"  🔴 Very High (>10): {very_high} headers")
    print(f"  ⚠️  High (8-10): {high} headers")
    print(f"  🟡 Moderate (5-7): {moderate} headers")
    print(f"  ✅ Low (0-4): {low} headers")
    print()

    # Save detailed results
    output_file = 'test_results/dependency_analysis/dependency_depths.txt'
    with open(output_file, 'w') as f:
        f.write("Dependency Depth Analysis\n")
        f.write("=" * 60 + "\n\n")

        f.write("Component,Depth,Status\n")
        for node, depth in sorted_depths:
            if depth > 10:
                status = "VERY_HIGH"
            elif depth > 7:
                status = "HIGH"
            elif depth > 4:
                status = "MODERATE"
            else:
                status = "LOW"

            f.write(f"{node},{depth},{status}\n")

    print(f"Dependency depths saved to: {output_file}")

if __name__ == '__main__':
    main()
