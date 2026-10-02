---
name: voidlight-test-suite-generator
description: Scaffolds Boost.Test infrastructure for a new SDL3 VoidLight-Framework manager, controller, or system - test source in the matching tests/<subsystem>/ directory, tests/CMakeLists.txt registration, a .sh/.bat runner pair (or an entry in an existing grouped runner), master-runner registration, and optional benchmark - following .claude/rules/tests.md conventions. Use when adding a new manager or system that has no test executable yet.
allowed-tools: [Read, Write, Bash, Edit, Grep, Glob]
---

# VoidLight-Framework Test Suite Generator

Scaffolds test infrastructure for a new VoidLight-Framework system following the live repo
patterns. This file is the playbook; code/script/CMake templates live in
`references/templates.md` and are loaded on demand from the steps below.

**Rules that override the templates:** root `CLAUDE.md`, `.claude/rules/tests.md`, and the narrower
`.claude/rules/tests-ai.md` / `.claude/rules/tests-managers.md` when the source lands there. Read the ones
that apply before generating; do not leave placeholder assertions (`BOOST_CHECK(true)`) in
generated cases.

## What This Skill Generates

1. **Functional test source** — `tests/<subsystem>/<SystemName>Tests.cpp`
2. **Benchmark source** (optional) — `tests/performance/<SystemName>Benchmark.cpp`
3. **`tests/CMakeLists.txt`** — executable in `ALL_TESTS` + source mapping (+ extra target
   wiring only when needed, e.g. `EVENT_ACCESS_TESTS`)
4. **Runner** — new `tests/test_scripts/run_<system>_tests.sh` + `.bat` pair, **or** an entry in
   an existing grouped runner (e.g. `run_controller_tests.sh`/`.bat` for controllers)
5. **Master runners** — `CORE_TEST_SCRIPTS` / `BENCHMARK_TEST_SCRIPTS` arrays in
   `tests/test_scripts/run_all_tests.sh` **and** the matching `for %%T in (...)` list in
   `run_all_tests.bat`
6. **Docs** — a short section in `tests/TESTING.md` (and the owning `docs/<subsystem>/` doc if
   it lists tests). There is no `tests/docs/` directory; do not create one.

## When To Use

- "generate tests for NewManager" / "scaffold tests for the new system"
- "set up testing for SoundManager"

If the system already has an executable, add cases to the existing source instead.

## Naming and Layout Invariants

- Executables are snake_case: `<system>_tests`, `<system>_benchmark` (or
  `<system>_scaling_benchmark`). Output: `bin/debug/` or `bin/release/`.
- Sources are PascalCase and live in the subsystem directory that matches `src/`:
  `tests/managers/`, `tests/controllers/`, `tests/core/`, `tests/ai/`, `tests/world/`,
  `tests/collisions/`, `tests/events/`, `tests/gpu/`, `tests/integration/` (cross-manager),
  `tests/utils/`, `tests/performance/` (benchmarks). Root `tests/*.cpp` is legacy — don't add there.
- Register in **`tests/CMakeLists.txt`** only (not root `CMakeLists.txt`). The shared `foreach`
  handles `add_executable`, linking `VoidLightLib Boost::unit_test_framework`,
  `BOOST_TEST_NO_SIGNAL_HANDLING`, and CTest with `WORKING_DIRECTORY` = project root.
  GPU tests use the separate `GPU_UNIT_TESTS` / `GPU_INTEGRATION_TESTS` / `GPU_SYSTEM_TESTS` lists.
- Boost.Test is linked, not header-only: `#define BOOST_TEST_MODULE <SystemName>Tests` then
  `#include <boost/test/unit_test.hpp>`.
- Every standalone runner ships as `.sh` + `.bat`. The real master runner is
  `tests/test_scripts/run_all_tests.sh`; root `run_all_tests.sh` is a redirect wrapper — never edit it.
- Runners write results flat into `test_results/` (e.g. `test_results/<system>_tests_results.txt`);
  no per-system subdirectory is needed.
- Benchmarks time with `std::chrono::steady_clock`.

## Collect Input First

1. **System name** (PascalCase) and the C++ class under test — verify it exists
   (`include/<subsystem>/<Class>.hpp`).
2. **Subsystem directory** for the source (derive from the header path).
3. **Runtime path participants** — trace which managers the class actually calls (EDM,
   ThreadSystem, EventManager, Collision, Pathfinder, WorkerBudget…) before writing the fixture.
4. **Benchmark?** — only if perf-critical.
5. **Key contracts** to cover — lifecycle (init / update / `prepareForStateTransition()` /
   clean), event persistence, cache invalidation, handle/slot reuse, worker-batch futures.

## Generation Workflow

### Step 1 — Discover the live convention (templates can drift)

```bash
ls tests/<subsystem>/                                   # neighbouring sources
Read: tests/managers/ProjectileManagerTests.cpp         # manager fixture pattern
Read: tests/controllers/ProjectileRenderControllerTests.cpp   # controller pattern
Read: tests/core/WorkerBudgetTests.cpp                  # global ThreadSystem fixture
Read: tests/test_scripts/run_projectile_manager_tests.sh     # current standalone runner (+ .bat)
Read: tests/CMakeLists.txt                              # ALL_TESTS + foreach mapping + CTest block
sed -n '/^CORE_TEST_SCRIPTS=(/,/^)/p' tests/test_scripts/run_all_tests.sh
```
Controllers: also read `tests/controllers/common/ControllerTestFixture.hpp` and the shared
`Controller*Tests.hpp` contract helpers, and `tests/test_scripts/run_controller_tests.sh`.

### Step 2 — Functional test source

Write `tests/<subsystem>/<SystemName>Tests.cpp`. Template: `references/templates.md` § 2.
Replace every commented placeholder with real assertions derived from the key contracts; delete
sections that do not apply rather than leaving empty cases.

### Step 3 — Benchmark source (optional)

`tests/performance/<SystemName>Benchmark.cpp`. Template: `references/templates.md` § 3.

### Step 4 — Register in `tests/CMakeLists.txt`

Add to `ALL_TESTS` and add the `elseif` mapping. If the test uses `EventManagerTestAccess`, add
it to `EVENT_ACCESS_TESTS`. Benchmarks that are slow under CTest get a `--run_test=` filter in
the CTest registration block. Snippets: `references/templates.md` § 4.

### Step 5 — Runner

- Controller → add a flag + `EXECUTABLES+=` entry to `run_controller_tests.sh` and the `.bat`.
- Otherwise → new `run_<system>_tests.sh` (`chmod +x`) + `.bat`. Template: § 1.
- Benchmark → `run_<system>_benchmark.sh` + `.bat` modeled on `run_projectile_benchmark.sh`.

### Step 6 — Master runners

Add the new `.sh` to `CORE_TEST_SCRIPTS` (or `BENCHMARK_TEST_SCRIPTS`) in
`tests/test_scripts/run_all_tests.sh`, and the `.bat` to the matching list in
`run_all_tests.bat`. Skip when the test was added to an existing grouped runner. § 5.

### Step 7 — Docs

Add a short section to `tests/TESTING.md` (what it covers, executable, runner). § 6.

### Step 8 — Verify (per-change gate)

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug    # only if not configured / CMake changed
ninja -C build <system>_tests
./bin/debug/<system>_tests --list_content
./bin/debug/<system>_tests
./tests/test_scripts/run_<system>_tests.sh           # confirm the runner finds the exe
```
Do **not** run `run_all_tests.sh --core-only` here — that is the Branch/PR gate.

## Report to the User

- Created / modified files (source, runner pair or grouped-runner edit, CMake, master runners, TESTING.md).
- Build result and test result, stating exactly which executable was run.
- Any contracts left uncovered and why.

## Operating Rules

- Verify the class exists and trace its runtime path before writing the fixture.
- Copyright header on every generated file (C++ block comment; `#`/`::` line in scripts).
- Match surrounding formatting when editing CMake and runner lists.
- If the build fails, show the errors and fix them.
