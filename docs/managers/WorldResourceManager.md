# WorldResourceManager

**Code:** `include/managers/WorldResourceManager.hpp`, `src/managers/WorldResourceManager.cpp`

## Overview

`WorldResourceManager` is a registry over EDM data, not a quantity store.

It tracks:

- which inventories belong to which world
- which harvestables belong to which world
- spatial indices for dropped items, harvestables, and containers
- the currently active world for proximity queries

Actual inventory quantities, dropped items, and harvestable state live in `EntityDataManager`.

## Responsibility Boundary

WRM behavior is query and registration focused. Quantity mutation belongs in EDM
inventory/resource APIs, not in WRM transfer-style APIs.

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

registerHarvestable(edmIndex, worldId);
unregisterHarvestable(edmIndex);

registerDroppedItem(edmIndex, position, worldId);
unregisterDroppedItem(edmIndex);

registerHarvestableSpatial(edmIndex, position, worldId);
unregisterHarvestableSpatial(edmIndex);

registerContainerSpatial(edmIndex, position, worldId);
unregisterContainerSpatial(edmIndex);
```

### Spatial Queries

```cpp
queryDroppedItemsInRadius(center, radius, outIndices);
queryHarvestablesInRadius(center, radius, outIndices);
queryContainersInRadius(center, radius, outIndices);
findClosestDroppedItem(center, radius, outIndex);
```

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

Gameplay and AI-heavy states should call `prepareForStateTransition()` before destroying entities and world state. This aligns with the repository-wide teardown order documented in `AGENTS.md`.
