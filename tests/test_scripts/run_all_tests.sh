#!/bin/bash

# Script to run all test shell scripts sequentially

# Set up colored output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Directory where all scripts are located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Process command line arguments
VERBOSE=false
RUN_CORE=true
RUN_BENCHMARKS=true

for arg in "$@"; do
  case $arg in
    --verbose)
      VERBOSE=true
      shift
      ;;
    --errors-only)
      ERRORS_ONLY=true
      shift
      ;;
    --core-only)
      RUN_CORE=true
      RUN_BENCHMARKS=false
      shift
      ;;
    --benchmarks-only)
      RUN_CORE=false
      RUN_BENCHMARKS=true
      shift
      ;;
    --no-benchmarks)
      RUN_CORE=true
      RUN_BENCHMARKS=false
      shift
      ;;
    --help)
      echo -e "${BLUE}All Tests Runner${NC}"
      echo -e "Usage: ./run_all_tests.sh [options]"
      echo -e "\nOptions:"
      echo -e "  --verbose         Run tests with verbose output"
      echo -e "  --errors-only     Filter output to show only warnings and errors"
      echo -e "  --core-only       Run only core functionality tests (fast)"
      echo -e "  --benchmarks-only Run only performance benchmarks (slow)"
      echo -e "  --no-benchmarks   Run core tests but skip benchmarks"
      echo -e "  --help            Show this help message"
      echo -e "\nTest Categories:"
      echo -e "  Core Tests:       Thread, AI, Behavior, GameState, Save, Settings, Event, ParticleManager, Collision, Pathfinding,"
      echo -e "                    Camera, InputManager, SIMD, BufferReuse, Rendering, LoadingState, UIManager,"
      echo -e "                    GameTimeManager, Controllers (Time, Weather, DayNight, Combat), GPU (if SDL3_GPU enabled),"
      echo -e "                    Integration tests (AI-Collision, Event Coordination)"
      echo -e "  Benchmarks:       AI scaling, EventManager scaling, ParticleManager, Collision system, Pathfinder,"
      echo -e "                    SIMD performance, GPU frame timing, and Integrated system benchmarks"
      echo -e "\nExecution Time:"
      echo -e "  Core tests:       ~2-5 minutes total"
      echo -e "  Benchmarks:       ~5-15 minutes total"
      echo -e "  All tests:        ~7-20 minutes total"
      echo -e "\nExamples:"
      echo -e "  ./run_all_tests.sh                 # Run all tests"
      echo -e "  ./run_all_tests.sh --core-only     # Quick validation"
      echo -e "  ./run_all_tests.sh --no-benchmarks # Skip slow benchmarks"
      echo -e "  ./run_all_tests.sh --benchmarks-only --verbose # Performance testing"
      exit 0
      ;;
  esac
done

# Define test categories
# Core functionality tests (fast execution)
CORE_TEST_SCRIPTS=(
  "$SCRIPT_DIR/run_thread_tests.sh"
  "$SCRIPT_DIR/run_buffer_utilization_tests.sh"
  "$SCRIPT_DIR/run_thread_safe_ai_tests.sh"
  "$SCRIPT_DIR/run_thread_safe_ai_integration_tests.sh"
  "$SCRIPT_DIR/run_ai_optimization_tests.sh"
  "$SCRIPT_DIR/run_behavior_functionality_tests.sh"
  "$SCRIPT_DIR/run_save_tests.sh"
  "$SCRIPT_DIR/run_settings_tests.sh"
  "$SCRIPT_DIR/run_game_state_manager_tests.sh"
  "$SCRIPT_DIR/run_event_tests.sh"
  "$SCRIPT_DIR/run_weather_event_tests.sh"
  "$SCRIPT_DIR/run_particle_manager_tests.sh"
  "$SCRIPT_DIR/run_resource_tests.sh"
  "$SCRIPT_DIR/run_resource_edge_case_tests.sh"
  "$SCRIPT_DIR/run_json_reader_tests.sh"
  "$SCRIPT_DIR/run_world_generator_tests.sh"
  "$SCRIPT_DIR/run_world_manager_tests.sh"
  "$SCRIPT_DIR/run_world_manager_event_integration_tests.sh"
  "$SCRIPT_DIR/run_world_resource_manager_tests.sh"
  "$SCRIPT_DIR/run_game_time_tests.sh"
  "$SCRIPT_DIR/run_controller_tests.sh"
  "$SCRIPT_DIR/run_collision_tests.sh"
  "$SCRIPT_DIR/run_pathfinding_tests.sh"
  "$SCRIPT_DIR/run_collision_pathfinding_integration_tests.sh"
  "$SCRIPT_DIR/run_pathfinder_ai_contention_tests.sh"
  "$SCRIPT_DIR/run_ai_collision_integration_tests.sh"
  "$SCRIPT_DIR/run_event_coordination_integration_tests.sh"
  "$SCRIPT_DIR/run_camera_tests.sh"
  "$SCRIPT_DIR/run_input_manager_tests.sh"
  "$SCRIPT_DIR/run_simd_correctness_tests.sh"
  "$SCRIPT_DIR/run_buffer_reuse_tests.sh"
  "$SCRIPT_DIR/run_rendering_pipeline_tests.sh"
  "$SCRIPT_DIR/run_loading_state_tests.sh"
  "$SCRIPT_DIR/run_ui_manager_functional_tests.sh"
  "$SCRIPT_DIR/run_entity_state_manager_tests.sh"
  "$SCRIPT_DIR/run_entity_data_manager_tests.sh"
  "$SCRIPT_DIR/run_ai_manager_edm_integration_tests.sh"
  "$SCRIPT_DIR/run_sidecar_tests.sh"
  "$SCRIPT_DIR/run_collision_manager_edm_integration_tests.sh"
  "$SCRIPT_DIR/run_pathfinder_manager_edm_integration_tests.sh"
  "$SCRIPT_DIR/run_pathfinder_manager_tests.sh"
  "$SCRIPT_DIR/run_projectile_manager_tests.sh"
  "$SCRIPT_DIR/run_npc_memory_tests.sh"
  "$SCRIPT_DIR/run_gpu_tests.sh"
  "$SCRIPT_DIR/run_crowd_runtime_tests.sh"
  "$SCRIPT_DIR/run_manager_runtime_tests.sh"
  "$SCRIPT_DIR/run_frame_profiler_tests.sh"
)

# Performance scaling benchmarks (slow execution)
BENCHMARK_TEST_SCRIPTS=(
  "$SCRIPT_DIR/run_event_scaling_benchmark.sh"
  "$SCRIPT_DIR/run_ai_benchmark.sh"
  "$SCRIPT_DIR/run_particle_manager_benchmark.sh"
  "$SCRIPT_DIR/run_collision_scaling_benchmark.sh"
  "$SCRIPT_DIR/run_pathfinder_benchmark.sh"
  "$SCRIPT_DIR/run_simd_benchmark.sh"
  "$SCRIPT_DIR/run_gpu_frame_benchmark.sh"
  "$SCRIPT_DIR/run_integrated_benchmark.sh"
  "$SCRIPT_DIR/run_background_simulation_manager_benchmark.sh"
  "$SCRIPT_DIR/run_adaptive_threading_analysis.sh"
  "$SCRIPT_DIR/run_projectile_benchmark.sh"
)

# Build the test scripts array based on user selection
TEST_SCRIPTS=()
if [ "$RUN_CORE" = true ]; then
  TEST_SCRIPTS+=("${CORE_TEST_SCRIPTS[@]}")
fi
if [ "$RUN_BENCHMARKS" = true ]; then
  TEST_SCRIPTS+=("${BENCHMARK_TEST_SCRIPTS[@]}")
fi

# Create a directory for the combined test results
mkdir -p "$PROJECT_ROOT/test_results/combined"
COMBINED_RESULTS="$PROJECT_ROOT/test_results/combined/all_tests_results.txt"
echo "All Tests Run $(date)" > "$COMBINED_RESULTS"

# Track overall success
OVERALL_SUCCESS=true
PASSED_COUNT=0
FAILED_COUNT=0
TOTAL_COUNT=${#TEST_SCRIPTS[@]}

# Function to run a test script
run_test_script() {
  local script=$1
  local script_name=$(basename "$script")
  local args=""

  # Pass along relevant flags (but not --errors-only, we handle that here)
  if [ "$VERBOSE" = true ]; then
    args="$args --verbose"
  fi

  # Special handling for scaling benchmarks and stress tests
  local is_benchmark=false
  if [[ "$script_name" == *"benchmark"* ]] || [[ "$script_name" == *"scaling"* ]] || [[ "$script_name" == *"stress"* ]]; then
    is_benchmark=true
    echo -e "\n${MAGENTA}=====================================================${NC}"
    echo -e "${CYAN}Running performance benchmark: ${YELLOW}$script_name${NC}"
    echo -e "${MAGENTA}This may take several minutes...${NC}"
    echo -e "${MAGENTA}=====================================================${NC}"
  else
    echo -e "\n${MAGENTA}=====================================================${NC}"
    echo -e "${CYAN}Running test script: ${YELLOW}$script_name${NC}"
    echo -e "${MAGENTA}=====================================================${NC}"
  fi

  # Check if the script exists and is executable
  if [ ! -f "$script" ]; then
    echo -e "${RED}Script not found: $script${NC}"
    echo "FAILED: Script not found: $script_name" >> "$COMBINED_RESULTS"
    OVERALL_SUCCESS=false
    ((FAILED_COUNT++))
    return 1
  fi

  # Make sure the script is executable
  chmod +x "$script"

  # Run the script with provided arguments
  if [ "$ERRORS_ONLY" = true ]; then
    # Redirect all output to temp file, preserve exit code
    local temp_output=$(mktemp)
    $script $args >"$temp_output" 2>&1
    local result=$?
    
    # Show only script name and result - suppress all normal output
    # Only show content if there are actual test failures
    # Filter out "has passed" lines first to avoid false positives from "check !failed has passed"
    if [ $result -ne 0 ] || grep -v "has passed" "$temp_output" | grep -qE "(BOOST_CHECK.*failed|BOOST_REQUIRE.*failed|has failed|FAILED.*test|BUILD FAILED|compilation.*failed|Segmentation fault|Assertion.*failed|\[error\].*test|\*\*\* FAILURE|✗.*failed)"; then
      echo -e "\n${RED}Test failures detected in $script_name:${NC}"
      grep -v "has passed" "$temp_output" | grep -E "(BOOST_CHECK.*failed|BOOST_REQUIRE.*failed|has failed|FAILED.*test|BUILD FAILED|compilation.*failed|Segmentation fault|Assertion.*failed|\[error\].*test|\*\*\* FAILURE|✗.*failed)" || echo "Script failed with exit code $result"
    fi
    rm -f "$temp_output"
  else
    $script $args
    local result=$?
  fi

  if [ $result -eq 0 ]; then
    if [ "$is_benchmark" = true ]; then
      echo -e "\n${GREEN}✓ Performance benchmark $script_name completed successfully${NC}"
    else
      echo -e "\n${GREEN}✓ Test script $script_name completed successfully${NC}"
    fi
    echo "PASSED: $script_name" >> "$COMBINED_RESULTS"
    ((PASSED_COUNT++))
    return 0
  else
    if [ "$is_benchmark" = true ]; then
      echo -e "\n${RED}✗ Performance benchmark $script_name failed with exit code $result${NC}"
    else
      echo -e "\n${RED}✗ Test script $script_name failed with exit code $result${NC}"
    fi
    echo "FAILED: $script_name (exit code: $result)" >> "$COMBINED_RESULTS"
    OVERALL_SUCCESS=false
    ((FAILED_COUNT++))
    return 1
  fi
}

# Print header with execution plan
echo -e "${BLUE}======================================================${NC}"
echo -e "${BLUE}              Running Test Scripts                    ${NC}"
echo -e "${BLUE}======================================================${NC}"

# Show execution plan
if [ "$RUN_CORE" = true ] && [ "$RUN_BENCHMARKS" = true ]; then
  echo -e "${YELLOW}Execution Plan: All tests (${#CORE_TEST_SCRIPTS[@]} core + ${#BENCHMARK_TEST_SCRIPTS[@]} benchmarks)${NC}"
  echo -e "${YELLOW}Note: Performance benchmarks will run last and may take several minutes${NC}"
elif [ "$RUN_CORE" = true ]; then
  echo -e "${YELLOW}Execution Plan: Core functionality tests only (${#CORE_TEST_SCRIPTS[@]} tests)${NC}"
  echo -e "${GREEN}Fast execution mode - skipping performance benchmarks${NC}"
elif [ "$RUN_BENCHMARKS" = true ]; then
  echo -e "${YELLOW}Execution Plan: Performance benchmarks only (${#BENCHMARK_TEST_SCRIPTS[@]} benchmarks)${NC}"
  echo -e "${YELLOW}Note: This will take several minutes to complete${NC}"
else
  echo -e "${RED}Error: No test categories selected${NC}"
  exit 1
fi

echo -e "${CYAN}Found ${#TEST_SCRIPTS[@]} test scripts to run${NC}"

# Run each test script
for script in "${TEST_SCRIPTS[@]}"; do
  run_test_script "$script"
done

# Print summary
echo -e "\n${BLUE}======================================================${NC}"
echo -e "${BLUE}                  Test Summary                       ${NC}"
echo -e "${BLUE}======================================================${NC}"
echo -e "Total scripts: $TOTAL_COUNT"
echo -e "${GREEN}Passed: $PASSED_COUNT${NC}"
echo -e "${RED}Failed: $FAILED_COUNT${NC}"

# Save summary to results file
echo -e "\nSummary:" >> "$COMBINED_RESULTS"
echo "Total: $TOTAL_COUNT" >> "$COMBINED_RESULTS"
echo "Passed: $PASSED_COUNT" >> "$COMBINED_RESULTS"
echo "Failed: $FAILED_COUNT" >> "$COMBINED_RESULTS"
echo "Completed at: $(date)" >> "$COMBINED_RESULTS"

# Exit with appropriate status code and summary
if [ "$OVERALL_SUCCESS" = true ]; then
  if [ "$RUN_CORE" = true ] && [ "$RUN_BENCHMARKS" = true ]; then
    echo -e "\n${GREEN}All test scripts completed successfully!${NC}"
  elif [ "$RUN_CORE" = true ]; then
    echo -e "\n${GREEN}All core functionality tests completed successfully!${NC}"
    echo -e "${CYAN}To run performance benchmarks: ./run_all_tests.sh --benchmarks-only${NC}"
  elif [ "$RUN_BENCHMARKS" = true ]; then
    echo -e "\n${GREEN}All performance benchmarks completed successfully!${NC}"
  fi
  exit 0
else
  if [ "$RUN_CORE" = true ] && [ "$RUN_BENCHMARKS" = true ]; then
    echo -e "\n${RED}Some test scripts failed. Please check the individual test results.${NC}"
  elif [ "$RUN_CORE" = true ]; then
    echo -e "\n${RED}Some core functionality tests failed. Please check the individual test results.${NC}"
  elif [ "$RUN_BENCHMARKS" = true ]; then
    echo -e "\n${RED}Some performance benchmarks failed. Please check the individual test results.${NC}"
  fi
  echo -e "Combined results saved to: ${YELLOW}$COMBINED_RESULTS${NC}"
  exit 1
fi
