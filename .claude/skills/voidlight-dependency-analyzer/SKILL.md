---
name: voidlight-dependency-analyzer
description: Script-driven include-dependency audit for SDL3 VoidLight-Framework - circular includes, layer violations (Core→Managers→GameStates→Entities/Controllers plus cross-cutting dirs), manager coupling, header bloat, include depth, and a health score/report. Use after major refactors, when adding a manager or cross-layer include, before releases, or when investigating circular-include errors or compile times.
allowed-tools: [Bash, Read, Write, Grep, Glob, AskUserQuestion]
---

# VoidLight-Framework Dependency Analyzer

Script-driven. The analysis lives in `scripts/`; this file is the playbook.
Layer rules, allowlists and exceptions are in `references/architecture-model.md`
(canonical direction: `docs/ARCHITECTURE.md` —
`Core → Managers → GameStates → Entities/Controllers`, with cross-cutting
`utils, events, ai, collisions, world, gpu`). Scripts discover headers and
directories from `include/` + `src/` on every run.

## Scripts

Run from the repo root (they read `include/`, `src/` and write
`test_results/dependency_analysis/`, relative to the cwd).

```bash
S=.claude/skills/voidlight-dependency-analyzer/scripts
GRAPH=test_results/dependency_analysis/dependency_graph.txt
python3 $S/extract_deps.py                 # always first: builds $GRAPH
python3 $S/detect_cycles.py $GRAPH         # exit 1 if cycles
python3 $S/detect_layer_violations.py
python3 $S/analyze_coupling.py $GRAPH .
python3 $S/analyze_header_bloat.py
python3 $S/calc_depth.py $GRAPH
python3 $S/generate_trees.py $GRAPH        # ASCII trees for key managers
python3 $S/calc_health_score.py            # last: reads all outputs above
```
All run in seconds. Output files, thresholds and algorithms:
`references/scripts.md` (read when interpreting a number or a script's output).

## Modes

| Mode | Scripts | Use when |
|------|---------|----------|
| Quick cycle check | extract → cycles | circular-include compile errors, header edits |
| Coupling | extract → coupling | adding/refactoring a manager |
| Full audit | all, then report | after major refactors, before releases |
| One component | extract → cycles, then grep the component's includes/includers | targeted investigation |

## Workflow

1. **Scope.** Pick the mode from the request; ask (AskUserQuestion) only if
   unclear, and whether a Markdown report is wanted.
2. **Run** the mode's scripts.
3. **Judge findings.** Read `references/architecture-model.md` before calling
   anything a violation. For every flagged layer violation or problematic
   coupling pair, open the including header / `.cpp` and decide: real
   violation, or a lightweight/approved case to add to the script's
   `LIGHTWEIGHT_CROSS_CUTTING_HEADERS`, `APPROVED_EXCEPTIONS`, or
   `functional_deps` (with a comment citing the doc that justifies it). Keep
   the reference file in sync when you change those sets.
4. **Report.** For a full audit or when asked, read
   `references/output-format.md` and write
   `docs/architecture/dependency_analysis_YYYY-MM-DD.md`; otherwise print the
   console summary only.

## Reference files

- `references/architecture-model.md` — read at step 3: layer table, approved
  exceptions, lightweight headers, functional-coupling allowlist, fix patterns.
- `references/scripts.md` — per-script outputs, thresholds, exit codes.
- `references/output-format.md` — read at step 4: report skeleton, console
  summary, scoring formula.
