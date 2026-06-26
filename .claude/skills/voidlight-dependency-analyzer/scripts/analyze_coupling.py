#!/usr/bin/env python3
"""
Analyze coupling metrics for VoidLight-Framework
"""

import sys
import os
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

def calculate_coupling_metrics(graph, all_nodes):
    """Calculate fan-out, fan-in, and instability for each node"""
    metrics = {}

    # Calculate fan-out (efferent coupling)
    fan_out = defaultdict(int)
    for source, targets in graph.items():
        fan_out[source] = len(targets)

    # Calculate fan-in (afferent coupling)
    fan_in = defaultdict(int)
    for source, targets in graph.items():
        for target in targets:
            fan_in[target] += 1

    # Calculate metrics for all nodes
    for node in all_nodes:
        out = fan_out.get(node, 0)
        in_count = fan_in.get(node, 0)
        total = out + in_count

        instability = out / total if total > 0 else 0

        # Classify coupling strength
        if out > 15:
            out_status = "🔴 HIGH"
        elif out > 10:
            out_status = "⚠️  MEDIUM"
        elif out > 5:
            out_status = "🟡 MODERATE"
        else:
            out_status = "✅ LOW"

        # Classify stability
        if in_count > 20:
            in_status = "⭐ CORE"
        elif in_count > 10:
            in_status = "📦 STABLE"
        elif in_count > 5:
            in_status = "🔧 UTILITY"
        else:
            in_status = "📄 LEAF"

        metrics[node] = {
            'fan_out': out,
            'fan_in': in_count,
            'instability': instability,
            'out_status': out_status,
            'in_status': in_status
        }

    return metrics

def analyze_manager_coupling(graph, base_dir):
    """Analyze manager-to-manager coupling"""
    manager_dir = os.path.join(base_dir, 'include', 'managers')
    if not os.path.exists(manager_dir):
        return None, [], []

    # Define functional game engine dependencies (EXPECTED and CORRECT)
    functional_deps = {
        ('AIManager.hpp', 'CollisionManager.hpp'),
        ('AIManager.hpp', 'PathfinderManager.hpp'),
        ('CollisionManager.hpp', 'WorldManager.hpp'),
        ('CollisionManager.hpp', 'EventManager.hpp'),
        ('WorldManager.hpp', 'EventManager.hpp'),
        ('WorldManager.hpp', 'WorldResourceManager.hpp'),
        ('WorldManager.hpp', 'TextureManager.hpp'),
        ('UIManager.hpp', 'FontManager.hpp'),
        ('UIManager.hpp', 'UIConstants.hpp'),
        ('InputManager.hpp', 'UIManager.hpp'),
        ('InputManager.hpp', 'FontManager.hpp'),
        ('PathfinderManager.hpp', 'EventManager.hpp'),
        ('ParticleManager.hpp', 'EventManager.hpp'),
        ('ResourceFactory.hpp', 'ResourceTemplateManager.hpp'),
        ('ResourceTemplateManager.hpp', 'ResourceFactory.hpp'),
        ('WorldResourceManager.hpp', 'EventManager.hpp'),
        # EDM auto-registers/unregisters static entities (DroppedItem, Harvestable,
        # Container) with WRM's spatial index on create/destroy. Intentional design
        # — callers rely on this to avoid having to register separately. .cpp-only.
        ('EntityDataManager.hpp', 'WorldResourceManager.hpp'),
    }

    # Find all manager headers
    managers = []
    for file in os.listdir(manager_dir):
        if file.endswith('.hpp'):
            managers.append(file)

    managers.sort()

    # Build coupling matrix
    matrix = {}
    for mgr1 in managers:
        matrix[mgr1] = {}
        targets = graph.get(mgr1, [])
        for mgr2 in managers:
            matrix[mgr1][mgr2] = mgr2 in targets

    # Find tight coupling issues
    functional_coupling = []
    problematic_coupling = []

    for mgr1 in managers:
        mgr1_cpp = os.path.join(base_dir, 'src', 'managers', mgr1.replace('.hpp', '.cpp'))
        if os.path.exists(mgr1_cpp):
            try:
                with open(mgr1_cpp, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()

                for mgr2 in managers:
                    if mgr1 != mgr2:
                        mgr2_class = mgr2.replace('.hpp', '')
                        ref_count = content.count(mgr2_class)

                        if ref_count > 10:
                            # Check if this is a functional dependency
                            is_functional = (mgr1, mgr2) in functional_deps

                            if is_functional:
                                functional_coupling.append({
                                    'from': mgr1,
                                    'to': mgr2,
                                    'refs': ref_count,
                                    'severity': '✅ FUNCTIONAL'
                                })
                            else:
                                problematic_coupling.append({
                                    'from': mgr1,
                                    'to': mgr2,
                                    'refs': ref_count,
                                    'severity': '🔴 TIGHT'
                                })
                        elif ref_count > 5:
                            # Moderate coupling - just report
                            pass
            except Exception:
                pass

    return matrix, functional_coupling, problematic_coupling

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_coupling.py <graph_file> [base_dir]")
        sys.exit(1)

    graph_file = sys.argv[1]
    base_dir = sys.argv[2] if len(sys.argv) > 2 else '.'

    graph, all_nodes = read_graph(graph_file)
    metrics = calculate_coupling_metrics(graph, all_nodes)

    print("=== Coupling Analysis ===")
    print()

    # Top fan-out (efferent coupling)
    print("Fan-Out (Efferent Coupling - what this component depends on):")
    print()

    sorted_by_out = sorted(metrics.items(), key=lambda x: x[1]['fan_out'], reverse=True)
    for node, data in sorted_by_out[:20]:
        if data['fan_out'] > 0:
            print(f"  {node:40s} {data['fan_out']:3d} dependencies - {data['out_status']}")

    print()

    # Top fan-in (afferent coupling)
    print("Fan-In (Afferent Coupling - what depends on this component):")
    print()

    sorted_by_in = sorted(metrics.items(), key=lambda x: x[1]['fan_in'], reverse=True)
    for node, data in sorted_by_in[:20]:
        if data['fan_in'] > 0:
            print(f"  {node:40s} {data['fan_in']:3d} dependents - {data['in_status']}")

    print()

    # Instability metric
    print("Instability Metric (I = Efferent / (Afferent + Efferent)):")
    print("  0.0 = Maximally Stable (hard to change)")
    print("  1.0 = Maximally Unstable (easy to change)")
    print()

    # Show interesting cases (high coupling)
    high_coupling = [
        (node, data) for node, data in metrics.items()
        if data['fan_out'] + data['fan_in'] > 10
    ]

    for node, data in sorted(high_coupling, key=lambda x: x[1]['fan_out'] + x[1]['fan_in'], reverse=True)[:20]:
        print(f"  {node:40s} I={data['instability']:.2f} (out={data['fan_out']:2d}, in={data['fan_in']:2d})")

    print()

    # Manager-to-manager coupling
    print("=== Manager-to-Manager Coupling ===")
    print()

    matrix, functional_coupling, problematic_coupling = analyze_manager_coupling(graph, base_dir)

    if matrix:
        managers = sorted(matrix.keys())

        print("Manager Coupling Matrix:")
        print()

        # Header row
        print(f"{'Manager':30s}", end='')
        for mgr in managers:
            mgr_short = mgr.replace('.hpp', '')[:10]
            print(f"{mgr_short:12s}", end='')
        print()

        # Matrix rows
        for mgr1 in managers:
            mgr1_short = mgr1.replace('.hpp', '')
            print(f"{mgr1_short:30s}", end='')

            for mgr2 in managers:
                if mgr1 == mgr2:
                    print(f"{'-':12s}", end='')
                elif matrix[mgr1][mgr2]:
                    print(f"{'✓':12s}", end='')
                else:
                    print(f"{' ':12s}", end='')
            print()

        print()
        print("Legend: ✓ = Direct dependency, - = Self, (blank) = No dependency")
        print()
        print("Note: Manager coupling is EXPECTED in game engines - systems must interact!")
        print()

        # Coupling analysis
        print("Coupling Strength Analysis:")
        print()

        if functional_coupling:
            print("✅ Functional Coupling (Expected & Correct):")
            print()
            for coupling in functional_coupling:
                from_short = coupling['from'].replace('.hpp', '')
                to_short = coupling['to'].replace('.hpp', '')
                print(f"  {coupling['severity']} {from_short} -> {to_short} ({coupling['refs']} references)")
                print(f"      Reason: Game systems require interaction")
            print()

        if problematic_coupling:
            print("🔴 Problematic Coupling (Review Required):")
            print()
            for coupling in problematic_coupling:
                from_short = coupling['from'].replace('.hpp', '')
                to_short = coupling['to'].replace('.hpp', '')
                print(f"  {coupling['severity']} {from_short} -> {to_short} ({coupling['refs']} references)")
                print(f"      Action: Verify functional necessity or refactor")
            print()
        else:
            print("✅ No problematic coupling detected")
            print()

        print(f"Summary: {len(functional_coupling)} functional, {len(problematic_coupling)} problematic")
    else:
        print("No managers found in include/managers/")

    print()

    # Save metrics to file
    output_file = os.path.join(os.path.dirname(graph_file), 'coupling_metrics.txt')
    with open(output_file, 'w') as f:
        f.write("Component,Fan-Out,Fan-In,Instability\n")
        for node in sorted(all_nodes):
            data = metrics[node]
            f.write(f"{node},{data['fan_out']},{data['fan_in']},{data['instability']:.3f}\n")

    print(f"Coupling metrics saved to: {output_file}")

    # Persist functional/problematic counts so downstream tools
    # (calc_health_score.py) consume measured values, not hardcoded assumptions.
    summary_file = os.path.join(os.path.dirname(graph_file), 'coupling_summary.txt')
    with open(summary_file, 'w') as f:
        f.write(f"functional_coupling={len(functional_coupling)}\n")
        f.write(f"problematic_coupling={len(problematic_coupling)}\n")

if __name__ == '__main__':
    main()
