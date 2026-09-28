# Helper Scripts Reference

Canonical, maintained implementations live in `scripts/` next to `SKILL.md`.
They discover headers/dirs from `include/` and `src/` at runtime and classify
every directory automatically. **Always prefer these over hand-rolled bash.**
All scripts write to `test_results/dependency_analysis/`. Run from the repo root.

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
| `detect_layer_violations.py` | `detect_layer_violations.py` | `$OUT/layer_violations.txt` | Auto-classifies every dir, enforces one-way layer rules (Core/Utils dependency-free, Managers not depending on States/Entities, no cross-state deps). |
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

## Exit Codes (overall analysis)

- **0:** No architectural issues detected
- **1:** Circular dependencies found (BLOCKING)
- **2:** Layer violations detected (CRITICAL)
- **3:** High coupling detected (WARNING)
- **4:** Multiple issues detected

## Underlying Algorithms (illustrative — the scripts are authoritative)

If a script is unavailable or you need to understand/reproduce a check by hand,
these are the algorithms the scripts implement. Prefer the scripts; this is
fallback documentation only.

**Graph build:** for each `*.hpp` under `include/` and `src/`, grep `^#include "..."`,
basename both ends, emit `Source.hpp -> Target.hpp` lines (local includes only,
skip system `<...>` includes).

**Cycle detection:** DFS with a recursion stack; when a neighbor is already on the
stack, the path slice from that neighbor to the current node is a cycle.

**Coupling:** fan-out = count of `^Header ->` lines; fan-in = count of `-> Header$`
lines; instability as above. Manager-to-manager: build a `✓` matrix from header
includes, then count references to each other manager inside the `.cpp` to grade
strength against the allowlist.

**Layer violations:** classify each header by its directory, then flag includes
that point "upward" or sideways against the per-layer rules:
- Core / Utils headers including anything outside core/ + utils/.
- Manager headers including `gameStates/` (forbidden) or `entities/` (review).
- State headers including another state header (cross-state dependency).

**Header bloat:** count `^#include "` per header; count how many files include each
header (`grep -r`); flag widely-included headers that are themselves include-heavy.

**Depth:** memoized DFS computing max distance to a leaf node per header.

**Trees:** recursive descent over the adjacency list with a visited set (mark
`(circular)` on revisit) and a max-depth cap (mark `(...)` at the cap).
