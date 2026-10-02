# Regression Report Template

Loaded when writing the report. Fill every `<...>` from extracted data; drop rows that do
not exist in the current output. Use one table shape for every bench:
`| Row | Baseline | Current | Change | Status |` with Status one of BLOCKING / WARNING /
MINOR / STABLE / IMPROVEMENT / NEW (no baseline). Put units in the row or header.

Save to `test_results/regression_reports/regression_YYYY-MM-DD.md`
(earlier filled examples live in that directory).

```markdown
# VoidLight-Framework Performance Regression Report
**Date:** <YYYY-MM-DD HH:MM>
**Branch / commit:** <branch> @ <short-sha> (<clean | dirty: acknowledged>)
**Build:** <Release | Debug | mixed — list Debug-only scripts>
**Platform:** <SIMD platform>, <hardware threads>, <WorkerBudget workers>
**Baseline:** <date + build from baseline_metadata.txt | created this run>
**Scripts run:** <n>/11 (<missing + blocker>)

## Overall Status: <PASSED | WARNING | FAILED | INCOMPLETE | BASELINE CREATED>
<one-paragraph summary>

## Blocking Regressions
1. **<bench — row>**: <baseline> → <current> (<±x%>). Scope changed? <yes/no + evidence>

## Warnings
## Improvements

## Results by System
### AI Scaling
Entity scaling (updates/sec, threading) and behavior mix; then one table each for
decision pressure, tactical reset, cold burst resolve, cadenced resolve.
WorkerBudget adaptive tuning: batch sizing <PASS>, throughput tracking <PASS>.

### Pathfinding (async)
| Batch | Completed | Completion ms (base → cur) | Change | Throughput (1 sig. fig.) | Status |

### Adaptive Threading
MIN_WORKLOAD <8/8 PASS>; learned thresholds AI/Collision/Particle; hysteresis <PASS>;
batch multipliers <in range>.

### Integrated System
Frame stats (avg/median/P95/P99/drops), scaling summary, max sustainable Active NPCs
within the 10 ms manager budget, coordination overhead (1K / 10K), sustained degradation %.

### Projectile Scaling
Entity scaling (entities/ms, ns/entity), best throughput, SIMD 4-wide curve.

### Collision
MM SAP, dense MM ns/pair, MS spatial hash, combined, density, trigger detection
(method switch at 50 detectors).

### Event Manager
Per-config events/sec, concurrency, batch vs single + batch speedup, threading-threshold
verdicts, combat burst frame totals.

### Particle Manager
Update time by particle count, 1K/5K update, high-count update avg, batch count.

### GPU Frame Timing (environment-sensitive)
Avg frame, swapchain, upload, submit. Session: <desktop | headless>.

### SIMD
| Operation | Platform | SIMD ms | Scalar ms | Speedup | Status |  (state build mode)

### Background Simulation
Scaling table, threshold-detection throughput, adaptive tuning summary.

## Recommendations
- <profiling target / commits to inspect per blocking or warning item>
- <baseline refresh only if asked or after a validated intentional change>
```

## Console Summary

```
=== Performance Regression Check ===
Status: <PASSED | WARNING | FAILED | INCOMPLETE | BASELINE CREATED>
Build: <mode>   Baseline: <date/build>   Scripts: <n>/11
Blocking:     <bench — row: ±x%>
Warnings:     <...>
Improvements: <...>
Report: test_results/regression_reports/regression_<date>.md
```
