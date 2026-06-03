#!/bin/bash
# Cppcheck Analysis Script for VoidLight-Framework
# This script runs cppcheck with optimized settings to focus on real issues

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INCLUDE_DIRS="include src"
LIBRARY_CONFIG="cppcheck_lib.cfg"
OUTPUT_DIR="${PROJECT_ROOT}/test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

echo -e "${BLUE}=== VoidLight-Framework - Cppcheck Analysis ===${NC}"
echo -e "${BLUE}Project Root: $PROJECT_ROOT${NC}"
echo -e "${BLUE}Timestamp: $TIMESTAMP${NC}"
echo ""

# Function to run cppcheck with specific settings
run_cppcheck() {
    local name=$1
    local options=$2
    local output_file="${OUTPUT_DIR}/cppcheck_${name}_${TIMESTAMP}.xml"
    local summary_file="${OUTPUT_DIR}/cppcheck_${name}_summary_${TIMESTAMP}.txt"

    echo -e "${YELLOW}Running $name analysis...${NC}"

    # Run cppcheck (prefer compile_commands.json for better cross-TU analysis)
    if [ -f "$PROJECT_ROOT/compile_commands.json" ]; then
        cppcheck \
            --project="$PROJECT_ROOT/compile_commands.json" \
            --enable=warning,style,performance,portability,information \
            --library=std,posix \
            --library="$LIBRARY_CONFIG" \
            --std=c++20 \
            --verbose \
            --xml \
            $options \
            2> "$output_file"
    else
        cppcheck \
            -I../../include \
            -I../../src \
            --enable=warning,style,performance,portability,information \
            --library=std,posix \
            --library="$LIBRARY_CONFIG" \
            --platform=unix64 \
            --std=c++20 \
            --verbose \
            --xml \
            $options \
            ../../src/ ../../include/ \
            2> "$output_file"
    fi

    # Generate summary
    if [ -f "$output_file" ]; then
        local error_count=$(grep -c '<error' "$output_file" 2>/dev/null || echo "0")
        local warning_count=$(grep -c 'severity="warning"' "$output_file" 2>/dev/null || echo "0")
        local style_count=$(grep -c 'severity="style"' "$output_file" 2>/dev/null || echo "0")
        local performance_count=$(grep -c 'severity="performance"' "$output_file" 2>/dev/null || echo "0")
        local portability_count=$(grep -c 'severity="portability"' "$output_file" 2>/dev/null || echo "0")
        local info_count=$(grep -c 'severity="information"' "$output_file" 2>/dev/null || echo "0")

        {
            echo "=== Cppcheck $name Analysis Summary ==="
            echo "Generated: $(date)"
            echo "Configuration: $LIBRARY_CONFIG"
            echo ""
            echo "Results:"
            echo "  Total Issues: $error_count"
            echo "  Warnings: $warning_count"
            echo "  Style: $style_count"
            echo "  Performance: $performance_count"
            echo "  Portability: $portability_count"
            echo "  Information: $info_count"
            echo ""

            if [ "$error_count" -gt 0 ]; then
                echo "Issue Breakdown by Type:"
                grep 'id=' "$output_file" | sed 's/.*id="\([^"]*\)".*/\1/' | sort | uniq -c | sort -nr
                echo ""

                echo "Issue Breakdown by Severity:"
                grep 'severity=' "$output_file" | sed 's/.*severity="\([^"]*\)".*/\1/' | sort | uniq -c | sort -nr
                echo ""

                if [ "$warning_count" -gt 0 ] || [ "$style_count" -gt 0 ] || [ "$performance_count" -gt 0 ]; then
                    echo "=== HIGH PRIORITY ISSUES ==="
                    grep -E 'severity="(warning|style|performance)"' "$output_file" | head -20
                    echo ""
                fi
            fi

        } > "$summary_file"

        # Display summary
        cat "$summary_file"

        # Color-coded result
        if [ "$warning_count" -eq 0 ] && [ "$style_count" -eq 0 ] && [ "$performance_count" -eq 0 ]; then
            echo -e "${GREEN}✓ $name analysis: No critical issues found!${NC}"
        elif [ "$error_count" -lt 10 ]; then
            echo -e "${YELLOW}⚠ $name analysis: $error_count issues found (review recommended)${NC}"
        else
            echo -e "${RED}✗ $name analysis: $error_count issues found (action required)${NC}"
        fi

        echo "  Full report: $output_file"
        echo "  Summary: $summary_file"
        echo ""

        return $error_count
    else
        echo -e "${RED}✗ Failed to generate $name analysis${NC}"
        return 1
    fi
}

# Main analysis
echo -e "${BLUE}Checking cppcheck version...${NC}"
cppcheck --version

echo -e "${BLUE}Validating configuration files...${NC}"
if [ ! -f "$LIBRARY_CONFIG" ]; then
    echo -e "${RED}Error: Library config file '$LIBRARY_CONFIG' not found${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Configuration files validated${NC}"
echo ""

# Run different analysis levels
total_issues=0

# 1. Critical Issues Only (warnings, performance, portability)
echo -e "${BLUE}=== ANALYSIS 1: Critical Issues Only ===${NC}"
run_cppcheck "critical" "--suppress=information --suppress=style"
critical_issues=$?
total_issues=$((total_issues + critical_issues))

# 2. Full Analysis (all enabled checks)
echo -e "${BLUE}=== ANALYSIS 2: Full Analysis ===${NC}"
run_cppcheck "full" ""
full_issues=$?

# 3. Style and Best Practices
echo -e "${BLUE}=== ANALYSIS 3: Style and Best Practices ===${NC}"
run_cppcheck "style" "--enable=style --suppress=information"
style_issues=$?

# Generate final summary
final_summary="${OUTPUT_DIR}/cppcheck_final_summary_${TIMESTAMP}.txt"
{
    echo "=== FINAL CPPCHECK SUMMARY ==="
    echo "Generated: $(date)"
    echo "Project: VoidLight-Framework"
    echo ""
    echo "Analysis Results:"
    echo "  Critical Issues: $critical_issues"
    echo "  Full Analysis Issues: $full_issues"
    echo "  Style Issues: $style_issues"
    echo ""

    if [ $critical_issues -eq 0 ]; then
        echo "STATUS: ✓ PASSED - No critical issues found"
        echo "The codebase appears to be free of critical defects."
    elif [ $critical_issues -lt 5 ]; then
        echo "STATUS: ⚠ REVIEW NEEDED - Few critical issues found"
        echo "Review the critical issues report and address if necessary."
    else
        echo "STATUS: ✗ ACTION REQUIRED - Multiple critical issues found"
        echo "Please review and address the critical issues before proceeding."
    fi

    echo ""
    echo "Next Steps:"
    echo "1. Review critical issues first (highest priority)"
    echo "2. Consider style improvements for code quality"
    echo "3. Re-run analysis after fixes"

} > "$final_summary"

# Display final summary
echo -e "${BLUE}=== FINAL SUMMARY ===${NC}"
cat "$final_summary"

# Set exit code based on critical issues
if [ $critical_issues -eq 0 ]; then
    echo -e "${GREEN}🎉 Cppcheck analysis completed successfully!${NC}"
    exit 0
elif [ $critical_issues -lt 5 ]; then
    echo -e "${YELLOW}⚠ Cppcheck analysis completed with minor issues${NC}"
    exit 1
else
    echo -e "${RED}❌ Cppcheck analysis found significant issues${NC}"
    exit 2
fi
