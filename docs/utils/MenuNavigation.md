# MenuNavigation

**Code:** `include/utils/MenuNavigation.hpp`, `src/utils/MenuNavigation.cpp`

## Role

`MenuNavigation` is the shared helper for action-mapped menu focus and navigation. It keeps MainMenu, Pause, and Settings menu behavior consistent without making each state duplicate controller/keyboard focus logic.

It reads `InputManager::Command` edges, not raw keys. This means rebinding menu commands automatically affects every state that uses this helper.

## API

```cpp
MenuNavigation::reset();
MenuNavigation::applySelection(navOrder, selectedIndex);
MenuNavigation::readInputs(navOrder, selectedIndex, enabled);
MenuNavigation::cancelPressed();
MenuNavigation::leftPressed();
MenuNavigation::rightPressed();
```

## Runtime Contract

- Call `reset()` from menu state `enter()` so each visit starts with keyboard focus hidden.
- Call `readInputs(...)` from state input handling to consume `MenuUp`, `MenuDown`, and `MenuConfirm`.
- Call `applySelection(...)` after UI update/rebuild so the current selected component receives keyboard/controller focus.
- `cancelPressed()`, `leftPressed()`, and `rightPressed()` read mapped command edges for back/cancel and horizontal adjustments.
- Focus highlighting appears after a controller is connected or after the user presses a mapped navigation command; mouse-only users keep normal hover behavior.

`MenuNavigation` uses `UIManager::simulateClick()` for confirm behavior. `simulateClick()` queues callbacks, so tests that assert callback effects should call `UIManager::update(...)` after simulating input.

## Related Docs

- [InputManager](../managers/InputManager.md)
- [UIManager Guide](../ui/UIManager_Guide.md)
- [SettingsMenuState](../gameStates/SettingsMenuState.md)
