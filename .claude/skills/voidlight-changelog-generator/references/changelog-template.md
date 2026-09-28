# Changelog Template

Load when writing the changelog file (Step 5 of `SKILL.md`). Omit any **optional** section that
has no real content — never fill it with placeholders or invented numbers. Recent examples to
match for tone and density: `changelogs/CHANGELOG_REVIEW_BRANCH.md`,
`changelogs/CHANGELOG_ITEMS_UPDATE.md`.

````markdown
/* Copyright (c) 2025 Hammer Forged Games, All rights reserved. Licensed under the MIT License - see LICENSE file for details */

# [Update Name]

**Branch:** `[branch]`
**Date:** [first commit date] → [last commit date]
**Review Status:** [✅ APPROVED / ⚠️ PENDING / ❌ NEEDS CHANGES / Not reviewed]
**Overall Grade:** [A (92/100) — only if a review ran; otherwise omit]

---

## Executive Summary

[2–3 paragraphs: what changed, why, impact.]

**Impact:**
- ✅ [Key change 1]
- ✅ [Key change 2]

---

## Changes Overview

### Scale

| Metric | Value |
|--------|-------|
| Commits | [X] |
| Files changed | [X] |
| Lines added | ~[X] |
| Lines removed | ~[X] |
| Net change | [+/-X] |
| Date range | [start → end] |

### Systems Touched

| System | Change Type | Magnitude |
|--------|-------------|-----------|
| [System] | [brief] | [lines/scope] |

---

## Detailed Changes

### 1. [Change Title]

[1–2 factual sentences: what changed and why.]

**Changes:**
- [Specific change]

```cpp
// Before
[snippet, only if illustrative]
// After
[snippet]
```

**Files:** `path/to/file`

---

## Performance Analysis            <!-- optional: only measured or code-evident facts -->

| Component / Operation | Before | After | Delta |
|-----------------------|--------|-------|-------|

## Threading / Thread Safety       <!-- optional -->

[Old vs new synchronization, lock order, WorkerBudget/ThreadSystem changes.]

## Architecture Coherence          <!-- optional -->

| Manager | [Pattern] | [Pattern] |
|---------|-----------|-----------|

## Integration Impact              <!-- optional -->

- **[System]:** [impact]

## Migration Notes

**Breaking changes:** [list or NONE]
**API changes:** [list or NONE]
**Behavioral changes:** [list or NONE]

---

## Testing Summary

[What was actually run this session, with exact executables / scripts and results; or
"Not re-run for this changelog (user-confirmed already tested)". New or changed test
sources on the branch.]

```bash
# Representative commands for re-validation
ninja -C build
./bin/debug/<test_executable>
```

---

## Architect Review Summary       <!-- only if game-systems-architect ran -->

**Review Status:** [...] · **Confidence:** [HIGH/MEDIUM/LOW] · **Reviewer:** game-systems-architect

| Category | Grade | Justification |
|----------|-------|---------------|
| Architecture Coherence | [X]/10 | |
| Performance Impact | [X]/10 | |
| Thread Safety | [X]/10 | |
| Code Quality | [X]/10 | |
| Testing | [X]/10 | |

**Strengths / Observations / Recommended Actions:** [numbered lists]

---

## Future Enhancements (Optional)

1. **[Enhancement]** — [description, rationale]

---

## Files Modified (High Signal)

```
[file]
├─ [function/area]   ([what changed])
```

---

## Commit History

```bash
[git log --oneline main..HEAD output]
```

### Suggested merge message

```bash
git commit -m "[type]([scope]): [summary]

- [detail]

Refs: [Update Name]"
```

*(Do not commit unless requested — changelog only.)*

---

## References

**Related Documentation:** [`docs/...` paths touched or relevant]
**Related Changelogs:** [`changelogs/...`]

---

## Changelog Version

**Document Version:** 1.0
**Last Updated:** [YYYY-MM-DD]
**Status:** [Draft / Final]

---

**END OF CHANGELOG**
````

## Grading scale (only when a review ran)

- A+ (95–100): exceptional, no observations
- A (90–94): excellent, minor observations
- B+ (85–89): good, some concerns
- B (80–84): acceptable, needs improvement
- Below B: not ready for merge
