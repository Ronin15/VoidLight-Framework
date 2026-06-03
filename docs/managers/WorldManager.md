# WorldManager

**Code:** `include/managers/WorldManager.hpp`, `src/managers/WorldManager.cpp`

## Overview

`WorldManager` owns the active world, chunk-oriented rendering setup, world
generation/loading, and the world-facing side of harvesting by spawning EDM
harvestables that match tile obstacles.

## Responsibilities

- load and unload worlds
- expose world bounds and dimensions
- coordinate chunk cache invalidation and season-driven tile refresh
- initialize harvestable/resource entities for the active world
- set the active world explicitly on `WorldResourceManager`

## Active World Handoff

When a world becomes current, `WorldManager` explicitly calls:

```cpp
WorldResourceManager::Instance().setActiveWorld(worldId);
```

This replaces the older event-driven active-world ownership model. Do not document or rely on `WorldManager` subscribing itself to world events to discover the active world.

## Harvestable Initialization

`initializeWorldResources()` now spawns harvestables with positional coherence between tiles and EDM entities.

### Obstacle-aligned spawning

For obstacle-backed resources, every matching tile obstacle becomes a harvestable at the same tile-centered position:

- `TREE` -> wood
- `ROCK` -> stone
- ore deposit obstacles -> ore harvestables
- gem deposit obstacles -> gem harvestables

This keeps:

- rendered obstacle location
- harvest interaction point
- depletion/update flow

in sync.

### Fallback biome/elevation spawning

Some resources are still distributed by biome or elevation when no dedicated tile obstacle exists:

- forest-only rare materials
- celestial or swamp specialty resources
- high-elevation specialty stone/resources

## Rendering Notes

- GPU path records tile sprites directly; chunk textures are not used for scene submission.
- Season changes update cached texture IDs and invalidate relevant chunk work.

## Event Notes

`WorldManager` still triggers world loaded/unloaded events through `EventManager`, but it no longer owns the old `setupEventHandlers()` / `registerEventHandlers()` pattern that previous docs described.

## Guidance

- Treat world harvestables as EDM entities registered into WRM, not as an internal WorldManager-only resource table.
- If you add a new harvestable obstacle type, update both the world generation/render data and the obstacle-to-resource spawn logic here.
