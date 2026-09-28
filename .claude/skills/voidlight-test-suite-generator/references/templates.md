# Test Suite Templates

Detail file for the `voidlight-test-suite-generator` Skill. Load on demand when a step in
`SKILL.md` points here. The live repo pattern discovered in Step 1 wins over these snippets, and
root `CLAUDE.md` + `.claude/rules/tests.md` (+ `.claude/rules/tests-ai.md` / `.claude/rules/tests-managers.md`) win
over both.

**Substitutions:**
- `<SystemName>` → PascalCase class name (e.g. `AnimationManager`)
- `<system>` → snake_case name used for executables/scripts (e.g. `animation_manager`)
- `<subsystem>` → test directory matching the header (`managers`, `controllers`, `core`, `ai`, `world`, …)
- `<header-path>` → include path as used in `src/` (e.g. `managers/AnimationManager.hpp`)

Reference files (read at least one before generating):
- Manager: `tests/managers/ProjectileManagerTests.cpp`
- Controller: `tests/controllers/ProjectileRenderControllerTests.cpp`, `tests/controllers/common/*.hpp`
- Core/unit: `tests/core/WorkerBudgetTests.cpp`
- World/EDM-heavy: `tests/world/WorldPopulationTests.cpp`
- Benchmark: `tests/performance/ProjectileScalingBenchmark.cpp`
- Runner: `tests/test_scripts/run_projectile_manager_tests.sh` / `.bat`

---

## 1. Standalone Runner (`tests/test_scripts/run_<system>_tests.sh`)

Mirrors `run_projectile_manager_tests.sh`. `chmod +x` after writing. Supported flags:
`--verbose`, `--run_test=<name>`, `--help` (add `--release` only if the user needs it — most
current runners are debug-only). `run_all_tests.sh` passes `--verbose` through and judges
pass/fail by the exit code, so the script must `exit $RESULT`.

```bash
#!/bin/bash
# Script to run <SystemName> tests
# Copyright (c) 2025 Hammer Forged Games, MIT License

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/../.."
VERBOSE=false
TEST_FILTER=""

for arg in "$@"; do
  case $arg in
    --verbose) VERBOSE=true; shift ;;
    --run_test=*) TEST_FILTER="${arg}"; shift ;;
    --help)
      echo -e "${BLUE}<SystemName> Tests Runner${NC}"
      echo -e "Usage: ./run_<system>_tests.sh [options]"
      echo -e "\nOptions:"
      echo -e "  --verbose          Run tests with verbose output"
      echo -e "  --run_test=<name>  Run a specific test case"
      echo -e "  --help             Show this help message"
      echo -e "\nTest Coverage:"
      echo -e "  <SuiteName>:"
      echo -e "    - <contract covered>"
      exit 0
      ;;
  esac
done

mkdir -p "$PROJECT_DIR/test_results"
RESULTS_FILE="$PROJECT_DIR/test_results/<system>_tests_results.txt"

echo -e "${BLUE}======================================================${NC}"
echo -e "${BLUE}         <SystemName> Tests${NC}"
echo -e "${BLUE}======================================================${NC}"

TEST_EXECUTABLE="$PROJECT_DIR/bin/debug/<system>_tests"
if [ ! -f "$TEST_EXECUTABLE" ]; then
    echo -e "${RED}Test executable not found: $TEST_EXECUTABLE${NC}"
    echo -e "${YELLOW}Make sure you have built the project with tests enabled.${NC}"
    echo -e "Run: ${CYAN}cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build${NC}"
    exit 1
fi

echo -e "${CYAN}Running <SystemName> tests...${NC}"
echo "<SystemName> Tests - $(date)" > "$RESULTS_FILE"

if [ "$VERBOSE" = true ]; then
    $TEST_EXECUTABLE --log_level=all $TEST_FILTER 2>&1 | tee -a "$RESULTS_FILE"
    RESULT=${PIPESTATUS[0]}
else
    $TEST_EXECUTABLE --log_level=test_suite $TEST_FILTER 2>&1 | tee -a "$RESULTS_FILE"
    RESULT=${PIPESTATUS[0]}
fi

echo "" >> "$RESULTS_FILE"
echo "Test completed at: $(date)" >> "$RESULTS_FILE"
echo "Exit code: $RESULT" >> "$RESULTS_FILE"

echo -e "\n${BLUE}======================================================${NC}"
if [ $RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ All <SystemName> tests passed!${NC}"
else
    echo -e "${RED}✗ Some <SystemName> tests failed${NC}"
    echo -e "${YELLOW}Check the detailed results in: $RESULTS_FILE${NC}"
fi
echo -e "${CYAN}Test results saved to: $RESULTS_FILE${NC}"
echo -e "${BLUE}======================================================${NC}"

exit $RESULT
```

### Windows `.bat` pair

Copy `run_projectile_manager_tests.bat` structure (`setlocal EnableDelayedExpansion`,
`:parse_args` loop, `bin\debug\<system>_tests.exe`, results in `test_results\`), apply the same
substitutions, and save as `tests/test_scripts/run_<system>_tests.bat`. Header comment:
`:: Copyright (c) 2025 Hammer Forged Games, MIT License`.

### Controllers — use the grouped runner instead

Controller tests run through `run_controller_tests.sh` / `.bat`. Add a `--<short>` flag +
`RUN_<SHORT>` variable in the arg parser and help text, and:

```bash
if [ "$RUN_ALL" = true ] || [ "$RUN_<SHORT>" = true ]; then
  EXECUTABLES+=("<system>_tests")
fi
```

Mirror the change in the `.bat`. Other grouped runners exist (`run_game_time_tests.sh`,
`run_entity_data_manager_tests.sh`, `run_npc_memory_tests.sh`, `run_resource_tests.sh`,
`run_event_tests.sh`) — extend one when the new executable belongs to that family.

---

## 2. Functional Test Source (`tests/<subsystem>/<SystemName>Tests.cpp`)

Pattern from current tests: copyright block → `@file` doc comment listing covered contracts →
`BOOST_TEST_MODULE` + linked `<boost/test/unit_test.hpp>` → project includes with subsystem
prefixes → global `ThreadSystem` fixture (only if the runtime path dispatches worker tasks) →
per-suite fixture that inits exactly the managers the path needs with `BOOST_REQUIRE`, cleaning
up in reverse order → `BOOST_FIXTURE_TEST_SUITE` groups by contract.

Fixture scope, determinism, event-wiring, and "don't override production state" rules come
from `.claude/rules/tests.md` — apply them; they are not repeated here.

```cpp
/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file <SystemName>Tests.cpp
 * @brief Tests for <SystemName>
 *
 * Covers:
 * - <contract 1, e.g. init/clean lifecycle and prepareForStateTransition() reset>
 * - <contract 2, e.g. cache invalidation on WorldUnloaded>
 * - <contract 3, e.g. worker-batch results committed on main thread>
 */

#define BOOST_TEST_MODULE <SystemName>Tests
#include <boost/test/unit_test.hpp>

#include "core/ThreadSystem.hpp"
#include "<header-path>"
// Add only the managers the runtime path actually uses, e.g.:
// #include "managers/EntityDataManager.hpp"
// #include "managers/EventManager.hpp"

// ============================================================================
// Global fixture — only when the path dispatches ThreadSystem work
// ============================================================================

struct GlobalThreadSystemFixture
{
    GlobalThreadSystemFixture()
    {
        if (!VoidLight::ThreadSystem::Instance().init())
        {
            throw std::runtime_error("ThreadSystem::init() failed in <SystemName>Tests");
        }
    }

    ~GlobalThreadSystemFixture()
    {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(GlobalThreadSystemFixture);

// ============================================================================
// Fixture — init in dependency order, clean in reverse
// ============================================================================

struct <SystemName>Fixture
{
    <SystemName>Fixture()
    {
        // BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(<SystemName>::Instance().init());
    }

    ~<SystemName>Fixture()
    {
        <SystemName>::Instance().clean();
        // EntityDataManager::Instance().clean();
    }

    static constexpr float FIXED_DT = 1.0f / 60.0f;
};

// ============================================================================
// Lifecycle
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(<SystemName>LifecycleTests, <SystemName>Fixture)

BOOST_AUTO_TEST_CASE(InitLeavesManagerReady)
{
    // Assert the observable post-init state, e.g.:
    // BOOST_CHECK(<SystemName>::Instance().isInitialized());
}

BOOST_AUTO_TEST_CASE(PrepareForStateTransitionResetsState)
{
    // Populate state through the public API, transition, assert it is cleared.
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// <Contract group> — one suite per contract area; delete unused groups
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(<SystemName><Contract>Tests, <SystemName>Fixture)

BOOST_AUTO_TEST_CASE(<ObservableBehaviorName>)
{
    // Arrange with deterministic data, act via update(FIXED_DT) or the public API,
    // assert externally visible results.
}

BOOST_AUTO_TEST_SUITE_END()
```

Controller tests: reuse `tests/controllers/common/ControllerTestFixture.hpp` and the shared
`ControllerGetNameTests.hpp`, `ControllerSubscriptionTests.hpp`,
`ControllerSuspendResumeTests.hpp`, `ControllerResubscribeTests.hpp`,
`ControllerOwnershipTests.hpp` helpers instead of hand-writing those contracts. GPU-dependent
cases must skip gracefully when no device is available (see
`ProjectileRenderControllerTests.cpp`).

---

## 3. Benchmark Source (`tests/performance/<SystemName>Benchmark.cpp`)

Only when requested. Pattern from `ProjectileScalingBenchmark.cpp`: one-time manager init
inside the fixture (`VOIDLIGHT_ENABLE_BENCHMARK_MODE()` first), `prepareForTest()` calling
`prepareForStateTransition()` on each participating manager (including
`WorkerBudgetManager`), seeded RNG, `std::chrono::steady_clock` timing, results printed to
stdout (the runner script tees them into `test_results/`). Do not write files or call
`system()` from the benchmark.

```cpp
/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * <SystemName> Scaling Benchmark
 *
 * 1. Entity scaling from <N> to <M>
 * 2. Threading mode comparison via WorkerBudget adaptive decisions
 */

#define BOOST_TEST_MODULE <SystemName>Benchmark
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <format>
#include <iostream>
#include <random>

#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "<header-path>"

namespace {

class <SystemName>BenchmarkFixture
{
public:
    <SystemName>BenchmarkFixture()
    {
        if (!s_initialized)
        {
            VOIDLIGHT_ENABLE_BENCHMARK_MODE();
            BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
            BOOST_REQUIRE(<SystemName>::Instance().init());
            s_initialized = true;
        }
        m_rng.seed(42);
    }

    void prepareForTest()
    {
        <SystemName>::Instance().prepareForStateTransition();
        VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    }

protected:
    static inline bool s_initialized{false};
    std::mt19937 m_rng;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(<SystemName>ScalingTests, <SystemName>BenchmarkFixture)

BOOST_AUTO_TEST_CASE(<SystemName>Scaling)
{
    constexpr float FIXED_DT = 1.0f / 60.0f;
    constexpr int FRAMES = 100;
    for (size_t count : {100u, 1000u, 5000u})
    {
        prepareForTest();
        // populate <count> entities deterministically

        const auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < FRAMES; ++frame)
        {
            <SystemName>::Instance().update(FIXED_DT);
        }
        const auto end = std::chrono::steady_clock::now();
        const double msPerFrame =
            std::chrono::duration<double, std::milli>(end - start).count() / FRAMES;

        std::cout << std::format("{:>6} entities: {:.3f} ms/frame\n", count, msPerFrame);
    }
}

BOOST_AUTO_TEST_SUITE_END()
```

Runner: model `run_<system>_benchmark.sh` / `.bat` on `run_projectile_benchmark.sh` (timestamped
results file under `test_results/` plus a `*_current.txt` copy). Name must contain `benchmark`
or `scaling` so `run_all_tests.sh` labels it as a benchmark.

---

## 4. CMake Registration (`tests/CMakeLists.txt`)

Never add tests to the root `CMakeLists.txt`, and never add `src/...` sources (`VoidLightLib`
already contains them). The `foreach` loop applies `add_executable`, links
`VoidLightLib Boost::unit_test_framework`, and sets `BOOST_TEST_NO_SIGNAL_HANDLING`; the CTest
loop registers each test with the project root as working directory.

**Edit 1 — `ALL_TESTS`** (append near related entries):
```cmake
    <system>_tests
    <system>_benchmark      # only if generated
```

**Edit 2 — source mapping** (before the closing `endif()` of the mapping chain):
```cmake
    elseif(${test_name} STREQUAL "<system>_tests")
        set(test_source "<subsystem>/<SystemName>Tests.cpp")
    elseif(${test_name} STREQUAL "<system>_benchmark")
        set(test_source "performance/<SystemName>Benchmark.cpp")
```
Careful: names starting with `resource_` or `particle_manager_` are routed through `MATCHES`
branches earlier in the chain — pick a name that does not collide, or extend that branch.

**Optional edits:**
- Uses `EventManagerTestAccess` → add `<system>_tests` to `EVENT_ACCESS_TESTS`.
- Needs a mock or extra helper source → `target_sources(<system>_tests PRIVATE mocks/...)`
  after the loop, like `save_manager_tests`.
- Slow benchmark under CTest → add an `elseif` with a `--run_test=<Suite>/<Case>` filter in the
  CTest registration loop.
- GPU tests → use the `GPU_UNIT_TESTS` / `GPU_INTEGRATION_TESTS` / `GPU_SYSTEM_TESTS` lists and
  their own mapping blocks instead of `ALL_TESTS`.

---

## 5. Master Runners

`tests/test_scripts/run_all_tests.sh` holds two arrays: `CORE_TEST_SCRIPTS` (fast; what
`--core-only` runs) and `BENCHMARK_TEST_SCRIPTS`. Add the new script to the right one:

```bash
  "$SCRIPT_DIR/run_<system>_tests.sh"
```

`tests/test_scripts/run_all_tests.bat` keeps its own `for %%T in (...)` lists — add the `.bat`
there too:

```bat
    run_<system>_tests.bat
```

Skip both when the executable was added to an existing grouped runner. Never edit the root
`run_all_tests.sh` redirect wrapper.

---

## 6. Documentation (`tests/TESTING.md`)

Add the runner to the "Available Test Scripts" lists (Linux/macOS and Windows) and a short
section under "Test Implementation Details":

````markdown
### <SystemName> Tests

- **Source:** `tests/<subsystem>/<SystemName>Tests.cpp`
- **Executable:** `bin/debug/<system>_tests`
- **Runner:** `tests/test_scripts/run_<system>_tests.sh` (`.bat` on Windows)
- **Covers:** <contracts, one line each>

```bash
./bin/debug/<system>_tests --list_content
./bin/debug/<system>_tests --run_test="<CaseName>*"
```
````

If the subsystem has an owning doc under `docs/<subsystem>/` that lists its tests, add a line there too.
