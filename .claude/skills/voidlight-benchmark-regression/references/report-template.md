# Regression Report Template

Full report structure for the `voidlight-benchmark-regression` Skill. Loaded on demand
when generating the Step 6 report. Example numbers are platform-specific illustrations —
substitute actual extracted/baseline values.

The Pathfinding System section is MANDATORY in every report. Do not omit it.

---

## Report Structure

```markdown
# VoidLight-Framework Performance Regression Report
**Date:** YYYY-MM-DD HH:MM:SS
**Branch:** <current-branch>
**Baseline:** <baseline-date or "New Baseline Created">
**Total Benchmark Time:** <duration>

---

## 🎯 Overall Status: <PASSED/FAILED/WARNING>

<summary-of-regressions>

---

## 📊 Performance Summary

### AI System - Entity Scaling (EDM Architecture)

**Purpose:** Tests AIManager performance with production behaviors

| Entities | Baseline | Current | Change | Threading | Status |
|----------|----------|---------|--------|-----------|--------|
| 100 | 5.0M/s | 4.8M/s | -4.0% | single | ⚪ Stable |
| 500 | 7.8M/s | 7.7M/s | -1.3% | single | ⚪ Stable |
| 1000 | 11.0M/s | 10.8M/s | -1.8% | single | ⚪ Stable |
| 2000 | 15.0M/s | 14.0M/s | -6.7% | single | 🟡 Minor |
| 5000 | 100M/s | 100M/s | +0.0% | multi | ⚪ Stable |
| 10000 | 200M/s | 207M/s | +3.5% | multi | 🟢 Improved |

**Status:** ⚪ **STABLE**
- All metrics within acceptable variance
- EDM architecture achieves ~200M+ updates/sec at 10K entities
- Threading beneficial above ~5K entities

**Threading Mode Comparison:**
| Entities | Single (ms) | Multi (ms) | Speedup |
|----------|-------------|------------|---------|
| 500 | 0.10 | 0.10 | 1.0x |
| 1000 | 0.18 | 0.17 | 1.1x |
| 2000 | 0.35 | 0.34 | 1.0x |
| 5000 | 0.86 | 0.30 | 2.9x |

**Note:** With EDM architecture, single-threaded processing is so fast that threading
overhead only becomes beneficial at higher entity counts (~5K+).

---

### Collision Scaling System

| Scenario | Baseline (ms) | Current (ms) | Change | Throughput | Status |
|----------|---------------|--------------|--------|------------|--------|
| MM 1000 movables | 0.25 | 0.22 | -12% | 4596/ms | 🟢 Improvement |
| MM 5000 movables | 1.20 | 1.11 | -8% | 4509/ms | 🟢 Improvement |
| MM 10000 movables | 2.50 | 2.26 | -10% | 4434/ms | 🟢 Improvement |
| MS 10K statics | 0.16 | 0.15 | -6% | FLAT | ⚪ Stable |
| MS 20K statics | 0.16 | 0.15 | -6% | FLAT | ⚪ Stable |
| Combined XL (6K) | 0.70 | 0.65 | -7% | N/A | 🟢 Improvement |
| Combined XXL (12K) | 1.40 | 1.31 | -6% | N/A | 🟢 Improvement |

**Status:** 🟢 **IMPROVEMENT**
- SAP (Sweep-and-Prune) for MM: O(n log n) scaling confirmed up to 10K movables
- Spatial Hash for MS: O(n) scaling confirmed - time stays FLAT from 100 to 20K statics
- Combined: Sub-quadratic scaling verified up to 12K entities

---

### Trigger Detection System (EventOnly Triggers)

**Purpose:** Tests detection of EventOnly triggers (water, area markers, etc.) by entities with NEEDS_TRIGGER_DETECTION flag.

| Detectors | Triggers | Baseline (ms) | Current (ms) | Change | Method | Status |
|-----------|----------|---------------|--------------|--------|--------|--------|
| 1 (Player) | 100 | 0.15 | 0.14 | -7% | spatial | ⚪ Stable |
| 1 (Player) | 400 | 0.15 | 0.14 | -7% | spatial | ⚪ Stable |
| 10 (NPCs) | 200 | 0.16 | 0.15 | -6% | spatial | ⚪ Stable |
| 25 (NPCs) | 200 | 0.16 | 0.15 | -6% | spatial | ⚪ Stable |
| 50 (threshold) | 200 | 0.25 | 0.23 | -8% | sweep | ⚪ Stable |
| 100 (NPCs) | 200 | 0.28 | 0.26 | -7% | sweep | ⚪ Stable |
| 200 (NPCs) | 400 | 0.45 | 0.43 | -4% | sweep | ⚪ Stable |

**Status:** ⚪ **STABLE**
- Adaptive strategy working correctly (spatial <50, sweep >=50)
- Performance within targets (<0.5ms for typical scenarios)
- Flag-based filtering eliminates unnecessary AABB tests

**Notes:**
- Only entities with NEEDS_TRIGGER_DETECTION flag are processed
- Player has flag by default; NPCs can opt-in via setTriggerDetection(true)
- Replaced O(movables × triggers) brute-force with adaptive O(N × k) or O((N+T) log (N+T))

---

### Pathfinding System **[ALWAYS INCLUDE - CRITICAL]**

**⚠️ IMPORTANT:** This section is MANDATORY in all regression reports. Pathfinding performance directly impacts integrated AI benchmarks.

| Distance (units) | Baseline Time | Current Time | Change | Path Nodes | Success Rate | Status |
|------------------|---------------|--------------|--------|------------|--------------|--------|
| 50 (Short) | 0.048 ms | 0.024 ms | -50.0% | 1 | 100% | 🟢 Major Improvement |
| 400 (Medium) | 0.259 ms | 0.049 ms | -81.1% | 3 | 100% | 🟢 Major Improvement |
| 2000 (Long) | 0.502 ms | 0.052 ms | -89.6% | 6 | 100% | 🟢 Major Improvement |
| 4000 (Very Long) | 0.756 ms | 0.128 ms | -83.1% | 10 | 100% | 🟢 Major Improvement |
| 8000 (Extreme) | N/A | 0.349 ms | N/A | 20 | 100% | 🟢 Excellent |

**Status:** [Determine based on actual results]
- Path calculation performance across all distance ranges
- Success rate (must be 100% - failures are critical regressions)
- Path quality (nodes explored should be reasonable)
- A* algorithm and cache effectiveness

**Template Notes:**
- Always show ALL distance ranges (50, 400, 2000, 4000, 8000 units)
- Include success rate for each distance (failures = critical regression)
- Note path quality (average nodes should be optimal)
- Highlight major improvements or regressions
- Cross-reference with integrated AI benchmark if pathfinding impacts it

---

### Event Manager

**Deferred Event Throughput (single enqueue + FIFO drain):**

| Config | Baseline (ev/s) | Current (ev/s) | Change | Status |
|--------|-----------------|-----------------|--------|--------|
| 10 events, 1 handler | 163K | 163K | 0% | ⚪ Stable |
| 50 events, 3 handlers | 145K | 145K | 0% | ⚪ Stable |
| 100 events, 4 handlers | 189K | 189K | 0% | ⚪ Stable |
| 200 events, 5 handlers | 270K | 270K | 0% | ⚪ Stable |

**Concurrency & Enqueue Methods:**

| Metric | Baseline | Current | Change | Status |
|--------|----------|---------|--------|--------|
| Concurrent (23 threads, 4K events) | 487K ev/s | 487K ev/s | 0% | ⚪ Stable |
| Batch enqueue (no alloc) | 7.9M ev/s | 7.9M ev/s | 0% | ⚪ Stable |
| Single enqueue (no alloc) | 1.8M ev/s | 1.8M ev/s | 0% | ⚪ Stable |
| Threading threshold | Single preferred | Single preferred | - | ⚪ Stable |

**Status:** ⚪ **STABLE**
- Batch enqueue 4-5x faster than single enqueue (lock reduction)
- WorkerBudget correctly prefers single-threaded at all tested counts

---

### Particle Manager

| Metric | Baseline | Current | Change | Status |
|--------|----------|---------|--------|--------|
| Particles/frame | 5000 | 5000 | 0.0% | ⚪ Stable |
| Render Time | 3.2ms | 3.1ms | -3.1% | ⚪ Stable |
| Batch Count | 12 | 11 | -8.3% | 🟢 Improvement |
| Culling Efficiency | 88% | 90% | +2.3% | 🟢 Improvement |

**Status:** 🟢 **IMPROVEMENT**
- Better batching efficiency
- Improved culling

---

### GPU Frame Timing System

**Purpose:** Tests GPU rendering pipeline performance (Vulkan/SPIR-V)

| Metric | Baseline | Current | Change | Status |
|--------|----------|---------|--------|--------|
| Avg Frame Time | 8.343ms | 8.343ms | 0.0% | ⚪ Stable |
| Avg Swapchain | 8.039ms | 8.039ms | 0.0% | ⚪ Stable |
| Avg GPU Upload | 0.001ms | 0.001ms | 0.0% | ⚪ Stable |
| Avg GPU Submit | 0.045ms | 0.045ms | 0.0% | ⚪ Stable |

**Status:** ⚪ **STABLE**
**Note:** Run from desktop session for meaningful VSync timings.

---

### SIMD Performance System

**Purpose:** Validates cross-platform SIMD optimizations deliver claimed speedups

| Operation | Platform | SIMD (ms) | Scalar (ms) | Speedup | Status |
|-----------|----------|-----------|-------------|---------|--------|
| AI Distance | NEON | 12.3 | 45.7 | 3.71x | 🟢 Excellent |
| Collision Bounds | NEON | 8.5 | 9.2 | 1.08x | ⚪ Stable |
| Layer Mask Filter | NEON | 5.1 | 4.8 | 0.94x | ⚪ Compiler auto-vec |
| Particle Physics | NEON | 10.2 | 38.4 | 3.76x | 🟢 Excellent |

**Status:** 🟢 **OPERATIONAL**
- SIMD detected and active (not scalar fallback)
- Key operations (AI Distance, Particle Physics) showing 3-4x speedups
- Auto-vectorization competitive for simple patterns (bounds, layer mask)

---

### Integrated System Performance

**Purpose:** Tests all managers under combined realistic load at 60 FPS target

| Scenario | Entities | Avg (ms) | P95 (ms) | Drops % | Status |
|----------|----------|----------|----------|---------|--------|
| Realistic (10K AI + 5K particles) | 15K | 10.34 | 14.56 | 3.8% | ⚪ Stable |
| Scaling 1K | 1K | 2.15 | 3.21 | 0.0% | ⚪ Stable |
| Scaling 5K | 5K | 5.82 | 8.45 | 1.2% | ⚪ Stable |
| Scaling 10K | 10K | 10.34 | 14.56 | 3.8% | ⚪ Stable |
| Scaling 15K | 15K | 16.23 | 22.34 | 8.5% | 🟡 MINOR |
| Scaling 20K | 20K | 24.56 | 35.67 | 18.2% | 🟠 WARNING |

**Max Sustainable @ 60 FPS:** 10,000 entities
**Coordination Overhead:** 1.2ms (< 2ms target) ✓
**Sustained Performance:** <5% degradation over 50s ✓

---

### Background Simulation System

**Purpose:** Tests background tier entity processing for tier-culled entities

| Entities | Baseline (ms) | Current (ms) | Change | Threaded | Batches | Status |
|----------|---------------|--------------|--------|----------|---------|--------|
| 100 | 0.003 | 0.003 | 0% | no | 1 | ⚪ Stable |
| 500 | 0.011 | 0.011 | 0% | no | 1 | ⚪ Stable |
| 1000 | 0.022 | 0.022 | 0% | no | 1 | ⚪ Stable |
| 5000 | 0.115 | 0.115 | 0% | no | 1 | ⚪ Stable |
| 10000 | 0.204 | 0.204 | 0% | no | 1 | ⚪ Stable |

**Status:** ⚪ **STABLE**
- Single-threaded throughput: ~50K items/ms (threading overhead not beneficial)
- Sub-linear scaling maintained
- WorkerBudget adaptive tuning: PASS

---

### Adaptive Threading Analysis (WorkerBudget Validation)

**Purpose:** Validates WorkerBudgetManager adaptive logic using Collision system

**Throughput Learning:**
| Metric | Baseline | Current | Change | Status |
|--------|----------|---------|--------|--------|
| Single TP (items/ms) | 588 | 617 | +5% | ⚪ Stable |
| Multi TP (items/ms) | 5018 | 4970 | -1% | ⚪ Stable |
| Batch Multiplier | 1.00 | 1.00 | 0% | ⚪ Stable |
| Multi Speedup | 8.5x | 8.0x | -6% | ⚪ Stable |

**Mode Switching Validation:**
| Test | Expected | Actual | Status |
|------|----------|--------|--------|
| 50 entities (below MIN_WORKLOAD=100) | SINGLE | SINGLE | ✓ PASS |
| 99 entities (below MIN_WORKLOAD=100) | SINGLE | SINGLE | ✓ PASS |
| 2000 entities (high count) | MULTI | MULTI | ✓ PASS |
| Bidirectional switching | Scale up→MULTI, down→SINGLE | Correct | ✓ PASS |

**Gradual Scale Down (Natural Crossover Detection):**
| Entities | Mode | Single TP | Multi TP | Ratio |
|----------|------|-----------|----------|-------|
| 1500 | MULTI | 2538 | 4541 | 1.79x |
| 500 | MULTI | 2278 | 4739 | 2.08x |
| 200 | MULTI | 2067 | 3384 | 1.64x |
| 125 | MULTI | 1883 | 2173 | 1.15x |
| 100 | SINGLE | 3591 | 1596 | 0.44x |

**Natural Crossover Point:** At MIN_WORKLOAD boundary (100 entities) - MULTI preferred above

**Status:** ⚪ **STABLE**
- Throughput learning: PASS (non-zero values after 1000 frames)
- Mode selection: PASS (correct mode based on throughput comparison)
- Batch multiplier: PASS (within [0.4, 2.0] range, stabilized)
- Bidirectional switching: PASS (SINGLE→MULTI on scale up, MULTI→SINGLE on scale down)

---

## 🚨 Critical Issues (BLOCKING)

1. **AI System FPS Below Threshold**
   - Current: 56.8 FPS (Target: 60+)
   - Regression: -8.8%
   - **Action Required:** Must fix before merge

---

## ⚠️ Warnings (Review Required)

1. **AI System CPU Usage Increase**
   - Current: 6.4% (Target: <6%)
   - Regression: +10.3%

2. **AI Update Time Increase**
   - Current: 13.9ms (Baseline: 12.4ms)
   - Regression: +12.1%

---

## 📈 Improvements

1. **Collision System Performance**
   - Query time improved 12.5%
   - Collision checks/sec improved 7.2%

2. **Particle Manager Batching**
   - Batch count reduced 8.3% (better efficiency)
   - Culling efficiency improved 2.3%

---

## 🔍 Detailed Analysis

### Performance Hotspots (if callgrind data available)

<parse callgrind reports from test_results/valgrind/callgrind/>

Top Functions by Time:
1. AIManager::updateBehaviors - 45% (up from 38% - REGRESSION)
2. CollisionManager::detectCollisions - 18% (down from 22% - IMPROVEMENT)
3. PathfinderManager::calculatePath - 12% (stable)

---

## 📋 Recommendations

### Immediate Actions (Critical)
1. Investigate AI System performance regression
2. Profile AIManager::updateBehaviors with valgrind/callgrind
3. Review commits since baseline for AI changes
4. Do not merge until FPS ≥60 restored

### Short-term Actions (Warnings)
1. Monitor Event Manager dispatch latency
2. Consider AI batch size optimization
3. Review recent AI behavior changes

### Long-term Actions (Optimization)
1. Apply collision system improvements to other managers
2. Document particle manager batching technique
3. Consider updating baseline after AI fixes validated

---

## 📁 Files

**Baseline:** `$PROJECT_ROOT/test_results/baseline/*.txt`
**Current Results:** `$PROJECT_ROOT/test_results/*/performance_metrics.txt`
**Callgrind Reports:** `$PROJECT_ROOT/test_results/valgrind/callgrind/` (if available)
**Full Report:** `$PROJECT_ROOT/test_results/regression_reports/regression_YYYY-MM-DD.md`

---

## ✅ Next Steps

- [ ] Fix AI System FPS regression (BLOCKING)
- [ ] Verify fixes with re-run: `claude run benchmark regression check`
- [ ] Update baseline after validation: `claude update performance baseline`
- [ ] Document optimization techniques from collision improvements

---

**Generated by:** voidlight-benchmark-regression Skill
**Report saved to:** $PROJECT_ROOT/test_results/regression_reports/regression_YYYY-MM-DD.md
```

---

## Console Summary

```
=== Performance Regression Check ===

Status: 🔴 REGRESSION DETECTED (BLOCKING)

Critical Issues:
  🔴 AI System FPS: 56.8 (target: 60+) - 8.8% regression

Warnings:
  🟠 AI CPU Usage: 6.4% (target: <6%) - 10.3% increase
  🟡 Event Dispatch Latency: +8.3%

Improvements:
  🟢 Collision Query Time: -12.5%
  🟢 Particle Batching: -8.3%

Total Benchmark Time: 21m 15s

❌ DO NOT MERGE - Fix AI regression first

Full Report: $PROJECT_ROOT/test_results/regression_reports/regression_2025-01-15.md
```
