# InventoryController

## Role

`InventoryController` is a state-scoped gameplay controller for player inventory interaction and inventory UI. It owns:

- pickup interaction within `PICKUP_RADIUS`
- nearby container open/close policy
- container loot/store/transfer UI
- inventory panel creation and visibility
- inventory grid refresh after `ResourceChangeEvent`
- gear slot display and equip/unequip clicks
- hotbar assignment handoff to `HudController`
- inventory slot drag/drop reordering
- event-log entries for resource changes

It does not own inventory storage. Player inventory and equipment state remain in the player/EDM resource path; the controller reflects inventory slot order through EDM slot APIs and keeps UI/input policy in the controller.

## Runtime Flow

`GamePlayState` adds the controller with
`m_controllers.add<InventoryController>(mp_Player)`, creates shared gameplay UI
such as the event log and status labels, then calls
`inventoryCtrl.initializeInventoryUI()` and
`HudController::initializeHotbarUI()`. After that, `m_controllers.updateAll(dt)`
plus state input routing drive it.

Typical input order in gameplay:

1. `OpenInventory` toggles the inventory panel.
2. `Interact` prioritizes merchant trade, then nearby container open, then
   pickup, then harvest interaction.
3. Locked containers report feedback instead of opening.
4. When the inventory is open, slot clicks may equip gear, consume usable items,
   transfer between inventory/container, loot all, or start a hotbar assignment.
5. Inventory slot drags use EDM slot-swap storage primitives. Dropping onto an occupied inventory slot swaps stacks, dropping onto an empty inventory slot moves the stack there, and dropping outside the inventory cancels the reorder.
6. During hotbar assignment, the controller tracks the dragged resource handle. Inventory-origin and container-origin drops over the hotbar assign through `HudController::assignHotbarItem(...)`; hotbar-origin drags reorder through `HudController::moveHotbarItem(...)`.

Open containers are validated during update. The controller auto-closes the
container view when the player walks out of interaction range or the container
handle/inventory becomes invalid.

## UI Contract

- `INVENTORY_PANEL_ID`, `INVENTORY_TITLE_ID`, `INVENTORY_STATUS_ID`, and `EVENT_LOG_ID` are stable component IDs used by gameplay state and tests.
- `refreshInventoryUI()` reflects ordered physical EDM inventory slots. Use `EntityDataManager::getInventorySlots(...)` for bulk slot reads instead of rebuilding the player grid from aggregate resource totals.
- `refreshInventoryUI()` rebuilds the grid entries, so click/drag handlers must copy any `ResourceHandle` they need before triggering refresh.
- Gear slots are displayed inside the existing inventory overlay; inventory items remain visible while gear is assigned.
- Container slots are controller-owned UI over EDM container inventory data.
- Pause/resume hides or restores inventory and hotbar UI through controller APIs instead of direct component ownership in the state.

## Rules

- Keep this controller state-scoped; it should not become a resource manager.
- Keep drag/drop policy in this controller. EDM only owns ordered slot storage, slot reads, and slot swaps.
- Keep gameplay stat effects separate from first-pass UI/state equipment handling unless a feature explicitly wires those effects.
- Unknown equipment slot metadata should fail equip instead of silently defaulting to weapon.

## Related Docs

- [HudController](HudController.md)
- [ControllerRegistry](ControllerRegistry.md)
- [EntityDataManager](../managers/EntityDataManager.md)
- [ResourceHandle System](../utils/ResourceHandle_System.md)
