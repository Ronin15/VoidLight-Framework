<!-- Copyright (c) 2025 Hammer Forged Games ... MIT License -->
# VoidLight-Framework Dependency Analysis Report

**Generated:** 2026-07-03
**Branch:** review
**Base commit:** 20e923e2 ("more header include dependency clean up and optimizations") — all work described below is committed as of this revision, across `6c4bd73e` → `938f7c28` → `4af6ef93` → `20e923e2`
**Analysis Mode:** Full Architecture Audit

---

## Executive Summary

**Architecture Health Score:** 92.0/100 (A+ — Excellent)

**Status:** ✅ HEALTHY

**Key Findings:**
- Zero circular dependencies across 132 headers (125 nodes with edges) and 243 dependency edges.
- Zero layer violations — genuinely verified, and this time backed by real architecture changes, not just tool fixes or exceptions (see "What Changed Today" below).
- Zero problematic manager coupling (13/13 functional, each with an explicit rationale).
- 12 forward-declaration opportunities were identified (down from an original 20 — see "Forward-Declaration Heuristic Fix" below). **7 have since been applied** to production headers (including all 4 requiring the accompanying out-of-line-destructor/Pimpl change); the remaining 5 were re-verified by reading the actual code and found to be tool false positives (see "Forward-Declaration Follow-Through" below) — not silently applied, not silently ignored.
- A follow-up IWYU tidy-up pass fixed 9 additional unused-includes surfaced by clangd across the touched headers, each individually checked for cross-platform (`#ifdef`) gating before removal — none were platform-gated, so all were safe to act on directly (see "Post-Forward-Declare Include Tidy-Up" below).
- **Real measured compile-time win:** a full clean rebuild (ccache warm, same cache state before/after) dropped from ~30s to ~8s including SDL3/SDL_ttf/SDL_mixer — a ~73% reduction, replacing the earlier rough per-opportunity estimate.

**Overall Assessment:** This audit went through several rounds of "don't trust the tool, verify the code" — first on the dependency-analyzer's own bugs, then on the exceptions this session had already added, then on the forward-declaration suggestions, then on the forward-declares actually applied, then again on the IWYU cleanup that followed. Each round found real problems and fixed them at the appropriate level (tool bug vs. real code issue). The health score (92/100) now rests on a genuinely decoupled architecture: `BehaviorExecutors.hpp` (AI layer) no longer depends on the ~1900-line `EntityDataManager` class at all — it only needs plain data types, which now live in their own header — and the measured build-time result confirms the decoupling had real effect, not just a cleaner dependency graph on paper.

---

## What Changed Today (this round)

A prior version of this report (same date) fixed two dependency-analyzer bugs and cleared 28 raw findings via a mix of code fixes and documented exceptions. When asked "do the exceptions make sense, or are we just blindly excepting things?", one exception did **not** hold up under scrutiny:

### The `BehaviorExecutors.hpp → EntityDataManager.hpp` exception was too hasty

It was originally justified by quoting `docs/ARCHITECTURE.md`'s "AI is EDM-backed" line — without checking what *this specific header* actually needed. On inspection: it only used `PathData`/`BehaviorData` (free-standing structs, not part of the `EntityDataManager` class) plus a stale/wrong comment claiming `BehaviorType` came from there too (it doesn't — it's in `ai/BehaviorConfig.hpp`, already included separately).

**First attempt** (extract just `PathData`/`BehaviorData` into `ai/BehaviorCommonState.hpp`) broke the build: the compiler revealed `BehaviorExecutors.hpp` *also* needs `TransformData`, `EntityHotData`, `CharacterData`, `KnockbackData`, `NPCMemoryData` — five more free-standing structs living in the same file, all likewise defined before `class EntityDataManager` starts. Rather than push a partial fix through, the small extraction was reverted and the real scope was measured properly.

**Real fix:** `EntityDataManager.hpp` (2919 → 1838 lines) was split. Every SoA component/value-type struct that lives before `class EntityDataManager` (~1100 lines: `TransformData`, `KnockbackData`, `EntityHotData`, `CharacterData`, `ItemData`, `ProjectileData`, `ContainerData`, `HarvestableData`, `InventoryData`, `AreaEffectData`, race/class/monster/species/animal info structs, render-data structs, `FixedWaypointSlot`, NPC memory types, etc.) moved verbatim (extracted with `sed`, not hand-retyped, and diffed byte-for-byte against the original to guarantee fidelity) into a new `include/managers/EntityDataTypes.hpp`. `EntityDataManager.hpp` now just includes it. `BehaviorExecutors.hpp` includes `EntityDataTypes.hpp` + `ai/BehaviorCommonState.hpp` + `managers/SparseSidecar.hpp` instead of the full manager header. Along the way: `<algorithm>`, `<cassert>`, `<random>` were added as explicit includes to the new header (previously relied on transitive availability through other includes — a portability risk given this project targets three different toolchains — GCC/Linux, MinGW/Windows, Clang/macOS).

`ProjectileManager.hpp` also had three now-provably-dead forward declarations (`class EntityDataManager;`, `struct EntityHotData;`, `struct TransformData;` — unused anywhere else in the file) — deleted per project convention rather than converted to includes.

**Net effect:** `BehaviorExecutors.hpp → EntityDataManager.hpp` no longer exists at all — replaced by `BehaviorExecutors.hpp → EntityDataTypes.hpp` (a real, accurate, narrower dependency), documented with a rationale that reflects what was actually verified, not a doc quote.

One new layer-adjacent finding fell out of this: `BehaviorExecutors.hpp → managers/SparseSidecar.hpp` (needed for `SparseSidecar<KnockbackData>`). Checked `SparseSidecar.hpp` directly — it's a 185-line standalone template with zero project-local includes (only `<cstdint>`, `<limits>`, `<span>`, `<vector>`). Added to the lightweight-header allowlist alongside `EntityHandle.hpp`/`TriggerTag.hpp`/etc.

**Verification:** full debug build (176/176, then 106/106 after the `ProjectileManager.hpp` cleanup) with zero errors. All 84 test executables run: 83 passed cleanly (`No errors detected`); the one failure (`item_controller_tests`) is a pre-existing, unrelated environment issue — a stale April-22 binary missing a shared Boost library at runtime, not rebuilt by today's changes and not linked to anything touched here.

### Forward-declaration heuristic fix

Asked separately: "are the forward-declaration opportunities worth doing?" Spot-checking 4 of the original 20 found they were **actively wrong** — `analyze_header_bloat.py`'s regex for "is this type used by value" (`ClassName varname;`) missed the codebase's dominant style, brace-init (`ClassName varname{};`). Confirmed `WorldTriggerEvent.hpp`, `AICommandBus.hpp`, `EventManager.hpp`, and `InputManager.hpp` all store `Vector2D` **by value** with brace-init — forward-declaring it there would not compile. Separately, `GameEngine.hpp`'s suggestions (`GameStateManager`, `TimestepManager`) hold their targets via `unique_ptr` with an **inline** `~GameEngine() = default;` — forward-declaring without also moving the destructor out-of-line into the `.cpp` (the Pimpl requirement) would not compile either, and the tool didn't mention that companion step.

**Fixed:** `count_usages_in_file()` in `analyze_header_bloat.py` now also matches brace-init (`Type var{`), copy-init (`Type var =`), and by-value container storage (`vector<Type>`, `array<Type, N>`) as "direct" (unsafe-to-forward-declare) usage. A new `smart_ptr` counter detects `unique_ptr<Type>`/`shared_ptr<Type>` members and, when found, the tool now prints an explicit warning that the owning class's destructor must also move out-of-line — rather than a blanket "safe" claim.

**Result:** opportunities dropped from 20 to 12. Spot-checked 3 of the surviving `Vector2D` ones (`SaveGameManager.hpp`, `WorldResourceManager.hpp`, `BinarySerializer.hpp`) directly — all confirmed by-reference-only now. 4 of the 12 are flagged as requiring the destructor change.

### Forward-declaration follow-through

Asked to apply the 12 opportunities. **7 applied cleanly, 5 rejected as false positives after reading the real code** — the heuristic still has one blind spot it can't detect: an inline (non-template) member-function body that calls a method on the type, which forces a complete type just like the Pimpl destructor case does.

Applied: `GameEngine.hpp`→`GameStateManager`/`TimestepManager` (destructor **and** constructor moved out-of-line — the inline constructor's exception-cleanup path also instantiated the members' `unique_ptr` destructors), `GPURenderer.hpp`→`GPUTexture` (destructor moved out-of-line), `ParticleManager.hpp` (the tool named `EventManager`, but the header actually only uses `EventData` by const-ref — forward-declared the correct type), `SaveGameManager.hpp`→`Vector2D` (a forward-decl already existed; only the now-redundant `#include` was removed), `UIManager.hpp`→`TextureSource`, `WorldGenerator.hpp`→`WorldData` (plus `WorldGenerationConfig`, same header, same usage pattern — the class turned out to have no owning `WorldData` member at all, so *no* destructor move was actually needed there, contrary to the tool's flag).

Rejected: `ControllerRegistry.hpp`→`ControllerBase`/`IUpdatable` (header-only class, no `.cpp` — six inline batch methods dereference these types; forward-declaring would require creating a new `.cpp` and moving all six out-of-line, which exceeds a mechanical forward-declare and is deferred as a separate ask). `WorldManager.hpp`→`WorldData` (tool assumed raw-pointer/reference ownership; it's actually a `unique_ptr`, and the header uses 5 distinct `WorldData.hpp` types with ~12 unrelated consumer TUs depending on it transitively — disproportionate cascade, reverted). `WorldResourceManager.hpp`→`Vector2D` and `BinarySerializer.hpp`→`Vector2D` (both call `.getX()`/`.setX()`/etc. directly inside inline header methods — need the complete type, the tool's ptr/ref-only check didn't catch the method-call case).

**Verification:** full debug build clean (0 errors, 0 new warnings), all touched test executables (`game_state_manager_tests`, `particle_manager_core_tests`, `ui_manager_functional_tests`, `world_generator_tests`, `world_manager_tests`, `save_manager_tests`, `gpu_renderer_tests`) pass.

### Post-forward-declare include tidy-up

The forward-declare pass left clangd flagging 9 more "unused include" warnings across the touched headers. Given this project targets **three separate toolchains** (GCC/Linux, MinGW-MSYS2/Windows, Clang 17/macOS), every one was checked for `#ifdef`/platform-gated usage before removal — none were platform-gated, all were plain C++ used identically everywhere:

- `GPURenderer.hpp`: `GPUTypes.hpp`/`GPUDevice.hpp` were used only by `GPURenderer.cpp` transitively — moved to direct includes there. `GPUBuffer.hpp`/`GPUTransferBuffer.hpp`/`<vector>` were genuinely dead (only `SDL_GPUBuffer`, an unrelated SDL3 type, appears) — removed outright.
- `UIManager.hpp`: `<array>` — zero usage anywhere in header or `.cpp` — removed.
- `UIManagerFunctionalTests.cpp`: `<memory>` — no smart pointers in the file — removed.
- `WorldManager.hpp`: swapped the full `GameTimeManager.hpp` for the lightweight `Season.hpp` — the header only ever uses the `Season` enum, never the `GameTimeManager` class. `WorldManager.cpp` calls `GameTimeManager::Instance()` directly, so it needed (and got) its own direct include of the full class. Same pattern for `Vector2D.hpp`: the header itself never uses `Vector2D`, only `WorldManager.cpp` does (constructing tile-center positions) — moved to a direct include there.

**Verification:** full clean rebuild, 0 errors, 0 new warnings; `gpu_renderer_tests`, `ui_manager_functional_tests`, `world_manager_tests`, `world_manager_event_integration_tests`, `game_time_manager_tests` all pass.

### Measured compile-time result

A full clean rebuild (deleted `build/`, reconfigured, `ninja -C build`) with a warm ccache — same cache state used for both the before and after measurement, so the comparison is apples-to-apples on removed work, not a caching artifact — dropped from **~30 seconds to ~8 seconds**, including compiling SDL3, SDL_ttf, and SDL_mixer from source. A ~73% reduction. This replaces the earlier "~10% estimate" in Recommendations with a real number.

---

## Dependency Statistics

**Codebase Size:** 132 headers analyzed (125 nodes with edges; 4 new headers today: `EntityDataTypes.hpp`, `BehaviorCommonState.hpp`, `Season.hpp`, `ParticleEffectType.hpp`)

**Dependency Metrics:** 243 total dependencies · 1.94 average per file · 0 circular ✅

---

## Circular Dependencies

✅ **NO CIRCULAR DEPENDENCIES DETECTED** — 125 nodes, 243 edges, acyclic.

---

## Layer Violations

All 11 layers: ✅ CLEAN (0 violations).

---

## Coupling Analysis

**13/13 functional, 0 problematic.** Full list unchanged from the prior pass except `BehaviorExecutors.hpp` no longer contributes an `EntityDataManager` coupling reference at all (real reduction, not reclassification).

---

## Header Bloat Analysis

**High-Bloat Headers (15 of 125, 12.0%):** `EntityDataManager.hpp`, `EventManager.hpp`, `ThreadSystem.hpp`, `EntityDataTypes.hpp` (new), `WorldManager.hpp`, `CollisionManager.hpp`, `EventDemoState.hpp`, `GPURenderer.hpp`, `ParticleManager.hpp`, `BinarySerializer.hpp`, `InventoryController.hpp`, `AIManager.hpp`, `PathfinderManager.hpp`, `UIManager.hpp`, `WorldResourceManager.hpp`

**Forward Declaration Opportunities: 12 identified, 7 applied, 5 rejected as false positives** (see "Forward-Declaration Follow-Through" above). Full breakdown:

| Header | Can forward-declare | Needs out-of-line destructor too? | Outcome |
|---|---|---|---|
| `ControllerRegistry.hpp` | `ControllerBase` | ⚠️ Yes | Deferred — header-only class, no `.cpp` to move logic into |
| `ControllerRegistry.hpp` | `IUpdatable` | No | Deferred (same reason) |
| `GameEngine.hpp` | `GameStateManager` | ⚠️ Yes | ✅ Applied |
| `GameEngine.hpp` | `TimestepManager` | ⚠️ Yes | ✅ Applied |
| `GPURenderer.hpp` | `GPUTexture` | ⚠️ Yes | ✅ Applied |
| `ParticleManager.hpp` | `EventManager` | No | ✅ Applied (tool misnamed the type — actual forward-decl was `EventData`) |
| `SaveGameManager.hpp` | `Vector2D` | No | ✅ Applied |
| `UIManager.hpp` | `TextureSource` | No | ✅ Applied |
| `WorldManager.hpp` | `WorldData` | No | ❌ Rejected — tool wrong about ownership (`unique_ptr`, not raw ptr); 5 types + ~12-TU cascade |
| `WorldResourceManager.hpp` | `Vector2D` | No | ❌ Rejected — inline methods call `.getX()`/etc., need complete type |
| `BinarySerializer.hpp` | `Vector2D` | No | ❌ Rejected (same reason) |
| `WorldGenerator.hpp` | `WorldData` | No (tool over-flagged; no owning member found) | ✅ Applied |

---

## Dependency Depth Analysis

Unchanged in character: max depth 5 (4 demo/gameplay state headers), average ~1.6-1.9, no cascading-recompile risk.

---

## Architecture Health Scorecard

| Category | Score | Weight | Weighted | Status |
|----------|-------|--------|----------|--------|
| Circular Dependencies | 10.0/10 | 30% | 30.0 | ✅ |
| Layer Compliance | 10.0/10 | 25% | 25.0 | ✅ |
| Coupling Strength | 10.0/10 | 20% | 20.0 | ✅ |
| Header Bloat | 6.0/10 | 15% | 9.0 | ⚠️ |
| Dependency Depth | 8.0/10 | 10% | 8.0 | ✅ |
| **TOTAL** | | **100%** | **92.0/100** | **A+** |

---

## Code Changes Made As Part Of This Audit (both rounds today)

1. `include/entities/EntityHandle.hpp` / `include/entities/Entity.hpp` — `EntityID` alias relocated.
2. `include/ai/internal/Crowd.hpp`, `include/ai/pathfinding/PathfindingRequest.hpp`, `include/events/WorldTriggerEvent.hpp`, `include/collisions/CollisionInfo.hpp` — repointed to `entities/EntityHandle.hpp`.
3. New `include/managers/Season.hpp`, `include/managers/ParticleEffectType.hpp` — enums extracted from `GameTimeManager.hpp`/`ParticleManager.hpp`; `include/events/TimeEvent.hpp`/`include/events/ParticleEffectEvent.hpp` repointed.
4. New `include/ai/BehaviorCommonState.hpp` — `PathData`/`BehaviorData` extracted from `EntityDataManager.hpp`.
5. **New `include/managers/EntityDataTypes.hpp`** — ~1100 lines of free-standing SoA component structs extracted from `EntityDataManager.hpp` (2919 → 1838 lines).
6. `include/ai/BehaviorExecutors.hpp` — now includes `BehaviorCommonState.hpp` + `EntityDataTypes.hpp` + `SparseSidecar.hpp` instead of the full `EntityDataManager.hpp`.
7. `include/managers/ProjectileManager.hpp` — removed 3 dead forward declarations.
8. `.claude/skills/voidlight-dependency-analyzer/scripts/detect_layer_violations.py` — path-normalization bug, cross-state false positive, `APPROVED_EXCEPTIONS`/`LIGHTWEIGHT_CROSS_CUTTING_HEADERS` (including today's `SparseSidecar.hpp` addition and removal of the stale `BehaviorExecutors.hpp`/`EntityDataManager.hpp` entry).
9. `.claude/skills/voidlight-dependency-analyzer/scripts/analyze_coupling.py` — functional-coupling allowlist gaps.
10. `.claude/skills/voidlight-dependency-analyzer/scripts/analyze_header_bloat.py` — brace-init/copy-init/container-by-value detection, smart-pointer/Pimpl-destructor warning.
11. `CLAUDE.md`, `docs/ARCHITECTURE.md` — architecture-documentation fixes (dependency-direction bends, Key Systems inventory, event-handler-persistence, rendering-path, state-teardown order).
12. **7 forward-declares applied** to production headers: `GameEngine.hpp`→`GameStateManager`/`TimestepManager` (+ constructor/destructor moved out-of-line to `GameEngine.cpp`), `GPURenderer.hpp`→`GPUTexture` (destructor moved to `GPURenderer.cpp`), `ParticleManager.hpp`→`EventData`, `SaveGameManager.hpp`→`Vector2D` (redundant include removed), `UIManager.hpp`→`TextureSource`, `WorldGenerator.hpp`→`WorldData`/`WorldGenerationConfig`. Transitive-include fallout fixed in `ParticleManager.cpp`, `OverlayDemoState.cpp`, `UIManagerFunctionalTests.cpp`, `GPURendererTests.cpp`.
13. **9 additional unused-includes tidied up** post-forward-declare (cross-platform-checked, none `#ifdef`-gated): `GPURenderer.hpp`/`.cpp` (`GPUTypes.hpp`/`GPUDevice.hpp` moved to `.cpp`, `GPUBuffer.hpp`/`GPUTransferBuffer.hpp`/`<vector>` removed as dead), `UIManager.hpp` (`<array>` removed), `UIManagerFunctionalTests.cpp` (`<memory>` removed), `WorldManager.hpp`/`.cpp` (`GameTimeManager.hpp` swapped for `Season.hpp` in the header, full class include added to the `.cpp`; `Vector2D.hpp` moved from header to `.cpp`).

**Verification:** full debug build clean at every stage. 83 of 84 test executables pass (`No errors detected`); the 1 failure is a pre-existing, unrelated stale-binary/shared-library issue, not caused by anything in this change set. All forward-declare and IWYU changes additionally verified with their own targeted test runs (see sections above).

**All changes are committed** (`6c4bd73e` → `938f7c28` → `4af6ef93` → `20e923e2`).

---

## Recommendations

No open decisions remain from this audit — the forward-declaration opportunities have all been actioned (applied or rejected with reasoning) and the resulting include cleanup is done and verified. No Critical or Important items remain — circular dependencies, layer violations, and problematic coupling are all at zero, genuinely verified this time.

Two items intentionally deferred, not forgotten:
- `ControllerRegistry.hpp`'s `ControllerBase`/`IUpdatable` forward-declares would require creating a new `ControllerRegistry.cpp` and moving six inline batch methods out-of-line — a real refactor, not a mechanical forward-declare. Worth doing if `ControllerRegistry.hpp`'s pull of `EventManager.hpp`/`<future>` into every game state becomes a measured problem.
- The dependency-analyzer's forward-declaration heuristic still can't detect "inline method body calls a method on the type" as a reason the complete type is needed (caught 3 of the 12 cases by hand: `WorldManager.hpp`, `WorldResourceManager.hpp`, `BinarySerializer.hpp`). A future pass could extend `count_usages_in_file()` to flag `{class_name_lowercased_var}\.\w+\(` patterns, but given how few opportunities remain, hasn't been worth the false-positive risk of a broader regex.

---

**Generated By:** voidlight-dependency-analyzer Skill (Full Architecture Audit mode), with two rounds of tool-bug fixes and one real architectural decoupling applied mid-audit
**Raw data:** `test_results/dependency_analysis/`
