# PauseState

**Code:** `include/gameStates/PauseState.hpp`, `src/gameStates/PauseState.cpp`

## Overview

`PauseState` is a stack-pushed overlay state above gameplay. It globally pauses
gameplay managers, displays a dimmed overlay and menu buttons, and resumes or
returns to the main menu through typed `GameStateId` calls.

## Lifecycle

- `enter()` calls `GameEngine::setGlobalPause(true)`, creates overlay/title/buttons,
  and initializes `MenuNavigation`
- `update()` runs `UIManager::update(...)` and applies the current selection
- `handleInput()` pops the state on `MenuCancel` or `Command::Pause`
- `exit()` calls `GameEngine::setGlobalPause(false)` and removes only
  PauseState-owned UI components

`exit()` does not call `UIManager::prepareForStateTransition()` because gameplay
UI is preserved underneath the pause overlay.

## Transitions

- Resume: `mp_stateManager->popState()`
- Main Menu: `mp_stateManager->changeStateClearingStack(GameStateId::MAIN_MENU)`

## Related Docs

- [GamePlayState](GamePlayState.md)
- [MenuNavigation](../utils/MenuNavigation.md)
- [UIManager Guide](../ui/UIManager_Guide.md)
