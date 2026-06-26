#!/usr/bin/env python3
"""
Detect architectural layer violations in VoidLight-Framework
"""

import os
import re

def classify_layer(file_path):
    """Classify a file into architectural layers"""
    path_lower = file_path.lower()

    if '/core/' in path_lower:
        return 'Core'
    elif '/managers/' in path_lower:
        return 'Managers'
    elif '/controllers/' in path_lower:
        return 'Controllers'
    elif '/gamestates/' in path_lower or '/states/' in path_lower:
        return 'States'
    elif '/entities/' in path_lower:
        return 'Entities'
    elif '/utils/' in path_lower:
        return 'Utils'
    elif '/ai/' in path_lower:
        return 'AI'
    elif '/events/' in path_lower:
        return 'Events'
    elif '/collisions/' in path_lower:
        return 'Collisions'
    elif '/world/' in path_lower:
        return 'World'
    elif '/gpu/' in path_lower:
        return 'GPU'
    else:
        return 'Other'

def find_headers_by_layer(base_dir):
    """Find all headers organized by layer"""
    layers = {
        'Core': [],
        'Managers': [],
        'Controllers': [],
        'States': [],
        'Entities': [],
        'Utils': [],
        'AI': [],
        'Events': [],
        'Collisions': [],
        'World': [],
        'GPU': [],
        'Other': []
    }

    for root, _, files in os.walk(os.path.join(base_dir, 'include')):
        for file in files:
            if file.endswith('.hpp'):
                full_path = os.path.join(root, file)
                layer = classify_layer(full_path)
                layers[layer].append(full_path)

    return layers

def extract_includes(file_path):
    """Extract all local includes from a file"""
    includes = []
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                match = re.match(r'^\s*#include\s+"([^"]+)"', line)
                if match:
                    includes.append(match.group(1))
    except Exception:
        pass
    return includes


# Known-good exceptions: intentional patterns reviewed and approved by the team.
# Add entries here to permanently silence a specific include from being flagged.
# Format: (including_file_basename, included_file_basename)
APPROVED_EXCEPTIONS = {
    # Logger is a foundational utility used at all layers — not a layer violation.
    ('BinarySerializer.hpp', 'Logger.hpp'),
}


def check_layer_violations(base_dir):
    """Check for architectural layer violations"""
    layers = find_headers_by_layer(base_dir)
    violations = []

    # Define allowed dependencies for each layer
    layer_rules = {
        'Core': [],  # Core should not depend on anything
        'Managers': ['Core', 'Utils', 'Events', 'AI', 'Entities', 'Collisions', 'World', 'GPU'],  # Managers can use most things except States/Controllers
        'Controllers': ['Core', 'Utils', 'Managers', 'Events', 'AI', 'Entities', 'Collisions', 'World', 'GPU'],  # State-scoped; uses managers/entities but not States
        'States': ['Core', 'Managers', 'Controllers', 'Utils', 'Entities', 'Events', 'AI', 'Collisions', 'World', 'GPU'],  # States can use everything except other States
        'Entities': ['Core', 'Utils', 'Events'],  # Entities should be minimal
        'Utils': [],  # Utils should be pure
        'AI': ['Core', 'Utils', 'Events'],  # AI behaviors should be reusable
        'Events': ['Core', 'Utils'],  # Events should be minimal
        'Collisions': ['Core', 'Utils'],  # Collision primitives should be minimal
        'World': ['Core', 'Utils', 'Events'],  # World data structures
        'GPU': ['Core', 'Utils', 'Events'],  # GPU rendering primitives
    }

    print("=== Layer Violation Detection ===")
    print()

    for layer_name, headers in layers.items():
        if not headers or layer_name == 'Other':
            continue

        allowed = layer_rules.get(layer_name, [])

        print(f"{layer_name} Layer ({len(headers)} files):")
        print(f"  Allowed dependencies: {', '.join(allowed) if allowed else 'None (pure/foundation layer)'}")

        layer_violations = []

        for header in headers:
            includes = extract_includes(header)

            for include in includes:
                # Determine layer of the included file
                include_layer = classify_layer(include)

                # Check for violations
                violates = False
                reason = ""

                if include_layer == layer_name:
                    # Same layer - check for cross-state dependencies
                    if layer_name == 'States':
                        if os.path.basename(header) != os.path.basename(include):
                            violates = True
                            reason = "Cross-state dependency (states should not depend on each other)"
                elif include_layer not in allowed and include_layer != 'Other':
                    violates = True
                    reason = f"Depends on {include_layer} layer (not allowed)"

                if violates:
                    key = (os.path.basename(header), os.path.basename(include))
                    if key in APPROVED_EXCEPTIONS:
                        continue  # Intentional pattern, skip
                    violation = {
                        'file': os.path.basename(header),
                        'includes': os.path.basename(include),
                        'include_layer': include_layer,
                        'reason': reason
                    }
                    layer_violations.append(violation)
                    violations.append(violation)

        if layer_violations:
            print(f"  🔴 {len(layer_violations)} violation(s) found:")
            for v in layer_violations[:5]:  # Limit output
                print(f"    - {v['file']} includes {v['includes']}")
                print(f"      {v['reason']}")
            if len(layer_violations) > 5:
                print(f"    ... and {len(layer_violations) - 5} more")
        else:
            print(f"  ✅ No violations")

        print()

    return violations

def main():
    base_dir = '.'

    violations = check_layer_violations(base_dir)

    print("=" * 60)
    print("Layer Violation Summary:")
    print()

    if not violations:
        print("✅ NO LAYER VIOLATIONS DETECTED")
        print()
        print("All components respect layered architecture boundaries.")
    else:
        print(f"🔴 FOUND {len(violations)} LAYER VIOLATIONS")
        print()
        print("Critical violations that break architectural integrity.")
        print("These should be fixed to maintain clean layered design.")

    print()

    # Save violations to file
    output_file = 'test_results/dependency_analysis/layer_violations.txt'
    with open(output_file, 'w') as f:
        f.write("Layer Violations\n")
        f.write("=" * 60 + "\n\n")

        if violations:
            for v in violations:
                f.write(f"File: {v['file']}\n")
                f.write(f"  Includes: {v['includes']} ({v['include_layer']} layer)\n")
                f.write(f"  Reason: {v['reason']}\n\n")
        else:
            f.write("No layer violations detected.\n")

    print(f"Layer violations report saved to: {output_file}")

if __name__ == '__main__':
    main()
