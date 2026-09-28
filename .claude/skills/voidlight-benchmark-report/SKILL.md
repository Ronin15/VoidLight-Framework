---
name: voidlight-benchmark-report
description: Writes a performance report for VoidLight-Framework from existing benchmark output in test_results/ (per-system tables, baseline deltas, run-to-run history, optional callgrind hotspots) into docs/performance_reports/. Use when the user asks for a performance report, milestone/release performance documentation, or a write-up of optimization results. Does not run benchmarks (use voidlight-benchmark-regression for that); not a per-change or slice-complete gate.
allowed-tools: [Bash, Read, Write, Grep]
---

# VoidLight-Framework Benchmark Report

Turns benchmark output that already exists into a report. It does not run benches; if
results are missing or stale, say so and point the user at the
`voidlight-benchmark-regression` skill (sequential scripts from the repo root).

Read on demand:
- **`../voidlight-benchmark-regression/references/benchmarks.md`** — at step 2: script →
  output file table, ANSI-stripping helper, grep commands per bench, baseline layout.
  Shared with the regression skill; do not duplicate it here.
- **`references/report-template.md`** — at step 4: report sections and console summary.

## Workflow

1. **Inventory.** List what exists and how fresh it is; report only systems with results.
   ```bash
   ls -lt test_results/*_current.txt test_results/pathfinder_benchmark_results.txt \
     test_results/event_scaling_benchmark_output.txt \
     test_results/particle_benchmark_performance_metrics.txt \
     test_results/gpu/gpu_frame_timing_benchmark_*.txt 2>/dev/null
   cat test_results/baseline/baseline_metadata.txt
   ```
   Record per file: mtime, build mode (header line in most outputs), platform. Flag
   results older than the last commit touching the relevant system as possibly stale.

2. **Extract** metrics with the per-bench greps in the shared `benchmarks.md`. Keep AI
   attack workloads (decision pressure, tactical reset, cold burst, cadenced resolve) as
   separate tables. Pathfinding: async only; its throughput has one significant digit, so
   compare completion times instead.

3. **Compare.**
   - vs `test_results/baseline/`: `(current - baseline) / baseline * 100`, inverted for
     lower-is-better; label >5% better Improving, >5% worse Degrading, else Stable; no
     baseline → New. Only compare like build modes.
   - History (optional): timestamped runs such as `ai_scaling_benchmark_<ts>.txt`,
     `collision_scaling_benchmark_<ts>.txt`, `projectile_scaling_benchmark_<ts>.txt` give a
     trend for headline rows. Watch for scope changes (row sets or counts differ) and
     build-mode switches between runs; don't draw a trend across them.
   - With ≥3 runs of the same scope, report mean, stddev and CoV; call out outliers.

4. **Assemble** from `references/report-template.md`. Every number must come from a
   parsed file; omit sections without data rather than filling placeholders.

5. **Hotspots (optional).** If callgrind output exists
   (`test_results/valgrind/callgrind/raw/*.callgrind.out`, summaries in
   `test_results/valgrind/callgrind/summaries/`), add the top functions by Ir for the
   relevant executables with `callgrind_annotate --auto=no <file> | head -40`.

6. **Save** to `docs/performance_reports/performance_report_YYYY-MM-DD.md` and repoint
   `docs/performance_reports/latest_report.md` (symlink) at it. HTML/PDF via `pandoc` only
   if asked. Print the console summary.

## Errors

- No benchmark output at all → stop; tell the user to run the regression skill first.
- Some systems missing → partial report listing the missing systems.
- No baseline → report absolute values plus history only; do not write a baseline.
