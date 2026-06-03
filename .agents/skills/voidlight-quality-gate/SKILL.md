---
name: voidlight-quality-gate
description: Run a VoidLight repository quality gate against local project instructions, architecture and coding standards, tests, cppcheck guidance, and clang-tidy guidance. Use when the user asks to run a quality check, quality pass, standards check, architecture check, cppcheck/clang-tidy pass, or to investigate and fix findings after checking this C++ repository.
---

# VoidLight Quality Gate

## Workflow

Run a focused repository quality pass around the repo's analyzer wrappers. Treat repository instructions as the source of truth, then run only the focused cppcheck and clang-tidy scripts documented by the repo.

1. Read applicable repository guidance before running checks:
   - Start with the root `AGENTS.md`; Codex launches from the project root and the root file is the guaranteed entrypoint.
   - Then read any nested `AGENTS.md` or `AGENTS.override.md` that applies to touched paths.
   - For this repository, the standards in root and nested `AGENTS.md` files are part of the quality gate.

2. Read tool guidance before invoking static analyzers:
   - Read `tests/cppcheck/README.md` before running cppcheck.
   - Read `tests/clang-tidy/README.md` before running clang-tidy.
   - Follow the documented focused commands, compile database requirements, current suppression/library-config behavior, and output expectations from those files.

3. Build a short execution plan:
   - Include architecture and coding standards checks from `AGENTS.md`.
   - Run only the focused analyzer wrappers documented by the repo: `tests/cppcheck/cppcheck_focused.sh` and `tests/clang-tidy/clang_tidy_focused.sh`.
   - Do not add build steps, broad test runs, or full analyzer passes unless the user explicitly asks for them.
   - If the user named a specific file, keep the standards review scoped there unless the analyzer finding requires tracing a dependency.

4. Run checks and capture findings:
   - Inspect `git status --short` first so pre-existing user changes are not mistaken for your edits.
   - Run `tests/cppcheck/cppcheck_focused.sh`.
   - Run `tests/clang-tidy/clang_tidy_focused.sh`.
   - Focused `cppcheck` uses `tests/cppcheck/cppcheck_lib.cfg` and `tests/cppcheck/cppcheck_suppressions.txt`; do not substitute the full cppcheck wrappers for the focused quality gate.
   - `clang-tidy` requires `compile_commands.json` and applies `tests/clang-tidy/clang_tidy_suppressions.txt` only if that file exists.
   - If either command fails because of missing dependencies, stale build configuration, missing compile database, or sandbox restrictions, report the exact blocker and either fix the local setup when appropriate or request the needed approval.

5. Investigate before fixing:
   - For every warning, failure, or standards concern, trace the relevant production code and nearby subsystem patterns first.
   - Classify each item as a real issue, false positive, tooling/configuration issue, or pre-existing unrelated issue.
   - Do not change tests to hide production failures.
   - Do not apply broad refactors or speculative architecture changes. Fix root causes with minimal scoped edits.

6. Fix and verify:
   - Apply production and test changes together when behavior changes.
   - Re-run whichever focused analyzer reported the issue.
   - State exactly what passed, what was not run, and any residual risk.

## Review Focus

Check for violations of local standards, especially:

- Architecture boundaries and subsystem ownership described in `AGENTS.md`.
- Threading rules, manager lifecycle rules, rendering one-present-per-frame rules, and UI/GameState patterns.
- C++ style rules such as C++20, Allman braces, const/reference conventions, `std::format()` logging, `VOIDLIGHT_DEBUG_ONLY(...)`, and unused-parameter handling.
- C++ API rules such as no new raw-pointer ownership, nullable raw-pointer parameters/returns, raw-pointer optional out-parameters, `const char*` constants, C-string APIs, or C-style code except at unavoidable SDL/C API boundaries.
- Performance rules such as avoiding per-frame allocations, preserving reusable buffer capacity, and using existing threading/budget helpers.
- Test alignment with production fixes and Boost.Test naming details from repo guidance.

Keep the final response concise: summarize findings investigated, files changed, commands run, and any checks skipped or blocked.
