# Helper Scripts Reference

Canonical, maintained implementations live in `scripts/` next to `SKILL.md`.
They discover headers/dirs from `include/` and `src/` at runtime and classify
every directory automatically. **Always prefer these over hand-rolled bash.**
All scripts read `include/` + `src/` and write to `test_results/dependency_analysis/`
relative to the current directory, so run them from the repo root.

## Run Order

```
extract_deps.py
  → detect_cycles.py <graph>
  → detect_layer_violations.py
  → analyze_coupling.py <graph> [base_dir]
  → analyze_header_bloat.py
  → calc_depth.py <graph>
  → generate_trees.py <graph>
  → calc_health_score.py        # reads the outputs of all the above
```

## Per-Script Reference

Paths below assume `OUT=test_results/dependency_analysis` and
`GRAPH=$OUT/dependency_graph.txt`. Invoke as
`python3 .claude/skills/voidlight-dependency-analyzer/scripts/<name> ...`.

| Script | Invocation | Produces | Notes |
|--------|-----------|----------|-------|
| `extract_deps.py` | `extract_deps.py` | `$OUT/dependency_graph.txt` | Builds the include adjacency list (`Source.hpp -> Target.hpp`) from `include/` + `src/`. Run first; everything else consumes the graph. |
| `detect_cycles.py` | `detect_cycles.py $GRAPH` | `$OUT/circular_dependencies.txt` | DFS cycle detection. Exit 0 = none, exit 1 = cycles found. Summary file holds `circular_dependencies=<N>`. |
| `detect_layer_violations.py` | `detect_layer_violations.py` | `$OUT/layer_violations.txt` | Auto-classifies every dir (bare includes resolve to the including header's dir), enforces the per-layer table in architecture-model.md (Core/Utils dependency-free, Managers/Controllers never include States, no cross-state deps). Always exits 0; count is in the file. |
| `analyze_coupling.py` | `analyze_coupling.py $GRAPH [base_dir]` | `$OUT/coupling_metrics.txt`, `$OUT/coupling_summary.txt` | Fan-in/out, instability, manager-to-manager coupling. Uses the functional-dependency allowlist (see architecture-model.md) so expected game-system coupling is not flagged. |
| `analyze_header_bloat.py` | `analyze_header_bloat.py` | `$OUT/header_bloat_analysis.txt` | High-include headers, frequently-included headers (ripple effect), forward-declaration opportunities. |
| `calc_depth.py` | `calc_depth.py $GRAPH` | `$OUT/dependency_depths.txt` | Max dependency depth per header = compile-time recompilation ripple. |
| `generate_trees.py` | `generate_trees.py $GRAPH` | `$OUT/dependency_trees.txt` | ASCII dependency trees for key components (e.g. GameEngine, AIManager). |
| `calc_health_score.py` | `calc_health_score.py` | `$OUT/health_scorecard.txt`, `$OUT/health_score.json` | Reads `circular_dependencies.txt`, `layer_violations.txt`, `coupling_metrics.txt`, `coupling_summary.txt`, `header_bloat_analysis.txt`, `dependency_depths.txt`. Run last. |

## Classification Thresholds

These are the thresholds the scripts (and any manual reading of their output) use.

**Fan-Out (efferent coupling):** `>15` 🔴 HIGH · `>10` ⚠️ MEDIUM · `>5` 🟡 MODERATE · else ✅ LOW

**Fan-In (afferent coupling):** `>20` ⭐ CORE · `>10` 📦 STABLE · `>5` 🔧 UTILITY · else 📄 LEAF

**Instability** `I = FanOut / (FanIn + FanOut)`: `0.0` = maximally stable (hard to change), `1.0` = maximally unstable. `I > 0.8` highly unstable; `I < 0.2` highly stable.

**Manager-pair reference count (in .cpp):** `>10` tight (functional if allowlisted, else review) · `>5` moderate.

**Header include count:** `>15` 🔴 HIGH · `>10` ⚠️ MODERATE. A header included by `>10` files with `>10` of its own includes = bloat amplification.

**Dependency depth:** `>10` 🔴 VERY HIGH · `>7` ⚠️ HIGH · `>4` 🟡 MODERATE · else ✅ LOW.

## Exit codes

Only `detect_cycles.py` signals via exit code (0 = none, 1 = cycles). All other
scripts exit 0 and report counts in their output files; a Python traceback
means a stale assumption in the script — fix the script, not the output.
