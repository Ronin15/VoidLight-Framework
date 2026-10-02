#!/bin/bash
# Script to run TimestepManager tests
# Copyright (c) 2025 Hammer Forged Games, MIT License

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/../.."
VERBOSE=false
TEST_FILTER=""

for arg in "$@"; do
  case $arg in
    --verbose) VERBOSE=true; shift ;;
    --run_test=*) TEST_FILTER="${arg}"; shift ;;
    --help)
      echo -e "${BLUE}TimestepManager Tests Runner${NC}"
      echo -e "Usage: ./run_timestep_manager_tests.sh [options]"
      echo -e "\nOptions:"
      echo -e "  --verbose          Run tests with verbose output"
      echo -e "  --run_test=<name>  Run a specific test case"
      echo -e "  --help             Show this help message"
      echo -e "\nTest Coverage:"
      echo -e "  TimestepManagerConfigTests:"
      echo -e "    - Construction defaults and configuration setters/getters"
      echo -e "    - Fixed-delta invariant for getUpdateDeltaTime()"
      echo -e "  TimestepManagerRenderFlagTests:"
      echo -e "    - Render flag lifecycle across startFrame()/endFrame()"
      echo -e "  TimestepManagerAccumulatorTests:"
      echo -e "    - Accumulator-driven update loop and spiral-of-death guard"
      echo -e "    - Interpolation alpha clamping and reset() semantics"
      exit 0
      ;;
  esac
done

mkdir -p "$PROJECT_DIR/test_results"
RESULTS_FILE="$PROJECT_DIR/test_results/timestep_manager_tests_results.txt"

echo -e "${BLUE}======================================================${NC}"
echo -e "${BLUE}         TimestepManager Tests${NC}"
echo -e "${BLUE}======================================================${NC}"

TEST_EXECUTABLE="$PROJECT_DIR/bin/debug/timestep_manager_tests"
if [ ! -f "$TEST_EXECUTABLE" ]; then
    echo -e "${RED}Test executable not found: $TEST_EXECUTABLE${NC}"
    echo -e "${YELLOW}Make sure you have built the project with tests enabled.${NC}"
    echo -e "Run: ${CYAN}cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build${NC}"
    exit 1
fi
chmod +x "$TEST_EXECUTABLE"

echo -e "${CYAN}Running TimestepManager tests...${NC}"
echo "TimestepManager Tests - $(date)" > "$RESULTS_FILE"

if [ "$VERBOSE" = true ]; then
    $TEST_EXECUTABLE --log_level=all $TEST_FILTER 2>&1 | tee -a "$RESULTS_FILE"
    RESULT=${PIPESTATUS[0]}
else
    $TEST_EXECUTABLE --log_level=test_suite $TEST_FILTER 2>&1 | tee -a "$RESULTS_FILE"
    RESULT=${PIPESTATUS[0]}
fi

echo "" >> "$RESULTS_FILE"
echo "Test completed at: $(date)" >> "$RESULTS_FILE"
echo "Exit code: $RESULT" >> "$RESULTS_FILE"

echo -e "\n${BLUE}======================================================${NC}"
if [ $RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ All TimestepManager tests passed!${NC}"
    echo -e "\nTest Coverage:"
    echo -e "  ${GREEN}✓${NC} Configuration defaults, setters/getters, fixed-delta invariant"
    echo -e "  ${GREEN}✓${NC} Render flag lifecycle"
    echo -e "  ${GREEN}✓${NC} Accumulator drain, spiral-of-death guard, interpolation alpha"
else
    echo -e "${RED}✗ Some TimestepManager tests failed${NC}"
    echo -e "${YELLOW}Check the detailed results in: $RESULTS_FILE${NC}"
fi

echo -e "${CYAN}Test results saved to: $RESULTS_FILE${NC}"
echo -e "${BLUE}======================================================${NC}"

exit $RESULT
