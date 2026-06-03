# Controllers

## Overview

Controllers are state-scoped helpers owned by `ControllerRegistry`. They do not own global data. They coordinate player actions, event subscriptions, and UI-facing state for a single GameState.

Current controller families:

- `controllers/combat/`
- `controllers/ui/`
- `controllers/social/`
- `controllers/world/`
- `controllers/render/`

## Current Controller Set

| Controller | Role | Notes |
|-----------|------|-------|
| [CombatController](CombatController.md) | Player melee/ranged combat, stamina timing, ammo use, and melee fallback | Emits damage/projectiles through current gameplay paths; logs player combat feedback |
| [HudController](HudController.md) | State-scoped HUD: hotbar UI + transient combat target state | Owns hotbar components; subscribes to Combat and ResourceChange events |
| [InventoryController](InventoryController.md) | Player inventory overlay, container UI, gear UI, pickup, slot reordering, loot/store transfers, and hotbar assignment | Syncs from ResourceChange events; uses EDM ordered slots; hands hotbar writes to HudController |
| [WeatherController](WeatherController.md) | Weather state tracking | Event-driven |
| [DayNightController](DayNightController.md) | Time-of-day visuals and GPU lighting | Requires `update(dt)` each frame |
| [HarvestController](HarvestController.md) | Progress-based harvesting of WRM/EDM harvestables | Cancels on movement, emits resource/harvest events |
| [SocialController](SocialController.md) | Merchant trading, gifts, theft, relationship/memory updates | Builds trade UI through `UIManager` |

## Ownership Pattern

```cpp
bool GamePlayState::enter() {
    m_controllers.add<CombatController>(mp_player);
    m_controllers.add<HudController>(mp_player);
    m_controllers.add<InventoryController>(mp_player);
    m_controllers.add<HarvestController>(mp_player);
    m_controllers.add<SocialController>(mp_player);
    m_controllers.add<WeatherController>();
    m_controllers.add<DayNightController>();
    m_controllers.subscribeAll();
    return true;
}

void GamePlayState::update(float dt) {
    m_controllers.updateAll(dt);
}

bool GamePlayState::exit() {
    m_controllers.clear();
    return true;
}
```

## Rules

- Controllers may subscribe to events, but GameStates still own overall teardown order.
- Controllers should not become shadow managers or alternate data stores.
- Controllers must not directly mutate AI behavior state in EDM. Queue behavior messages instead.
- UI-facing values are fine, but GameStates should remain responsible for deciding when and how to render UI.
- Render-focused controllers such as `ResourceRenderController` live in `controllers/render/`; state-owned render helpers should still follow the same registry lifecycle when they are registry-managed.
