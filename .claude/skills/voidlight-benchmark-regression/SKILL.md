---
name: voidlight-benchmark-regression
description: Runs the VoidLight-Framework benchmark scripts sequentially and compares results to platform-local baselines in test_results/baseline/ as percentage deltas. Use when the user asks to run benches, check for performance regressions, or refresh/compare baselines, or before merging performance-sensitive AI, collision, pathfinding, event, particle, projectile, threading, GPU, SIMD, or integrated changes. Not a per-change or slice-complete gate.
allowed-tools: [Bash, Read, Write, Grep]
---

# VoidLight-Framework Benchmark Regression

Benches are **not** mixed into correctness tests and are **not** a per-change,
slice-complete, or default Branch/PR gate (see the gate table in
`docs/framework-implementation-slices.md`). Run them when the user asks or when a
change is performance-sensitive. Numbers are machine-local: compare against
`test_results/baseline/` as percentage deltas, never as portable absolutes.
Repo rules live in root `CLAUDE.md` plus `.claude/rules/`.

Read references only at the step that needs them:
- **`references/benchmarks.md`** — before step 5 (or earlier if a script's build flag or
  output file is unclear): script → executable → output file table, baseline layout,
  grep commands per bench, per-bench detection notes.
- **`references/report-template.md`** — when writing the report.
- **`references/troubleshooting.md`** — on a timeout/crash, noisy results, or before
  handing over the report (completeness checklist).

## Run

1. `git status --short`. Do not treat an unacknowledged dirty tree as a clean baseline.
2. Prefer Release. If only Debug is practical, say so. Several scripts are Debug-only
   (see the build-mode column in `references/benchmarks.md`); record the build mode per
   script and never compare a Release result against a Debug baseline.
3. Use `test_results/baseline/` (read `baseline_metadata.txt` for build mode, platform,
   and dates). No matching baseline → **baseline-creation mode**, not pass/fail. Refresh
   baselines only when asked or after a validated intentional perf change.
4. Run the scripts **sequentially** from the repo root, in the foreground, one at a time.
   Parallel runs distort timings. A timeout or crash makes the whole pass incomplete.

```bash
./tests/test_scripts/run_ai_benchmark.sh
./tests/test_scripts/run_collision_scaling_benchmark.sh
./tests/test_scripts/run_pathfinder_benchmark.sh
./tests/test_scripts/run_event_scaling_benchmark.sh
./tests/test_scripts/run_particle_manager_benchmark.sh
./tests/test_scripts/run_gpu_frame_benchmark.sh
./tests/test_scripts/run_simd_benchmark.sh --verbose 2>&1 | tee test_results/simd_benchmark_current.txt
./tests/test_scripts/run_integrated_benchmark.sh 2>&1 | tee test_results/integrated_benchmark_current.txt
./tests/test_scripts/run_background_simulation_manager_benchmark.sh
./tests/test_scripts/run_adaptive_threading_analysis.sh
./tests/test_scripts/run_projectile_benchmark.sh
```

Add `--release` to scripts that accept it (AI, event, particle, GPU, projectile) when
running Release. The SIMD and integrated scripts only print to stdout, so capture them
with `tee` as shown. `run_collision_benchmark.sh` is just a wrapper for the collision
scaling script; do not run both. `./tests/test_scripts/run_all_tests.sh --benchmarks-only`
runs the same 11 scripts but gives less control over capture and ordering.

AI, pathfinding, adaptive threading, integrated, and projectile results are required
for a complete report. Mark GPU frame timing environment-sensitive if not run on a
normal desktop session (the average frame is usually VSync-bound).

5. Compare `test_results/` output (including `*_current.txt`) to the matching baseline
   files. Per metric: `change_pct = (current - baseline) / baseline * 100`, sign inverted
   for lower-is-better metrics (times, ns/entity). **>15% degradation on a critical system
   (AI, collision, pathfinding, event, projectile, integrated) is blocking** unless that
   bench's docs say otherwise. If a result regresses, check whether the bench **scope**
   changed (entity counts, workload text, new/removed test cases, build mode; `git log`
   on the bench source listed in `references/benchmarks.md`) before calling it an
   algorithm regression.

Keep AI attack rows separate. Do not fold decision pressure, tactical reset, cold burst,
and cadenced resolve into one "attack" metric.

## Severity

| Severity | Condition |
|----------|-----------|
| **BLOCKING** | >15% degradation on a critical system; benchmark timeout/crash; pathfinding success below 100%; adaptive-threading PASS checks failing; SIMD platform reported as `Scalar (no SIMD)` |
| **WARNING** | 10-15% degradation, or >15% on a non-critical/environment-sensitive bench (GPU, particle, background sim) |
| **MINOR** | 5-10% degradation (monitor) |
| **STABLE** | within ±5% (noise) |
| **IMPROVEMENT** | >5% better |

Per-bench nuances (SIMD Debug vs Release, integrated manager budget, low-precision
pathfinding numbers) are in `references/benchmarks.md`.

## Metrics

- **AI:** entity scaling (updates/sec, threading mode), behavior mix, WorkerBudget
  adaptive tuning. Decision pressure / tactical reset: logic and movement only (damage
  and projectiles suppressed). Cold burst: synchronized fresh-state spike (melee
  EventManager damage + ranged AICommandBus projectiles before WorkerBudget learning).
  Cadenced resolve: primary ongoing combat throughput (AI update, ranged commit, melee
  dispatch, projectile create).
- **Collision:** MM (SAP), dense MM ns/pair, MS (spatial hash), combined, density,
  trigger detection counts and method.
- **Pathfinding:** async throughput, completion/success rate, batching — not deprecated
  immediate-path timings.
- **Event:** throughput, per-event latency, concurrency, batch vs single enqueue,
  threading threshold, combat burst profile.
- **Particles:** update time by particle count, high-count update average, batch count.
- **GPU:** average frame, swapchain, upload, submit.
- **SIMD:** platform and speedup for AI distance, collision bounds, layer mask,
  particle physics.
- **Integrated:** avg/P95/P99 frame, dropped-frame %, max sustainable active NPCs,
  coordination overhead, sustained degradation.
- **Background sim:** scaling, throughput, threading, batch count.
- **Adaptive threading:** `MIN_WORKLOAD`, learned thresholds, hysteresis, batch
  multiplier range.
- **Projectile:** entities/ms, ns/entity, threading mode, SIMD 4-wide curve.

## Report

Blocking regressions first, then warnings, then improvements. Include scripts run,
build mode per script, and baseline source/date. The report is **incomplete** if any
of the 11 scripts is missing without an explicit blocker, or if required metrics above
are absent. Use `references/report-template.md`; save to
`test_results/regression_reports/regression_YYYY-MM-DD.md`.

Do not edit production code on a regression pass unless the user asks to fix a
confirmed regression.

## Exit Codes

- **0:** no regressions
- **1:** blocking regressions
- **2:** warnings only
- **3:** a benchmark failed to run (timeout/crash) — pass incomplete
- **4:** baseline-creation mode (informational)
