# HarvestController

**Code:** `include/controllers/world/HarvestController.hpp`, `src/controllers/world/HarvestController.cpp`

## Overview

`HarvestController` is the progress-based harvesting controller for resource collection. It operates on harvestable entities already registered with `WorldResourceManager` and backed by `EntityDataManager`.

Supported flows include:

- chopping wood
- mining stone and ore deposits
- harvesting gem deposits
- gathering any other configured `HarvestType`

## Core API

```cpp
bool startHarvest();
void cancelHarvest();
void update(float deltaTime);

bool isHarvesting() const;
float getProgress() const;
VoidLight::HarvestType getCurrentType() const;
std::string_view getActionVerb() const;
Vector2D getTargetPosition() const;
```

Constants:

```cpp
MOVEMENT_CANCEL_THRESHOLD = 8.0f
```

Reach is the shared `VoidLight::HarvestCommit::HARVEST_RANGE` (48 px) from
`include/world/HarvestCommit.hpp`; AI foragers use the same constant for
arrival. The controller no longer defines its own range.

## Runtime Flow

1. `startHarvest()` queries nearby harvestables through `WorldResourceManager::queryHarvestablesInRadius(...)`.
2. It selects the closest valid, non-depleted EDM harvestable.
3. Harvest duration comes from `HarvestConfig` for the resolved `HarvestType`.
4. `update()` advances progress until complete or cancels if the player moves too far.
5. `completeHarvest()` calls `HarvestCommit::commit(target, player, 0)`. On `nullopt` (stale handle or already depleted) it cancels. On a yield it awards resources or spawns a dropped item fallback.

## Integration Details

- WRM provides the spatial query.
- EDM provides the harvestable payload, depletion state, and dropped-item creation.
- Depletion goes through the shared main-thread `HarvestCommit::commit`, the same path AI foragers use via `AIManager::commitQueuedHarvests()`. It performs the generation check, yield roll, EDM `markHarvestableDepleted`, WRM `notifyHarvestableStateChanged()`, `HarvestResourceEvent`, and `ScarcityEvent`. The player passes a reserve of 0, so it may take the last node in an area; that depletion emits `ScarcityEvent` with `availableCount == 0`. A node already depleted by an NPC in the same frame is rejected.
- Successful inventory inserts emit `ResourceChangeEvent` for UI and inventory synchronization.
- Harvest completion emits `HarvestResourceEvent` (from `HarvestCommit`) so world/tile systems can react visually.
- `GamePlayState` feeds `isHarvesting()` / `getProgress()` into `HudController::setHarvestProgress()` each frame; harvest UI is not owned here.

## Inventory Fallback

If the player inventory is full, harvest output is spawned as a dropped item in the active world instead of being discarded.

That means harvesting now depends on:

- a valid player inventory index when available
- `WorldResourceManager::getActiveWorld()` for dropped-item registration

## Usage Notes

- Treat this as a controller, not a data store. Persistent harvestable state remains in EDM.
- The controller keeps a reusable harvestable index buffer to avoid repeated allocation.
- Player movement is part of the interaction contract; harvesting is intentionally interruptible.
