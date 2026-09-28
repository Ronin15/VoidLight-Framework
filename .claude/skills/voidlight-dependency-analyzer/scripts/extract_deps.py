#!/usr/bin/env python3
"""
Extract include dependencies from VoidLight-Framework headers
"""

import os
import re
from pathlib import Path
from collections import defaultdict

def find_headers(base_dir):
    """Find all .hpp headers"""
    headers = []
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith('.hpp'):
                headers.append(os.path.join(root, file))
    return sorted(headers)

def extract_includes(header_file):
    """Extract local includes from a header file"""
    includes = []
    try:
        with open(header_file, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                # Match #include "..." (local includes only)
                match = re.match(r'^\s*#include\s+"([^"]+)"', line)
                if match:
                    includes.append(match.group(1))
    except Exception as e:
        print(f"Warning: Could not read {header_file}: {e}")
    return includes

def build_dependency_graph(base_dirs):
    """Build dependency graph from headers"""
    graph = defaultdict(list)
    all_headers = []

    # Find all headers
    for base_dir in base_dirs:
        if os.path.exists(base_dir):
            all_headers.extend(find_headers(base_dir))

    print(f"Found {len(all_headers)} headers")

    # Extract dependencies
    for header in all_headers:
        header_name = os.path.basename(header)
        includes = extract_includes(header)

        for include in includes:
            include_name = os.path.basename(include)
            graph[header_name].append(include_name)

    return graph, all_headers

def save_graph(graph, output_file):
    """Save dependency graph to file"""
    with open(output_file, 'w') as f:
        for source, targets in sorted(graph.items()):
            for target in targets:
                f.write(f"{source} -> {target}\n")

def main():
    base_dirs = ['include', 'src']
    output_dir = 'test_results/dependency_analysis'

    os.makedirs(output_dir, exist_ok=True)

    print("Extracting dependencies...")
    graph, headers = build_dependency_graph(base_dirs)

    # Save graph
    graph_file = os.path.join(output_dir, 'dependency_graph.txt')
    save_graph(graph, graph_file)

    # Statistics
    total_edges = sum(len(targets) for targets in graph.values())
    files_with_deps = len([s for s in graph if graph[s]])

    print(f"\nDependency extraction complete:")
    print(f"  - Headers analyzed: {len(headers)}")
    print(f"  - Files with dependencies: {files_with_deps}")
    print(f"  - Total dependencies: {total_edges}")
    print(f"  - Graph saved to: {graph_file}")

if __name__ == '__main__':
    main()
