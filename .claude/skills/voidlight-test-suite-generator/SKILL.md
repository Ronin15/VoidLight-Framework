---
name: voidlight-test-suite-generator
description: Generates complete test suite infrastructure (test scripts, functional tests, benchmark tests, output directories, CMake integration) for a new SDL3 VoidLight-Framework system or manager following project conventions. Use when adding a new manager or system that needs testing infrastructure.
allowed-tools: [Read, Write, Bash, Edit, Grep]
---

# VoidLight-Framework Test Suite Generator

Automates standardized test infrastructure for a new VoidLight-Framework system, following the
project's established patterns. This file is the lean playbook; all code/script/CMake templates
live in `references/templates.md` and are loaded on demand from the steps below.

## What This Skill Generates

1. **Test runner pair** — `tests/test_scripts/run_<system>_tests.sh` + `.bat`
2. **Functional test source** — `tests/<SystemName>Tests.cpp` (PascalCase source)
3. **Benchmark source** (optional) — `tests/<SystemName>Benchmark.cpp` + runner pair
4. **`tests/CMakeLists.txt`** — add executable to the `ALL_TESTS` list + source mapping
5. **Master runner** — add to the `SCRIPT_DIR` array in `tests/test_scripts/run_all_tests.sh`
6. **Output directory** — `test_results/<system>/`
7. **Documentation stub** — `tests/docs/<SystemName>_Testing.md`

## When To Use

Activate automatically when the user says things like:
- "generate tests for NewManager"
- "create test suite for AnimationSystem"
- "set up testing for SoundManager"
- "scaffold tests for new system"

## Naming Invariants (do not drift from these)

- Test **executables** are snake_case: `<system>_tests`, `<system>_benchmark`.
- Test **source files** are PascalCase: `<SystemName>Tests.cpp`, `<SystemName>Benchmark.cpp`.
- Tests register in **`tests/CMakeLists.txt`** (NOT root `CMakeLists.txt`): add to the
  `ALL_TESTS` list and map to the source in the `foreach` block. No per-test `add_executable`.
- Every runner ships as a `.sh` + `.bat` pair.
- The real master runner is `tests/test_scripts/run_all_tests.sh` with a `SCRIPT_DIR` array;
  root `run_all_tests.sh` is a backward-compat wrapper — never edit it.
- Benchmarks time with `std::chrono::steady_clock`.

## Collect User Input First

1. **System Name** — PascalCase, used for file/suite naming (e.g. `AnimationManager`).
2. **Manager Class Name** — actual C++ class under test; must exist in the codebase.
3. **Test categories** — Functional (always), Integration (if it integrates), Benchmark (if perf-critical).
4. **Integration dependencies** (if any) — e.g. `AIManager, CollisionManager`.
5. **Key functionality** — brief description used to seed test cases.

Verify the named class exists before generating. If the system already has tests, ask whether
to regenerate (overwrite) or add cases instead.

## Generation Workflow

### Step 1 — Discover the current convention (do NOT trust templates blindly)

Repo conventions drift. Read the live pattern first and prefer it over the templates if they differ:

```bash
# 1. List existing runner pairs (every .sh has a .bat)
ls tests/test_scripts/run_*tests*.sh tests/test_scripts/run_*tests*.bat

# 2. Read a representative runner pair to copy structure verbatim
Read: $PROJECT_ROOT/tests/test_scripts/run_ai_optimization_tests.sh
Read: $PROJECT_ROOT/tests/test_scripts/run_ai_optimization_tests.bat

# 3. Confirm CMake registration + master runner mechanism
Read: $PROJECT_ROOT/tests/CMakeLists.txt                 # ALL_TESTS list + foreach mapping
Read: $PROJECT_ROOT/tests/test_scripts/run_all_tests.sh  # SCRIPT_DIR array

# 4. Inspect a real test source for include/style/naming (PascalCase, e.g. AIOptimizationTest.cpp)
ls tests/*.cpp
```

### Step 2 — Generate the test runner pair

Generate `tests/test_scripts/run_<system>_tests.sh` (then `chmod +x`) and its Windows `.bat`
pair. Template and substitutions: read `references/templates.md` § 1. Timeout: 30s functional,
120s benchmark.

### Step 3 — Generate the functional test source

Write `tests/<SystemName>Tests.cpp` (PascalCase). Template: read `references/templates.md` § 2.
Add `#define INTEGRATION_TESTS` if integration was selected and `#define THREAD_SAFETY_TESTS`
for managers; seed cases from the key-functionality description.

### Step 4 — Generate the benchmark source (optional)

Only if "Benchmark Tests" was selected. Write `tests/<SystemName>Benchmark.cpp` and its runner
pair (`run_<system>_benchmark.sh` + `.bat`). Template: read `references/templates.md` § 3.

### Step 5 — Register in `tests/CMakeLists.txt`

Two Edits: add the executable to `ALL_TESTS`, and add a source mapping in the `foreach` block.
Exact snippets and rules: read `references/templates.md` § 4. Read the file first, match
formatting. Do not add per-test `add_executable` or `src/...` sources — `VoidLightLib` covers them.

### Step 6 — Update the master runner

Edit `tests/test_scripts/run_all_tests.sh` to add the new runner path(s) into the `SCRIPT_DIR`
array. Details: read `references/templates.md` § 5. Never edit root `run_all_tests.sh`.

### Step 7 — Create the output directory

```bash
mkdir -p "$PROJECT_ROOT/test_results/<system>"
touch "$PROJECT_ROOT/test_results/<system>/.gitkeep"
```

### Step 8 — Generate the documentation stub

Write `tests/docs/<SystemName>_Testing.md`. Template: read `references/templates.md` § 6.

### Step 9 — Verification build

```bash
cd $PROJECT_ROOT
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
ls -lh bin/debug/<system>_tests           # and bin/debug/<system>_benchmark if generated
./tests/test_scripts/run_<system>_tests.sh --verbose
```

## Report to the User

Summarize what was created and modified, plus build status and initial test result:
- Created: runner pair(s), `tests/<SystemName>Tests.cpp` (+ benchmark), docs stub, `test_results/<system>/`.
- Modified: `tests/CMakeLists.txt` (ALL_TESTS + mapping), `tests/test_scripts/run_all_tests.sh` (array).
- Next steps: implement real cases (replace placeholders), rebuild, run the runner, define benchmark baselines.
- Test executables: `bin/{debug,release}/<system>_tests`, `bin/debug/<system>_benchmark`.

## Operating Rules

- Always collect user input and verify the class exists before generating.
- Follow the naming invariants above; include the copyright header on every generated file.
- Make scripts executable; verify CMake edits match surrounding formatting.
- Build after generation to catch errors early. If CMake edits fail, show the snippet and
  instruct manual insertion. If the build fails, show errors and suggest fixes.
