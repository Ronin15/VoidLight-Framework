---
name: voidlight-benchmark-regression
description: Runs performance benchmarks for SDL3 VoidLight-Framework and detects regressions by comparing metrics against baseline. Use when testing performance-sensitive changes to AI, collision, pathfinding, particle systems, or before merging features to ensure no performance degradation.
allowed-tools: [Bash, Read, Write, Grep]
---

# VoidLight-Framework Performance Regression Detection

This Skill is **critical** for SDL3 VoidLight-Framework's performance requirements: 10,000+
entities at 60+ FPS with minimal CPU. It detects performance regressions before they reach
production.

This file is the lean playbook. Detail lives in `references/` and is loaded on demand:
- **`references/benchmarks.md`** — per-benchmark catalog, baseline file format, metric
  extraction commands, and per-benchmark regression thresholds.
- **`references/report-template.md`** — the full regression report + console summary template.
- **`references/troubleshooting.md`** — timeouts, the final validation gate, and troubleshooting.

## Performance Requirements (from CLAUDE.md)

- **AI System:** 10,000+ entities at 60+ FPS with <6% CPU
- **Collision System:** Spatial hash with efficient AABB detection
- **Pathfinding:** A* pathfinding with dynamic weights (async-only in production)
- **Event System:** 1K-10K event throughput
- **Particle System:** Camera-aware batched rendering

## Cross-Platform Note

Absolute performance numbers vary by platform (CPU, memory, OS). Always compare **percentage
change against a platform-specific baseline** in `test_results/baseline/`, never hard-coded
absolutes. Severity thresholds (>15% = CRITICAL, etc.) apply universally. Example outputs in
the reference files are representative of one platform only.

## Workflow

1. **Identify or create baseline** — store previous metrics
2. **Discover + run the full benchmark suite** — sequentially, AI first
3. **Extract metrics** — parse each benchmark's output
4. **Compare vs baseline** — percentage change per metric
5. **Flag regressions** — classify by severity
6. **Generate report** — full analysis with recommendations

**⚠️ AI Scaling Benchmark is MANDATORY.** The AI System is the most performance-critical
component. Always run `ai_scaling_benchmark` (via `./tests/test_scripts/run_ai_benchmark.sh`)
and never proceed to report generation without AI results.

---

### Step 1: Identify Baseline

Baselines live in `$PROJECT_ROOT/test_results/baseline/`. If that directory does not exist,
create a fresh baseline from this run (exit code 4, informational). Also create a new baseline
when the user requests a refresh or after validating an intentional optimization.

For the exact baseline file layout, creation logic, and update/history commands, read
**`references/benchmarks.md`** (Baseline File Format).

### Step 2: Discover + Run the Suite

**Discover the current benchmark set at runtime** — do NOT assume a fixed list; the suite grows
as systems are added. Enumerate what the repo actually builds and reconcile against the catalog:
```bash
ls bin/debug/*_benchmark bin/debug/*_analysis 2>/dev/null
ls tests/test_scripts/run_*benchmark*.sh tests/test_scripts/run_*analysis*.sh 2>/dev/null
```

**All discovered benchmarks MUST be run** for a complete regression analysis. Run any benchmark
surfaced by discovery even if it is not yet documented in `references/benchmarks.md`. The core
expected set is the AI scaling, collision scaling, pathfinder, event scaling, particle, GPU frame
timing, SIMD, integrated system, background simulation, adaptive threading, and projectile scaling
benchmarks — each has a matching `run_*.sh` script.

**⚠️ SEQUENTIAL EXECUTION ONLY.** Run benchmarks one at a time, waiting for each to fully complete
before starting the next. NEVER run benchmarks in parallel (background tasks, concurrent shells) —
parallelism causes CPU/memory contention that skews timing. Use foreground (`run_in_background:
false`) for every invocation. Run the AI benchmark first (most critical, longest-running).

```bash
# Set PROJECT_ROOT and run from the project directory first:
#   cd /path/to/VoidLight-Framework && export PROJECT_ROOT=$(pwd)
# Then run each discovered script in order, AI first, e.g.:
./tests/test_scripts/run_ai_benchmark.sh        # CRITICAL — always first
./tests/test_scripts/run_collision_scaling_benchmark.sh
./tests/test_scripts/run_pathfinder_benchmark.sh
# ... continue through every discovered run_*.sh, sequentially
```

Each benchmark has timeout protection (AI: 600s, others: 300s). A timeout signals a likely
infinite loop or performance catastrophe — flag it (exit code 3). Full per-benchmark catalog,
durations (suite ~26 min), test cases, and targets are in **`references/benchmarks.md`**.

### Step 3: Extract Metrics

Parse each benchmark's output file in `test_results/` for its key metrics. Each benchmark has a
specific grep recipe, expected output shape, and baseline key format documented in
**`references/benchmarks.md`** (Metrics Extraction Patterns). Critical extraction rules:

- **AI:** entity-scaling table + updates/sec (primary metric).
- **Pathfinding:** **async throughput (paths/sec) + success rate ONLY** — immediate/synchronous
  pathfinding is deprecated and must NOT be tracked.
- **Collision:** SAP/Hash timing + trigger detection (detectors, overlaps, spatial/sweep method).
- **SIMD:** speedup factor + platform (must not be "Scalar (no SIMD)") for all 4 operations.
- **Adaptive Threading:** MIN_WORKLOAD enforcement (8/8 PASS), per-system threshold learning,
  hysteresis, batch multiplier.
- **Integrated / Background Sim / Projectile / Event / Particle / GPU:** see reference.

### Step 4: Compare Against Baseline

For each metric: `change_pct = ((current - baseline) / baseline) * 100` (invert sign for
lower-is-better metrics like time/CPU). Classify against the universal severity thresholds below.

### Step 5: Flag Regressions

| Severity | Condition |
|----------|-----------|
| 🔴 **CRITICAL** (block merge) | Any metric degrades >15%; AI FPS <60; AI CPU >8%; benchmark timeout |
| 🟠 **WARNING** (review) | Degradation 10-15%; AI FPS 60-65; collision/pathfinding >10% slower |
| 🟡 **MINOR** (monitor) | Degradation 5-10% |
| ⚪ **STABLE** | Change <5% (measurement noise) |
| 🟢 **IMPROVEMENT** | Improvement >5% |

The AI system carries hard gates (FPS <60 or CPU >8% are always CRITICAL regardless of %).
For the `classify_change()` reference implementation and per-benchmark detection specifics
(AI scaling patterns, SIMD Release vs Debug, integrated, background sim, adaptive threading,
projectile), read **`references/benchmarks.md`** (Per-Benchmark Regression Detection Thresholds).

### Step 6: Generate Report

Build the report from the template in **`references/report-template.md`** (full markdown report
+ console summary). Save to `$PROJECT_ROOT/test_results/regression_reports/regression_YYYY-MM-DD.md`.

The **Pathfinding System section is MANDATORY** in every report — never skip it. Before
submitting, run the **Final Report Validation Checklist** in `references/troubleshooting.md`;
if any item is unchecked, extract the missing data first rather than submitting.

---

## Exit Codes

- **0:** All benchmarks passed, no regressions
- **1:** Critical regressions detected (BLOCKING)
- **2:** Warnings detected (review required)
- **3:** Benchmark failed to run (timeout/crash)
- **4:** Baseline creation mode (informational)

## When to Use

Activate automatically when the user says: "check for performance regressions", "run benchmarks",
"test performance", "verify no performance degradation", "compare against baseline". Also use
before merging feature branches, after optimizations, weekly during active development, before
releases, and when modifying AI, collision, pathfinding, or particle systems.

Full suite ~27 minutes + ~2-3 minutes report generation. For timeouts and troubleshooting
(inconsistent results, no baseline, etc.), read **`references/troubleshooting.md`**.
