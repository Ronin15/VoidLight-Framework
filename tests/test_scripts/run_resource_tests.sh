#!/bin/bash

# Resource Manager Test Runner
# Copyright (c) 2025 Hammer Forged Games

# Set up colored output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Process command line arguments
VERBOSE=false
TEST_FILTER=""

for arg in "$@"; do
  case $arg in
    --verbose)
      VERBOSE=true
      shift
      ;;
    --resource-template-test)
      TEST_FILTER="resource_template_manager_tests"
      shift
      ;;
    --resource-factory-test)
      TEST_FILTER="resource_factory_tests"
      shift
      ;;
    --resource-template-json-test)
      TEST_FILTER="resource_template_manager_json_tests"
      shift
      ;;
    --world-resource-test)
      TEST_FILTER="world_resource_manager_tests"
      shift
      ;;
    --resource-event-test)
      TEST_FILTER="resource_change_event_tests"
      shift
      ;;
    --resource-path-test)
      TEST_FILTER="resource_path_tests"
      shift
      ;;
    --edge-case-test)
      TEST_FILTER="resource_edge_case_tests"
      shift
      ;;
    --integration-test)
      TEST_FILTER="resource_integration_tests"
      shift
      ;;
    --architecture-test)
      TEST_FILTER="resource_architecture_tests"
      shift
      ;;
    --help)
      echo -e "${BLUE}Resource Manager Test Runner${NC}"
      echo -e "Usage: ./run_resource_tests.sh [options]"
      echo -e "\nOptions:"
      echo -e "  --verbose                    Run tests with verbose output"
      echo -e "  --resource-template-test     Run only resource template manager tests"
      echo -e "  --resource-factory-test      Run only resource factory tests"
      echo -e "  --resource-template-json-test Run only resource template JSON tests"
      echo -e "  --world-resource-test        Run only world resource manager tests"
      echo -e "  --resource-event-test        Run only resource event tests"
      echo -e "  --resource-path-test         Run only resource path resolution tests"
      echo -e "  --edge-case-test             Run only resource edge case tests"
      echo -e "  --integration-test           Run only resource integration tests"
      echo -e "  --architecture-test          Run only resource architecture tests"
      echo -e "  --help                       Show this help message"
      echo -e "\nTest Suite Overview:"
      echo -e "  Resource Template Tests:       Core resource template/type definitions"
      echo -e "  Resource Factory Tests:        Resource creation and factory patterns"
      echo -e "  Resource Template JSON Tests:  JSON-based resource template operations"
      echo -e "  World Resource Tests:          Global resource quantity tracking"
      echo -e "  Resource Event Tests:          Resource change event handling"
      echo -e "  Resource Path Tests:           Search-path priority and resolution"
      echo -e "  Edge Case Tests:               Resource boundary and error conditions"
      echo -e "  Integration Tests:             Cross-system resource operations"
      echo -e "  Architecture Tests:            Resource-Entity separation validation"
      echo -e "\nExecution Time:"
      echo -e "  Full test suite:               ~2-3 seconds"
      echo -e "  Individual tests:              ~200-500ms each"
      echo -e "\nExamples:"
      echo -e "  ./run_resource_tests.sh                          # Run all resource tests"
      echo -e "  ./run_resource_tests.sh --verbose                # Run with detailed output"
      echo -e "  ./run_resource_tests.sh --world-resource-test    # Run only world resource tests"
      echo -e "  ./bin/debug/resource_template_manager_json_tests --run_test=\"ResourceTemplateManagerJsonTestSuite/TestLoadDefaultResources\" # Run default catalog test"
      exit 0
      ;;
  esac
done

echo -e "${BLUE}Running Resource Manager System Tests...${NC}"

# Get the directory where this script is located and find project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Set up test environment
export SDL_VIDEODRIVER=dummy
export DISPLAY=:99.0

# List of test binaries (in order)
if [ -n "$TEST_FILTER" ]; then
    TESTS=("$TEST_FILTER")
else
    TESTS=(
        "resource_template_manager_tests"
        "resource_factory_tests"
        "resource_template_manager_json_tests"
        "world_resource_manager_tests"
        "resource_change_event_tests"
        "resource_path_tests"
        "resource_edge_case_tests"
        "resource_integration_tests"
        "resource_architecture_tests"
    )
fi

# Create test_results directory if it doesn't exist
mkdir -p "$PROJECT_ROOT/test_results"

# Create output file for results
RESULTS_FILE="$PROJECT_ROOT/test_results/resource_tests_$(date +%Y%m%d_%H%M%S).txt"
echo "Resource Manager Tests Run $(date)" > "$RESULTS_FILE"
echo "=========================================" >> "$RESULTS_FILE"

RESULT=0
PASSED_COUNT=0
FAILED_COUNT=0
TOTAL_COUNT=${#TESTS[@]}

for TEST in "${TESTS[@]}"; do
    echo -e "\n${BLUE}=========================================${NC}"
    echo -e "${CYAN}Running $TEST...${NC}"
    echo -e "${BLUE}=========================================${NC}"
    
    # Check if test executable exists
    TEST_EXECUTABLE="$PROJECT_ROOT/bin/debug/$TEST"
    if [ ! -f "$TEST_EXECUTABLE" ]; then
        echo -e "${RED}Test executable not found at $TEST_EXECUTABLE${NC}"
        echo -e "${YELLOW}Searching for test executable...${NC}"
        FOUND_EXECUTABLE=$(find "$PROJECT_ROOT" -name "$TEST" -type f -executable | head -n 1)
        if [ -n "$FOUND_EXECUTABLE" ]; then
            TEST_EXECUTABLE="$FOUND_EXECUTABLE"
            echo -e "${GREEN}Found test executable at $TEST_EXECUTABLE${NC}"
        else
            echo -e "${RED}Could not find test executable!${NC}"
            echo "FAILED: $TEST - executable not found" >> "$RESULTS_FILE"
            RESULT=1
            ((FAILED_COUNT++))
            continue
        fi
    fi
    
    LOG_FILE="$PROJECT_ROOT/test_results/${TEST}_output.log"
    
    # Set test options
    if [ "$VERBOSE" = true ]; then
        TEST_OPTS="--log_level=all --report_level=detailed"
    else
        TEST_OPTS="--log_level=error --report_level=short"
    fi
    
    # Change to project root directory before running tests
    cd "$PROJECT_ROOT"
    
    # Run test with timeout protection
    echo -e "${GREEN}Running test with options: $TEST_OPTS${NC}"
    echo "Running $TEST at $(date)" >> "$RESULTS_FILE"
    echo "Command: $TEST_EXECUTABLE $TEST_OPTS" >> "$RESULTS_FILE"
    echo "=========================================" >> "$RESULTS_FILE"
    
    # Run in background with timeout
    "$TEST_EXECUTABLE" $TEST_OPTS > "$LOG_FILE" 2>&1 &
    PID=$!
    TIMEOUT=60
    
    while [ $TIMEOUT -gt 0 ] && kill -0 $PID 2>/dev/null; do
        sleep 1
        TIMEOUT=$((TIMEOUT - 1))
    done
    
    if kill -0 $PID 2>/dev/null; then
        echo -e "${RED}Test $TEST timed out after 60 seconds, killing process${NC}"
        kill -9 $PID 2>/dev/null
        wait $PID 2>/dev/null || true
        echo "Test execution timed out after 60 seconds" >> "$LOG_FILE"
        echo "FAILED: $TEST - timeout" >> "$RESULTS_FILE"
        RESULT=1
        ((FAILED_COUNT++))
    else
        wait $PID
        EXIT_CODE=$?
        
        if [ $EXIT_CODE -ne 0 ]; then
            echo -e "${RED}✗ Test $TEST failed with exit code $EXIT_CODE${NC}"
            if [ "$VERBOSE" = true ]; then
                cat "$LOG_FILE"
            else
                # Show last few lines for context
                echo -e "${YELLOW}Last 10 lines of test output:${NC}"
                tail -n 10 "$LOG_FILE"
            fi
            echo "FAILED: $TEST (exit code: $EXIT_CODE)" >> "$RESULTS_FILE"
            cat "$LOG_FILE" >> "$RESULTS_FILE"
            RESULT=1
            ((FAILED_COUNT++))
        else
            echo -e "${GREEN}✓ Test $TEST passed${NC}"
            if [ "$VERBOSE" = true ]; then
                cat "$LOG_FILE"
            fi
            echo "PASSED: $TEST" >> "$RESULTS_FILE"
            # Extract test summary
            grep -E "Test case.*passed|assertions out of.*passed|No errors detected" "$LOG_FILE" >> "$RESULTS_FILE" 2>/dev/null || true
            ((PASSED_COUNT++))
        fi
        
        # Append test output to results file
        echo >> "$RESULTS_FILE"
        cat "$LOG_FILE" >> "$RESULTS_FILE"
        echo "=========================================" >> "$RESULTS_FILE"
    fi
    
    # Clean up log file
    rm -f "$LOG_FILE"
done

# Extract performance metrics and test statistics
echo -e "${YELLOW}Extracting test metrics...${NC}"
METRICS_FILE="$PROJECT_ROOT/test_results/resource_test_metrics.txt"
echo "============ RESOURCE TEST METRICS ============" > "$METRICS_FILE"
echo "Date: $(date)" >> "$METRICS_FILE"
echo "Total tests run: $TOTAL_COUNT" >> "$METRICS_FILE"
echo "Passed: $PASSED_COUNT" >> "$METRICS_FILE"
echo "Failed: $FAILED_COUNT" >> "$METRICS_FILE"
echo "===============================================" >> "$METRICS_FILE"
echo >> "$METRICS_FILE"

# Extract timing information and test case counts
grep -E "time:|microseconds|Test case.*passed|assertions out of.*passed" "$RESULTS_FILE" >> "$METRICS_FILE" 2>/dev/null || true

# Extract test case details
echo >> "$METRICS_FILE"
echo "============ TEST CASE SUMMARY ============" >> "$METRICS_FILE"
for TEST in "${TESTS[@]}"; do
    echo "Test: $TEST" >> "$METRICS_FILE"
    grep -A 5 -B 1 "Running $TEST" "$RESULTS_FILE" | grep -E "Test case|passed|failed" >> "$METRICS_FILE" 2>/dev/null || true
    echo >> "$METRICS_FILE"
done
echo "===========================================" >> "$METRICS_FILE"

# Print summary
echo -e "\n${BLUE}======================================================${NC}"
echo -e "${BLUE}                Resource Test Summary                 ${NC}"
echo -e "${BLUE}======================================================${NC}"
echo -e "Total tests: $TOTAL_COUNT"
echo -e "${GREEN}Passed: $PASSED_COUNT${NC}"
echo -e "${RED}Failed: $FAILED_COUNT${NC}"

# Save final summary to results file
echo >> "$RESULTS_FILE"
echo "============ FINAL SUMMARY ============" >> "$RESULTS_FILE"
echo "Completed at: $(date)" >> "$RESULTS_FILE"
echo "Total: $TOTAL_COUNT" >> "$RESULTS_FILE"
echo "Passed: $PASSED_COUNT" >> "$RESULTS_FILE"
echo "Failed: $FAILED_COUNT" >> "$RESULTS_FILE"
echo "=======================================" >> "$RESULTS_FILE"

if [ $RESULT -eq 0 ]; then
    echo -e "\n${GREEN}✓ All Resource Tests Completed Successfully!${NC}"
    echo -e "${BLUE}Results saved to:${NC}"
    echo -e "  - Full log: ${BLUE}$RESULTS_FILE${NC}"
    echo -e "  - Metrics: ${BLUE}$METRICS_FILE${NC}"
    exit 0
else
    echo -e "\n${RED}✗ Some Resource Tests Failed or Timed Out!${NC}"
    echo -e "${YELLOW}Please check the detailed results for failure information.${NC}"
    echo -e "${BLUE}Results saved to:${NC}"
    echo -e "  - Full log: ${BLUE}$RESULTS_FILE${NC}"
    echo -e "  - Metrics: ${BLUE}$METRICS_FILE${NC}"
    
    # Print failed test summary
    echo -e "\n${YELLOW}Failed Tests Summary:${NC}"
    grep "FAILED:" "$RESULTS_FILE" | while read line; do
        echo -e "  - ${RED}$line${NC}"
    done
    
    exit 1
fi
