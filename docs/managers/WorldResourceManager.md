# WorldResourceManager

**Code:** `include/managers/WorldResourceManager.hpp`, `src/managers/WorldResourceManager.cpp`

## Overview

`WorldResourceManager` is a registry over EDM data, not a quantity store.

It tracks:

- which inventories belong to which world
- which harvestables belong to which world (the harvestable spatial index is
  the single world-membership + spatial index, EDM indices only)
- spatial indices for dropped items, harvestables, and containers
- the currently active world for proximity queries

Actual inventory quantities, dropped items, and harvestable state live in `EntityDataManager`.

## Responsibility Boundary

WRM behavior is query and registration focused. Quantity mutation belongs in EDM
inventory/resource APIs, not in WRM transfer-style APIs.

Harvest depletion is not WRM policy. `HarvestCommit::commit`
(`include/world/HarvestCommit.hpp`, main thread) is the single depletion path
for the player and AI; WRM only versions the registry and counts available
nodes for it.

## Core API

### Lifecycle

```cpp
init();
clean();
prepareForStateTransition();
```

`prepareForStateTransition()` clears registries and spatial fast paths so AI/gameplay states can shut down cleanly before world teardown.

### World Tracking

```cpp
createWorld(worldId);
removeWorld(worldId);
hasWorld(worldId);
getWorldIds();
setActiveWorld(worldId);
getActiveWorld();
clearSpatialDataForWorld(worldId);
```

### Registration

```cpp
registerInventory(inventoryIndex, worldId);
unregisterInventory(inventoryIndex);

registerHarvestable(edmIndex, position, worldId);
unregisterHarvestable(edmIndex);
copyHarvestableIndices(worldId, out);  // snapshot static EDM indices; unordered; does not destroy

registerDroppedItem(edmIndex, position, worldId);
unregisterDroppedItem(edmIndex);

registerContainerSpatial(edmIndex, position, worldId);
unregisterContainerSpatial(edmIndex);
```

Harvestables have one container per world: the harvestable `SpatialIndex`
(world-membership + spatial index, EDM indices only) plus the
`m_harvestableToWorld` reverse lookup. `getHarvestableCount`,
`copyHarvestableIndices`, `queryHarvestableTotal`, `getWorldResources`, and the
radius queries all read it, so `clearSpatialDataForWorld` and `removeWorld`
empty the count and copy as well as the radius queries. The
`harvestablesRegistered` stat tracks the same container (register, unregister,
`clearSpatialDataForWorld`, `removeWorld`).

`copyHarvestableIndices` order is unspecified. Callers needing a deterministic
order sort the result (`AIManager::refreshHarvestableSnapshot` sorts by static
index before building the worker snapshot).

### Spatial Queries

```cpp
queryDroppedItemsInRadius(center, radius, outIndices);
queryHarvestablesInRadius(center, radius, outIndices);
queryContainersInRadius(center, radius, outIndices);
findClosestDroppedItem(center, radius, outIndex);
countAvailableHarvestablesInRadius(center, radius);  // live, non-depleted, any kind
```

`countAvailableHarvestablesInRadius` walks the active world's harvestable grid
cells directly under the shared registry lock (allocation-free) and counts live,
non-depleted harvestables of any resource kind. `HarvestCommit` uses it for the
NPC area reserve and the scarcity threshold.

`getStats().queryCount` counts `queryHarvestablesInRadius`,
`copyHarvestableIndices`, and `countAvailableHarvestablesInRadius` calls (and
therefore also the player `HarvestController`'s nearest-node lookups). Tests use
it to prove Forage workers do not query WRM per entity per frame.

### Harvestable Version

```cpp
getHarvestableVersion();          // monotonic, never reset
notifyHarvestableStateChanged();  // bump only; WRM stores no availability state
```

`m_harvestableVersion` is an atomic counter bumped by `registerHarvestable`,
`unregisterHarvestable`, `setActiveWorld`, `clearSpatialDataForWorld`,
`removeWorld`, `prepareForStateTransition`, `clean`, and
`notifyHarvestableStateChanged` (called by `HarvestCommit` after the EDM
depletion write). `AIManager` reads it before copying indices and rebuilds its
worker harvestable snapshot only when it changes.

### Query-only Resource Totals

```cpp
queryInventoryTotal(worldId, handle);
queryHarvestableTotal(worldId, handle);
queryWorldTotal(worldId, handle);
hasResource(worldId, handle, minimumQuantity);
getWorldResources(worldId);
```

## Active World Model

The active world is set explicitly by state/world code, typically through `WorldManager::loadNewWorld()`. Spatial queries without an explicit world parameter operate against `m_activeWorld`.

That means:

- dropped-item pickup
- harvestable proximity lookup
- container proximity lookup

depend on correct active-world setup.

## Relationship to EDM

WRM should be described as a fast lookup/indexing layer:

- EDM owns inventory content
- EDM owns harvestable depletion/yield data
- EDM creates dropped items and static entities
- WRM provides world grouping and spatial acceleration

## State Transition Notes

AI-heavy `exit()` unloads the world **before** WRM `prepareForStateTransition()` (GamePlay: destroy NPCs → AI/projectile/BSM/World prepare → `unloadWorld()` → then WRM prepare). WRM is an index; it is not cleared “before world teardown.” See `CLAUDE.md` and `GamePlayState::exit()`.
