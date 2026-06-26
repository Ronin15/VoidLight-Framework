# Benchmark Catalog, Baseline Format, Metrics & Thresholds

Detail reference for the `voidlight-benchmark-regression` Skill. Loaded on demand.
SKILL.md owns the lean workflow; this file owns per-benchmark specifics.

**Discovery is authoritative.** The numbered list below documents the expected core
benchmarks and how to interpret each. Always reconcile against what the repo actually
builds (`ls bin/debug/*_benchmark bin/debug/*_analysis`) and run anything discovery
surfaces that is not yet covered here. All discovered benchmarks MUST be run.

---

## Benchmark Catalog (ALL REQUIRED)

**Working Directory:** Use absolute path to project root or set `$PROJECT_ROOT`.
All paths below are relative to project root.

1. **AI Scaling Benchmark** (`./bin/debug/ai_scaling_benchmark`) **[REQUIRED - CRITICAL]**
   - Script: `./tests/test_scripts/run_ai_benchmark.sh`
   - Tests: Entity scaling (100-10000), threading comparison, behavior mix
   - Metrics: Entity updates/sec, time per update, threading speedup
   - Target: Compare against platform baseline (no regression >15%)
   - Duration: ~3 minutes
   - **Status: CRITICAL - Engine core performance benchmark**

2. **Collision Scaling Benchmark** (`./bin/debug/collision_scaling_benchmark`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_collision_scaling_benchmark.sh`
   - Tests: SAP (Sweep-and-Prune) for MM, Spatial Hash for MS, Trigger Detection scaling
   - Metrics: MM/MS time, throughput, pair counts, trigger detection overlaps, sub-quadratic scaling
   - **Trigger Detection:** Tests spatial query (<50 entities) and sweep-and-prune (>=50 entities) paths
   - Duration: ~2 minutes

3. **Pathfinder Benchmark** (`./bin/debug/pathfinder_benchmark`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_pathfinder_benchmark.sh`
   - Tests: Async pathfinding throughput at scale
   - Metrics: **Async throughput (paths/sec), batch processing performance, success rate**
   - **Note:** Immediate pathfinding deprecated - only track async metrics
   - Duration: ~5 minutes

4. **Event Manager Scaling** (`./bin/debug/event_manager_scaling_benchmark`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_event_scaling_benchmark.sh`
   - Tests: Event throughput 10-4000 events, concurrency
   - Metrics: Events/sec, dispatch latency, queue depth
   - Duration: ~2 minutes

5. **Particle Manager Benchmark** (`./bin/debug/particle_manager_performance_tests`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_particle_manager_benchmark.sh`
   - Tests: Batch rendering performance, particle updates
   - Metrics: Particles/frame, update time, batch count
   - Duration: ~2 minutes

6. **GPU Frame Timing Benchmark** (`./bin/debug/gpu_frame_timing_benchmark`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_gpu_frame_benchmark.sh`
   - Tests: GPU rendering pipeline performance (Vulkan/SPIR-V)
   - Metrics: Avg frame time, swapchain time, GPU upload time, GPU submit time
   - Duration: ~1 minute
   - **Note:** Run from a desktop session for meaningful swapchain/VSync timings.

7. **SIMD Performance Benchmark** (`./bin/debug/simd_performance_benchmark`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_simd_benchmark.sh`
   - Tests: SIMD vs scalar performance across AI, Collision, Particle operations
   - Metrics: Speedup factor (scalar time / SIMD time), platform detection (SSE2/AVX2/NEON)
   - Target: SIMD ≥1.0x in Release builds (Debug may show SIMD slower due to no optimization)
   - Duration: ~1 minute
   - **Note:** Debug builds lack -O3 optimization, so SIMD intrinsic overhead may exceed benefit.
     Run in Release mode for accurate SIMD performance measurement.

8. **Integrated System Benchmark** (`./bin/debug/integrated_system_benchmark`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_integrated_benchmark.sh`
   - Tests: Realistic game simulation (10K AI + 5K particles), scaling 1K-20K entities
   - Metrics: Frame time avg/P95/P99, frame drop %, max sustainable entity count, coordination overhead
   - Target (Release): <16.67ms average (60 FPS), <5% frame drops, <2ms coordination overhead
   - Target (Debug): ~5K entities @ 60 FPS (Debug overhead significant)
   - Duration: ~3 minutes

9. **Background Simulation Benchmark** (`./bin/debug/background_simulation_manager_benchmark`) **[REQUIRED]**
   - Script: `./tests/test_scripts/run_background_simulation_manager_benchmark.sh`
   - Tests: Background tier entity scaling (100-10K), threading threshold, adaptive tuning
   - Metrics: Update time, throughput (entities/ms), batch count, threading mode
   - Target: Sub-linear scaling, ~50K+ items/ms throughput (EDM architecture)
   - Duration: ~2 minutes

10. **Adaptive Threading Analysis** (`./bin/debug/adaptive_threading_analysis`) **[REQUIRED]**
    - Script: `./tests/test_scripts/run_adaptive_threading_analysis.sh`
    - Tests: WorkerBudget adaptive logic validation across AI, Collision, Particle, Event systems
    - **Test Cases:**
      - `MinWorkloadEnforcement`: All 4 systems respect MIN_WORKLOAD=100 (forced SINGLE below 100)
      - `AIManager_ThresholdLearning`: AI learns threading threshold when single-threaded time ≥0.9ms
      - `CollisionManager_ThresholdLearning`: Collision learns threshold (typically hits at 2000 entities)
      - `ParticleManager_ThresholdLearning`: Particle learns threshold (typically ~16K-17K particles)
      - `HysteresisRelearning`: Threshold reset and re-learned when workload drops below 95% band
      - `BatchMultiplierTuning`: Batch multiplier validated in range [0.4, 2.0]
      - `ThreadingStateSummary`: Final state summary for all systems
    - Metrics: Learned threshold per system, hysteresis behavior, batch multiplier range
    - **Key Thresholds:**
      - `MIN_WORKLOAD=100`: Always single-threaded below 100 entities (all systems)
      - `LEARNING_TIME_THRESHOLD_MS=0.9ms`: Threading threshold learned when single time ≥ 0.9ms
      - `HYSTERESIS_FACTOR=0.95`: Re-learning triggered when workload drops to 95% of learned threshold
    - Target: All 8 MIN_WORKLOAD tests PASS, thresholds learned, hysteresis working
    - Duration: ~2 minutes

11. **Projectile Scaling Benchmark** (`./bin/debug/projectile_scaling_benchmark`) **[REQUIRED]**
    - Script: `./tests/test_scripts/run_projectile_benchmark.sh`
    - Tests: Projectile entity throughput scaling, SIMD 4-wide position integration
    - **Test Cases:**
      - `ProjectileScaling`: Entity counts 100-5000, entities/ms and ns/entity
      - `ThreadingModeComparison`: Single vs multi-threaded at different counts
      - `SIMDThroughput`: SIMD 4-wide batching efficiency (4-4000 projectiles)
    - Metrics: Entities/ms, ns/entity, SIMD throughput/ms, threading mode
    - Target: ~17K entities/ms peak throughput, SIMD shows improving efficiency 4→256 projectiles
    - Duration: ~1 minute

### Total Benchmark Duration: ~26 minutes

Per-benchmark durations (full suite ~27 min + 2-3 min report):
AI ~3m | Collision ~2m | Pathfinder ~5m | Event ~2m | Particle ~2m | GPU ~1m |
SIMD ~1m | Integrated ~3m | Background Sim ~2m | Adaptive Threading ~2m | Projectile ~1m

---

## Baseline File Format

**Baseline Storage Location:**
```
$PROJECT_ROOT/test_results/baseline/
├── thread_safe_ai_baseline.txt         (AI system baseline)
├── collision_benchmark_baseline.txt
├── pathfinder_benchmark_baseline.txt
├── event_benchmark_baseline.txt
├── particle_manager_baseline.txt
├── buffer_utilization_baseline.txt
├── resource_tests_baseline.txt
├── serialization_baseline.txt
└── baseline_metadata.txt
```

**Baseline Creation Logic:**
```bash
# Check if baseline exists (requires PROJECT_ROOT to be set)
if [ ! -d "$PROJECT_ROOT/test_results/baseline/" ]; then
    echo "No baseline found. Creating baseline from current run..."
    mkdir -p "$PROJECT_ROOT/test_results/baseline/"
    CREATING_BASELINE=true
fi
```

**When to Create New Baseline:**
- No baseline exists (first run)
- User explicitly requests baseline refresh
- Major optimization work completed (intentional performance change)
- After validating improvements (new baseline for future comparisons)

**Baseline Update Command:**
```bash
# After validating improvements, update baseline (requires PROJECT_ROOT)
cp "$PROJECT_ROOT/test_results/"*/performance_metrics.txt "$PROJECT_ROOT/test_results/baseline/"
echo "Baseline updated: $(date)" > "$PROJECT_ROOT/test_results/baseline/baseline_date.txt"
```

**Baseline History** — keep historical baselines for long-term tracking:
```
$PROJECT_ROOT/test_results/baseline_history/
├── 2025-01-01_baseline/
├── 2025-01-15_baseline/
└── 2025-02-01_baseline/
```

---

## Metrics Extraction Patterns

### AI System Metrics

**Extract scaling performance:**
```bash
# Parse tabular output from AIEntityScaling test
grep -A 20 "AI Entity Scaling" test_results/ai_scaling_benchmark_*.txt | \
  grep -E "^\s+[0-9]+\s+[0-9.]+"

# Extract summary metrics (primary regression detection)
grep -E "Entity updates per second:|Threading mode:|Threading threshold" \
  test_results/ai_scaling_benchmark_*.txt

# Or use the current run file
cat test_results/ai_scaling_current.txt | grep -A 5 "SCALABILITY SUMMARY"
```

**Example Output (EDM Architecture - values are platform-specific):**
```
--- AI Entity Scaling ---
  Entities   Time (ms)   Updates/sec   Threading     Status
       100        0.02       4998406      single        OK
       500        0.10       7740798      single        OK
      1000        0.18      11275361      single        OK
      2000        0.43      13993477      single        OK
      5000        0.30     100413620       multi        OK
     10000        0.30     201148289       multi        OK

SCALABILITY SUMMARY:
Entity updates per second: 201148289 (at 10000 entities)
Threading mode: WorkerBudget Multi-threaded
```

**Note:** The EDM architecture (pure data-driven behaviors) achieves ~200M+ updates/sec due to:
- No behavior class instances or virtual dispatch
- Contiguous EDM arrays with excellent cache locality
- Free-function dispatch via `Behaviors::execute(ctx, config)`

**Baseline Key Format:** `Entity_<count>_UpdatesPerSec`

**Key Metrics:**
- **Updates/sec:** Primary performance metric (higher is better)
- **Threading threshold:** Adaptive via WorkerBudget (typically 2000-5000 entities for multi-threading benefit)
- **Scaling efficiency:** Updates/sec should increase sub-linearly with entity count

### Collision Scaling Metrics
```bash
# Extract metrics from collision scaling benchmark
grep -E "Movables|Statics|Time \(ms\)|Throughput|Scenario" test_results/collision_scaling_current.txt
```

**Example Output:**
```
--- MM Scaling (SAP) ---
  Movables   Time (ms)    MM Pairs    Throughput
       100        0.02           5        5151/ms
       500        0.14          31        3559/ms
      1000        0.22          59        4596/ms
      2000        0.41          97        4893/ms
      5000        1.11         282        4509/ms
     10000        2.26         527        4434/ms

--- MS Scaling (Spatial Hash) ---
   Statics    Movables   Time (ms)    MS Pairs          Mode
       100         200        0.15         155          hash
       500         200        0.14          93          hash
      2000         200        0.15          83          hash
      5000         200        0.15          79          hash
     10000         200        0.15          70          hash
     20000         200        0.15          72          hash

--- Combined Scaling ---
       Scenario   Time (ms)        MM        MS      Total
    Small (500)        0.11        14        15          29
  Medium (1500)        0.19        35        36          71
   Large (3000)        0.34        64        65         129
      XL (6000)        0.65       164       164         328
    XXL (12000)        1.31       305       305         610
```

**Key Metrics:**
- MM SAP: O(n log n) - time grows sub-quadratically with movable count
- MS Hash: O(n) - time stays FLAT as static count increases (spatial hash effectiveness)
- Combined: Sub-quadratic scaling verified up to 12K entities

### Trigger Detection Metrics
```bash
# Extract trigger detection scaling from collision benchmark
grep -E "Detectors|Triggers|Overlaps|Method" test_results/collision_scaling_current.txt | \
  grep -v "^--"
```

**Example Output:**
```
--- Trigger Detection Scaling ---
   Detectors    Triggers   Time (ms)    Overlaps        Method
           1         100       0.143           0        spatial
           1         400       0.138           0        spatial
          10         200       0.145           1        spatial
          25         200       0.148           4        spatial
          50         200       0.228           3          sweep
         100         200       0.255           9          sweep
         200         400       0.433          44          sweep
```

**Key Metrics:**
- **Detectors:** Entities with NEEDS_TRIGGER_DETECTION flag (Player + enabled NPCs)
- **Triggers:** EventOnly triggers in the world (water, area markers, etc.)
- **Method:** Spatial query (<50 entities) or sweep-and-prune (>=50 entities)
- **Performance Target:** <0.5ms for typical scenarios (1-50 detectors, 100-400 triggers)

**Adaptive Strategy Thresholds:**
- < 50 entities: Spatial queries O(N × ~k nearby triggers)
- ≥ 50 entities: Sweep-and-prune O((N+T) log (N+T))

### Pathfinder Metrics **[ASYNC THROUGHPUT ONLY]**

**⚠️ IMPORTANT:** PathfinderManager uses **async-only pathfinding** in production. Immediate (synchronous) pathfinding is deprecated and should NOT be tracked in regression analysis.

**Production Metrics Extraction:**
```bash
# Extract async pathfinding throughput - PRIMARY METRIC
grep -E "Async.*Throughput|paths/sec" test_results/pathfinder_benchmark_results.txt | \
  grep -E "Throughput:"

# Example output format:
#   Throughput: 3e+02 paths/sec
#   Throughput: 4e+02 paths/sec
#   Throughput: 4e+02 paths/sec
```

**REQUIRED Metrics to Extract:**
1. **Async throughput** (paths/second) - Production metric
2. **Success rate** (must be 100%)
3. **Batch processing performance** (if high-volume scenarios tested)

**DEPRECATED Metrics (DO NOT TRACK):**
- ❌ Immediate pathfinding timing (deprecated, not used in production)
- ❌ Path calculation time by distance (legacy synchronous metric)
- ❌ Per-path latency measurements (not relevant for async architecture)

**Baseline Comparison Keys:**
- `Pathfinding_Async_Throughput_PathsPerSec`
- `Pathfinding_Batch_Processing_Enabled`
- `Pathfinding_SuccessRate`

**Example Baseline Comparison:**
```
| Metric | Baseline | Current | Change | Status |
|--------|----------|---------|--------|--------|
| Async Throughput | 300-400 paths/sec | 300-400 paths/sec | 0% | ⚪ Stable |
| Batch Processing | 50K paths/sec | 100K paths/sec | +100% | 🟢 Major Improvement |
| Success Rate | 100% | 100% | 0% | ✓ Maintained |
```

**What to Report:**
- Always include a dedicated "Pathfinding System" section in regression reports
- Focus on async throughput as primary metric
- Highlight batch processing performance for high-volume scenarios
- Note success rate (failures are critical regressions)
- Exclude deprecated immediate pathfinding metrics from analysis

### Event Manager Metrics
```bash
# Extract key throughput metrics from deferred event benchmark
grep -E "Events/sec:|Total time:|Time per event:" test_results/event_scaling_benchmark_output.txt

# Extract threading threshold detection
grep -A 10 "THREADING THRESHOLD DETECTION" test_results/event_scaling_benchmark_output.txt

# Extract concurrency benchmark
grep -A 5 "CONCURRENCY BENCHMARK" test_results/event_scaling_benchmark_output.txt

# Extract batch vs single enqueue comparison
grep -A 5 "BATCH ENQUEUE vs SINGLE" test_results/event_scaling_benchmark_output.txt
```

**Example Output:**
```
===== BASIC HANDLER PERFORMANCE TEST =====
  Config: 3 types, 1 handlers per type, 10 deferred events
  Events/sec: 163436
  Time per event: 0.0061 ms

===== CONCURRENCY BENCHMARK =====
  Config: 23 threads, 173 events/thread = 3979 total deferred events
  Total time: 8.18 ms
  Events/sec: 486696

===== BATCH ENQUEUE vs SINGLE ENQUEUE BENCHMARK =====
  Single + alloc:     583,736 events/sec
  Single (no alloc): 1,805,799 events/sec
  Batch (no alloc):  7,857,195 events/sec
```

**Key Metrics:**
- **Events/sec per config:** Throughput at different handler/event counts
- **Concurrency throughput:** Multi-threaded event processing (23 threads)
- **Batch vs Single:** Enqueue method comparison (batch should be 5-10x faster)
- **Threading threshold:** WorkerBudget mode selection for event processing
- **Time per event:** Dispatch latency (should be <0.01ms)

### Particle Manager Metrics
```bash
grep -E "Particles/frame:|Render Time:|Batch Count:" test_results/particle_benchmark/performance_metrics.txt
```

**Example Output:**
```
Particles/frame: 5000
Render Time: 3.2ms
Batch Count: 12
Culling Efficiency: 88%
```

### GPU Frame Timing Metrics
```bash
grep -E "Avg frame time:|Avg swapchain:|Avg GPU upload:|Avg GPU submit:" test_results/gpu/gpu_frame_timing_benchmark_debug.txt
```

**Example Output:**
```
  Avg frame time:   8.343 ms
  Avg swapchain:    8.039 ms
  Avg GPU upload:   0.001 ms
  Avg GPU submit:   0.045 ms
```

**Key Metrics:**
- **Frame time:** Total CPU+GPU frame time (lower is better)
- **Swapchain:** VSync/present wait time (display-dependent)
- **GPU upload:** Vertex data upload overhead (should be <0.1ms)
- **GPU submit:** Draw call submission overhead (should be <0.5ms)

### SIMD Performance Metrics
```bash
# Extract speedup factors
grep -E "Speedup:|SIMD Time:|Scalar Time:|Status:" test_results/simd_benchmark_current.txt

# Platform detection
grep -E "Detected SIMD:|Platform:" test_results/simd_benchmark_current.txt
```

**Example Output:**
```
=== AIManager Distance Calculation ===
Platform: NEON (ARM64)
SIMD Time:   12.345 ms
Scalar Time: 45.678 ms
Speedup:     3.70x
Status: PASS (SIMD faster than scalar)

=== ParticleManager Physics Update ===
Platform: NEON (ARM64)
SIMD Time:   10.234 ms
Scalar Time: 38.456 ms
Speedup:     3.76x
Status: PASS (SIMD faster than scalar)
```

**Key Metrics:**
- **Speedup factor:** Must be ≥1.0x (SIMD faster than scalar)
- **Platform:** SSE2/AVX2/NEON (should not be "Scalar (no SIMD)")
- **Status:** PASS/FAIL per operation

### Integrated System Benchmark Metrics
```bash
# Extract frame statistics
grep -E "Average:|P95:|P99:|Frame drops|Max:" test_results/integrated_benchmark_current.txt

# Scaling summary
grep -A 15 "Scaling Summary" test_results/integrated_benchmark_current.txt

# Coordination overhead
grep -E "Coordination overhead:" test_results/integrated_benchmark_current.txt
```

**Example Output:**
```
=== Integrated System Load Benchmark ===
Frame Time Statistics:
  Average: 8.45ms ✓ (target < 16.67ms)
  P95: 12.32ms ✓ (target < 20ms)
  P99: 15.67ms ✓ (target < 25ms)
  Max: 18.45ms
  Min: 6.12ms
  Frame drops (>16.67ms): 12/600 (2.0%) ✓

=== Scaling Summary ===
Entities    Avg (ms)    P95 (ms)    Drops (%)   Status
1000        2.15        3.21        0.0         ✓ 60+ FPS
5000        5.82        8.45        1.2         ✓ 60+ FPS
10000       10.34       14.56       3.8         ✓ 60+ FPS
15000       16.23       22.34       8.5         ~ 40-60 FPS
20000       24.56       35.67       18.2        ✗ < 40 FPS

Maximum sustainable entity count @ 60 FPS: 10000

Coordination Overhead Analysis:
  Coordination overhead: 1.2ms (3.5%)
✓ PASS: Coordination overhead < 2ms
```

**Key Metrics:**
- **Average frame time:** Target <16.67ms (60 FPS)
- **P95 frame time:** Target <20ms
- **Frame drop %:** Target <5%
- **Max sustainable entities:** Highest count maintaining 60 FPS
- **Coordination overhead:** Target <2ms

### Background Simulation Manager Metrics
```bash
# Extract scaling performance
grep -E "Entities|Avg \(ms\)|Threaded|Batches" test_results/bgsim_benchmark_current.txt

# Threading recommendation
grep -A 5 "THREADING RECOMMENDATION" test_results/bgsim_benchmark_current.txt

# Adaptive tuning summary
grep -A 10 "ADAPTIVE TUNING SUMMARY" test_results/bgsim_benchmark_current.txt
```

**Example Output (EDM Architecture - values are platform-specific):**
```
===== BACKGROUND SIMULATION SCALING TEST =====
    Entities       Avg (ms)       Min (ms)       Max (ms)    Threaded   Batches
         100          0.003          0.002          0.003          no         1
         500          0.011          0.011          0.011          no         1
        1000          0.022          0.020          0.029          no         1
        2500          0.051          0.050          0.055          no         1
        5000          0.115          0.111          0.131          no         1
        7500          0.169          0.151          0.228          no         1
       10000          0.204          0.200          0.212          no         1

=== THREADING RECOMMENDATION ===
Single throughput: 51733.15 items/ms
Multi throughput:  0.00 items/ms
Batch multiplier:  1.00

=== ADAPTIVE TUNING SUMMARY ===
Batch sizing:       PASS
```

**Note:** With EDM architecture, background simulation is so fast (~50K items/ms) that threading
overhead exceeds the benefit. Single-threaded processing handles 10K entities in ~0.2ms.

**Key Metrics:**
- **Update time:** Should scale sub-linearly with entity count
- **Threading mode:** Adaptive via WorkerBudget (may stay single-threaded if fast enough)
- **Batches:** WorkerBudget batch sizing effectiveness
- **Throughput:** Single throughput ~50K+ items/ms expected with EDM architecture

### Adaptive Threading Analysis Metrics

**Note:** This benchmark validates WorkerBudgetManager adaptive logic across AI, Collision, Particle, and Event systems.

```bash
# Extract MIN_WORKLOAD enforcement results
grep -A 15 "MIN_WORKLOAD ENFORCEMENT" test_results/adaptive_threading_current.txt

# Extract per-system threshold learning
grep -E "Learned threshold:|Threshold active:" test_results/adaptive_threading_current.txt

# Extract hysteresis re-learning result
grep -A 5 "Hysteresis Band" test_results/adaptive_threading_current.txt

# Extract batch multiplier validation
grep -A 10 "BATCH MULTIPLIER TUNING" test_results/adaptive_threading_current.txt

# Extract final WBM state summary
grep -A 10 "WORKERBUDGET STATE SUMMARY" test_results/adaptive_threading_current.txt
```

**Example Output:**
```
===== MIN_WORKLOAD ENFORCEMENT (ALL SYSTEMS) =====
Testing MIN_WORKLOAD=100 enforcement:
  System      Workload  Expected  Actual    Result
  ----------  --------  --------  ------    ------
  AI                50    SINGLE  SINGLE    PASS
  AI                99    SINGLE  SINGLE    PASS
  Collision         50    SINGLE  SINGLE    PASS
  Collision         99    SINGLE  SINGLE    PASS
  Particle          50    SINGLE  SINGLE    PASS
  Particle          99    SINGLE  SINGLE    PASS
  Event             50    SINGLE  SINGLE    PASS
  Event             99    SINGLE  SINGLE    PASS
Validation: MIN_WORKLOAD enforcement: PASS

===== COLLISION MANAGER THRESHOLD LEARNING =====
  Threshold learned at frame 10: 2000 entities
  Final state:
    Learned threshold: 2000
    Threshold active: true

===== PARTICLE MANAGER THRESHOLD LEARNING =====
  Threshold learned at frame 70: 16750 particles
  Final state:
    Learned threshold: 16750
    Threshold active: true

===== HYSTERESIS BAND RE-LEARNING =====
  Learned threshold: 3000 (active)
  Hysteresis low boundary (95%): 2850
  After workload drop to 2840:
    Threshold: 0 (was 3000), Active: false, Decision: SINGLE
Validation: Re-learning triggered: PASS

===== BATCH MULTIPLIER TUNING =====
  System      Multiplier  InRange
  AI          1.000       PASS
  Collision   1.000       PASS
  Particle    1.000       PASS
  Event       1.000       PASS
Validation: All multipliers in range: PASS

===== WORKERBUDGET STATE SUMMARY (ALL SYSTEMS) =====
System       Threshold   Active    BatchMult   MultiTP
AI                   0    false         1.00         0
Collision            0    false         1.00      4294
Particle             0    false         1.00         0
Event                0    false         1.00         0
Pathfinding          0    false         1.00         0
```

**Key Metrics:**
- **MIN_WORKLOAD enforcement:** All 8 tests (4 systems × 2 workloads) must PASS
- **Threshold learning:** Collision typically learns at 2000 entities, Particle at ~16K-17K particles
- **AI threshold:** May not be learned if hardware is fast enough (0.9ms threshold not hit) — expected
- **Hysteresis:** Re-learning must trigger when workload drops below 95% band — PASS required
- **Batch multiplier:** All systems must be within [0.4, 2.0] range
- **Collision MultiTP:** Should be non-zero after test (typically 4000-5000 items/ms)

### Projectile Scaling Metrics
```bash
# Extract entity scaling table
grep -A 10 "Projectile Entity Scaling" test_results/projectile_scaling_current.txt | \
  grep -E "^\s+[0-9]+|Projectiles"

# Extract scalability summary
grep -A 3 "SCALABILITY SUMMARY" test_results/projectile_scaling_current.txt

# Extract SIMD throughput table
grep -A 12 "SIMD 4-Wide Throughput" test_results/projectile_scaling_current.txt | \
  grep -E "^\s+[0-9]+|Projectiles"
```

**Example Output:**
```
--- Projectile Entity Scaling ---
 Projectiles   Time (ms)   Entities/ms   ns/entity   Threading   Status
         100       0.006         15903        62.9      single        OK
         500       0.029         17003        58.8      single        OK
        1000       0.058         17309        57.8      single        OK
        2000       0.115         17385        57.5      single        OK
        5000       0.286         17477        57.2      single        OK

SCALABILITY SUMMARY:
Best throughput: 17477 entities/ms (at 5000 projectiles)

--- SIMD 4-Wide Throughput ---
 Projectiles   Time (ms)   ns/entity  Throughput/ms
           4      0.0008       189.8            5269
          16      0.0019       118.5            8440
          64      0.0041        64.8           15435
         256      0.0153        59.7           16764
        1000      0.0581        58.1           17218
        4000      0.2288        57.2           17481
```

**Key Metrics:**
- **Peak throughput:** ~17K entities/ms (at 1000-5000 projectiles)
- **ns/entity:** Should converge to ~57-63ns at 1000+ projectiles (cache-warm regime)
- **SIMD efficiency:** Throughput should improve significantly from 4→64 projectiles (SIMD batching warmup), then plateau
- **Threading:** Typically all single-threaded (fast enough that threading overhead not beneficial)

---

## Per-Benchmark Regression Detection Thresholds

Universal severity thresholds (>15% = CRITICAL, 10-15% = WARNING, 5-10% = MINOR,
<5% = STABLE, >+5% = IMPROVEMENT) live in SKILL.md. The classifier and per-benchmark
specifics below refine them.

**Classifier:**
```python
def classify_change(metric_name, baseline, current, is_lower_better=False):
    change_pct = ((current - baseline) / baseline) * 100

    # Invert for metrics where lower is better (e.g., CPU%, time)
    if is_lower_better:
        change_pct = -change_pct

    # Critical thresholds for AI system (most important)
    if "AI" in metric_name or "FPS" in metric_name:
        if metric_name == "FPS" and current < 60:
            return "CRITICAL", "FPS below 60 threshold"
        if metric_name == "CPU" and current > 8:
            return "CRITICAL", "CPU exceeds 8% threshold"

    # General thresholds
    if change_pct < -15:
        return "CRITICAL", f"{abs(change_pct):.1f}% regression"
    elif change_pct < -10:
        return "WARNING", f"{abs(change_pct):.1f}% regression"
    elif change_pct < -5:
        return "MINOR", f"{abs(change_pct):.1f}% regression"
    elif change_pct > 5:
        return "IMPROVEMENT", f"{change_pct:.1f}% improvement"
    else:
        return "STABLE", f"{abs(change_pct):.1f}% variance (acceptable)"
```

### AI Benchmark Regression Detection Strategy

1. **Scaling Regression (Low Entity Counts):**
   - 100-500 entity performance regresses more than larger counts
   - **Root Cause:** Single-threaded path overhead, behavior initialization
   - **Action:** Profile single-threaded update path, check behavior creation costs
   - **Impact:** Small-world performance degraded

2. **Scaling Regression (High Entity Counts):**
   - 5000-10000 entity performance regresses
   - **Root Cause:** Threading overhead, batch processing, WorkerBudget tuning
   - **Action:** Profile ThreadSystem, check batch sizes, validate WorkerBudget
   - **Impact:** Large-world performance degraded

3. **Threading Speedup Degraded:**
   - Single-threaded time stable but multi-threaded time increases
   - **Root Cause:** Thread contention, lock overhead, false sharing
   - **Action:** Profile thread synchronization, check mutex usage
   - **Impact:** Threading efficiency degraded

4. **Uniform Regression:**
   - All entity counts regress proportionally
   - **Root Cause:** Core update loop changes, behavior execution overhead
   - **Action:** Profile behavior update(), check per-entity costs
   - **Impact:** System-wide performance degradation

### SIMD Benchmark Regression Detection

*Note: SIMD benchmarks should be run in Release mode for accurate results. Debug mode lacks
-O3 optimization, causing SIMD intrinsic overhead to exceed benefit (expected behavior).*

**Release Mode:**
- 🔴 CRITICAL: SIMD slower than scalar (speedup < 1.0x) for AI Distance or Particle Physics
- 🔴 CRITICAL: Platform shows "Scalar (no SIMD)" - SIMD not compiling correctly
- 🟠 WARNING: Speedup <2.0x for AI Distance (expect 3-4x)
- ⚪ STABLE: Speedup within ±20% of baseline

**Debug Mode:**
- ⚪ EXPECTED: SIMD may be slower than scalar (no optimization)
- 🔴 CRITICAL: Platform shows "Scalar (no SIMD)" - SIMD not compiling correctly

### Integrated System Regression Detection

*Note: Targets below are for Release builds. Debug builds have significant overhead;
expect ~5K max sustainable entities @ 60 FPS in Debug mode.*

**Release Mode:**
- 🔴 CRITICAL: Average frame time >16.67ms (below 60 FPS) at 10K entities
- 🔴 CRITICAL: Frame drop % >10% at standard load
- 🟠 WARNING: P95 >20ms or coordination overhead >2ms
- 🟠 WARNING: Max sustainable entities decreased >20%
- 🟡 MINOR: Sustained performance degradation >5% over 50s
- ⚪ STABLE: All metrics within targets

**Debug Mode:**
- ⚪ EXPECTED: ~5K max sustainable entities @ 60 FPS
- 🔴 CRITICAL: Coordination overhead >2ms (should still be low)
- 🟠 WARNING: Max sustainable entities <3K @ 60 FPS

### Background Simulation Regression Detection
- 🔴 CRITICAL: Update time regression >50% from baseline
- 🔴 CRITICAL: Adaptive tuning failing (batch sizing not converging)
- 🟠 WARNING: Throughput regression >25% from baseline
- 🟡 MINOR: Sub-linear scaling not maintained
- ⚪ STABLE: Performance within ±15% of baseline

**Note:** With EDM architecture, background simulation is extremely fast. Threading may not be
beneficial as single-threaded processing is often faster than threading overhead. WBM adapts correctly.

### Adaptive Threading Analysis Regression Detection

*Note: The benchmark now tests all 4 systems (AI, Collision, Particle, Event). AI threshold may not be
learned if single-threaded AI is fast enough that 0.9ms is never hit — this is expected behavior.*

- 🔴 CRITICAL: Any MIN_WORKLOAD test fails (expected: all 8/8 PASS for SINGLE below 100)
- 🔴 CRITICAL: Collision threshold not learned (expected: learned at frame ~10 for 2000 entities)
- 🔴 CRITICAL: Hysteresis re-learning not triggered (PASS expected when workload drops below 95% band)
- 🟠 WARNING: Batch multiplier outside valid range [0.4, 2.0]
- 🟠 WARNING: Particle threshold learned significantly later (>frame 200) suggesting performance regression
- ⚪ STABLE: All 8 MIN_WORKLOAD tests PASS, Collision/Particle thresholds learned, hysteresis working
- ⚪ EXPECTED: AI threshold not learned (hardware fast enough that 0.9ms not hit in benchmark)

### Projectile Scaling Regression Detection
- 🔴 CRITICAL: Peak throughput regression >15% from baseline (~17K entities/ms target)
- 🟠 WARNING: ns/entity at 1000+ projectiles regression >10% (expected ~57-63ns)
- 🟡 MINOR: SIMD warmup curve degraded (4→64 projectile improvement smaller than baseline)
- ⚪ STABLE: Throughput within ±10% of baseline, ns/entity consistent
- ⚪ EXPECTED: All single-threaded (threading overhead exceeds benefit at these counts)
