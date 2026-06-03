#!/bin/bash

# Helper script to build and run Controller tests (all controller tests)

# Set up colored output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Process command line arguments
VERBOSE=false
RUN_ALL=true
RUN_REGISTRY=false
RUN_WEATHER=false
RUN_DAYNIGHT=false
RUN_HARVEST=false
RUN_NPCRENDER=false
RUN_INVENTORY=false
RUN_COMBAT=false
RUN_HUD=false
RUN_RESOURCERENDER=false
RUN_SOCIAL=false

for arg in "$@"; do
  case $arg in
    --verbose)
      VERBOSE=true
      shift
      ;;
    --help)
      echo -e "${BLUE}Controller Test Runner${NC}"
      echo -e "Usage: ./run_controller_tests.sh [options]"
      echo -e "\nOptions:"
      echo -e "  --verbose         Run tests with verbose output"
      echo -e "  --registry        Run only ControllerRegistry tests"
      echo -e "  --weather         Run only WeatherController tests"
      echo -e "  --daynight        Run only DayNightController tests"
      echo -e "  --harvest         Run only HarvestController tests"
      echo -e "  --npcrender       Run only NPCRenderController tests"
      echo -e "  --inventory       Run only InventoryController tests"
      echo -e "  --combat          Run only CombatController tests"
      echo -e "  --hud             Run only HudController tests"
      echo -e "  --resourcerender  Run only ResourceRenderController tests"
      echo -e "  --social          Run only SocialController tests"
      echo -e "  --help            Show this help message"
      exit 0
      ;;
    --registry)
      RUN_ALL=false
      RUN_REGISTRY=true
      shift
      ;;
    --weather)
      RUN_ALL=false
      RUN_WEATHER=true
      shift
      ;;
    --daynight)
      RUN_ALL=false
      RUN_DAYNIGHT=true
      shift
      ;;
    --harvest)
      RUN_ALL=false
      RUN_HARVEST=true
      shift
      ;;
    --npcrender)
      RUN_ALL=false
      RUN_NPCRENDER=true
      shift
      ;;
    --inventory)
      RUN_ALL=false
      RUN_INVENTORY=true
      shift
      ;;
    --combat)
      RUN_ALL=false
      RUN_COMBAT=true
      shift
      ;;
    --hud)
      RUN_ALL=false
      RUN_HUD=true
      shift
      ;;
    --resourcerender)
      RUN_ALL=false
      RUN_RESOURCERENDER=true
      shift
      ;;
    --social)
      RUN_ALL=false
      RUN_SOCIAL=true
      shift
      ;;
  esac
done

echo -e "${BLUE}Running Controller tests...${NC}"

# Define the test executables to run
EXECUTABLES=()

if [ "$RUN_ALL" = true ] || [ "$RUN_REGISTRY" = true ]; then
  EXECUTABLES+=("controller_registry_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_WEATHER" = true ]; then
  EXECUTABLES+=("weather_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_DAYNIGHT" = true ]; then
  EXECUTABLES+=("day_night_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_HARVEST" = true ]; then
  EXECUTABLES+=("harvest_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_NPCRENDER" = true ]; then
  EXECUTABLES+=("npc_render_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_INVENTORY" = true ]; then
  EXECUTABLES+=("inventory_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_COMBAT" = true ]; then
  EXECUTABLES+=("combat_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_HUD" = true ]; then
  EXECUTABLES+=("hud_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_RESOURCERENDER" = true ]; then
  EXECUTABLES+=("resource_render_controller_tests")
fi

if [ "$RUN_ALL" = true ] || [ "$RUN_SOCIAL" = true ]; then
  EXECUTABLES+=("social_controller_tests")
fi

# Get the directory where this script is located and find project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Track the final result
FINAL_RESULT=0

# Run the tests
for EXEC in "${EXECUTABLES[@]}"; do

  # Check if test executable exists
  TEST_EXECUTABLE="$PROJECT_ROOT/bin/debug/$EXEC"
  if [ ! -f "$TEST_EXECUTABLE" ]; then
    echo -e "${RED}Test executable not found at $TEST_EXECUTABLE${NC}"
    echo -e "${YELLOW}Searching for test executable...${NC}"
    FOUND_EXECUTABLE=$(find "$PROJECT_ROOT" -name "$EXEC" -type f -executable | head -n 1)
    if [ -n "$FOUND_EXECUTABLE" ]; then
      TEST_EXECUTABLE="$FOUND_EXECUTABLE"
      echo -e "${GREEN}Found test executable at $TEST_EXECUTABLE${NC}"
    else
      echo -e "${RED}Could not find test executable!${NC}"
      exit 1
    fi
  fi

  # Run the tests
  echo -e "${GREEN}Running $EXEC...${NC}"
  echo -e "${BLUE}====================================${NC}"

  echo -e "${YELLOW}Executing: $TEST_EXECUTABLE${NC}"

  # Create a temporary log file
  touch "${EXEC}_output.log"

  # Run with appropriate options and timeout to prevent hanging
  if [ "$VERBOSE" = true ]; then
    echo -e "${YELLOW}Running with verbose output${NC}"
    "$TEST_EXECUTABLE" --log_level=all --report_level=detailed > "${EXEC}_output.log" 2>&1 &
    TEST_PID=$!
  else
    "$TEST_EXECUTABLE" --report_level=short --log_level=test_suite > "${EXEC}_output.log" 2>&1 &
    TEST_PID=$!
  fi

  # Wait for up to 30 seconds for the test to complete
  TIMEOUT=30
  while [ $TIMEOUT -gt 0 ] && kill -0 $TEST_PID 2>/dev/null; do
    echo -n "."
    sleep 1
    TIMEOUT=$((TIMEOUT - 1))
  done

  # If the test is still running after timeout, kill it
  if kill -0 $TEST_PID 2>/dev/null; then
    echo -e "\n${RED}Test $EXEC timed out after 30 seconds, killing process${NC}"
    kill -9 $TEST_PID 2>/dev/null
    wait $TEST_PID 2>/dev/null || true
    echo "Test execution timed out after 30 seconds" >> "${EXEC}_output.log"
    TEST_RESULT=1
  else
    wait $TEST_PID
    TEST_RESULT=$?
    echo "" # New line after dots
  fi

  # Display the test output
  cat "${EXEC}_output.log"
  echo -e "${BLUE}====================================${NC}"

  # Create test_results directory if it doesn't exist
  mkdir -p "$PROJECT_ROOT/test_results"

  # Save test results with timestamp
  TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
  cp "${EXEC}_output.log" "$PROJECT_ROOT/test_results/${EXEC}_output_${TIMESTAMP}.txt"
  cp "${EXEC}_output.log" "$PROJECT_ROOT/test_results/${EXEC}_output.txt"

  # Extract test cases that were run
  echo -e "\n=== Test Cases Executed ===" > "$PROJECT_ROOT/test_results/${EXEC}_test_cases.txt"
  grep -E "Entering test case|Test case.*passed" "${EXEC}_output.log" >> "$PROJECT_ROOT/test_results/${EXEC}_test_cases.txt" || true

  # Extract just the test case names for easy reporting
  grep -E "Entering test case" "${EXEC}_output.log" | sed 's/.*Entering test case "\([^"]*\)".*/\1/' > "$PROJECT_ROOT/test_results/${EXEC}_test_cases_run.txt" || true

  # Report test results
  if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}All tests passed for $EXEC!${NC}"
    echo -e "${BLUE}Test results saved to:${NC} test_results/${EXEC}_output_${TIMESTAMP}.txt"

    # Print summary of test cases run
    echo -e "\n${BLUE}Test Cases Run:${NC}"
    if [ -f "$PROJECT_ROOT/test_results/${EXEC}_test_cases_run.txt" ] && [ -s "$PROJECT_ROOT/test_results/${EXEC}_test_cases_run.txt" ]; then
      while read -r testcase; do
        echo -e "  - ${testcase}"
      done < "$PROJECT_ROOT/test_results/${EXEC}_test_cases_run.txt"
    else
      echo -e "${YELLOW}  No test case details found.${NC}"
    fi
  else
    echo -e "${RED}Some tests failed for $EXEC. Please check the output above.${NC}"
    echo -e "${YELLOW}Test results saved to:${NC} test_results/${EXEC}_output_${TIMESTAMP}.txt"

    # Print a summary of failed tests if available
    echo -e "\n${YELLOW}Failed Test Summary:${NC}"
    grep -E "FAILED|ASSERT" "$PROJECT_ROOT/test_results/${EXEC}_output.txt" || echo -e "${YELLOW}No specific failure details found.${NC}"

    # Print summary of test cases run
    echo -e "\n${BLUE}Test Cases Run:${NC}"
    if [ -f "$PROJECT_ROOT/test_results/${EXEC}_test_cases_run.txt" ] && [ -s "$PROJECT_ROOT/test_results/${EXEC}_test_cases_run.txt" ]; then
      while read -r testcase; do
        echo -e "  - ${testcase}"
      done < "$PROJECT_ROOT/test_results/${EXEC}_test_cases_run.txt"
    else
      echo -e "${YELLOW}  No test case details found.${NC}"
    fi

    FINAL_RESULT=$TEST_RESULT
  fi

  # Clean up temporary file but keep it if verbose is enabled
  if [ "$VERBOSE" = false ]; then
    rm "${EXEC}_output.log"
  else
    echo -e "${YELLOW}Log file kept at: ${EXEC}_output.log${NC}"
  fi
done

if [ "$FINAL_RESULT" -ne 0 ]; then
  echo -e "\n${RED}Some controller tests failed!${NC}"
  exit $FINAL_RESULT
else
  echo -e "\n${GREEN}All controller tests completed successfully!${NC}"
  for EXEC in "${EXECUTABLES[@]}"; do
    echo -e "${GREEN}✓ ${EXEC}${NC}"
  done
  exit 0
fi
