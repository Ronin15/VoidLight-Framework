---
name: voidlight-benchmark-regression
description: >-
  Run VoidLight benchmark regression vs platform-local baselines. Use when the
  user asks to run benches, check performance regressions, refresh or compare
  baselines, or validate performance-sensitive AI, collision, pathfinding,
  particle, projectile, threading, GPU, SIMD, event, or integrated changes.
  Not a per-change or slice-complete gate.
---

# VoidLight Benchmark Regression

Not mixed into correctness tests or slice-complete. Numbers are
machine-local: compare against `test_results/baseline/` as percentage
deltas, not portable absolutes.

## Run

1. `git status --short`. Do not treat unacknowledged dirty trees as a
   clean baseline.
2. Prefer Release. If only Debug is practical, say so.
3. Use `test_results/baseline/`. No baseline → baseline-creation mode, not
   pass/fail. Refresh baselines only when asked or after a validated
   intentional perf change.
4. Run scripts **sequentially** from the repo root. Parallel runs distort
   timings. A timeout/crash makes the whole pass incomplete.

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

AI, pathfinding, adaptive threading, integrated, and projectile results
are required for a complete report. Mark GPU frame timing
environment-sensitive if not run on a normal desktop session.

5. Compare `test_results/` (including `*_current.txt`) to matching
   baselines. >15% degradation on a critical system is blocking unless
   that bench's docs say otherwise. If a result regresses, check whether
   the bench **scope** changed before calling it an algorithm regression.

Keep AI attack rows separate — do not fold cold burst, cadenced resolve,
and decision-only into one “attack” metric.

## Metrics

- **AI:** entity scaling, updates/sec, threading mode, learned threshold.
  Decision pressure / tactical reset: logic and movement only (damage and
  projectiles suppressed). Cold burst: synchronized fresh-state spike
  (melee EventManager damage + ranged AICommandBus projectiles before
  WorkerBudget learning). Cadenced resolve: primary ongoing combat
  throughput (AI update, ranged commit, melee dispatch, projectile
  create).
- **Collision:** movable/movable and movable/static timing, trigger
  counts and method.
- **Pathfinding:** async throughput, success rate, batching — not
  deprecated immediate-path timings.
- **Event:** throughput, latency, queue depth, concurrent
  dispatch/enqueue.
- **Particles:** update time, particles/frame, batch count, culling or
  render timing if present.
- **GPU:** average frame, swapchain, upload, submit.
- **SIMD:** platform and speedup for AI distance, collision bounds, layer
  mask, particle physics.
- **Integrated:** avg/P95/P99 frame, dropped-frame %, max sustainable
  entities, coordination overhead.
- **Background sim:** scaling, throughput, threading, batch count.
- **Adaptive threading:** `MIN_WORKLOAD`, learned thresholds, hysteresis,
  batch multiplier range.
- **Projectile:** entities/ms, ns/entity, threading mode, SIMD 4-wide
  curve.

## Report

Blocking regressions first, then warnings, then improvements. Include
scripts run, build mode, and baseline source. Incomplete if any of the 11
scripts is missing without an explicit blocker, or if required metrics
above are absent.

Do not edit production code on a regression pass unless the user asks to
fix a confirmed regression.
