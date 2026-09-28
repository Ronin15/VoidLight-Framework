# Troubleshooting & Completeness Checklist

Detail reference for the `voidlight-benchmark-regression` Skill. Loaded on demand.

## Timeouts and Crashes

Script-level timeouts: AI 180s, projectile 120s, particle 300s. Other scripts have none;
treat a run hanging well past its usual duration as a timeout. Any timeout or crash makes
the pass incomplete (exit code 3). Re-run the single executable to reproduce, e.g.
`gdb --args ./bin/debug/ai_scaling_benchmark --log_level=test_suite`.

## Noisy or Inconsistent Results

- Something else was running (other benches, builds, browsers): re-run sequentially on an
  idle machine.
- Thermal throttling or power profile changes: check CPU governor / power mode.
- Build-mode mismatch with the baseline (`baseline_metadata.txt`): not comparable.
- Scope change (new rows, changed counts, workload text changed): check `git log` on the
  bench source before calling it a regression.
- Single outlier row (e.g. Max ≫ Avg in background sim): re-run that bench once before
  reporting; report both runs if it persists.

## No Baseline

Baseline-creation mode for that bench (exit 4 if nothing else to compare). Report the
numbers as the new reference; copy into `test_results/baseline/` only when the user
agrees, and update `baseline_metadata.txt`.

## Completeness Checklist (before handing over the report)

- [ ] All 11 scripts ran, or each missing one has an explicit blocker (e.g. GPU not built /
      no display)
- [ ] Build mode recorded per script; baseline source and date recorded
- [ ] AI: entity scaling + all four attack tables reported separately
- [ ] Pathfinding: async `Completed` and completion time for every batch size (no
      immediate-path timings)
- [ ] Adaptive threading: MIN_WORKLOAD 8/8, `Validation:` lines, learned thresholds
- [ ] Integrated: frame stats, scaling summary, max sustainable Active NPCs, coordination
      overhead, sustained degradation
- [ ] Projectile: entity scaling + SIMD 4-wide table
- [ ] Collision, event, particle, GPU, SIMD, background sim metrics present
- [ ] Overall status (PASSED / WARNING / FAILED / BASELINE CREATED) stated
