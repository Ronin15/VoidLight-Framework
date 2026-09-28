---
name: voidlight-dependency-analyzer
description: Verify dependency structure and architecture health for SDL3 VoidLight-Framework. Detects circular dependencies, excessive coupling, layer violations, header bloat, and provides dependency graph visualization. Ensures adherence to layered architecture (Core→Managers→States→Entities). Use monthly, after major refactors, or when investigating compile time issues.
allowed-tools: [Bash, Read, Write, Grep, Glob, AskUserQuestion]
---

# VoidLight-Framework Dependency Analyzer

Comprehensive dependency structure analysis for SDL3 VoidLight-Framework. Verifies
architectural integrity, detects circular dependencies, identifies coupling
issues, and maintains clean layered design.

This Skill is **script-driven**: the canonical analysis lives in `scripts/`, which
re-derive the layer/dir set from the filesystem on every run. The detailed
architecture rules, the full report template, and per-script docs live in
`references/` and are loaded on demand — keep this file as the playbook.

## Architecture (overview)

VoidLight-Framework follows a **layered architecture** (per CLAUDE.md):
`Core → Managers → GameStates → Entities/Controllers`, with cross-cutting layers
`Utils, Events, AI, Collisions, World, GPU`. `src/` and `include/` mirror each
other.

**Discover the current layer/dir set at runtime — do NOT assume a frozen list:**
```bash
ls -d src/*/ include/*/        # current top-level layers
ls include/managers/*.hpp      # current manager set
```
At time of writing the live set is `{core, managers, controllers, gameStates,
entities, events, ai, collisions, utils, world, gpu}` (11 layers), but the helper
scripts re-derive and classify it automatically.

**For the full per-layer dependency rules, the functional-coupling allowlist,
approved layer exceptions, and the recurring issue catalog, read
`references/architecture-model.md`.** Load it before deciding whether any
dependency is a true violation or an expected game-system coupling.

## What this Skill ensures

1. **Circular Dependency Detection** — prevent include cycles that break compilation
2. **Coupling Analysis** — maintain loose coupling between managers
3. **Layer Violation Detection** — enforce one-way dependencies (no upward deps)
4. **Header Bloat Identification** — find unnecessary includes slowing compilation
5. **Forward Declaration Opportunities** — reduce compilation dependencies
6. **Dependency Graph Visualization** — understand system relationships
7. **Compile Time Impact Analysis** — estimate compilation cost per component
8. **Architecture Health Scoring** — quantify overall design quality

> **Game engine context:** tight manager coupling is often *functionally
> necessary and correct* (AI needs collision, world needs events, etc.). Only
> circular dependencies and layer violations are always bad. See
> `references/architecture-model.md`.

## Helper Scripts

Canonical implementations live in `scripts/` next to this file. **Prefer
invoking these over hand-rolled bash.** All write to
`test_results/dependency_analysis/`. Run from the repo root.

- `extract_deps.py` — build the include dependency graph (`dependency_graph.txt`)
- `detect_cycles.py <graph>` — circular dependency detection (exit 1 if cycles)
- `detect_layer_violations.py` — layer-boundary violations (auto-classifies dirs)
- `analyze_coupling.py <graph> [base_dir]` — fan-in/out, instability, manager coupling
- `analyze_header_bloat.py` — high-include headers + forward-declaration opportunities
- `calc_depth.py <graph>` — dependency depth / compile-time impact
- `generate_trees.py <graph>` — ASCII dependency trees
- `calc_health_score.py` — overall health score, derived from the outputs above

**Run order:** `extract_deps.py` → (`detect_cycles.py` / `detect_layer_violations.py` /
`analyze_coupling.py` / `analyze_header_bloat.py` / `calc_depth.py` /
`generate_trees.py`) → `calc_health_score.py`.

**For per-script usage signatures, exact output filenames, classification
thresholds (fan-out/bloat/depth cutoffs), exit codes, and the underlying
algorithms (manual fallback), read `references/scripts.md`.**

## Analysis Modes

| Mode | Time | Use when | Scripts to run |
|------|------|----------|----------------|
| **1. Quick Circular Check** | 2-3 min | daily dev, pre-commit | `extract_deps` → `detect_cycles` |
| **2. Coupling Analysis** | 5-10 min | adding/refactoring managers | `extract_deps` → `analyze_coupling` |
| **3. Full Architecture Audit** | 15-20 min | monthly, major refactors, release prep | all scripts |
| **4. Specific Component** | 3-5 min | targeted coupling investigation | `extract_deps` → `detect_cycles` + grep on the component |

Manual equivalent of a full audit: ~60-120 minutes.

## Workflow

### Step 1 — Gather input (AskUserQuestion)

Ask three questions:
1. **Mode** — Quick Circular Check / Coupling Analysis / Full Architecture Audit / Specific Component.
2. **Component** — only if Mode = Specific Component (AIManager / CollisionManager / PathfinderManager / EventManager / Custom).
3. **Format** — Markdown Report (default, saved to `docs/`) / ASCII Tree (console) / Both.

### Step 2 — Build the dependency graph

```bash
python3 .claude/skills/voidlight-dependency-analyzer/scripts/extract_deps.py
```
Produces `test_results/dependency_analysis/dependency_graph.txt`
(referred to as `$GRAPH` below). Required before any other check.

### Step 3 — Run the checks for the selected mode

Let `S=.claude/skills/voidlight-dependency-analyzer/scripts` and
`GRAPH=test_results/dependency_analysis/dependency_graph.txt`.

- **Mode 1 (Quick):** `python3 $S/detect_cycles.py $GRAPH` — exit 1 = cycles found
  (BLOCKING). For fix patterns (forward declaration / interface extraction /
  dependency inversion) see `references/architecture-model.md`.
- **Mode 2 (Coupling):** `python3 $S/analyze_coupling.py $GRAPH .` — fan-in/out,
  instability, manager-to-manager matrix. Interpret tight coupling against the
  functional allowlist in `references/architecture-model.md`.
- **Mode 3 (Full Audit):** run all of `detect_cycles.py`,
  `detect_layer_violations.py`, `analyze_coupling.py $GRAPH .`,
  `analyze_header_bloat.py`, `calc_depth.py $GRAPH`, then continue to Step 4.
- **Mode 4 (Specific Component):** run `detect_cycles.py $GRAPH`, then report the
  one component's efferent deps (`grep '^#include "' <header>`), afferent deps
  (`grep -r '#include "<Header>"' include/ src/`), instability, layer
  classification, and any cycle membership. Thresholds in `references/scripts.md`.

### Step 4 — Visualize (Full Audit, or when Format ≠ report-only)

```bash
python3 .claude/skills/voidlight-dependency-analyzer/scripts/generate_trees.py $GRAPH
```
Produces ASCII dependency trees in `test_results/dependency_analysis/dependency_trees.txt`.

### Step 5 — Health score + report

```bash
python3 .claude/skills/voidlight-dependency-analyzer/scripts/calc_health_score.py
```
Reads the prior outputs and writes `health_scorecard.txt` + `health_score.json`.

If Format includes a Markdown report, assemble it from the prior outputs and save
to `docs/architecture/dependency_analysis_$(date +%Y-%m-%d).md`. **For the full
report template, the health scorecard table, and the scoring guidance/formula,
read `references/output-format.md`.**

### Step 6 — Console summary

Print a concise summary (mode, files analyzed, health score, circular/violation/
coupling/bloat counts, status, top recommendations, report path). **The exact
console summary template is in `references/output-format.md`.**

## When to use

Activate automatically when the user says: "analyze dependencies", "check
architecture health", "find circular dependencies", "check manager coupling",
"analyze AIManager dependencies", or "audit header dependencies".

- **Regular maintenance:** monthly audits, after major refactors, before releases.
- **Development checkpoints:** adding a manager, refactoring, investigating compile times.
- **Problem investigation:** circular dependency errors, slow compilation, tight-coupling concerns.

## Reference Files

- `references/architecture-model.md` — full layer model, per-layer rules,
  coupling rules + functional allowlist, approved exceptions, common issues, and
  circular-dependency fix patterns.
- `references/scripts.md` — per-script usage, output filenames, run order,
  classification thresholds, exit codes, and underlying algorithms.
- `references/output-format.md` — full Markdown report template, console summary
  template, and game-engine scoring guidance/formula.
