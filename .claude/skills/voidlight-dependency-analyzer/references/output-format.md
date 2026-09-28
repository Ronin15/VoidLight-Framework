# Report Template, Console Summary & Scoring

Load this when generating the final Markdown report (Step 5), the console summary
(Step 6), or computing/explaining the health score. Save the generated report to
`docs/architecture/dependency_analysis_YYYY-MM-DD.md`.

## Full Markdown Report Template

```markdown
# VoidLight-Framework Dependency Analysis Report

**Generated:** YYYY-MM-DD HH:MM:SS
**Branch:** <branch>
**Commit:** <hash>
**Analysis Mode:** <mode>

---

## Executive Summary

**Architecture Health Score:** [85/100] (GOOD)

**Status:** ✅ HEALTHY / ⚠️ NEEDS ATTENTION / 🔴 CRITICAL ISSUES

**Key Findings:**
- [Finding 1]
- [Finding 2]
- [Finding 3]

**Overall Assessment:** [2-3 sentence summary]

---

## Dependency Statistics

**Codebase Size:**
- Total headers analyzed: [N]
- Core layer: [N] files
- Managers layer: [N] files
- States layer: [N] files
- Entities layer: [N] files
- Utils layer: [N] files

**Dependency Metrics:**
- Total dependencies: [N]
- Average dependencies per file: [X.X]
- Max dependencies (single file): [N]
- Circular dependencies: [N] 🔴/✅

---

## Circular Dependencies (CRITICAL)

[If none:]
✅ **NO CIRCULAR DEPENDENCIES DETECTED**

All include hierarchies are acyclic. Compilation order is deterministic.

[If found:]
🔴 **FOUND [N] CIRCULAR DEPENDENCIES**

### Cycle 1: [Component A] ↔ [Component B]

**Cycle Path:**
```
ComponentA.hpp -> ComponentB.hpp -> ComponentA.hpp
```

**Impact:** Breaks compilation or requires workarounds

**Suggested Fix:**
1. **Forward Declaration** (RECOMMENDED)
   - In ComponentA.hpp, replace `#include "ComponentB.hpp"` with `class ComponentB;`
   - Move include to ComponentA.cpp

2. **Interface Extraction**
   - Create IComponentB.hpp with pure virtual interface
   - ComponentA depends on interface (breaks cycle)

[Repeat for each cycle]

---

## Coupling Analysis

### High-Coupling Components (Top 10)

| Component | Fan-Out | Fan-In | Instability | Status |
|-----------|---------|--------|-------------|--------|
| [Component1] | 18 | 5 | 0.78 | 🔴 HIGH |
| [Component2] | 15 | 12 | 0.55 | ⚠️  MODERATE |
| [Component3] | 8 | 20 | 0.29 | ✅ STABLE |

**Legend:**
- **Fan-Out:** Number of dependencies (efferent coupling)
- **Fan-In:** Number of dependents (afferent coupling)
- **Instability:** Fan-Out / (Fan-In + Fan-Out); 0.0 = stable, 1.0 = unstable

### Manager-to-Manager Coupling

**Coupling Matrix:** [include the matrix from the coupling/tree output]

**Important Context:** Game engines have **necessary functional dependencies**.
Tight coupling between game systems is often **CORRECT and required**.

**Functional Coupling (✅ Expected & Correct):**
- AIManager → CollisionManager (AI obstacle avoidance, LOS checks)
- CollisionManager → WorldManager (world geometry queries)
- Managers → EventManager (event-driven notifications)
- UIManager → FontManager (text rendering)
- etc.

**Status:** ✅ These dependencies are functionally necessary.

**Problematic Coupling (🔴 Review Required):**
[If any non-functional tight coupling exists]
1. **[Manager1] → [Manager2]** (N references)
   - Status: 🔴 REVIEW — unclear functional necessity
   - Recommendation: Verify this coupling serves game functionality

[If none:] ✅ **NO PROBLEMATIC COUPLING** — all tight coupling serves game systems.

---

## Layer Violations

**Core Layer** (should depend on nothing): [✅ CLEAN / 🔴 [N] violations]
**Managers Layer** (Core, Utils only): [✅ CLEAN / 🔴 [N] violations]
**States Layer** (no cross-state deps): [✅ CLEAN / 🔴 [N] violations]
**Utils Layer** (dependency-free): [✅ CLEAN / 🔴 [N] violations]

### Violation Details

[If found:]
**Violation 1: AIManager includes GamePlayState.hpp**
- **File:** include/managers/AIManager.hpp:15
- **Issue:** Manager layer depending on State layer
- **Impact:** Violates layered architecture, creates tight coupling
- **Fix:** Remove include, use interface or event system

[If none:] ✅ **NO LAYER VIOLATIONS**

---

## Header Bloat Analysis

### High-Include Headers

| Header | #Includes | Status | Dependents |
|--------|-----------|--------|-----------|
| [Header1] | 22 | 🔴 HIGH | 15 files |
| [Header2] | 18 | ⚠️  MODERATE | 8 files |

**Bloat Amplification:** widely-used include-heavy headers cause ripple recompiles.

**Worst Offenders:**
1. **GameEngine.hpp** - 22 includes, used by 15 files (~330 transitive includes)
   - Recommendation: Split into GameEngine_fwd.hpp with forward declarations

### Forward Declaration Opportunities

[Top 10 opportunities from analyze_header_bloat.py]

**Estimated Compile Time Savings:** ~15-25% reduction

---

## Dependency Depth Analysis

| Header | Depth | Impact | Recommendation |
|--------|-------|--------|----------------|
| [Header1] | 14 | 🔴 VERY HIGH | Reduce dependencies |
| [Header2] | 9 | ⚠️  HIGH | Consider splitting |
| [Header3] | 6 | 🟡 MODERATE | Monitor |

**Total Dependency Depth:** [N] · **Average Depth:** [X.X]

Changing high-depth headers causes cascading recompilation. Use forward
declarations and split large headers.

---

## Architecture Health Scorecard

| Category | Score | Weight | Weighted | Status |
|----------|-------|--------|----------|--------|
| Circular Dependencies | [X/10] | 30% | [X.X] | 🔴/⚠️/✅ |
| Layer Compliance | [X/10] | 25% | [X.X] | 🔴/⚠️/✅ |
| Coupling Strength | [X/10] | 20% | [X.X] | 🔴/⚠️/✅ |
| Header Bloat | [X/10] | 15% | [X.X] | 🔴/⚠️/✅ |
| Dependency Depth | [X/10] | 10% | [X.X] | 🔴/⚠️/✅ |
| **TOTAL** | | **100%** | **[XX/100]** | **[GRADE]** |

**Grading Scale:**
- 90-100: A+ (Excellent) · 80-89: A (Good, minor issues) · 70-79: B (Fair)
- 60-69: C (Poor, refactoring required) · Below 60: F (Critical)

---

## Recommendations

### Critical (Fix Immediately)
1. **Break Circular Dependencies** ([N] found) — forward declarations / interfaces. HIGH, 2-4h.
2. **Fix Layer Violations** ([N] found) — remove inappropriate includes / use events. HIGH, 1-2h.

### Important (Address Soon)
3. **Review Non-Functional Coupling (if any)** — verify it serves a game system requirement; document or refactor. MEDIUM (only if problematic coupling exists).
4. **Optimize High-Include Headers** — split into interface + implementation, add fwd-decl headers. MEDIUM, 2-3h.

> Do NOT refactor functional game system dependencies — manager coupling is often necessary and correct.

### Optional (Consider)
5. **Improve Compile Times** — apply forward declaration opportunities, split large headers. ~15-25% faster. LOW, 3-5h.

---

## Comparison with Previous Analysis (if baseline exists)

| Metric | Previous | Current | Change | Trend |
|--------|----------|---------|--------|-------|
| Total Dependencies | [N] | [N] | [+/-N] | 📈/📉/➡️ |
| Circular Dependencies | [N] | [N] | [+/-N] | 📈/📉/➡️ |
| Layer Violations | [N] | [N] | [+/-N] | 📈/📉/➡️ |
| Average Coupling | [X.X] | [X.X] | [+/-X.X] | 📈/📉/➡️ |
| Health Score | [XX] | [XX] | [+/-XX] | 📈/📉/➡️ |

**Overall Trend:** [Improving/Stable/Degrading]

---

## Action Plan

### Immediate (This Week)
- [ ] Break circular dependency: [Cycle description]
- [ ] Fix layer violation: [Specific violation]

### Short-term (This Month)
- [ ] Reduce coupling between [Component1] and [Component2]
- [ ] Split high-include headers: [Header list]
- [ ] Apply top 5 forward declaration opportunities

### Long-term (This Quarter)
- [ ] Comprehensive header cleanup
- [ ] Establish pre-commit dependency checks
- [ ] Document architecture patterns

---

## Files Requiring Attention

**High Priority:** [Circular dependency / layer violation files]
**Medium Priority:** [High coupling / header bloat files]
**Low Priority:** [Forward declaration opportunity files]

---

## Next Steps

1. Review with team/architect
2. Prioritize fixes by impact and effort
3. Create tickets for identified issues
4. Re-run analysis after fixes to verify improvements
5. Schedule regular audits (monthly recommended)

---

**Generated By:** voidlight-dependency-analyzer Skill
**Saved To:** `docs/architecture/dependency_analysis_YYYY-MM-DD.md`
```

## Console Summary Template (Step 6)

```
=== VoidLight-Framework Dependency Analysis ===

Mode: [Mode Name]
Files Analyzed: [N] headers

Architecture Health: [Score]/100 ([GRADE])

Circular Dependencies: [N] 🔴/✅
Layer Violations: [N] 🔴/✅
High Coupling: [N] components ⚠️
Header Bloat: [N] headers 🔴/⚠️

[If issues:]
Status: 🔴 CRITICAL ISSUES / ⚠️  NEEDS ATTENTION
Critical Issues:
  - [Issue 1]
  - [Issue 2]
Recommendations:
  1. [Top recommendation]
  2. [Second recommendation]

[If clean:]
Status: ✅ ARCHITECTURE HEALTHY
  ✅ No circular dependencies
  ✅ No layer violations
  ✅ Coupling within acceptable limits
  ✅ No excessive header bloat

Full Report: docs/architecture/dependency_analysis_YYYY-MM-DD.md
Next: [Suggested action based on results]
```

## Scoring Guidance for Game Engines

`calc_health_score.py` computes the scorecard. When interpreting or explaining
the Coupling Strength category (20% of the score), distinguish:

1. **Functional Coupling** (✅ Good, score 8-10/10) — game systems that must
   interact (AI→Collision, Collision→World, Managers→Events). Correct design;
   these should NOT significantly reduce the score.
2. **Problematic Coupling** (🔴 Bad, score 0-4/10) — circular dependencies,
   layer violations (Manager→State), unclear/unnecessary dependencies.

**Scoring Formula:**
```
Coupling Score = 10 - (problematic_coupling_count * 1.5)

problematic_coupling_count =
  circular dependencies + layer-violating dependencies + non-functional tight coupling

Functional coupling does NOT reduce score.
```

**Examples:**
- 0 problematic, 9 functional → 10/10 ✅ Excellent
- 1-2 problematic, 9 functional → 7-8.5/10 ✅ Good
- 3-5 problematic, 9 functional → 4-6/10 ⚠️ Needs work
- 6+ problematic → 0-2/10 🔴 Critical

Game engines NEED manager coupling to function — score accordingly.
