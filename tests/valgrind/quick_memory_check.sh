#!/bin/bash

# VoidLight-Framework - Targeted Valgrind Memcheck runner
# Runs a curated set of ownership/lifetime-heavy test executables.

set -u

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${PROJECT_ROOT}/tests/valgrind/valgrind_targets.sh"

BUILD_TYPE="debug"
TIMEOUT_SECONDS=180
RUN_EXTENDED=false
TARGET_REQUESTS=()
TARGET_SPECS=()

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
  --debug              Use bin/debug test executables (default)
  --profile            Use bin/profile test executables
  --extended           Include lower-frequency lifecycle/IO targets
  --target <name>      Run one target, EDM slice, or EDM slice set
  --timeout <seconds>  Per-target timeout (default: ${TIMEOUT_SECONDS})
  --help, -h           Show this help

Examples:
  $0
  $0 --extended
  $0 --target entity_data_manager_tests
  $0 --target edm_lifecycle
  $0 --profile --target manager_runtime_tests
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
        --extended)
            RUN_EXTENDED=true
            shift
            ;;
        --target)
            if [[ $# -lt 2 ]]; then
                echo -e "${RED}ERROR: --target requires an executable name${NC}" >&2
                exit 2
            fi
            TARGET_REQUESTS+=("$2")
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

spec_label() {
    local spec="$1"
    if [[ "${spec}" == *"|"* ]]; then
        printf '%s\n' "${spec%%|*}"
    else
        printf '%s\n' "${spec}"
    fi
}

append_target_entry() {
    local entry="$1"
    if [[ "${entry}" == *"|"* ]]; then
        TARGET_SPECS+=("${entry}")
    else
        TARGET_SPECS+=("${entry}|${entry}|")
    fi
}

append_target_request() {
    local requested="$1"

    if [[ "${requested}" == "edm_lifecycle" ]]; then
        for spec in "${VALGRIND_MEMCHECK_EDM_LIFECYCLE_TARGET_SPECS[@]}"; do
            TARGET_SPECS+=("${spec}")
        done
        return
    fi

    if [[ "${requested}" == "entity_data_manager_tests" ]]; then
        for spec in "${VALGRIND_MEMCHECK_EDM_TARGET_SPECS[@]}"; do
            TARGET_SPECS+=("${spec}")
        done
        return
    fi

    for spec in "${VALGRIND_MEMCHECK_EDM_TARGET_SPECS[@]}"; do
        if [[ "$(spec_label "${spec}")" == "${requested}" ]]; then
            TARGET_SPECS+=("${spec}")
            return
        fi
    done

    append_target_entry "${requested}"
}

if [[ ${#TARGET_REQUESTS[@]} -eq 0 ]]; then
    for target in "${VALGRIND_MEMCHECK_TARGETS[@]}"; do
        append_target_entry "${target}"
    done
    if [[ "${RUN_EXTENDED}" == true ]]; then
        for target in "${VALGRIND_MEMCHECK_EXTENDED_TARGETS[@]}"; do
            append_target_entry "${target}"
        done
    fi
else
    for target in "${TARGET_REQUESTS[@]}"; do
        append_target_request "${target}"
    done
fi

BIN_DIR="${PROJECT_ROOT}/bin/${BUILD_TYPE}"
RESULTS_DIR="${PROJECT_ROOT}/test_results/valgrind/memcheck"
SUPPRESSIONS_FILE="${PROJECT_ROOT}/tests/valgrind/valgrind_suppressions.supp"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S_%N")
REPORT_FILE="${RESULTS_DIR}/memcheck_report_${BUILD_TYPE}_${TIMESTAMP}.md"
mkdir -p "${RESULTS_DIR}"

if ! command -v valgrind >/dev/null 2>&1; then
    echo -e "${RED}ERROR: valgrind is not installed${NC}" >&2
    exit 2
fi

if [[ ! -d "${BIN_DIR}" ]]; then
    echo -e "${RED}ERROR: build output directory not found: ${BIN_DIR}${NC}" >&2
    exit 2
fi

echo -e "${BOLD}VoidLight targeted Memcheck${NC}"
echo -e "Build: ${CYAN}${BUILD_TYPE}${NC}"
echo -e "Targets: ${CYAN}${#TARGET_SPECS[@]}${NC}"
echo -e "Results: ${CYAN}${RESULTS_DIR}${NC}"
echo -e "Report: ${CYAN}${REPORT_FILE}${NC}"
echo ""

{
    echo "# Memcheck Report"
    echo ""
    echo "- Generated: $(date)"
    echo "- Build: ${BUILD_TYPE}"
    echo "- Tool: $(valgrind --version 2>/dev/null || echo unknown)"
    echo "- Timeout: ${TIMEOUT_SECONDS}s per target"
    echo ""
    echo "| Target | Executable | Filter | Status | Errors | Definite bytes | Indirect bytes | Possible bytes | Action | Detail |"
    echo "|---|---|---|---:|---:|---:|---:|---:|---|---|"
} > "${REPORT_FILE}"

passed=0
failed=0
warned=0
missing=0
timed_out=0
tool_failed=0
total_errors=0
total_definite=0
total_indirect=0
total_possible=0

extract_leak_bytes() {
    local label="$1"
    local log_file="$2"
    local value
    value=$(grep "${label}:" "${log_file}" | tail -1 | sed "s/.*${label}: \([0-9,]*\) bytes.*/\1/" | tr -d ',' 2>/dev/null || echo "0")
    [[ "${value}" =~ ^[0-9]+$ ]] || value="0"
    echo "${value}"
}

first_issue() {
    local log_file="$1"
    grep -m1 -E "Invalid read|Invalid write|Invalid free|Mismatched free|Use of uninitialised|Conditional jump.*uninitialised|definitely lost|indirectly lost|possibly lost" "${log_file}" 2>/dev/null \
        | sed 's/^==[0-9]*==[[:space:]]*//' \
        | sed 's/|/ /g' \
        || true
}

first_app_frame() {
    local log_file="$1"
    grep -m1 -E "(/src/|/include/|src/|include/)" "${log_file}" 2>/dev/null \
        | sed 's/^==[0-9]*==[[:space:]]*//' \
        | sed 's/|/ /g' \
        || true
}

run_target() {
    local label="$1"
    local executable="$2"
    local boost_filter="$3"
    local exe_path="${BIN_DIR}/${executable}"
    local log_file="${RESULTS_DIR}/${label}.memcheck.log"
    local filter_display="${boost_filter:-full}"
    local context="exe=${executable}"

    if [[ -n "${boost_filter}" ]]; then
        context="${context} filter=${boost_filter}"
    fi

    if [[ ! -x "${exe_path}" ]]; then
        echo -e "${YELLOW}MISSING${NC} memcheck ${label} ${context} action=build_target path=${exe_path}"
        echo "| ${label} | ${executable} | ${filter_display} | missing | N/A | N/A | N/A | N/A | build_target | ${exe_path} |" >> "${REPORT_FILE}"
        ((missing++))
        return
    fi

    local opts=(
        --tool=memcheck
        --leak-check=full
        --show-leak-kinds=definite,indirect,possible
        --errors-for-leak-kinds=definite,indirect
        --track-origins=yes
        --error-exitcode=99
        --num-callers=20
        "--log-file=${log_file}"
    )

    if [[ -f "${SUPPRESSIONS_FILE}" ]]; then
        opts+=("--suppressions=${SUPPRESSIONS_FILE}")
    fi

    local test_args=()
    if [[ -n "${boost_filter}" ]]; then
        test_args=(
            "--run_test=${boost_filter}"
            --catch_system_errors=no
            --report_level=short
            --log_level=nothing
        )
    fi

    timeout "${TIMEOUT_SECONDS}s" valgrind "${opts[@]}" "${exe_path}" "${test_args[@]}" >/dev/null 2>&1
    local status=$?

    if [[ ${status} -eq 124 ]]; then
        echo -e "${YELLOW}TIMEOUT${NC} memcheck ${label} ${context} after=${TIMEOUT_SECONDS}s action=reduce_filter_or_rerun_single_slice log=${log_file}"
        echo "| ${label} | ${executable} | ${filter_display} | timeout | N/A | N/A | N/A | N/A | reduce_filter_or_rerun_single_slice | ${log_file} |" >> "${REPORT_FILE}"
        ((timed_out++))
        return
    fi

    if [[ ! -s "${log_file}" ]]; then
        echo -e "${RED}TOOL_FAILED${NC} memcheck ${label} ${context} reason=no_log action=check_valgrind_invocation"
        echo "| ${label} | ${executable} | ${filter_display} | tool_failed | N/A | N/A | N/A | N/A | check_valgrind_invocation | no Valgrind log produced |" >> "${REPORT_FILE}"
        ((tool_failed++))
        return
    fi

    if grep -Eq "Fatal error at startup|valgrind: Fatal error|ld-linux.*mandatory redirection" "${log_file}"; then
        echo -e "${RED}TOOL_FAILED${NC} memcheck ${label} ${context} reason=valgrind_startup action=install_glibc_debug_symbols log=${log_file}"
        echo "| ${label} | ${executable} | ${filter_display} | tool_failed | N/A | N/A | N/A | N/A | install_glibc_debug_symbols | ${log_file} |" >> "${REPORT_FILE}"
        ((tool_failed++))
        return
    fi

    if [[ ${status} -ne 0 && ${status} -ne 99 ]]; then
        echo -e "${RED}FAIL${NC} memcheck ${label} ${context} test_exit=${status} action=rerun_test_without_valgrind log=${log_file}"
        echo "| ${label} | ${executable} | ${filter_display} | fail | test_exit_${status} | N/A | N/A | N/A | rerun_test_without_valgrind | ${log_file} |" >> "${REPORT_FILE}"
        ((failed++))
        return
    fi

    local errors="0"
    errors=$(grep "ERROR SUMMARY:" "${log_file}" | tail -1 | sed 's/.*ERROR SUMMARY: \([0-9][0-9]*\).*/\1/' 2>/dev/null || echo "0")
    [[ "${errors}" =~ ^[0-9]+$ ]] || errors="0"

    local definite indirect possible
    definite=$(extract_leak_bytes "definitely lost" "${log_file}")
    indirect=$(extract_leak_bytes "indirectly lost" "${log_file}")
    possible=$(extract_leak_bytes "possibly lost" "${log_file}")

    total_errors=$((total_errors + errors))
    total_definite=$((total_definite + definite))
    total_indirect=$((total_indirect + indirect))
    total_possible=$((total_possible + possible))

    if [[ ${status} -eq 99 || "${errors}" != "0" ]]; then
        local issue frame action detail
        issue=$(first_issue "${log_file}")
        frame=$(first_app_frame "${log_file}")
        if [[ "${definite}" != "0" || "${indirect}" != "0" ]]; then
            action="fix_leak"
        else
            action="fix_memory_error"
        fi
        detail="${issue:-inspect_log}"
        [[ -n "${frame}" ]] && detail="${detail}; ${frame}"
        echo -e "${RED}FAIL${NC} memcheck ${label} ${context} errors=${errors} definite=${definite}B indirect=${indirect}B possible=${possible}B action=${action} detail=\"${detail}\" log=${log_file}"
        echo "| ${label} | ${executable} | ${filter_display} | fail | ${errors} | ${definite} | ${indirect} | ${possible} | ${action} | ${detail} |" >> "${REPORT_FILE}"
        ((failed++))
    elif [[ "${possible}" != "0" ]]; then
        local detail
        detail=$(first_issue "${log_file}")
        echo -e "${YELLOW}REVIEW${NC} memcheck ${label} ${context} errors=${errors} definite=${definite}B indirect=${indirect}B possible=${possible}B action=classify_possible_leak detail=\"${detail:-inspect_log}\" log=${log_file}"
        echo "| ${label} | ${executable} | ${filter_display} | review | ${errors} | ${definite} | ${indirect} | ${possible} | classify_possible_leak | ${detail:-inspect_log} |" >> "${REPORT_FILE}"
        ((warned++))
    else
        echo -e "${GREEN}PASS${NC} memcheck ${label} ${context} errors=${errors} definite=${definite}B indirect=${indirect}B possible=${possible}B action=none"
        echo "| ${label} | ${executable} | ${filter_display} | pass | ${errors} | ${definite} | ${indirect} | ${possible} | none | clean |" >> "${REPORT_FILE}"
        ((passed++))
    fi
}

for target in "${TARGET_SPECS[@]}"; do
    IFS='|' read -r label executable boost_filter <<< "${target}"
    run_target "${label}" "${executable}" "${boost_filter}"
done

echo ""
echo -e "${BOLD}Memcheck summary${NC}"
echo "  passed: ${passed}"
echo "  review: ${warned}"
echo "  failed: ${failed}"
echo "  timeout: ${timed_out}"
echo "  missing: ${missing}"
echo "  tool_failed: ${tool_failed}"
echo "  total_errors: ${total_errors}"
echo "  total_definite: ${total_definite}B"
echo "  total_indirect: ${total_indirect}B"
echo "  total_possible: ${total_possible}B"
echo "  report: ${REPORT_FILE}"

if [[ ${failed} -gt 0 || ${timed_out} -gt 0 || ${missing} -gt 0 || ${tool_failed} -gt 0 ]]; then
    echo -e "  result: ${RED}FAIL${NC}"
elif [[ ${warned} -gt 0 ]]; then
    echo -e "  result: ${YELLOW}REVIEW${NC}"
else
    echo -e "  result: ${GREEN}PASS${NC}"
fi

{
    echo ""
    echo "## Summary"
    echo ""
    echo "- Passed: ${passed}"
    echo "- Review: ${warned}"
    echo "- Failed: ${failed}"
    echo "- Timeout: ${timed_out}"
    echo "- Missing: ${missing}"
    echo "- Tool failed: ${tool_failed}"
    echo "- Total errors: ${total_errors}"
    echo "- Total definite bytes: ${total_definite}"
    echo "- Total indirect bytes: ${total_indirect}"
    echo "- Total possible bytes: ${total_possible}"
    echo ""
    if [[ ${failed} -gt 0 || ${timed_out} -gt 0 || ${missing} -gt 0 || ${tool_failed} -gt 0 ]]; then
        echo "Result: **fail**"
    elif [[ ${warned} -gt 0 ]]; then
        echo "Result: **review**"
    else
        echo "Result: **pass**"
    fi
} >> "${REPORT_FILE}"

if [[ ${failed} -gt 0 || ${timed_out} -gt 0 || ${missing} -gt 0 || ${tool_failed} -gt 0 ]]; then
    exit 1
fi

exit 0
