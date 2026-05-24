#!/bin/bash
# Script to run the Thread-Safe AI Integration tests
# Copyright (c) 2025 Hammer Forged Games, MIT License

# Get the directory where this script is located and find project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
echo "Running Thread-Safe AI Integration tests..."

# Note: SCRIPT_DIR and PROJECT_ROOT already calculated at top of script

# Determine test executable path based on build type
if [ "$BUILD_TYPE" = "Debug" ]; then
  TEST_EXECUTABLE="$SCRIPT_DIR/../../bin/debug/thread_safe_ai_integration_tests"
else
  TEST_EXECUTABLE="$SCRIPT_DIR/../../bin/release/thread_safe_ai_integration_tests"
fi

# Verify executable exists
if [ ! -f "$TEST_EXECUTABLE" ]; then
  echo "Error: Test executable not found at '$TEST_EXECUTABLE'"
  exit 1
fi

# Run tests and save output
echo "Running Thread-Safe AI Integration tests..."

# Create the test_results directory if it doesn't exist
mkdir -p "$PROJECT_ROOT/test_results"

# Create a temporary file for test output
TEMP_OUTPUT="$PROJECT_ROOT/test_results/thread_safe_ai_integration_test_output.txt"

# Clear any existing output file
> "$TEMP_OUTPUT"

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
TEST_OPTS="--log_level=all"

# Run the tests with a clean environment and timeout
if [ -n "$TIMEOUT_CMD" ]; then
  if [ "$VERBOSE" = true ]; then
    echo "Running with options: $TEST_OPTS --catch_system_errors=no"
    $TIMEOUT_CMD 300s "$TEST_EXECUTABLE" $TEST_OPTS --catch_system_errors=no 2>&1 | tee "$TEMP_OUTPUT"
    TEST_RESULT=${PIPESTATUS[0]}
  else
    echo "Running tests..."
    $TIMEOUT_CMD 300s "$TEST_EXECUTABLE" --catch_system_errors=no 2>&1 | tee "$TEMP_OUTPUT"
    TEST_RESULT=${PIPESTATUS[0]}
  fi
else
  if [ "$VERBOSE" = true ]; then
    echo "Running with options: $TEST_OPTS --catch_system_errors=no"
    "$TEST_EXECUTABLE" $TEST_OPTS --catch_system_errors=no 2>&1 | tee "$TEMP_OUTPUT"
    TEST_RESULT=${PIPESTATUS[0]}
  else
    echo "Running tests..."
    "$TEST_EXECUTABLE" --catch_system_errors=no 2>&1 | tee "$TEMP_OUTPUT"
    TEST_RESULT=${PIPESTATUS[0]}
  fi
fi

# Extract performance metrics
echo "Extracting performance metrics..."
grep -E "time:|entities:|processed:|Concurrent processing time" "$TEMP_OUTPUT" > "$PROJECT_ROOT/test_results/thread_safe_ai_integration_performance_metrics.txt" || true

# Check for timeout
if [ -n "$TIMEOUT_CMD" ] && grep -q "Operation timed out" "$TEMP_OUTPUT"; then
  echo "⚠️ Test execution timed out after 300 seconds!"
  echo "Test execution timed out after 300 seconds!" >> "$TEMP_OUTPUT"
  exit 124
fi

# Extract test results information
TOTAL_TESTS=$(grep -E "Running [0-9]+ test cases" "$TEMP_OUTPUT" | grep -o "[0-9]\+")
if [ -z "$TOTAL_TESTS" ]; then
  TOTAL_TESTS="unknown number of"
fi

# First check for a clear success pattern and a successful process exit.
if [ $TEST_RESULT -eq 0 ] && grep -q "\*\*\* No errors detected\|All tests completed successfully\|TestCacheInvalidation completed" "$TEMP_OUTPUT"; then
  echo "✅ All Thread-Safe AI Integration tests passed!"

  # Mention the "Test is aborted" messages as informational only
  if grep -q "Test is aborted" "$TEMP_OUTPUT"; then
    echo "ℹ️ Note: 'Test is aborted' messages were detected but are harmless since all tests passed."
  fi
  exit 0
fi

# Check for other success patterns
if [ $TEST_RESULT -eq 0 ] && \
   (grep -q "test cases: $TOTAL_TESTS.*failed: 0" "$TEMP_OUTPUT" || \
   grep -q "Running $TOTAL_TESTS test case.*No errors detected" "$TEMP_OUTPUT" || \
   grep -q "successful: $TOTAL_TESTS" "$TEMP_OUTPUT"); then
  echo "✅ All Thread-Safe AI Integration tests passed!"
  exit 0
fi

# Only if no success pattern was found, check for errors
# Check for crash indicators during test execution (not after all tests passed)
if grep -q "memory access violation\|segmentation fault\|Segmentation fault\|Abort trap" "$TEMP_OUTPUT" && ! grep -q "\*\*\* No errors detected\|Tests completed successfully with known cleanup issue" "$TEMP_OUTPUT"; then
  echo "❌ Tests crashed! See $PROJECT_ROOT/test_results/thread_safe_ai_integration_test_output.txt for details."
  exit 1
fi

# Check for any failed assertions, but exclude "Test is aborted" as a fatal error
if grep -v "Test is aborted" "$TEMP_OUTPUT" | grep -q "fail\|error:\|assertion.*failed\|exception"; then
  echo "❌ Some tests failed! See $PROJECT_ROOT/test_results/thread_safe_ai_integration_test_output.txt for details."
  exit 1
fi

if [ $TEST_RESULT -eq 0 ]; then
  # All tests completed, and no explicit failures were found
  echo "✅ All Thread-Safe AI Integration tests have completed successfully!"
  exit 0
else
  echo "❌ Thread-Safe AI Integration tests failed or terminated early."
  echo "Review $PROJECT_ROOT/test_results/thread_safe_ai_integration_test_output.txt for details."

  # Show the beginning and end of the output for context
  echo "First few lines of test output:"
  head -5 "$TEMP_OUTPUT"
  echo "..."
  echo "Last few lines of test output:"
  tail -5 "$TEMP_OUTPUT"
  exit 1
fi
