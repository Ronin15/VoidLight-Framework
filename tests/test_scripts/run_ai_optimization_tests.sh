#!/bin/bash
# Script to run the AI Optimization Tests
# Copyright (c) 2025 Hammer Forged Games, MIT License

# Set colors for better readability
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Running AI Optimization Tests...${NC}"

# Note: We calculate paths below and don't cd here to avoid path resolution issues

# Set default build type
BUILD_TYPE="Debug"
VERBOSE=false

# Process command-line options
while [[ $# -gt 0 ]]; do
  case $1 in
    --debug)
      BUILD_TYPE="Debug"
      shift
      ;;
    --release)
      BUILD_TYPE="Release"
      shift
      ;;
    --verbose)
      VERBOSE=true
      shift
      ;;
    --help)
      echo "Usage: $0 [--debug] [--release] [--verbose] [--help]"
      echo "  --debug     Run in Debug mode (default)"
      echo "  --release   Run in Release mode"
      echo "  --verbose   Show verbose output"
      echo "  --help      Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--debug] [--release] [--verbose] [--help]"
      exit 1
      ;;
  esac
done

# Prepare to run tests
echo -e "${YELLOW}Preparing to run AI Optimization tests...${NC}"

# Get the directory where this script is located and find project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Determine the correct path to the test executable
if [ "$BUILD_TYPE" = "Debug" ]; then
  TEST_EXECUTABLE="$PROJECT_ROOT/bin/debug/ai_optimization_tests"
else
  TEST_EXECUTABLE="$PROJECT_ROOT/bin/release/ai_optimization_tests"
fi

# Verify executable exists
if [ ! -f "$TEST_EXECUTABLE" ]; then
  echo -e "${RED}Error: Test executable not found at '$TEST_EXECUTABLE'${NC}"
  # Attempt to find the executable
  echo -e "${YELLOW}Searching for test executable...${NC}"
  FOUND_EXECUTABLE=$(find "$PROJECT_ROOT/bin" -name "ai_optimization_tests" -type f -executable | head -n 1)
  if [ -n "$FOUND_EXECUTABLE" ]; then
    echo -e "${GREEN}Found executable at: $FOUND_EXECUTABLE${NC}"
    TEST_EXECUTABLE="$FOUND_EXECUTABLE"
  else
    echo -e "${RED}Could not find the test executable. Build may have failed or placed the executable in an unexpected location.${NC}"
    exit 1
  fi
fi

# Run tests and save output
echo -e "${YELLOW}Running AI Optimization tests...${NC}"

# Ensure test_results directory exists
mkdir -p "$PROJECT_ROOT/test_results"

# Output file
OUTPUT_FILE="$PROJECT_ROOT/test_results/ai_optimization_tests_output.txt"
METRICS_FILE="$PROJECT_ROOT/test_results/ai_optimization_tests_performance_metrics.txt"

# Set test command options
TEST_OPTS="--log_level=all --catch_system_errors=no"
if [ "$VERBOSE" = true ]; then
  TEST_OPTS="$TEST_OPTS --report_level=detailed"
fi

# Check for timeout command availability
TIMEOUT_CMD=""
if command -v timeout &> /dev/null; then
  TIMEOUT_CMD="timeout"
elif command -v gtimeout &> /dev/null; then
  TIMEOUT_CMD="gtimeout"
else
  echo -e "${YELLOW}Warning: Neither 'timeout' nor 'gtimeout' command found. Tests will run without timeout protection.${NC}"
fi

# Run the tests with additional safeguards
echo -e "${YELLOW}Running with options: $TEST_OPTS${NC}"
if [ -n "$TIMEOUT_CMD" ]; then
  $TIMEOUT_CMD 30s "$TEST_EXECUTABLE" $TEST_OPTS 2>&1 | tee "$OUTPUT_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
else
  "$TEST_EXECUTABLE" $TEST_OPTS 2>&1 | tee "$OUTPUT_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
fi

# Extract performance metrics
echo -e "${YELLOW}Extracting performance metrics...${NC}"
grep -E "time:|entities:|processed:|Performance|Execution time|optimization" "$OUTPUT_FILE" > "$METRICS_FILE" || true

# Handle timeout scenario
if [ -n "$TIMEOUT_CMD" ] && [ $TEST_RESULT -eq 124 ]; then
  echo -e "${RED}⚠️ Test execution timed out after 30 seconds!${NC}"
  echo "Test execution timed out after 30 seconds!" >> "$OUTPUT_FILE"
fi

# Check test status
if [ $TEST_RESULT -eq 124 ]; then
  echo -e "${RED}❌ Tests timed out! See $OUTPUT_FILE for details.${NC}"
  exit $TEST_RESULT
elif [ $TEST_RESULT -ne 0 ] || grep -q "failure\|test cases failed\|memory access violation\|fatal error\|Segmentation fault\|Abort trap\|assertion failed\|error:" "$OUTPUT_FILE"; then
  echo -e "${RED}❌ Some tests failed! See $OUTPUT_FILE for details.${NC}"
  exit 1
else
  echo -e "${GREEN}✅ All AI Optimization tests passed!${NC}"
  echo -e "${GREEN}Test results saved to $OUTPUT_FILE${NC}"
  echo -e "${GREEN}Performance metrics saved to $METRICS_FILE${NC}"
  exit 0
fi
