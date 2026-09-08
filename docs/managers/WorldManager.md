# WorldManager

**Code:** `include/managers/WorldManager.hpp`, `src/managers/WorldManager.cpp`

## Overview

`WorldManager` owns the active world, chunk-oriented rendering setup, world
generation/loading, and coordinates harvest/NPC populate helpers. Harvest spawn
policy lives in `WorldHarvestInit`; NPC spawn policy lives in `WorldPopulation`
and `spawnNpc`. Registry and settlement queries stay on WorldManager.

## Responsibilities

- load and unload worlds
- expose world bounds and dimensions
- coordinate chunk cache invalidation and season-driven tile refresh
- initialize harvestable/resource entities for the active world (`WorldHarvestInit`)
- populate settlement NPCs after resource init (`WorldPopulation` / `spawnNpc`, keyed by `worldId`)
- destroy that world's harvestables then enqueue populated NPCs on unload
- set the active world explicitly on `WorldResourceManager`

## Active World Handoff

When a world becomes current, `WorldManager` explicitly calls:

```cpp
WorldResourceManager::Instance().setActiveWorld(worldId);
```

This replaces the older event-driven active-world ownership model. Do not document or rely on `WorldManager` subscribing itself to world events to discover the active world.

## Harvestable Initialization

`loadNewWorld` calls `WorldHarvestInit::initialize` after WRM `setActiveWorld`.
Spawn behavior is obstacle-aligned, with biome/elevation fallbacks when no
dedicated tile obstacle exists. Harvestables are static EDM entities registered
into WRM.

`unloadWorldLocked` snapshots that world's harvestable static indices from WRM,
destroys them via `EntityDataManager::destroyEntity` (immediate static path),
then clears populated NPCs and calls WRM `removeWorld`. `WorldManager::clean`
uses the same locked unload. Public `unloadWorld` drains the EDM destruction
queue after locks drop; the `loadNewWorld` worker uses locked unload only.

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
`WorldHarvestInit::initialize`, still under `m_worldMutex`. Population is a
load-time helper (`WorldPopulation` + `spawnNpc`), not a manager singleton and
not a `GamePlayState` tile loop.

- Per overworld settlement: 1 merchant, 2 guards, 4 villagers. Empty
  `behaviorOverride` keeps `classes.json` suggestedBehavior (Idle/Guard/Wander).
- Sparse forest/haunted hostiles (Human/Warrior, faction 1) pass `"Attack"` to
  `spawnNpc`. Do not change Warrior `suggestedBehavior` in JSON.
- Total populated NPCs are capped at 256 per `worldId`.
- Default simulation tier is Active; `BackgroundSimulationManager` retier
  after the player exists. Populate does not assign tiers from camera/player.
- Registry is keyed by `worldId`. If that id is already populated, skip
  (idempotent). `unloadWorldLocked()` / `clearPopulatedNpcs` queue those
  NPC handles for destroy and drop the registry entry before
  `m_currentWorld.reset()`. `unloadWorld()` drains the EDM destruction
  queue on the calling (main/test) thread after dropping world locks.
- `LoadingState` takes the exclusive structural window (`setGlobalPause(true)`).
  Gameplay producers pause; EventManager stays the gameplay and lifecycle bus
  with deferred drain on so WorldLoaded can complete (WorldManager is not
  paused). GameEngine skips destroy-drain while globally paused.
  `GamePlayState::enter()` already `setGlobalPause(false)`.
- Public queries (shared `m_worldMutex`): `isWorldPopulated(worldId)` and
  `getPopulatedNpcCount(worldId)` for the populate registry; `getSettlements()`,
  `findSettlementAtTile`, and `findSettlementAtPixel` for the current world
  (same current-world rule as `getTileCopyAt`).
- Debug `R` Warriors spawned from `GamePlayState` via `spawnNpc` are not
  registered in the populated-NPC map.

Do not dump later-slice policy into WorldManager (environment/stance/forage/
decision → AI/EDM; discovery → WorldData + SaveGameManager + HUD; background
tick → BSM).

Do not populate from `GamePlayState`. When saved-world `loadWorld` lands
later, it must call the same populate after `WorldData` is restored if NPCs
are not in the save.

## Rendering Notes

- GPU path records tile sprites directly; chunk textures are not used for scene submission.
- Season changes update cached texture IDs and invalidate relevant chunk work.

## Event Notes

`WorldManager` still triggers world loaded/unloaded events through `EventManager`. `setupEventHandlers()` still exists as a thin wrapper around `TileRenderer::subscribeToSeasonEvents()` (persistent Time handler). It is not a second public subscribe API; prefer subscribe-from-init / first load.

## Guidance

- Treat world harvestables as EDM entities registered into WRM, not as an internal WorldManager-only resource table.
- If you add a new harvestable obstacle type, update both the world generation/render data and `WorldHarvestInit`.
