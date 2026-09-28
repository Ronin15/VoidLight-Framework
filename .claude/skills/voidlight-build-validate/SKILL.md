---
name: voidlight-build-validate
description: Branch/PR-level validation for VoidLight-Framework - full Debug build with warning count, optional app smoke test, the core-only suite (run_all_tests.sh --core-only --errors-only), and a summary report. Use when a branch is ready to merge/PR or the user explicitly asks for a full validation or core-suite run. Not for per-change or slice-complete checks.
allowed-tools: [Bash, Read, Write]
---

# VoidLight-Framework Build Validation

Runs the core-only suite, so it is the **Branch / PR** gate from
`docs/framework-implementation-slices.md` (or an explicit user request). Gate summary:

| Gate | What to run | This Skill? |
| --- | --- | --- |
| Per-change | `ninja -C build app` (app only, fast) or `ninja -C build <test_exe>` + `./bin/debug/<test_exe> [--run_test="Case*"]` | No — run directly |
| Slice complete | `ninja -C build` + the Boost.Test executables covering the changed code; no core-only suite, no benches | No |
| Slice review | `game-systems-architect` on the slice diff before commit | No |
| Branch / PR | `run_all_tests.sh --core-only --errors-only` + cppcheck, clang-tidy, ASan, TSan | **Yes** — core-suite part |

For "does it build?" during implementation, run the per-change gate instead and stop.
Static analysis is `voidlight-quality-check`; sanitizer builds are separate (mutually
exclusive, see root `CLAUDE.md`). Run everything from the project root. Never commit or edit sources.

## Step 1 — Debug build

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build 2>&1 | tee /tmp/voidlight_build.log
```

Full `ninja -C build` is required: the core suite needs every test executable (`app` builds
only `VoidLight_Template`). Success = `ninja` exit code 0 (`${PIPESTATUS[0]}`). Count warnings
from the captured log — an incremental build only reports files it recompiled:

```bash
grep -E "warning:|error:" /tmp/voidlight_build.log | sort -u | head -n 100
```

Flag >5 warnings as a warning, >20 as a concern. On failure, show the first ~20 errors with
file:line and stop (report exit code 1).

## Step 2 — Smoke test (display required)

Skip and report "skipped (headless)" when neither `$DISPLAY` nor `$WAYLAND_DISPLAY` is set —
the app opens an SDL3 GPU window.

```bash
timeout 60s ./bin/debug/VoidLight_Template > /tmp/app_log.txt 2>&1; echo $?
```

Exit 124 (timeout) or 0 = pass. Anything else, or segfault / assertion / exception / SDL error
text in the log, = crash: show the last 50 log lines and suggest an ASan build.

## Step 3 — Core suite

```bash
./tests/test_scripts/run_all_tests.sh --core-only --errors-only
```

`--core-only` runs only the `CORE_TEST_SCRIPTS` array; derive the count at runtime rather than
hardcoding it:

```bash
sed -n '/^CORE_TEST_SCRIPTS=(/,/^)/p' tests/test_scripts/run_all_tests.sh
```

Parse pass/fail counts and failed script names (also in
`test_results/combined/all_tests_results.txt`). Typical runtime 2–5 minutes. For each failure,
point to the most targeted repro first:

```bash
./bin/debug/<test_executable> --run_test="FailingCase*"
./tests/test_scripts/run_<system>_tests.sh --verbose
```

Classify failures per `.claude/rules/tests.md` (production bug, test setup, stale expectation,
environment, pre-existing). Never relax expectations to get green.

## Step 4 — Report

Save to `/tmp/voidlight_build_validation_report.md` and print a console summary:

```markdown
# Build Validation Report
**Date:** YYYY-MM-DD HH:MM · **Branch:** <branch>

✓/✗ **Build:** <status> (<n> warnings)
✓/✗/– **Smoke Test:** <status> (<exit reason> | skipped: headless)
✓/✗ **Core Tests:** <passed>/<total> scripts passed

## Build Warnings (max 10)
## Test Failures (script, failing case, brief error)
## Recommendations
**Status:** ✓ PASSED / ✗ FAILED · Total time: <t>
```

Exit codes: 0 all passed, 1 build failed, 2 smoke crash, 3 core tests failed, 4 multiple.
Remind the user the rest of the Branch/PR gate (cppcheck, clang-tidy, ASan, TSan) is not covered.

## Troubleshooting

- **Linker/stale build errors:** `rm build/CMakeCache.txt` and reconfigure (required when
  switching sanitizers or major options).
- **Smoke crash / memory errors:** rebuild with the ASan command in root `CLAUDE.md`.
- **Hangs:** `timeout 120s ./bin/debug/<test>`; check recent ThreadSystem/future changes; use a
  TSan build with `TSAN_OPTIONS="suppressions=$(pwd)/tests/tsan_suppressions.txt"`.
- **High warning count:** check against root `CLAUDE.md` standards; run `voidlight-quality-check`.
