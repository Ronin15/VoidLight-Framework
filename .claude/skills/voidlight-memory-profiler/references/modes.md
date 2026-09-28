# Memory Profiler — Per-Mode Commands

Read only the section for the selected mode. Run everything from the repo root.
`S=.claude/skills/voidlight-memory-profiler/scripts`,
`OUT=test_results/memory_profiles`.

## Test selection

- **All Systems:** discover at runtime, never freeze a list. Skip `gpu_*`
  (device/display-dependent; manual only per `tests/valgrind/README.md`):
  ```bash
  mapfile -t TEST_EXECUTABLES < <(find ./bin/debug -maxdepth 1 -name '*tests' ! -name 'gpu_*' -type f -executable | sort)
  ```
- **Core:** `thread_system_tests buffer_utilization_tests buffer_reuse_tests event_manager_tests`
- **AI:** `thread_safe_ai_manager_tests ai_optimization_tests behavior_functionality_tests ai_manager_edm_integration_tests`
- **Collision/Pathfinding:** `collision_system_tests pathfinder_manager_tests collision_pathfinding_integration_tests`
- **Threading (TSan):** `thread_system_tests thread_safe_ai_manager_tests thread_safe_ai_integration_tests particle_manager_threading_tests event_coordination_integration_tests pathfinder_ai_contention_tests`

Confirm names with `ls bin/debug` if a run reports a missing executable.

## Mode 1 — Leak check (valgrind memcheck, Linux)

Prefer the repo's curated runner (suppressions, timeouts, EDM slices,
PASS/REVIEW/FAIL lines, writes `test_results/valgrind/memcheck/`):
```bash
./tests/valgrind/quick_memory_check.sh                 # default target set
./tests/valgrind/quick_memory_check.sh --extended
./tests/valgrind/quick_memory_check.sh --target <exe-or-edm-slice>
```
For an ad-hoc pattern over arbitrary executables (writes `$OUT/<test>_memcheck.log`):
```bash
$S/run_leak_check.sh "*collision*tests"
```
Drill into a log: `grep -E "definitely lost|indirectly lost|possibly lost|Invalid (read|write)|ERROR SUMMARY" <log>`.
Runtime (app lifecycle) leaks: `./tests/valgrind/runtime_memory_analysis.sh`.
If valgrind aborts on an `ld-linux` / `memcmp` redirection error, install glibc
debug symbols — environment issue, not a test failure.

## Mode 2 — AddressSanitizer

Build with the ASan command from CLAUDE.md ("Sanitizers"). Remove
`build/CMakeCache.txt` first when switching sanitizer/build options (no need
to delete `build/`). The ASan build overwrites `bin/debug/`; rebuild plain
Debug afterwards before running valgrind.
```bash
mkdir -p $OUT
export ASAN_OPTIONS="detect_leaks=1:symbolize=1"
for t in "${TEST_EXECUTABLES[@]}"; do "$t" --log_level=test_suite > "$OUT/$(basename "$t")_asan_output.txt" 2>&1; echo "$(basename "$t") exit=$?"; done
grep -lE "ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:" $OUT/*_asan_output.txt
```
Classify by the first `ERROR:` line (heap-buffer-overflow, heap-use-after-free,
double-free, alloc-dealloc-mismatch, detected memory leaks) and report the top
project frame.

## Mode 2b — ThreadSanitizer

Build with the TSan command from CLAUDE.md (mutually exclusive with ASan; clear
`build/CMakeCache.txt` first) and export its suppressions:
```bash
export TSAN_OPTIONS="suppressions=$(pwd)/tests/tsan_suppressions.txt"
for t in "${THREAD_TESTS[@]}"; do "$t" --log_level=test_suite > "$OUT/$(basename "$t")_tsan_output.txt" 2>&1; done
grep -c "WARNING: ThreadSanitizer: \(data race\|lock-order-inversion\|thread leak\)" $OUT/*_tsan_output.txt
```

## Mode 3 — Heap profile (valgrind massif, Linux, slow)

```bash
$S/run_massif_all_tests.sh [--output DIR] [--tests DIR]   # all *tests except gpu_*; tens of minutes
python3 $S/parse_massif.py                                 # reads $OUT (or $OUTPUT_DIR); writes memory_baseline.csv
```
For a scoped run, invoke massif directly on one executable:
`valgrind --tool=massif --massif-out-file=$OUT/<t>_massif.out --time-unit=ms <exe>`
then `ms_print $OUT/<t>_massif.out > $OUT/<t>_massif_report.txt` before parsing.
Name allocation sites from the `ms_print` tree (project frames only).

## Mode 4 — Buffer-reuse audit (static)

Grep for candidates, then **read each hit** and trace whether it is on a
per-frame / per-batch path before reporting it (the rules themselves are in
CLAUDE.md "Performance and Threading"):
```bash
# locals constructed inside update/process/batch functions
grep -nE "^\s+std::(vector|unordered_map|string)<[^>]*>\s+\w+\s*(;|\{|=)" -r --include=*.cpp src/managers src/ai src/controllers
# capacity-losing resets and swaps
grep -rnE "= std::vector<|\.swap\(|shrink_to_fit" --include=*.cpp src/managers src/ai src/controllers
# return-by-value container APIs in headers (prefer ref-based buffer APIs)
grep -nE "^\s+std::vector<[^>]+>\s+\w+\(.*\)\s*(const)?\s*;" -r include/managers include/ai
```
Treat init/load/shutdown-time allocations as fine; only hot paths matter.

## Baseline comparison

- Baseline dir: `$OUT/baseline/` (copies of `*_memcheck.log`, `*_massif.out`,
  `memory_baseline.csv`, plus `baseline_metadata.txt` with date/branch/commit).
- Compare `definitely lost` bytes and `total heap usage` allocs per memcheck
  log, and `peak_bytes` per row of `memory_baseline.csv`. Report deltas;
  any new definite leak is a regression regardless of size.
- "Create new baseline": `mkdir -p $OUT/baseline && cp $OUT/*_memcheck.log $OUT/*_massif.out $OUT/memory_baseline.csv $OUT/baseline/ 2>/dev/null`
  and write the metadata file.
