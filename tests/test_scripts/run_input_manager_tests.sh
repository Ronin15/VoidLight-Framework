#!/bin/bash
# Script to run the Input Manager Tests
# Copyright (c) 2025 Hammer Forged Games, MIT License

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${YELLOW}Running Input Manager Tests...${NC}"

BUILD_TYPE="Debug"
VERBOSE=false

while [[ $# -gt 0 ]]; do
  case $1 in
    --debug) BUILD_TYPE="Debug"; shift ;;
    --release) BUILD_TYPE="Release"; shift ;;
    --verbose) VERBOSE=true; shift ;;
    --help)
      echo "Usage: $0 [--debug] [--release] [--verbose] [--help]"
      exit 0
      ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

mkdir -p "$PROJECT_ROOT/test_results"

TEST_OPTS="--log_level=all --catch_system_errors=no"
if [ "$VERBOSE" = true ]; then
  TEST_OPTS="$TEST_OPTS --report_level=detailed"
fi

if [ "$BUILD_TYPE" = "Debug" ]; then
  BIN_DIR="$PROJECT_ROOT/bin/debug"
else
  BIN_DIR="$PROJECT_ROOT/bin/release"
fi

TEST_EXECUTABLES=(
  "input_manager_tests"
  "input_manager_command_tests"
)

FINAL_RESULT=0
for EXEC in "${TEST_EXECUTABLES[@]}"; do
  TEST_EXECUTABLE="$BIN_DIR/$EXEC"
  OUTPUT_FILE="$PROJECT_ROOT/test_results/${EXEC}_output.txt"

  if [ ! -f "$TEST_EXECUTABLE" ]; then
    echo -e "${RED}Error: Test executable not found at '$TEST_EXECUTABLE'${NC}"
    exit 1
  fi

  "$TEST_EXECUTABLE" $TEST_OPTS 2>&1 | tee "$OUTPUT_FILE"
  TEST_RESULT=${PIPESTATUS[0]}

  if [ $TEST_RESULT -ne 0 ] || grep -q "failure\|test cases failed\|fatal error" "$OUTPUT_FILE"; then
    echo -e "${RED}❌ Some tests failed in $EXEC! See $OUTPUT_FILE for details.${NC}"
    FINAL_RESULT=1
  else
    echo -e "${GREEN}✅ $EXEC passed!${NC}"
  fi
done

if [ $FINAL_RESULT -ne 0 ]; then
  exit $FINAL_RESULT
fi

echo -e "${GREEN}✅ All Input Manager tests passed!${NC}"
exit 0
