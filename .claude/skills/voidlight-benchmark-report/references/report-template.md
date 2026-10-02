# Performance Report Template

Loaded at the assembly step. Fill `<...>` only with parsed values; drop any section or
row without data. Earlier filled reports live in `docs/performance_reports/`
(`latest_report.md` points at the newest).

Per-system table shape:
`| Row | Current | Baseline | Change | Trend |` (Trend: Improving / Stable / Degrading / New).

```markdown
# VoidLight-Framework Performance Report

**Generated:** <YYYY-MM-DD>
**Branch / commit:** <branch> @ <short-sha>
**Build:** <Release | Debug | mixed — per system>
**Platform:** <OS>, <CPU>, <hardware threads>, <WorkerBudget workers>, <SIMD platform>
**Baseline:** <date + build from test_results/baseline/baseline_metadata.txt | none>
**Data freshness:** <oldest and newest result file dates; note any stale systems>

## Executive Summary
| System | Headline metric | Change vs baseline | Status |
|--------|-----------------|--------------------|--------|
| AI | <peak updates/sec @ N entities> | | |
| Collision | <MM ms @ 10K movables> | | |
| Pathfinding | <async completion ms @ batch 500, N/N completed> | | |
| Event | <batch enqueue events/sec; batch speedup> | | |
| Particles | <high-count update avg ms> | | |
| Projectile | <best entities/ms> | | |
| Integrated | <avg frame ms; max sustainable Active NPCs> | | |
| Background sim | <ms @ 10K> | | |
| Adaptive threading | <MIN_WORKLOAD 8/8, validations> | | |
| SIMD | <speedups, build mode> | | |
| GPU | <upload/submit ms> | | |

<2-4 sentences: what changed, what to watch, build-mode caveats>

## Detailed Results
### AI Scaling
Entity scaling; behavior mix; attack workloads as four separate tables (decision
pressure, tactical reset, cold burst resolve, cadenced resolve); WorkerBudget tuning.
### Collision
MM SAP, dense MM, MS spatial hash, combined, density, trigger detection.
### Pathfinding (async)
Per batch size: completed, completion ms, throughput (1 sig. fig.); cache analysis
(informational).
### Event Manager
Scalability configs, concurrency, batch vs single, threading threshold, combat burst.
### Particle Manager / Projectile / Background Simulation / Adaptive Threading
### Integrated System
Frame stats, scaling under load vs 10 ms manager budget, coordination overhead,
sustained degradation.
### SIMD / GPU Frame Timing (mark environment-sensitive)

## Trends (if ≥2 comparable runs)
| Metric | <run date 1> | <run date 2> | ... | Trend |
Note any scope or build-mode change that breaks comparability.

## Hotspots (if callgrind data exists)
| Executable | Function | Ir % |

## Findings & Recommendations
- <degrading items with evidence and a profiling next step>
- <baseline refresh suggestion only for validated intentional changes>

## Method
Scripts/executables and flags, sequential execution, files parsed, statistics used.
```

## Console Summary

```
=== VoidLight-Framework Benchmark Report ===
Systems reported: <n> (missing: <list>)
Build: <mode>   Baseline: <date/build | none>
Degrading: <...>   Improving: <...>
Report: docs/performance_reports/performance_report_<date>.md
```
