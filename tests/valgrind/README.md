# Valgrind Diagnostics - VoidLight-Framework

This directory contains targeted Valgrind workflows for deeper diagnostics and
performance profiling. These scripts are not the primary test runner.

Use the normal Boost/CTest tests first. Use ASan and TSan for fast memory and
race feedback. Use Valgrind when you need deeper heap/lifetime evidence, runtime
shutdown inspection, or cache/function profiling.

## What To Run

Build Debug for Memcheck:

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build
```

Build Profile for Cachegrind or Callgrind:

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Profile
ninja -C build
```

Run targeted Memcheck over the faster high-risk ownership/lifetime tests:

```bash
./tests/valgrind/quick_memory_check.sh
./tests/valgrind/quick_memory_check.sh --extended
./tests/valgrind/quick_memory_check.sh --target entity_data_manager_tests
./tests/valgrind/quick_memory_check.sh --target edm_lifecycle
```

Run runtime memory analysis against the real application lifecycle:

```bash
./tests/valgrind/runtime_memory_analysis.sh
./tests/valgrind/runtime_memory_analysis.sh --profile 300
```

Run Cachegrind performance analysis:

```bash
./tests/valgrind/cache_performance_analysis.sh
./tests/valgrind/cache_performance_analysis.sh --benchmarks
./tests/valgrind/runtime_cache_analysis.sh --profile 300
```

Run Callgrind function profiling:

```bash
./tests/valgrind/callgrind_profiling_analysis.sh ai
./tests/valgrind/callgrind_profiling_analysis.sh events resources
./tests/valgrind/callgrind_profiling_analysis.sh collision_pathfinding
./tests/valgrind/callgrind_profiling_analysis.sh runtime_managers
./tests/valgrind/callgrind_profiling_analysis.sh render_ui
./tests/valgrind/callgrind_profiling_analysis.sh benchmarks
./tests/valgrind/analyze_callgrind_summaries.sh
```

## Script Roles

- `quick_memory_check.sh`: targeted Memcheck over deterministic unit and
  integration tests with ownership, sidecar, manager lifecycle, and cleanup
  risk.
- `runtime_memory_analysis.sh`: Memcheck over the real `VoidLight_Template`
  executable to inspect shutdown leaks, file descriptors, and SDL/resource
  lifecycle behavior.
- `cache_performance_analysis.sh`: Cachegrind over selected data-oriented and
  performance-sensitive tests.
- `runtime_cache_analysis.sh`: Cachegrind over the real application runtime.
- `callgrind_profiling_analysis.sh`: function-level profiling by subsystem
  category.
- `analyze_callgrind_summaries.sh`: post-processes Callgrind summaries.
- `valgrind_targets.sh`: shared target manifest used by the retained test
  runners. Keep this aligned with `ctest --test-dir build -N`.
- `valgrind_suppressions.supp`: suppressions for SDL/font/system-library noise.

The old broad wrappers were intentionally removed. A single "complete Valgrind
suite" was too slow and mixed unrelated tools: Memcheck, Cachegrind, Callgrind,
and race detection. The remaining scripts each have one job.

## Effectiveness For Memory Issues

Running tests under Valgrind is useful, but only for paths the tests execute.

Memcheck is good at finding:

- Heap leaks.
- Invalid reads and writes.
- Use-after-free and double free.
- Some uninitialized-value flows.
- Shutdown cleanup issues when the fixture or runtime exercises the real owner
  lifecycle.

Memcheck is not enough by itself:

- It does not cover untested runtime paths.
- It is much slower than normal tests and should stay targeted.
- It can produce noise from SDL, font, graphics, and C++ runtime internals.
- It is not the best race detector for modern C++ code.

For this project, use this order:

1. Run the targeted Boost executable normally.
2. Use ASan/UBSan for fast memory and undefined-behavior feedback.
3. Use TSan for data races.
4. Use Valgrind Memcheck for deeper heap/lifetime inspection and periodic
   sampling.
5. Use Cachegrind and Callgrind for performance profiling, not correctness.

Valgrind is useful here, but it should not be treated as proof of complete
memory safety. It is a targeted diagnostic and profiling layer beside the
sanitizer workflows.

## Target Policy

Default Memcheck targets are the faster ownership/lifetime-heavy tests:

```text
sparse_sidecar_tests
knockback_sidecar_tests
entity_state_manager_tests
npc_memory_tests
resource_factory_tests
resource_template_manager_tests
resource_template_manager_json_tests
resource_integration_tests
world_resource_manager_tests
world_manager_tests
world_manager_event_integration_tests
event_manager_tests
event_manager_behavior_tests
projectile_manager_tests
collision_manager_edm_integration_tests
pathfinder_manager_edm_integration_tests
manager_runtime_tests
loading_state_tests
background_simulation_manager_tests
```

Extended Memcheck adds slow or lower-frequency lifecycle/IO coverage. EDM is
covered by filtered slices instead of the full `entity_data_manager_tests`
binary, because the full executable is too broad for a quick Memcheck pass and
can dominate or time out the run.

```text
save_manager_tests
settings_manager_tests
game_state_manager_tests
resource_edge_case_tests
resource_architecture_tests
pathfinder_ai_contention_tests
collision_pathfinding_integration_tests
edm_lifecycle
edm_destruction_queue
edm_slot_reuse
edm_transition_cache_clear
edm_inventory_transfer
```

`--target entity_data_manager_tests` is a convenience alias for the EDM slice
set above. Use a slice target such as `--target edm_slot_reuse` when you only
need one EDM memory path.

The EDM slices currently exercise:

- `edm_lifecycle`, an alias for memory-relevant lifecycle cleanup cases:
  `TestCleanAndReinit`, `TestDirectDestroyClearsBehaviorConfigForSlotReuse`,
  and `TestStateTransitionClearsBehaviorStatePools`.
- `DestructionQueueTests/*`
- `SlotReuseTests/TestSlotReuseAfterDestruction`
- `StateTransitionCachedIndicesTests/TestPrepareForStateTransitionClearsKindIndices`
- `NPCRenderDataTests/TestInventoryTransferMovesFullQuantityAtomically`

Run the full `entity_data_manager_tests` executable normally or under ASan when
you need full EDM behavioral coverage. Keep Memcheck focused on representative
ownership, destruction, slot-reuse, cache-clear, and inventory-transfer paths.

GPU/device tests remain manual. They usually need device/display state and tend
to produce driver/runtime noise under Valgrind.

## Prerequisites

- Linux.
- Valgrind 3.18+.
- Debug or Profile build artifacts.
- glibc debug symbols on distributions whose dynamic linker is stripped
  (`libc6-dbg` on Debian/Ubuntu, `glibc-debuginfo` on Fedora/RHEL/SUSE).

If Valgrind exits before launching a test with a mandatory redirection error for
`ld-linux-x86-64.so.2` and `memcmp`, install the glibc debug symbols package.
That is an environment prerequisite failure, not a VoidLight test failure.

## Outputs

Generated artifacts are written under:

```text
test_results/valgrind/memcheck/
test_results/valgrind/cachegrind/
test_results/valgrind/callgrind/
test_results/valgrind/runtime/
test_results/valgrind/runtime_cache/
```

Inspect the specific log before treating a summary line as authoritative.

## Reading Results

Each retained runner prints actionable terminal output and writes a concise
report. Use the terminal output first; raw Valgrind logs are drill-down material
only.

The target-level runners print one useful line per target with the measured
values and a next action. Examples:

```text
PASS memcheck entity_state_manager_tests errors=0 definite=0B indirect=0B possible=0B action=none
REVIEW cachegrind pathfinder_manager_tests I1=0.12% D1=12.40% LL=0.04% branch=1.50% action=review_data_layout_or_iteration_order
PASS callgrind ai_optimization_tests instructions=123456789 top="517,392,310 (88.49%) src/managers/EntityDataManager.cpp:EntityDataManager::init()" action=review_top_hotspot
```

The final summary ends in `Result: PASS`, `Result: REVIEW`, or `Result: FAIL`.

Memcheck results:

- Each target line includes `errors`, `definite`, `indirect`, `possible`, and
  `action`.

```text
PASS memcheck entity_state_manager_tests errors=0 definite=0B indirect=0B possible=0B
```

- `PASS`: no Valgrind errors and no definite/indirect leaks for that target.
- `REVIEW`: no hard failure, but possible leaks were reported. Possible leaks
  are often runtime or library noise.
- `FAIL`: Valgrind reported memory errors, definite leaks, indirect leaks, or
  the test executable exited non-zero.
- `TOOL_FAILED`: Valgrind did not get far enough to run the test. The action
  field names the setup fix when it can, such as `install_glibc_debug_symbols`.

Memcheck writes an aggregate table to:

```text
test_results/valgrind/memcheck/memcheck_report_<build>_<timestamp>.md
```

Per-target logs are written as:

```text
test_results/valgrind/memcheck/<target_label>.memcheck.log
```

Cachegrind results:

- The terminal summary shows the generated Markdown report path.
- Each target line includes I1, D1, LL, branch miss rates, and an action such as
  `review_data_layout_or_iteration_order`, `review_ll_cache_misses`, or
  `review_branching_in_hot_path`.
- The final `Result` is `PASS` when every selected target completed and `FAIL`
  when any target is missing, timed out, or failed to produce Cachegrind output.
- The report table lists I1, D1, LL, and branch miss rates for each target.
- Raw data is written under `test_results/valgrind/cachegrind/raw/`.
- Annotated output, when `cg_annotate` is available, is written under
  `test_results/valgrind/cachegrind/annotations/`.

Callgrind results:

- The terminal summary shows the generated Markdown report path.
- Each target line shows whether raw and summary output were produced,
  instruction totals when `callgrind_annotate` is available, the top parsed
  project hotspot, and an action. Startup frames, Boost harness frames,
  standard library frames, and Callgrind synthetic frames such as
  `???:(below main)` are filtered out of the terminal hotspot.
- The final `Result` is `PASS` when every selected target produced Callgrind
  output and `FAIL` when any target is missing, timed out, or failed.
- Raw call graph data is written under `test_results/valgrind/callgrind/raw/`.
- Function summaries are written under
  `test_results/valgrind/callgrind/summaries/`.
- Open raw `.callgrind.out` files in KCachegrind, or run
  `./tests/valgrind/analyze_callgrind_summaries.sh` after profiling.

Runtime results:

- Runtime memory reports are written under `test_results/valgrind/runtime/`.
- Runtime cache reports are written under
  `test_results/valgrind/runtime_cache/`.
- Runtime scripts write summary files and Markdown reports with an automated
  `Result` and `Action`. Inspect the raw Valgrind log only when the summary
  reports a finding.

## Maintenance

When tests are added, removed, or renamed:

1. Run `ctest --test-dir build -N`.
2. Update `tests/valgrind/valgrind_targets.sh` only if the new target belongs in
   Memcheck, Cachegrind, or Callgrind.
3. Keep GPU/device and very long benchmark coverage opt-in.
4. Run `bash -n tests/valgrind/*.sh`.
5. If the host supports Valgrind, run one narrow smoke target.
