# HudController

## Role

`HudController` is a **state-scoped** controller that owns the gameplay HUD: the hotbar UI subtree and the transient combat target-frame state.

- Owns the hotbar UI: 9 slot buttons + key labels/icons/counts created via `UIManager` in `initializeHotbarUI()`
- Tracks selected hotbar slot, polls keyboard hotbar input each frame, applies selection highlighting
- Subscribes to `EventTypeId::Combat` to populate the transient target display when the player deals damage
- Subscribes to `EventTypeId::ResourceChange` so hotbar item counts stay current
- Exposes `hasActiveTarget()`, `getTargetLabel()`, and `getTargetHealth()` for HUD rendering by the active `GameState`

The target display is transient and auto-expires after `TARGET_DISPLAY_DURATION`. The hotbar UI is only mutated and polled when `m_hotbarUICreated` is true.

## Typical Usage (GamePlayState)

- Add it in `enter()` via `ControllerRegistry` with the shared player pointer: `m_controllers.add<HudController>(mp_Player)`
- Call `initializeHotbarUI()` once after creation to build hotbar components
- Call `m_controllers.updateAll(dt)` each frame
- Call `handleHotbarInput()` from state input handling after command state has been refreshed
- Toggle `setHotbarVisible(false)` on pause, `true` on resume
- Query `hasActiveTarget()` / `getTargetLabel()` / `getTargetHealth()` when updating the combat HUD
- Let `InventoryController` assign and reorder items through `assignHotbarItem()`, `moveHotbarItem()`, and `clearHotbarItem()` instead of mutating UI components directly

States that only need the target-frame state (e.g., demos) may add the controller without calling `initializeHotbarUI()`; hotbar polling is gated on UI creation and will no-op.

## Rules

- Controllers remain **state-scoped**; they must not become shadow managers.
- Prefer event-driven updates; only poll EDM for small, transient UI state (like current target health).
- `m_hotbarUICreated` must stay in lockstep with the existence of the hotbar components in `UIManager`.
- Hotbar assignment stores `ResourceHandle`s and refreshes icons/counts from player inventory state.

## Related Docs

- [Controllers Overview](README.md)
- [ControllerRegistry](ControllerRegistry.md)
- [CombatController](CombatController.md)
- [InventoryController](InventoryController.md)
- [EventManager](../events/EventManager.md)
