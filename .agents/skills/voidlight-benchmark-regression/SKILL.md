---
name: voidlight-benchmark-regression
description: Run VoidLight-Framework benchmark regression checks and compare current performance metrics against platform-local baselines. Use when the user asks to run benchmarks, check for performance regressions, verify no performance degradation, refresh or compare baselines, or validate performance-sensitive changes to AI, collision, pathfinding, particles, projectile, threading, GPU frame timing, SIMD, event, or integrated systems.
---

# VoidLight Benchmark Regression

## Purpose

Run the repo's benchmark regression workflow without mixing it into correctness testing or style analysis. Performance numbers are platform-specific, so compare current results against this machine's `test_results/baseline/` data and report percentage deltas instead of treating absolute values as portable truth.

## Workflow

1. Read governing instructions first.
   - Start with the nearest `AGENTS.md` or `AGENTS.override.md` for benchmark scripts, related report files, or touched performance-sensitive code.
   - Treat repo performance, threading, lifecycle, and verification rules as binding.

2. Inspect current state.
   - Run `git status --short` before starting.
   - Do not benchmark unacknowledged dirty changes as if they were a clean baseline.
   - Confirm benchmark scripts exist under `tests/test_scripts/`.
   - Prefer Release builds for performance conclusions. If only Debug is available or practical, say so explicitly.

3. Establish baseline mode.
   - Use `test_results/baseline/` for platform-local comparisons.
   - If no baseline exists, create or report baseline-creation mode instead of claiming pass/fail regression status.
   - Refresh baselines only when the user explicitly asks or after an intentional performance change has been validated.

4. Run benchmarks sequentially.
   - Never run benchmark scripts in parallel; concurrent runs distort timings.
   - Run each command from the repository root and wait for it to complete before starting the next one.
   - If a benchmark times out, crashes, or cannot run, classify the whole pass as incomplete and report the failed script.
   - If a result regresses, verify whether the benchmark's scope changed before calling it an algorithm regression.

5. Extract and compare metrics.
   - Parse current result files under `test_results/`, including the `*_current.txt` files when present.
   - Compare against matching baseline files by percentage change.
   - Treat >15% degradation in a critical system as blocking unless the relevant benchmark documentation defines a different threshold.
   - Distinguish regression, warning, stable result, improvement, and benchmark-scope drift.
   - For AI attack benchmark sections, keep the workload labels intact. Do not summarize the cold burst, cadenced resolve, and decision-only rows as one generic "attack" metric.

6. Produce a compact report.
   - Include the command set, build mode, baseline source, benchmark completion status, critical regressions, warnings, improvements, and report path if one was written.
   - Do not imply complete coverage if any required benchmark was skipped, blocked, or missing metrics.
   - Recommend targeted profiling only for actual regressions.

## Required Benchmark Order

Run all of these for a complete regression pass:

```bash
./tests/test_scripts/run_ai_benchmark.sh
./tests/test_scripts/run_collision_scaling_benchmark.sh
./tests/test_scripts/run_pathfinder_benchmark.sh
./tests/test_scripts/run_event_scaling_benchmark.sh
./tests/test_scripts/run_particle_manager_benchmark.sh
./tests/test_scripts/run_gpu_frame_benchmark.sh
./tests/test_scripts/run_simd_benchmark.sh
./tests/test_scripts/run_integrated_benchmark.sh
./tests/test_scripts/run_background_simulation_manager_benchmark.sh
./tests/test_scripts/run_adaptive_threading_analysis.sh
./tests/test_scripts/run_projectile_benchmark.sh
```

AI, pathfinding, adaptive threading, integrated, and projectile results are mandatory for a complete report. GPU frame timing should be marked environment-sensitive if not run from a normal desktop/rendering session.

## Required Metrics

- AI: entity scaling, updates/sec, threading mode, learned threshold.
  - Attack decision pressure: logic and pressure movement throughput only; damage/projectile resolution is intentionally suppressed.
  - Attack decision tactical reset: pressure scoring and reset movement throughput only; damage/projectile resolution is intentionally suppressed.
  - Attack cold burst resolve: synchronized fresh-state worst-case frame; includes melee EventManager damage and ranged AICommandBus projectile creation before WorkerBudget learning can engage. Treat as a spike/regression signal, not normal steady-state engine cost.
  - Attack cadenced resolve: cooldown-driven combat sequence; includes AI update, ranged command commit, melee EventManager dispatch, and projectile creation. Treat this as the primary ongoing combat throughput signal.
- Collision: movable/movable and movable/static timing, trigger detection counts, trigger method.
- Pathfinding: async throughput, success rate, batch processing performance. Do not substitute deprecated immediate-path timings for the production metric.
- Event: throughput, latency, queue depth, concurrent dispatch/enqueue behavior.
- Particles: update time, particles/frame, batch count, culling or render timing where available.
- GPU frame timing: average frame, swapchain, upload, and submit timing.
- SIMD: platform detection and speedup for AI distance, collision bounds, layer mask, and particle physics where available.
- Integrated: average/P95/P99 frame time, dropped-frame percentage, max sustainable entity count, coordination overhead.
- Background simulation: entity scaling, throughput, threading mode, batch count.
- Adaptive threading: `MIN_WORKLOAD` enforcement, learned thresholds, hysteresis, batch multiplier range.
- Projectile: entities/ms, ns/entity, threading mode, SIMD 4-wide throughput curve.

## Report Validation Checklist

Before giving the final result:

- All 11 benchmark scripts either completed or have an explicit blocker recorded.
- AI scaling metrics are present.
- AI Attack workload sections are present and interpreted separately: decision pressure, decision tactical reset, cold burst resolve, and cadenced resolve.
- Pathfinding async throughput metrics are present.
- Projectile scaling and SIMD throughput metrics are present.
- Integrated frame statistics are present.
- Adaptive threading `MIN_WORKLOAD`, threshold, hysteresis, and batch multiplier results are present.
- Baseline files used for comparison are identified.
- Any missing baseline is reported as baseline-creation mode, not a clean pass.
- Any skipped benchmark is called out as incomplete coverage.

## Output Expectations

- Keep the user-facing result short unless regressions exist.
- Put blocking regressions first, then warnings, then improvements.
- Include exact scripts run and the build configuration used.
- If benchmark output indicates a real regression, trace the likely subsystem before recommending changes.
- Do not update production code during a regression pass unless the user explicitly asks to fix a confirmed regression.
