#!/bin/bash

# VoidLight-Framework - Callgrind function profiling
# Produces call graph data and summaries for selected performance categories.

set -u

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${PROJECT_ROOT}/tests/valgrind/valgrind_targets.sh"

BUILD_TYPE=""
TIMEOUT_SECONDS=900
CATEGORIES=()
TARGETS=()

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

show_help() {
    cat <<EOF
Usage: $0 [options] [category ...]

Categories:
  ai
  events
  resources
  collision_pathfinding
  runtime_managers
  render_ui
  benchmarks
  all

Options:
  --debug              Use bin/debug test executables
  --profile            Use bin/profile test executables
  --target <name>      Run one executable directly
  --timeout <seconds>  Per-target timeout (default: ${TIMEOUT_SECONDS})
  --help, -h           Show this help

By default this script uses bin/profile when it exists, otherwise bin/debug.
Callgrind is for function-level performance analysis, not memory correctness.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            BUILD_TYPE="debug"
            shift
            ;;
        --profile)
            BUILD_TYPE="profile"
            shift
            ;;
        --target)
            if [[ $# -lt 2 ]]; then
                echo -e "${RED}ERROR: --target requires an executable name${NC}" >&2
                exit 2
            fi
            TARGETS+=("$2")
            shift 2
            ;;
        --timeout)
            if [[ $# -lt 2 || ! "$2" =~ ^[0-9]+$ ]]; then
                echo -e "${RED}ERROR: --timeout requires seconds${NC}" >&2
                exit 2
            fi
            TIMEOUT_SECONDS="$2"
            shift 2
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        ai|events|resources|collision_pathfinding|runtime_managers|render_ui|benchmarks|all)
            CATEGORIES+=("$1")
            shift
            ;;
        *)
            echo -e "${RED}ERROR: Unknown category or option: $1${NC}" >&2
            show_help
            exit 2
            ;;
    esac
done

if [[ -z "${BUILD_TYPE}" ]]; then
    if [[ -d "${PROJECT_ROOT}/bin/profile" ]]; then
        BUILD_TYPE="profile"
    else
        BUILD_TYPE="debug"
    fi
fi

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    if [[ ${#CATEGORIES[@]} -eq 0 ]]; then
        CATEGORIES=(ai)
    fi

    for category in "${CATEGORIES[@]}"; do
        while IFS= read -r target; do
            [[ -n "${target}" ]] && TARGETS+=("${target}")
        done < <(valgrind_targets_for_callgrind_category "${category}") || {
            echo -e "${RED}ERROR: unknown Callgrind category: ${category}${NC}" >&2
            exit 2
        }
    done
fi

# De-duplicate while preserving category order.
DEDUPED_TARGETS=()
for target in "${TARGETS[@]}"; do
    seen=false
    for existing in "${DEDUPED_TARGETS[@]}"; do
        if [[ "${existing}" == "${target}" ]]; then
            seen=true
            break
        fi
    done
    [[ "${seen}" == false ]] && DEDUPED_TARGETS+=("${target}")
done
TARGETS=("${DEDUPED_TARGETS[@]}")

BIN_DIR="${PROJECT_ROOT}/bin/${BUILD_TYPE}"
RESULTS_DIR="${PROJECT_ROOT}/test_results/valgrind/callgrind"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
REPORT_FILE="${RESULTS_DIR}/callgrind_report_${BUILD_TYPE}_${TIMESTAMP}.md"
mkdir -p "${RESULTS_DIR}/raw" "${RESULTS_DIR}/summaries" "${RESULTS_DIR}/annotations"

if ! command -v valgrind >/dev/null 2>&1; then
    echo -e "${RED}ERROR: valgrind is not installed${NC}" >&2
    exit 2
fi

if [[ ! -d "${BIN_DIR}" ]]; then
    echo -e "${RED}ERROR: build output directory not found: ${BIN_DIR}${NC}" >&2
    exit 2
fi

{
    echo "# Callgrind Profiling Report"
    echo ""
    echo "- Generated: $(date)"
    echo "- Build: ${BUILD_TYPE}"
    echo "- Tool: $(valgrind --version 2>/dev/null || echo unknown)"
    echo ""
    echo "| Target | Status | Instructions | Top hotspot | Action | Raw output | Summary |"
    echo "|---|---:|---:|---|---|---|---|"
} > "${REPORT_FILE}"

echo -e "${BOLD}VoidLight Callgrind profiling${NC}"
echo -e "Build: ${CYAN}${BUILD_TYPE}${NC}"
echo -e "Targets: ${CYAN}${#TARGETS[@]}${NC}"
echo -e "Report: ${CYAN}${REPORT_FILE}${NC}"
echo ""

passed=0
failed=0
missing=0
timed_out=0

benchmark_args_for() {
    case "$1" in
        ai_scaling_benchmark)
            echo "--run_test=AIScalingTests/AIEntityScaling"
            ;;
        collision_scaling_benchmark)
            echo "--run_test=CollisionScalingTests/MMScaling"
            ;;
        projectile_scaling_benchmark)
            echo "--run_test=ProjectileScalingTests/ProjectileScaling"
            ;;
        event_manager_scaling_benchmark)
            echo "--run_test=ComprehensiveScalabilityTest"
            ;;
        integrated_system_benchmark)
            echo "--run_test=IntegratedSystemBenchmarkSuite/TestRealisticGameSimulation60FPS"
            ;;
        *)
            echo ""
            ;;
    esac
}

top_hotspot() {
    local summary_file="$1"
    grep -E "^[[:space:]]*[0-9]" "${summary_file}" 2>/dev/null \
        | grep -v -E "PROGRAM TOTALS|ld-linux|libc\.so|libstdc\+\+|BOOST_|boost::unit_test|test_main" \
        | head -1 \
        | sed 's/^[[:space:]]*//' \
        | sed 's/|/ /g' \
        || true
}

run_target() {
    local executable="$1"
    local exe_path="${BIN_DIR}/${executable}"
    local out_file="${RESULTS_DIR}/raw/${executable}.callgrind.out"
    local log_file="${RESULTS_DIR}/raw/${executable}.callgrind.log"
    local summary_file="${RESULTS_DIR}/summaries/${executable}_summary.txt"
    local annotation_file="${RESULTS_DIR}/annotations/${executable}_annotation.txt"
    local test_args
    test_args=$(benchmark_args_for "${executable}")

    if [[ ! -x "${exe_path}" ]]; then
        echo -e "${YELLOW}MISSING${NC} callgrind ${executable} path=${exe_path}"
        echo "| ${executable} | missing | N/A | N/A | build_target | | |" >> "${REPORT_FILE}"
        ((missing++))
        return
    fi

    timeout "${TIMEOUT_SECONDS}s" valgrind \
        --tool=callgrind \
        "--callgrind-out-file=${out_file}" \
        --collect-jumps=yes \
        --collect-atstart=yes \
        --instr-atstart=yes \
        --compress-strings=yes \
        --compress-pos=yes \
        --cache-sim=no \
        --branch-sim=no \
        "--log-file=${log_file}" \
        "${exe_path}" ${test_args} >/dev/null 2>&1
    local status=$?

    if [[ ${status} -eq 124 ]]; then
        echo -e "${YELLOW}TIMEOUT${NC} callgrind ${executable} after=${TIMEOUT_SECONDS}s log=${log_file}"
        echo "| ${executable} | timeout | N/A | N/A | rerun_target_with_higher_timeout | ${out_file} | |" >> "${REPORT_FILE}"
        ((timed_out++))
        return
    fi

    if [[ ! -s "${out_file}" ]]; then
        echo -e "${RED}FAIL${NC} callgrind ${executable} reason=no_output log=${log_file}"
        echo "| ${executable} | failed | N/A | N/A | inspect_callgrind_log | ${out_file} | |" >> "${REPORT_FILE}"
        ((failed++))
        return
    fi

    if command -v callgrind_annotate >/dev/null 2>&1; then
        callgrind_annotate --auto=yes --inclusive=yes "${out_file}" > "${summary_file}" 2>/dev/null || true
        callgrind_annotate --auto=yes --threshold=99 "${out_file}" > "${annotation_file}" 2>/dev/null || true
    else
        {
            echo "callgrind_annotate not found."
            echo "Raw output: ${out_file}"
        } > "${summary_file}"
    fi

    local instructions="N/A"
    if [[ -s "${summary_file}" ]]; then
        instructions=$(grep "PROGRAM TOTALS" "${summary_file}" 2>/dev/null | tail -1 | awk '{print $1}' | tr -d ',' || true)
        [[ -n "${instructions}" ]] || instructions="N/A"
    fi

    local hotspot action
    hotspot=$(top_hotspot "${summary_file}")
    if [[ -n "${hotspot}" ]]; then
        action="review_top_hotspot"
    else
        hotspot="N/A"
        action="open_raw_profile"
    fi

    echo -e "${GREEN}PASS${NC} callgrind ${executable} instructions=${instructions} top=\"${hotspot}\" action=${action} raw=${out_file}"
    echo "| ${executable} | complete | ${instructions} | ${hotspot} | ${action} | ${out_file} | ${summary_file} |" >> "${REPORT_FILE}"
    ((passed++))
}

for target in "${TARGETS[@]}"; do
    run_target "${target}"
done

echo ""
echo -e "${BOLD}Callgrind summary${NC}"
echo "  complete: ${passed}"
echo "  failed: ${failed}"
echo "  timeout: ${timed_out}"
echo "  missing: ${missing}"
echo "  report: ${REPORT_FILE}"
echo ""
echo "Use kcachegrind or callgrind_annotate on files in ${RESULTS_DIR}/raw."

{
    echo ""
    echo "## Summary"
    echo ""
    echo "- Complete: ${passed}"
    echo "- Failed: ${failed}"
    echo "- Timeout: ${timed_out}"
    echo "- Missing: ${missing}"
    echo ""
    if [[ ${failed} -gt 0 || ${timed_out} -gt 0 || ${missing} -gt 0 ]]; then
        echo "Result: **fail**"
    else
        echo "Result: **pass**"
    fi
} >> "${REPORT_FILE}"

if [[ ${failed} -gt 0 || ${timed_out} -gt 0 || ${missing} -gt 0 ]]; then
    echo -e "  result: ${RED}FAIL${NC}"
    exit 1
fi

echo -e "  result: ${GREEN}PASS${NC}"
exit 0
