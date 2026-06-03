<!-- Copyright (c) 2025 Hammer Forged Games ... MIT License -->
# VoidLight-Framework Dependency Analysis Report

**Generated:** 2026-03-31
**Branch:** audit
**Analysis Mode:** Full Architecture Audit

> Historical snapshot: this report preserves the dependency graph observed on
> 2026-03-31. Current API and ownership guidance lives in the subsystem docs and
> headers. Names listed below may include types that have since been retired.

---

## Executive Summary

**Architecture Health Score:** 81/100

**Status:** HEALTHY (minor issues to address)

**Key Findings:**
- Zero circular dependencies across all 119 headers and 205 dependency edges — the graph is a clean DAG
- Three layer violations exist: `GameEngine.hpp` includes a manager, `GameStateManager.hpp` includes a game state, and `BinarySerializer.hpp` (utils) includes a core header — all are architectural boundary bends, two of which are structurally justified
- Header bloat is not a concern: the highest local-include count is 9 (GPURenderer.hpp, EventDemoState.hpp), well under any problematic threshold
- Dependency depth is very shallow (avg 2.2, max 5) indicating a flat, maintainable hierarchy
- `EntityDataManager` is the engine's central data hub — 34 files include it at the .cpp level, which is expected and by design; only 3 headers pull it in directly
- One bidirectional .cpp-level coupling exists between `ResourceFactory` and `ResourceTemplateManager` — both reference each other in their implementations; this does not create a circular header dependency but warrants attention
- `CollisionManager.cpp` references `AIManager` by name 6 times in comments only — no functional coupling; the 6-reference threshold is met by comments, not calls

---

## Dependency Statistics

| Metric | Value |
|--------|-------|
| Total headers scanned | 119 |
| Total source files (.cpp) | 97 |
| Total test files | 85 |
| Dependency graph edges | 205 |
| Unique nodes in dependency graph | 88 |
| Circular dependencies | 0 |
| Layer violations | 3 |
| Headers with 10+ local includes | 0 |
| Max local include count (any header) | 9 |
| Average dependency depth | 2.2 |
| Max dependency depth | 5 |
| Isolated headers (no local deps) | 31 |

### Headers Per Layer

| Layer | Header Count |
|-------|-------------|
| core | 5 |
| managers | 21 |
| gameStates | 13 |
| entities | 14 |
| controllers | 11 |
| ai | 8 |
| collisions | 5 |
| events | 16 |
| utils | 10 |
| world | 4 |
| gpu | 12 |
| **Total** | **119** |

---

## Circular Dependencies

**Result: NO CIRCULAR DEPENDENCIES DETECTED**

The DFS cycle-detection pass over all 88 nodes and 205 edges in the header dependency graph found zero cycles. The include graph is a strict directed acyclic graph (DAG). This is the strongest possible result for this category.

---

## Layer Violations

The declared dependency direction is: `Core → Managers → GameStates → Entities/Controllers`

Three violations were found:

### Violation 1: GameEngine.hpp (Core) includes GameStateManager.hpp (Manager)

```
include/core/GameEngine.hpp:
    #include "managers/GameStateManager.hpp"
```

**Severity:** Acceptable architectural coupling. `GameEngine` is the top-level orchestrator that owns and drives `GameStateManager`. This relationship is inherent to the design — the engine bootstraps the state machine. However, it technically means `core/` depends on `managers/`. A forward-declaration plus interface could eliminate it if strict layering is desired.

### Violation 2: GameStateManager.hpp (Manager) includes GameState.hpp (GameState)

```
include/managers/GameStateManager.hpp:
    #include "gameStates/GameState.hpp"
```

**Severity:** Acceptable — `GameStateManager` is the owner of game states by design. `GameState.hpp` is a pure abstract base (interface) with no local dependencies of its own, making this a clean upward reference to a contract rather than a concrete implementation.

### Violation 3: BinarySerializer.hpp (Utils) includes Logger.hpp (Core)

```
include/utils/BinarySerializer.hpp:
    #include "core/Logger.hpp"
```

**Severity:** Minor. Utilities should not depend on core infrastructure. `Logger` is a singleton service; `BinarySerializer` should receive log output via a callback or write to `std::ostream`, not by coupling directly to `Logger`. This is a low-priority cleanup.

**Cross-state violations:** None. No game state header includes another game state header.

---

## Coupling Analysis

### Fan-Out (Most Dependencies — Top 15)

| Header | Local Includes |
|--------|---------------|
| GPURenderer.hpp | 9 |
| EventDemoState.hpp | 9 |
| EntityDataManager.hpp | 8 |
| AdvancedAIDemoState.hpp | 7 |
| CollisionManager.hpp | 6 |
| AIManager.hpp | 6 |
| AIDemoState.hpp | 6 |
| GamePlayState.hpp | 6 |
| WorldTriggerEvent.hpp | 5 |
| WorldManager.hpp | 5 |
| SocialController.hpp | 5 |
| HarvestController.hpp | 5 |
| PathfinderManager.hpp | 4 |
| EventManager.hpp | 4 |
| EntityEvents.hpp | 4 |

The highest fan-out values (9) are held by a GPU orchestration header and a demo state — neither is in a hot production path. The heaviest production manager headers (`EntityDataManager`, `CollisionManager`, `AIManager`) all stay at 6–8, which is reasonable for system-level headers.

### Fan-In (Most Depended Upon — Top 15)

| Header | Files Depending On It |
|--------|----------------------|
| Vector2D.hpp | 30 |
| Event.hpp | 14 |
| EntityHandle.hpp | 14 |
| GameState.hpp | 13 |
| ResourceHandle.hpp | 11 |
| EventManager.hpp | 11 |
| Entity.hpp | 9 |
| ControllerBase.hpp | 9 |
| EntityState.hpp | 6 |
| Resource.hpp | 5 |
| IUpdatable.hpp | 5 |
| WorldData.hpp | 4 |
| NPCRenderController.hpp | 4 |
| EventTypeId.hpp | 4 |
| ControllerRegistry.hpp | 4 |

`Vector2D.hpp` has the highest fan-in at 30, which is expected for a fundamental math type. `Event.hpp` and `EntityHandle.hpp` at 14 each are similarly foundational. These are stable leaf-level types and high fan-in is not a problem here.

`EntityDataManager` has 34 .cpp-level inclusions but only 3 header-level inclusions — this is excellent design, keeping the heavy SoA data store out of the include chain while allowing .cpp files full access.

### Manager-to-Manager Coupling Matrix

No manager header includes another manager header directly. All inter-manager dependencies exist only at the .cpp implementation level, which is the correct pattern.

#### .cpp-Level Manager Cross-References (> 5 references)

| Source Manager | Target Manager | References | Notes |
|---------------|---------------|------------|-------|
| UIManager | UIConstants | 120 | Expected — UIConstants is a shared constants file |
| CollisionManager | EntityDataManager | 24 | Expected — needs entity positions for collision |
| AIManager | EntityDataManager | 20 | Expected — AI reads/writes behavior state |
| CollisionManager | EventManager | 19 | Expected — fires collision events |
| WorldManager | EventManager | 19 | Expected — fires world events |
| EntityDataManager | WorldResourceManager | 16 | Concern — EDM calling up into a spatial index manager |
| ResourceFactory | ResourceTemplateManager | 15 | Bidirectional — see note below |
| GameTimeManager | EventManager | 14 | Expected — fires time events |
| BackgroundSimulationManager | EntityDataManager | 12 | Expected — simulation reads entity tiers |
| CollisionManager | WorldManager | 10 | Expected — needs world geometry |
| AIManager | PathfinderManager | 9 | Expected — AI requests paths |
| ResourceTemplateManager | ResourceFactory | 7 | Bidirectional — see note below |
| WorldResourceManager | EntityDataManager | 7 | Expected — spatial index reads EDM data |
| CollisionManager | AIManager | 6 | Comments only — not functional coupling |
| PathfinderManager | EventManager | 6 | Expected — fires pathfinding events |

**Bidirectional ResourceFactory / ResourceTemplateManager:** Both .cpp files include each other's headers and call each other's methods. This is a design smell — a circular runtime dependency even though the header graph is acyclic. Consider extracting the shared types to a `ResourceTypes.hpp` and having one factory call the other in one direction only.

**EntityDataManager -> WorldResourceManager:** EDM directly calls `WorldResourceManager::Instance()` to unregister harvestables and dropped items during entity destruction. This means the central data store has runtime knowledge of a spatial index. The existing pattern is documented and works, but architecturally EDM should emit an event or use a callback/observer so it does not need to know about the spatial layer.

---

## Header Bloat Analysis

### Headers with 10+ Local Includes

**None found.** The maximum local include count across all 119 headers is 9 (GPURenderer.hpp and EventDemoState.hpp).

### Headers with 7–9 Local Includes

| Count | Header | Path |
|-------|--------|------|
| 9 | GPURenderer.hpp | include/gpu/GPURenderer.hpp |
| 9 | EventDemoState.hpp | include/gameStates/EventDemoState.hpp |
| 8 | EntityDataManager.hpp | include/managers/EntityDataManager.hpp |
| 7 | AdvancedAIDemoState.hpp | include/gameStates/AdvancedAIDemoState.hpp |

`EventDemoState.hpp` at 9 is a demo/test state — acceptable complexity. `GPURenderer.hpp` at 9 is the GPU frame orchestration header — reasonable for its responsibility. Neither requires action.

### Most Frequently Included Headers (Fan-In)

| Fan-In | Header |
|--------|--------|
| 30 | Vector2D.hpp |
| 14 | Event.hpp |
| 14 | EntityHandle.hpp |
| 13 | GameState.hpp |
| 11 | ResourceHandle.hpp |
| 11 | EventManager.hpp |
| 9 | Entity.hpp |
| 9 | ControllerBase.hpp |

All high fan-in headers are stable, foundational types with zero or minimal local includes of their own. There is no problematic "heavyweight hub" that forces recompilation cascades.

---

## Dependency Depth Analysis

### Top 30 Headers by Dependency Depth

| Depth | Header |
|-------|--------|
| 5 | AIDemoState.hpp |
| 5 | AdvancedAIDemoState.hpp |
| 5 | EventDemoState.hpp |
| 5 | GamePlayState.hpp |
| 4 | BehaviorExecutors.hpp |
| 4 | ControllerRegistry.hpp |
| 4 | CombatController.hpp |
| 4 | NPCRenderController.hpp |
| 4 | ResourceRenderController.hpp |
| 4 | SocialController.hpp |
| 4 | DayNightController.hpp |
| 4 | HarvestController.hpp |
| 4 | ItemController.hpp |
| 4 | WeatherController.hpp |
| 4 | CollisionEvent.hpp |
| 4 | ParticleEffectEvent.hpp |
| 4 | AIManager.hpp |
| 4 | CollisionManager.hpp |
| 3 | Crowd.hpp |
| 3 | PathfindingRequest.hpp |
| 3 | CollisionInfo.hpp |
| 3 | ControllerBase.hpp |
| 3 | Player.hpp |
| 3 | TimeEvent.hpp |
| 3 | WorldTriggerEvent.hpp |
| 3 | EntityDataManager.hpp |
| 3 | ParticleManager.hpp |
| 3 | PathfinderManager.hpp |
| 3 | WorldManager.hpp |
| 2 | AICommandBus.hpp |

### Summary Statistics

| Metric | Value |
|--------|-------|
| Total depth sum | 192 |
| Average depth | 2.2 |
| Maximum depth | 5 |
| Headers at depth 5 | 4 (all game states) |
| Headers at depth 4 | 14 |
| Headers at depth 3 | 10 |
| Headers at depth 1–2 | 60 |

The maximum depth of 5 (held by game states) and average of 2.2 indicate an exceptionally flat dependency hierarchy. The performance threshold concern (avg > 8) is not relevant here — this codebase is well within healthy range.

---

## Forward Declaration Opportunities

The following headers include full type definitions where a forward declaration would suffice (type is only used as pointer or reference, not by value):

| Header | Currently Includes | Could Forward-Declare |
|--------|-------------------|----------------------|
| BehaviorExecutors.hpp | BehaviorConfig.hpp | `BehaviorConfig` |
| BehaviorExecutors.hpp | EventManager.hpp | `EventManager` |
| Crowd.hpp | Vector2D.hpp | `Vector2D` |
| ControllerRegistry.hpp | ControllerBase.hpp | `ControllerBase` |
| GPURenderer.hpp | GPUDevice.hpp | `GPUDevice` |
| GPURenderer.hpp | GPUTexture.hpp | `GPUTexture` |
| GPUVertexPool.hpp | GPUBuffer.hpp | `GPUBuffer` |
| CollisionManager.hpp | CollisionInfo.hpp | `CollisionInfo` |
| SaveGameManager.hpp | Vector2D.hpp | `Vector2D` |
| WorldManager.hpp | WorldData.hpp | `WorldData` |
| WorldResourceManager.hpp | Vector2D.hpp | `Vector2D` |
| WorldGenerator.hpp | WorldData.hpp | `WorldData` |

Note: Forward declarations for `Vector2D` and `BehaviorConfig` are low-value since these headers are lightweight. The highest-value opportunities are `EventManager` in `BehaviorExecutors.hpp` (reduces AI header footprint) and `GPUDevice`/`GPUTexture` in `GPURenderer.hpp` (reduces GPU subsystem coupling).

---

## Architecture Health Scorecard

| Category | Weight | Raw Score | Weighted Score | Notes |
|----------|--------|-----------|----------------|-------|
| Circular Dependencies | 30% | 10/10 | 30/30 | Zero cycles found |
| Layer Compliance | 25% | 7.5/10 | 18.75/25 | 3 violations: 2 justified architectural, 1 fixable |
| Coupling Strength | 20% | 8/10 | 16/20 | No header coupling; 2 .cpp concerns (EDM->WRM, bidirectional ResourceFactory) |
| Header Bloat | 15% | 10/10 | 15/15 | Max 9 includes, avg well under 5, no bloat |
| Dependency Depth | 10% | 10/10 | 10/10 | Avg 2.2, max 5 — excellent |
| **Total** | **100%** | — | **89.75 → 81/100** | *Score adjusted to 81 accounting for the EDM->WRM inversion and ResourceFactory bidirectional coupling as concrete issues beyond the violation count* |

---

## Recommendations

### Critical (Must Fix)

None. The codebase has no circular dependencies and no critical architectural breakdowns.

### Important (Should Fix)

**1. Resolve bidirectional ResourceFactory / ResourceTemplateManager coupling**
- `src/managers/ResourceFactory.cpp` and `src/managers/ResourceTemplateManager.cpp` each include the other's header.
- Extract shared resource type definitions to a `ResourceTypes.hpp` in `include/managers/` and have one side depend on the other, not both ways.
- This does not cause build failures today but is a logical coupling smell that complicates future changes.

**2. Decouple EntityDataManager from WorldResourceManager**
- `src/managers/EntityDataManager.cpp` calls `WorldResourceManager::Instance()` directly to unregister spatial data during entity destruction (lines 408–439, 1075–1076).
- EDM is the canonical data store; having it call upward into a spatial index manager inverts the intended dependency.
- Preferred fix: have `WorldResourceManager` subscribe to an `EntityDestroyedEvent` (already in the event system) and handle its own cleanup reactively. Alternatively, register a cleanup callback with EDM at startup from `WorldResourceManager`.

**3. Fix BinarySerializer.hpp including Logger.hpp (utils → core violation)**
- Change `BinarySerializer` to write errors to `std::ostream` or `std::string` output, removing the `Logger` dependency from the header.
- The `.cpp` implementation can optionally log, but the header should not force a core dependency on utility users.

### Optional (Nice to Have)

**4. Replace GameEngine.hpp full inclusion of GameStateManager.hpp with a forward declaration**
- `GameEngine.hpp` only needs a pointer/reference to `GameStateManager`. A forward declaration `class GameStateManager;` would break the core→manager header chain.
- Low urgency — this is an intentional architectural relationship, but the forward-declare option is cleaner.

**5. Forward-declare EventManager in BehaviorExecutors.hpp**
- `BehaviorExecutors.hpp` includes `EventManager.hpp` but only uses `EventManager` as a pointer or reference.
- Replacing with a forward declaration reduces the include weight of the AI behavior header across all translation units that include it.

**6. Forward-declare GPUDevice and GPUTexture in GPURenderer.hpp**
- `GPURenderer.hpp` can forward-declare these types instead of fully including their headers, reducing the compile surface of the GPU subsystem.

---

## Action Plan

### Immediate (can be done in one session)

- [ ] Fix `BinarySerializer.hpp`: remove `#include "core/Logger.hpp"`, add logging only in the `.cpp` implementation
- [ ] Add `class EventManager;` forward declaration to `BehaviorExecutors.hpp` and remove the `EventManager.hpp` include

### Short-Term (1–3 sprints)

- [ ] Implement entity destruction event (`EntityDestroyedEvent`) subscribed by `WorldResourceManager` to eliminate the EDM→WRM direct call
- [ ] Extract shared type definitions to `ResourceTypes.hpp`, breaking the bidirectional `ResourceFactory` ↔ `ResourceTemplateManager` coupling
- [ ] Forward-declare `GPUDevice` and `GPUTexture` in `GPURenderer.hpp`

### Long-Term (architectural)

- [ ] Evaluate whether `GameStateManager.hpp` should be forward-declared in `GameEngine.hpp` via an `IGameStateManager` interface (low urgency, architectural polish)
- [ ] Re-run this dependency audit after the short-term fixes to verify score improvement
- [ ] Consider generating a visual dependency graph (e.g., via `cmake --graphviz` or a Graphviz pass over `dependency_graph.txt`) for developer onboarding documentation

---

*Report generated by SDL3 VoidLight-Framework testing and validation pipeline.*
*Analysis files: `test_results/dependency_analysis/dependency_graph.txt`, `detect_cycles.py`, `calc_depth.py`*
