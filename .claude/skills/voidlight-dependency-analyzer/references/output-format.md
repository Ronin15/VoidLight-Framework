# Report, Console Summary & Scoring

Read this at Step 5/6 or when explaining the score. Build the report from the
files in `test_results/dependency_analysis/`; don't re-derive numbers by hand.
Save to `docs/architecture/dependency_analysis_YYYY-MM-DD.md` (prior reports
there are the baseline for the comparison section).

## Report skeleton

```markdown
# VoidLight-Framework Dependency Analysis — YYYY-MM-DD

**Branch/Commit:** <branch> @ <short-sha> · **Mode:** <mode>
**Health:** <score>/100 (<grade>) · **Status:** HEALTHY / NEEDS ATTENTION / CRITICAL

## Summary
3-5 bullets, most severe first, plus a 2-sentence assessment.

## Statistics
Headers analyzed, total edges, files with deps, per-layer header counts
(from detect_layer_violations output).

## Circular dependencies
None, or each cycle path + fix (forward declaration first; see
architecture-model.md "Circular Dependency Fix Patterns").

## Layer violations
One line per layer (CLEAN / N violations). For each violation: file, include,
layer rule broken, whether it is real or should become an approved exception
(and why), fix.

## Coupling
Top fan-out / fan-in / instability rows; manager matrix; functional pairs
(one line) vs problematic pairs (each with a verdict after reading the .cpp).

## Header bloat & depth
High-include headers, ripple headers, top forward-declaration opportunities
(flag any needing an out-of-line destructor), max/avg depth.

## Scorecard
| Category | Score | Weight | Weighted |  (from health_scorecard.txt)

## Comparison with previous report (if one exists)
| Metric | Previous | Current | Delta |  (edges, cycles, violations, problematic coupling, score)

## Action items
Critical (cycles, real layer violations) / Important (problematic coupling,
bloat on hot headers) / Optional (forward declarations) — each with file.
```

## Console summary

```
=== Dependency Analysis: <mode> ===
Headers: N   Edges: N   Health: <score>/100 (<grade>)
Cycles: N   Layer violations: N   Problematic coupling: N   High-bloat headers: N
<one line per critical/important finding>
Report: docs/architecture/dependency_analysis_YYYY-MM-DD.md
```

## Scoring (as implemented in `calc_health_score.py`)

Weighted categories, each scored 0-10, total ×10 → /100:

| Category | Weight | Scoring |
|----------|--------|---------|
| Circular deps | 30% | 0 → 10; ≤2 → 5; else 0 |
| Layer violations | 25% | 0 → 10; ≤3 → 6; ≤10 → 3; else 0 |
| Coupling | 20% | problematic pairs only: 0 → 10; ≤2 → 8; ≤5 → 5; ≤10 → 3; else 1 |
| Header bloat | 15% | high-bloat / graph nodes: <5% → 10; <10% → 8; <15% → 6; <25% → 4; else 2 |
| Depth | 10% | max depth ≤4 → 10; ≤7 → 8; ≤10 → 6; else 4 |

Grades: ≥90 A+, ≥80 A, ≥70 B, ≥60 C, else F. Functional (allowlisted)
manager coupling never lowers the score — don't recommend refactoring it.
