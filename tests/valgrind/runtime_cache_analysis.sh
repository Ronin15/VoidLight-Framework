#!/bin/bash

# SDL3 VoidLight-Framework - Runtime Cache Performance Analysis
# Runs the main application under Cachegrind for extended duration testing
# Analyzes L1/L2/LLC cache performance during real-world usage
#
# Usage:
#   ./runtime_cache_analysis.sh [OPTIONS] [DURATION]
#
# Options:
#   --profile    Test profile build (bin/profile) - Valgrind-compatible optimized build
#   --debug      Test debug build (bin/debug) [default]
#   --help       Show this help message
#
# Duration: Time in seconds (default: 180)
#
# Examples:
#   ./runtime_cache_analysis.sh                  # Debug build, 3 minutes
#   ./runtime_cache_analysis.sh 300              # Debug build, 5 minutes
#   ./runtime_cache_analysis.sh --profile 300   # Profile build, 5 minutes (recommended)

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

# Default values
BUILD_TYPE="debug"
DEFAULT_RUNTIME=180
RUNTIME=""

# Parse command line arguments
show_help() {
    head -20 "$0" | tail -18 | sed 's/^# //' | sed 's/^#//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --profile)
            BUILD_TYPE="profile"
            shift
            ;;
        --debug)
            BUILD_TYPE="debug"
            shift
            ;;
        --help|-h)
            show_help
            ;;
        *)
            # Assume it's the runtime duration
            if [[ "$1" =~ ^[0-9]+$ ]]; then
                RUNTIME="$1"
            else
                echo -e "${RED}Unknown option: $1${NC}"
                echo "Use --help for usage information"
                exit 1
            fi
            shift
            ;;
    esac
done

# Set runtime to default if not specified
RUNTIME=${RUNTIME:-$DEFAULT_RUNTIME}

# Set paths based on build type
BIN_DIR="${PROJECT_ROOT}/bin/${BUILD_TYPE}"
RESULTS_DIR="${PROJECT_ROOT}/test_results/valgrind/runtime_cache"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
EXECUTABLE="${BIN_DIR}/VoidLight_Template"

# Create results directory
mkdir -p "${RESULTS_DIR}"

# Result files (include build type in filename)
LOG_FILE="${RESULTS_DIR}/cache_analysis_${BUILD_TYPE}_${TIMESTAMP}.log"
CACHEGRIND_OUT="${RESULTS_DIR}/cachegrind_${BUILD_TYPE}.out.${TIMESTAMP}"
SUMMARY_FILE="${RESULTS_DIR}/cache_summary_${BUILD_TYPE}_${TIMESTAMP}.txt"
REPORT_FILE="${RESULTS_DIR}/cache_report_${BUILD_TYPE}_${TIMESTAMP}.md"
ANNOTATE_FILE="${RESULTS_DIR}/cache_annotate_${BUILD_TYPE}_${TIMESTAMP}.txt"

# Industry benchmark thresholds (percentage miss rates)
# Sources:
#   - Intel VTune documentation (memory access analysis)
#   - SPEC CPU2017 characterization study (NIH PMC6675054)
#   - Branch prediction research (arxiv:1906.08170)
#   - Modern processor analysis showing L1 latency ~3-4 cycles,
#     L2 ~10-20 cycles, LLC ~40-70 cycles, RAM ~150-300 cycles
#
# L1 instruction cache: Very efficient for most code, misses rarely costly
# L1 data cache: More variable; 1-5% typical for compute-bound workloads
# LLC: Misses go to RAM (150+ cycles), keep very low
# Branch: Modern predictors achieve <2%, historical average 5-10%
declare -A BENCHMARKS=(
    ["l1i_excellent"]="1.0"
    ["l1i_good"]="2.0"
    ["l1i_average"]="5.0"
    ["l1d_excellent"]="3.0"
    ["l1d_good"]="5.0"
    ["l1d_average"]="10.0"
    ["ll_excellent"]="0.5"
    ["ll_good"]="1.0"
    ["ll_average"]="3.0"
    ["branch_excellent"]="2.0"
    ["branch_good"]="5.0"
    ["branch_average"]="10.0"
)

# Format build type for display
BUILD_TYPE_UPPER=$(echo "${BUILD_TYPE}" | tr '[:lower:]' '[:upper:]')
case "${BUILD_TYPE}" in
    "profile") BUILD_COLOR="${CYAN}" ;;
    *) BUILD_COLOR="${YELLOW}" ;;  # debug
esac

echo -e "${BOLD}${PURPLE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${PURPLE}║         SDL3 VoidLight-Framework Runtime Cache Analysis            ║${NC}"
echo -e "${BOLD}${PURPLE}║                                                              ║${NC}"
echo -e "${BOLD}${PURPLE}║  Extended duration cache performance profiling              ║${NC}"
echo -e "${BOLD}${PURPLE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BOLD}Build Type: ${BUILD_COLOR}${BUILD_TYPE_UPPER}${NC}"
echo -e "${CYAN}Analysis Timestamp: ${TIMESTAMP}${NC}"
echo -e "${CYAN}Runtime Duration: ${RUNTIME} seconds ($(($RUNTIME / 60)) minutes)${NC}"
echo -e "${CYAN}Executable: ${EXECUTABLE}${NC}"
echo -e "${CYAN}System: $(uname -s -r -m)${NC}"
echo -e "${CYAN}CPU: $(grep "model name" /proc/cpuinfo 2>/dev/null | head -1 | cut -d: -f2 | xargs || echo "Unknown")${NC}"
echo -e "${CYAN}Valgrind: $(valgrind --version 2>/dev/null || echo "Not found")${NC}"
echo ""

# Verify prerequisites
if [[ ! -f "${EXECUTABLE}" ]]; then
    echo -e "${RED}ERROR: Main executable not found: ${EXECUTABLE}${NC}"
    echo -e "${YELLOW}Please build the project first: ninja -C build${NC}"
    exit 1
fi

if ! command -v valgrind &> /dev/null; then
    echo -e "${RED}ERROR: Valgrind is not installed${NC}"
    exit 1
fi

echo -e "${YELLOW}Starting runtime cache analysis...${NC}"
echo -e "${YELLOW}The application will run for ${RUNTIME} seconds under Cachegrind.${NC}"
echo -e "${YELLOW}Note: Application will run ~20-100x slower under Cachegrind.${NC}"
echo ""

# Build cachegrind options
CACHEGRIND_OPTS=""
CACHEGRIND_OPTS+="--tool=cachegrind "
CACHEGRIND_OPTS+="--cache-sim=yes "
CACHEGRIND_OPTS+="--branch-sim=yes "
CACHEGRIND_OPTS+="--cachegrind-out-file=${CACHEGRIND_OUT} "

# Run the application under cachegrind with timeout
echo -e "${CYAN}Running: timeout ${RUNTIME}s valgrind --tool=cachegrind ${EXECUTABLE}${NC}"
echo ""

START_TIME=$(date +%s)

timeout ${RUNTIME}s valgrind ${CACHEGRIND_OPTS} \
    --log-file="${LOG_FILE}" \
    "${EXECUTABLE}" 2>&1 || true

END_TIME=$(date +%s)
ACTUAL_RUNTIME=$((END_TIME - START_TIME))

echo ""
echo -e "${GREEN}Application ran for ${ACTUAL_RUNTIME} seconds${NC}"
echo ""

# Parse results
if [[ ! -f "${LOG_FILE}" ]]; then
    echo -e "${RED}ERROR: Cachegrind log file not generated${NC}"
    exit 1
fi

# Extract cache statistics from log file
echo -e "${CYAN}Parsing cache statistics...${NC}"

# Function to extract numeric value from cachegrind output (handles commas)
extract_num() {
    local pattern="$1"
    local file="$2"
    grep "${pattern}" "${file}" 2>/dev/null | head -1 | awk '{for(i=1;i<=NF;i++) if($i ~ /^[0-9,]+$/) {gsub(/,/,"",$i); print $i; exit}}' || echo "0"
}

# Function to extract percentage from cachegrind output
extract_rate() {
    local pattern="$1"
    local file="$2"
    grep "${pattern}" "${file}" 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "0"
}

# Extract instruction cache stats
I_REFS=$(extract_num "I refs:" "${LOG_FILE}")
I1_MISSES=$(extract_num "I1  misses:" "${LOG_FILE}")
LLI_MISSES=$(extract_num "LLi misses:" "${LOG_FILE}")
I1_MISS_RATE=$(extract_rate "I1  miss rate:" "${LOG_FILE}")
LLI_MISS_RATE=$(extract_rate "LLi miss rate:" "${LOG_FILE}")

# Extract data cache stats
D_REFS=$(extract_num "D refs:" "${LOG_FILE}")
D1_MISSES=$(extract_num "D1  misses:" "${LOG_FILE}")
LLD_MISSES=$(extract_num "LLd misses:" "${LOG_FILE}")
D1_MISS_RATE=$(extract_rate "D1  miss rate:" "${LOG_FILE}")
LLD_MISS_RATE=$(extract_rate "LLd miss rate:" "${LOG_FILE}")

# Extract branch stats
BRANCHES=$(extract_num "Branches:" "${LOG_FILE}")
BRANCH_MISSES=$(extract_num "Mispredicts:" "${LOG_FILE}")
BRANCH_MISS_RATE=$(extract_rate "Mispred rate:" "${LOG_FILE}")

# Extract LL (Last Level) stats
LL_REFS=$(extract_num "LL refs:" "${LOG_FILE}")
LL_MISSES=$(extract_num "LL misses:" "${LOG_FILE}")
LL_MISS_RATE=$(extract_rate "LL miss rate:" "${LOG_FILE}")

# Ensure we have valid numbers (fallback defaults)
[[ -z "${I_REFS}" || "${I_REFS}" == "0" ]] && I_REFS="1"
[[ -z "${D_REFS}" || "${D_REFS}" == "0" ]] && D_REFS="1"
[[ -z "${I1_MISS_RATE}" ]] && I1_MISS_RATE="0"
[[ -z "${D1_MISS_RATE}" ]] && D1_MISS_RATE="0"
[[ -z "${LL_MISS_RATE}" ]] && LL_MISS_RATE="0"
[[ -z "${BRANCH_MISS_RATE}" ]] && BRANCH_MISS_RATE="0"

# Calculate combined stats
TOTAL_REFS=$((I_REFS + D_REFS))
TOTAL_LL_MISSES=$((LLI_MISSES + LLD_MISSES))

# Function to assess performance level (using awk for portability)
assess_level() {
    local rate=$1
    local excellent=$2
    local good=$3
    local average=$4

    # Handle empty or invalid rates
    [[ -z "${rate}" || "${rate}" == "0" ]] && rate="0.0"

    awk -v rate="${rate}" -v exc="${excellent}" -v good="${good}" -v avg="${average}" 'BEGIN {
        if (rate <= exc) print "EXCELLENT"
        else if (rate <= good) print "GOOD"
        else if (rate <= avg) print "AVERAGE"
        else print "POOR"
    }'
}

# Calculate MPKI (Misses Per Kilo Instructions) - industry standard metric
# MPKI = (misses / instructions) * 1000
# This is more meaningful than miss rate because it accounts for instruction efficiency
calculate_mpki() {
    local misses=$1
    local instructions=$2
    [[ -z "${misses}" || "${misses}" == "0" ]] && misses=0
    [[ -z "${instructions}" || "${instructions}" == "0" ]] && { echo "0.00"; return; }
    awk -v m="${misses}" -v i="${instructions}" 'BEGIN { printf "%.2f", (m / i) * 1000 }'
}

I1_MPKI=$(calculate_mpki "${I1_MISSES}" "${I_REFS}")
D1_MPKI=$(calculate_mpki "${D1_MISSES}" "${D_REFS}")
LL_MPKI=$(calculate_mpki "${TOTAL_LL_MISSES}" "$((I_REFS + D_REFS))")
BRANCH_MPKI=$(calculate_mpki "${BRANCH_MISSES}" "${BRANCHES}")

# Assess each cache level (still useful for quick overview)
I1_LEVEL=$(assess_level "${I1_MISS_RATE}" "${BENCHMARKS[l1i_excellent]}" "${BENCHMARKS[l1i_good]}" "${BENCHMARKS[l1i_average]}")
D1_LEVEL=$(assess_level "${D1_MISS_RATE}" "${BENCHMARKS[l1d_excellent]}" "${BENCHMARKS[l1d_good]}" "${BENCHMARKS[l1d_average]}")
LL_LEVEL=$(assess_level "${LL_MISS_RATE}" "${BENCHMARKS[ll_excellent]}" "${BENCHMARKS[ll_good]}" "${BENCHMARKS[ll_average]}")
BRANCH_LEVEL=$(assess_level "${BRANCH_MISS_RATE}" "${BENCHMARKS[branch_excellent]}" "${BENCHMARKS[branch_good]}" "${BENCHMARKS[branch_average]}")

# Generate cg_annotate output if available
if command -v cg_annotate &> /dev/null && [[ -f "${CACHEGRIND_OUT}" ]]; then
    echo -e "${CYAN}Generating annotated output...${NC}"
    cg_annotate --auto=yes "${CACHEGRIND_OUT}" > "${ANNOTATE_FILE}" 2>/dev/null || true
fi

# Determine overall assessment
OVERALL_ASSESSMENT="UNKNOWN"
EXCELLENT_COUNT=0
GOOD_COUNT=0
POOR_COUNT=0

for level in "${I1_LEVEL}" "${D1_LEVEL}" "${LL_LEVEL}" "${BRANCH_LEVEL}"; do
    case "${level}" in
        "EXCELLENT") EXCELLENT_COUNT=$((EXCELLENT_COUNT + 1)) ;;
        "GOOD") GOOD_COUNT=$((GOOD_COUNT + 1)) ;;
        "POOR") POOR_COUNT=$((POOR_COUNT + 1)) ;;
    esac
done

if [[ ${EXCELLENT_COUNT} -ge 3 ]]; then
    OVERALL_ASSESSMENT="WORLD_CLASS"
elif [[ ${EXCELLENT_COUNT} -ge 2 && ${POOR_COUNT} -eq 0 ]]; then
    OVERALL_ASSESSMENT="EXCELLENT"
elif [[ ${POOR_COUNT} -eq 0 ]]; then
    OVERALL_ASSESSMENT="GOOD"
elif [[ ${POOR_COUNT} -le 1 ]]; then
    OVERALL_ASSESSMENT="ACCEPTABLE"
else
    OVERALL_ASSESSMENT="NEEDS_OPTIMIZATION"
fi

# Format large numbers with commas for readability
format_number() {
    echo "$1" | sed ':a;s/\B[0-9]\{3\}\>/,&/;ta'
}

I_REFS_FMT=$(format_number "${I_REFS}")
D_REFS_FMT=$(format_number "${D_REFS}")
I1_MISSES_FMT=$(format_number "${I1_MISSES}")
D1_MISSES_FMT=$(format_number "${D1_MISSES}")
LL_MISSES_FMT=$(format_number "${TOTAL_LL_MISSES}")
BRANCH_MISSES_FMT=$(format_number "${BRANCH_MISSES}")

# Generate summary file (different format for profile vs debug)
if [[ "${BUILD_TYPE}" == "profile" ]]; then
    cat > "${SUMMARY_FILE}" << EOF
SDL3 VoidLight-Framework Runtime Cache Analysis Summary
================================================
Timestamp: $(date)
Runtime: ${ACTUAL_RUNTIME} seconds
Build: ${BUILD_TYPE_UPPER}
Executable: ${EXECUTABLE}

KEY PERFORMANCE METRICS (What Actually Matters):
================================================
Total Instructions Executed: ${I_REFS_FMT}
Total Data References:       ${D_REFS_FMT}

ABSOLUTE CACHE MISSES (Lower = Better):
  L1 Instruction Misses: ${I1_MISSES_FMT}
  L1 Data Misses:        ${D1_MISSES_FMT}
  LLC Misses (RAM hits): ${LL_MISSES_FMT}  <-- Most expensive!
  Branch Mispredictions: ${BRANCH_MISSES_FMT}

MISSES PER 1000 INSTRUCTIONS (MPKI - Industry Standard):
  L1i MPKI: ${I1_MPKI}
  L1d MPKI: ${D1_MPKI}
  LLC MPKI: ${LL_MPKI}  <-- Each miss = ~200 CPU cycles
  Branch MPKI: ${BRANCH_MPKI}

MISS RATES (Less Meaningful for Optimized Builds):
  L1i Miss Rate: ${I1_MISS_RATE}%
  L1d Miss Rate: ${D1_MISS_RATE}%
  LLC Miss Rate: ${LL_MISS_RATE}%
  Branch Miss Rate: ${BRANCH_MISS_RATE}%

OVERALL ASSESSMENT: ${OVERALL_ASSESSMENT}
EOF
else
    cat > "${SUMMARY_FILE}" << EOF
SDL3 VoidLight-Framework Runtime Cache Analysis Summary
================================================
Timestamp: $(date)
Runtime: ${ACTUAL_RUNTIME} seconds
Build: ${BUILD_TYPE_UPPER}
Executable: ${EXECUTABLE}

CACHE PERFORMANCE:
==================
Total Instructions: ${I_REFS_FMT}
Total Data Refs:    ${D_REFS_FMT}

MISS RATES:
  L1 Instruction: ${I1_MISS_RATE}%
  L1 Data:        ${D1_MISS_RATE}%
  Last Level:     ${LL_MISS_RATE}%
  Branch:         ${BRANCH_MISS_RATE}%

OVERALL ASSESSMENT: ${OVERALL_ASSESSMENT}
EOF
fi

# Generate markdown report (different format for profile vs debug)
if [[ "${BUILD_TYPE}" == "profile" ]]; then
    cat > "${REPORT_FILE}" << EOF
# SDL3 VoidLight-Framework Runtime Cache Analysis Report

**Build Type**: ${BUILD_TYPE_UPPER}
**Generated**: $(date)
**Runtime Duration**: ${ACTUAL_RUNTIME} seconds ($(($ACTUAL_RUNTIME / 60)) minutes)
**Executable**: \`${EXECUTABLE}\`
**Valgrind Version**: $(valgrind --version 2>/dev/null)
**CPU**: $(grep "model name" /proc/cpuinfo 2>/dev/null | head -1 | cut -d: -f2 | xargs || echo "Unknown")

## Key Metrics (What Actually Matters)

| Metric | Value | Why It Matters |
|--------|-------|----------------|
| **Instructions Executed** | ${I_REFS_FMT} | Lower = more efficient code |
| **LLC Misses (RAM hits)** | ${LL_MISSES_FMT} | Each miss costs ~200 CPU cycles |
| **L1 Data Misses** | ${D1_MISSES_FMT} | Data access bottlenecks |
| **Branch Mispredictions** | ${BRANCH_MISSES_FMT} | Pipeline stalls |

## MPKI (Misses Per 1000 Instructions) - Industry Standard

| Cache Level | Total Misses | MPKI | Miss Rate |
|-------------|--------------|------|-----------|
| **L1 Instruction** | ${I1_MISSES_FMT} | ${I1_MPKI} | ${I1_MISS_RATE}% |
| **L1 Data** | ${D1_MISSES_FMT} | ${D1_MPKI} | ${D1_MISS_RATE}% |
| **Last Level (LLC)** | ${LL_MISSES_FMT} | ${LL_MPKI} | ${LL_MISS_RATE}% |
| **Branch Prediction** | ${BRANCH_MISSES_FMT} | ${BRANCH_MPKI} | ${BRANCH_MISS_RATE}% |

> **Note**: MPKI is more meaningful than miss rate for optimized builds.

## Overall Assessment: **${OVERALL_ASSESSMENT}**

EOF
else
    cat > "${REPORT_FILE}" << EOF
# SDL3 VoidLight-Framework Runtime Cache Analysis Report

**Build Type**: ${BUILD_TYPE_UPPER}
**Generated**: $(date)
**Runtime Duration**: ${ACTUAL_RUNTIME} seconds ($(($ACTUAL_RUNTIME / 60)) minutes)
**Executable**: \`${EXECUTABLE}\`
**Valgrind Version**: $(valgrind --version 2>/dev/null)
**CPU**: $(grep "model name" /proc/cpuinfo 2>/dev/null | head -1 | cut -d: -f2 | xargs || echo "Unknown")

## Cache Performance Summary

| Cache Level | Miss Rate | Assessment |
|-------------|-----------|------------|
| **L1 Instruction** | ${I1_MISS_RATE}% | ${I1_LEVEL} |
| **L1 Data** | ${D1_MISS_RATE}% | ${D1_LEVEL} |
| **Last Level (LLC)** | ${LL_MISS_RATE}% | ${LL_LEVEL} |
| **Branch Prediction** | ${BRANCH_MISS_RATE}% | ${BRANCH_LEVEL} |

## Overall Assessment: **${OVERALL_ASSESSMENT}**

EOF
fi

# Add assessment description
case "${OVERALL_ASSESSMENT}" in
    "WORLD_CLASS")
        cat >> "${REPORT_FILE}" << EOF
**EXCELLENT CACHE PERFORMANCE**

This run shows very low cache miss rates and strong branch prediction. Treat it
as current-run evidence only; compare against project baselines before making
regression or release claims.
EOF
        ;;
    "EXCELLENT")
        cat >> "${REPORT_FILE}" << EOF
🎯 **EXCELLENT CACHE PERFORMANCE**

The application shows outstanding cache behavior:
- Very low cache miss rates across all levels
- Well-optimized memory access patterns
- Good branch prediction
- Exceeds industry standards
EOF
        ;;
    "GOOD")
        cat >> "${REPORT_FILE}" << EOF
✅ **GOOD CACHE PERFORMANCE**

The application has solid cache efficiency:
- Cache miss rates within acceptable ranges
- No major performance bottlenecks
- Suitable for production use
EOF
        ;;
    "ACCEPTABLE")
        cat >> "${REPORT_FILE}" << EOF
⚠️ **ACCEPTABLE CACHE PERFORMANCE**

The application has reasonable cache behavior:
- Some areas could benefit from optimization
- Consider profiling hotspots for improvement
- Review data access patterns
EOF
        ;;
    *)
        cat >> "${REPORT_FILE}" << EOF
🔧 **OPTIMIZATION RECOMMENDED**

The application shows cache performance issues:
- High miss rates detected in one or more cache levels
- Review memory access patterns
- Consider data structure reorganization
- Profile hotspots using cg_annotate
EOF
        ;;
esac

cat >> "${REPORT_FILE}" << EOF

## Detailed Analysis

### L1 Instruction Cache
- **References**: ${I_REFS}
- **L1 Misses**: ${I1_MISSES} (${I1_MISS_RATE}%)
- **LLC Misses**: ${LLI_MISSES} (${LLI_MISS_RATE}%)
- **Assessment**: ${I1_LEVEL}

**Industry Benchmarks**:
- Excellent: < ${BENCHMARKS[l1i_excellent]}%
- Good: < ${BENCHMARKS[l1i_good]}%
- Average: < ${BENCHMARKS[l1i_average]}%

### L1 Data Cache
- **References**: ${D_REFS}
- **L1 Misses**: ${D1_MISSES} (${D1_MISS_RATE}%)
- **LLC Misses**: ${LLD_MISSES} (${LLD_MISS_RATE}%)
- **Assessment**: ${D1_LEVEL}

**Industry Benchmarks**:
- Excellent: < ${BENCHMARKS[l1d_excellent]}%
- Good: < ${BENCHMARKS[l1d_good]}%
- Average: < ${BENCHMARKS[l1d_average]}%

### Last Level Cache (LLC)
- **Total References**: ${TOTAL_REFS}
- **Total Misses**: ${TOTAL_LL_MISSES} (${LL_MISS_RATE}%)
- **Assessment**: ${LL_LEVEL}

**Industry Benchmarks**:
- Excellent: < ${BENCHMARKS[ll_excellent]}%
- Good: < ${BENCHMARKS[ll_good]}%
- Average: < ${BENCHMARKS[ll_average]}%

### Branch Prediction
- **Branches**: ${BRANCHES}
- **Mispredictions**: ${BRANCH_MISSES} (${BRANCH_MISS_RATE}%)
- **Assessment**: ${BRANCH_LEVEL}

**Industry Benchmarks**:
- Excellent: < ${BENCHMARKS[branch_excellent]}%
- Good: < ${BENCHMARKS[branch_good]}%
- Average: < ${BENCHMARKS[branch_average]}%

## Performance Hotspot Analysis

To identify specific functions with high cache miss rates:

\`\`\`bash
# View annotated output
cat ${ANNOTATE_FILE}

# Or use cg_annotate directly
cg_annotate --auto=yes ${CACHEGRIND_OUT}

# Sort by D1 cache misses
cg_annotate --sort=D1mr ${CACHEGRIND_OUT}

# Sort by instruction cache misses
cg_annotate --sort=I1mr ${CACHEGRIND_OUT}
\`\`\`

## Files Generated

- **Cachegrind Output**: \`${CACHEGRIND_OUT}\`
- **Annotated Analysis**: \`${ANNOTATE_FILE}\`
- **Summary**: \`${SUMMARY_FILE}\`
- **This Report**: \`${REPORT_FILE}\`
- **Raw Log**: \`${LOG_FILE}\`

## Usage

\`\`\`bash
# Run with default 3-minute duration
./tests/valgrind/runtime_cache_analysis.sh

# Run with custom duration (in seconds)
./tests/valgrind/runtime_cache_analysis.sh 300  # 5 minutes
./tests/valgrind/runtime_cache_analysis.sh 600  # 10 minutes
\`\`\`

## Optimization Recommendations

EOF

# Add specific recommendations based on results
if [[ "${I1_LEVEL}" == "POOR" || "${I1_LEVEL}" == "AVERAGE" ]]; then
    cat >> "${REPORT_FILE}" << EOF
### Instruction Cache Optimization
- Review code layout and function ordering
- Consider using \`__attribute__((hot))\` for frequently called functions
- Minimize code bloat from templates and inlining
- Check for instruction cache thrashing in hot loops

EOF
fi

if [[ "${D1_LEVEL}" == "POOR" || "${D1_LEVEL}" == "AVERAGE" ]]; then
    cat >> "${REPORT_FILE}" << EOF
### Data Cache Optimization
- Review data structure layout (cache line alignment)
- Consider Structure of Arrays (SoA) vs Array of Structures (AoS)
- Minimize pointer chasing
- Use contiguous memory allocation
- Implement data prefetching for predictable access patterns

EOF
fi

if [[ "${LL_LEVEL}" == "POOR" || "${LL_LEVEL}" == "AVERAGE" ]]; then
    cat >> "${REPORT_FILE}" << EOF
### Last Level Cache Optimization
- Review working set size
- Implement cache blocking/tiling for large data
- Consider memory pool allocation
- Optimize memory allocation patterns

EOF
fi

if [[ "${BRANCH_LEVEL}" == "POOR" || "${BRANCH_LEVEL}" == "AVERAGE" ]]; then
    cat >> "${REPORT_FILE}" << EOF
### Branch Prediction Optimization
- Use branchless programming techniques where possible
- Consider \`__builtin_expect()\` for predictable branches
- Replace conditionals with lookup tables
- Organize hot/cold code paths

EOF
fi

cat >> "${REPORT_FILE}" << EOF

---
*Generated by SDL3 VoidLight-Framework Runtime Cache Analysis Suite*
EOF

# Display results
echo -e "${BOLD}${PURPLE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}${PURPLE}                    CACHE ANALYSIS RESULTS                     ${NC}"
echo -e "${BOLD}${PURPLE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Function to color-code assessment
color_level() {
    local level=$1
    case "${level}" in
        "EXCELLENT") echo -e "${GREEN}${level}${NC}" ;;
        "GOOD") echo -e "${CYAN}${level}${NC}" ;;
        "AVERAGE") echo -e "${YELLOW}${level}${NC}" ;;
        "POOR") echo -e "${RED}${level}${NC}" ;;
        *) echo "${level}" ;;
    esac
}

if [[ "${BUILD_TYPE}" == "profile" ]]; then
    # Profile build: Show MPKI analysis (more meaningful for optimized code)
    echo -e "${BOLD}Key Metrics (Instructions Executed: ${I_REFS_FMT}):${NC}"
    echo ""
    printf "  %-18s %15s %10s %10s\n" "Metric" "Absolute Misses" "MPKI" "Grade"
    echo "  ─────────────────────────────────────────────────────────────"
    printf "  %-18s %15s %10s  " "L1 Instruction" "${I1_MISSES_FMT}" "${I1_MPKI}"
    color_level "${I1_LEVEL}"
    printf "  %-18s %15s %10s  " "L1 Data" "${D1_MISSES_FMT}" "${D1_MPKI}"
    color_level "${D1_LEVEL}"
    printf "  %-18s %15s %10s  " "LLC (→ RAM)" "${LL_MISSES_FMT}" "${LL_MPKI}"
    color_level "${LL_LEVEL}"
    printf "  %-18s %15s %10s  " "Branch Mispredict" "${BRANCH_MISSES_FMT}" "${BRANCH_MPKI}"
    color_level "${BRANCH_LEVEL}"
    echo ""
    echo -e "  ${CYAN}MPKI = Misses Per 1000 Instructions (lower = better)${NC}"
    echo -e "  ${CYAN}LLC misses are most expensive (~200 CPU cycles each)${NC}"
    echo ""
else
    # Debug build: Show traditional miss rate analysis
    echo -e "${BOLD}Cache Performance Summary:${NC}"
    echo ""
    printf "  %-20s %15s %12s\n" "Cache Level" "Miss Rate" "Assessment"
    echo "  ────────────────────────────────────────────────────────"
    printf "  %-20s %14s%% " "L1 Instruction" "${I1_MISS_RATE}"
    color_level "${I1_LEVEL}"
    printf "  %-20s %14s%% " "L1 Data" "${D1_MISS_RATE}"
    color_level "${D1_LEVEL}"
    printf "  %-20s %14s%% " "Last Level (LLC)" "${LL_MISS_RATE}"
    color_level "${LL_LEVEL}"
    printf "  %-20s %14s%% " "Branch Prediction" "${BRANCH_MISS_RATE}"
    color_level "${BRANCH_LEVEL}"
    echo ""
fi

# Overall assessment with color
echo -e "${BOLD}Overall Assessment:${NC}"
RESULT_STATUS="REVIEW"
RESULT_ACTION="inspect_runtime_cache_report"
if [[ "${OVERALL_ASSESSMENT}" == "WORLD_CLASS" || "${OVERALL_ASSESSMENT}" == "EXCELLENT" || "${OVERALL_ASSESSMENT}" == "GOOD" ]]; then
    RESULT_STATUS="PASS"
    RESULT_ACTION="none"
elif [[ "${OVERALL_ASSESSMENT}" == "NEEDS_OPTIMIZATION" ]]; then
    RESULT_STATUS="FAIL"
    RESULT_ACTION="review_runtime_cache_hotspots"
elif [[ "${OVERALL_ASSESSMENT}" == "ACCEPTABLE" ]]; then
    RESULT_ACTION="compare_against_cache_baseline"
fi

{
    echo ""
    echo "## Automated Result"
    echo ""
    echo "Result: **${RESULT_STATUS,,}**"
    echo "Action: **${RESULT_ACTION}**"
} >> "${REPORT_FILE}"

case "${OVERALL_ASSESSMENT}" in
    "WORLD_CLASS")
        echo -e "${BOLD}${GREEN}EXCELLENT CACHE PERFORMANCE${NC}"
        echo -e "${GREEN}Very low cache miss rates in this run.${NC}"
        ;;
    "EXCELLENT")
        echo -e "${BOLD}${GREEN}🎯 EXCELLENT CACHE PERFORMANCE${NC}"
        echo -e "${GREEN}Outstanding cache behavior across all levels.${NC}"
        ;;
    "GOOD")
        echo -e "${BOLD}${CYAN}✅ GOOD CACHE PERFORMANCE${NC}"
        echo -e "${CYAN}Solid cache efficiency suitable for production.${NC}"
        ;;
    "ACCEPTABLE")
        echo -e "${BOLD}${YELLOW}⚠️ ACCEPTABLE CACHE PERFORMANCE${NC}"
        echo -e "${YELLOW}Some optimization opportunities available.${NC}"
        ;;
    *)
        echo -e "${BOLD}${RED}🔧 OPTIMIZATION RECOMMENDED${NC}"
        echo -e "${RED}Review cache performance hotspots.${NC}"
        ;;
esac

case "${RESULT_STATUS}" in
    "PASS")
        echo -e "Result: ${GREEN}PASS${NC}"
        ;;
    "FAIL")
        echo -e "Result: ${RED}FAIL${NC}"
        ;;
    *)
        echo -e "Result: ${YELLOW}REVIEW${NC}"
        ;;
esac
echo "Action: ${RESULT_ACTION}"
echo ""

echo ""
echo -e "${BOLD}Output Files:${NC}"
echo -e "  📊 Report:     ${CYAN}${REPORT_FILE}${NC}"
echo -e "  📋 Summary:    ${CYAN}${SUMMARY_FILE}${NC}"
echo -e "  📈 Cachegrind: ${CYAN}${CACHEGRIND_OUT}${NC}"
if [[ -f "${ANNOTATE_FILE}" ]]; then
    echo -e "  🔍 Annotated:  ${CYAN}${ANNOTATE_FILE}${NC}"
fi
echo -e "  📝 Log:        ${CYAN}${LOG_FILE}${NC}"
echo ""
echo -e "${BOLD}${PURPLE}Runtime cache analysis complete!${NC}"
