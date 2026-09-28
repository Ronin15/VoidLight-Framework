# Test Suite Templates

Detail file for the `voidlight-test-suite-generator` Skill. Load this on demand when a
generation step in `SKILL.md` points here. Always prefer the live repo convention discovered
in Step 1 over these snippets if they differ.

**Common substitutions across all templates:**
- `<SystemName>` → User-provided class name, PascalCase (e.g. `AnimationManager`)
- `<system>` → snake_case system name used for executables/paths (e.g. `animation_manager`)
- `<brief-description>` → User-provided key functionality
- `<OtherSystem>` → Integration dependency names (if applicable)
- `${TIMEOUT_DURATION}` → `30` for functional tests, `120` for benchmarks

---

## 1. Test Runner Script (`run_<system>_tests.sh`)

Save to `$PROJECT_ROOT/tests/test_scripts/run_<system>_tests.sh`, then `chmod +x` it.

```bash
#!/bin/bash

# Copyright (c) 2025 Hammer Forged Games
# All rights reserved.
# Licensed under the MIT License - see LICENSE file for details

# Test runner for <SystemName> tests
# Usage: ./run_<system>_tests.sh [--verbose] [--debug] [--release] [--help]

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RESET='\033[0m'

# Find project root (directory containing CMakeLists.txt)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Default values
BUILD_TYPE="debug"
VERBOSE=""
TIMEOUT_DURATION=30

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose)
            VERBOSE="--log_level=all"
            shift
            ;;
        --debug)
            BUILD_TYPE="debug"
            shift
            ;;
        --release)
            BUILD_TYPE="release"
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --verbose    Enable verbose test output"
            echo "  --debug      Run debug build tests (default)"
            echo "  --release    Run release build tests"
            echo "  --help       Show this help message"
            echo ""
            echo "Description:"
            echo "  Runs <SystemName> functional tests"
            echo "  Tests: <brief-description>"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Test executable name
TEST_EXECUTABLE="<system>_tests"

# Determine test executable path
if [ "$BUILD_TYPE" = "release" ]; then
    TEST_PATH="$PROJECT_ROOT/bin/release/$TEST_EXECUTABLE"
else
    TEST_PATH="$PROJECT_ROOT/bin/debug/$TEST_EXECUTABLE"
fi

# Check if test executable exists
if [ ! -f "$TEST_PATH" ]; then
    echo -e "${RED}Error: Test executable not found at $TEST_PATH${RESET}"
    echo "Please build the project first:"
    echo "  cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build"
    exit 1
fi

# Create output directory
OUTPUT_DIR="$PROJECT_ROOT/test_results/<system>"
mkdir -p "$OUTPUT_DIR"

# Output file
OUTPUT_FILE="$OUTPUT_DIR/<system>_test_results.txt"

# Run tests
echo -e "${BLUE}Running <SystemName> Tests...${RESET}"
echo "Executable: $TEST_PATH"
echo "Output: $OUTPUT_FILE"
echo ""

# Run with timeout protection
if command -v timeout &> /dev/null; then
    timeout ${TIMEOUT_DURATION}s "$TEST_PATH" $VERBOSE 2>&1 | tee "$OUTPUT_FILE"
    TEST_EXIT_CODE=${PIPESTATUS[0]}
elif command -v gtimeout &> /dev/null; then
    gtimeout ${TIMEOUT_DURATION}s "$TEST_PATH" $VERBOSE 2>&1 | tee "$OUTPUT_FILE"
    TEST_EXIT_CODE=${PIPESTATUS[0]}
else
    "$TEST_PATH" $VERBOSE 2>&1 | tee "$OUTPUT_FILE"
    TEST_EXIT_CODE=$?
fi

# Check results
echo ""
if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ <SystemName> Tests PASSED${RESET}"
    exit 0
elif [ $TEST_EXIT_CODE -eq 124 ]; then
    echo -e "${RED}✗ <SystemName> Tests TIMEOUT (exceeded ${TIMEOUT_DURATION}s)${RESET}"
    echo "Possible infinite loop or performance issue"
    exit 3
else
    echo -e "${RED}✗ <SystemName> Tests FAILED (exit code: $TEST_EXIT_CODE)${RESET}"
    echo ""
    echo "To debug, run:"
    echo "  $TEST_PATH --verbose"
    echo ""
    exit 1
fi
```

### Windows `.bat` pair

Every runner in `tests/test_scripts/` ships as both `.sh` and `.bat`. Copy the structure from
the example `.bat` pair read in Step 1 (e.g. `run_ai_optimization_tests.bat`), apply the same
substitutions, and save to `$PROJECT_ROOT/tests/test_scripts/run_<system>_tests.bat`.

---

## 2. Functional Test Source (`<SystemName>Tests.cpp`)

Source-file naming: PascalCase ending in `Tests.cpp` (e.g. `AnimationManagerTests.cpp`),
matching existing files in `tests/`. The executable stays snake_case (`<system>_tests`); the
`tests/CMakeLists.txt` mapping connects the two. Read a real source first to copy
include/style conventions (e.g. `tests/AIOptimizationTest.cpp`).

Save to `$PROJECT_ROOT/tests/<SystemName>Tests.cpp`.

**Customization based on user input:**
- If "Integration Tests" selected, include `#define INTEGRATION_TESTS`
- If system is a manager (multi-threaded), include `#define THREAD_SAFETY_TESTS`
- Generate specific test cases based on the "Key Functionality" description

```cpp
/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE <SystemName>Tests
#include <boost/test/included/unit_test.hpp>

#include "<SystemName>.hpp"
// Include other dependencies as needed

/**
 * @file <SystemName>Tests.cpp
 * @brief Functional tests for <SystemName>
 *
 * Test Categories:
 * - Construction/Destruction
 * - Basic Functionality
 * - Edge Cases
 * - Error Handling
 * - Thread Safety (if applicable)
 * - Integration (if applicable)
 */

// ============================================================================
// Test Fixtures
// ============================================================================

struct <SystemName>Fixture
{
    <SystemName>Fixture()
    {
        // Setup code
        BOOST_TEST_MESSAGE("Setting up <SystemName> test fixture");
    }

    ~<SystemName>Fixture()
    {
        // Cleanup code
        BOOST_TEST_MESSAGE("Tearing down <SystemName> test fixture");
    }

    // Helper members
    // <SystemName>* mp_system = nullptr;
};

// ============================================================================
// Construction/Destruction Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(<SystemName>TestSuite, <SystemName>Fixture)

BOOST_AUTO_TEST_CASE(TestConstruction)
{
    BOOST_TEST_MESSAGE("Testing <SystemName> construction");

    // Test default construction
    <SystemName> system;

    // Verify initial state
    // BOOST_CHECK_EQUAL(system.getSomeValue(), expectedValue);

    BOOST_TEST_MESSAGE("<SystemName> construction test passed");
}

BOOST_AUTO_TEST_CASE(TestDestruction)
{
    BOOST_TEST_MESSAGE("Testing <SystemName> destruction");

    // Create and destroy system
    {
        <SystemName> system;
        // Use system
    }

    // Verify proper cleanup (no leaks, resources released)
    BOOST_CHECK(true); // Placeholder

    BOOST_TEST_MESSAGE("<SystemName> destruction test passed");
}

// ============================================================================
// Basic Functionality Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(TestBasicFunctionality)
{
    BOOST_TEST_MESSAGE("Testing <SystemName> basic functionality");

    <SystemName> system;

    // Test key functionality based on user input
    // Example:
    // system.initialize();
    // BOOST_CHECK(system.isInitialized());

    BOOST_TEST_MESSAGE("<SystemName> basic functionality test passed");
}

// ============================================================================
// Edge Cases
// ============================================================================

BOOST_AUTO_TEST_CASE(TestEdgeCases)
{
    BOOST_TEST_MESSAGE("Testing <SystemName> edge cases");

    <SystemName> system;

    // Test boundary conditions
    // Test null inputs
    // Test empty states
    // Test maximum values

    BOOST_CHECK(true); // Placeholder

    BOOST_TEST_MESSAGE("<SystemName> edge case test passed");
}

// ============================================================================
// Error Handling
// ============================================================================

BOOST_AUTO_TEST_CASE(TestErrorHandling)
{
    BOOST_TEST_MESSAGE("Testing <SystemName> error handling");

    <SystemName> system;

    // Test error conditions
    // Verify exceptions thrown correctly
    // Verify error codes returned

    // Example:
    // BOOST_CHECK_THROW(system.invalidOperation(), std::runtime_error);

    BOOST_TEST_MESSAGE("<SystemName> error handling test passed");
}

// ============================================================================
// Thread Safety Tests (if applicable)
// ============================================================================

// Only include if system is used in multi-threaded context
#ifdef THREAD_SAFETY_TESTS

BOOST_AUTO_TEST_CASE(TestThreadSafety)
{
    BOOST_TEST_MESSAGE("Testing <SystemName> thread safety");

    <SystemName> system;

    // Test concurrent access
    // Verify mutex protection
    // Check for race conditions

    BOOST_CHECK(true); // Placeholder - implement actual threading test

    BOOST_TEST_MESSAGE("<SystemName> thread safety test passed");
}

#endif

// ============================================================================
// Integration Tests (if applicable)
// ============================================================================

// Only include if system integrates with others
#ifdef INTEGRATION_TESTS

BOOST_AUTO_TEST_CASE(TestIntegrationWith<OtherSystem>)
{
    BOOST_TEST_MESSAGE("Testing <SystemName> integration with <OtherSystem>");

    <SystemName> system;
    // <OtherSystem> otherSystem;

    // Test interaction between systems
    // Verify data flow
    // Check synchronization

    BOOST_CHECK(true); // Placeholder

    BOOST_TEST_MESSAGE("<SystemName> integration test passed");
}

#endif

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Entry Point
// ============================================================================

// Boost.Test automatically generates main() with BOOST_TEST_MODULE
```

---

## 3. Benchmark Test Source (`<SystemName>Benchmark.cpp`)

Only generate if the user selects "Benchmark Tests". Timing uses `std::chrono::steady_clock`.
Save to `$PROJECT_ROOT/tests/<SystemName>Benchmark.cpp`.

```cpp
/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE <SystemName>Benchmark
#include <boost/test/included/unit_test.hpp>

#include "<SystemName>.hpp"
#include <chrono>
#include <iostream>
#include <fstream>

/**
 * @file <SystemName>Benchmark.cpp
 * @brief Performance benchmarks for <SystemName>
 *
 * Benchmark Categories:
 * - Throughput Testing
 * - Latency Measurement
 * - Scaling Analysis
 * - Resource Usage
 */

// ============================================================================
// Benchmark Helpers
// ============================================================================

class BenchmarkTimer
{
public:
    void start()
    {
        m_start = std::chrono::steady_clock::now();
    }

    double stop()
    {
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> duration = end - m_start;
        return duration.count();
    }

private:
    std::chrono::steady_clock::time_point m_start;
};

void saveMetric(const std::string& name, double value, const std::string& unit)
{
    // NOTE: Requires PROJECT_ROOT environment variable
    const char* root = std::getenv("PROJECT_ROOT");
    std::string path = root ? std::string(root) + "/test_results/<system>/performance_metrics.txt"
                            : "test_results/<system>/performance_metrics.txt";
    std::ofstream file(path, std::ios::app);
    file << name << ": " << value << " " << unit << std::endl;
    std::cout << name << ": " << value << " " << unit << std::endl;
}

// ============================================================================
// Benchmark Fixture
// ============================================================================

struct <SystemName>BenchmarkFixture
{
    <SystemName>BenchmarkFixture()
    {
        BOOST_TEST_MESSAGE("Setting up <SystemName> benchmark fixture");
        // Create output directory (requires PROJECT_ROOT environment variable)
        system("mkdir -p \"$PROJECT_ROOT/test_results/<system>\"");
        // Clear previous metrics
        system("rm -f \"$PROJECT_ROOT/test_results/<system>/performance_metrics.txt\"");
    }

    ~<SystemName>BenchmarkFixture()
    {
        BOOST_TEST_MESSAGE("Tearing down <SystemName> benchmark fixture");
    }

    BenchmarkTimer timer;
    // <SystemName> system;
};

// ============================================================================
// Throughput Benchmarks
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(<SystemName>BenchmarkSuite, <SystemName>BenchmarkFixture)

BOOST_AUTO_TEST_CASE(BenchmarkThroughput_1K)
{
    BOOST_TEST_MESSAGE("Benchmarking <SystemName> throughput (1K operations)");

    const int OPERATIONS = 1000;
    <SystemName> system;

    timer.start();
    for (int i = 0; i < OPERATIONS; i++)
    {
        // Perform operation
        // system.doOperation();
    }
    double elapsed = timer.stop();

    double throughput = OPERATIONS / (elapsed / 1000.0); // ops/sec
    saveMetric("Throughput_1K", throughput, "ops/sec");
    saveMetric("Latency_1K", elapsed / OPERATIONS, "ms/op");

    BOOST_TEST_MESSAGE("Throughput (1K): " << throughput << " ops/sec");
}

BOOST_AUTO_TEST_CASE(BenchmarkThroughput_10K)
{
    BOOST_TEST_MESSAGE("Benchmarking <SystemName> throughput (10K operations)");

    const int OPERATIONS = 10000;
    <SystemName> system;

    timer.start();
    for (int i = 0; i < OPERATIONS; i++)
    {
        // Perform operation
        // system.doOperation();
    }
    double elapsed = timer.stop();

    double throughput = OPERATIONS / (elapsed / 1000.0);
    saveMetric("Throughput_10K", throughput, "ops/sec");
    saveMetric("Latency_10K", elapsed / OPERATIONS, "ms/op");

    BOOST_TEST_MESSAGE("Throughput (10K): " << throughput << " ops/sec");
}

// ============================================================================
// Scaling Benchmarks
// ============================================================================

BOOST_AUTO_TEST_CASE(BenchmarkScaling)
{
    BOOST_TEST_MESSAGE("Benchmarking <SystemName> scaling characteristics");

    <SystemName> system;

    // Test scaling from 100 to 10000 operations
    std::vector<int> sizes = {100, 500, 1000, 5000, 10000};

    for (int size : sizes)
    {
        timer.start();
        for (int i = 0; i < size; i++)
        {
            // Perform operation
            // system.doOperation();
        }
        double elapsed = timer.stop();

        double throughput = size / (elapsed / 1000.0);
        std::string metricName = "Throughput_" + std::to_string(size);
        saveMetric(metricName, throughput, "ops/sec");
    }

    BOOST_TEST_MESSAGE("<SystemName> scaling benchmark completed");
}

// ============================================================================
// Resource Usage Benchmarks
// ============================================================================

BOOST_AUTO_TEST_CASE(BenchmarkMemoryUsage)
{
    BOOST_TEST_MESSAGE("Benchmarking <SystemName> memory usage");

    // Measure memory usage with different loads
    // This is a placeholder - actual implementation depends on system

    <SystemName> system;

    // Estimate memory per operation
    // For actual measurement, consider using valgrind massif

    saveMetric("Estimated_Memory_Per_Op", 0.0, "bytes"); // Placeholder

    BOOST_TEST_MESSAGE("<SystemName> memory usage benchmark completed");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Benchmark Summary
// ============================================================================

struct BenchmarkSummaryFixture
{
    ~BenchmarkSummaryFixture()
    {
        const char* root = std::getenv("PROJECT_ROOT");
        std::string resultsPath = root ? std::string(root) + "/test_results/<system>/performance_metrics.txt"
                                        : "test_results/<system>/performance_metrics.txt";
        BOOST_TEST_MESSAGE("=== <SystemName> Benchmark Summary ===");
        BOOST_TEST_MESSAGE("Results saved to: " << resultsPath);
        BOOST_TEST_MESSAGE("Review metrics for performance analysis");
    }
};

BOOST_FIXTURE_TEST_SUITE(SummaryGeneration, BenchmarkSummaryFixture)

BOOST_AUTO_TEST_CASE(GenerateSummary)
{
    // Generate summary report
    const char* root = std::getenv("PROJECT_ROOT");
    std::string reportPath = root ? std::string(root) + "/test_results/<system>/performance_report.md"
                                   : "test_results/<system>/performance_report.md";
    std::ofstream report(reportPath);
    report << "# <SystemName> Performance Report\n\n";
    report << "**Date:** " << __DATE__ << " " << __TIME__ << "\n\n";
    report << "## Metrics\n\n";
    report << "See `performance_metrics.txt` for detailed metrics.\n\n";
    report << "## Analysis\n\n";
    report << "TODO: Add performance analysis\n";
    report.close();

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
```

---

## 4. CMake Registration (`tests/CMakeLists.txt`)

Tests are NOT defined in the root `CMakeLists.txt`. They are registered in
**`tests/CMakeLists.txt`** via two edits. Linking (`VoidLightLib Boost::unit_test_framework`),
the `BOOST_TEST_NO_SIGNAL_HANDLING` define, output directory, and CTest registration are all
applied generically by the existing `foreach` loop — do NOT write per-test `add_executable` /
`target_link_libraries` / `set_target_properties`. The test links against `VoidLightLib`, which
already contains the production sources — do NOT add `src/...` files to the test target. Read
the file first to copy the live structure, then use the Edit tool to match surrounding
formatting exactly.

**Edit 1 — add to the `ALL_TESTS` list:**
```cmake
set(ALL_TESTS
    ...
    <system>_tests
    <system>_benchmark   # only if a benchmark was generated
)
```

**Edit 2 — add a source mapping inside the `foreach(test_name ${ALL_TESTS})` block:**
```cmake
    elseif(${test_name} STREQUAL "<system>_tests")
        set(test_source "<SystemName>Tests.cpp")
    elseif(${test_name} STREQUAL "<system>_benchmark")
        set(test_source "<SystemName>Benchmark.cpp")
```
(If the source lives in a subdirectory like `tests/ai/`, use the relative path, e.g.
`set(test_source "ai/<SystemName>Tests.cpp")`.)

---

## 5. Master Test Runner (`tests/test_scripts/run_all_tests.sh`)

The real master runner is **`tests/test_scripts/run_all_tests.sh`** (the root
`run_all_tests.sh` is only a backward-compat wrapper — never edit it). It runs a
`SCRIPT_DIR`-based array of runner paths, not per-section `echo`/`check_status` blocks. Read it
first, then use the Edit tool to insert the new runner into the array, in the relevant grouping:

```bash
  "$SCRIPT_DIR/run_<system>_tests.sh"
  "$SCRIPT_DIR/run_<system>_benchmark.sh"   # only if a benchmark runner was generated
```

---

## 6. Documentation Stub (`tests/docs/<SystemName>_Testing.md`)

Save to `$PROJECT_ROOT/tests/docs/<SystemName>_Testing.md`.

```markdown
# <SystemName> Testing

## Overview

Tests for <SystemName> functionality and performance.

## Test Suites

### Functional Tests
- **Location:** `tests/<SystemName>Tests.cpp`
- **Runner:** `tests/test_scripts/run_<system>_tests.sh`
- **Coverage:**
  - Construction/Destruction
  - Basic Functionality
  - Edge Cases
  - Error Handling
  - Thread Safety (if applicable)

### Benchmark Tests
- **Location:** `tests/<SystemName>Benchmark.cpp`
- **Runner:** `tests/test_scripts/run_<system>_benchmark.sh`
- **Metrics:**
  - Throughput (ops/sec)
  - Latency (ms/op)
  - Scaling characteristics
  - Resource usage

## Running Tests

```bash
# Functional tests
./tests/test_scripts/run_<system>_tests.sh --verbose

# Benchmarks
./tests/test_scripts/run_<system>_benchmark.sh --verbose

# All tests (included in master runner)
./tests/test_scripts/run_all_tests.sh --core-only
./tests/test_scripts/run_all_tests.sh --benchmarks-only
```

## Test Results

Results are saved to:
- `test_results/<system>/<system>_test_results.txt`
- `test_results/<system>/performance_metrics.txt`
- `test_results/<system>/performance_report.md`

## Adding New Tests

1. Add test case to `tests/<SystemName>Tests.cpp`
2. Use `BOOST_AUTO_TEST_CASE` macro
3. Follow existing test patterns
4. Run tests to verify

## Performance Baselines

TODO: Document expected performance baselines for benchmarks.

## Known Issues

TODO: Document any known test issues or limitations.
```
