---
name: voidlight-quality-check
description: Runs comprehensive code quality checks for SDL3 VoidLight-Framework including compilation warnings, static analysis (cppcheck, clang-tidy), coding standards validation, threading safety verification, and architecture compliance. Use before commits, pull requests, or when the user wants to verify code meets project quality standards.
allowed-tools: [Bash, Read, Grep]
---

# VoidLight-Framework Code Quality Gate

Enforces SDL3 VoidLight-Framework quality standards from `CLAUDE.md`. Catches issues before they reach version control: compilation warnings, static analysis, coding standards, threading safety, and architecture compliance.

This SKILL.md is the lean playbook. The detailed material is loaded on demand from `references/`:

- **`references/checks.md`** — the full numbered check catalog (detection commands, forbidden-pattern examples, quality gates), the Quality Report Format, exit codes, severity classification, and the git-hook appendix.
- **`references/standards.md`** — coding-standard rules mirrored from CLAUDE.md (naming, formatting) plus the Quick Fix Guide for the 20 most common violations.

## When to Use

Activate automatically when the user says things like: "check code quality", "run quality gate", "verify my code before commit", "make sure code follows standards", or "check for threading violations". Also use before every commit, during PR review, after merging, and when adding new systems.

## Check Categories

Run in order. Each item is detailed in `references/checks.md` (section 3 in `references/standards.md`).

1. **Compilation Quality** — zero-warning policy; build and grep for warning/unused/error.
2. **Static Analysis**
   - 2.1 **cppcheck** — memory leaks, null derefs, buffer overflows, uninitialized vars.
   - 2.2 **clang-tidy** — bug/modernize/performance checks; gated on availability (optional tool).
3. **Coding Standards** — naming (UpperCamelCase / lowerCamelCase / `m_` / `mp_` / ALL_CAPS), 4-space Allman formatting. See `references/standards.md`.
4. **Threading Safety (CRITICAL)** — no static vars in threaded code (4.1), no raw `std::thread` (4.2), mutex protection on managers (4.3).
5. **Architecture Compliance (5.1–5.19)**
   - 5.1 GPU frame-lifecycle ownership — no `endFrame`/`present`/submit from GameStates.
   - 5.2 RAII & smart pointers — minimal raw `new`/`delete`.
   - 5.3 Smart-pointer performance — no `shared_ptr` copies/captures in hot paths.
   - 5.4 String parameters — no `string_view`→`string` conversion for map lookups.
   - 5.5 Logger usage — `std::format`, `*_IF` macros, `VOIDLIGHT_DEBUG_ONLY` (never raw `#ifdef DEBUG`).
   - 5.6 Buffer reuse — no per-frame allocations; `reserve()` when size known.
   - 5.7 UI positioning — `setComponentPositioning()` after every create.
   - 5.8 Rendering rules — deferred transitions; `LoadingState` for async loads.
   - 5.9 Singleton access — no cached `mp_*` pointers; cache locals when multi-use.
   - 5.10 Controller access — no cached `mp_*Ctrl`; cache when multi-use.
   - 5.11 Behavior entity state — per-entity state in EDM, not behavior members.
   - 5.12 Controller→AI boundary — behavior messages, not direct EDM mutation.
   - 5.13 State-transition completeness — all 11 managers in correct order, both exit paths.
   - 5.14 Thread-local capacity — `clear()`, never `swap`/return-by-value.
   - 5.15 World-lifecycle cleanup — world-scoped caches cleared on unload.
   - 5.16 Second source of truth (WARNING) — cross-frame per-entity state belongs in EDM.
   - 5.17 Render-controller lifecycle (WARNING) — render controllers don't own teardown.
   - 5.18 Event-contract bypass (WARNING) — new writers fire established events.
   - 5.19 EDM policy creep (WARNING) — EDM is state, not policy/thresholds.
6. **Copyright & Legal** — MIT header on every source file.
7. **Test Coverage** — new managers have a test file + script wired into `run_all_tests.sh`.

For the full check catalog with commands and fixes, read `references/checks.md`. For coding-standard rules and the Quick Fix Guide, read `references/standards.md`.

## Core Workflow

All commands run from `$PROJECT_ROOT/`.

1. **Build and scan warnings:**
   ```bash
   ninja -C build -v 2>&1 | grep -E "(warning|unused|error)" | head -n 100
   ```
2. **cppcheck:**
   ```bash
   ./tests/cppcheck/cppcheck_focused.sh \
     || cppcheck --enable=all --suppress=missingIncludeSystem --std=c++20 --quiet src/ include/ 2>&1
   ```
3. **clang-tidy (gated — optional tool, may not be installed):**
   ```bash
   command -v clang-tidy >/dev/null 2>&1 && ./tests/clang-tidy/clang_tidy_focused.sh \
     || echo "clang-tidy not installed — skipping (cppcheck still covers static analysis)"
   ```
4. **Standards + threading + architecture greps:** run the per-check detection commands from `references/checks.md` (sections 3–5) and `references/standards.md` (section 3). These are fast (~5–10s total). Each section gives its detection command, forbidden patterns, and quality gate. Several use runtime discovery (e.g. grep behavior headers under `include/ai/`, AI-enabled states under `src/gameStates/`) — run those greps, do not hardcode names.
5. **Copyright + test coverage:** sections 6–7 of `references/checks.md`.
6. **Report:** produce the report using the Quality Report Format in `references/checks.md`, classify findings by severity (BLOCKING / WARNING / INFO), and set the exit code from the Exit Codes table.

## Quality Gates (summary)

- **BLOCKING (must fix):** static vars in threaded code; per-entity state in behavior members; `shared_ptr` copies in hot paths; per-frame allocations; duplicate `Instance()` / cached `mp_*` in GameStates; frame end/present/submit from GameStates; controller→AI direct mutation; missing managers in the 11-manager transition order (both exit paths); thread-local `swap`/return-by-value; world caches surviving unload; compile errors; critical cppcheck/clang-tidy; missing copyright headers.
- **WARNING (should fix):** second source of truth; render-controller teardown; event-contract bypass; EDM policy creep; compile warnings; naming violations; missing tests; string concat in logs; missing `*_IF` macros; missing UI positioning; missing `reserve()`.
- **INFO:** style, perf hints, organization.

Full severity classification: `references/checks.md`. Per-violation Quick Fix Guide: `references/standards.md`.

## Performance Expectations

Compilation ~10–30s · cppcheck ~30–60s · clang-tidy ~60–120s · grep checks ~5–10s · total ~2–4 min.
