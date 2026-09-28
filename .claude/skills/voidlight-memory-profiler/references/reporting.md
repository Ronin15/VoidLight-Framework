# Memory Profiler — Report Template & Console Summary

Loaded on demand from `SKILL.md` for the reporting step. Fill in only the
sections relevant to the mode that was run.

## Markdown Report Structure

```markdown
# VoidLight-Framework Memory Profile Report

**Generated:** YYYY-MM-DD HH:MM:SS
**Branch:** <current-branch>
**Commit:** <commit-hash>
**Profiling Mode:** <mode>
**Test Scope:** <scope>

---

## Executive Summary

**Overall Status:** CLEAN / WARNINGS / CRITICAL ISSUES

**Key Findings:**
- [Finding 1]
- [Finding 2]
- [Finding 3]

**Memory Health:** [Excellent/Good/Fair/Poor]

---

## Leak Detection Results

### Critical Leaks (BLOCKING)

| Test | Definite Leaks | Invalid Access | Status |
|------|----------------|----------------|--------|
| [Test 1] | [X bytes] | [N violations] | CRITICAL/OK |
| [Test 2] | [X bytes] | [N violations] | CRITICAL/OK |

**Total Definite Leaks:** [X bytes] (Target: 0 bytes)

### Leak Details

**Test:** [test_name]
**Leak Location:** [file:line]
**Stack Trace:**
```
[valgrind stack trace]
```

**Likely Cause:** [Analysis]
**Suggested Fix:** [Specific code change]

---

## Allocation Profiling (if Mode 2)

### Per-Frame Allocation Hotspots

| System | Allocations/Frame | Impact | Status |
|--------|-------------------|--------|--------|
| AIManager | [N] | [Frame spike: Xms] | WARN/OK |
| CollisionManager | [N] | [Frame spike: Xms] | WARN/OK |
| ParticleManager | [N] | [Frame spike: Xms] | WARN/OK |

**Total Per-Frame Allocations:** [N] (Target: 0 in hot paths)

### Anti-Pattern Violations

**1. Local Vectors in Update Functions:**
- `AIManager.cpp:123` - `std::vector<Data> localBuffer;` in `processBatch()`
  - **Fix:** Make `m_processingBuffer` member variable, use `clear()` per frame

**2. Push_back Without Reserve:**
- `CollisionManager.cpp:456` - Loop with `results.push_back()` without `reserve()`
  - **Fix:** Add `results.reserve(expectedCount);` before loop

**3. Vector Reconstruction:**
- `PathfinderManager.cpp:789` - `m_pathCache = std::vector<Path>();`
  - **Fix:** Replace with `m_pathCache.clear();` to preserve capacity

---

## Memory Usage by System (if Mode 3)

### Peak Memory

| System | Peak Allocation | % of Total | Trend |
|--------|----------------|------------|-------|
| AIManager | [X MB] | [%] | up/down/flat |
| CollisionManager | [X MB] | [%] | up/down/flat |
| PathfinderManager | [X MB] | [%] | up/down/flat |
| EventManager | [X MB] | [%] | up/down/flat |
| ParticleManager | [X MB] | [%] | up/down/flat |
| **Total** | **[X MB]** | **100%** | - |

### Top Allocation Sites

1. **AIManager::processBatch()** - [X MB] ([%] of total)
   - [N] allocations
   - Stack trace: [abbreviated]

2. **CollisionManager::detectCollisions()** - [X MB] ([%] of total)
   - [N] allocations
   - Stack trace: [abbreviated]

---

## Buffer Reuse Audit (if Mode 4)

### Pattern Compliance

| Manager | Member Buffers | clear() Usage | reserve() Usage | Grade |
|---------|----------------|---------------|-----------------|-------|
| AIManager | Yes | Correct | Present | A |
| CollisionManager | Yes | Partial | Missing | C |
| ParticleManager | No | Local vars | Missing | F |

**Overall Compliance:** [%] (Target: 100%)

### Recommendations

**AIManager:**
- Excellent buffer reuse pattern; document as reference implementation

**CollisionManager:**
- Add `reserve()` calls in `detectCollisions()` before `results.push_back()` loop
- Estimated improvement: -50 allocs/frame

**ParticleManager:**
- Critical: Replace local `std::vector<Particle> activeParticles;` with member `m_activeParticles`
- Add `m_activeParticles.clear()` at start of `update()`
- Add `reserve(maxParticles)` in constructor
- Estimated improvement: -200 allocs/frame

---

## Baseline Comparison (if applicable)

### Leak Trend

| Test | Baseline | Current | Change | Status |
|------|----------|---------|--------|--------|
| [Test 1] | [X bytes] | [X bytes] | [+/-] | regress/improve/stable |

### Memory Usage Trend

[Chart or table showing memory usage over time]

**Overall Trend:** [Improving/Stable/Degrading]

---

## Optimization Opportunities

### High Priority (Immediate Fix)

1. **ParticleManager: Eliminate per-frame allocations**
   - **Current:** 200 allocs/frame (~128 KB/frame)
   - **Impact:** Frame spikes of 5-10ms
   - **Fix:** [Specific code changes]
   - **Expected Improvement:** -5ms frame time

2. **CollisionManager: Add reserve() calls**
   - **Current:** Incremental reallocations in query results
   - **Impact:** 1-2ms overhead
   - **Fix:** [Specific code changes]
   - **Expected Improvement:** -1ms query time

### Medium Priority

3. **AIManager: Increase batch buffer size**
   - **Current:** 1024 entities, reallocs when exceeded
   - **Fix:** Increase to 2048 or make dynamic
   - **Expected Improvement:** Eliminate rare reallocs

### Low Priority

4. **EventManager: Consider event pool**
   - **Current:** Event objects allocated per dispatch
   - **Fix:** Implement object pool for event reuse
   - **Expected Improvement:** -10% event dispatch time

---

## Specific Code Fixes

### Fix 1: ParticleManager Buffer Reuse

**File:** `include/managers/ParticleManager.hpp:45`

**Before:**
```cpp
class ParticleManager
{
    // ... no reusable buffer
};
```

**After:**
```cpp
class ParticleManager
{
    std::vector<Particle> m_activeParticles;  // Reusable buffer
    // ... reserve in constructor
};
```

**File:** `src/managers/ParticleManager.cpp:123`

**Before:**
```cpp
void ParticleManager::update(float deltaTime)
{
    std::vector<Particle> activeParticles;  // Allocation every frame!
    // ... use activeParticles
}
```

**After:**
```cpp
void ParticleManager::update(float deltaTime)
{
    m_activeParticles.clear();  // Reuse capacity
    // ... use m_activeParticles
}
```

**File:** `src/managers/ParticleManager.cpp:34` (constructor)

**Add:**
```cpp
ParticleManager::ParticleManager()
{
    m_activeParticles.reserve(MAX_PARTICLES);  // Pre-allocate
}
```

---

## Test Results Summary

**Tests Run:** [N]
**Tests Passed:** [N]
**Critical Issues:** [N]
**Warnings:** [N]

**Status:** [CLEAN / NEEDS REVIEW / FIX REQUIRED]

---

## Action Items

### Critical (Fix Before Commit)
- [ ] [Critical issue 1]
- [ ] [Critical issue 2]

### Important (Fix Soon)
- [ ] [Important issue 1]
- [ ] [Important issue 2]

### Optional (Consider)
- [ ] [Optional improvement 1]
- [ ] [Optional improvement 2]

---

## Files Modified (Recommended)

```
include/managers/ParticleManager.hpp      (add member buffer)
src/managers/ParticleManager.cpp          (use buffer, add clear/reserve)
src/managers/CollisionManager.cpp         (add reserve calls)
```

---

## Next Steps

1. **If critical issues:** Fix immediately, re-run profile to verify
2. **If warnings:** Review and plan fixes
3. **If clean:** Update baseline (save as reference)
4. **Consider:** Run full benchmark suite to measure performance impact

---

**Report Generated By:** voidlight-memory-profiler Skill
**Report Saved To:** `test_results/memory_profiles/memory_profile_YYYY-MM-DD.md`
```

### Save the report

```bash
REPORT_FILE="test_results/memory_profiles/memory_profile_$(date +%Y-%m-%d_%H-%M-%S).md"
cat > "$REPORT_FILE" <<'EOF'
[Generated markdown report]
EOF

echo "Memory profile report saved to: $REPORT_FILE"
```

---

## Console Summary (output to user)

```
=== VoidLight-Framework Memory Profile ===

Mode: [Mode Name]
Scope: [Test Scope]
Duration: [Time taken]

Overall Status: [CLEAN / WARNINGS / CRITICAL]

Critical Issues: [N]
Warnings: [N]

[If critical:]
CRITICAL ISSUES FOUND - DO NOT COMMIT
  - [Issue 1]
  - [Issue 2]

[If warnings:]
WARNINGS DETECTED - REVIEW RECOMMENDED
  - [Warning 1]
  - [Warning 2]

[If clean:]
NO MEMORY ISSUES DETECTED
  - 0 bytes leaked
  - 0 invalid access violations
  - Buffer reuse patterns correct

Memory Usage:
  - Peak: [X MB]
  - Total allocations: [N]
  - Per-frame allocations: [N] (Target: 0)

Top Allocation Sites:
  1. [System] - [X MB]
  2. [System] - [X MB]
  3. [System] - [X MB]

Baseline Comparison: [If applicable]
  - Leaks: [+/-X bytes]
  - Allocations: [+/-N]
  - Trend: [Improving/Stable/Degrading]

Full Report: test_results/memory_profiles/memory_profile_YYYY-MM-DD.md

Next Steps:
  [If critical] - Fix issues and re-run profile
  [If clean] - Update baseline: "update memory baseline"
```

## Exit Codes

- **0:** No memory issues detected
- **1:** Critical leaks detected (BLOCKING)
- **2:** Warnings detected (review required)
- **3:** Buffer reuse violations (performance impact)
- **4:** Baseline comparison shows regression
