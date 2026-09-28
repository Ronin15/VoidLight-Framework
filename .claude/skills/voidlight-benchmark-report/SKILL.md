---
name: voidlight-benchmark-report
description: Generates professional performance analysis reports from SDL3 VoidLight-Framework benchmark results including statistical analysis, comparison tables, visualizations, and recommendations. Use when preparing performance documentation, analyzing optimization efforts, or generating milestone/release reports.
allowed-tools: [Bash, Read, Write, Grep]
---

# VoidLight-Framework Benchmark Report Generator

A lean playbook for turning SDL3 VoidLight-Framework benchmark results into a
professional performance report. Discover what benchmarks actually ran, parse
their metrics, run light statistics, then assemble the report from the
on-demand template.

**The full report template (sections, comparison tables, appendices, example
tables, charts) lives in `references/report-template.md` — read it when you
reach the assembly step. Do NOT inline it here.**

## When to Use

Activate when the user asks to:
- "generate performance report" / "create benchmark report"
- "document current performance" / "prepare performance analysis"
- "make stakeholder report"
- Document optimization efforts, prep a milestone/release, or do monthly tracking.

## Input Sources

- **Benchmark results:** `$PROJECT_ROOT/test_results/` (set `PROJECT_ROOT` to the
  repo root). Each system writes `performance_metrics.txt` / `performance_report.md`
  under a per-system subdir; profiling lands in `test_results/valgrind/`.
- **Historical baselines (optional):** `$PROJECT_ROOT/test_results/baseline/` and
  `$PROJECT_ROOT/test_results/baseline_history/`.

## Workflow

### Step 1: Discover available benchmark results (runtime, not hardcoded)

The suite changes as systems are added. **Enumerate the real benchmark set and
only report systems that actually produced results:**
```bash
find "$PROJECT_ROOT/test_results/" -name "performance_metrics.txt" -type f
ls "$PROJECT_ROOT"/bin/debug/*_benchmark "$PROJECT_ROOT"/bin/debug/*_analysis 2>/dev/null
ls "$PROJECT_ROOT"/tests/test_scripts/run_*benchmark*.sh "$PROJECT_ROOT"/tests/test_scripts/run_*analysis*.sh 2>/dev/null
```

**Common benchmarks** (reconcile against the discovery output above; only report
systems that actually produced results):
- AI Scaling Benchmark (`ai_scaling_benchmark`)
- Collision Scaling Benchmark (`collision_scaling_benchmark`)
- Pathfinder Benchmark (`pathfinder_benchmark`)
- Event Manager Scaling (`event_manager_scaling_benchmark`)
- Particle Manager Benchmark (`particle_manager_performance_tests`)
- GPU Frame Timing (`gpu_frame_timing_benchmark`)
- SIMD Performance (`simd_performance_benchmark`)
- Integrated System (`integrated_system_benchmark`)
- Background Simulation (`background_simulation_manager_benchmark`)
- Adaptive Threading Analysis (`adaptive_threading_analysis`)
- Projectile Scaling (`projectile_scaling_benchmark`)

For each result found: record the timestamp (file mtime), extract metrics, and
categorize by system.

### Step 2: Extract and parse metrics

Use targeted greps per system. The AI system has a dual benchmark set (synthetic
infrastructure vs. integrated production behaviors):
```bash
# AI synthetic (infrastructure)
grep -B 5 -A 10 "TestSynthetic" "$PROJECT_ROOT/test_results/ai_scaling_benchmark_"*.txt | \
  grep -E "Entity updates per second:|entities"
# AI integrated (production)
grep -B 5 -A 10 "TestIntegrated" "$PROJECT_ROOT/test_results/ai_scaling_benchmark_"*.txt | \
  grep -E "Entity updates per second:|entities"

# Other systems (adjust paths to discovered result dirs)
grep -E "^(Collision Checks|Query Time|Hash Efficiency|AABB Tests):"        "$PROJECT_ROOT/test_results/collision_benchmark/performance_metrics.txt"
grep -E "^(Path Calculation|Nodes Explored|Cache Hits|A\* Performance):"    "$PROJECT_ROOT/test_results/pathfinder_benchmark/performance_metrics.txt"
grep -E "^(Events/sec|Dispatch Latency|Queue Depth|Peak Throughput):"       "$PROJECT_ROOT/test_results/event_manager_scaling/performance_metrics.txt"
grep -E "^(Particles/frame|Render Time|Batch Count|Culling Efficiency):"    "$PROJECT_ROOT/test_results/particle_benchmark/performance_metrics.txt"
grep -E "^(Components|Render Time|Event Handling|DPI Scaling):"             "$PROJECT_ROOT/test_results/ui_stress/performance_metrics.txt"
```

See `references/report-template.md` for illustrative parse structures.

### Step 3: Statistical analysis

For any metric with multiple data points, compute mean, median, stddev, p50/p95/p99,
min/max, and coefficient of variation (`stddev/mean * 100`). Aggregate across runs
when several exist, and flag outliers (>2 stddev from mean). A reference Python
implementation:
```python
def calculate_statistics(values):
    n = len(values)
    mean = sum(values) / n
    s = sorted(values)
    median = s[n//2] if n % 2 else (s[n//2-1] + s[n//2]) / 2
    stddev = (sum((x - mean) ** 2 for x in values) / n) ** 0.5
    return {
        'mean': mean, 'median': median, 'stddev': stddev,
        'p50': median, 'p95': s[int(n * 0.95)], 'p99': s[int(n * 0.99)],
        'min': min(values), 'max': max(values),
        'coefficient_of_variation': (stddev / mean) * 100 if mean else 0,
    }
```

### Step 4: Callgrind analysis (if available)

```bash
CALLGRIND_FILE=$(ls -t "$PROJECT_ROOT/test_results/valgrind/callgrind/callgrind.out."* 2>/dev/null | head -n 1)
[ -f "$CALLGRIND_FILE" ] && callgrind_annotate --auto=yes "$CALLGRIND_FILE" | head -n 50
```
Extract top functions by instruction reads (Ir) and by calls, plus call graphs for
critical paths (AI update, collision detection, rendering). Example hotspot output
is in `references/report-template.md`.

### Step 5: Trend analysis (if baselines available)

For each metric, load the baseline value, compute percent change, and classify:
- **Improving:** >5% better than baseline
- **Degrading:** >5% worse than baseline
- **Stable:** within +/-5% of baseline
- **New:** no baseline for comparison

### Step 6: Assemble the report

**Read `references/report-template.md` now** and fill its sections with the parsed,
real data from Steps 1-5:
1. Executive Summary
2. Detailed System Analysis (per discovered system)
3. Cross-System Analysis (frame budget, interactions, resource usage)
4. Optimization Opportunities
5. Historical Trends
6. Comparative Analysis
7. Technical Details
8. Appendices (raw metrics, callgrind, test scripts via Appendix C `ls` discovery, baseline history)

The template's numbers/tables/charts are **illustrative scaffolding only** — replace
every value with discovered data and drop sections for systems that produced no results.

### Step 7: Format and save

Save markdown to `docs/performance_reports/performance_report_YYYY-MM-DD.md`. Optional
HTML/PDF via pandoc:
```bash
pandoc performance_report.md -o performance_report.html --standalone \
  --metadata title="VoidLight-Framework Performance Report"
pandoc performance_report.md -o performance_report.pdf --pdf-engine=xelatex \
  --variable geometry:margin=1in --variable fontsize=11pt \
  --metadata title="VoidLight-Framework Performance Report"
```

### Step 8: Emit a console summary

Print a short status block (systems analyzed, data points, baseline availability,
overall status, key highlights, generated file paths, next steps). See the console
summary template in `references/report-template.md`.

## Report Customization

Ask the user (default in brackets): **Scope** [all systems vs. specific], **Detail**
[full report vs. exec-summary-only vs. technical deep-dive with callgrind], **Formats**
[Markdown always; HTML/PDF optional], **Sections** [include/exclude trends, callgrind,
optimization, raw appendices], **Comparison** [vs baseline / vs historical / vs industry].

## Error Handling

- **No benchmark data:** report "No benchmark data available. Run benchmarks first:"
  and show `./tests/test_scripts/run_all_tests.sh --benchmarks-only`.
- **No baseline:** note "No baseline for comparison. This will serve as baseline." and
  save current metrics as baseline.
- **Incomplete data:** generate a partial report, note missing systems, recommend
  running the missing benchmarks.

**Exit codes:** 0 success | 1 no data | 2 generation failed | 3 partial (missing data).

## File Management

Reports live in `docs/performance_reports/` (`latest_report.md` symlinks the most
recent). Archive reports older than 12 months:
```bash
find docs/performance_reports/ -name "*.md" -mtime +365 -exec mv {} archive/ \;
```

## Quality Bar

Every report should cover all discovered systems, apply statistical checks (min 3
data points), include trend analysis when historical data exists, use clear status
indicators, give actionable recommendations, and record version/environment info
(git commit, date, platform, build type) for reproducibility.

## Expected Time

~2-4 minutes end to end (data collection 1-2 min, analysis + formatting ~1-2 min)
versus ~45-60 minutes done manually.
