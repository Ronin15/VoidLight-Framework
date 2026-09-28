---
name: voidlight-memory-profiler
description: Memory leak, memory-error, data-race, and heap-usage profiling for SDL3 VoidLight-Framework using valgrind memcheck/massif (Linux), AddressSanitizer, ThreadSanitizer, and a static buffer-reuse audit, with baseline comparison and a report under test_results/memory_profiles/. Use when the user asks to check for leaks, profile memory or peak heap, investigate per-frame allocations or frame spikes, run ASan/TSan, or audit buffer reuse - or at Branch/PR time for sanitizer runs (Valgrind is optional there).
allowed-tools: [Bash, Read, Write, Grep, Glob]
---

# VoidLight-Framework Memory Profiler

Playbook only. Per-mode commands live in `references/modes.md`; the report
template in `references/reporting.md`. Coding rules for buffers, allocations,
and threading are in CLAUDE.md ("Performance and Threading") — cite them, don't
restate them.

## Gate placement

Sanitizers (ASan, TSan) are Branch/PR gates; Valgrind is optional there
(`docs/framework-implementation-slices.md`). None of this is a per-change or
slice-complete check. ASan and TSan are mutually exclusive builds.

## Modes

| Mode | Tool | Platform | Use when |
|------|------|----------|----------|
| 1. Leak check | valgrind memcheck | Linux | leaks / invalid access in specific systems |
| 2. ASan | AddressSanitizer build | any | fast memory-error + leak feedback, PR gate |
| 2b. TSan | ThreadSanitizer build | any | data races / lock-order issues, PR gate |
| 3. Heap profile | valgrind massif | Linux | peak heap by test/system, release prep (slow) |
| 4. Buffer-reuse audit | static grep + code reading | any | per-frame allocation suspicions |

## Bundled scripts (`scripts/`, run from repo root)

- `run_leak_check.sh [PATTERN]` — ad-hoc memcheck over `bin/debug/<PATTERN>`
  (default `*tests`, skips `gpu_*`), uses `tests/valgrind/valgrind_suppressions.supp`,
  logs to `test_results/memory_profiles/`.
- `run_massif_all_tests.sh [-o DIR] [-t DIR] [-v]` — massif + `ms_print` over
  all non-GPU `*tests` (tens of minutes).
- `parse_massif.py` — summarizes `*_massif_report.txt` in `$OUTPUT_DIR`
  (default `test_results/memory_profiles/`), writes `memory_baseline.csv`.

Repo-owned Valgrind runners in `tests/valgrind/` (see its README) are preferred
for curated memcheck (`quick_memory_check.sh`), app-lifecycle leaks
(`runtime_memory_analysis.sh`), and cache/callgrind profiling.

## Workflow

1. **Scope.** Ask the user only if unclear: mode; scope (Core / AI /
   Collision-Pathfinding / All / named executables); baseline (compare / none /
   create). Skip questions the request already answers.
2. **Build.** Modes 1/3 need a plain Debug build
   (`cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build`).
   Modes 2/2b need the sanitizer build from CLAUDE.md ("Sanitizers");
   remove `build/CMakeCache.txt` when switching.
3. **Run.** Read `references/modes.md` → the chosen mode's section (it also
   has the scoped executable lists and baseline commands).
4. **Verify findings.** Before reporting a leak or per-frame allocation, read
   the code at the reported frame. Possible leaks in SDL/font/driver frames are
   usually noise. Never relax a test to hide a finding.
5. **Report.** Read `references/reporting.md`; write the report and print the
   console summary.

## Reference files

- `references/modes.md` — read at step 3: executable scopes, per-mode commands,
  log grep patterns, baseline save/compare.
- `references/reporting.md` — read at step 5: report skeleton, console summary,
  severity classification.
