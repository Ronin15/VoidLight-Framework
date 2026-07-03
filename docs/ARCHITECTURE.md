# Architecture

**Code:** `README.md`, `docs/core/GameEngine.md`, `docs/events/EventManager.md`, `docs/managers/EntityDataManager.md`, `docs/managers/WorldResourceManager.md`, `docs/core/TimestepManager.md`, `docs/gpu/GPURendering.md`

## Overview

The engine is layered as:

- Core
- Managers
- GameStates
- Entities / Controllers

Current branch contracts:

- `EventManager` is the central event hub with main-thread deferred draining, built-in combat processing, and sequence-preserved ordering across combat and non-combat queues
- AI is EDM-backed and executed through `BehaviorExecutors` + `AICommandBus`
- `WorldResourceManager` is a registry/spatial index over EDM, not a quantity store
- GPU rendering uses explicit swapchain acquisition
- `TimestepManager` accepts real display refresh data for cadence snapping
- `GameOverState` is part of the state graph

## Update Flow

Typical frame shape:

1. `GameEngine` starts the frame
2. input and deferred events are processed
3. active GameState updates UI/controllers/state logic
4. managers such as AI, collision, particles, and background simulation run
5. render path draws scene/UI
6. present completes the frame

## Rendering Path

```text
beginFrame
state.recordGPUVertices()
beginScenePass         (internally acquires the swapchain texture on first use)
state.renderGPUScene()
beginSwapchainPass
renderComposite         (scene texture -> swapchain, zoom applied here)
state.renderGPUUI()
endFrame                (called from GameEngine::present(), separate from render())
```

States provide `recordGPUVertices()`/`renderGPUScene()`/`renderGPUUI()` hooks, but frame lifetime, swapchain acquisition, compositing, and presentation remain engine/GPURenderer-owned.

## GameState Flow

Key states include:

- `LogoState`
- `MainMenuState`
- `SettingsMenuState`
- `LoadingState`
- `GamePlayState`
- `PauseState`
- demo states (`AIDemoState`, `AdvancedAIDemoState`, `EventDemoState`, `OverlayDemoState`, `UIDemoState`)
- `GameOverState`

Important transitions:

- `MainMenuState -> LoadingState -> GamePlayState`
- `GamePlayState -> GameOverState`
- AI-heavy demos may also route to `GameOverState`

## State Teardown

AI/world-heavy states follow this pattern:

1. destroy state-owned NPCs and unregister state-owned handlers
2. call `prepareForStateTransition()` on active managers in dependency order — this is also where world unload (`WorldManager::unloadWorld()`) and EntityDataManager teardown happen, mid-sequence
3. clear UI and controllers
4. reset remaining cached state (camera, player pointer, init flag)

This matters because deferred events, AI command commits, and WRM spatial indices all participate in runtime state now. Note the order: world/entity teardown happens *before* UI/controllers are cleared, not after — see `GamePlayState::exit()`.

## Related Docs

- [GameEngine](core/GameEngine.md)
- [EventManager](events/EventManager.md)
- [EntityDataManager](managers/EntityDataManager.md)
- [WorldResourceManager](managers/WorldResourceManager.md)
- [TimestepManager](core/TimestepManager.md)
