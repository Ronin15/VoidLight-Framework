#!/bin/bash

# VoidLight-Framework Memory Profiler - Run Massif on All Tests
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
NC='\033[0m' # No Color

# Usage
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    cat << EOF
Usage: $0 [OPTIONS]

Run valgrind massif on all VoidLight-Framework test executables.

OPTIONS:
    -h, --help          Show this help message
    -o, --output DIR    Output directory (default: test_results/memory_profiles)
    -t, --tests DIR     Test executables directory (default: bin/debug)
    -v, --verbose       Verbose output

ENVIRONMENT:
    BASE_DIR            Project root directory
    OUTPUT_DIR          Where to save massif outputs
    TEST_DIR            Where to find test executables

EXAMPLES:
    # Run all tests with defaults
    $0

    # Specify custom output
    $0 --output /tmp/memory_profiles

    # Verbose mode
    $0 --verbose
EOF
    exit 0
fi

# Parse arguments
VERBOSE=0
while [[ $# -gt 0 ]]; do
    case $1 in
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -t|--tests)
            TEST_DIR="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# valgrind/ms_print are not available on macOS/darwin. CLAUDE.md documents
# AddressSanitizer as the supported memory-profiling path on this platform.
if ! command -v valgrind >/dev/null 2>&1; then
    echo -e "${RED}Error: valgrind not found (unavailable on macOS/darwin).${NC}"
    echo -e "${YELLOW}massif requires valgrind. On macOS use the AddressSanitizer build from CLAUDE.md instead:${NC}"
    echo '  cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug \'
    echo '    -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=address -fno-omit-frame-pointer -g" \'
    echo '    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" -DUSE_MOLD_LINKER=OFF && ninja -C build'
    exit 1
fi

# Validate directories
if [ ! -d "$TEST_DIR" ]; then
    echo -e "${RED}Error: Test directory not found: $TEST_DIR${NC}"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Find all test executables
mapfile -t TEST_EXECUTABLES < <(find "$TEST_DIR" -maxdepth 1 -name "*tests" -type f -executable | sort)

if [ ${#TEST_EXECUTABLES[@]} -eq 0 ]; then
    echo -e "${RED}Error: No test executables found in $TEST_DIR${NC}"
    exit 1
fi

TOTAL_TESTS=${#TEST_EXECUTABLES[@]}
CURRENT=0
FAILED=0
START_TIME=$(date +%s)

echo "=========================================="
echo "VoidLight-Framework Memory Profiler"
echo "=========================================="
echo "Test directory: $TEST_DIR"
echo "Output directory: $OUTPUT_DIR"
echo "Total tests: $TOTAL_TESTS"
echo "Started: $(date)"
echo "=========================================="
echo ""

# Run massif on each test
for TEST_PATH in "${TEST_EXECUTABLES[@]}"; do
    CURRENT=$((CURRENT + 1))
    TEST_NAME=$(basename "$TEST_PATH")

    # Progress calculation
    PERCENT=$((CURRENT * 100 / TOTAL_TESTS))
    ELAPSED=$(($(date +%s) - START_TIME))

    if [ $CURRENT -gt 1 ]; then
        AVG_TIME=$((ELAPSED / (CURRENT - 1)))
        REMAINING=$(( (TOTAL_TESTS - CURRENT) * AVG_TIME ))
        TIME_INFO="Elapsed: ${ELAPSED}s | Est. remaining: ${REMAINING}s"
    else
        TIME_INFO="Starting..."
    fi

    echo -e "${GREEN}[$CURRENT/$TOTAL_TESTS - $PERCENT%]${NC} $TEST_NAME"
    echo "  $TIME_INFO"

    # Run massif
    if [ $VERBOSE -eq 1 ]; then
        valgrind \
            --tool=massif \
            --massif-out-file="$OUTPUT_DIR/${TEST_NAME}_massif.out" \
            --time-unit=ms \
            --detailed-freq=1 \
            --max-snapshots=100 \
            --threshold=0.1 \
            "$TEST_PATH" --log_level=test_suite
    else
        valgrind \
            --tool=massif \
            --massif-out-file="$OUTPUT_DIR/${TEST_NAME}_massif.out" \
            --time-unit=ms \
            --detailed-freq=1 \
            --max-snapshots=100 \
            --threshold=0.1 \
            "$TEST_PATH" --log_level=test_suite \
            > "$OUTPUT_DIR/${TEST_NAME}_run.log" 2>&1
    fi

    if [ $? -eq 0 ]; then
        # Generate text report
        ms_print "$OUTPUT_DIR/${TEST_NAME}_massif.out" > "$OUTPUT_DIR/${TEST_NAME}_massif_report.txt" 2>&1
        echo -e "  ${GREEN}✅ Complete${NC}"
    else
        echo -e "  ${RED}❌ Failed${NC}"
        FAILED=$((FAILED + 1))
    fi

    echo ""
done

END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

echo "=========================================="
echo "Memory Profiling Complete"
echo "=========================================="
echo "Tests profiled: $((TOTAL_TESTS - FAILED))/$TOTAL_TESTS"
echo "Failed: $FAILED"
echo "Total time: ${TOTAL_TIME}s ($((TOTAL_TIME / 60))m $((TOTAL_TIME % 60))s)"
echo "Output: $OUTPUT_DIR"
echo "=========================================="

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ All tests profiled successfully${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠️  $FAILED test(s) failed${NC}"
    exit 1
fi
