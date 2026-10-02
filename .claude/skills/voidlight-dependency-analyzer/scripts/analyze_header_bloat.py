#!/usr/bin/env python3
"""
Analyze header bloat and forward declaration opportunities in VoidLight-Framework
"""

import os
import re
from collections import defaultdict

def find_headers(base_dir):
    """Find all header files"""
    headers = []
    for root, dirs, files in os.walk(os.path.join(base_dir, 'include')):
        for file in files:
            if file.endswith('.hpp'):
                headers.append(os.path.join(root, file))
    return sorted(headers)

def count_includes(file_path):
    """Count number of includes in a file"""
    count = 0
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                if re.match(r'^\s*#include', line):
                    count += 1
    except Exception:
        pass
    return count

def extract_includes(file_path):
    """Extract local includes from a file"""
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

def count_usages_in_file(file_path, class_name):
    """Count how many times a class is used in a file.

    A forward declaration is only safe when the type is NEVER needed as a
    complete type in THIS header. That rules out more than plain "ClassName
    varname;" — it also rules out:
      - brace-init by-value members/locals: `ClassName x{};`, `ClassName x{1,2};`
        (the single most common pattern in this codebase's style, and the one
        the old regex missed entirely — see review-non-issues / dependency
        audit 2026-07-03).
      - copy-init by-value: `ClassName x = ...;`
      - by-value storage inside a standard container: `std::vector<ClassName>`,
        `std::array<ClassName, N>` (these need the complete type for most
        real operations even though technically declarable incomplete in
        limited cases in C++17+ — treat as unsafe, not a good opportunity).
    `ptr_ref` counts smart-pointer members separately (`unique_ptr<ClassName>`/
    `shared_ptr<ClassName>`) because those are forward-declare-safe ONLY if
    the owning class's destructor (and any special member function that
    would instantiate the deleter) is defined out-of-line in the .cpp — the
    classic Pimpl requirement. Callers must not claim these are safe without
    checking that separately.
    """
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # Look for pointer/reference usage: ClassName* or ClassName&
        ptr_ref_pattern = rf'\b{class_name}\s*[*&]'
        ptr_ref_count = len(re.findall(ptr_ref_pattern, content))

        # Smart-pointer member/local: unique_ptr<ClassName> / shared_ptr<ClassName>
        # Forward-declarable ONLY with an out-of-line destructor (Pimpl) — flagged
        # separately, never folded into a blanket "safe" verdict.
        smart_ptr_pattern = rf'\b(?:unique_ptr|shared_ptr)\s*<\s*{class_name}\s*>'
        smart_ptr_count = len(re.findall(smart_ptr_pattern, content))

        # Look for direct usage requiring a complete type:
        #   - ClassName varname; / ClassName varname( / ClassName::member
        #   - ClassName varname{...}   <- brace-init (previously missed)
        #   - ClassName varname = ...  <- copy-init
        #   - vector<ClassName> / array<ClassName, ...>  <- by-value container storage
        direct_pattern = (
            rf'\b{class_name}\s+\w+[;\(]'
            rf'|\b{class_name}::'
            rf'|\b{class_name}\s+\w+\s*\{{'
            rf'|\b{class_name}\s+\w+\s*='
            rf'|\b(?:vector|array)\s*<\s*{class_name}\s*[,>]'
        )
        direct_count = len(re.findall(direct_pattern, content))

        # Look for member variables: m_className
        member_pattern = rf'\bm_\w*{class_name}\w*'
        member_count = len(re.findall(member_pattern, content, re.IGNORECASE))

        return {
            'ptr_ref': ptr_ref_count,
            'direct': direct_count,
            'member': member_count,
            'smart_ptr': smart_ptr_count,
        }
    except Exception:
        return {'ptr_ref': 0, 'direct': 0, 'member': 0, 'smart_ptr': 0}

def analyze_header_bloat(base_dir):
    """Analyze header bloat"""
    headers = find_headers(base_dir)

    print("=== Header Bloat Analysis ===")
    print()

    # Find headers with high include counts
    include_counts = []
    for header in headers:
        count = count_includes(header)
        include_counts.append((os.path.basename(header), count))

    include_counts.sort(key=lambda x: x[1], reverse=True)

    print("Headers with High Include Count (potential bloat):")
    print()

    high_bloat = []
    for name, count in include_counts:
        if count > 15:
            print(f"  🔴 {name:45s} {count} includes (HIGH - review for bloat)")
            high_bloat.append(name)
        elif count > 10:
            print(f"  ⚠️  {name:45s} {count} includes (MODERATE)")
            high_bloat.append(name)
        elif count > 7:
            print(f"  🟡 {name:45s} {count} includes")

    print()

    return high_bloat

def analyze_frequently_included(base_dir):
    """Find frequently included headers"""
    headers = find_headers(base_dir)

    # Count how many files include each header
    include_freq = defaultdict(int)

    for header in headers:
        includes = extract_includes(header)
        for include in includes:
            include_name = os.path.basename(include)
            include_freq[include_name] += 1

    print("Frequently Included Headers (ripple effect on compile times):")
    print()

    sorted_freq = sorted(include_freq.items(), key=lambda x: x[1], reverse=True)

    ripple_headers = []
    for header_name, freq in sorted_freq[:15]:
        # Find the actual header file
        header_path = None
        for h in headers:
            if os.path.basename(h) == header_name:
                header_path = h
                break

        if header_path:
            header_includes = count_includes(header_path)

            status = ""
            if header_includes > 10:
                status = f"⚠️  bloat amplification ({header_includes} includes)"
                ripple_headers.append((header_name, freq, header_includes))

            print(f"  {header_name:45s} included by {freq:2d} files {status}")

    print()

    return ripple_headers

def analyze_forward_declaration_opportunities(base_dir):
    """Find forward declaration opportunities"""
    headers = find_headers(base_dir)

    print("=== Forward Declaration Opportunities ===")
    print()

    opportunities = []

    for header in headers:
        includes = extract_includes(header)

        for include in includes:
            include_name = os.path.basename(include)
            class_name = include_name.replace('.hpp', '')

            # Analyze usage in the header
            usage = count_usages_in_file(header, class_name)

            # Never a candidate if the complete type is needed anywhere in
            # this header (by-value member/local, brace/copy-init, or
            # by-value container storage — see count_usages_in_file).
            if usage['direct'] != 0 or usage['ptr_ref'] == 0:
                continue

            opportunities.append({
                'header': os.path.basename(header),
                'include': include_name,
                'class': class_name,
                # A raw pointer/reference is safe to forward-declare outright.
                # A unique_ptr/shared_ptr member is ONLY safe if the owning
                # class's destructor (and copy/move special members, if any)
                # are also moved out-of-line into the .cpp — otherwise the
                # implicit/defaulted destructor needs the complete type to
                # destroy the pointee and this will fail to compile.
                'needs_out_of_line_destructor': usage['smart_ptr'] > 0,
            })

    # Show top opportunities
    print(f"Found {len(opportunities)} forward declaration opportunities:")
    print()

    for i, opp in enumerate(opportunities[:20], 1):
        print(f"  {i:2d}. {opp['header']:40s}")
        print(f"      Can forward-declare {opp['class']}")
        print(f"      Remove: #include \"{opp['include']}\"")
        print(f"      Add: class {opp['class']};  // Forward declaration")
        if opp['needs_out_of_line_destructor']:
            print(f"      ⚠️  Held via unique_ptr/shared_ptr — ALSO move the owning class's")
            print(f"          destructor (and any defaulted copy/move members) out-of-line")
            print(f"          into the .cpp, defined after {opp['class']} is complete there.")
            print(f"          Skipping that step will not compile.")
        print()

    if len(opportunities) > 20:
        print(f"  ... and {len(opportunities) - 20} more opportunities")
        print()

    print("Note: Move #include to .cpp file after forward declaration")
    print()

    return opportunities

def main():
    base_dir = '.'

    high_bloat = analyze_header_bloat(base_dir)
    print()

    ripple_headers = analyze_frequently_included(base_dir)
    print()

    opportunities = analyze_forward_declaration_opportunities(base_dir)

    # Estimate compile time savings
    if opportunities:
        estimated_savings = len(opportunities) * 0.5  # Rough estimate
        print(f"Estimated Compile Time Savings: ~{estimated_savings:.0f}% reduction")
        print()

    # Save results
    output_file = 'test_results/dependency_analysis/header_bloat_analysis.txt'
    with open(output_file, 'w') as f:
        f.write("Header Bloat Analysis\n")
        f.write("=" * 60 + "\n\n")

        f.write("High-Bloat Headers:\n")
        for header in high_bloat:
            f.write(f"  - {header}\n")
        f.write("\n")

        f.write("Ripple Effect Headers:\n")
        for header, freq, includes in ripple_headers:
            f.write(f"  - {header} (included by {freq} files, has {includes} includes)\n")
        f.write("\n")

        f.write(f"Forward Declaration Opportunities: {len(opportunities)}\n")

    print(f"Header bloat analysis saved to: {output_file}")

if __name__ == '__main__':
    main()
