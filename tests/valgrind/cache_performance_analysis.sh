#!/bin/bash

# VoidLight-Framework - Cachegrind performance analysis
# Profiles cache and branch behavior for selected data/performance-heavy tests.

set -u

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${PROJECT_ROOT}/tests/valgrind/valgrind_targets.sh"

BUILD_TYPE=""
TIMEOUT_SECONDS=300
INCLUDE_BENCHMARKS=false
TARGETS=()

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

show_help() {
    cat <<EOF
Usage: $0 [options]

Options:
  --debug              Use bin/debug test executables
  --profile            Use bin/profile test executables
  --benchmarks         Include benchmark executables
  --target <name>      Run one executable name from the target manifest
  --timeout <seconds>  Per-target timeout (default: ${TIMEOUT_SECONDS})
  --help, -h           Show this help

By default this script uses bin/profile when it exists, otherwise bin/debug.
Cachegrind is a performance profiling tool; it is not a memory-safety gate.
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
        --benchmarks)
            INCLUDE_BENCHMARKS=true
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
        *)
            echo -e "${RED}ERROR: Unknown option: $1${NC}" >&2
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
    TARGETS=("${VALGRIND_CACHE_TARGETS[@]}")
    if [[ "${INCLUDE_BENCHMARKS}" == true ]]; then
        TARGETS+=("${VALGRIND_CACHE_BENCHMARK_TARGETS[@]}")
    fi
fi

BIN_DIR="${PROJECT_ROOT}/bin/${BUILD_TYPE}"
RESULTS_DIR="${PROJECT_ROOT}/test_results/valgrind/cachegrind"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
REPORT_FILE="${RESULTS_DIR}/cachegrind_report_${BUILD_TYPE}_${TIMESTAMP}.md"
mkdir -p "${RESULTS_DIR}/raw" "${RESULTS_DIR}/annotations"

if ! command -v valgrind >/dev/null 2>&1; then
    echo -e "${RED}ERROR: valgrind is not installed${NC}" >&2
    exit 2
fi

if [[ ! -d "${BIN_DIR}" ]]; then
    echo -e "${RED}ERROR: build output directory not found: ${BIN_DIR}${NC}" >&2
    exit 2
fi

{
    echo "# Cachegrind Performance Report"
    echo ""
    echo "- Generated: $(date)"
    echo "- Build: ${BUILD_TYPE}"
    echo "- Tool: $(valgrind --version 2>/dev/null || echo unknown)"
    echo ""
    echo "| Target | Status | I1 miss | D1 miss | LL miss | Branch miss | Assessment | Action | Output |"
    echo "|---|---:|---:|---:|---:|---:|---|---|---|"
} > "${REPORT_FILE}"

echo -e "${BOLD}VoidLight Cachegrind analysis${NC}"
echo -e "Build: ${CYAN}${BUILD_TYPE}${NC}"
echo -e "Targets: ${CYAN}${#TARGETS[@]}${NC}"
echo -e "Report: ${CYAN}${REPORT_FILE}${NC}"
echo ""

passed=0
failed=0
missing=0
timed_out=0
review=0

extract_metric() {
    local pattern="$1"
    local log_file="$2"
    local value
    value=$(awk -v pattern="${pattern}" '
        index($0, pattern) {
            line = substr($0, index($0, pattern) + length(pattern))
            if (match(line, /[0-9]+([.][0-9]+)?%/)) {
                value = substr(line, RSTART, RLENGTH - 1)
            }
        }
        END {
            if (value != "") {
                print value
            }
        }
    ' "${log_file}" 2>/dev/null || true)
    [[ -n "${value}" ]] && echo "${value}" || echo "N/A"
}

rate_gt() {
    local value="$1"
    local threshold="$2"
    [[ "${value}" =~ ^[0-9]+([.][0-9]+)?$ ]] || return 1
    awk -v value="${value}" -v threshold="${threshold}" 'BEGIN { exit !((value + 0) > (threshold + 0)) }'
}

cache_assessment() {
    local i1="$1"
    local d1="$2"
    local ll="$3"
    local branch="$4"

    if [[ "${i1}" == "N/A" || "${d1}" == "N/A" || "${ll}" == "N/A" || "${branch}" == "N/A" ]]; then
        echo "review|inspect_cachegrind_metric_parse"
        return
    fi

    if rate_gt "${ll}" "3.0"; then
        echo "review|review_ll_cache_misses"
    elif rate_gt "${d1}" "10.0"; then
        echo "review|review_data_layout_or_iteration_order"
    elif rate_gt "${branch}" "10.0"; then
        echo "review|review_branching_in_hot_path"
    else
        echo "ok|none"
    fi
}

run_target() {
    local executable="$1"
    local exe_path="${BIN_DIR}/${executable}"
    local out_file="${RESULTS_DIR}/raw/${executable}.cachegrind.out"
    local log_file="${RESULTS_DIR}/raw/${executable}.cachegrind.log"
    local annotation_file="${RESULTS_DIR}/annotations/${executable}.cachegrind.txt"

    if [[ ! -x "${exe_path}" ]]; then
        echo -e "${YELLOW}MISSING${NC} cachegrind ${executable} path=${exe_path}"
        echo "| ${executable} | missing | N/A | N/A | N/A | N/A | fail | build_target | |" >> "${REPORT_FILE}"
        ((missing++))
        return
    fi

    timeout "${TIMEOUT_SECONDS}s" valgrind \
        --tool=cachegrind \
        --cache-sim=yes \
        --branch-sim=yes \
        "--cachegrind-out-file=${out_file}" \
        "--log-file=${log_file}" \
        "${exe_path}" >/dev/null 2>&1
    local status=$?

    if [[ ${status} -eq 124 ]]; then
        echo -e "${YELLOW}TIMEOUT${NC} cachegrind ${executable} after=${TIMEOUT_SECONDS}s log=${log_file}"
        echo "| ${executable} | timeout | N/A | N/A | N/A | N/A | fail | rerun_target_with_higher_timeout | ${log_file} |" >> "${REPORT_FILE}"
        ((timed_out++))
        return
    fi

    if [[ ! -s "${out_file}" || ! -s "${log_file}" ]]; then
        echo -e "${RED}FAIL${NC} cachegrind ${executable} reason=no_output log=${log_file}"
        echo "| ${executable} | failed | N/A | N/A | N/A | N/A | fail | inspect_cachegrind_log | ${log_file} |" >> "${REPORT_FILE}"
        ((failed++))
        return
    fi

    if command -v cg_annotate >/dev/null 2>&1; then
        cg_annotate "${out_file}" > "${annotation_file}" 2>/dev/null || true
    fi

    local i1 d1 ll branch
    i1=$(extract_metric "I1  miss rate:" "${log_file}")
    d1=$(extract_metric "D1  miss rate:" "${log_file}")
    ll=$(extract_metric "LL miss rate:" "${log_file}")
    branch=$(extract_metric "Mispred rate:" "${log_file}")

    local assessment action
    IFS='|' read -r assessment action <<< "$(cache_assessment "${i1}" "${d1}" "${ll}" "${branch}")"
    if [[ "${assessment}" == "review" ]]; then
        echo -e "${YELLOW}REVIEW${NC} cachegrind ${executable} I1=${i1}% D1=${d1}% LL=${ll}% branch=${branch}% action=${action} raw=${out_file}"
        ((review++))
    else
        echo -e "${GREEN}PASS${NC} cachegrind ${executable} I1=${i1}% D1=${d1}% LL=${ll}% branch=${branch}% action=${action} raw=${out_file}"
    fi
    echo "| ${executable} | complete | ${i1}% | ${d1}% | ${ll}% | ${branch}% | ${assessment} | ${action} | ${out_file} |" >> "${REPORT_FILE}"
    ((passed++))
}

for target in "${TARGETS[@]}"; do
    run_target "${target}"
done

echo ""
echo -e "${BOLD}Cachegrind summary${NC}"
echo "  complete: ${passed}"
echo "  review: ${review}"
echo "  failed: ${failed}"
echo "  timeout: ${timed_out}"
echo "  missing: ${missing}"
echo "  report: ${REPORT_FILE}"

{
    echo ""
    echo "## Summary"
    echo ""
    echo "- Complete: ${passed}"
    echo "- Review: ${review}"
    echo "- Failed: ${failed}"
    echo "- Timeout: ${timed_out}"
    echo "- Missing: ${missing}"
    echo ""
    if [[ ${failed} -gt 0 || ${timed_out} -gt 0 || ${missing} -gt 0 ]]; then
        echo "Result: **fail**"
    elif [[ ${review} -gt 0 ]]; then
        echo "Result: **review**"
    else
        echo "Result: **pass**"
    fi
} >> "${REPORT_FILE}"

if [[ ${failed} -gt 0 || ${timed_out} -gt 0 || ${missing} -gt 0 ]]; then
    echo -e "  result: ${RED}FAIL${NC}"
    exit 1
fi

if [[ ${review} -gt 0 ]]; then
    echo -e "  result: ${YELLOW}REVIEW${NC}"
    exit 0
fi

echo -e "  result: ${GREEN}PASS${NC}"
exit 0
