# SettingsMenuState

**Code:** `include/gameStates/SettingsMenuState.hpp`, `src/gameStates/SettingsMenuState.cpp`

## Overview

`SettingsMenuState` is the tabbed settings UI. It stages graphics, audio, and gameplay values in local temporary state until the user applies them, and it also owns the Controls tab for command rebinding.

Current tabs:

- `Graphics`
- `Audio`
- `Gameplay`
- `Controls`

## Runtime Flow

1. `enter()` resets shared menu navigation, loads current settings, creates tab/body/action UI, creates Controls rows, and switches to Graphics.
2. `update(dt)` runs `UIManager::update(dt)` unless a rebind capture is active or has just finished, then applies the current `MenuNavigation` selection.
3. `handleInput()` reads action-mapped menu navigation, slider left/right adjustments, and cancel/back behavior. It suppresses menu input while rebinding.
4. `renderGPUUI()` renders the UI through `UIManager`; state render code should not update UI state.
5. `exit()` calls `UIManager::prepareForStateTransition()`.

## Controls Tab

The Controls tab is backed by `InputManager` command bindings. Each visible command row has a keyboard/mouse binding button and a controller binding button. Clicking a binding button starts category-specific rebind capture:

- keyboard and mouse inputs replace only the keyboard/mouse binding
- gamepad buttons and axes replace only the controller binding
- opposite-category inputs are ignored during capture
- after capture ends, the row labels refresh from `InputManager::describeBinding(...)`

The tab intentionally shows gameplay and menu commands, but omits hotbar slot commands from the Controls presentation. Hotbar slots keep their default select-only keyboard mapping for this iteration.

`Reset Controls to Defaults` calls `InputManager::resetBindingsToDefaults()`, saves to `res/input_bindings.json`, and refreshes the displayed binding labels.

## Settings Persistence

`applySettings()` writes staged graphics/audio/gameplay values to `SettingsManager`, applies fullscreen/VSync through `GameEngine`, saves `res/settings.json`, saves current input bindings to `res/input_bindings.json`, and returns to the main menu.

Back/cancel exits without applying staged settings. Rebind changes are saved when Apply is clicked or when the Controls reset button writes defaults.

## Navigation

The state builds a stable navigation order for the active tab. Sliders consume `MenuLeft`/`MenuRight` in 5% steps. Controls rows use generated component IDs stored in `m_navBacking` so the `std::string_view` navigation order remains valid for the state's lifetime.

## Related Docs

- [InputManager](../managers/InputManager.md)
- [MenuNavigation](../utils/MenuNavigation.md)
- [UIManager Guide](../ui/UIManager_Guide.md)
