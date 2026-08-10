/* Copyright (c) 2025 Hammer Forged Games, All rights reserved. Licensed under the MIT License - see LICENSE file for details */

# Review Branch — Hardening, Ownership & Consistency

**Branch:** `review`
**Date:** 2026-06-25 → 2026-08-04
**Review Status:** ✅ APPROVED (multi-wave review completed on branch)
**Overall Grade:** A (92/100) — review already completed; grade is a post-hoc summary of outcomes

---

## Executive Summary

The `review` branch is a multi-wave hardening and consistency pass across the engine: ownership contracts, state-transition lifecycle, header dependency health, build-safety tiers, audio/settings integration, Windows portability, and static-analysis cleanup. It is not a single feature ship — it is the engineering record of review findings turned into concrete code, docs, and tests.

The primary themes are: **managers serve the states** (documented and enforced in `GameStateManager` / full-screen UI clear policy), **header decoupling** (`EntityDataTypes.hpp`, `BehaviorCommonState.hpp`, `ParticleEffectType.hpp`, `Season.hpp`), **build safety** (`ReleaseSafe`, logging/stats macros, SIMD bounds asserts), **runtime correctness** (ParticleManager effect-mutex concurrency, pause/resume controller re-subscribe, JsonReader integer vs float identity, music start delay), and **product polish** (main-menu ambient scene, menu/adventure music, settings-backed volumes).

Work was validated on the branch (Windows MSYS UCRT + macOS), with new dedicated tests for WorkerBudget, TimestepManager, ProjectileRenderController, and controller re-subscribe regression guards. Adjudicated non-issues are permanently recorded in `docs/review-non-issues.md` so future review passes do not re-flag deliberate decisions.

**Impact:**
- ✅ State ownership contract codified: states drive, domain managers serve; exit-then-enter + full-screen UI clear documented and implemented
- ✅ EDM / AI header bloat reduced via free-standing type headers (`EntityDataTypes`, `BehaviorCommonState`)
- ✅ `ReleaseSafe` build tier + `VOIDLIGHT_STATS_ONLY` + documented assert/`NDEBUG`/AVX2 policy
- ✅ ParticleManager effect-map concurrency fixed for strict Windows UCRT (`shared_mutex` → `mutex` + locked `stopEffect`)
- ✅ DayNightController pause/resume no longer re-announces state on every resume
- ✅ SoundManager delayed music start + SettingsManager volume persistence through transitions
- ✅ Main menu ambient GPU scene + menu/adventure music tracks
- ✅ JsonReader preserves integer vs real token identity (SettingsManager typed round-trip)
- ✅ New core/controller tests and Windows sidecar bat runners
- ✅ Architecture docs, dependency analysis (2026-07-03), and review-non-issues ledger
- ✅ Grok C++ design/implement/review specialist agents + cpp-workflows skill

---

## Changes Overview

### Scale

| Metric | Value |
|--------|-------|
| Commits | 36 |
| Files changed | 218 |
| Lines added | ~7,957 |
| Lines removed | ~3,906 |
| Net change | +4,051 |
| Date range | 2026-06-25 → 2026-08-04 |

### Systems Touched

| System | Change Type | Magnitude |
|--------|-------------|-----------|
| ParticleManager | Concurrency + logging/stats alignment | ~1,950 lines touched in `.cpp` |
| EntityDataManager headers | Split free-standing types | −1,245 / +1,131 (move to `EntityDataTypes.hpp`) |
| MainMenuState | Ambient scene + music + polish | +407 lines `.cpp` |
| GameStateManager + states | Transition contract + cleanup | ~365 net across state/manager docs+code |
| SoundManager + Settings | Delayed music + volume wiring | Significant API/docs alignment |
| Core (Logger, CMake, GameEngine) | ReleaseSafe, macros, app target | Build/docs impact |
| AI behaviors / pathfinding / Crowd | Review hardening | Chase/Idle/Patrol/Attack + PathfindingGrid |
| Collision / HierarchicalSpatialHash | Consistency + static-analysis | Moderate |
| EventManager | Cleanup / ownership notes | Moderate |
| Tests | New suites + Windows bat hardening | 4 new test sources + many fixes |
| Docs / tooling | Architecture, non-issues, Grok agents | New + rewritten |

---

## Detailed Changes

### 1. Managers Serve the States — Ownership Contract

States own screen lifecycle and policy; domain managers provide services. `GameStateManager` is stack infrastructure, not a domain manager.

**Changes:**
- Documented ownership table and transition model in `docs/ARCHITECTURE.md`
- Rewrote `docs/managers/GameStateManager.md` around exit-then-enter, overlay vs full-screen replace, and UI clear as a **service** via `UIManager::prepareForStateTransition()`
- Header contract comment on `GameStateManager`; private `clearUIForFullScreenReplace()`
- State `exit()`/`enter()` cleanup across GamePlay, demos, menu, pause, game-over, loading
- GPU frame hooks documented as state-provided (`recordGPUVertices` / `renderGPUScene` / `renderGPUUI`) with engine-owned present

**Transition model (summary):**

| API | Behavior |
|-----|----------|
| `changeState` | Exit top → if stack empty, clear UI → enter new; restore previous on enter failure when possible |
| `changeStateClearingStack` | Exit entire stack → clear UI → enter new |
| `pushState` / `popState` | Overlay: pause/resume underlying; **no** full UI clear |

**Files:** `include/managers/GameStateManager.hpp`, `src/managers/GameStateManager.cpp`, `docs/ARCHITECTURE.md`, `docs/managers/GameStateManager.md`, `docs/gameStates/README.md`, multiple `src/gameStates/*`

---

### 2. Header Decoupling & Dependency Health

Heavy manager headers were split so lightweight consumers (behaviors, enums) no longer pull singleton/threading machinery.

**Changes:**
- **`EntityDataTypes.hpp`** — free-standing SoA component types extracted from `EntityDataManager.hpp` (~1,131 lines). EDM remains the storage/processing layer and includes this header.
- **`BehaviorCommonState.hpp`** — path/common AI state (`PathData` and related) extracted for lock-free batch consumers without full manager includes.
- **`ParticleEffectType.hpp`**, **`Season.hpp`** — enum-only headers for fast dispatch / type safety without manager includes.
- Dependency analyzer scripts updated; new report `docs/architecture/dependency_analysis_2026-07-03.md`.
- Include cleanups on `GameEngine`, `GPURenderer`, `UIManager`, `WorldManager`, `WorldGenerator`, and related `.cpp` files (include-what-you-use style).

**Files:** `include/managers/EntityDataTypes.hpp`, `include/managers/EntityDataManager.hpp`, `include/ai/BehaviorCommonState.hpp`, `include/managers/ParticleEffectType.hpp`, `include/managers/Season.hpp`, dependency analyzer scripts under `.claude/skills/voidlight-dependency-analyzer/`

---

### 3. ParticleManager Concurrency (Windows UCRT)

Stricter winpthreads `shared_mutex` / rwlock behavior on MinGW UCRT64 asserted under write-lock contention. Effect instance maps were also inconsistently locked.

**Changes:**
- Replaced `m_effectsMutex` (`std::shared_mutex`) with plain `std::mutex` — call sites were already almost entirely exclusive.
- `stopEffect()` now takes the same lock as `playEffect()` / weather stop paths.
- Broader ParticleManager touch-up for stats macros, logging alignment, and effect-type header use.

```cpp
// Before — exclusive use of shared_mutex + unlocked stopEffect
mutable std::shared_mutex m_effectsMutex;
// stopEffect() mutated maps without the lock

// After
mutable std::mutex m_effectsMutex;
// playEffect / stopEffect / stopWeatherEffects all use std::lock_guard
```

**Files:** `include/managers/ParticleManager.hpp`, `src/managers/ParticleManager.cpp`

---

### 4. Pause / Resume Controller Re-subscribe

Controllers that announce current state from `subscribe()` re-dispatched that announcement on every `ControllerBase::resume()` because resume re-calls `subscribe()`.

**Changes:**
- `DayNightController` no longer re-announces on resume when nothing changed while suspended.
- Shared test helper `checkNoDuplicateAnnounceOnResume()` for any subscribe-announces-current-state controller.
- DayNight tests extended to cover the regression.

**Files:** `src/controllers/world/DayNightController.cpp`, `include/controllers/world/DayNightController.hpp`, `tests/controllers/common/ControllerResubscribeTests.hpp`, `tests/controllers/DayNightControllerTests.cpp`

---

### 5. SoundManager, Settings, and Music Lifecycle

**Changes:**
- Music start is **delayed** (`MUSIC_START_DELAY_SEC`): `playMusic()` stops current music and schedules a start; `SoundManager::update(dt)` advances the timer; later play/stop cancels or replaces pending requests.
- Volume docs and clamps aligned to intentional **`[0.0, 10.0]`** range (unity at 1.0; values >1 amplify) — pinned by manager runtime tests; recorded in `docs/review-non-issues.md`.
- SettingsManager wired for volume persistence; verified through state transitions (menu ↔ gameplay ↔ pause ↔ settings).
- New tracks: `res/music/menu_theme.ogg`, `res/music/adventure_loop.ogg`.
- Input bindings / settings JSON updated for the new audio paths.

**Files:** `include/managers/SoundManager.hpp`, `src/managers/SoundManager.cpp`, `src/managers/SettingsManager.cpp`, game states that start/stop music, `res/music/*`, `res/settings.json`

---

### 6. Main Menu Ambient Scene

Main menu is no longer a static UI-only screen.

**Changes:**
- Ambient GPU scene recording/presentation in `MainMenuState` (particles / scene hooks, menu music).
- Scene tuning follow-ups; title branding updated (engine name removed from main menu and logo titles).
- Aligns with production menu references used elsewhere in the project.

**Files:** `include/gameStates/MainMenuState.hpp`, `src/gameStates/MainMenuState.cpp`, `src/gameStates/LogoState.cpp`

---

### 7. Build Safety: ReleaseSafe, SIMD Asserts, App Target

**ReleaseSafe** (`-O2`, asserts live, STL hardening on project code only, no `-ffast-math` / wide SIMD aggressiveness):

| Build | Optimization | `assert()` | STL hardening | Aggressive flags | Use case |
|-------|--------------|------------|---------------|------------------|----------|
| Debug | `-O0` | Yes | Platform default | None | Daily dev |
| **ReleaseSafe** | **`-O2`** | **Yes** | **Project-only** | **None** | **Soak / QA** |
| Release | `-O3`+LTO | **Yes** (no `NDEBUG`) | Off | Yes (AVX2 on non-Apple) | Ship |
| Profile | `-O2` | No (`NDEBUG`) | Off | SSE4.2 max | Valgrind/profile |

**Documented traps** (also in `CLAUDE.md` / `BuildSafetyControls.md`):
- Release does **not** define `-DNDEBUG` (only Profile does).
- Non-Apple Release AVX2 is a **hard minimum** via compile-time `__AVX2__` — no runtime dispatch; older CPUs get `SIGILL`.

**Other build/dev quality:**
- `SIMDMath::load_byte16` takes explicit `remaining` and asserts `remaining >= 16` (ParticleManager wide flag load path).
- `add_custom_target(app DEPENDS VoidLight_Template)` → fast `ninja -C build app` without linking all tests.
- SDL3 FetchContent tag bump: `release-3.4.10` → `release-3.4.12`.
- README / CLAUDE updated for the matrix and the app target.

**Files:** `CMakeLists.txt`, `docs/performance/BuildSafetyControls.md`, `include/utils/SIMDMath.hpp`, `README.md`, `CLAUDE.md`

---

### 8. Logging & Stats Macros Across Profiles

Release/ReleaseSafe logging stubs changed so arguments stay referenced under `if (false)` — avoids `-Wunused-variable` fallout when ReleaseSafe enables warnings, without paying runtime cost.

**Changes:**
- `VOIDLIGHT_WARN` / `INFO` / `DEBUG` (+ `_IF` variants) use `do { if (false) { (void)(msg); } } while (0)` in non-Debug builds.
- New **`VOIDLIGHT_STATS_ONLY(...)`** — compiles in Debug **and** ReleaseSafe (`VOIDLIGHT_STATS_ENABLED`), never in Release/Profile. Use for telemetry acceptable in soak, not in ship.
- Call sites across ParticleManager, EventManager, Pathfinder, Crowd, GameStateManager, GPU paths aligned to the correct macro tier.

**Files:** `include/core/Logger.hpp`, managers/AI/GPU/core call sites

---

### 9. JsonReader Integer vs Float Identity

Numbers were always stored as `double`, collapsing authored `1` and `1.0`. SettingsManager typed variants could not round-trip integer vs real.

**Changes:**
- `JsonValue` tracks `m_numberIsInteger` from the authored token.
- `JsonValue::makeNumber(double, bool isInteger)` + `isIntegerNumber()`.
- SettingsManager uses the flag for typed storage; tests cover the collapse case.
- Entry removed from “non-issues” once the real fix landed.

**Files:** `include/utils/JsonReader.hpp`, `src/utils/JsonReader.cpp`, `src/managers/SettingsManager.cpp`, `tests/SettingsManagerTests.cpp`

---

### 10. AI, Collision, Pathfinding Hardening

Multi-wave review fixes (not a redesign of AI policy):

- **Chase / Idle / Patrol / Attack** — consistency cleanups, fewer redundant paths, alignment with behavior state headers.
- **PathfindingGrid / Crowd** — safer bounds and logging/stats macro use; request/path state consistency.
- **HierarchicalSpatialHash / CollisionManager** — static-analysis and review cleanups; dead/duplicate paths removed.
- **EventManager** — ownership/docs alignment with persistent vs transient handlers; combat-commit index caching rationale documented as non-issue.

**Files:** `src/ai/behaviors/*`, `src/ai/pathfinding/PathfindingGrid.cpp`, `src/ai/internal/Crowd.cpp`, `src/collisions/HierarchicalSpatialHash.cpp`, `src/managers/CollisionManager.cpp`, `src/managers/EventManager.cpp`

---

### 11. Review Non-Issues Ledger

`docs/review-non-issues.md` records findings that were **investigated and deliberately not changed**, with rationale (capacity-preserving moves, latent-only races under current lifecycle, intentional volume range, rectangular-world flood-fill bounds, etc.).

Reviewers (human or agent) are expected to read this file before re-flagging items. If an entry becomes wrong because the code path changed, update the file in the same change.

---

### 12. Static Analysis, Windows Portability, Tests

**Static analysis:**
- cppcheck suppressions refreshed; clang-tidy/cppcheck-driven cleanups on CollisionManager, ParticleManager, EntityHandle, EquipmentResources, HarvestConfig, Crowd, settings/menu states.

**Windows:**
- Pathfinder and InputManager test hardening for MSYS UCRT.
- Bat runners updated; new `tests/test_scripts/run_sidecar_tests.bat`.
- ResourcePath path separators where needed.

**New / expanded tests:**
| Asset | Role |
|-------|------|
| `tests/core/WorkerBudgetTests.cpp` | WorkerBudget decision/batch strategy coverage |
| `tests/core/TimestepManagerTests.cpp` | Timestep / clock behavior |
| `tests/controllers/ProjectileRenderControllerTests.cpp` | Projectile render controller suite |
| `tests/controllers/common/ControllerResubscribeTests.hpp` | Shared resume-announce guard |
| DayNight / Settings / GameStateManager / particle tests | Hardened for review fixes |

**Testing note:** Full suite re-runs were performed on the branch during review (including Windows). This changelog does not re-execute the suite; status is inherited from branch validation.

---

### 13. Tooling — Grok C++ Specialists

Agent routing for design → implement → review:

- `.grok/agents/cpp-design-specialist.md`
- `.grok/agents/cpp-specialist.md`
- `.grok/agents/cpp-review-specialist.md`
- `.grok/skills/cpp-workflows/SKILL.md`

Aligned with repo AGENTS/CLAUDE rules (EDM ownership, managers vs controllers, transition cleanup, no exploit of non-issues).

---

## Performance Analysis

This branch is primarily correctness, ownership, and build/dev ergonomics rather than a throughput optimization campaign.

### Memory / Allocation

| Area | Effect |
|------|--------|
| Header splits | Compile-time / include graph only — no runtime footprint change |
| ParticleManager mutex type | `shared_mutex` → `mutex` (smaller lock object; same exclusive usage pattern) |
| SoundManager delayed music | Small fixed pending-request state; no per-frame allocations |
| JsonReader integer flag | One `bool` per numeric `JsonValue` |

### Threading

| Component | Before | After |
|-----------|--------|-------|
| Particle effect instances | `shared_mutex` (exclusive in practice); `stopEffect` unlocked | `mutex` + consistent lock on mutate paths |
| Sound play path | Unlocked (still correct: load futures join before play) | Unchanged model; documented as non-issue |
| AI batch path | Lock-free EDM / behavior access | Preserved; PathData atomics for path-request tokens |

### Build / Dev Loop

| Metric | Change |
|--------|--------|
| `ninja -C build app` | Main binary only — skips full test graph |
| ReleaseSafe | Near-Release speed with live asserts + STL hardening |
| SDL3 | 3.4.12 stable tag |

---

## Architecture Coherence

### Ownership & Lifecycle

| Pattern | Status |
|---------|--------|
| States drive / managers serve | ✅ Documented + GameStateManager policy |
| Full-screen UI clear only when stack empties | ✅ |
| Overlay push/pop no full UI clear | ✅ |
| Persistent vs transient EventManager handlers | ✅ Preserved; non-issues documented |
| Controllers do not mutate AI behavior state in EDM | ✅ Unchanged contract |

### Header Layering

| Consumer | Can use without full manager header |
|----------|-------------------------------------|
| Behavior / light AI code | `EntityDataTypes`, `BehaviorCommonState` |
| Particle type dispatch | `ParticleEffectType` |
| Season consumers | `Season` |

**Result:** Dependency direction Core → Managers → GameStates → Entities/Controllers remains intact; accepted boundary bends unchanged (see dependency analysis report).

---

## Thread Safety Analysis

### Particle effect maps

**Before:** `shared_mutex` with exclusive locks almost everywhere; `stopEffect` mutated without the lock — race under concurrent play/stop.

**After:** Single `mutex`; all mutate sites lock. Avoids winpthreads rwlock assert under contention.

### Documented non-issues (do not “harden” blindly)

- SoundManager load vs play asymmetry (futures joined before play)
- EventManager combat-commit cached EDM indices (deferred destroy, no compaction)
- ThreadSystem diagnostic unlocked counters
- ResourceTemplateManager loaders called under `init()` lock (non-recursive `shared_mutex`)

See `docs/review-non-issues.md` for the full ledger.

---

## Integration Impact

| System | Impact |
|--------|--------|
| GameEngine | SoundManager `update(dt)`; state machine still owns transitions |
| UIManager | Full-screen clear is a **service** for GameStateManager, not a policy owner |
| SettingsManager | Volume persistence + JsonReader integer identity |
| Main menu / Pause / GamePlay | Music start/stop and delayed start semantics |
| Windows CI/local MSYS | Particle + bat test runners hardened |
| Review agents | Prefer non-issues ledger before new findings |

---

## Migration Notes

### Breaking Changes

NONE at the public gameplay API level. Internal contracts clarified rather than replaced.

### API / Behavioral Notes

| Area | Note |
|------|------|
| `SoundManager::playMusic` | Starts after delay; requires per-frame `update(dt)` |
| Volume range | Documented `[0, 10]` — do not “fix” to `[0, 1]` without test + design change |
| `JsonValue` | Integer vs real tokens distinguishable via `isIntegerNumber()` |
| Build types | Fourth type: `ReleaseSafe`; output under `bin/releasesafe` |
| Dev build | Prefer `ninja -C build app` when tests are not needed |
| GameStateManager | `GameStateId`-based transitions (pre-existing direction; docs now match) |

### Configuration

- `res/settings.json`, `res/input_bindings.json` updated for audio / bindings.
- New music assets under `res/music/`.

---

## Testing Summary

### On-branch validation (already completed)

- Multi-wave review fixes closed with targeted and broader test runs (macOS + Windows UCRT).
- New suites: WorkerBudget, TimestepManager, ProjectileRenderController, controller re-subscribe helper.
- Particle concurrency issue discovered and fixed under stricter Windows runtime.

### Representative commands (for re-validation)

```bash
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build
./bin/debug/entity_data_manager_tests
# plus controller / particle / settings / GameStateManager suites as needed

cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=ReleaseSafe && ninja -C build app
```

This changelog generation did **not** re-run the full suite (user-confirmed already tested).

---

## Code Quality Improvements

### Documentation

- `docs/ARCHITECTURE.md` — ownership table, GPU frame flow, state teardown order
- `docs/managers/GameStateManager.md` — current transition API
- `docs/performance/BuildSafetyControls.md` — build matrix and AVX2/`NDEBUG` traps
- `docs/review-non-issues.md` — adjudicated findings
- `docs/architecture/dependency_analysis_2026-07-03.md` — coupling report
- AGENTS / CLAUDE traps for Release asserts and AVX2 minimum

### Static analysis

- cppcheck suppressions + clang-tidy-driven cleanups across managers and types
- Dead exclusive-lock shared_mutex patterns and unused includes removed where review required

---

## Architect Review Summary

**Review Status:** ✅ APPROVED (completed on branch across multiple waves)
**Confidence Level:** HIGH
**Reviewer:** Multi-wave human + agent review (recorded in commits + non-issues ledger)

### Assessment Grades

| Category | Grade | Justification |
|----------|-------|---------------|
| Architecture Coherence | 9.5/10 | Ownership contract explicit; managers-serve-states consistent |
| Performance Impact | 8.5/10 | Not a perf push; mutex change is correctness-first; no known regressions |
| Thread Safety | 9.0/10 | Particle race fixed; intentional non-issues documented |
| Code Quality | 9.0/10 | Header splits, macros, static analysis, docs |
| Testing | 9.0/10 | New core/controller coverage; Windows hardening |

**Overall: A (92/100)**

### Key Observations

**Strengths:**
1. Ownership and transition model written into architecture + code, not only tribal knowledge.
2. Review discipline: non-issues ledger prevents thrashing deliberate designs.
3. Build safety tier and logging/stats split match real soak vs ship needs.
4. Windows concurrency and test portability improved with root-cause fixes.

**Observations:**
1. ParticleManager `.cpp` churn is large; future refactors should keep effect-map lock policy intact.
2. Main menu scene still noted in history as tunable polish (acceptable for this branch).
3. AVX2 hard minimum remains a shipping-policy decision (documented, not softened).

**Recommended Actions (optional follow-ups):**
1. Keep `docs/review-non-issues.md` updated when lifecycle/threading assumptions change.
2. If broader hardware support is required, consider a Profile-like ship baseline or runtime SIMD dispatch (out of scope here).
3. Re-run core + controller + particle suites after any further ParticleManager edits.

---

## Future Enhancements (Optional)

### Low Priority

1. **Main menu scene polish** (effort: small–medium)  
   - Lighting/particle balance and transition fades.

2. **Dependency analyzer automation** (effort: medium)  
   - CI gate on layer violations using updated scripts.

### Nice-to-Have

1. **Shared controller re-subscribe coverage** for other announce-on-subscribe controllers beyond DayNight.

---

## Files Modified (High Signal)

```
CMakeLists.txt
├─ ReleaseSafe build type + project-only hardening flags
├─ add_custom_target(app)
└─ SDL3 release-3.4.12

include/managers/EntityDataTypes.hpp          (new — SoA types split)
include/ai/BehaviorCommonState.hpp            (new)
include/managers/ParticleEffectType.hpp       (new)
include/managers/Season.hpp                   (new)
include/managers/EntityDataManager.hpp        (slimmed via include)
include/managers/ParticleManager.hpp          (mutex + API alignment)
include/managers/SoundManager.hpp             (delayed music + volume docs)
include/managers/GameStateManager.hpp         (transition contract)
include/core/Logger.hpp                       (release stubs + STATS_ONLY)
include/utils/JsonReader.hpp                  (integer number identity)
include/utils/SIMDMath.hpp                    (load_byte16 remaining assert)

src/managers/ParticleManager.cpp              (concurrency + stats macros)
src/managers/GameStateManager.cpp             (full-screen UI clear path)
src/managers/SoundManager.cpp                 (update + delayed start)
src/managers/SettingsManager.cpp              (typed JSON + volumes)
src/gameStates/MainMenuState.cpp              (ambient scene + music)
src/gameStates/*                              (transition/lifecycle cleanup)
src/controllers/world/DayNightController.cpp  (no re-announce on resume)
src/ai/behaviors/* , pathfinding, Crowd       (review hardening)
src/collisions/HierarchicalSpatialHash.cpp
src/managers/CollisionManager.cpp / EventManager.cpp / PathfinderManager.cpp

docs/ARCHITECTURE.md
docs/managers/GameStateManager.md
docs/performance/BuildSafetyControls.md       (new)
docs/review-non-issues.md                     (new)
docs/architecture/dependency_analysis_2026-07-03.md (new)

tests/core/WorkerBudgetTests.cpp              (new)
tests/core/TimestepManagerTests.cpp           (new)
tests/controllers/ProjectileRenderControllerTests.cpp (new)
tests/controllers/common/ControllerResubscribeTests.hpp (new)
tests/test_scripts/run_sidecar_tests.bat      (new)
tests/**                                      (Windows + review hardening)

.grok/agents/cpp-*.md + skills/cpp-workflows  (new tooling)
res/music/menu_theme.ogg , adventure_loop.ogg
```

**Total Changes:**
- Lines added: ~7,957
- Lines removed: ~3,906
- Files changed: 218
- Net: +4,051 lines
- Commits: 36

---

## Commit History

```bash
2278a1b0 removed engine from logo state as well
701397ad removed engine from the main menu title
7d7fc5ee CPP check and Clang tidy fixes with review.
b4369b13 state cleanup and re-work along with documenting the Manager state contract
5c83b2b3 review tweaks
f2fb0ac4 json reader float collapse. fix
65864d51 deep rview fixes
25e457cb grok tooling
9ebf7b59 state and menu transition fixes, along with a music delay.
4da571f6 main readme update for the the new quick ninja -C build app option
536489f1 logger macro heck clean up. Fall out from ReleaseSafe
a13317e1 logging and debug macros cleaned up and used correctly accross all build profiles.
41f3fc1c issue doc updated
0ccfc3bf build saftey and simd assert to assure particles dont have issues.
93621df0 release safe added to build options
41386710 added a build app for faster dev builds that don't include tests
d51f5f97 main menu scene tuning
bbbdae69 added main menu scene and added main menu track. Scene still need some adjustment
769be454 particle manager concurrency issue found on stricter windows msys UCRT
b10577f8 windows test fixing and hardening, working correctly
9a63dec7 windows test fixes
fe7b82d0 pause re-subscribe bug fix and test for daynight controller fixed
6a9060db updated
5f92f179 updated
9b8b5687 updated
19a1bfbf fixed mac compile issues
da3b53dc added an song track and wired up sound manager with settings managere and verified persistance through State transitions.
40274b81 dependency report updated
20e923e2 more header include denpendency clean up and optmizations
4af6ef93 updated SDL version
938f7c28 Dependency analyzer clean up and update and fixes made
6c4bd73e Architecture doc and dependency analyzer update
1c388083 claude reivew support file created
e6ea8dbd review 2
f018562b update to docs
3bc02d51 first review wave changes
```

### Suggested merge message

```bash
git commit -m "docs(changelog): record review branch hardening and ownership work

- Managers-serve-states contract, GameStateManager exit-then-enter, UI clear policy
- EntityDataTypes/BehaviorCommonState/enum header splits; dependency analysis
- ReleaseSafe, logging/stats macros, SIMD load_byte16 remaining assert, ninja app
- ParticleManager effect-mutex concurrency; DayNight re-subscribe guard
- Sound delay + settings volumes; main menu scene and music tracks
- JsonReader integer identity; Windows test/bat hardening; Grok C++ agents

Refs: Review Branch Changelog"
```

*(Do not commit unless requested — changelog only.)*

---

## References

**Related Documentation:**
- `docs/ARCHITECTURE.md`
- `docs/managers/GameStateManager.md`
- `docs/performance/BuildSafetyControls.md`
- `docs/review-non-issues.md`
- `docs/architecture/dependency_analysis_2026-07-03.md`
- `docs/gameStates/README.md`

**Related Changelogs:**
- `changelogs/CHANGELOG_AUDIT_REFACTOR.md` — prior codebase-wide audit pattern
- `changelogs/CHANGELOG_ARCHITECTURE_UPDATE.md`
- `changelogs/CHANGELOG_EDM_DATA_ORIENTED_REFACTOR.md`

**Related Workstreams:**
- Multi-wave code review on `review` (first wave → deep review → static analysis)
- Windows MSYS UCRT portability for particles and tests

---

## Changelog Version

**Document Version:** 1.0  
**Last Updated:** 2026-08-04  
**Status:** Final — Ready for merge documentation (not committed automatically)

---

**END OF CHANGELOG**
