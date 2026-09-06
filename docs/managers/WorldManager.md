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
- populate settlement NPCs after resource init (`WorldPopulation`, keyed by `worldId`)
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

## World Population

`loadNewWorld` calls `populateWorldEntities()` immediately after
`initializeWorldResources()`, still under `m_worldMutex`. Population is a
load-time helper (`WorldPopulation`), not a manager singleton and not a
`GamePlayState` tile loop.

- Per overworld settlement: 1 merchant (Idle), 2 guards, 4 villagers (Wander).
- Sparse forest/haunted hostiles (Human/Warrior, faction 1, Attack) at one
  candidate per 64×64 tile block, capped at 32 hostiles.
- Total populated NPCs are capped at 256 per `worldId`.
- Default simulation tier is Active; `BackgroundSimulationManager` retier
  after the player exists. Populate does not assign tiers from camera/player.
- Registry is keyed by `worldId`. If that id is already populated, skip
  (idempotent). `unloadWorldLocked()` / `clearPopulatedNpcs` queue those
  NPC handles for destroy and drop the registry entry before
  `m_currentWorld.reset()`. `unloadWorld()` drains the EDM destruction
  queue on the calling (main/test) thread after dropping world locks.
- `LoadingState` pauses `AIManager` for the async load so
  `AIManager::update()` does not run against NPCs being created. WorldManager
  does not locally pause AI. `assignBehavior` still serializes index updates
  with `m_entitiesMutex`.
- Public queries (shared `m_worldMutex`): `isWorldPopulated(worldId)` and
  `getPopulatedNpcCount(worldId)` for the populate registry; `getSettlements()`,
  `findSettlementAtTile`, and `findSettlementAtPixel` for the current world
  (same current-world rule as `getTileCopyAt`).
- Debug `R` Warriors spawned from `GamePlayState` are not registered in the
  populated-NPC map.

Do not populate from `GamePlayState`. When saved-world `loadWorld` lands
later, it must call the same populate after `WorldData` is restored if NPCs
are not in the save.

## Rendering Notes

- GPU path records tile sprites directly; chunk textures are not used for scene submission.
- Season changes update cached texture IDs and invalidate relevant chunk work.

## Event Notes

`WorldManager` still triggers world loaded/unloaded events through `EventManager`, but it no longer owns the old `setupEventHandlers()` / `registerEventHandlers()` pattern that previous docs described.

## Guidance

- Treat world harvestables as EDM entities registered into WRM, not as an internal WorldManager-only resource table.
- If you add a new harvestable obstacle type, update both the world generation/render data and the obstacle-to-resource spawn logic here.
