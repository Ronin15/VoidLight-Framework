#!/usr/bin/env python3
"""
Calculate Architecture Health Score for VoidLight-Framework
"""

import os
import json

def read_metrics():
    """Read all analysis results"""
    base_dir = 'test_results/dependency_analysis'

    metrics = {
        'circular_deps': 0,
        'layer_violations': 0,
        'tight_coupling': 0,
        'high_bloat_headers': 0,
        'high_depth_headers': 0,
        'total_headers': 0,
        'avg_coupling': 0.0,
        'max_depth': 0
    }

    # Read circular dependencies from detect_cycles.py output (measured, not assumed)
    circular_file = os.path.join(base_dir, 'circular_dependencies.txt')
    if os.path.exists(circular_file):
        with open(circular_file, 'r') as f:
            for line in f:
                if line.startswith('circular_dependencies='):
                    metrics['circular_deps'] = int(line.strip().split('=', 1)[1])
                    break

    # Read layer violations from detect_layer_violations.py output (measured)
    violations_file = os.path.join(base_dir, 'layer_violations.txt')
    if os.path.exists(violations_file):
        with open(violations_file, 'r') as f:
            metrics['layer_violations'] = sum(
                1 for line in f if line.startswith('File:')
            )

    # Read coupling metrics
    coupling_file = os.path.join(base_dir, 'coupling_metrics.txt')
    if os.path.exists(coupling_file):
        with open(coupling_file, 'r') as f:
            lines = f.readlines()[1:]  # Skip header
            metrics['total_headers'] = len(lines)

            fan_outs = []
            for line in lines:
                parts = line.strip().split(',')
                if len(parts) >= 4:
                    fan_out = int(parts[1])
                    fan_outs.append(fan_out)

            if fan_outs:
                metrics['avg_coupling'] = sum(fan_outs) / len(fan_outs)

    # Read coupling severity from analyze_coupling.py output (measured).
    # NOTE: For game engines, most manager coupling is FUNCTIONAL and necessary.
    # Only PROBLEMATIC coupling (circular, layer-violating, unclear purpose) is penalized.
    coupling_summary = os.path.join(base_dir, 'coupling_summary.txt')
    if os.path.exists(coupling_summary):
        with open(coupling_summary, 'r') as f:
            for line in f:
                if line.startswith('functional_coupling='):
                    metrics['functional_coupling'] = int(line.strip().split('=', 1)[1])
                elif line.startswith('problematic_coupling='):
                    metrics['problematic_coupling'] = int(line.strip().split('=', 1)[1])
    metrics['tight_coupling'] = (
        metrics.get('functional_coupling', 0) + metrics.get('problematic_coupling', 0)
    )

    # Count high-bloat headers (>10 includes) from analyze_header_bloat.py output (measured)
    bloat_file = os.path.join(base_dir, 'header_bloat_analysis.txt')
    if os.path.exists(bloat_file):
        with open(bloat_file, 'r') as f:
            in_section = False
            count = 0
            for line in f:
                stripped = line.strip()
                if stripped.startswith('High-Bloat Headers:'):
                    in_section = True
                    continue
                if in_section:
                    if stripped.startswith('- '):
                        count += 1
                    elif stripped == '' or ':' in stripped:
                        break
            metrics['high_bloat_headers'] = count

    # Read dependency depths
    depth_file = os.path.join(base_dir, 'dependency_depths.txt')
    if os.path.exists(depth_file):
        with open(depth_file, 'r') as f:
            lines = f.readlines()

            depths = []
            for line in lines:
                line = line.strip()
                if not line or line.startswith('=') or line.startswith('Dependency') or line.startswith('Component'):
                    continue

                parts = line.split(',')
                if len(parts) >= 2:
                    try:
                        depth = int(parts[1])
                        depths.append(depth)
                    except ValueError:
                        continue

            if depths:
                metrics['max_depth'] = max(depths)
                metrics['high_depth_headers'] = sum(1 for d in depths if d > 7)

    return metrics

def calculate_health_score(metrics):
    """Calculate overall architecture health score"""

    # Scoring categories (out of 10)
    scores = {}

    # 1. Circular Dependencies (30% weight)
    if metrics['circular_deps'] == 0:
        scores['circular_deps'] = 10.0
    elif metrics['circular_deps'] <= 2:
        scores['circular_deps'] = 5.0
    else:
        scores['circular_deps'] = 0.0

    # 2. Layer Compliance (25% weight)
    if metrics['layer_violations'] == 0:
        scores['layer_violations'] = 10.0
    elif metrics['layer_violations'] <= 3:
        scores['layer_violations'] = 6.0
    elif metrics['layer_violations'] <= 10:
        scores['layer_violations'] = 3.0
    else:
        scores['layer_violations'] = 0.0

    # 3. Coupling Strength (20% weight)
    # GAME ENGINE CONTEXT: Only count PROBLEMATIC coupling, not functional dependencies
    # Functional coupling (AI→Collision, UI→Font, etc.) is CORRECT and expected

    problematic = metrics.get('problematic_coupling', 0)

    if problematic == 0:
        scores['coupling'] = 10.0  # Perfect - no problematic coupling
    elif problematic <= 2:
        scores['coupling'] = 8.0   # Good - minimal issues
    elif problematic <= 5:
        scores['coupling'] = 5.0   # Fair - some concerns
    elif problematic <= 10:
        scores['coupling'] = 3.0   # Poor - many issues
    else:
        scores['coupling'] = 1.0   # Critical - severe coupling problems

    # 4. Header Bloat (15% weight)
    bloat_ratio = metrics['high_bloat_headers'] / metrics['total_headers'] if metrics['total_headers'] > 0 else 0

    if bloat_ratio < 0.05:
        scores['bloat'] = 10.0
    elif bloat_ratio < 0.10:
        scores['bloat'] = 8.0
    elif bloat_ratio < 0.15:
        scores['bloat'] = 6.0
    elif bloat_ratio < 0.25:
        scores['bloat'] = 4.0
    else:
        scores['bloat'] = 2.0

    # 5. Dependency Depth (10% weight)
    if metrics['max_depth'] <= 4:
        scores['depth'] = 10.0
    elif metrics['max_depth'] <= 7:
        scores['depth'] = 8.0
    elif metrics['max_depth'] <= 10:
        scores['depth'] = 6.0
    else:
        scores['depth'] = 4.0

    # Calculate weighted total
    weights = {
        'circular_deps': 0.30,
        'layer_violations': 0.25,
        'coupling': 0.20,
        'bloat': 0.15,
        'depth': 0.10
    }

    total_score = sum(scores[cat] * weights[cat] * 10 for cat in scores)

    return scores, total_score

def get_grade(score):
    """Convert score to letter grade"""
    if score >= 90:
        return 'A+', 'Excellent'
    elif score >= 80:
        return 'A', 'Good'
    elif score >= 70:
        return 'B', 'Fair'
    elif score >= 60:
        return 'C', 'Poor'
    else:
        return 'F', 'Critical'

def get_status(score):
    """Get overall status"""
    if score >= 80:
        return '✅ HEALTHY'
    elif score >= 60:
        return '⚠️  NEEDS ATTENTION'
    else:
        return '🔴 CRITICAL ISSUES'

def main():
    print("=== Architecture Health Scorecard ===")
    print()

    metrics = read_metrics()
    scores, total_score = calculate_health_score(metrics)
    grade, grade_desc = get_grade(total_score)
    status = get_status(total_score)

    print("| Category                  | Score | Weight | Weighted | Status |")
    print("|---------------------------|-------|--------|----------|--------|")

    categories = [
        ('Circular Dependencies', 'circular_deps', 0.30),
        ('Layer Compliance', 'layer_violations', 0.25),
        ('Coupling Strength', 'coupling', 0.20),
        ('Header Bloat', 'bloat', 0.15),
        ('Dependency Depth', 'depth', 0.10),
    ]

    for name, key, weight in categories:
        score = scores[key]
        weighted = score * weight * 10

        if score >= 8:
            status_icon = '✅'
        elif score >= 6:
            status_icon = '⚠️'
        else:
            status_icon = '🔴'

        print(f"| {name:25s} | {score:5.1f} | {weight*100:5.0f}% | {weighted:8.1f} | {status_icon}    |")

    print(f"| {'**TOTAL**':25s} |       | **100%** | **{total_score:6.1f}** | **{grade}**  |")

    print()
    print(f"**Architecture Health Score:** {total_score:.1f}/100 ({grade_desc})")
    print(f"**Status:** {status}")
    print()

    print("Grading Scale:")
    print("  90-100: A+ (Excellent architecture)")
    print("  80-89:  A  (Good architecture, minor issues)")
    print("  70-79:  B  (Fair architecture, needs improvement)")
    print("  60-69:  C  (Poor architecture, refactoring required)")
    print("  Below 60: F  (Critical issues, major refactoring needed)")
    print()

    # Detailed breakdown
    print("=== Detailed Breakdown ===")
    print()
    print(f"Circular Dependencies: {metrics['circular_deps']} found")
    print(f"Layer Violations: {metrics['layer_violations']} found")
    print()
    print("Manager Coupling:")
    print(f"  - Total coupling instances: {metrics['tight_coupling']}")
    print(f"  - Functional coupling (✅ correct): {metrics.get('functional_coupling', 0)}")
    print(f"  - Problematic coupling (🔴 review): {metrics.get('problematic_coupling', 0)}")
    print()
    print("Note: Functional coupling is EXPECTED in game engines - systems must interact!")
    print()
    print(f"High-Bloat Headers: {metrics['high_bloat_headers']} of {metrics['total_headers']} ({metrics['high_bloat_headers']/metrics['total_headers']*100:.1f}%)")
    print(f"Maximum Dependency Depth: {metrics['max_depth']}")
    print(f"Average Coupling: {metrics['avg_coupling']:.2f} dependencies per header")
    print()

    # Save scorecard
    output_file = 'test_results/dependency_analysis/health_scorecard.txt'
    with open(output_file, 'w') as f:
        f.write("Architecture Health Scorecard\n")
        f.write("=" * 60 + "\n\n")
        f.write(f"Overall Score: {total_score:.1f}/100\n")
        f.write(f"Grade: {grade} ({grade_desc})\n")
        f.write(f"Status: {status}\n\n")

        for name, key, weight in categories:
            f.write(f"{name}: {scores[key]:.1f}/10 (weight: {weight*100:.0f}%)\n")

    print(f"Health scorecard saved to: {output_file}")

    # Save as JSON for potential automation
    json_file = 'test_results/dependency_analysis/health_score.json'
    result = {
        'total_score': total_score,
        'grade': grade,
        'grade_description': grade_desc,
        'status': status,
        'category_scores': scores,
        'metrics': metrics
    }

    with open(json_file, 'w') as f:
        json.dump(result, f, indent=2)

    print(f"Health score JSON saved to: {json_file}")

if __name__ == '__main__':
    main()
