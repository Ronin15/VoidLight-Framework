# GameStates

## Overview

GameStates own screen-level behavior, state-scoped controllers, event tokens, and their teardown order.

**Managers serve the states:** states drive lifecycle and screen policy; domain managers (UI, AI, World, …) provide APIs states call. `GameStateManager` is the state **stack/transition** machine, not a domain manager. See [ARCHITECTURE.md](../ARCHITECTURE.md) “Managers serve the states”.

## Documented States

- [LoadingState](LoadingState.md)
- [LogoState](LogoState.md)
- [MainMenuState](MainMenuState.md)
- [GamePlayState](GamePlayState.md)
- [PauseState](PauseState.md)
- [SettingsMenuState](SettingsMenuState.md)
- [GameOverState](GameOverState.md)

## Current State Pattern

### Transitions and UI

`GameStateManager` uses **exit-then-enter** for replaces:

1. `exit()` leaving state(s)
2. If nothing remains on the stack (**full-screen replace**), clear global UI once
3. `enter()` destination and rebuild that state's UI

Overlay `pushState` / `popState` do **not** full-clear UI (Pause over GamePlay keeps the HUD registered).

Full-screen `enter()` may defensively call `UIManager::prepareForStateTransition()` before building widgets. Overlay/settings states must only remove their own widget ids in `exit()`.

See also [ARCHITECTURE.md](../ARCHITECTURE.md) “State Transitions (UI + lifecycle)”.

### Update owns UI updates

```cpp
void SomeState::update(float dt) {
    UIManager::Instance().update(dt);
    m_controllers.updateAll(dt);
}
```

`render()` should draw only. Do not call `ui.update()` from `render()`.

### Deferred transitions

If a state needs to redirect immediately, set a flag in `enter()` and transition from `update()`.

### State-owned controllers and event tokens

States commonly own:

- `ControllerRegistry`
- explicit event handler tokens
- HUD/UI setup for that state's features

## AI / World Teardown Pattern

AI-heavy states use a strict cleanup sequence in `exit()`:

1. unregister event handlers
2. call `prepareForStateTransition()` on active managers
3. clear controllers / world-owned state
4. leave full-screen UI teardown to `GameStateManager` (it clears UI after `exit()` on full-screen replace)

Typical manager order when present:

- `AIManager`
- `ProjectileManager`
- `BackgroundSimulationManager`
- `WorldManager` (unload world before WRM/events)
- `WorldResourceManager`
- `EventManager`
- `CollisionManager`
- `PathfinderManager`
- `EntityDataManager`
- `WorkerBudgetManager`
- `ParticleManager`

## GameOverState

This branch adds a dedicated `GameOverState` so gameplay/demo states can route player death into a real state instead of handling game-over UI inline.
