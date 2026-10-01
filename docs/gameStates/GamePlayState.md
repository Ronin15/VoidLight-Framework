# GamePlayState

**Code:** `include/gameStates/GamePlayState.hpp`, `src/gameStates/GamePlayState.cpp`

## Overview

`GamePlayState` owns the main gameplay screen. It coordinates world loading,
player setup, camera ownership, state-scoped controllers, gameplay UI, event
tokens, GPU scene recording, pause/game-over transitions, and transition-safe
manager cleanup.

## Enter Flow

When no world is loaded, `enter()` sets a deferred loading intent and returns.
`update()` configures `LoadingState` with `GameStateId::GAME_PLAY` and performs
the transition.

When the world is ready, `enter()`:

- resumes global managers
- creates and initializes the player one tile east of the first settlement
  center (screen center if the world has no settlements)
- spawns the starter gear chest near the player
- caches the player handle in `AIManager`
- initializes the camera and `GPUSceneRecorder`
- registers controllers through `ControllerRegistry`
- creates event log, time label, and FPS label (session chrome)
- initializes inventory UI, then `HudController::initializeActionHUD()` and `initializeHotbarUI()`
- world already arrives inhabited from `WorldManager::loadNewWorld` (settlement NPCs + sparse wilderness Warriors, faction 1, Chase home role); debug `R` spawns a faction-1 Warrior via `spawnNpc` and drops player standing with faction 1 to the minimum via `AIManager::adjustPlayerStanding` (the stance table is untouched; not in the populate registry)
- subscribes state-owned transient handlers (`registerEventHandlers()`):
  - `Time` and `Weather`: time label, weather particles, and event log
  - `Harvest`: calls `WorldManager::handleHarvestResource()` to clear the
    depleted node's tile obstacle
  - `StanceChanged`: logs only events with `isTowardPlayer()` (an NPC
    faction's relation toward the player changed), prefixed with the
    settlement id when the incident had one; NPC-faction ↔ NPC-faction stance
    changes are not logged
  - `Scarcity`: player-relevance filter — the event is unfiltered at emission,
    so the handler logs it only when the player was the harvester or the
    depletion center is within the event's scarcity radius of the player

Pause/resume toggles session chrome (`event_log`, `time_label`, `fps`) directly
and the action HUD through `HudController::setVisible()`. The state does not
enumerate `hud_*` widget ids. Each update feeds `HarvestController` progress
into `HudController::setHarvestProgress()`.

## Controller Ownership

`GamePlayState` registers:

- `WeatherController`
- `DayNightController`
- `CombatController`
- `HudController`
- `InventoryController`
- `HarvestController`
- `ResourceRenderController`
- `SocialController`

`NPCRenderController` and `ProjectileRenderController` are direct state
members (render helpers), not registry entries.

The state stores controllers in `ControllerRegistry` and calls
`m_controllers.clear()` during exit so re-entry creates fresh instances with
valid player/UI references.

## Input Routing

- trade UI is modal through `SocialController::handleTradeInput(...)`
- `Pause` pushes `PauseState`
- `OpenInventory` delegates to `InventoryController`
- hotbar input delegates to `HudController`
- attack delegates to `CombatController`
- `Interact` prioritizes merchant trade, then nearby container open, pickup, and
  harvesting
- zoom commands call `Camera::zoomIn()` / `zoomOut()`

## Cleanup

Full exit and loading transitions both unregister state-owned handlers before
manager transition cleanup. AI-heavy cleanup follows the project manager order:

1. AI
2. Projectile
3. Background simulation
4. World
5. World resources
6. Event
7. Collision
8. Pathfinder
9. EDM
10. WorkerBudget
11. Particle

World unload happens before WRM/EventManager transition cleanup so persistent
world-unload handlers and reverse lookups remain available.

## Related Docs

- [LoadingState](LoadingState.md)
- [PauseState](PauseState.md)
- [ControllerRegistry](../controllers/ControllerRegistry.md)
- [InventoryController](../controllers/InventoryController.md)
- [HudController](../controllers/HudController.md)
- [GPU Rendering](../gpu/GPURendering.md)
