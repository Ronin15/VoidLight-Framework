# GameStates

## Overview

GameStates own screen-level behavior, state-scoped controllers, event tokens, and their teardown order.

## Documented States

- [LoadingState](LoadingState.md)
- [LogoState](LogoState.md)
- [MainMenuState](MainMenuState.md)
- [GamePlayState](GamePlayState.md)
- [PauseState](PauseState.md)
- [SettingsMenuState](SettingsMenuState.md)
- [GameOverState](GameOverState.md)

## Current State Pattern

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

AI-heavy states use a strict cleanup sequence:

1. unregister event handlers
2. call `prepareForStateTransition()` on active managers
3. clear controllers
4. clean UI and state-owned world data

Typical manager order when present:

- `AIManager`
- `ProjectileManager`
- `BackgroundSimulationManager`
- `WorldResourceManager`
- `EventManager`
- `CollisionManager`
- `PathfinderManager`
- `EntityDataManager`
- `WorkerBudgetManager`
- `ParticleManager`

## GameOverState

This branch adds a dedicated `GameOverState` so gameplay/demo states can route player death into a real state instead of handling game-over UI inline.
