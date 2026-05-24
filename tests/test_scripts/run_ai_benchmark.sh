#!/bin/bash
# Script to run the AI Scaling Benchmark
# Copyright (c) 2025 Hammer Forged Games, MIT License

# Set colors for better readability
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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
    --verbose)
      VERBOSE=true
      shift
      ;;
    --release)
      BUILD_TYPE="Release"
      shift
      ;;
    --help)
      echo "Usage: $0 [--debug] [--release] [--verbose] [--help]"
      echo "  --debug       Run debug build (default)"
      echo "  --release     Run release build"
      echo "  --verbose     Show detailed output"
      echo "  --help        Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--debug] [--release] [--verbose] [--help]"
      exit 1
      ;;
  esac
done

echo -e "${YELLOW}Running AI Scaling Benchmark...${NC}"

# Get the directory where this script is located and find project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Determine the correct path to the benchmark executable
if [ "$BUILD_TYPE" = "Debug" ]; then
  BENCHMARK_EXECUTABLE="$PROJECT_ROOT/bin/debug/ai_scaling_benchmark"
else
  BENCHMARK_EXECUTABLE="$PROJECT_ROOT/bin/release/ai_scaling_benchmark"
fi

# Verify executable exists
if [ ! -f "$BENCHMARK_EXECUTABLE" ]; then
  echo -e "${RED}Error: Benchmark executable not found at '$BENCHMARK_EXECUTABLE'${NC}"
  echo -e "${YELLOW}Searching for benchmark executable...${NC}"
  FOUND_EXECUTABLE=$(find "$PROJECT_ROOT/bin" -name "ai_scaling_benchmark*" -type f -executable | head -n 1)
  if [ -n "$FOUND_EXECUTABLE" ]; then
    echo -e "${GREEN}Found executable at: $FOUND_EXECUTABLE${NC}"
    BENCHMARK_EXECUTABLE="$FOUND_EXECUTABLE"
  else
    echo -e "${RED}Could not find the benchmark executable. Build may have failed.${NC}"
    exit 1
  fi
fi

# Create output file for results
RESULTS_FILE="$PROJECT_ROOT/test_results/ai_scaling_benchmark_$(date +%Y%m%d_%H%M%S).txt"
mkdir -p "$PROJECT_ROOT/test_results"

# Check for timeout command availability
TIMEOUT_CMD=""
if command -v timeout &> /dev/null; then
  TIMEOUT_CMD="timeout"
elif command -v gtimeout &> /dev/null; then
  TIMEOUT_CMD="gtimeout"
else
  echo -e "${YELLOW}Warning: Neither 'timeout' nor 'gtimeout' command found.${NC}"
fi

TIMEOUT_DURATION=180s  # 3 minutes for full benchmark suite

# Set test command options
TEST_OPTS="--catch_system_errors=no"
if [ "$VERBOSE" = true ]; then
  TEST_OPTS="$TEST_OPTS --log_level=all"
else
  TEST_OPTS="$TEST_OPTS --log_level=test_suite"
fi

echo -e "${BLUE}Starting benchmark run at $(date)${NC}"
echo -e "${YELLOW}Build type: $BUILD_TYPE${NC}"
echo -e "${YELLOW}Timeout: $TIMEOUT_DURATION${NC}"

# Run the benchmark
echo "============ BENCHMARK START ============" > "$RESULTS_FILE"
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "Build type: $BUILD_TYPE" >> "$RESULTS_FILE"
echo "=========================================" >> "$RESULTS_FILE"
echo >> "$RESULTS_FILE"

if [ -n "$TIMEOUT_CMD" ]; then
  $TIMEOUT_CMD --preserve-status $TIMEOUT_DURATION "$BENCHMARK_EXECUTABLE" $TEST_OPTS 2>&1 | tee -a "$RESULTS_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
else
  "$BENCHMARK_EXECUTABLE" $TEST_OPTS 2>&1 | tee -a "$RESULTS_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
fi

echo >> "$RESULTS_FILE"
echo "============ BENCHMARK END =============" >> "$RESULTS_FILE"
echo "Exit code: $TEST_RESULT" >> "$RESULTS_FILE"

# Check for success
if [ $TEST_RESULT -eq 0 ]; then
  echo -e "${GREEN}Benchmark completed successfully!${NC}"
elif [ $TEST_RESULT -eq 124 ]; then
  echo -e "${RED}Benchmark timed out after $TIMEOUT_DURATION!${NC}"
else
  echo -e "${RED}Benchmark failed with exit code $TEST_RESULT${NC}"
fi

# Extract performance metrics
echo -e "${BLUE}Performance Summary:${NC}"

print_table_section() {
  local section_title="$1"
  awk -v title="$section_title" '
    index($0, title) { printing = 1; next }
    printing && /^$/ { exit }
    printing && ($0 ~ /^Workload:/ || $0 ~ /^[[:space:]]*[0-9]+/ || $0 ~ /Entities|Attackers/) { print }
  ' "$RESULTS_FILE"
}

# Extract key metrics from new tabular format
echo "--- AI Entity Scaling Results ---"
print_table_section "AI Entity Scaling"

echo
echo "--- Attack Behavior Pressure Scaling ---"
print_table_section "Attack Behavior Decision Pressure Scaling"

echo
echo "--- Attack Behavior Tactical Reset Scaling ---"
print_table_section "Attack Behavior Decision Tactical Reset Scaling"

echo
echo "--- Attack Behavior Cold Burst Resolve Scaling ---"
print_table_section "Attack Behavior Cold Burst Resolve Scaling"

echo
echo "--- Attack Behavior Cadenced Resolve Scaling ---"
print_table_section "Attack Behavior Cadenced Resolve Scaling"

echo
echo "--- Scalability Summary ---"
awk '
  /SCALABILITY SUMMARY/ { printing = 1 }
  printing { print; ++lines }
  printing && lines >= 4 { exit }
' "$RESULTS_FILE"

# Create a current run copy for regression detection
cp "$RESULTS_FILE" "$PROJECT_ROOT/test_results/ai_scaling_current.txt"

echo
echo -e "${GREEN}Results saved to:${NC}"
echo -e "  - Full log: ${BLUE}$RESULTS_FILE${NC}"
echo -e "  - Current run: ${BLUE}$PROJECT_ROOT/test_results/ai_scaling_current.txt${NC}"

exit $TEST_RESULT
