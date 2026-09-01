#!/bin/bash

# Copyright (c) 2025 Hammer Forged Games
# All rights reserved.
# Licensed under the MIT License - see LICENSE file for details

# SIMD Performance Benchmark Runner
# Validates claimed SIMD speedups (3-4x for AI, 2-3x for Collision on ARM64)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BENCHMARK_BINARY="$PROJECT_ROOT/bin/debug/simd_performance_benchmark"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
VERBOSE=0
REPORT_ONLY=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --report-only|-r)
            REPORT_ONLY=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --verbose, -v       Show detailed benchmark output"
            echo "  --report-only, -r   Only show summary report (errors only)"
            echo "  --help, -h          Show this help message"
            echo ""
            echo "Description:"
            echo "  Runs SIMD performance benchmarks to validate claimed speedups:"
            echo "  - AIManager Distance Calculation: 3-4x speedup (docs/utils/SIMDMath.md)"
            echo "  - CollisionManager Bounds: 2-3x speedup on ARM64 (tests/performance/SIMD_BENCHMARK_README.md)"
            echo "  - ParticleManager Physics Update: 2-4x speedup"
            echo ""
            echo "  Benchmarks test 10K entities over 1000 iterations."
            echo "  Minimum speedup threshold: 2.0x"
            echo ""
            echo "Exit codes:"
            echo "  0 - All benchmarks passed"
            echo "  1 - One or more benchmarks failed"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check if benchmark binary exists
if [[ ! -f "$BENCHMARK_BINARY" ]]; then
    echo -e "${RED}Error: Benchmark binary not found: $BENCHMARK_BINARY${NC}"
    echo "Please build the project first:"
    echo "  cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build"
    exit 1
fi

echo -e "${BLUE}=== SIMD Performance Benchmark Suite ===${NC}"
echo "Binary: $BENCHMARK_BINARY"
echo ""

# Run benchmark
if [[ $VERBOSE -eq 1 ]]; then
    # Show all output
    "$BENCHMARK_BINARY"
    EXIT_CODE=$?
elif [[ $REPORT_ONLY -eq 1 ]]; then
    # Only show errors
    "$BENCHMARK_BINARY" 2>&1 | grep -E "(FAIL|Error|error|PASS)" || true
    EXIT_CODE=${PIPESTATUS[0]}
else
    # Show platform detection and summary
    "$BENCHMARK_BINARY" 2>&1 | grep -A 20 "Platform Detection\|Benchmark Summary\|===.*===" || true
    EXIT_CODE=${PIPESTATUS[0]}
fi

echo ""

# Report results
if [[ $EXIT_CODE -eq 0 ]]; then
    echo -e "${GREEN}✓ All SIMD benchmarks PASSED${NC}"
    echo ""
    echo "SIMD optimizations are working correctly and provide claimed speedups."
else
    echo -e "${RED}✗ SIMD benchmarks FAILED${NC}"
    echo ""
    echo "Possible causes:"
    echo "  1. SIMD not available on this platform (check platform detection)"
    echo "  2. SIMD speedup below threshold (< 2.0x)"
    echo "  3. Build configuration issue (SIMD flags not enabled)"
    echo ""
    echo "Try running with --verbose for detailed output:"
    echo "  $0 --verbose"
fi

exit $EXIT_CODE
