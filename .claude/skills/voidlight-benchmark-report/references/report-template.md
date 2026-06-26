# Benchmark Report Template & Scaffolding

This file holds the full report-output template, section scaffolding, comparison
tables, appendices, and chart/sparkline helpers for the
`voidlight-benchmark-report` skill. Read it on demand when assembling a report;
the lean `SKILL.md` points here.

> **IMPORTANT — illustrative scaffolding, not real data.** Every metric, table
> value, FPS number, hotspot percentage, trend line, and date below is a
> **fabricated example** kept as template scaffolding so the report shape is
> clear. When generating an actual report, replace ALL of these with values
> parsed from real benchmark results discovered at runtime (see `SKILL.md`).
> Only report systems that actually produced results.

---

## Illustrative parse structures (example numbers only)

When parsing AI results into an intermediate structure, the shape looks like the
following. **The numbers are made up** — substitute measured values.

```
AI_System_Synthetic:
  Entity_100: 170000
  Entity_200: 750000
  Entity_1000: 975000
  Entity_5000: 925000
  Entity_10000: 995000

AI_System_Integrated:
  Entity_100: 569152
  Entity_200: 579794
  Entity_500: 611098
  Entity_1000: 1192606
  Entity_2000: 1587491
```

### Example callgrind hotspot data (illustrative)

```
Function                               Ir        %
AIManager::updateBehaviors()           15,234M   45.2%
CollisionManager::detectCollisions()    6,123M   18.1%
PathfinderManager::calculatePath()      4,056M   12.0%
```

---

## Report Section Templates

Assemble these sections into the final markdown report. Keep the structure;
replace all illustrative values with parsed data.

### Section 1: Executive Summary

```markdown
# SDL3 VoidLight-Framework Performance Report

**Generated:** YYYY-MM-DD HH:MM:SS
**Benchmark Suite Version:** <git-commit-hash>
**Branch:** <current-branch>
**Platform:** <OS-version>
**Build Type:** Debug/Release

---

## Executive Summary

### Overall Performance: OK EXCELLENT / OK GOOD / WARN FAIR / FAIL NEEDS IMPROVEMENT

SDL3 VoidLight-Framework demonstrates <strong/adequate/weak> performance across all critical systems:

- **AI System:** Handles 10,000+ entities at 62 FPS with 5.8% CPU (Target: 60+ FPS, <6% CPU)
- **Collision System:** 125,000 collision checks/sec, 0.08ms query time
- **Pathfinding:** 8.5ms path calculation, 78% cache hit rate
- **Event System:** 8,500 events/sec throughput, 0.12ms dispatch latency
- **Particle System:** 5,000 particles/frame, 3.2ms render time
- **UI System:** 1,000 components, 4.5ms render time

### Key Achievements

- AI system exceeds 10K entity target with headroom
- Collision system optimization improved query speed by 12%
- Event throughput supports 8K+ concurrent events
- All systems meet or exceed performance targets

### Areas for Improvement

- Event dispatch latency increased 8% (monitor)
- AI behavior updates showing slight variance (9% stddev)

### Recommendation

**Status:** Ready for production
**Next Milestones:** Optimize event dispatching, stabilize AI behavior timing
```

### Section 2: Detailed System Analysis

```markdown
## Detailed Performance Analysis

### AI System - Synthetic Benchmarks (Infrastructure Performance)

#### Purpose
Tests AIManager infrastructure without integration overhead

#### Benchmark Results

| Entity Count | Value (updates/sec) | Status | Baseline | Change |
|--------------|---------------------|--------|----------|--------|
| 100 | 170K | OK | 170K | 0.0% |
| 200 | 750K | OK | 750K | 0.0% |
| 1000 | 975K | OK | 975K | 0.0% |
| 5000 | 925K | OK | 925K | 0.0% |
| 10000 | 995K | OK | 995K | 0.0% |

#### Threading Efficiency
- Single-threaded (100): 170K updates/sec
- Multi-threaded (5000): 925K updates/sec
- Speedup: 5.4x

#### Statistical Summary
- Mean: 963K updates/sec (across entity counts)
- Std Dev: 138K (14% CoV)
- Consistent performance across entity scales

---

### AI System - Integrated Benchmarks (Production Workload)

#### Purpose
Tests AIManager with PathfinderManager/CollisionManager integration

#### Benchmark Results

| Entity Count | Value (updates/sec) | Status | Baseline | Change |
|--------------|---------------------|--------|----------|--------|
| 100 | 569K | OK | 569K | 0.0% |
| 200 | 580K | OK | 580K | 0.0% |
| 500 | 611K | OK | 611K | 0.0% |
| 1000 | 1193K | OK | 1193K | 0.0% |
| 2000 | 1587K | OK | 1587K | 0.0% |

#### Threading Efficiency
- Single-threaded (100): 569K updates/sec
- Multi-threaded (2000): 1587K updates/sec
- Speedup: 2.8x

#### Statistical Summary
- Mean: 908K updates/sec (across entity counts)
- Std Dev: 444K (49% CoV)
- Performance scales with entity count

---

### AI System - Integration Overhead Analysis

#### Overhead Metrics

| Entity Count | Synthetic | Integrated | Overhead | Assessment |
|--------------|-----------|------------|----------|------------|
| 100 | 170K/s | 569K/s | -70% | Data inconsistency* |
| 200 | 750K/s | 580K/s | +23% | Expected |
| 1000 | 975K/s | 1193K/s | -22% | Data inconsistency* |
| 2000 | N/A | 1587K/s | N/A | N/A |

*Note: Negative overhead indicates synthetic values are estimates while integrated are measured.
Expected overhead: 20-40% (integrated slower due to PathfinderManager)

#### Overhead Sources
- PathfinderManager: Path requests, cache lookups, A* computation
- CollisionManager: Spatial hash queries for neighbors
- Production behaviors: Complex state machines and calculations

**Stability Analysis:**
- FPS variance low (2.9% CoV) - excellent stability
- CPU usage consistent (0.3% stddev)
- Update time predictable (<1ms variance)

#### Performance Profile (Callgrind Hotspots)

Top functions by instruction reads:
1. `AIManager::updateBehaviors()` - 45.2% (expected, main update loop)
2. `AIManager::processBatch()` - 12.3% (batch processing)
3. `BehaviorCache::lookup()` - 8.7% (cache lookups)
4. `ChaseBehavior::executeLogic()` - 6.1% (behavior logic)
5. `PathfinderManager::requestPath()` - 4.2% (pathfinding integration)

**Analysis:** Hotspot distribution is as expected. Most time in core update loop.

#### Trend Analysis

Performance trend over time (vs historical baselines):

```
FPS History:
Jan 2025:  62.3 FPS <- Current
Dec 2024:  61.8 FPS (+0.8% improvement)
Nov 2024:  59.2 FPS (+5.2% improvement)
```

**Trend:** Improving steadily

#### Recommendations

1. **Maintain current performance** - AI system exceeds targets
2. **Monitor behavior update variance** - Consider additional caching
3. **Document batch processing optimization** - Apply pattern to other systems
```

**Repeat similar detailed analysis for:** Collision System, Pathfinding System,
Event Manager, Particle Manager, UI System.

### Section 3: Cross-System Analysis

```markdown
## Cross-System Performance Comparison

### Frame Budget Analysis (60 FPS = 16.67ms budget)

| System | Time (ms) | % Budget | Status |
|--------|-----------|----------|--------|
| AI Update | 12.4 | 74.4% | OK |
| Collision Detection | 2.8 | 16.8% | OK |
| Pathfinding | 1.2 | 7.2% | OK |
| Event Processing | 0.5 | 3.0% | OK |
| Particle Update | 0.8 | 4.8% | OK |
| UI Rendering | 4.5 | 27.0% | OK |
| **Total** | **22.2** | **133.3%** | WARN |

**Note:** Total exceeds 100% because systems run on separate threads (update vs render).

**Update Thread Budget (60 FPS = 16.67ms):**
- AI: 12.4ms
- Collision: 2.8ms
- Pathfinding: 1.2ms
- Event: 0.5ms
- **Total Update:** 16.9ms (101% of budget) WARN Slight overrun

**Render Thread Budget:**
- Particle Render: 0.8ms
- UI Render: 4.5ms
- World Render: 3.2ms
- **Total Render:** 8.5ms (51% of budget) OK Plenty of headroom

### System Interaction Analysis

**AI <-> Pathfinding:**
- Pathfinding requests/frame: 15
- Average latency: 8.5ms
- Cache hit rate: 78%
- Integration efficient

**Collision <-> Pathfinding:**
- Dynamic obstacle updates: 50/frame
- Pathfinding weight adjustments: 12/frame
- Integration smooth

**Event <-> All Systems:**
- Event throughput: 8,500 events/sec
- Dispatch latency: 0.12ms
- Queue depth: 128 events
- No bottlenecks detected

### Resource Usage Summary

**CPU Usage by System:**
```
AI Manager:         5.8%
Collision Manager:  2.3%
Pathfinder:         1.2%
Event Manager:      0.8%
Particle Manager:   1.5%
UI Manager:         0.9%
Total Engine:      12.5%
```

**Memory Usage:**
```
AI Manager:         45 MB
Collision Manager:  32 MB
Pathfinder:         18 MB
Event Manager:       8 MB
Particle Manager:   12 MB
UI Manager:         15 MB
Total Engine:      130 MB
```
```

### Section 4: Optimization Opportunities

```markdown
## Optimization Opportunities

### High Priority

1. **Update Thread Frame Budget Overrun**
   - Current: 16.9ms (101% of 16.67ms budget)
   - Impact: Occasional frame drops
   - Recommendation: Reduce AI update time by 0.5ms
   - Approach: Increase batch size or optimize behavior cache

2. **Event Dispatch Latency Increase**
   - Current: 0.12ms (up 8% from baseline)
   - Impact: Slight event processing delay
   - Recommendation: Profile event dispatch path
   - Approach: Reduce lock contention or optimize event routing

### Medium Priority

3. **Pathfinding Cache Hit Rate**
   - Current: 78%
   - Target: 85%+
   - Recommendation: Increase cache size or improve eviction policy
   - Expected Improvement: Reduce path calculation time by ~15%

4. **Particle Culling Efficiency**
   - Current: 88%
   - Target: 95%+
   - Recommendation: Improve camera frustum culling
   - Expected Improvement: Reduce render time by ~10%

### Low Priority

5. **UI Component Render Time**
   - Current: 4.5ms (stable, within budget)
   - Opportunity: Apply batching technique from particle system
   - Expected Improvement: Reduce to 3.5ms (~22% faster)
```

### Section 5: Historical Trends

```markdown
## Performance Trends

### AI System FPS Over Time

```
68 |
66 |          .-
64 |        .-'
62 |    .---'   <- Current (62.3)
60 |  .-'
58 |.-'
56 +-------------------------
   Nov  Dec  Jan
   2024 2024 2025
```

**Trend:** Improving (+5.2% over 3 months)

### Collision System Query Time

```
0.12 |.
0.10 | '.
0.08 |   '-------.  <- Current (0.08ms)
0.06 |           '-
0.04 +-------------------------
     Nov  Dec  Jan
     2024 2024 2025
```

**Trend:** Improving (-33% over 3 months)

### Event Throughput

```
9000 |      .---  <- Current (8500/sec)
8500 |    .-'
8000 |  .-'
7500 |.-'
7000 +-------------------------
     Nov  Dec  Jan
     2024 2024 2025
```

**Trend:** Improving (+21% over 3 months)
```

### Section 6: Comparative Analysis

```markdown
## Comparative Analysis

### Performance vs Industry Standards

| System | VoidLight-Framework | Industry Avg | Status |
|--------|--------------|--------------|--------|
| Entity Count @ 60 FPS | 10,000 | 5,000-8,000 | Above Avg |
| Collision Checks/sec | 125,000 | 80,000-100,000 | Above Avg |
| Event Throughput | 8,500/sec | 5,000-10,000 | Average |
| Memory/Entity | 13 KB | 10-20 KB | Average |

**Overall:** VoidLight-Framework performs above industry averages for 2D game engines.

### Performance vs Project Goals

| Goal | Target | Current | Status |
|------|--------|---------|--------|
| 10K+ Entities @ 60 FPS | 60 FPS | 62.3 FPS | Exceeded |
| AI CPU Usage | <6% | 5.8% | Met |
| Event Throughput | 10K/sec | 8.5K/sec | 85% of goal |
| Collision Efficiency | N/A | 94.2% hash | Excellent |
| Pathfinding Speed | <10ms | 8.5ms | Met |

**Overall Progress:** 80% of goals met or exceeded
```

### Section 7: Technical Details

```markdown
## Technical Details

### Test Environment

- **Hardware:** <CPU-model>, <RAM-size>
- **OS:** Linux 6.16.4 (Bazzite Fedora 42)
- **Compiler:** GCC/Clang <version>, C++20
- **Build Flags:** -O3 -flto -march=x86-64-v3 -mavx2
- **SDL Version:** SDL3 (latest)

### Benchmark Methodology

- **Duration:** 20 minutes total
- **Repetitions:** 5 runs per benchmark (median reported)
- **Warm-up:** 30 seconds per test
- **Isolation:** Tests run sequentially, system idle
- **Profiling:** Callgrind with 1% sampling

### Data Collection

- **Metrics Collection:** Automated via test scripts
- **Storage:** $PROJECT_ROOT/test_results/ directory
- **Baseline:** Updated monthly
- **History:** 6 months retained

### Reliability

- **FPS Variance:** 2.9% CoV (excellent)
- **CPU Variance:** 5.1% CoV (good)
- **Memory Variance:** 1.2% CoV (excellent)

**Overall:** Results are highly reliable and reproducible.
```

### Section 8: Appendices

```markdown
## Appendix A: Raw Metrics

### AI System Benchmark (Raw Data)

```
Entities: 10000
FPS: 62.3
CPU: 5.8%
Update Time: 12.4ms
Batch Processing: 2.1ms
Behavior Updates: 8.3ms
Memory Usage: 45 MB
Thread Safety: Mutex-protected
Double Buffer: Enabled
Cache Efficiency: 92%
```

<Include raw data for all systems>

## Appendix B: Callgrind Full Output

```
<Full callgrind_annotate output if available>
```

## Appendix C: Test Scripts

All benchmark tests are located in `tests/test_scripts/`. Enumerate the current set rather than hardcoding:
```bash
ls tests/test_scripts/run_*benchmark*.sh tests/test_scripts/run_*analysis*.sh
```
Examples (verify they still exist):
- `tests/test_scripts/run_ai_benchmark.sh`
- `tests/test_scripts/run_collision_scaling_benchmark.sh`
- `tests/test_scripts/run_pathfinder_benchmark.sh`

Run full suite:
```bash
./tests/test_scripts/run_all_tests.sh --benchmarks-only
```

## Appendix D: Baseline History

| Date | AI FPS | Collision Checks | Event Throughput |
|------|--------|------------------|------------------|
| 2025-01-15 | 62.3 | 125,000 | 8,500 |
| 2024-12-15 | 61.8 | 120,000 | 8,200 |
| 2024-11-15 | 59.2 | 110,000 | 7,000 |

<Full historical baseline data>
```

---

## Console Summary Template

Emit a console summary after writing the report. **All values illustrative.**

```
=== VoidLight-Framework Benchmark Report Generated ===

Report Date: 2025-01-15 14:30:22
Benchmarks Analyzed: 6 systems
Metrics Collected: 42 data points
Baseline Comparison: Available (2024-12-15)

Performance Status: EXCELLENT

Key Highlights:
  AI System: 10,000 entities @ 62.3 FPS
  Collision: 125,000 checks/sec
  Pathfinding: 8.5ms avg calculation
  Event Dispatch: +8% latency (monitor)

Report Generated:
  Markdown: docs/performance_reports/performance_report_2025-01-15.md
  HTML: docs/performance_reports/performance_report_2025-01-15.html
  PDF: docs/performance_reports/performance_report_2025-01-15.pdf

Report Size: 2.3 MB (includes charts and raw data)
Generation Time: 2m 15s

Next Steps:
  1. Review optimization opportunities (Section 4)
  2. Address update thread budget overrun
  3. Monitor event dispatch latency trend
  4. Update baseline after validating improvements

---
Report ready for distribution
```

---

## Chart & Visualization Helpers

### ASCII Charts

Generate simple ASCII charts for trends:

```
FPS Trend:
68 |          .-
66 |        .-'
64 |      .-'
62 |    .-'
60 |  .-'
58 |.-'
   +-------------
```

### Sparklines

Compact trend indicators (use Unicode block glyphs):
- AI FPS: low-to-high blocks (improving)
- Collision: high-to-low blocks (degrading)

### Color Coding (in HTML/PDF)

- Green: Exceeds targets
- Yellow: Meets targets
- Orange: Below targets (warning)
- Red: Critical issues
