# MainMenuState

**Code:** `include/gameStates/MainMenuState.hpp`, `src/gameStates/MainMenuState.cpp`

## Overview

`MainMenuState` is the top-level menu state. It pauses gameplay managers through
`GameEngine::setGlobalPause(true)`, creates the main menu UI, and routes button
callbacks to typed `GameStateId` transitions.

Buttons: Start Game, AI Demo (load test), Event Demo (power bench), Settings,
Exit. Debug shortcuts `A` / `E` jump to AI Demo / Event Demo.

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
- `exit()` only `clearKeyboardSelection()`. Full-screen UI clear is owned by `GameStateManager` after `exit()`. Do not unpause here; destination `enter()` owns pause.

## GPU Rendering

The state owns a GPU scene (diorama + particles) and UI:

```cpp
recordGPUSceneVertices(...);  // diorama, particles
recordGPUUIVertices(...);     // UIManager
renderGPUScene(...);
renderGPUUI(...);
```

## Related Docs

- [MenuNavigation](../utils/MenuNavigation.md)
- [UIManager Guide](../ui/UIManager_Guide.md)
- [GameStateManager](../managers/GameStateManager.md)
