#!/bin/bash
# GPU Test Runner for SDL3 VoidLight-Framework
# Runs GPU rendering subsystem tests with configurable options

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Default options
VERBOSE=false
UNIT_ONLY=false
INTEGRATION_ONLY=false
SYSTEM_ONLY=false
SKIP_GPU=false
BUILD_TYPE="debug"
ERRORS_ONLY=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --unit-only)
            UNIT_ONLY=true
            shift
            ;;
        --integration-only)
            INTEGRATION_ONLY=true
            shift
            ;;
        --system-only)
            SYSTEM_ONLY=true
            shift
            ;;
        --skip-gpu)
            SKIP_GPU=true
            shift
            ;;
        --release)
            BUILD_TYPE="release"
            shift
            ;;
        --errors-only)
            ERRORS_ONLY=true
            shift
            ;;
        --help|-h)
            echo "GPU Test Runner for SDL3 VoidLight-Framework"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --verbose, -v      Show detailed test output"
            echo "  --unit-only        Run only unit tests (no GPU required)"
            echo "  --integration-only Run only GPU integration tests"
            echo "  --system-only      Run only full frame flow tests"
            echo "  --skip-gpu         Skip tests requiring GPU (for CI)"
            echo "  --release          Run tests in release mode"
            echo "  --errors-only      Show only test failures"
            echo "  --help, -h         Show this help message"
            echo ""
            echo "Test Categories:"
            echo "  Unit Tests:        GPUTypes, PipelineConfig (no GPU)"
            echo "  Integration Tests: Device, Shaders, Resources, VertexPool, SpriteBatch"
            echo "  System Tests:      GPURenderer full frame flow"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Create results directory
RESULTS_DIR="${PROJECT_ROOT}/test_results/gpu"
mkdir -p "${RESULTS_DIR}"

# Binary directory
BIN_DIR="${PROJECT_ROOT}/bin/${BUILD_TYPE}"

# Test executables
UNIT_TESTS=(
    "gpu_types_tests"
    "gpu_pipeline_config_tests"
)

INTEGRATION_TESTS=(
    "gpu_device_tests"
    "gpu_shader_manager_tests"
    "gpu_resource_tests"
    "gpu_vertex_pool_tests"
    "sprite_batch_tests"
)

SYSTEM_TESTS=(
    "gpu_renderer_tests"
)

# Function to run a test
run_test() {
    local test_name=$1
    local test_path="${BIN_DIR}/${test_name}"

    if [[ ! -f "${test_path}" ]]; then
        echo -e "${RED}  [FAIL] ${test_name} (not built)${NC}"
        return 1
    fi

    local output_file="${RESULTS_DIR}/${test_name}_output.txt"
    local test_args="--catch_system_errors=no"

    if [[ "${VERBOSE}" == "true" ]]; then
        test_args="${test_args} --log_level=all"
    elif [[ "${ERRORS_ONLY}" == "true" ]]; then
        test_args="${test_args} --log_level=error"
    fi

    echo -n "  Running ${test_name}..."

    if "${test_path}" ${test_args} > "${output_file}" 2>&1; then
        echo -e " ${GREEN}PASS${NC}"
        return 0
    else
        echo -e " ${RED}FAIL${NC}"
        if [[ "${VERBOSE}" == "true" || "${ERRORS_ONLY}" == "true" ]]; then
            echo "    Output:"
            tail -20 "${output_file}" | sed 's/^/    /'
        fi
        return 1
    fi
}

# Function to run a category of tests
run_category() {
    local category_name=$1
    shift
    local tests=("$@")
    local passed=0
    local failed=0
    local skipped=0

    echo -e "${BLUE}Running ${category_name}...${NC}"

    for test in "${tests[@]}"; do
        run_test "${test}"
        local result=$?
        if [[ $result -eq 0 ]]; then
            passed=$((passed + 1))
        elif [[ $result -eq 2 ]]; then
            skipped=$((skipped + 1))
        else
            failed=$((failed + 1))
        fi
    done

    echo ""
    echo -e "  ${category_name} Results: ${GREEN}${passed} passed${NC}, ${RED}${failed} failed${NC}, ${YELLOW}${skipped} skipped${NC}"
    echo ""

    return $failed
}

# Main execution
echo ""
echo "=================================="
echo "  GPU Test Suite"
echo "  Build: ${BUILD_TYPE}"
echo "=================================="
echo ""

total_failed=0

# Run unit tests (always, no GPU required)
if [[ "${INTEGRATION_ONLY}" != "true" && "${SYSTEM_ONLY}" != "true" ]]; then
    run_category "Unit Tests" "${UNIT_TESTS[@]}"
    total_failed=$((total_failed + $?))
fi

# Run integration tests (require GPU)
if [[ "${UNIT_ONLY}" != "true" && "${SYSTEM_ONLY}" != "true" && "${SKIP_GPU}" != "true" ]]; then
    run_category "Integration Tests" "${INTEGRATION_TESTS[@]}"
    total_failed=$((total_failed + $?))
fi

# Run system tests (require GPU)
if [[ "${UNIT_ONLY}" != "true" && "${INTEGRATION_ONLY}" != "true" && "${SKIP_GPU}" != "true" ]]; then
    run_category "System Tests" "${SYSTEM_TESTS[@]}"
    total_failed=$((total_failed + $?))
fi

# Summary
echo "=================================="
if [[ $total_failed -eq 0 ]]; then
    echo -e "  ${GREEN}All tests passed!${NC}"
else
    echo -e "  ${RED}${total_failed} test suite(s) failed${NC}"
fi
echo "=================================="
echo ""
echo "Results saved to: ${RESULTS_DIR}"

exit $total_failed
