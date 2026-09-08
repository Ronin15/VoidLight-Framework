# LoadingState

**Code:** `include/gameStates/LoadingState.hpp`, `src/gameStates/LoadingState.cpp`

## Overview

`LoadingState` is the non-blocking world-loading screen. It runs world
generation through `ThreadSystem`, keeps progress/status data thread-safe, waits
for the pathfinding grid to become ready, then transitions to a typed target
state.

Rendering stays inside the SDL3_GPU state pipeline. The state records UI
vertices and renders UI through `UIManager`; it does not clear, present, or
manually submit GPU work.

## Configuration

Configure before transitioning into the state:

```cpp
auto* loadingState = dynamic_cast<LoadingState*>(
    mp_stateManager->getState(GameStateId::LOADING).get());

VoidLight::WorldGenerationConfig config{};
config.width = 200;
config.height = 200;
config.seed = 12345;

loadingState->configure(GameStateId::GAME_PLAY, config);
mp_stateManager->changeState(GameStateId::LOADING);
```

`configure(...)` resets progress, completion, pathfinding-wait, and error state
so the same registered `LoadingState` instance can be reused.

## Lifecycle

1. `enter()` validates that `configure(...)` supplied a target
   `GameStateId`.
2. `GameEngine::setGlobalPause(true)` then `EventManager::setGlobalPause(false)`.
   EventManager is one bus for gameplay and engine/lifecycle events. The
   exclusive window pauses gameplay *producers* (AI, collision update,
   pathfinder update, game time, projectiles, particles) and GameEngine
   destroy-drain so the load worker can create harvestables and NPCs.
   WorldManager is not paused; it posts Deferred `WorldLoaded`. EventManager
   drain stays on so that lifecycle event can run: collision rebuilds static
   colliders from the handler (not from `update()`) and fires Immediate
   `StaticCollidersReady`. Destination `enter()` (GamePlayState,
   AIDemoState, EventDemoState) already unpauses and EventManager is the
   gameplay bus again.
3. loading UI is created in pixel-space using `GameEngine::getWidthInPixels()`
   and `getHeightInPixels()`.
4. `startAsyncWorldLoad()` enqueues world generation on `ThreadSystem`.
5. `update()` pushes progress/status into UI components.
6. once world loading completes, wait until `PathfinderManager::isGridReady()`
   unless the load failed (`m_loadFailed`). A failed load does not wait for a
   grid that will never be built.
7. the state transitions to the configured target via
   `mp_stateManager->changeState(m_targetStateId)`.
8. `exit()` removes loading UI and waits for any still-valid future. If the
   load never handed off to the destination (`changeState` to GamePlay/demo),
   `exit()` calls `WorldManager::unloadWorld()` only — Loading is not a
   gameplay owner and does not run the AI-heavy `prepareForStateTransition()`
   sequence.

If world generation fails, `hasError()` / `getLastError()` expose the failure,
but the state still attempts to transition to the target state. Target states
that care about recovery should inspect the loading state during `enter()`.

## GPU UI Contract

`LoadingState` implements:

```cpp
void recordGPUUIVertices(VoidLight::GPURenderer&);
void renderGPUUI(VoidLight::GPURenderer&, SDL_GPURenderPass*);
```

`hasGPUScene()` is false. `recordGPUUIVertices()` lets `UIManager` record
title/progress/status geometry. `renderGPUUI()` submits that UI during the
swapchain UI pass.

## Deferred Transition Pattern

States that need a world should set intent in `enter()` and transition from
`update()`. `GamePlayState` uses this pattern so state changes do not happen
inside `enter()`:

```cpp
bool GamePlayState::enter()
{
    if (!m_worldLoaded) {
        m_needsLoading = true;
        m_worldLoaded = true;
        return true;
    }

    return initializeGameplay();
}

void GamePlayState::update(float)
{
    if (m_needsLoading) {
        m_needsLoading = false;
        loadingState->configure(GameStateId::GAME_PLAY, config);
        m_transitioningToLoading = true;
        mp_stateManager->changeState(GameStateId::LOADING);
        return;
    }
}
```

## Related Docs

- [GameStateManager](../managers/GameStateManager.md)
- [ThreadSystem](../core/ThreadSystem.md)
- [WorldManager](../managers/WorldManager.md)
- [PathfinderManager](../managers/PathfinderManager.md)
- [UIManager Guide](../ui/UIManager_Guide.md)
