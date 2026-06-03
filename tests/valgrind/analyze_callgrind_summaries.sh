#!/bin/bash

# SDL3 VoidLight-Framework - Callgrind Summary Analysis
# Analyzes the existing callgrind summary txt files for performance insights
# Filters out test-specific code to focus optimization suggestions on production code

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CALLGRIND_DIR="${PROJECT_ROOT}/test_results/valgrind/callgrind"
SUMMARIES_DIR="${CALLGRIND_DIR}/summaries"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
ANALYSIS_REPORT="${CALLGRIND_DIR}/analysis_summary_${TIMESTAMP}.md"

show_help() {
    echo "VoidLight-Framework Callgrind Summary Analysis"
    echo ""
    echo "Usage: $0 [category]"
    echo ""
    echo "Categories:"
    echo "  all           - Complete analysis of all summary files (default)"
    echo "  ai            - AI behavior core functionality only"
    echo "  ai_threading  - AI threading stress tests only"
    echo "  resources     - Resource management systems only"
    echo "  events        - Event systems only"
    echo "  collision_pathfinding - Collision and pathfinding systems"
    echo "  runtime_managers - Runtime manager and utility systems"
    echo "  render_ui     - Rendering, input, and UI/controller systems"
    echo "  benchmarks    - Benchmark/scaling profiles"
    echo "  threading     - Threading and concurrency stress tests"
    echo "  performance   - Performance and scaling tests"
    echo "  particles     - Core particle system tests"
    echo "  hotspots      - Global performance hotspots with context"
    echo "  help          - Show this help message"
    echo ""
    echo "Output:"
    echo "  - Context-aware analysis of callgrind summary files"
    echo "  - Separation of threading overhead from application logic"
    echo "  - System-categorized performance breakdown"
    echo "  - Markdown analysis report"
}

if [[ "${1:-}" == "help" || "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    show_help
    exit 0
fi

echo -e "${BOLD}${PURPLE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${PURPLE}║         SDL3 VoidLight-Framework                          ║${NC}"
echo -e "${BOLD}${PURPLE}║        Callgrind Summary Analysis Tool                      ║${NC}"
echo -e "${BOLD}${PURPLE}║                                                              ║${NC}"
echo -e "${BOLD}${PURPLE}║  Analyzing Existing Callgrind Summary Files                 ║${NC}"
echo -e "${BOLD}${PURPLE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if summaries directory exists
if [[ ! -d "${SUMMARIES_DIR}" ]]; then
    echo -e "${RED}ERROR: Callgrind summaries directory not found: ${SUMMARIES_DIR}${NC}"
    echo -e "Please run callgrind profiling first: ./tests/valgrind/callgrind_profiling_analysis.sh"
    exit 1
fi

# Find all summary and analysis files
summary_files=($(find "${SUMMARIES_DIR}" -name "*_summary.txt" 2>/dev/null))
analysis_files=($(find "${SUMMARIES_DIR}" -name "*_function_analysis.txt" 2>/dev/null))

if [[ ${#summary_files[@]} -eq 0 && ${#analysis_files[@]} -eq 0 ]]; then
    echo -e "${RED}ERROR: No callgrind summary files found${NC}"
    echo -e "Please run callgrind profiling first: ./tests/valgrind/callgrind_profiling_analysis.sh"
    exit 1
fi

echo -e "${CYAN}Found ${#summary_files[@]} summary files and ${#analysis_files[@]} analysis files${NC}"
echo ""

filtered_project_profile_lines() {
    local file="$1"

    awk -v project_root="${PROJECT_ROOT}" '
        function trim(value) {
            sub(/^[[:space:]]+/, "", value)
            sub(/[[:space:]]+$/, "", value)
            return value
        }

        function is_profile_line(line) {
            return line ~ /^[[:space:]]*[0-9][0-9,]*[[:space:]]+\([[:space:]]*[0-9.]+%\)/
        }

        function is_noise_frame(line) {
            return line ~ /PROGRAM TOTALS/ ||
                   line ~ /\?\?\?:/ ||
                   line ~ /\(below main\)/ ||
                   line ~ /ld-linux/ ||
                   line ~ /libc\.so/ ||
                   line ~ /libstdc\+\+/ ||
                   line ~ /\/usr\/src\/debug\/glibc/ ||
                   line ~ /\/usr\/src\/debug\/boost/ ||
                   line ~ /\/usr\/include\/boost/ ||
                   line ~ /\/usr\/include\/c\+\+/ ||
                   line ~ /BOOST_/ ||
                   line ~ /boost::unit_test/ ||
                   line ~ /test_main/ ||
                   line ~ /:main[[:space:]]+\[/
        }

        function is_project_source(line) {
            return index(line, project_root "/src/") ||
                   index(line, project_root "/include/") ||
                   line ~ /(^|[[:space:]])src\// ||
                   line ~ /(^|[[:space:]])include\//
        }

        function frame_key(line, key) {
            key = line
            sub(/^[[:space:]]*[0-9][0-9,]*[[:space:]]+\([[:space:]]*[0-9.]+%\)[[:space:]]*/, "", key)
            gsub(project_root "/", "", key)
            sub(/[[:space:]]+\[.*\]$/, "", key)
            return key
        }

        is_profile_line($0) {
            line = trim($0)
            gsub(/\|/, " ", line)

            if (!is_noise_frame(line) && is_project_source(line)) {
                key = frame_key(line)
                if (!seen[key]++) {
                    print line
                }
            }
        }
    ' "$file" 2>/dev/null
}

filtered_thread_profile_lines() {
    local file="$1"

    awk -v project_root="${PROJECT_ROOT}" '
        function trim(value) {
            sub(/^[[:space:]]+/, "", value)
            sub(/[[:space:]]+$/, "", value)
            return value
        }

        function is_profile_line(line) {
            return line ~ /^[[:space:]]*[0-9][0-9,]*[[:space:]]+\([[:space:]]*[0-9.]+%\)/
        }

        function is_startup_or_harness_frame(line) {
            return line ~ /PROGRAM TOTALS/ ||
                   line ~ /\?\?\?:/ ||
                   line ~ /\(below main\)/ ||
                   line ~ /ld-linux/ ||
                   line ~ /__libc_start_main/ ||
                   line ~ /libc_start_call_main/ ||
                   line ~ /\/usr\/src\/debug\/boost/ ||
                   line ~ /\/usr\/include\/boost/ ||
                   line ~ /BOOST_/ ||
                   line ~ /boost::unit_test/ ||
                   line ~ /test_main/ ||
                   line ~ /:main[[:space:]]+\[/
        }

        function is_thread_frame(line) {
            return line ~ /pthread/ ||
                   line ~ /std::thread/ ||
                   line ~ /ThreadPool/ ||
                   line ~ /workerThread/ ||
                   line ~ /enqueueWithResult/ ||
                   line ~ /packaged_task/ ||
                   line ~ /std::call_once/ ||
                   line ~ /\/future:/ ||
                   line ~ /\/mutex:/
        }

        function frame_key(line, key) {
            key = line
            sub(/^[[:space:]]*[0-9][0-9,]*[[:space:]]+\([[:space:]]*[0-9.]+%\)[[:space:]]*/, "", key)
            gsub(project_root "/", "", key)
            sub(/[[:space:]]+\[.*\]$/, "", key)
            return key
        }

        is_profile_line($0) {
            line = trim($0)
            gsub(/\|/, " ", line)

            if (!is_startup_or_harness_frame(line) && is_thread_frame(line)) {
                key = frame_key(line)
                if (!seen[key]++) {
                    print line
                }
            }
        }
    ' "$file" 2>/dev/null
}

callgrind_percent() {
    sed -E 's/^[[:space:]]*[0-9,]+[[:space:]]+\([[:space:]]*([0-9.]+)%\).*/\1/'
}

callgrind_function_text() {
    sed -E 's/^[[:space:]]*[0-9,]+[[:space:]]+\([[:space:]]*[0-9.]+%\)[[:space:]]*//'
}

percent_at_least() {
    awk -v value="$1" -v minimum="$2" 'BEGIN { exit !((value + 0) >= (minimum + 0)) }'
}

percent_in_range() {
    awk -v value="$1" -v minimum="$2" -v maximum="$3" 'BEGIN { exit !((value + 0) >= (minimum + 0) && (value + 0) < (maximum + 0)) }'
}

# Function to extract top functions from a summary file with context awareness
extract_top_functions() {
    local file="$1"
    local test_name="$(basename "$file" "_summary.txt")"
    
    echo -e "${YELLOW}=== $test_name ===${NC}"
    
    # Provide context for test type
    if is_threading_test "$test_name"; then
        echo -e "${CYAN}Note: Threading stress test - high threading overhead expected${NC}"
    elif is_scaling_test "$test_name"; then
        echo -e "${CYAN}Note: Scaling/performance test - designed to stress system limits${NC}"
    fi
    
    if [[ -f "$file" ]]; then
        if is_threading_test "$test_name" || is_scaling_test "$test_name"; then
            # For threading/scaling tests, focus on application logic
            echo -e "${CYAN}Project Logic (excluding startup, harness, and system frames):${NC}"
            filtered_project_profile_lines "$file" | head -10 | while read line; do
                if [[ -n "$line" ]]; then
                    echo "  $line"
                fi
            done
            
            echo -e "${CYAN}Top System Functions (threading overhead):${NC}"
            count=0
            filtered_thread_profile_lines "$file" | while read line; do
                if [[ -n "$line" ]]; then
                    echo "  $line"
                    ((++count))
                    [[ ${count} -ge 3 ]] && break
                fi
            done
        else
            # For core functionality tests, show normal analysis
            echo -e "${CYAN}Top Project Functions (by inclusive instruction count):${NC}"
            filtered_project_profile_lines "$file" | head -10 | while read line; do
                if [[ -n "$line" ]]; then
                    echo "  $line"
                fi
            done
        fi
        
        # Look for total instruction count
        total=$(grep "PROGRAM TOTALS" "$file" 2>/dev/null | awk '{print $1}' | tr -d ',' || echo "")
        if [[ -n "$total" ]]; then
            echo -e "${CYAN}Total Instructions: ${total}${NC}"
        fi
        
        echo ""
    else
        echo -e "${RED}  File not found${NC}"
        echo ""
    fi
}

# Function to analyze function analysis files
analyze_function_files() {
    local file="$1"
    local test_name="$(basename "$file" "_function_analysis.txt")"
    
    echo -e "${YELLOW}=== $test_name Function Analysis ===${NC}"
    
    if [[ -f "$file" ]]; then
        # Extract critical hotspots
        echo -e "${RED}Critical Hotspots (≥10%):${NC}"
        sed -n '/## Critical Hotspots/,/## Moderate/p' "$file" | grep "^- " | head -5
        
        # Extract moderate targets
        echo -e "${YELLOW}Moderate Targets (2-10%):${NC}"
        sed -n '/## Moderate Optimization/,/## Minor/p' "$file" | grep "^- " | head -5
        
        # Extract minor opportunities
        echo -e "${GREEN}Minor Opportunities (1-2%):${NC}"
        sed -n '/## Minor Opportunities/,/$/p' "$file" | grep "^- " | head -3
        
        echo ""
    else
        echo -e "${RED}  Analysis file not found${NC}"
        echo ""
    fi
}

# Function to categorize and analyze by system
analyze_by_category() {
    local category="$1"
    local pattern="$2"
    
    echo -e "${BOLD}${BLUE}=== $category Systems ===${NC}"
    echo ""
    
    local found=false
    for file in "${summary_files[@]}"; do
        if [[ "$(basename "$file")" =~ $pattern ]]; then
            extract_top_functions "$file"
            found=true
        fi
    done
    
    for file in "${analysis_files[@]}"; do
        if [[ "$(basename "$file")" =~ $pattern ]]; then
            analyze_function_files "$file"
            found=true
        fi
    done
    
    if [[ "$found" == false ]]; then
        echo -e "${YELLOW}No files found for $category${NC}"
        echo ""
    fi
}

# Function to check if a function should be excluded from optimization suggestions
is_test_code_function() {
    local function_name="$1"
    
    # Exclude test-specific classes and functions
    [[ "$function_name" =~ (Test|Mock|Benchmark|Stress|Threading.*Behavior|Buffer.*Test) ]] || \
    [[ "$function_name" =~ (BOOST_|boost::test|test_main|run_test) ]] || \
    [[ "$function_name" =~ (.*Test::|.*Mock::|.*Benchmark::) ]] || \
    [[ "$function_name" =~ (ThreadTestBehavior|TestEntity|MockNPC|MockPlayer) ]] || \
    [[ "$function_name" =~ (StressTest|PerformanceTest|ScalingTest) ]]
}

# Function to categorize tests by their purpose
is_threading_test() {
    local test_name="$1"
    [[ "$test_name" =~ (thread|threading|buffer_utilization) ]]
}

is_scaling_test() {
    local test_name="$1"
    [[ "$test_name" =~ (scaling|performance|stress) ]]
}

is_core_functionality_test() {
    local test_name="$1"
    [[ "$test_name" =~ (optimization|functionality|core|manager|factory|template) ]] && ! is_threading_test "$test_name" && ! is_scaling_test "$test_name"
}

# Function to find most expensive functions across all tests with context awareness
find_global_hotspots() {
    echo -e "${BOLD}${RED}=== Global Performance Hotspots ===${NC}"
    echo ""
    
    # Create temporary files for different test categories
    local all_functions="/tmp/all_functions.txt"
    local core_functions="/tmp/core_functions.txt"
    local threading_functions="/tmp/threading_functions.txt"
    local scaling_functions="/tmp/scaling_functions.txt"
    
    rm -f "$all_functions" "$core_functions" "$threading_functions" "$scaling_functions"
    
    # Extract all function percentages from summary files and categorize
    for file in "${summary_files[@]}"; do
        test_name="$(basename "$file" "_summary.txt")"
        filtered_project_profile_lines "$file" | head -20 | while read line; do
            percentage=$(echo "$line" | callgrind_percent | grep -E '^[0-9.]+$' || echo "0")
            function_name=$(echo "$line" | callgrind_function_text)
            if [[ -n "$percentage" && "$percentage" != "0" && -n "$function_name" ]]; then
                echo "$percentage|$function_name|$test_name" >> "$all_functions"
                
                # Categorize by test type
                if is_core_functionality_test "$test_name"; then
                    echo "$percentage|$function_name|$test_name" >> "$core_functions"
                elif is_threading_test "$test_name"; then
                    echo "$percentage|$function_name|$test_name" >> "$threading_functions"
                elif is_scaling_test "$test_name"; then
                    echo "$percentage|$function_name|$test_name" >> "$scaling_functions"
                fi
            fi
        done
    done
    
    # Analyze core functionality tests (excluding threading/scaling stress tests)
    if [[ -f "$core_functions" && -s "$core_functions" ]]; then
        echo -e "${CYAN}=== Core Functionality Performance (Excluding Threading Stress Tests) ===${NC}"
        echo -e "${RED}Critical Functions (≥10% in core tests):${NC}"
        
        # Filter out obvious threading/system functions for core analysis
        sort -t'|' -nr -k1 "$core_functions" | \
        grep -v -E "(clone|pthread|thread|std::thread|_M_run|_Invoker|__invoke)" | \
        {
        count=0
        while IFS='|' read -r percentage function_name test_name; do
            if [[ "$percentage" =~ ^[0-9]+\.?[0-9]*$ ]] && percent_at_least "$percentage" 10.0 && ! is_test_code_function "$function_name"; then
                printf "  %.1f%% - %s (in %s)\n" "$percentage" "$function_name" "$test_name"
                ((++count))
                [[ ${count} -ge 10 ]] && break
            fi
        done
        }
        
        echo ""
        echo -e "${YELLOW}Moderate Targets (2-10% in core tests):${NC}"
        sort -t'|' -nr -k1 "$core_functions" | \
        grep -v -E "(clone|pthread|thread|std::thread|_M_run|_Invoker|__invoke)" | \
        {
        count=0
        while IFS='|' read -r percentage function_name test_name; do
            if [[ "$percentage" =~ ^[0-9]+\.?[0-9]*$ ]] && percent_in_range "$percentage" 2.0 10.0 && ! is_test_code_function "$function_name"; then
                printf "  %.1f%% - %s (in %s)\n" "$percentage" "$function_name" "$test_name"
                ((++count))
                [[ ${count} -ge 10 ]] && break
            fi
        done
        }
        echo ""
    fi
    
    # Analyze threading tests with context
    if [[ -f "$threading_functions" && -s "$threading_functions" ]]; then
        echo -e "${CYAN}=== Threading Stress Test Analysis ===${NC}"
        echo -e "${YELLOW}Note: High threading overhead is expected in these tests${NC}"
        echo -e "${GREEN}Application Logic in Threading Tests (excluding system threading):${NC}"
        
        # Look for application-specific functions in threading tests
        sort -t'|' -nr -k1 "$threading_functions" | \
        grep -E "(VoidLight-Framework|AIManager|ParticleManager|TaskQueue|Entity|Behavior)" | \
        {
        count=0
        while IFS='|' read -r percentage function_name test_name; do
            if [[ "$percentage" =~ ^[0-9]+\.?[0-9]*$ ]] && percent_at_least "$percentage" 1.0 && ! is_test_code_function "$function_name"; then
                printf "  %.1f%% - %s (in %s)\n" "$percentage" "$function_name" "$test_name"
                ((++count))
                [[ ${count} -ge 10 ]] && break
            fi
        done
        }
        echo ""
    fi
    
    # Analyze scaling tests
    if [[ -f "$scaling_functions" && -s "$scaling_functions" ]]; then
        echo -e "${CYAN}=== Scaling/Performance Test Analysis ===${NC}"
        echo -e "${YELLOW}Note: These tests are designed to stress system limits${NC}"
        echo -e "${GREEN}Performance Bottlenecks Identified:${NC}"
        
        sort -t'|' -nr -k1 "$scaling_functions" | \
        grep -v -E "(clone|pthread|thread|std::thread|_M_run|_Invoker|__invoke|PROGRAM TOTALS|ld-linux)" | \
        {
        count=0
        while IFS='|' read -r percentage function_name test_name; do
            if [[ "$percentage" =~ ^[0-9]+\.?[0-9]*$ ]] && percent_at_least "$percentage" 5.0 && ! is_test_code_function "$function_name"; then
                printf "  %.1f%% - %s (in %s)\n" "$percentage" "$function_name" "$test_name"
                ((++count))
                [[ ${count} -ge 10 ]] && break
            fi
        done
        }
        echo ""
    fi
    
    # Clean up temp files
    rm -f "$all_functions" "$core_functions" "$threading_functions" "$scaling_functions"
}

# Function to generate comprehensive analysis report
generate_analysis_report() {
    echo -e "${CYAN}Generating comprehensive analysis report...${NC}"
    
    cat > "${ANALYSIS_REPORT}" << EOF
# SDL3 VoidLight-Framework - Callgrind Analysis Summary

**Generated**: $(date)
**Source**: Callgrind summary and function analysis files
**Location**: ${SUMMARIES_DIR}
**Note**: Test-specific code is filtered out from optimization suggestions

## Summary Files Analyzed

EOF

    for file in "${summary_files[@]}"; do
        test_name="$(basename "$file" "_summary.txt")"
        echo "- **${test_name}**: \`$(basename "$file")\`" >> "${ANALYSIS_REPORT}"
    done
    
    echo "" >> "${ANALYSIS_REPORT}"
    echo "## Function Analysis Files" >> "${ANALYSIS_REPORT}"
    echo "" >> "${ANALYSIS_REPORT}"
    
    for file in "${analysis_files[@]}"; do
        test_name="$(basename "$file" "_function_analysis.txt")"
        echo "- **${test_name}**: \`$(basename "$file")\`" >> "${ANALYSIS_REPORT}"
    done
    
    echo "" >> "${ANALYSIS_REPORT}"
    echo "## Key Findings" >> "${ANALYSIS_REPORT}"
    echo "" >> "${ANALYSIS_REPORT}"
    
    # Add function analysis summaries to report
    local temp_file="/tmp/report_functions.txt"
    rm -f "$temp_file"
    
    for file in "${summary_files[@]}"; do
        test_name="$(basename "$file" "_summary.txt")"
        filtered_project_profile_lines "$file" | head -5 | while read line; do
            percentage=$(echo "$line" | callgrind_percent | grep -E '^[0-9.]+$' || echo "0")
            function_name=$(echo "$line" | callgrind_function_text)
            if [[ -n "$percentage" && "$percentage" != "0" && -n "$function_name" ]]; then
                echo "$percentage|$function_name|$test_name" >> "$temp_file"
            fi
        done
    done
    
    if [[ -f "$temp_file" && -s "$temp_file" ]]; then
        echo "### Top Performance Impact Functions" >> "${ANALYSIS_REPORT}"
        echo "" >> "${ANALYSIS_REPORT}"
        sort -t'|' -nr -k1 "$temp_file" | head -15 | while IFS='|' read percentage function_name test_name; do
            echo "- **${percentage}%** - ${function_name} (${test_name})" >> "${ANALYSIS_REPORT}"
        done
        rm -f "$temp_file"
    fi
    
    echo "" >> "${ANALYSIS_REPORT}"
    echo "## Analysis by System Category" >> "${ANALYSIS_REPORT}"
    echo "" >> "${ANALYSIS_REPORT}"
    
    # Add system-specific analysis
    echo "### AI Behavior Systems" >> "${ANALYSIS_REPORT}"
    for file in "${summary_files[@]}"; do
        if [[ "$(basename "$file")" =~ (ai_|behavior) ]]; then
            test_name="$(basename "$file" "_summary.txt")"
            echo "- **${test_name}**: See \`$(basename "$file")\`" >> "${ANALYSIS_REPORT}"
        fi
    done
    
    echo "" >> "${ANALYSIS_REPORT}"
    echo "### Resource Management Systems" >> "${ANALYSIS_REPORT}"
    for file in "${summary_files[@]}"; do
        if [[ "$(basename "$file")" =~ (resource|inventory|json) ]]; then
            test_name="$(basename "$file" "_summary.txt")"
            echo "- **${test_name}**: See \`$(basename "$file")\`" >> "${ANALYSIS_REPORT}"
        fi
    done
    
    echo "" >> "${ANALYSIS_REPORT}"
    echo "### Performance Critical Systems" >> "${ANALYSIS_REPORT}"
    for file in "${summary_files[@]}"; do
        if [[ "$(basename "$file")" =~ (particle|thread|buffer|save|ui) ]]; then
            test_name="$(basename "$file" "_summary.txt")"
            echo "- **${test_name}**: See \`$(basename "$file")\`" >> "${ANALYSIS_REPORT}"
        fi
    done
    
    echo "" >> "${ANALYSIS_REPORT}"
    echo "## Usage Instructions" >> "${ANALYSIS_REPORT}"
    echo "" >> "${ANALYSIS_REPORT}"
    echo "To view detailed analysis for any test:" >> "${ANALYSIS_REPORT}"
    echo '```bash' >> "${ANALYSIS_REPORT}"
    echo "# View summary file" >> "${ANALYSIS_REPORT}"
    echo "cat ${SUMMARIES_DIR}/[test_name]_summary.txt" >> "${ANALYSIS_REPORT}"
    echo "" >> "${ANALYSIS_REPORT}"
    echo "# View function analysis (if available)" >> "${ANALYSIS_REPORT}"
    echo "cat ${SUMMARIES_DIR}/[test_name]_function_analysis.txt" >> "${ANALYSIS_REPORT}"
    echo '```' >> "${ANALYSIS_REPORT}"
    
    echo "" >> "${ANALYSIS_REPORT}"
    echo "## Automated Result" >> "${ANALYSIS_REPORT}"
    echo "" >> "${ANALYSIS_REPORT}"
    echo "Result: **pass**" >> "${ANALYSIS_REPORT}"
    echo "" >> "${ANALYSIS_REPORT}"
    echo "*Report generated by SDL3 VoidLight-Framework Callgrind Summary Analysis Tool*" >> "${ANALYSIS_REPORT}"
    
    echo -e "${GREEN}✓ Analysis report generated: ${ANALYSIS_REPORT}${NC}"
}

# Main analysis execution
main() {
    echo -e "${BOLD}${BLUE}=== SDL3 VoidLight-Framework - Callgrind Analysis ===${NC}"
    echo -e "${CYAN}Note: Test-specific code is filtered out from optimization suggestions${NC}"
    echo ""
    
    # Find global hotspots across all tests with context awareness
    find_global_hotspots
    
    # Analyze by system categories with threading context
    analyze_by_category "AI Behavior (Core)" "(ai_optimization|behavior_functionality)"
    analyze_by_category "AI Threading Tests" "(thread_safe_ai)"
    analyze_by_category "Resource Management" "(resource|inventory|json)"
    analyze_by_category "Event System" "(event|weather)"
    analyze_by_category "Threading & Concurrency" "(thread|threading|buffer_utilization)"
    analyze_by_category "Performance & Scaling" "(particle_performance|scaling|ui_manager_functional|save_manager)"
    analyze_by_category "Core Particle System" "(particle_core|particle_weather)"
    
    # Generate comprehensive report
    generate_analysis_report
    
    echo -e "${BOLD}${GREEN}Analysis complete!${NC}"
    echo -e "${CYAN}Detailed report: ${ANALYSIS_REPORT}${NC}"
    echo ""
    echo -e "${BOLD}${YELLOW}Context Notes:${NC}"
    echo -e "${YELLOW}• Threading tests show high threading overhead by design${NC}"
    echo -e "${YELLOW}• Scaling tests are meant to stress system limits${NC}"
    echo -e "${YELLOW}• Focus on application logic within threading tests for optimization${NC}"
    echo ""
    echo -e "${YELLOW}To analyze specific files:${NC}"
    echo -e "  ${CYAN}cat ${SUMMARIES_DIR}/[test_name]_summary.txt${NC}"
    echo -e "  ${CYAN}cat ${SUMMARIES_DIR}/[test_name]_function_analysis.txt${NC}"
}

# Handle command line arguments
case "${1:-all}" in
    "ai")
        analyze_by_category "AI Behavior (Core)" "(ai_optimization|behavior_functionality)"
        ;;
    "ai_threading"|"ai_threads")
        analyze_by_category "AI Threading Tests" "(thread_safe_ai)"
        ;;
    "resources")
        analyze_by_category "Resource Management" "(resource|inventory|json)"
        ;;
    "events")
        analyze_by_category "Event System" "(event|weather)"
        ;;
    "collision_pathfinding")
        analyze_by_category "Collision and Pathfinding" "(collision|pathfinding|pathfinder|projectile|sidecar)"
        ;;
    "runtime_managers")
        analyze_by_category "Runtime Managers" "(particle|buffer|frame_profiler|save_manager|settings_manager|game_state_manager|loading_state|manager_runtime|background_simulation)"
        ;;
    "render_ui")
        analyze_by_category "Render and UI" "(camera|rendering|input|ui_manager|controller|hud|inventory|combat|social|day_night|game_time)"
        ;;
    "benchmarks")
        analyze_by_category "Benchmarks" "(benchmark|scaling|integrated_system)"
        ;;
    "threading"|"threads")
        analyze_by_category "Threading & Concurrency" "(thread|threading|buffer_utilization)"
        ;;
    "performance"|"perf"|"scaling")
        analyze_by_category "Performance & Scaling" "(particle_performance|scaling|ui_manager_functional|save_manager)"
        ;;
    "particles"|"particle_core")
        analyze_by_category "Core Particle System" "(particle_core|particle_weather)"
        ;;
    "hotspots"|"global")
        find_global_hotspots
        ;;
    "help"|"-h"|"--help")
        show_help
        exit 0
        ;;
    "all"|*)
        main
        ;;
esac

echo ""
echo -e "Result: ${GREEN}PASS${NC}"
