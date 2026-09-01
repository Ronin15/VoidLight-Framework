# HudController

## Role

`HudController` is a **state-scoped** owner of the gameplay **action HUD**: player HP/SP bars, combat target frame, optional hotbar, and harvest progress.

- Creates vitals, target, and harvest widgets in `initializeActionHUD()` via `UIManager` primitives
- Owns the hotbar UI: 9 slot buttons + key labels/icons/counts created in `initializeHotbarUI()`
- Tracks selected hotbar slot, polls keyboard hotbar input each frame, applies selection highlighting
- Subscribes to `EventTypeId::Combat` to populate the transient target display when the player deals damage
- Subscribes to `EventTypeId::ResourceChange` so hotbar item counts stay current
- Updates widgets with dirty-flag `setValue` / `setText` / visibility (no unconditional per-frame writes)
- Exposes `setVisible(bool)` for pause/resume of **its** subtree (vitals + logical target/harvest + hotbar)
- Exposes `setHarvestProgress(bool, float)` so the state can feed `HarvestController` samples without making this controller a harvest owner

Session chrome (`event_log`, `time_label`, fps) stays on `GamePlayState`. Inventory and trade stay on their controllers. `UIManager` is the widget service only.

The target display is transient and auto-expires after `TARGET_DISPLAY_DURATION`. Widget mutations run only when `m_actionHUDCreated` is true. Hotbar polling is gated on `m_hotbarUICreated`.

## Public Widget IDs

Stable `hud_*` IDs for tests and documentation. Production pause/resume must not enumerate them; use `setVisible()`.

- `HEALTH_LABEL_ID` / `HEALTH_BAR_ID`
- `STAMINA_LABEL_ID` / `STAMINA_BAR_ID`
- `TARGET_NAME_ID` / `TARGET_HP_LABEL_ID` / `TARGET_HEALTH_BAR_ID`
- `HARVEST_LABEL_ID` / `HARVEST_BAR_ID`

Target getters (`hasActiveTarget()`, `getTargetLabel()`, `getTargetHealth()`) remain for tests and sibling controllers. Production states do not push those values into UI.

## Typical Usage (GamePlayState)

- Add it in `enter()` via `ControllerRegistry` with the shared player pointer: `m_controllers.add<HudController>(mp_Player)`
- Call `initializeActionHUD()` and `initializeHotbarUI()` after inventory init (both are idempotent)
- Call `m_controllers.updateAll(dt)` each frame, then `setHarvestProgress(harvestCtrl.isHarvesting(), harvestCtrl.getProgress())`
- Call `handleHotbarInput()` from state input handling after command state has been refreshed
- Toggle `setVisible(false)` on pause, `true` on resume. `setVisible(true)` restores player vitals and hotbar, then applies current logical target/harvest visibility (it does not force-show target/harvest)
- Let `InventoryController` assign and reorder items through `assignHotbarItem()`, `moveHotbarItem()`, and `clearHotbarItem()` instead of mutating UI components directly

`AdvancedAIDemoState` calls `initializeActionHUD()` only (no hotbar, harvest feed, or `setVisible`). Target-state-only tests may add the controller without either init. Hotbar tests may call `initializeHotbarUI()` only.

## Rules

- Controllers remain **state-scoped**; they must not become shadow managers.
- Prefer event-driven updates; only poll EDM for small, transient UI state (like current target health).
- Do not mutate widgets from combat event handlers; `update()` applies the target frame.
- `m_actionHUDCreated` / `m_hotbarUICreated` must stay in lockstep with the existence of those components in `UIManager`.
- Player HP/SP bars are 0..100 percent of max, not raw stat values.
- Harvest widgets are a static "Harvest" label plus a 0..1 bar, hidden unless `setHarvestProgress(true, ...)`.
- Hotbar assignment stores `ResourceHandle`s and refreshes icons/counts from player inventory state.

## Related Docs

- [Controllers Overview](README.md)
- [ControllerRegistry](ControllerRegistry.md)
- [CombatController](CombatController.md)
- [HarvestController](HarvestController.md)
- [InventoryController](InventoryController.md)
- [EventManager](../events/EventManager.md)
