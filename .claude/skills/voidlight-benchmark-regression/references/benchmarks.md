# Benchmark Catalog, Baselines, Metrics & Detection Notes

Detail reference for the `voidlight-benchmark-regression` Skill. Loaded on demand.
SKILL.md owns the procedure and severity table; this file owns per-bench specifics.

All paths are relative to the repo root. Example outputs are from one platform
(x86-64 SSE2/AVX2, 24 hardware threads, Debug) and are **shape references only** —
never use them as targets.

**Output files contain ANSI color codes** (Boost.Test log colouring). Strip them before
anchoring greps on line starts:

```bash
strip() { sed 's/\x1b\[[0-9;]*m//g' "$1"; }
```

---

## Script Catalog (11 scripts, all required for a complete pass)

| # | Script | Executable | Build flag | Output file(s) |
|---|--------|------------|-----------|----------------|
| 1 | `run_ai_benchmark.sh` | `ai_scaling_benchmark` | `--release` (default Debug) | `test_results/ai_scaling_benchmark_<ts>.txt`, copied to `ai_scaling_current.txt` |
| 2 | `run_collision_scaling_benchmark.sh` | `collision_scaling_benchmark` | Debug only | `collision_scaling_benchmark_<ts>.txt`, `collision_scaling_current.txt` |
| 3 | `run_pathfinder_benchmark.sh` | `pathfinder_benchmark` | Debug only | `pathfinder_benchmark_results.txt` (overwritten each run) |
| 4 | `run_event_scaling_benchmark.sh` | `event_manager_scaling_benchmark` | `--release` | `event_scaling_benchmark_output.txt` (overwritten) |
| 5 | `run_particle_manager_benchmark.sh` | `particle_manager_performance_tests` | `--release` | `particle_benchmark_performance_metrics.txt`, `particle_benchmark.csv`, `particle_benchmark_<ts>.txt`, `particle_manager/particle_manager_performance_tests_output.txt` |
| 6 | `run_gpu_frame_benchmark.sh` | `gpu_frame_timing_benchmark` | `--release` | `test_results/gpu/gpu_frame_timing_benchmark_{debug,release}.txt` |
| 7 | `run_simd_benchmark.sh` | `simd_performance_benchmark` | Debug only | stdout only — capture with `--verbose ... \| tee test_results/simd_benchmark_current.txt` |
| 8 | `run_integrated_benchmark.sh` | `integrated_system_benchmark` | Debug only | stdout only — capture with `\| tee test_results/integrated_benchmark_current.txt` |
| 9 | `run_background_simulation_manager_benchmark.sh` | `background_simulation_manager_benchmark` | Debug only | `background_simulation_benchmark_<ts>.txt`, `background_simulation_benchmark_current.txt` |
| 10 | `run_adaptive_threading_analysis.sh` | `adaptive_threading_analysis` | Debug only | `adaptive_threading_analysis_<ts>.txt`, `adaptive_threading_analysis_current.txt` |
| 11 | `run_projectile_benchmark.sh` | `projectile_scaling_benchmark` | `--release` | `projectile_scaling_benchmark_<ts>.txt`, `projectile_scaling_current.txt` |

"Debug only" scripts hard-code `bin/debug/<exe>`. For a Release number from those,
run the executable directly (`./bin/release/<exe> --log_level=test_suite 2>&1 | tee ...`)
and say so in the report. `run_collision_benchmark.sh` is a thin wrapper around #2.

Verify the set still matches before a pass (the suite grows):
```bash
ls tests/test_scripts/run_*benchmark*.sh tests/test_scripts/run_*analysis*.sh
ls bin/debug/*_benchmark bin/debug/*_analysis bin/debug/particle_manager_performance_tests
```
Run anything new that discovery surfaces and note it as uncatalogued.

Bench sources (for scope-change checks via `git log`): `tests/AIScalingBenchmark.cpp`,
`tests/EventManagerScalingBenchmark.cpp`, `tests/gpu/GPUFrameTimingBenchmark.cpp`,
`tests/managers/BackgroundSimulationManagerBenchmark.cpp`,
`tests/particle/ParticleManagerPerformanceTest.cpp`, and `tests/performance/*.cpp`
(collision, pathfinder, SIMD, integrated, adaptive threading, projectile).

Timeouts built into scripts: AI 180s, projectile 120s, particle 300s; others have none —
treat a run that hangs well past its normal duration as a timeout.

---

## Baseline Layout

`test_results/baseline/` holds copies of each script's output file under the same name,
plus metadata:

```
test_results/baseline/
├── baseline_metadata.txt                    (dates, build mode, platform, notes)
├── ai_scaling_current.txt
├── collision_scaling_current.txt
├── pathfinder_benchmark_results.txt
├── event_scaling_benchmark_output.txt
├── particle_benchmark_performance_metrics.txt
├── gpu_frame_timing_benchmark_debug.txt
├── background_simulation_benchmark_current.txt
├── adaptive_threading_analysis_current.txt
└── projectile_scaling_current.txt
```

SIMD and integrated have no baseline file until one is created from a captured run
(`simd_benchmark_current.txt`, `integrated_benchmark_current.txt`). Missing baseline for
a bench → baseline-creation mode for that bench only.

Snapshots of older baselines use sibling dirs named `test_results/baseline_<date>_<label>/`
(e.g. `baseline_2026-04-21_pre-patch/`).

**Refresh** (only when asked, or after a validated intentional perf change):
```bash
cp test_results/ai_scaling_current.txt test_results/baseline/   # repeat per bench refreshed
# then update baseline_metadata.txt: date, build mode, platform, branch, reason
```
Keep build mode consistent: a Debug baseline cannot judge a Release run.

---

## Metric Extraction

### 1. AI Scaling (`ai_scaling_current.txt`)

Test cases: `AIEntityScaling`, `BehaviorMixTest`, `AttackBehaviorPressureScaling`,
`AttackBehaviorTacticalResetScaling`, `AttackBehaviorBurstResolveScaling`,
`AttackBehaviorCadencedResolveScaling`, `WorkerBudgetAdaptiveTuning`.

```bash
strip test_results/ai_scaling_current.txt | grep -A 9 -- '--- AI Entity Scaling ---'
strip test_results/ai_scaling_current.txt | grep -E 'Entity updates per second:|Threading mode:'
strip test_results/ai_scaling_current.txt | grep -A 4 -- '--- Behavior Mix Test'
# One table per attack workload — keep them separate
for t in 'Decision Pressure' 'Decision Tactical Reset' 'Cold Burst Resolve' 'Cadenced Resolve'; do
  strip test_results/ai_scaling_current.txt | grep -A 8 -- "--- Attack Behavior $t"
done
strip test_results/ai_scaling_current.txt | grep -E 'Batch sizing:|Throughput tracking:'
```

Shape:
```
--- AI Entity Scaling ---
  Entities   Time (ms)   Updates/sec   Threading   Status
       100        0.02       4290960      single        OK
      5000        0.24      20516997       multi        OK
     25000        0.84      29594885       multi        OK
SCALABILITY SUMMARY:
Measurement: median of 5 runs per entity count
Entity updates per second: 29594885 (at 25000 entities)
Threading mode: WorkerBudget Multi-threaded
```
Attack tables use `Attackers  Time (ms)  Updates/sec  Threading  Status` for 100–5000.
Compare updates/sec per row (higher is better). Row keys: `AI_<table>_<count>`.

### 2. Collision Scaling (`collision_scaling_current.txt`)

Sections: `--- MM Scaling (SAP) ---`, `--- Dense MM Scaling (narrowphase-heavy) ---`,
`--- MS Scaling (Spatial Hash) ---`, `--- Combined Scaling ---`,
`--- Entity Density Test ... ---`, `--- Trigger Detection Scaling ---`.

```bash
for s in 'MM Scaling (SAP)' 'Dense MM Scaling' 'MS Scaling' 'Combined Scaling' 'Entity Density' 'Trigger Detection Scaling'; do
  strip test_results/collision_scaling_current.txt | grep -A 9 -- "--- $s"
done
```

Expectations: MM SAP grows sub-quadratically; MS time stays roughly flat as statics grow;
dense MM ns/pair roughly constant; trigger method switches `spatial` (<50 detectors) →
`sweep` (≥50). Compare Time (ms) per row (lower is better). Pair/overlap counts should be
identical to baseline — a change means scope/data changed, not performance.

### 3. Pathfinder (`pathfinder_benchmark_results.txt`) — async only

```bash
strip test_results/pathfinder_benchmark_results.txt \
  | grep -E '^Batch size|^  Completed:|Actual completion time:|Throughput: .*paths/sec'
strip test_results/pathfinder_benchmark_results.txt | grep -A 25 '=== Cache Performance Analysis ===' \
  | grep -E 'Speedup ratio|Cache efficiency|Paths/second'
```

Shape:
```
Batch size 500:
  Completed: 500/500
  Actual completion time: 2.03ms
  Throughput: 2e+05 paths/sec
```

- Track per batch size (10/50/100/250/500): `Completed: N/N` (anything short of N/N is
  blocking) and `Actual completion time` (lower is better).
- `Throughput:` is printed with **one significant digit** (`2e+05`), so percentage deltas
  on it are meaningless; use completion time for the % comparison and quote throughput
  as context only.
- Ignore the `=== Immediate Pathfinding Performance ===` and `Path Length vs Performance`
  sections — immediate/synchronous pathfinding is deprecated. Cache speedup is
  informational (the bench itself flags it as within timing variance).

### 4. Event Manager (`event_scaling_benchmark_output.txt`)

```bash
F=test_results/event_scaling_benchmark_output.txt
strip $F | grep -E '^=====|Events/sec:|Time per event:'
strip $F | grep -A 8 '===== CONCURRENCY BENCHMARK ====='
strip $F | grep -A 18 '===== EVENT THREADING THRESHOLD DETECTION ====='
strip $F | grep -A 16 '===== BATCH ENQUEUE vs SINGLE ENQUEUE BENCHMARK ====='
strip $F | grep -E 'frame:$|Total:|Throughput:|WorkerBudget mode:|Queue usage'   # combat burst profile
```

Sections: basic handler, medium scale, scalability (per `3 types, N handlers, M events`),
concurrency (23 threads), threading verification, extreme scale, threading threshold
detection, WorkerBudget adaptive tuning, batch vs single enqueue, combat burst profile
(Skirmish/Raid/Siege/Large combat/Near-cap battle frames).

Key comparisons: Events/sec per config, concurrency Events/sec, batch vs single
Events/sec and `Batch speedup: N.NNx`, combat burst `Total:` ms per frame type.

### 5. Particle Manager (`particle_benchmark_performance_metrics.txt`)

The metrics file is a grep of the performance test output (no "Particles/frame" /
"Render Time" / "Culling" lines exist).

```bash
F=test_results/particle_benchmark_performance_metrics.txt
grep -E '^Particles: [0-9]+, Update time:' $F                 # scaling rows
grep -E 'Testing update performance with|^Update time:' $F   # 1K / 5K update
grep -E 'HighCountBench: update_avg_ms' $F                   # ~100K particle update avg
grep -E 'Initial batch count|Current threshold' $F
cat test_results/particle_benchmark.csv
```

Note: `HighCountBench` targets 10K/25K/50K but actual counts land near 100K in all three
rows (see `actual=` in the output); compare the update averages, not the target labels.

### 6. GPU Frame Timing (`gpu/gpu_frame_timing_benchmark_<debug|release>.txt`)

```bash
grep -E 'Mode:|Quads/frame:|Avg frame time:|Avg swapchain:|Avg GPU upload:|Avg GPU submit:' \
  test_results/gpu/gpu_frame_timing_benchmark_debug.txt
```
Frame and swapchain times are VSync-bound (~16.6ms at 60Hz); regressions show in upload
and submit. Environment-sensitive: the script skips if the executable is not built, and
headless/remote sessions give meaningless swapchain numbers.

### 7. SIMD (`simd_benchmark_current.txt`, captured)

```bash
grep -E '^=== |Platform:|Build:|SIMD Time:|Scalar Time:|Speedup:|Status:' test_results/simd_benchmark_current.txt
grep 'Detected SIMD:' test_results/simd_benchmark_current.txt
```
Operations: AI distance calculation, collision bounds expansion, collision layer mask
filtering, particle physics update. Platform strings: `AVX2 (x86-64)`, `SSE2 (x86-64)`,
`NEON (ARM64)`, `Scalar (no SIMD)`.

### 8. Integrated System (`integrated_benchmark_current.txt`, captured)

Test cases: `TestRealisticGameSimulation60FPS`, `TestScalingUnderLoad`,
`TestManagerCoordinationOverhead`, `TestSustainedPerformance`.

```bash
F=test_results/integrated_benchmark_current.txt
grep -E 'Average:|Median:|P95:|P99:|Max:|Frame drops' $F
grep -A 10 '=== Scaling Summary ===' $F
grep -E 'Max Active NPCs sustainable' $F
grep -E 'Coordination overhead:|PASS: overhead|FAIL: overhead|^Overall:' $F
grep -A 4 'Degradation Analysis:' $F
```

Budgets are constants in `tests/performance/IntegratedSystemBenchmark.cpp`:
full frame 16.67ms, manager-only budget 10ms, P95 20ms, P99 25ms, frame drops <5%;
coordination overhead ≤2ms at 1000 entities, ≤5ms at 10000; sustained degradation <10%.
Scaling runs 500/1000/2500/5000/10000 total entities (Active = 60%). Read scenario sizes
from the output rather than assuming them.

### 9. Background Simulation (`background_simulation_benchmark_current.txt`)

```bash
F=test_results/background_simulation_benchmark_current.txt
strip $F | grep -A 11 '===== BACKGROUND SIMULATION SCALING TEST ====='
strip $F | grep -A 11 '===== BACKGROUND SIM THREADING THRESHOLD DETECTION ====='
strip $F | grep -A 4 '=== ADAPTIVE TUNING SUMMARY ==='
```
Scaling table: `Entities  Avg (ms)  Min (ms)  Max (ms)  Threaded  Batches` (100–10000).
Throughput (items/ms) is a column of the threshold-detection table. Currently
single-threaded at every count; `Throughput tracking: NO_DATA` is expected when multi
never runs.

### 10. Adaptive Threading (`adaptive_threading_analysis_current.txt`)

```bash
F=test_results/adaptive_threading_analysis_current.txt
strip $F | grep -A 15 '===== MIN_WORKLOAD ENFORCEMENT'
strip $F | grep -E '^===== .*THRESHOLD LEARNING|Learned threshold:|Threshold active:'
strip $F | grep -A 15 '===== HYSTERESIS BAND RE-LEARNING'
strip $F | grep -A 10 '===== BATCH MULTIPLIER TUNING'
strip $F | grep -A 10 '===== WORKERBUDGET STATE SUMMARY'
strip $F | grep '^Validation:'
```
Constants (`include/core/WorkerBudget.hpp`): `MIN_WORKLOAD = 100`,
`LEARNING_TIME_THRESHOLD_MS = 0.9`, `HYSTERESIS_FACTOR = 0.95`. Batch multiplier
valid range [0.4, 2.0] (checked by the test).

Expect: 8/8 MIN_WORKLOAD rows PASS (AI/Collision/Particle/Event × 50/99), all
`Validation:` lines PASS, Collision and Particle thresholds learned and active. AI
threshold `0`/inactive is expected on fast hardware (0.9ms never reached). Learned
threshold values (e.g. Collision 2000, Particle ~14K–17K) drift with machine speed —
compare them as % like any other metric, not as fixed targets.

### 11. Projectile Scaling (`projectile_scaling_current.txt`)

```bash
F=test_results/projectile_scaling_current.txt
strip $F | grep -A 7 -- '--- Projectile Entity Scaling ---'
strip $F | grep -E 'Best throughput:'
strip $F | grep -A 8 -- '--- Threading Mode Comparison ---'
strip $F | grep -A 9 -- '--- SIMD 4-Wide Throughput ---'
```
Tables: entity scaling (100–5000: Time, Entities/ms, ns/entity, Threading), threading
comparison (200–5000), SIMD 4-wide throughput (4–4000: ns/entity, Throughput/ms). SIMD
throughput should rise steeply from 4→256 projectiles, then plateau. Typically all
single-threaded at these counts.

---

## Per-Bench Detection Notes

The severity table in SKILL.md applies to every % comparison. Additional rules:

- **AI:** Classify the pattern when a regression appears — low counts only (single-thread
  path/per-entity cost), high counts only (threading/batching/WorkerBudget), threaded
  rows only (contention/false sharing), or uniform (core loop or behavior execution).
  Cold burst is a synchronized spike and is noisier than cadenced resolve; report both.
- **Collision:** A change in pair/overlap counts with the same inputs means the bench or
  collision semantics changed — report it as a scope change, not a perf delta.
- **Pathfinding:** Any `Completed` short of the batch size is blocking.
- **SIMD:** Debug builds lack optimisation, so SIMD slower than scalar in Debug is
  expected. In Release, speedup <1.0x for AI distance or particle physics is blocking.
  `Scalar (no SIMD)` platform is blocking in any build.
- **Integrated:** The bench's own `Result: FAIL` (realistic simulation) or a failed
  sustained-degradation check is blocking; P95/P99 over target, coordination-overhead
  FAIL, or max sustainable Active NPCs dropping >15% is at least a warning.
- **Background sim:** Batch sizing not PASS is a warning; otherwise % rules.
- **Adaptive threading:** Any MIN_WORKLOAD row or `Validation:` line not PASS, or Collision
  threshold not learned, is blocking. Multiplier outside [0.4, 2.0] is a warning.
- **Projectile:** % rules on best throughput and ns/entity at ≥1000 projectiles; a flatter
  SIMD 4→256 curve than baseline is a minor finding.
- **GPU:** Upload/submit deltas matter; frame/swapchain deltas are display-dependent.
