---
name: voidlight-quality-check
description: Runs VoidLight-Framework quality checks - focused cppcheck and clang-tidy plus grep-based standards, threading, and architecture checks against the CLAUDE.md rules. Use when the user asks for a quality, standards, architecture, or cppcheck/clang-tidy pass, or when a branch is ready for merge/PR. Branch/PR gate - not for per-change or slice-complete checks.
allowed-tools: [Bash, Read, Grep]
---

# VoidLight-Framework Code Quality Check

Enforces the rules in root `CLAUDE.md` plus the `.claude/rules/` file for each touched path (see the "Path Rules" table in `CLAUDE.md`). On conflict, those rules win over this skill.

Load references only when needed:

- **`references/checks.md`** — read before running sections 1–2 and 4–7 (commands, per-check severity, report format, exit codes).
- **`references/standards.md`** — read for section 3 (naming/formatting/API greps) or when writing fixes (Quick Fix Guide).

## Gate Positioning

Gates are defined in `docs/framework-implementation-slices.md` — do not mix them:

| Gate | What runs | This skill |
|------|-----------|------------|
| Per-change | targeted `ninja -C build` + named Boost.Test executable | Not this skill (grep checks on the touched files are fine if asked) |
| Slice complete | `ninja -C build` + Boost.Test executables for the slice | Not this skill |
| Slice review | review specialist on the slice diff | grep/architecture checks may support the review |
| **Branch / PR** | `run_all_tests.sh --core-only --errors-only`, cppcheck, clang-tidy, ASan, TSan | **cppcheck + clang-tidy live here** |

Do not add builds, the core-only suite, sanitizers, or full (unfocused) analyzer passes unless the user asks. If the user names a file, stay there unless a finding requires tracing a dependency.

## When to Use

"check code quality", "run the quality gate", "cppcheck/clang-tidy pass", "check standards/architecture/threading", or the branch is ready for PR. Also useful as a targeted grep pass while reviewing a diff.

## Check Categories

Each item is detailed in `references/checks.md` (section 3 in `references/standards.md`).

1. **Compilation Quality** — zero-warning policy (uses an existing build; only build if asked).
2. **Static Analysis (Branch/PR gate)** — 2.1 focused cppcheck, 2.2 focused clang-tidy.
3. **Coding Standards** — naming, 4-space Allman, C++ API rules (no raw-pointer ownership / nullable raw pointers, no C-strings / raw arrays, unused params unnamed). See `references/standards.md`.
4. **Threading Safety (CRITICAL)** — 4.1 no non-`thread_local` static state in threaded code, 4.2 `ThreadSystem` + `WorkerBudget` only (no raw threads / private pools / thread-count heuristics), 4.3 synchronization matches actual thread ownership.
5. **Architecture Compliance (5.1–5.20)**
   - 5.1 GPU frame lifecycle — one present per frame; no clear/end/submit/present from GameStates.
   - 5.2 RAII & ownership — no raw `new`/`delete`, no raw-pointer ownership.
   - 5.3 Smart-pointer performance — no `shared_ptr` copies/captures in hot paths.
   - 5.4 String parameters — no `string_view`→`string` churn for map lookups.
   - 5.5 Logger usage — `std::format`, `*_IF` macros, `VOIDLIGHT_DEBUG_ONLY` (never raw `#ifdef DEBUG`).
   - 5.6 Buffer reuse — no per-frame allocations; `reserve()` when size known.
   - 5.7 UI positioning — `setComponentPositioning()` after create; controllers use `UIManager` sizing APIs.
   - 5.8 Rendering/transition rules — deferred transitions; `LoadingState` for async loads.
   - 5.9 Singleton access — no cached manager `mp_*` members; local references.
   - 5.10 Controller access — `m_controllers.add<T>()` in `enter()`, no cached `mp_*Ctrl`.
   - 5.11 Behavior per-entity state — lives in EDM behavior state, not behavior-file globals.
   - 5.12 Controller→AI boundary — behavior messages, not direct EDM behavior mutation.
   - 5.13 State-transition completeness — 11-manager order, both exit paths, `ControllerRegistry::clear()`.
   - 5.14 Thread-local capacity — `clear()`, never `swap`/return-by-value.
   - 5.15 World-lifecycle cleanup — world caches cleared by transition cleanup or unload.
   - 5.16 Second source of truth (WARNING).
   - 5.17 Render-controller lifecycle (WARNING).
   - 5.18 Event-contract bypass (WARNING).
   - 5.19 EDM policy creep (WARNING).
   - 5.20 Event handler & collision-callback ownership — persistent vs transient handlers.
6. **Copyright** — project MIT header on every source file.
7. **Test Coverage** — behavior changes ship with focused Boost.Test coverage.

## Core Workflow

All commands run from the repo root.

1. `git status --short` so pre-existing dirty files are not mistaken for new findings.
2. **Warnings** (existing build log or an incremental build if asked): `ninja -C build 2>&1 | grep -E "warning|error" | head -n 100`
3. **Static analysis (Branch/PR gate)** — read `tests/cppcheck/README.md` and `tests/clang-tidy/README.md`, then run only the focused scripts:
   ```bash
   tests/cppcheck/cppcheck_focused.sh
   tests/clang-tidy/clang_tidy_focused.sh
   ```
   Focused cppcheck uses `tests/cppcheck/cppcheck_lib.cfg` + `tests/cppcheck/cppcheck_suppressions.txt` — do not substitute the full `run_cppcheck.sh` / `run_clang_tidy.sh` wrappers or a raw `cppcheck src/ include/` invocation. clang-tidy needs `compile_commands.json` (from `cmake -B build/ ...`) and uses `tests/clang-tidy/.clang-tidy` + `clang_tidy_suppressions.txt`. If a tool is missing or the compile DB is stale, report the blocker instead of improvising.
4. **Standards / threading / architecture greps** — sections 3–5 of the references (~5–10s). Grep hits are candidates, not verdicts: open the code before reporting. Use runtime discovery (states under `src/gameStates/`, behaviors under `src/ai/behaviors/`); do not hardcode names.
5. **Copyright + test coverage** — sections 6–7 of `references/checks.md`.
6. **Triage** — classify each finding as real issue, false positive, tooling, or pre-existing. Check `docs/review-non-issues.md` before raising anything; do not re-flag adjudicated items. Never hide a production failure in tests.
7. **Report** — Quality Report Format in `references/checks.md`, severity BLOCKING / WARNING / INFO. If fixes are requested: minimal scoped fixes, production + tests together when behavior changes, then re-run the analyzer that reported the issue.

## Quality Gates (summary)

- **BLOCKING:** non-`thread_local` static state in threaded code; raw threads / bypassing `WorkerBudget`; behavior per-entity state outside EDM; `shared_ptr` copies in hot paths; per-frame allocations; cached manager/controller `mp_*` members in GameStates; clear/end/submit/present from GameStates; controller→AI direct mutation; missing managers in the 11-manager transition order (both exit paths); thread-local `swap`/return-by-value; world caches surviving transition/unload; raw-pointer ownership; compile errors; critical cppcheck/clang-tidy; missing copyright headers.
- **WARNING:** second source of truth; render-controller teardown; event-contract bypass; EDM policy creep; handler registered in the wrong lifetime; compile warnings; naming; missing tests; log string concat; missing `*_IF`; missing UI positioning; missing `reserve()`; nullable raw-pointer params/returns; C-string / raw-array APIs.
- **INFO:** style, perf hints, organization.

## Performance Expectations

grep checks ~5–10s · cppcheck focused ~30–60s · clang-tidy focused several minutes (scales with `src/` size).
