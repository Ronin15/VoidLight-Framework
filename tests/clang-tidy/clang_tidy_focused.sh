#!/bin/bash
# Focused clang-tidy analysis for VoidLight-Framework
# Shows only important safety and bug issues

# Don't exit on error - clang-tidy returns non-zero when it finds issues
set +e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${BLUE}=== VoidLight-Framework - clang-tidy Analysis ===${NC}"
echo ""

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo -e "${RED}Error: clang-tidy not found${NC}"
    echo "Install via: brew install llvm"
    echo "Then add to PATH: export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\""
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd -P)"

# Check compile_commands.json exists
if [ ! -f "$PROJECT_ROOT/compile_commands.json" ]; then
    echo -e "${RED}Error: compile_commands.json not found${NC}"
    echo "Run cmake first: cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug"
    exit 1
fi

# Extract .cpp files from compile_commands.json, filter to project src/ only
FILES=$(grep '"file":' "$PROJECT_ROOT/compile_commands.json" | \
        sed 's/.*"file": "//;s/".*//' | \
        grep '/src/' | \
        grep -v '/build/' | \
        grep -v '/tests/' | \
        grep -v '/_deps/' | \
        tr '\n' ' ')

FILE_COUNT=$(echo "$FILES" | wc -w | tr -d ' ')
echo -e "${YELLOW}Analyzing $FILE_COUNT source files...${NC}"
echo ""

# Run clang-tidy with parallel execution
TEMP_OUTPUT=$(mktemp)
echo -e "${CYAN}Running analysis in parallel...${NC}"

# Use run-clang-tidy for parallel execution if available, else fall back to single-threaded
if command -v run-clang-tidy &> /dev/null; then
    run-clang-tidy -p "$PROJECT_ROOT" \
        -clang-tidy-binary="$(which clang-tidy)" \
        -config-file="$SCRIPT_DIR/.clang-tidy" \
        -quiet \
        $FILES > "$TEMP_OUTPUT" 2>&1
else
    # Fall back to xargs for parallel execution
    echo "$FILES" | tr ' ' '\n' | xargs -P 4 -I {} \
        clang-tidy -p "$PROJECT_ROOT/compile_commands.json" \
        --config-file="$SCRIPT_DIR/.clang-tidy" \
        --quiet {} >> "$TEMP_OUTPUT" 2>&1
fi

# Filter to only project files (not system headers)
PROJECT_WARNINGS=$(grep -E "/src/.*: warning:" "$TEMP_OUTPUT" 2>/dev/null || true)

# Apply suppressions from file
SUPPRESSIONS_FILE="$SCRIPT_DIR/clang_tidy_suppressions.txt"
if [ -f "$SUPPRESSIONS_FILE" ] && [ -n "$PROJECT_WARNINGS" ]; then
    # Read suppressions (format: file_pattern:check_name:reason)
    while IFS=: read -r file_pattern check_name reason || [ -n "$file_pattern" ]; do
        # Skip comments and empty lines
        [[ "$file_pattern" =~ ^#.*$ || -z "$file_pattern" ]] && continue
        # Filter out matching warnings
        PROJECT_WARNINGS=$(echo "$PROJECT_WARNINGS" | grep -v "${file_pattern}.*${check_name}" || true)
    done < "$SUPPRESSIONS_FILE"
fi

# Count suppressions applied
SUPPRESSION_COUNT=0
if [ -f "$SUPPRESSIONS_FILE" ]; then
    SUPPRESSION_COUNT=$(grep -v '^#' "$SUPPRESSIONS_FILE" | grep -v '^$' | wc -l | tr -d ' ')
fi

# Count by severity category (handle empty strings properly)
if [ -z "$PROJECT_WARNINGS" ]; then
    CRITICAL_COUNT=0
    HIGH_COUNT=0
    MEDIUM_COUNT=0
    LOW_COUNT=0
else
    # Critical: bugs that crash/corrupt (excluding narrowing which is common in games)
    CRITICAL_COUNT=$(echo "$PROJECT_WARNINGS" | grep -cE 'bugprone-infinite-loop|bugprone-use-after-move|bugprone-null-dereference|clang-analyzer-' 2>/dev/null | tr -d '\n' || echo 0)

    # High: performance and safety (macro issues, override, perf)
    HIGH_COUNT=$(echo "$PROJECT_WARNINGS" | grep -cE 'performance-|modernize-use-override|bugprone-macro' 2>/dev/null | tr -d '\n' || echo 0)

    # Medium: code quality (const, unused, naming)
    MEDIUM_COUNT=$(echo "$PROJECT_WARNINGS" | grep -cE 'misc-const-correctness|misc-unused-parameters|readability-make-member-function-const' 2>/dev/null | tr -d '\n' || echo 0)

    # Low: style (narrowing, braces, identifiers)
    LOW_COUNT=$(echo "$PROJECT_WARNINGS" | grep -cE 'narrowing-conversion|readability-braces|readability-identifier' 2>/dev/null | tr -d '\n' || echo 0)

    # Ensure counts are integers (handle empty or non-numeric)
    [[ "$CRITICAL_COUNT" =~ ^[0-9]+$ ]] || CRITICAL_COUNT=0
    [[ "$HIGH_COUNT" =~ ^[0-9]+$ ]] || HIGH_COUNT=0
    [[ "$MEDIUM_COUNT" =~ ^[0-9]+$ ]] || MEDIUM_COUNT=0
    [[ "$LOW_COUNT" =~ ^[0-9]+$ ]] || LOW_COUNT=0
fi

# Get issues by severity
CRITICAL_ISSUES=$(echo "$PROJECT_WARNINGS" | grep -E 'bugprone-infinite-loop|bugprone-use-after-move|bugprone-null-dereference|clang-analyzer-' 2>/dev/null || true)
HIGH_ISSUES=$(echo "$PROJECT_WARNINGS" | grep -E 'performance-|modernize-use-override|bugprone-macro' 2>/dev/null || true)
MEDIUM_ISSUES=$(echo "$PROJECT_WARNINGS" | grep -E 'misc-const-correctness|misc-unused-parameters|readability-make-member-function-const' 2>/dev/null || true)

echo -e "${BLUE}=== Summary by Severity ===${NC}"
echo ""
echo -e "${RED}  CRITICAL (bugs):${NC}    $CRITICAL_COUNT"
echo -e "${YELLOW}  HIGH (perf/safety):${NC} $HIGH_COUNT"
echo -e "${CYAN}  MEDIUM (quality):${NC}   $MEDIUM_COUNT"
echo -e "  LOW (style):        $LOW_COUNT"
echo ""

# Show suppressions applied
if [ "$SUPPRESSION_COUNT" -gt 0 ]; then
    echo -e "${GREEN}Suppressions applied: $SUPPRESSION_COUNT from clang_tidy_suppressions.txt${NC}"
    echo ""
fi

# Show file-by-file breakdown
if [ -n "$PROJECT_WARNINGS" ]; then
    echo -e "${BLUE}=== Files with Most Issues (top 10) ===${NC}"
    echo "$PROJECT_WARNINGS" | sed 's/:.*//;s|.*/||' | sort | uniq -c | sort -rn | head -10
    echo ""
fi

# Show critical issues first
if [ -n "$CRITICAL_ISSUES" ]; then
    echo -e "${RED}=== CRITICAL Issues (action required) ===${NC}"
    echo "$CRITICAL_ISSUES" | head -30
    echo ""
fi

# Show high priority issues
if [ -n "$HIGH_ISSUES" ]; then
    echo -e "${YELLOW}=== HIGH Priority Issues (recommended fixes) ===${NC}"
    echo "$HIGH_ISSUES" | head -30
    echo ""
fi

# Show medium priority issues
if [ -n "$MEDIUM_ISSUES" ]; then
    echo -e "${CYAN}=== MEDIUM Priority Issues (quality improvements) ===${NC}"
    echo "$MEDIUM_ISSUES" | head -20
    echo ""
fi

# Cleanup
rm -f "$TEMP_OUTPUT"

# Final status
TOTAL_ACTIONABLE=$((CRITICAL_COUNT + HIGH_COUNT))
if [ "$CRITICAL_COUNT" -eq 0 ] && [ "$HIGH_COUNT" -eq 0 ]; then
    echo -e "${GREEN}✓ No critical or high-priority issues found!${NC}"
    exit 0
elif [ "$CRITICAL_COUNT" -gt 0 ]; then
    echo -e "${RED}✗ Found $CRITICAL_COUNT critical issues requiring action${NC}"
    exit 1
else
    echo -e "${YELLOW}⚠ Found $HIGH_COUNT high-priority warnings to review${NC}"
    exit 0
fi
