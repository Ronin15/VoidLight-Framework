#!/bin/bash
# Full clang-tidy analysis for VoidLight-Framework
# Checks all source files (comprehensive but slower)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== VoidLight-Framework - Full clang-tidy Analysis ===${NC}"
echo ""

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo -e "${RED}Error: clang-tidy not found${NC}"
    echo "Install via: brew install llvm"
    echo "Then add to PATH: export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\""
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

mkdir -p "$OUTPUT_DIR"

# Check compile_commands.json exists
if [ ! -f "$PROJECT_ROOT/compile_commands.json" ]; then
    echo -e "${RED}Error: compile_commands.json not found${NC}"
    echo "Run cmake first: cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug"
    exit 1
fi

echo -e "${YELLOW}Checking all source files...${NC}"
echo "This may take several minutes."
echo ""

# Get all source files (excluding tests)
FILES=$(find "$PROJECT_ROOT/src" -name "*.cpp" | tr '\n' ' ')

# Run clang-tidy with output to file
OUTPUT_FILE="$OUTPUT_DIR/clang_tidy_full_${TIMESTAMP}.txt"

echo -e "${BLUE}Running clang-tidy...${NC}"

clang-tidy -p "$PROJECT_ROOT/compile_commands.json" \
    --config-file="$SCRIPT_DIR/.clang-tidy" \
    $FILES 2>&1 | tee "$OUTPUT_FILE"

# Count issues
ERROR_COUNT=$(grep -c ': error:' "$OUTPUT_FILE" 2>/dev/null || echo "0")
WARNING_COUNT=$(grep -c ': warning:' "$OUTPUT_FILE" 2>/dev/null || echo "0")

# Generate summary
SUMMARY_FILE="$OUTPUT_DIR/clang_tidy_summary_${TIMESTAMP}.txt"
{
    echo "=== clang-tidy Full Analysis Summary ==="
    echo "Generated: $(date)"
    echo ""
    echo "Results:"
    echo "  Errors: $ERROR_COUNT"
    echo "  Warnings: $WARNING_COUNT"
    echo ""
    echo "Full report: $OUTPUT_FILE"
} > "$SUMMARY_FILE"

cat "$SUMMARY_FILE"

echo ""
if [ "$ERROR_COUNT" -eq 0 ] && [ "$WARNING_COUNT" -eq 0 ]; then
    echo -e "${GREEN}✓ No issues found!${NC}"
    exit 0
elif [ "$ERROR_COUNT" -eq 0 ]; then
    echo -e "${YELLOW}⚠ Warnings found (review recommended)${NC}"
    exit 0
else
    echo -e "${RED}✗ Errors found (action required)${NC}"
    exit 1
fi
