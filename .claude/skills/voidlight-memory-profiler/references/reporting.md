# Memory Profiler — Report & Console Summary

Read this only at the reporting step. Include only the sections for the mode
that ran. Save to `test_results/memory_profiles/memory_profile_$(date +%Y-%m-%d_%H-%M-%S).md`.

## Report skeleton

```markdown
# VoidLight-Framework Memory Profile — YYYY-MM-DD

**Branch/Commit:** <branch> @ <short-sha> · **Mode:** <mode> · **Scope:** <scope>
**Status:** CLEAN / WARNINGS / CRITICAL

## Summary
- 3-5 bullet findings, most severe first.

## Leak / error results (Modes 1, 2, 2b)
| Test | Definite | Indirect | Invalid R/W | Races/UAF | Exit | Status |
|------|----------|----------|-------------|-----------|------|--------|

For each CRITICAL row: file:line, trimmed stack (top project frames only),
likely cause, concrete fix.

## Peak heap (Mode 3)
Paste `parse_massif.py` top consumers + category table. Name the top 3
allocation sites from `ms_print` (project frames only).

## Buffer reuse audit (Mode 4)
| File:line | Pattern | Hot path? | Fix |
|-----------|---------|-----------|-----|
Only list findings confirmed by reading the code (per-frame path traced).

## Baseline delta (if requested)
| Test | Baseline | Current | Delta | Verdict |

## Action items
- Critical (fix before merge) / Important / Optional — each with file:line.
```

## Console summary

```
=== Memory Profile: <mode> / <scope> ===
Status: CLEAN | WARNINGS | CRITICAL      Duration: <t>
Critical: N   Warnings: N
<one line per critical/warning issue: test or file:line — issue>
Peak heap (Mode 3): <top test> <X MB>
Baseline: <leaks +/-, peak +/-, trend>   (if requested)
Report: test_results/memory_profiles/memory_profile_<stamp>.md
```

## Severity

- **CRITICAL (block merge):** definite/indirect leaks, invalid read/write,
  use-after-free, double free, heap/stack overflow, data race, lock-order
  inversion, test exiting non-zero under the tool.
- **WARNING (review):** possible leaks (often SDL/font/driver noise — check
  suppressions first), uninitialized-value use, TSan thread leaks, confirmed
  per-frame allocation in a hot path.
- **INFO:** still-reachable blocks from static/SDL globals.
