# GameOverState

**Code:** `include/gameStates/GameOverState.hpp`, `src/gameStates/GameOverState.cpp`

## Overview

Player-death screen. GamePlay routes here with `changeState(GAME_OVER)`, which **exits GamePlay and unloads the world**. Retry is a new run through Loading, not a paused overlay over a live world.

## Behavior

- `enter()` calls `GameEngine::setGlobalPause(true)` and builds overlay / title / Retry / Main Menu
- Retry and `R` return to `m_returnState` (defaults to `GAME_PLAY`; callers may `setReturnState`)
- Main Menu and `M` use `changeState(MAIN_MENU)`
- Keyboard/controller focus uses `MenuNavigation`

## GPU

There is no `render()` / SDL renderer path.

- `recordGPUUIVertices` records UI through `UIManager`
- `renderGPUUI` draws UI on the swapchain pass
- No world scene (`hasGPUScene() == false`); the scene target is cleared

## Exit

`exit()` only `clearKeyboardSelection()`. It does **not** call `UIManager::prepareForStateTransition()`. Full-screen replace is owned by `GameStateManager` after `exit()` (stack empty). Overlay widgets must not full-wipe UI from this state.

## Related docs

- [GamePlayState](GamePlayState.md)
- [GameStateManager](../managers/GameStateManager.md)
- [ARCHITECTURE](../ARCHITECTURE.md)
