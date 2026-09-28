# Architecture Model & Rules

Full reference for the VoidLight-Framework layered architecture, per-layer
dependency rules, coupling rules, and the recurring dependency issues this Skill
exists to catch. Load this when you need to classify a directory, decide whether
a dependency is a true violation, or explain why a coupling pair is acceptable.

> **Always confirm the live layer/dir set at runtime** with `ls -d src/*/ include/*/`.
> The helper scripts in `scripts/` re-derive and classify every directory from
> the filesystem on each run — do NOT assume the list below is frozen.

## Dependency Direction

```
Core (GameEngine, ThreadSystem, Logger, TimestepManager, WorkerBudget)
  ↓
Managers (AIManager, CollisionManager, EventManager, WorldManager, etc.)
  ↓
GameStates (GameState, MainMenuState, GamePlayState, PauseState, etc.)
  ↓
Entities / Controllers (EntityDataManager SoA data; state-scoped controllers)

Cross-cutting layers (used by the above): Utils, Events, AI, Collisions, World, GPU
```

At time of writing the live top-level set is `{core, managers, controllers,
gameStates, entities, events, ai, collisions, utils, world, gpu}` (11 layers).

## Per-Layer Rules

**1. Core Layer** (`src/core/`, `include/core/`)
- **Can depend on:** Nothing (foundation layer)
- **Used by:** Everything
- **Components:** GameEngine, ThreadSystem, Logger, TimestepManager, WorkerBudget

**2. Managers Layer** (`src/managers/`, `include/managers/`)
- **Can depend on:** Core, Utils
- **Cannot depend on:** States, Entities (except via interfaces)
- **Coupling:** Managers should be loosely coupled, communicate via GameEngine
- **Components:** AIManager, CollisionManager, PathfinderManager, EventManager, etc.

**3. States Layer** (`src/gameStates/`, `include/gameStates/`)
- **Can depend on:** Core, Managers, Controllers, Utils
- **Cannot depend on:** Other States (no cross-state dependencies)
- **Components:** GameState, MainMenuState, GamePlayState, PauseState, etc.

**4. Entities Layer** (`src/entities/`, `include/entities/`)
- **Can depend on:** Core, Utils
- **Should avoid:** Direct manager dependencies (use interfaces/callbacks)
- **Components:** Entity, Component classes

**5. Utils Layer** (`src/utils/`, `include/utils/`)
- **Can depend on:** Nothing (pure utility functions)
- **Used by:** Everything
- **Components:** Vector2D, SIMDMath, JsonReader, BinarySerializer, Camera

**6. Controllers Layer** (`src/controllers/`, `include/controllers/`)
- **Can depend on:** Core, Utils, Managers, Entities, Events, World, GPU, AI, Collisions
- **Cannot depend on:** States (controllers are state-scoped via ControllerRegistry)
- **Components:** ControllerRegistry, CombatController, HudController, WeatherController, etc. (under `combat/`, `social/`, `world/`, `render/`, `ui/`)

**7. GPU Layer** (`src/gpu/`, `include/gpu/`)
- **Can depend on:** Core, Utils, Events
- **Components:** GPUDevice, GPURenderer, GPUShaderManager, SpriteBatch, GPUVertexPool, etc.

> Additional cross-cutting layers exist: `ai/`, `events/`, `collisions/`, `world/`.
> The helper scripts classify every directory automatically.

## Coupling Rules

### Game Engine Functional Coupling

Game engines have **necessary functional dependencies** between managers. The
following patterns are **CORRECT and expected**:

✅ **Functional Game System Dependencies (GOOD):**
- AIManager → CollisionManager (AI needs collision queries for obstacle avoidance, LOS)
- AIManager → PathfinderManager (AI needs pathfinding for navigation)
- CollisionManager → WorldManager (collision needs world geometry/tile data)
- Managers → EventManager (event-driven notifications are good architecture)
- UIManager → FontManager (UI needs fonts to render text)
- WorldManager → TextureManager (world needs tile/sprite textures)
- WorldManager → WorldResourceManager (world registers resource nodes in the spatial index)
- ResourceFactory → ResourceTemplateManager (factory pattern requires templates)
- EntityDataManager → WorldResourceManager (EDM auto-registers static entities with WRM spatial index on create/destroy — intentional, .cpp-only)
- ResourceTemplateManager ↔ ResourceFactory (.cpp-only bidirectional: RTM initializes/uses RF; RF calls RTM.generateHandle() — no circular headers)

✅ **Approved Layer Exceptions (do not flag):**
- BinarySerializer.hpp (Utils) includes Logger.hpp (Core) — Logger is a foundational utility used at all layers; this is not a true violation

**Manager-to-Manager Rules:**
- ✅ GOOD: Functional dependencies for game systems
- ✅ GOOD: Event-based communication between managers
- 🔴 FORBIDDEN: Circular Manager dependencies (breaks compilation)
- 🔴 FORBIDDEN: Managers depending on States (violates layer boundaries)

**What Actually Matters:**
- **Circular dependencies:** 🔴 ALWAYS BAD (breaks compilation)
- **Layer violations:** 🔴 ALWAYS BAD (breaks architecture)
- **Tight coupling:** ✅ OFTEN NECESSARY for game systems to work together
- **High reference counts:** ✅ EXPECTED when systems interact functionally

**State-to-Manager:**
- ✅ GOOD: GamePlayState → AIManager (states use managers)
- 🔴 FORBIDDEN: AIManager → GamePlayState (managers don't know about states)

**Header Inclusion:**
- ✅ GOOD: Forward declarations in headers, include in .cpp
- ⚠️  WARNING: Including heavy headers in .hpp (ripple effect)
- 🔴 FORBIDDEN: Circular includes (breaks compilation)

### Functional Dependency Allowlist (used by analyze_coupling.py)

These pairs are treated as expected and NOT flagged as problematic tight coupling:

```
AIManager->CollisionManager        AIManager->PathfinderManager
CollisionManager->WorldManager     CollisionManager->EventManager
WorldManager->EventManager         WorldManager->WorldResourceManager
WorldManager->TextureManager       UIManager->FontManager
UIManager->UIConstants             InputManager->UIManager
InputManager->FontManager          PathfinderManager->EventManager
ParticleManager->EventManager      ResourceFactory->ResourceTemplateManager
ResourceTemplateManager->ResourceFactory
WorldResourceManager->EventManager EntityDataManager->WorldResourceManager
```

## Common Dependency Issues in VoidLight-Framework

### Issue 1: Manager Circular Dependencies
**Symptom:** Compilation errors with forward declaration issues
**Cause:** Two managers including each other's headers
**Solution:** One-way dependency with interface or event system

### Issue 2: State-to-State Dependencies
**Symptom:** States including other state headers
**Cause:** Sharing data/logic between states
**Solution:** Move shared logic to Manager or GameEngine

### Issue 3: GameEngine.hpp Bloat
**Symptom:** Long compile times for any GameEngine change
**Cause:** GameEngine includes all managers in header
**Solution:** Forward declarations + includes in .cpp

### Issue 4: Layer Violations
**Symptom:** Manager includes State header
**Cause:** Manager needs state-specific logic
**Solution:** Dependency inversion - state registers callback with manager

### Issue 5: Utils Dependencies
**Symptom:** Utils including Core or Manager headers
**Cause:** Utils trying to use engine-specific types
**Solution:** Make Utils pure (STL only), or move to appropriate layer

## Circular Dependency Fix Patterns

When a cycle (e.g. `AIManager.hpp -> PathfinderManager.hpp -> AIManager.hpp`) is found:

1. **Forward Declaration (RECOMMENDED):**
   - In `AIManager.hpp`: remove `#include "PathfinderManager.hpp"`, add `class PathfinderManager;`
   - In `AIManager.cpp`: add `#include "PathfinderManager.hpp"`
2. **Interface Extraction:** create `IPathfinder.hpp` pure-virtual interface; `AIManager` depends on the interface; `PathfinderManager` implements it.
3. **Dependency Inversion:** both managers depend on an abstract interface; `GameEngine` wires the concrete implementations.
