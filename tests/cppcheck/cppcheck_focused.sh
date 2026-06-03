#!/bin/bash
# Simple focused cppcheck analysis for VoidLight-Framework
# This script runs cppcheck with optimized settings to show only real issues

set -e
set -o pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== VoidLight-Framework - Focused Cppcheck Analysis ===${NC}"
echo ""

# Check if cppcheck is available
if ! command -v cppcheck &> /dev/null; then
    echo -e "${RED}Error: cppcheck not found. Please install cppcheck first.${NC}"
    exit 1
fi

# Run focused analysis - only real issues
echo -e "${YELLOW}Running focused analysis (errors, warnings, performance issues only)...${NC}"
echo ""

# Get script directory to handle relative paths correctly
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# compile_commands.json stores the source root spelling used by CMake. Use that
# for cppcheck filters so symlink/case differences do not exclude all files.
FILTER_ROOT="$PROJECT_ROOT"
if [ -f "$PROJECT_ROOT/compile_commands.json" ]; then
    COMPILE_ROOT=$(grep -m 1 '"file": ".*/src/' "$PROJECT_ROOT/compile_commands.json" 2>/dev/null | \
        sed 's/.*"file": "//;s|/src/.*||' || true)
    if [ -n "$COMPILE_ROOT" ]; then
        FILTER_ROOT="$COMPILE_ROOT"
    fi
fi

# Run cppcheck and capture output for counting
TEMP_OUTPUT=$(mktemp)

# Determine number of CPU cores for parallel analysis
if command -v nproc &> /dev/null; then
    JOBS=$(nproc)
elif command -v sysctl &> /dev/null; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=4
fi
echo -e "${BLUE}Using $JOBS parallel jobs${NC}"

# Timeout in seconds (5 minutes)
TIMEOUT_SECONDS=300

# Check compile_commands.json exists for proper cross-TU analysis
if [ -f "$PROJECT_ROOT/compile_commands.json" ]; then
    echo -e "${BLUE}Using compile_commands.json for improved analysis${NC}"
    echo -e "${YELLOW}Analysis may take 1-3 minutes...${NC}"
    set +e
    timeout $TIMEOUT_SECONDS cppcheck \
        --project="$PROJECT_ROOT/compile_commands.json" \
        --file-filter="$FILTER_ROOT/src/*" \
        --file-filter="$FILTER_ROOT/include/*" \
        -i"$PROJECT_ROOT/build" \
        -i"$PROJECT_ROOT/build/*" \
        -j$JOBS \
        --enable=warning,style,performance,portability \
        --library=std,posix \
        --library="$SCRIPT_DIR/cppcheck_lib.cfg" \
        --suppressions-list="$SCRIPT_DIR/cppcheck_suppressions.txt" \
        --std=c++20 \
        --quiet \
        --template='{file}:{line}: [{severity}] {message}' \
        2>&1 | tee "$TEMP_OUTPUT"
    CPPCHECK_EXIT=$?
    set -e
else
    echo -e "${YELLOW}Warning: compile_commands.json not found, using manual include paths${NC}"
    echo "Run cmake first for better analysis: cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug"
    set +e
    timeout $TIMEOUT_SECONDS cppcheck \
        -I"$PROJECT_ROOT/include" \
        -I"$PROJECT_ROOT/src" \
        -j$JOBS \
        --enable=warning,style,performance,portability \
        --library=std,posix \
        --library="$SCRIPT_DIR/cppcheck_lib.cfg" \
        --suppressions-list="$SCRIPT_DIR/cppcheck_suppressions.txt" \
        --platform=unix64 \
        --std=c++20 \
        --quiet \
        --template='{file}:{line}: [{severity}] {message}' \
        "$PROJECT_ROOT/src/" "$PROJECT_ROOT/include/" \
        2>&1 | tee "$TEMP_OUTPUT"
    CPPCHECK_EXIT=$?
    set -e
fi

# Check for timeout
if [ $CPPCHECK_EXIT -eq 124 ]; then
    echo ""
    echo -e "${RED}⚠ Analysis timed out after ${TIMEOUT_SECONDS}s${NC}"
    echo -e "${YELLOW}Partial results shown above. Consider running on specific directories.${NC}"
elif [ $CPPCHECK_EXIT -ne 0 ]; then
    echo -e "${YELLOW}Cppcheck exited with code ${CPPCHECK_EXIT}; checking whether findings were reported.${NC}"
fi

# Count issues with simple grep and wc. Zero matches are valid.
set +e
ERROR_COUNT=$(grep '\[error\]' "$TEMP_OUTPUT" | wc -l | tr -d ' ')
WARNING_COUNT=$(grep '\[warning\]' "$TEMP_OUTPUT" | wc -l | tr -d ' ')
STYLE_COUNT=$(grep '\[style\]' "$TEMP_OUTPUT" | wc -l | tr -d ' ')
PERFORMANCE_COUNT=$(grep '\[performance\]' "$TEMP_OUTPUT" | wc -l | tr -d ' ')
PORTABILITY_COUNT=$(grep '\[portability\]' "$TEMP_OUTPUT" | wc -l | tr -d ' ')
set -e

# Ensure counts are numeric (default to 0 if empty)
ERROR_COUNT=${ERROR_COUNT:-0}
WARNING_COUNT=${WARNING_COUNT:-0}
STYLE_COUNT=${STYLE_COUNT:-0}
PERFORMANCE_COUNT=${PERFORMANCE_COUNT:-0}
PORTABILITY_COUNT=${PORTABILITY_COUNT:-0}

# Calculate total
TOTAL_COUNT=$((ERROR_COUNT + WARNING_COUNT + STYLE_COUNT + PERFORMANCE_COUNT + PORTABILITY_COUNT))

if [ $CPPCHECK_EXIT -ne 0 ] && [ $TOTAL_COUNT -eq 0 ]; then
    echo ""
    echo -e "${RED}Cppcheck failed with exit code ${CPPCHECK_EXIT} before reporting analyzable findings.${NC}"
    rm -f "$TEMP_OUTPUT"
    exit $CPPCHECK_EXIT
fi

# Clean up
rm -f "$TEMP_OUTPUT"

echo ""
echo -e "${GREEN}Analysis complete!${NC}"
echo ""

# Dynamic summary based on actual results
if [ $TOTAL_COUNT -eq 0 ]; then
    echo -e "${GREEN}✓ No issues found!${NC}"
    echo -e "${BLUE}Status: cppcheck reported no warnings, errors, or performance issues${NC}"
else
    echo -e "${YELLOW}Found $TOTAL_COUNT issues:${NC}"
    [ "$ERROR_COUNT" -gt 0 ] && echo -e "${RED}  Errors: $ERROR_COUNT${NC}"
    [ "$WARNING_COUNT" -gt 0 ] && echo -e "${YELLOW}  Warnings: $WARNING_COUNT${NC}"
    [ "$STYLE_COUNT" -gt 0 ] && echo -e "${BLUE}  Style: $STYLE_COUNT${NC}"
    [ "$PERFORMANCE_COUNT" -gt 0 ] && echo -e "${CYAN}  Performance: $PERFORMANCE_COUNT${NC}"
    [ "$PORTABILITY_COUNT" -gt 0 ] && echo -e "${MAGENTA}  Portability: $PORTABILITY_COUNT${NC}"
    echo ""
    if [ $((ERROR_COUNT + WARNING_COUNT)) -eq 0 ]; then
        echo -e "${GREEN}Good news: No critical errors or warnings!${NC}"
        echo -e "${BLUE}Only style/performance suggestions remain${NC}"
    else
        echo -e "${YELLOW}Priority: Address errors and warnings first${NC}"
    fi
fi

echo ""
echo -e "${YELLOW}Note: This configuration keeps the output focused on genuine code quality issues.${NC}"
