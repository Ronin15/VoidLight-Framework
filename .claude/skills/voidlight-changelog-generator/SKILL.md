---
name: voidlight-changelog-generator
description: Writes a branch-scoped engineering changelog for VoidLight-Framework from git history into changelogs/CHANGELOG_<NAME>.md and updates the changelogs/CHANGELOG.md index, optionally with targeted test results and a game-systems-architect review. Use when the user asks to document a branch, prepare release/merge notes, or write a change report.
allowed-tools: [Bash, Read, Write, Edit, Grep, Glob, AskUserQuestion, Agent]
---

# VoidLight-Framework Changelog Generator

Produces an engineering log: what changed, why, and the impact — stated factually. Do not
frame entries as "problem → solution" bug reports. Do not commit anything.

## Step 1 — Ask (one AskUserQuestion call)

1. **Update name** — e.g. "World Update #N", "Performance Optimization", or custom. Becomes
   `CHANGELOG_<UPPER_SNAKE_NAME>.md`.
2. **Primary scope** — AI, collision/pathfinding, rendering/particles, threading/perf, or multiple.
3. **Architect review?** — Yes (launch `game-systems-architect`) / No. Default No.
4. **Testing?** — "Already tested, don't re-run" (default) / "Run targeted tests for changed
   systems". Never default to the core-only suite.

## Step 2 — Analyze the branch

```bash
git rev-parse --abbrev-ref HEAD
git log --oneline main..HEAD
git log --format='%ad' --date=short main..HEAD | sort | sed -n '1p;$p'   # date range
git diff --shortstat main...HEAD
git diff --stat main...HEAD | tail -n 40
git diff --name-status main...HEAD
```

Read diffs for the high-signal files (`git diff main...HEAD -- <path>`) rather than whole
files. Group changes by system (`src/<subsystem>/`, `include/<subsystem>/`, `tests/`, `docs/`,
`res/`). Note per-frame allocation changes, buffer reuse, WorkerBudget/ThreadSystem changes,
event handler ownership, and API/behavior changes — use root `CLAUDE.md` rules as the lens,
don't restate them. Only report performance numbers that were measured or are evident from the
code; otherwise describe the change qualitatively.

## Step 3 — Tests (only if requested)

Per-change / slice-complete style: build and run the executables for the changed systems.

```bash
ninja -C build
./bin/debug/<test_executable>          # e.g. entity_data_manager_tests, collision_system_tests
```

Map changed paths to executables via `tests/CMakeLists.txt` (`ALL_TESTS` + source mapping).
Run `./tests/test_scripts/run_all_tests.sh --core-only --errors-only` only if the user asks for
the Branch/PR gate. Record exact commands and results.

## Step 4 — Architect review (only if requested)

Launch with the `Agent` tool, `subagent_type: game-systems-architect`, prompt along the lines of:

> Review the changes on branch `<branch>` (`git diff main...HEAD`) for architecture coherence,
> performance (per-frame allocations, buffer reuse), thread safety, cross-system integration,
> and code quality against root CLAUDE.md and .claude/rules/. Return grades /10 per category, an
> overall /100, strengths, observations, and recommended actions.

Put the result in the "Architect Review Summary" section. Without a review, write
"Not reviewed" and omit grades.

## Step 5 — Write the changelog

Read `references/changelog-template.md` and write `changelogs/CHANGELOG_<NAME>.md` (repo-root
`changelogs/`, not `docs/`). Include the copyright comment line at the top, as existing
changelogs do.

Writing rules:
- Lead with what changed: "`processBatch()` now takes an out-parameter; per-batch vectors removed."
- Be specific: file paths, function names, line counts, measured numbers.
- Before/after snippets only where they clarify.
- Tables for scale, systems, comparisons, grades.
- Omit empty optional sections instead of filling placeholders.

## Step 6 — Update the index

Add a bullet at the top of "Branch Changelogs" in `changelogs/CHANGELOG.md`, matching the
existing format:

```markdown
- [`CHANGELOG_<NAME>.md`](CHANGELOG_<NAME>.md) — `<branch>` (<start> → <end>): <one-line summary>.
```

## Step 7 — Report

Tell the user the changelog path, grade (if reviewed), key changes, files-changed count, and
which tests (if any) were run. Remind them nothing was committed.
