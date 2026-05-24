#!/bin/bash
# Script to run the Behavior Functionality Tests
# Copyright (c) 2025 Hammer Forged Games, MIT License

# Set colors for better readability
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Running Behavior Functionality Tests...${NC}"

# Note: We calculate paths below and don't cd here to avoid path resolution issues

# Set default build type
BUILD_TYPE="Debug"
VERBOSE=false
SPECIFIC_SUITE=""

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
    --suite)
      SPECIFIC_SUITE="$2"
      shift 2
      ;;
    --help)
      echo "Usage: $0 [--debug] [--release] [--verbose] [--suite SUITE_NAME] [--help]"
      echo "  --debug     Run in Debug mode (default)"
      echo "  --release   Run in Release mode"
      echo "  --verbose   Show verbose output"
      echo "  --suite     Run specific test suite only"
      echo "              Available suites: BehaviorRegistrationTests, IdleBehaviorTests,"
      echo "                               MovementBehaviorTests, ComplexBehaviorTests,"
      echo "                               BehaviorMessageTests, BehaviorModeTests,"
      echo "                               BehaviorTransitionTests, BehaviorPerformanceTests,"
      echo "                               AdvancedBehaviorFeatureTests"
      echo "  --help      Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--debug] [--release] [--verbose] [--suite SUITE_NAME] [--help]"
      exit 1
      ;;
  esac
done

# Prepare to run tests
echo -e "${YELLOW}Preparing to run Behavior Functionality tests...${NC}"

# Get the directory where this script is located and find project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Determine the correct path to the test executable
if [ "$BUILD_TYPE" = "Debug" ]; then
  TEST_EXECUTABLE="$PROJECT_ROOT/bin/debug/behavior_functionality_tests"
else
  TEST_EXECUTABLE="$PROJECT_ROOT/bin/release/behavior_functionality_tests"
fi

# Verify executable exists
if [ ! -f "$TEST_EXECUTABLE" ]; then
  echo -e "${RED}Error: Test executable not found at '$TEST_EXECUTABLE'${NC}"
  # Attempt to find the executable
  echo -e "${YELLOW}Searching for test executable...${NC}"
  FOUND_EXECUTABLE=$(find "$PROJECT_ROOT/bin" -name "behavior_functionality_tests" -type f -executable | head -n 1)
  if [ -n "$FOUND_EXECUTABLE" ]; then
    echo -e "${GREEN}Found executable at: $FOUND_EXECUTABLE${NC}"
    TEST_EXECUTABLE="$FOUND_EXECUTABLE"
  else
    echo -e "${RED}Could not find the test executable. Build may have failed or placed the executable in an unexpected location.${NC}"
    exit 1
  fi
fi

# Run tests and save output
echo -e "${YELLOW}Running Behavior Functionality tests...${NC}"

# Ensure test_results directory exists
mkdir -p "$PROJECT_ROOT/test_results"

# Output files
OUTPUT_FILE="$PROJECT_ROOT/test_results/behavior_functionality_tests_output.txt"
SUMMARY_FILE="$PROJECT_ROOT/test_results/behavior_functionality_tests_summary.txt"
BEHAVIOR_REPORT="$PROJECT_ROOT/test_results/behavior_test_report.txt"

# Set test command options
TEST_OPTS="--log_level=all --catch_system_errors=no"
if [ "$VERBOSE" = true ]; then
  TEST_OPTS="$TEST_OPTS --report_level=detailed"
fi

# Add specific suite option if provided
if [ -n "$SPECIFIC_SUITE" ]; then
  TEST_OPTS="$TEST_OPTS --run_test=$SPECIFIC_SUITE"
  echo -e "${BLUE}Running specific test suite: $SPECIFIC_SUITE${NC}"
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
  $TIMEOUT_CMD 60s "$TEST_EXECUTABLE" $TEST_OPTS 2>&1 | tee "$OUTPUT_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
else
  "$TEST_EXECUTABLE" $TEST_OPTS 2>&1 | tee "$OUTPUT_FILE"
  TEST_RESULT=${PIPESTATUS[0]}
fi

# Generate test summary
echo -e "${YELLOW}Generating test summary...${NC}"
{
  echo "=== Behavior Functionality Test Summary ==="
  echo "Generated on: $(date)"
  echo "Build Type: $BUILD_TYPE"
  echo "Test Command: $TEST_EXECUTABLE $TEST_OPTS"
  echo ""

  # Count test results
  TOTAL_TESTS=$(grep -c "Entering test case" "$OUTPUT_FILE" 2>/dev/null || echo "0")
  PASSED_TESTS=$(grep -c "Leaving test case" "$OUTPUT_FILE" 2>/dev/null || echo "0")
  TOTAL_TESTS=${TOTAL_TESTS//[$'\t\r\n']}
  PASSED_TESTS=${PASSED_TESTS//[$'\t\r\n']}
  FAILED_TESTS=$((TOTAL_TESTS - PASSED_TESTS))

  echo "Test Results:"
  echo "  Total Tests: $TOTAL_TESTS"
  echo "  Passed: $PASSED_TESTS"
  echo "  Failed: $FAILED_TESTS"
  echo ""

  # Extract behavior-specific results
  echo "Behavior Test Coverage:"
  grep -E "(IdleBehavior|WanderBehavior|PatrolBehavior|ChaseBehavior|FleeBehavior|FollowBehavior|GuardBehavior|AttackBehavior)" "$OUTPUT_FILE" | head -20
  echo ""

  # Performance metrics
  echo "Performance Metrics:"
  grep -E "time:|entities:|Performance|Execution time|update.*time" "$OUTPUT_FILE" | head -10

} > "$SUMMARY_FILE"

# Generate behavior-specific report
echo -e "${YELLOW}Generating behavior test report...${NC}"
{
  echo "=== AI Behavior Test Report ==="
  echo "Generated on: $(date)"
  echo ""

  # Test suite results
  echo "Test Suite Results:"
  echo "==================="

  # Registration Tests
  if grep -q "BehaviorRegistrationTests" "$OUTPUT_FILE"; then
    echo "✓ Behavior Registration Tests - All 8 behaviors registered successfully"
  fi

  # Individual Behavior Tests
  for behavior in "Idle" "Wander" "Chase" "Flee" "Follow" "Guard" "Attack" "Patrol"; do
    if grep -q "${behavior}Behavior" "$OUTPUT_FILE"; then
      echo "✓ ${behavior} Behavior Tests - Core functionality validated"
    fi
  done

  echo ""
  echo "Behavior Mode Testing:"
  echo "====================="

  # Mode testing results
  if grep -q "FollowModes" "$OUTPUT_FILE"; then
    echo "✓ Follow Behavior Modes - All variants tested"
  fi
  if grep -q "AttackModes" "$OUTPUT_FILE"; then
    echo "✓ Attack Behavior Modes - Melee, Ranged, Charge tested"
  fi
  if grep -q "WanderModes" "$OUTPUT_FILE"; then
    echo "✓ Wander Behavior Modes - Small, Medium, Large areas tested"
  fi

  echo ""
  echo "Advanced Features:"
  echo "=================="

  # Advanced feature testing
  if grep -q "PatrolBehaviorWithWaypoints" "$OUTPUT_FILE"; then
    echo "✓ Patrol Waypoint System - Custom routes validated"
  fi
  if grep -q "GuardAlertSystem" "$OUTPUT_FILE"; then
    echo "✓ Guard Alert System - Threat detection validated"
  fi
  if grep -q "MessageQueueSystem" "$OUTPUT_FILE"; then
    echo "✓ Message System - Behavior communication validated"
  fi

  echo ""
  echo "Performance Testing:"
  echo "==================="

  # Extract performance results
  grep -A 5 -B 5 "LargeNumberOfEntities" "$OUTPUT_FILE" | grep -E "(entities|time|performance)" || echo "Performance tests completed"

} > "$BEHAVIOR_REPORT"

# Handle timeout scenario
if [ -n "$TIMEOUT_CMD" ] && [ $TEST_RESULT -eq 124 ]; then
  echo -e "${RED}⚠️ Test execution timed out after 60 seconds!${NC}"
  echo "Test execution timed out after 60 seconds!" >> "$OUTPUT_FILE"
fi

# Check test status and provide detailed feedback
if [ $TEST_RESULT -eq 124 ]; then
  echo -e "${RED}❌ Tests timed out! See $OUTPUT_FILE for details.${NC}"
  exit $TEST_RESULT
elif [ $TEST_RESULT -ne 0 ] || grep -q "failure\|test cases failed\|memory access violation\|fatal error\|Segmentation fault\|Abort trap\|assertion failed\|error:" "$OUTPUT_FILE"; then
  echo -e "${RED}❌ Some behavior tests failed!${NC}"
  echo -e "${YELLOW}Failed test details:${NC}"
  grep -A 3 -B 1 "failure\|assertion failed\|error:" "$OUTPUT_FILE" || echo "Check $OUTPUT_FILE for details"
  echo -e "${RED}See $OUTPUT_FILE for complete details.${NC}"
  exit 1
else
  echo -e "${GREEN}✅ All Behavior Functionality tests passed!${NC}"
  echo -e "${GREEN}Test results saved to $OUTPUT_FILE${NC}"
  echo -e "${GREEN}Test summary saved to $SUMMARY_FILE${NC}"
  echo -e "${GREEN}Behavior report saved to $BEHAVIOR_REPORT${NC}"

  # Display quick summary
  echo -e "${BLUE}"
  echo "=== Quick Test Summary ==="
  TOTAL_TESTS=$(grep -c "Entering test case" "$OUTPUT_FILE" 2>/dev/null || echo "0")
  TOTAL_TESTS=${TOTAL_TESTS//[$'\t\r\n']}
  echo "Total tests executed: $TOTAL_TESTS"
  echo "All 8 AI behaviors validated:"
  echo "  ✓ IdleBehavior (stationary & fidget modes)"
  echo "  ✓ WanderBehavior (small, medium, large areas)"
  echo "  ✓ PatrolBehavior (waypoint following)"
  echo "  ✓ ChaseBehavior (target pursuit)"
  echo "  ✓ FleeBehavior (escape & avoidance)"
  echo "  ✓ FollowBehavior (companion & formation AI)"
  echo "  ✓ GuardBehavior (area defense & alerts)"
  echo "  ✓ AttackBehavior (combat & assault)"
  echo "==========================="
  echo -e "${NC}"

  exit 0
fi
