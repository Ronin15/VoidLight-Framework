# Troubleshooting, Timeouts & Validation Gate

Detail reference for the `voidlight-benchmark-regression` Skill. Loaded on demand.

---

## Timeout Protection

Each benchmark has timeout protection:
- AI Scaling: 600 seconds (10 minutes)
- Others: 300 seconds (5 minutes)

If timeout occurs, flag as potential infinite loop or performance catastrophe (exit code 3).

---

## Final Report Validation Checklist

**⚠️ MANDATORY: Verify BEFORE submitting report to user.**
If ANY checklist item is unchecked, DO NOT submit the report. Extract missing data first.

### Benchmark Execution
- [ ] All discovered benchmarks completed successfully (no timeouts/crashes)
- [ ] AI Scaling: Entity scaling results present with updates/sec metrics
- [ ] **Pathfinding: Async throughput metrics extracted** ← CRITICAL!
- [ ] Collision: SAP/Hash timing and trigger detection data extracted
- [ ] Event Manager: Throughput data extracted
- [ ] Particle Manager: Update timing data extracted
- [ ] GPU Frame Timing: Frame time, swapchain, upload, submit metrics extracted
- [ ] SIMD Performance: Platform detection and speedup data extracted
- [ ] Integrated System: Frame statistics, scaling, coordination overhead extracted
- [ ] Background Simulation: Scaling and adaptive tuning summary extracted
- [ ] Adaptive Threading: MIN_WORKLOAD enforcement (8/8 PASS), threshold learning, hysteresis extracted
- [ ] Projectile Scaling: Entity throughput (entities/ms) and SIMD 4-wide curve extracted

### Report Completeness
- [ ] **Pathfinding System section included in report** ← DO NOT SKIP!
- [ ] AI System: Entity scaling and threading comparison sections present
- [ ] Collision System: Performance table present
- [ ] Event Manager: Metrics table present
- [ ] Particle Manager: Performance data present
- [ ] GPU Frame Timing: Performance data present
- [ ] SIMD System: Platform and speedup table present
- [ ] Integrated System: Frame statistics, scaling summary, coordination overhead present
- [ ] Background Simulation: Scaling table and threading data present
- [ ] Adaptive Threading: MIN_WORKLOAD results, threshold learning per system, hysteresis present
- [ ] Projectile Scaling: Entity scaling table and SIMD 4-wide throughput table present
- [ ] Overall status determined (PASSED/WARNING/FAILED)
- [ ] Regression/improvement analysis complete

### Critical Pathfinding Verification
- [ ] Pathfinding metrics extracted from `test_results/pathfinder_benchmark_current.txt`
- [ ] All 5 distance ranges present (50, 400, 2000, 4000, 8000 units)
- [ ] Success rates reported (must be 100%)
- [ ] Performance comparison against baseline completed
- [ ] Pathfinding section visible in final report

---

## Troubleshooting

**Benchmark timeouts:**
- Possible infinite loop or catastrophic performance regression
- Run individual benchmark with debugging: `gdb ./bin/debug/ai_scaling_benchmark`

**Inconsistent results:**
- System load affecting benchmarks
- Re-run benchmarks in clean environment
- Close other applications
- Check for thermal throttling

**No baseline found:**
- Skill will create baseline from current run
- Subsequent runs will compare against this baseline
- Update baseline after validating improvements
