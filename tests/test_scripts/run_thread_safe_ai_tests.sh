#!/bin/bash
# Script to run the Thread-Safe AI Manager tests
# Copyright (c) 2025 Hammer Forged Games, MIT License

# Navigate to script directory (in case script is run from elsewhere)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Create required directories
mkdir -p "$PROJECT_ROOT/test_results"

# Set default build type
BUILD_TYPE="Debug"
VERBOSE=false

# Process command-line options
while [[ $# -gt 0 ]]; do
  case $1 in
    --release)
      BUILD_TYPE="Release"
      shift
      ;;
    --verbose)
      VERBOSE=true
      shift
      ;;
    --help)
      echo "Usage: $0 [--release] [--verbose] [--help]"
      echo "  --release   Run the release build of the tests"
      echo "  --verbose   Show detailed test output"
      echo "  --help      Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--release] [--verbose] [--help]"
      exit 1
      ;;
  esac
done

# Run the tests
echo "Running Thread-Safe AI Manager tests..."

# Get the directory where this script is located and find project root
# (Already defined above)

# Determine test executable path based on build type
if [ "$BUILD_TYPE" = "Debug" ]; then
  TEST_EXECUTABLE="$PROJECT_ROOT/bin/debug/thread_safe_ai_manager_tests"
else
  TEST_EXECUTABLE="$PROJECT_ROOT/bin/release/thread_safe_ai_manager_tests"
fi

# Verify executable exists
if [ ! -f "$TEST_EXECUTABLE" ]; then
  echo "Error: Test executable not found at '$TEST_EXECUTABLE'"
  # Attempt to find the executable
  echo "Searching for test executable..."
  FOUND_EXECUTABLE=$(find "$PROJECT_ROOT/bin" -name "thread_safe_ai_manager_tests" -type f -executable | head -n 1)
  if [ -n "$FOUND_EXECUTABLE" ]; then
    echo "Found executable at: $FOUND_EXECUTABLE"
    TEST_EXECUTABLE="$FOUND_EXECUTABLE"
  else
    echo "Could not find the test executable. Build may have failed or placed the executable in an unexpected location."
    exit 1
  fi
fi

# Run tests and save output

# Ensure test_results directory exists
mkdir -p "$PROJECT_ROOT/test_results"

# Use the output file directly instead of a temporary file
TEMP_OUTPUT="$PROJECT_ROOT/test_results/thread_safe_ai_test_output.txt"

# Check for timeout command availability
TIMEOUT_CMD=""
if command -v timeout &> /dev/null; then
  TIMEOUT_CMD="timeout"
elif command -v gtimeout &> /dev/null; then
  TIMEOUT_CMD="gtimeout"
else
  echo "Warning: Neither 'timeout' nor 'gtimeout' command found. Tests will run without timeout protection."
fi

# Set test command options
TEST_OPTS="--log_level=all --catch_system_errors=no --detect_memory_leak=0 --build_info=no --detect_fp_exceptions=no"
if [ "$VERBOSE" = true ]; then
  TEST_OPTS="$TEST_OPTS --report_level=detailed"
fi

# Run the tests with additional safeguards
echo "Running with options: $TEST_OPTS"
if [ -n "$TIMEOUT_CMD" ]; then
  # Use bash trap to handle segmentation faults (signal 11)
  # This prevents the script from exiting when threads cause segfaults during cleanup
  trap 'echo "Signal 11 (segmentation fault) caught but continuing..." >> "$TEMP_OUTPUT"' SEGV
  # Run timeout with --preserve-status to preserve the exit status of the command
  $TIMEOUT_CMD --preserve-status 30s "$TEST_EXECUTABLE" $TEST_OPTS 2>&1 | tee "$TEMP_OUTPUT"
  TEST_RESULT=${PIPESTATUS[0]}
  trap - SEGV
else
  trap 'echo "Signal 11 (segmentation fault) caught but continuing..." >> "$TEMP_OUTPUT"' SEGV
  "$TEST_EXECUTABLE" $TEST_OPTS 2>&1 | tee "$TEMP_OUTPUT"
  TEST_RESULT=${PIPESTATUS[0]}
  trap - SEGV
fi

# Print exit code for debugging
echo "Test exit code: $TEST_RESULT" >> "$TEMP_OUTPUT"

# Handle timeout scenario and core dumps
if [ -n "$TIMEOUT_CMD" ] && [ $TEST_RESULT -eq 124 ]; then
  echo "⚠️ Test execution timed out after 30 seconds!"
  echo "Test execution timed out after 30 seconds!" >> "$TEMP_OUTPUT"
elif [ $TEST_RESULT -eq 139 ] || grep -q "dumped core\|Segmentation fault" "$TEMP_OUTPUT"; then
  echo "⚠️ Test execution completed but crashed during cleanup (segmentation fault)!"
  echo "Test execution completed but crashed during cleanup (segmentation fault)!" >> "$TEMP_OUTPUT"
fi

# Extract performance metrics
echo "Extracting performance metrics..."
grep -E "time:|entities:|processed:|Concurrent processing time" "$TEMP_OUTPUT" > "$PROJECT_ROOT/test_results/thread_safe_ai_performance_metrics.txt" || true

# Check test status
if [ $TEST_RESULT -eq 124 ]; then
  echo "❌ Tests timed out! See $PROJECT_ROOT/test_results/thread_safe_ai_test_output.txt for details."
  exit $TEST_RESULT
elif [ $TEST_RESULT -ne 0 ] || grep -q "failure\|test cases failed\|assertion failed\|error:" "$TEMP_OUTPUT"; then
  echo "❌ Some tests failed! See $PROJECT_ROOT/test_results/thread_safe_ai_test_output.txt for details."
  exit 1
else
  echo "✅ All Thread-Safe AI Manager tests passed!"
  exit 0
fi
