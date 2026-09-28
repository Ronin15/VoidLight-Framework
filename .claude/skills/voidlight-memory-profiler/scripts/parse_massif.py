#!/usr/bin/env python3
import os
import re
import glob

# Resolve the profiles directory dynamically. Honors OUTPUT_DIR (set by the
# companion shell scripts); otherwise defaults to test_results/memory_profiles
# under the current working directory (project root).
BASE_DIR = os.environ.get(
    "OUTPUT_DIR",
    os.path.join(os.getcwd(), "test_results", "memory_profiles"),
)

def parse_massif_report(report_path):
    """Extract peak memory data from massif report"""
    with open(report_path, 'r') as f:
        content = f.read()

    # Find peak snapshot number
    peak_match = re.search(r'(\d+)\s+\(peak\)', content)
    if not peak_match:
        return None

    peak_num = peak_match.group(1)

    # Find peak snapshot data line (now handles commas in time column too)
    pattern = rf'^\s*{peak_num}\s+([\d,]+)\s+([\d,]+)\s+([\d,]+)\s+([\d,]+)\s+\d+'
    for line in content.split('\n'):
        match = re.match(pattern, line)
        if match:
            time_ms = int(match.group(1).replace(',', ''))
            total_bytes = int(match.group(2).replace(',', ''))
            useful_bytes = int(match.group(3).replace(',', ''))
            overhead_bytes = int(match.group(4).replace(',', ''))

            return {
                'time_ms': time_ms,
                'total_bytes': total_bytes,
                'useful_bytes': useful_bytes,
                'overhead_bytes': overhead_bytes,
                'efficiency': (useful_bytes * 100 // total_bytes) if total_bytes > 0 else 0
            }

    return None

def main():
    reports = glob.glob(os.path.join(BASE_DIR, '*_massif_report.txt'))
    results = []

    for report_path in sorted(reports):
        test_name = os.path.basename(report_path).replace('_massif_report.txt', '')
        data = parse_massif_report(report_path)

        if data:
            results.append((test_name, data))
        else:
            print(f"⚠️  Could not parse: {test_name}")

    # Generate summary
    total_peak = sum(r[1]['total_bytes'] for r in results)

    print(f"\n=== VoidLight-Framework Comprehensive Memory Profile ===")
    print(f"Tests Analyzed: {len(results)}")
    print(f"Total Peak Memory: {total_peak / 1024 / 1024:.1f} MB\n")

    # Sort by peak memory
    results.sort(key=lambda x: x[1]['total_bytes'], reverse=True)

    print("Top 20 Memory Consumers:")
    print(f"{'Test':<45} {'Peak Memory':>15} {'Efficiency':>12}")
    print("=" * 75)

    for test_name, data in results[:20]:
        mb = data['total_bytes'] / 1024 / 1024
        if mb >= 1:
            size_str = f"{mb:.2f} MB"
        else:
            size_str = f"{data['total_bytes'] / 1024:.1f} KB"

        print(f"{test_name:<45} {size_str:>15} {data['efficiency']:>11}%")

    # Category breakdown
    print("\n\n=== By System Category ===\n")

    categories = {
        'AI': ['ai_', 'behavior_'],
        'Collision/Pathfinding': ['collision_', 'pathfind'],
        'Particle': ['particle_'],
        'World': ['world_'],
        'Resource': ['resource_'],
        'Threading': ['thread_'],
        'Event': ['event_'],
        'Save/Load': ['save_'],
        'UI': ['ui_', 'settings_', 'input_'],
        'Core': ['game_engine', 'game_state', 'buffer_', 'camera']
    }

    for category, prefixes in categories.items():
        cat_tests = [r for r in results if any(r[0].startswith(p) for p in prefixes)]
        if cat_tests:
            cat_peak = sum(t[1]['total_bytes'] for t in cat_tests)
            cat_mb = cat_peak / 1024 / 1024
            print(f"{category:<25} {len(cat_tests):>3} tests  {cat_mb:>8.2f} MB")

    # Save detailed CSV
    csv_path = os.path.join(BASE_DIR, 'memory_baseline.csv')
    with open(csv_path, 'w') as f:
        f.write("test_name,peak_mb,peak_bytes,useful_bytes,overhead_bytes,efficiency_pct,time_ms\n")
        for test_name, data in sorted(results):
            f.write(f"{test_name},"
                   f"{data['total_bytes']/1024/1024:.3f},"
                   f"{data['total_bytes']},"
                   f"{data['useful_bytes']},"
                   f"{data['overhead_bytes']},"
                   f"{data['efficiency']},"
                   f"{data['time_ms']}\n")

    print(f"\n✅ Detailed baseline saved to: {csv_path}")
    print(f"✅ All {len(results)} tests successfully parsed")

if __name__ == '__main__':
    main()
