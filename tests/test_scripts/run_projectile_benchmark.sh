#!/bin/bash
# Script to run the Projectile Scaling Benchmark
# Copyright (c) 2025 Hammer Forged Games, MIT License

# Set colors for better readability
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
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

echo -e "${PURPLE}=====================================================${NC}"
echo -e "${PURPLE}         Projectile Scaling Benchmark                ${NC}"
echo -e "${PURPLE}=====================================================${NC}"
echo -e "${YELLOW}Testing projectile entity throughput and SIMD 4-wide batching${NC}"

# Get the directory where this script is located and find project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Determine the correct path to the benchmark executable
if [ "$BUILD_TYPE" = "Debug" ]; then
  BENCHMARK_EXECUTABLE="$PROJECT_ROOT/bin/debug/projectile_scaling_benchmark"
else
  BENCHMARK_EXECUTABLE="$PROJECT_ROOT/bin/release/projectile_scaling_benchmark"
fi

# Verify executable exists
if [ ! -f "$BENCHMARK_EXECUTABLE" ]; then
  echo -e "${RED}Error: Benchmark executable not found at '$BENCHMARK_EXECUTABLE'${NC}"
  FOUND_EXECUTABLE=$(find "$PROJECT_ROOT/bin" -name "projectile_scaling_benchmark*" -type f -executable | head -n 1)
  if [ -n "$FOUND_EXECUTABLE" ]; then
    echo -e "${GREEN}Found executable at: $FOUND_EXECUTABLE${NC}"
    BENCHMARK_EXECUTABLE="$FOUND_EXECUTABLE"
  else
    echo -e "${RED}Could not find the benchmark executable. Build may have failed.${NC}"
    exit 1
  fi
fi

# Create output directory and file
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="$PROJECT_ROOT/test_results/projectile_scaling_benchmark_${TIMESTAMP}.txt"
mkdir -p "$PROJECT_ROOT/test_results"

# Check for timeout command
TIMEOUT_CMD=""
if command -v timeout &> /dev/null; then
  TIMEOUT_CMD="timeout"
elif command -v gtimeout &> /dev/null; then
  TIMEOUT_CMD="gtimeout"
fi

TIMEOUT_DURATION=120s

# Set test options
TEST_OPTS="--catch_system_errors=no"
if [ "$VERBOSE" = true ]; then
  TEST_OPTS="$TEST_OPTS --log_level=all"
else
  TEST_OPTS="$TEST_OPTS --log_level=test_suite"
fi

echo -e "${BLUE}Starting benchmark run at $(date)${NC}"
echo -e "${YELLOW}Build type: $BUILD_TYPE${NC}"

# Write header to results file
{
  echo "Projectile Scaling Benchmark - $(date)"
  echo "Build: $BUILD_TYPE"
  echo ""
} > "$RESULTS_FILE"

# Run the benchmark
echo -e "${BLUE}Running projectile scaling benchmarks...${NC}"
if [ -n "$TIMEOUT_CMD" ]; then
  $TIMEOUT_CMD --preserve-status $TIMEOUT_DURATION "$BENCHMARK_EXECUTABLE" $TEST_OPTS 2>&1 | tee -a "$RESULTS_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
else
  "$BENCHMARK_EXECUTABLE" $TEST_OPTS 2>&1 | tee -a "$RESULTS_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
fi

# Check for success
if [ $TEST_RESULT -eq 0 ]; then
  echo -e "${GREEN}Benchmark completed successfully!${NC}"
elif [ $TEST_RESULT -eq 124 ]; then
  echo -e "${RED}Benchmark timed out after $TIMEOUT_DURATION!${NC}"
else
  echo -e "${RED}Benchmark failed with exit code $TEST_RESULT${NC}"
fi

# Extract performance summary
echo -e "${BLUE}Performance Summary:${NC}"
echo "--- Projectile Entity Scaling ---"
grep -A 15 "Projectile Entity Scaling" "$RESULTS_FILE" | grep -E "^\s+[0-9]+|Projectiles|SCALABILITY"

echo ""
echo "--- SIMD 4-Wide Throughput ---"
grep -A 12 "SIMD 4-Wide Throughput" "$RESULTS_FILE" | grep -E "^\s+[0-9]+|Projectiles|Throughput"

echo ""
echo "--- Scalability Summary ---"
grep -A 3 "SCALABILITY SUMMARY" "$RESULTS_FILE"

# Copy to current run file for regression detection
cp "$RESULTS_FILE" "$PROJECT_ROOT/test_results/projectile_scaling_current.txt"

echo ""
echo -e "${GREEN}Results saved to:${NC}"
echo -e "  - Full log: ${BLUE}$RESULTS_FILE${NC}"
echo -e "  - Current run: ${BLUE}$PROJECT_ROOT/test_results/projectile_scaling_current.txt${NC}"

exit $TEST_RESULT
