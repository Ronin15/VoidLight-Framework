#!/bin/bash

# VoidLight-Framework Memory Profiler - Quick Leak Check
# Part of voidlight-memory-profiler skill

set -e

# Configuration
BASE_DIR="${BASE_DIR:-$(pwd)}"
OUTPUT_DIR="${OUTPUT_DIR:-$BASE_DIR/test_results/memory_profiles}"
TEST_DIR="${TEST_DIR:-$BASE_DIR/bin/debug}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Usage
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    cat << EOF
Usage: $0 [TEST_PATTERN]

Run an ad-hoc valgrind memcheck over test executables matching a pattern.

For the curated ownership/lifetime target set (with EDM slices, timeouts and
PASS/REVIEW/FAIL output) prefer the repo runner instead:
    ./tests/valgrind/quick_memory_check.sh [--extended | --target <name>]

ARGUMENTS:
    TEST_PATTERN        Glob pattern for test files (default: "*tests",
                        which skips device-dependent gpu_* tests)

OPTIONS:
    -h, --help          Show this help message

EXAMPLES:
    # Check all tests
    $0

    # Check only AI tests
    $0 "*ai*tests"

    # Check specific test
    $0 "thread_system_tests"
EOF
    exit 0
fi

TEST_PATTERN="${1:-*tests}"

# valgrind is Linux-only. CLAUDE.md ("Sanitizers") documents the AddressSanitizer
# build used as the leak-detection path where valgrind is unavailable.
SUPPRESSIONS_FILE="$BASE_DIR/tests/valgrind/valgrind_suppressions.supp"
if ! command -v valgrind >/dev/null 2>&1; then
    echo -e "${RED}Error: valgrind not found (unavailable on macOS/darwin).${NC}"
    echo -e "${YELLOW}Use the AddressSanitizer build from CLAUDE.md instead:${NC}"
    echo '  cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug \'
    echo '    -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=address -fno-omit-frame-pointer -g" \'
    echo '    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" -DUSE_MOLD_LINKER=OFF && ninja -C build'
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Find matching test executables
# GPU/device tests need a display/device and produce driver noise under
# valgrind (tests/valgrind/README.md) - skip them unless explicitly requested.
if [ -z "$1" ]; then
    mapfile -t TEST_EXECUTABLES < <(find "$TEST_DIR" -maxdepth 1 -name "$TEST_PATTERN" ! -name 'gpu_*' -type f -executable | sort)
else
    mapfile -t TEST_EXECUTABLES < <(find "$TEST_DIR" -maxdepth 1 -name "$TEST_PATTERN" -type f -executable | sort)
fi

VALGRIND_SUPP_ARGS=()
if [ -f "$SUPPRESSIONS_FILE" ]; then
    VALGRIND_SUPP_ARGS=("--suppressions=$SUPPRESSIONS_FILE")
fi

if [ ${#TEST_EXECUTABLES[@]} -eq 0 ]; then
    echo -e "${RED}Error: No test executables found matching: $TEST_PATTERN${NC}"
    exit 1
fi

TOTAL_TESTS=${#TEST_EXECUTABLES[@]}
LEAKS_FOUND=0
ERRORS_FOUND=0

echo "=========================================="
echo "VoidLight-Framework Quick Leak Check"
echo "=========================================="
echo "Pattern: $TEST_PATTERN"
echo "Tests found: $TOTAL_TESTS"
echo "Output: $OUTPUT_DIR"
echo "=========================================="
echo ""

# Run memcheck on each test
for TEST_PATH in "${TEST_EXECUTABLES[@]}"; do
    TEST_NAME=$(basename "$TEST_PATH")

    echo -e "${GREEN}Checking:${NC} $TEST_NAME"

    # A failing test must not abort the loop under `set -e`; the exit code is
    # reported separately from the memcheck findings.
    TEST_EXIT=0
    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --num-callers=20 \
        "${VALGRIND_SUPP_ARGS[@]}" \
        --log-file="$OUTPUT_DIR/${TEST_NAME}_memcheck.log" \
        "$TEST_PATH" --log_level=test_suite \
        > /dev/null 2>&1 || TEST_EXIT=$?

    # Parse results
    DEFINITE_LEAKS=$(grep "definitely lost:" "$OUTPUT_DIR/${TEST_NAME}_memcheck.log" | tail -1 | awk '{print $4}' | tr -d ',')
    DEFINITE_LEAKS=${DEFINITE_LEAKS:-0}

    INVALID_READ=$(grep -c "Invalid read" "$OUTPUT_DIR/${TEST_NAME}_memcheck.log" || true)
    INVALID_WRITE=$(grep -c "Invalid write" "$OUTPUT_DIR/${TEST_NAME}_memcheck.log" || true)

    # Report
    if [ "$DEFINITE_LEAKS" -gt 0 ] || [ "$INVALID_READ" -gt 0 ] || [ "$INVALID_WRITE" -gt 0 ] || [ "$TEST_EXIT" -ne 0 ]; then
        echo -e "  ${RED}❌ ISSUES FOUND${NC}"
        [ "$TEST_EXIT" -ne 0 ] && echo -e "    - Test exited with code: ${RED}$TEST_EXIT${NC}"
        [ "$DEFINITE_LEAKS" -gt 0 ] && echo -e "    - Definite leaks: ${RED}$DEFINITE_LEAKS bytes${NC}"
        [ "$INVALID_READ" -gt 0 ] && echo -e "    - Invalid reads: ${RED}$INVALID_READ${NC}"
        [ "$INVALID_WRITE" -gt 0 ] && echo -e "    - Invalid writes: ${RED}$INVALID_WRITE${NC}"
        LEAKS_FOUND=$((LEAKS_FOUND + 1))
    else
        echo -e "  ${GREEN}✅ Clean${NC}"
    fi

    echo ""
done

echo "=========================================="
echo "Leak Check Complete"
echo "=========================================="
echo "Tests checked: $TOTAL_TESTS"
echo "Tests with issues: $LEAKS_FOUND"
echo "Logs: $OUTPUT_DIR"
echo "=========================================="

if [ $LEAKS_FOUND -eq 0 ]; then
    echo -e "${GREEN}✅ No memory leaks detected${NC}"
    exit 0
else
    echo -e "${RED}❌ Found issues in $LEAKS_FOUND test(s)${NC}"
    echo -e "${YELLOW}Review logs in: $OUTPUT_DIR${NC}"
    exit 1
fi
