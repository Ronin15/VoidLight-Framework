# Architecture Model & Rules

Full reference for the VoidLight-Framework layered architecture, per-layer
dependency rules, coupling rules, and the recurring dependency issues this Skill
exists to catch. Load this when you need to classify a directory, decide whether
a dependency is a true violation, or explain why a coupling pair is acceptable.

> **Always confirm the live layer/dir set at runtime** with `ls -d src/*/ include/*/`.
> The helper scripts in `scripts/` re-derive and classify every directory from
> the filesystem on each run — do NOT assume the list below is frozen.

## Dependency Direction

Canonical source: `docs/ARCHITECTURE.md` and root `CLAUDE.md`
("Dependency direction: `Core -> Managers -> GameStates -> Entities/Controllers`").
Higher layers may include lower layers; managers must not depend on states.

```
Core (GameEngine, ThreadSystem, Logger, TimestepManager, WorkerBudget)
  ↓
Managers (AIManager, CollisionManager, EventManager, WorldManager,
          EntityDataManager + EntityDataTypes, GameStateManager, etc.)
  ↓
GameStates (GameState, MainMenuState, GamePlayState, LoadingState, etc.)
  ↓
Entities / Controllers (entity types/handles; state-scoped controllers)

Cross-cutting layers (used by the above): Utils, Events, AI, Collisions, World, GPU
```

Note: `GameStateManager` lives under `managers/` but is state-stack
infrastructure, not a domain manager (docs/ARCHITECTURE.md). `EntityDataManager`
and its data-type module `EntityDataTypes.hpp` live under `managers/`.

At time of writing the live top-level set is `{core, managers, controllers,
gameStates, entities, events, ai, collisions, utils, world, gpu}` (11 layers).

## Per-Layer Rules (as enforced by `detect_layer_violations.py`)

The script's `layer_rules` table is authoritative; this mirrors it.
Same-layer includes are always allowed (except state→state). Bare includes
(`"Event.hpp"`) are classified by the including header's directory.

| Layer (dir) | May include | Notes |
|-------------|-------------|-------|
| Core (`core/`) | nothing | Approved bend: `GameEngine.hpp → GameStateManager.hpp` |
| Utils (`utils/`) | nothing | Approved bend: `BinarySerializer.hpp → Logger.hpp` |
| Managers (`managers/`) | Core, Utils, Events, AI, Entities, Collisions, World, GPU | Never GameStates or Controllers. Approved bend: `GameStateManager.hpp → GameState.hpp` |
| Controllers (`controllers/`) | Core, Utils, Managers, Events, AI, Entities, Collisions, World, GPU | Never GameStates (state-scoped via `ControllerRegistry`) |
| GameStates (`gameStates/`) | everything except other states | `GameState.hpp` base include is allowed |
| Entities (`entities/`) | Core, Utils, Events | |
| AI (`ai/`) | Core, Utils, Events | Approved: `BehaviorExecutors.hpp → EventManager.hpp`, `→ EntityDataTypes.hpp` |
| Events (`events/`) | Core, Utils | |
| Collisions (`collisions/`) | Core, Utils | |
| World (`world/`) | Core, Utils, Events | |
| GPU (`gpu/`) | Core, Utils, Events | |

**Lightweight cross-cutting headers** (any layer may include; stdlib-only
enum/tag/value types filed under a domain dir): `EntityHandle.hpp`,
`TriggerTag.hpp`, `Season.hpp`, `ParticleEffectType.hpp`, `SparseSidecar.hpp`,
`EventTypeId.hpp`, `FactionStance.hpp`. Extend `LIGHTWEIGHT_CROSS_CUTTING_HEADERS`
in the script only for genuinely dependency-free type headers.

## Coupling Rules

- **Always bad:** circular includes; managers/controllers including states;
  state→state includes.
- **Usually fine:** functional manager→manager coupling (systems must
  interact), managers → `EventManager`, anything → `EntityDataManager` /
  `EntityDataTypes` (EDM is the SoA data hub). High reference counts between
  allowlisted pairs are expected — don't recommend refactoring them.
- **Worth flagging:** heavy headers included from `.hpp` where a forward
  declaration would do; non-allowlisted tight coupling (>10 refs in the `.cpp`)
  with no clear functional reason.

Non-obvious allowlisted pairs and why:
- EntityDataManager → WorldResourceManager: EDM auto-registers static entities
  with the WRM spatial index on create/destroy (.cpp-only).
- EntityDataManager → AIManager: `createNPCWithRaceClass` auto-registers
  classes.json suggestedBehavior via `AIManager::registerEntity` (CLAUDE.md,
  "EDM, AI, and Controllers" — do not add more policy there).
- ResourceTemplateManager ↔ ResourceFactory: .cpp-only bidirectional use, no
  circular headers.
- `Season` / `ParticleEffectType` / `UIConstants`: single-enum/constant headers
  filed under `managers/`, not peer managers.

### Functional Dependency Allowlist (used by analyze_coupling.py)

These pairs are treated as expected and NOT flagged as problematic tight coupling:

```
AIManager->CollisionManager        AIManager->PathfinderManager
AIManager->EventManager            AIManager->EntityDataManager
CollisionManager->WorldManager     CollisionManager->EventManager
CollisionManager->EntityDataManager
WorldManager->EventManager         WorldManager->WorldResourceManager
WorldManager->TextureManager       WorldManager->Season
UIManager->FontManager             UIManager->UIConstants
InputManager->UIManager            InputManager->FontManager
PathfinderManager->EventManager    ParticleManager->EventManager
ParticleManager->ParticleEffectType
GameTimeManager->EventManager      GameTimeManager->Season
ResourceFactory->ResourceTemplateManager
ResourceTemplateManager->ResourceFactory
WorldResourceManager->EventManager
EntityDataManager->WorldResourceManager
EntityDataManager->ResourceTemplateManager
EntityDataManager->AIManager
BackgroundSimulationManager->EntityDataManager
```
The script (`functional_deps` in `analyze_coupling.py`) is authoritative.

## Common Issues → Fix

| Symptom | Usual cause | Fix |
|---------|-------------|-----|
| Incomplete-type / include-order compile errors | Two headers include each other | Forward-declare one side; include in `.cpp` |
| State includes another state | Shared logic between screens | Move it to a manager service or utility |
| Manager includes a state header | Manager needs screen policy | Invert: state calls the manager (states drive, managers serve) |
| Utils/Core header pulls in engine types | Type filed in the wrong layer | Move the type down, or split a lightweight type header |
| Widely-included heavy header | Implementation types leaking into `.hpp` | Split data types out (as `EntityDataTypes.hpp` was) / forward-declare |

## Circular Dependency Fix Patterns

When a cycle (e.g. `AIManager.hpp -> PathfinderManager.hpp -> AIManager.hpp`) is found:

1. **Forward Declaration (RECOMMENDED):**
   - In `AIManager.hpp`: remove `#include "PathfinderManager.hpp"`, add `class PathfinderManager;`
   - In `AIManager.cpp`: add `#include "PathfinderManager.hpp"`
2. **Interface Extraction:** create `IPathfinder.hpp` pure-virtual interface; `AIManager` depends on the interface; `PathfinderManager` implements it.
3. **Dependency Inversion:** both managers depend on an abstract interface; `GameEngine` wires the concrete implementations.
