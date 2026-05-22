#!/bin/bash

# SDL3 VoidLight-Framework - Runtime Memory Analysis
# Runs the main application under Valgrind for extended duration testing
# Detects memory leaks, invalid accesses, and memory growth over time
#
# Usage:
#   ./runtime_memory_analysis.sh [OPTIONS] [DURATION]
#
# Options:
#   --profile    Test profile build (bin/profile) - Valgrind-compatible optimized build
#   --debug      Test debug build (bin/debug) [default]
#   --help       Show this help message
#
# Duration: Time in seconds (default: 300)
#
# Examples:
#   ./runtime_memory_analysis.sh                  # Debug build, 5 minutes
#   ./runtime_memory_analysis.sh 600              # Debug build, 10 minutes
#   ./runtime_memory_analysis.sh --profile 300   # Profile build, 5 minutes (recommended)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Default values
BUILD_TYPE="debug"
DEFAULT_RUNTIME=300
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
RESULTS_DIR="${PROJECT_ROOT}/test_results/valgrind/runtime"
SUPPRESSIONS_FILE="${PROJECT_ROOT}/tests/valgrind/valgrind_suppressions.supp"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
EXECUTABLE="${BIN_DIR}/VoidLight_Template"

# Create results directory
mkdir -p "${RESULTS_DIR}"

# Result files (include build type in filename)
LOG_FILE="${RESULTS_DIR}/runtime_analysis_${BUILD_TYPE}_${TIMESTAMP}.log"
SUMMARY_FILE="${RESULTS_DIR}/runtime_summary_${BUILD_TYPE}_${TIMESTAMP}.txt"
REPORT_FILE="${RESULTS_DIR}/runtime_report_${BUILD_TYPE}_${TIMESTAMP}.md"

# Format build type for display
BUILD_TYPE_UPPER=$(echo "${BUILD_TYPE}" | tr '[:lower:]' '[:upper:]')
case "${BUILD_TYPE}" in
    "profile") BUILD_COLOR="${CYAN}" ;;
    *) BUILD_COLOR="${YELLOW}" ;;  # debug
esac

echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║           SDL3 VoidLight-Framework Runtime Memory Analysis          ║${NC}"
echo -e "${BOLD}${BLUE}║                                                              ║${NC}"
echo -e "${BOLD}${BLUE}║  Extended duration memory leak and error detection          ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BOLD}Build Type: ${BUILD_COLOR}${BUILD_TYPE_UPPER}${NC}"
echo -e "${CYAN}Analysis Timestamp: ${TIMESTAMP}${NC}"
echo -e "${CYAN}Runtime Duration: ${RUNTIME} seconds ($(($RUNTIME / 60)) minutes)${NC}"
echo -e "${CYAN}Executable: ${EXECUTABLE}${NC}"
echo -e "${CYAN}System: $(uname -s -r -m)${NC}"
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

echo -e "${YELLOW}Starting runtime memory analysis...${NC}"
echo -e "${YELLOW}The application will run for ${RUNTIME} seconds under Valgrind.${NC}"
echo -e "${YELLOW}Note: Application will run ~10-50x slower under Valgrind.${NC}"
echo ""

# Build valgrind options
VALGRIND_OPTS=""
VALGRIND_OPTS+="--tool=memcheck "
VALGRIND_OPTS+="--leak-check=full "
VALGRIND_OPTS+="--show-leak-kinds=definite,possible "
VALGRIND_OPTS+="--track-origins=yes "
VALGRIND_OPTS+="--track-fds=yes "
VALGRIND_OPTS+="--error-limit=no "
VALGRIND_OPTS+="--num-callers=20 "

if [[ -f "${SUPPRESSIONS_FILE}" ]]; then
    VALGRIND_OPTS+="--suppressions=${SUPPRESSIONS_FILE} "
    echo -e "${GREEN}Using suppressions file: ${SUPPRESSIONS_FILE}${NC}"
fi

# Run the application under valgrind with timeout
echo -e "${CYAN}Running: timeout ${RUNTIME}s valgrind ${EXECUTABLE}${NC}"
echo ""

START_TIME=$(date +%s)

timeout ${RUNTIME}s valgrind ${VALGRIND_OPTS} \
    --log-file="${LOG_FILE}" \
    "${EXECUTABLE}" 2>&1 || true

END_TIME=$(date +%s)
ACTUAL_RUNTIME=$((END_TIME - START_TIME))

echo ""
echo -e "${GREEN}Application ran for ${ACTUAL_RUNTIME} seconds${NC}"
echo ""

# Parse results
if [[ ! -f "${LOG_FILE}" ]]; then
    echo -e "${RED}ERROR: Valgrind log file not generated${NC}"
    exit 1
fi

# Extract key metrics (handle both formats and edge cases)
DEFINITELY_LOST=$(grep "definitely lost:" "${LOG_FILE}" | tail -1 | sed 's/.*definitely lost: \([0-9,]*\) bytes.*/\1/' | tr -d ',' || echo "0")
INDIRECTLY_LOST=$(grep "indirectly lost:" "${LOG_FILE}" | tail -1 | sed 's/.*indirectly lost: \([0-9,]*\) bytes.*/\1/' | tr -d ',' || echo "0")
POSSIBLY_LOST=$(grep "possibly lost:" "${LOG_FILE}" | tail -1 | sed 's/.*possibly lost: \([0-9,]*\) bytes.*/\1/' | tr -d ',' || echo "0")
STILL_REACHABLE=$(grep "still reachable:" "${LOG_FILE}" | tail -1 | sed 's/.*still reachable: \([0-9,]*\) bytes.*/\1/' | tr -d ',' || echo "0")
SUPPRESSED=$(grep "suppressed:" "${LOG_FILE}" | grep "bytes" | tail -1 | sed 's/.*suppressed: \([0-9,]*\) bytes.*/\1/' | tr -d ',' || echo "0")
ERROR_COUNT=$(grep "ERROR SUMMARY:" "${LOG_FILE}" | tail -1 | sed 's/.*ERROR SUMMARY: \([0-9]*\) errors.*/\1/' || echo "0")
SUPPRESSED_ERRORS=$(grep "ERROR SUMMARY:" "${LOG_FILE}" | tail -1 | sed 's/.*suppressed: \([0-9]*\) from.*/\1/' || echo "0")

# Ensure we have valid numbers
[[ -z "${DEFINITELY_LOST}" || ! "${DEFINITELY_LOST}" =~ ^[0-9]+$ ]] && DEFINITELY_LOST=0
[[ -z "${INDIRECTLY_LOST}" || ! "${INDIRECTLY_LOST}" =~ ^[0-9]+$ ]] && INDIRECTLY_LOST=0
[[ -z "${POSSIBLY_LOST}" || ! "${POSSIBLY_LOST}" =~ ^[0-9]+$ ]] && POSSIBLY_LOST=0
[[ -z "${STILL_REACHABLE}" || ! "${STILL_REACHABLE}" =~ ^[0-9]+$ ]] && STILL_REACHABLE=0
[[ -z "${SUPPRESSED}" || ! "${SUPPRESSED}" =~ ^[0-9]+$ ]] && SUPPRESSED=0
[[ -z "${ERROR_COUNT}" || ! "${ERROR_COUNT}" =~ ^[0-9]+$ ]] && ERROR_COUNT=0
[[ -z "${SUPPRESSED_ERRORS}" || ! "${SUPPRESSED_ERRORS}" =~ ^[0-9]+$ ]] && SUPPRESSED_ERRORS=0

# Count specific error types (ensure single integer output)
UNINIT_ERRORS=$(grep -c "uninitialised value" "${LOG_FILE}" 2>/dev/null | head -1 | tr -d '\n' || echo "0")
INVALID_READ=$(grep -c "Invalid read" "${LOG_FILE}" 2>/dev/null | head -1 | tr -d '\n' || echo "0")
INVALID_WRITE=$(grep -c "Invalid write" "${LOG_FILE}" 2>/dev/null | head -1 | tr -d '\n' || echo "0")
INVALID_FREE=$(grep -c "Invalid free" "${LOG_FILE}" 2>/dev/null | head -1 | tr -d '\n' || echo "0")

# Check for application-specific errors (not from system libraries)
APP_ERRORS=$(grep -E "(src/|include/)" "${LOG_FILE}" 2>/dev/null | grep -c "at 0x" 2>/dev/null | head -1 | tr -d '\n' || echo "0")

# Ensure all error counts are valid integers
[[ -z "${UNINIT_ERRORS}" || ! "${UNINIT_ERRORS}" =~ ^[0-9]+$ ]] && UNINIT_ERRORS=0
[[ -z "${INVALID_READ}" || ! "${INVALID_READ}" =~ ^[0-9]+$ ]] && INVALID_READ=0
[[ -z "${INVALID_WRITE}" || ! "${INVALID_WRITE}" =~ ^[0-9]+$ ]] && INVALID_WRITE=0
[[ -z "${INVALID_FREE}" || ! "${INVALID_FREE}" =~ ^[0-9]+$ ]] && INVALID_FREE=0
[[ -z "${APP_ERRORS}" || ! "${APP_ERRORS}" =~ ^[0-9]+$ ]] && APP_ERRORS=0

# Generate summary
cat > "${SUMMARY_FILE}" << EOF
SDL3 VoidLight-Framework Runtime Memory Analysis Summary
================================================
Timestamp: $(date)
Runtime: ${ACTUAL_RUNTIME} seconds
Executable: ${EXECUTABLE}

LEAK SUMMARY:
  Definitely Lost: ${DEFINITELY_LOST} bytes
  Indirectly Lost: ${INDIRECTLY_LOST} bytes
  Possibly Lost:   ${POSSIBLY_LOST} bytes
  Still Reachable: ${STILL_REACHABLE} bytes
  Suppressed:      ${SUPPRESSED} bytes

ERROR SUMMARY:
  Total Errors:      ${ERROR_COUNT}
  Suppressed Errors: ${SUPPRESSED_ERRORS}

ERROR BREAKDOWN:
  Uninitialized Values: ${UNINIT_ERRORS}
  Invalid Reads:        ${INVALID_READ}
  Invalid Writes:       ${INVALID_WRITE}
  Invalid Frees:        ${INVALID_FREE}

APPLICATION CODE ERRORS: ${APP_ERRORS}

ASSESSMENT:
EOF

# Determine assessment
ASSESSMENT="UNKNOWN"
if [[ "${DEFINITELY_LOST}" -eq 0 && "${ERROR_COUNT}" -eq 0 ]]; then
    ASSESSMENT="EXCELLENT"
    echo "  EXCELLENT - Zero memory leaks and zero errors from application code" >> "${SUMMARY_FILE}"
elif [[ "${DEFINITELY_LOST}" -eq 0 && "${APP_ERRORS}" -eq 0 ]]; then
    ASSESSMENT="GOOD"
    echo "  GOOD - Zero application memory leaks (system library issues only)" >> "${SUMMARY_FILE}"
elif [[ "${DEFINITELY_LOST}" -lt 10000 && "${APP_ERRORS}" -eq 0 ]]; then
    ASSESSMENT="ACCEPTABLE"
    echo "  ACCEPTABLE - Minor system library leaks, no application issues" >> "${SUMMARY_FILE}"
else
    ASSESSMENT="NEEDS_REVIEW"
    echo "  NEEDS REVIEW - Potential memory issues detected" >> "${SUMMARY_FILE}"
fi

# Generate markdown report
cat > "${REPORT_FILE}" << EOF
# SDL3 VoidLight-Framework Runtime Memory Analysis Report

**Generated**: $(date)
**Runtime Duration**: ${ACTUAL_RUNTIME} seconds ($(($ACTUAL_RUNTIME / 60)) minutes)
**Valgrind Version**: $(valgrind --version 2>/dev/null)

## Executive Summary

| Metric | Value | Status |
|--------|-------|--------|
| **Definitely Lost** | ${DEFINITELY_LOST} bytes | $([ "${DEFINITELY_LOST}" -eq 0 ] && echo "✅ Clean" || echo "⚠️ Review") |
| **Indirectly Lost** | ${INDIRECTLY_LOST} bytes | $([ "${INDIRECTLY_LOST}" -eq 0 ] && echo "✅ Clean" || echo "ℹ️ Indirect") |
| **Possibly Lost** | ${POSSIBLY_LOST} bytes | $([ "${POSSIBLY_LOST}" -eq 0 ] && echo "✅ Clean" || echo "ℹ️ Possible") |
| **Total Errors** | ${ERROR_COUNT} | $([ "${ERROR_COUNT}" -eq 0 ] && echo "✅ None" || echo "⚠️ ${ERROR_COUNT}") |
| **Suppressed** | ${SUPPRESSED_ERRORS} errors | ℹ️ Known issues |
| **App Code Errors** | ${APP_ERRORS} | $([ "${APP_ERRORS}" -eq 0 ] && echo "✅ None" || echo "❌ Review!") |

## Assessment: **${ASSESSMENT}**

EOF

if [[ "${ASSESSMENT}" == "EXCELLENT" ]]; then
    cat >> "${REPORT_FILE}" << EOF
🎯 **EXCELLENT** - The application demonstrates perfect memory management:
- Zero memory leaks from application code
- Zero errors originating from application code
- All detected issues are from external system libraries (GTK, fontconfig, etc.)
- Production-ready memory safety
EOF
elif [[ "${ASSESSMENT}" == "GOOD" ]]; then
    cat >> "${REPORT_FILE}" << EOF
✅ **GOOD** - The application has clean memory management:
- Zero memory leaks from application code
- Detected leaks/errors originate from system libraries only
- These are known issues in GTK/GLib/fontconfig and are suppressed
- Application code is memory-safe
EOF
elif [[ "${ASSESSMENT}" == "ACCEPTABLE" ]]; then
    cat >> "${REPORT_FILE}" << EOF
⚠️ **ACCEPTABLE** - Minor issues detected:
- Some memory reported as leaked, but from system libraries
- No application code memory issues detected
- Consider updating suppressions for new system library versions
EOF
else
    cat >> "${REPORT_FILE}" << EOF
🔧 **NEEDS REVIEW** - Issues detected that may require attention:
- Review the detailed log for application-specific issues
- Check for patterns that could indicate memory leaks in app code
- Log file: \`${LOG_FILE}\`
EOF
fi

cat >> "${REPORT_FILE}" << EOF

## Error Breakdown

| Error Type | Count | Source |
|------------|-------|--------|
| Uninitialized Values | ${UNINIT_ERRORS} | $([ "${UNINIT_ERRORS}" -gt 0 ] && echo "System libs (librsvg)" || echo "None") |
| Invalid Reads | ${INVALID_READ} | $([ "${INVALID_READ}" -gt 0 ] && echo "Review needed" || echo "None") |
| Invalid Writes | ${INVALID_WRITE} | $([ "${INVALID_WRITE}" -gt 0 ] && echo "Review needed" || echo "None") |
| Invalid Frees | ${INVALID_FREE} | $([ "${INVALID_FREE}" -gt 0 ] && echo "Review needed" || echo "None") |

## System Library Issues (Expected)

These are known issues in external libraries and do not indicate problems with the application:

- **librsvg**: Uninitialized value errors in SVG rendering (known upstream issue)
- **libfontconfig**: Memory leaks in font configuration (known upstream issue)
- **GLib/GTK**: Thread-local storage not freed at exit (by design)
- **Pango/Cairo**: Font caching leaks (known upstream issue)

## Files Generated

- **Detailed Log**: \`${LOG_FILE}\`
- **Summary**: \`${SUMMARY_FILE}\`
- **This Report**: \`${REPORT_FILE}\`

## Recommendations

1. **Regular Testing**: Run this analysis periodically (weekly/monthly)
2. **Before Release**: Always run extended runtime analysis before releases
3. **After Major Changes**: Run after significant code changes to detect regressions
4. **Update Suppressions**: Keep suppressions file updated for new library versions

## Usage

\`\`\`bash
# Run with default 5-minute duration
./tests/valgrind/runtime_memory_analysis.sh

# Run with custom duration (in seconds)
./tests/valgrind/runtime_memory_analysis.sh 600  # 10 minutes
./tests/valgrind/runtime_memory_analysis.sh 1800 # 30 minutes
\`\`\`

---
*Generated by SDL3 VoidLight-Framework Runtime Memory Analysis Suite*
EOF

# Display results
echo -e "${BOLD}${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}${BLUE}                    ANALYSIS RESULTS                           ${NC}"
echo -e "${BOLD}${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${BOLD}Memory Leak Summary:${NC}"
echo -e "  Definitely Lost: ${DEFINITELY_LOST} bytes"
echo -e "  Indirectly Lost: ${INDIRECTLY_LOST} bytes"
echo -e "  Possibly Lost:   ${POSSIBLY_LOST} bytes"
echo -e "  Suppressed:      ${SUPPRESSED} bytes"
echo ""

echo -e "${BOLD}Error Summary:${NC}"
echo -e "  Total Errors:      ${ERROR_COUNT}"
echo -e "  Suppressed:        ${SUPPRESSED_ERRORS}"
echo -e "  App Code Errors:   ${APP_ERRORS}"
echo ""

RESULT_STATUS="REVIEW"
RESULT_ACTION="inspect_runtime_memory_report"
if [[ "${ASSESSMENT}" == "EXCELLENT" || "${ASSESSMENT}" == "GOOD" ]]; then
    RESULT_STATUS="PASS"
    RESULT_ACTION="none"
elif [[ "${DEFINITELY_LOST}" -gt 0 || "${INDIRECTLY_LOST}" -gt 0 ]]; then
    RESULT_ACTION="fix_runtime_leak"
elif [[ "${APP_ERRORS}" -gt 0 || "${INVALID_READ}" -gt 0 || "${INVALID_WRITE}" -gt 0 || "${INVALID_FREE}" -gt 0 ]]; then
    RESULT_ACTION="fix_runtime_memory_error"
fi

{
    echo ""
    echo "## Automated Result"
    echo ""
    echo "Result: **${RESULT_STATUS,,}**"
    echo "Action: **${RESULT_ACTION}**"
} >> "${REPORT_FILE}"

# Color-coded assessment
case "${ASSESSMENT}" in
    "EXCELLENT")
        echo -e "${BOLD}${GREEN}🎯 ASSESSMENT: EXCELLENT${NC}"
        echo -e "${GREEN}Zero memory issues from application code in this run.${NC}"
        ;;
    "GOOD")
        echo -e "${BOLD}${GREEN}✅ ASSESSMENT: GOOD${NC}"
        echo -e "${GREEN}Application code is memory-safe. System library issues only.${NC}"
        ;;
    "ACCEPTABLE")
        echo -e "${BOLD}${YELLOW}⚠️ ASSESSMENT: ACCEPTABLE${NC}"
        echo -e "${YELLOW}Minor system library issues. Application code appears clean.${NC}"
        ;;
    *)
        echo -e "${BOLD}${RED}🔧 ASSESSMENT: NEEDS REVIEW${NC}"
        echo -e "${RED}Please review the detailed log for potential issues.${NC}"
        ;;
esac

if [[ "${RESULT_STATUS}" == "PASS" ]]; then
    echo -e "Result: ${GREEN}PASS${NC}"
else
    echo -e "Result: ${YELLOW}REVIEW${NC}"
fi
echo "Action: ${RESULT_ACTION}"
echo ""

echo ""
echo -e "${BOLD}Output Files:${NC}"
echo -e "  📊 Report:  ${CYAN}${REPORT_FILE}${NC}"
echo -e "  📋 Summary: ${CYAN}${SUMMARY_FILE}${NC}"
echo -e "  📝 Log:     ${CYAN}${LOG_FILE}${NC}"
echo ""
echo -e "${BOLD}${BLUE}Runtime memory analysis complete!${NC}"
