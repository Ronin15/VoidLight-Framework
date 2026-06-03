# MainMenuState

**Code:** `include/gameStates/MainMenuState.hpp`, `src/gameStates/MainMenuState.cpp`

## Overview

`MainMenuState` is the top-level menu state. It pauses gameplay managers through
`GameEngine::setGlobalPause(true)`, creates the main menu UI, and routes button
callbacks to typed `GameStateId` transitions.

## UI and Input

- menu buttons are centered with `UIManager` positioning helpers
- keyboard/controller focus uses `MenuNavigation`
- mouse hover remains separate from keyboard/controller selection
- the quit action opens a modal dialog instead of stopping the engine directly
- the modal dialog uses a parent panel with linked child components and overlay
  occlusion

## Lifecycle

- `enter()` resets menu navigation, waits briefly for fonts, creates UI, and
  wires callbacks
- `update()` runs `UIManager::update(...)` and applies menu focus
- `handleInput()` routes menu commands through `MenuNavigation`
- `exit()` calls `UIManager::prepareForStateTransition()`

## GPU Rendering

The state records and renders UI through:

```cpp
UIManager::Instance().recordGPUVertices(gpuRenderer);
UIManager::Instance().renderGPU(gpuRenderer, swapchainPass);
```

## Related Docs

- [MenuNavigation](../utils/MenuNavigation.md)
- [UIManager Guide](../ui/UIManager_Guide.md)
- [GameStateManager](../managers/GameStateManager.md)
