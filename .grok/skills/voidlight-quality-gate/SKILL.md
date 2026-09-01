---
name: voidlight-quality-gate
description: >-
  Run a VoidLight focused cppcheck + clang-tidy quality pass. Use when the user
  asks for a quality check, standards check, architecture check, or
  cppcheck/clang-tidy pass. Branch/PR gate — not per-change or slice-complete.
---

# VoidLight Quality Gate

Branch/PR static analysis only. Do not mix into per-change or slice-complete
unless the user asks. Rules: root and nested `AGENTS.md`. Tooling:
`tests/cppcheck/README.md`, `tests/clang-tidy/README.md`.

## Run

1. `git status --short` so pre-existing dirty files are not mistaken for
   your edits.
2. Read the READMEs above. Run only:

```bash
tests/cppcheck/cppcheck_focused.sh
tests/clang-tidy/clang_tidy_focused.sh
```

   Focused cppcheck uses `tests/cppcheck/cppcheck_lib.cfg` and
   `tests/cppcheck/cppcheck_suppressions.txt` — do not substitute the full
   wrappers. clang-tidy needs `compile_commands.json` and applies
   `tests/clang-tidy/clang_tidy_suppressions.txt` if present.
3. Do not add builds, the core-only suite, sanitizers, or full analyzer
   passes unless asked. If the user named a file, stay there unless a
   finding requires tracing a dependency.
4. If a command fails (missing deps, stale cache, no compile DB, sandbox),
   report the blocker.

## Investigate, then fix

Trace production code and nearby patterns. Classify: real issue, false
positive, tooling, or pre-existing. Do not hide production failures in
tests. Minimal scoped fixes; production + tests together when behavior
changes. Re-run the analyzer that reported the issue.

Check `AGENTS.md` ownership, threading, one-present-per-frame, C++20 API
rules, buffer reuse, and test alignment.

Report: findings, files changed, commands run, checks skipped or blocked.
