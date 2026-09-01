# Framework Implementation Slices

Process for complete feature chunks in this repo. This file owns the process —
not a product roadmap. Add work as a `## Slice N` section when it is scheduled.

A **numbered slice** has a **Goal**, **Checklist**, and **Acceptance checks**.
It is not done until every item in **that section** is `[x]`, owning docs and
tests are updated, the slice-complete gate has passed, and
**cpp-review-specialist** has reviewed the diff.

Do not implement from chat notes. Add the section first, then implement from
it. **Code wins over stale slice prose.** Durable contracts live in `AGENTS.md`
and subsystem docs.

## Ground Rules

- Keep `ninja -C build app` and `./bin/<cfg>/VoidLight_Template` working after
  every slice.
- Partial wiring stays `[ ]` with remaining notes in that section — never
  implied complete elsewhere.
- If a dependent system does not exist yet, label the work as foundation and
  leave the checklist incomplete.
- Implement only the open slice's scope. No unrelated refactors.

## Gates (do not mix these)

Canonical gate names used by agents and `AGENTS.md`:

| Gate | When | What |
| --- | --- | --- |
| **Per-change** | Every edit while implementing | Targeted `ninja -C build` or `ninja -C build app` plus the named Boost.Test executable for the touched system. **Not** cppcheck, clang-tidy, ASan, TSan, or the core-only suite. |
| **Slice complete** | Before marking the slice done | Every Checklist and Acceptance item `[x]`; owning docs and tests updated; `ninja -C build`; then the Boost.Test executables covering the slice's changed code (`--run_test` when a case is enough). No `run_all_tests.sh --core-only`. No benches. |
| **Slice review** | Before committing the slice | `cpp-review-specialist` on the slice diff. Do not commit a completed slice unreviewed. |
| **Branch / PR** | When the branch is ready to merge — not each commit or slice completion | `./tests/test_scripts/run_all_tests.sh --core-only --errors-only` (`.bat` on Windows); cppcheck, clang-tidy, ASan, TSan (sanitizers are mutually exclusive). Optional Valgrind. |

Interactive `./bin/<cfg>/VoidLight_Template` is display-gated. Leave visual/GPU
residuals `[ ]` until confirmed; they do not block slice-complete.

## Implementing a slice

Route through `.grok/skills/cpp-workflows` when using specialists.

1. **Open or add** `## Slice N: …`. Read Goal, Current foundation, and
   Architecture notes. Cross-read [ARCHITECTURE.md](ARCHITECTURE.md) and any
   doc linked in the slice.
2. **Design first** (`cpp-design-specialist`) if ownership, lifecycle, or
   multi-manager flow is unclear. Do not mark the slice complete in the design.
3. **Implement only that slice's scope** (`cpp-specialist`) in the owning
   `include/` + `src/` modules. Check off Checklist items as each integration
   lands (runtime behavior + tests). Use the **per-change** gate while iterating.
4. **Satisfy Acceptance checks.** Update durable docs when contracts change.
5. **Slice-complete gate**, then **slice review**. Do not commit until review
   has run.
6. When fully complete, move the entire section to
   `docs/framework-implementation-slices-archive.md` (create that file when the
   first slice is archived). Leave residual follow-ups only in a later slice —
   do not delete acceptance history.

### Section shape

| Block | Use |
| --- | --- |
| **Goal** | What “done” means |
| **Current foundation** | What already exists — do not rebuild |
| **Architecture notes** / **Problem** | Constraints and ownership |
| **Checklist** | `[ ]` / `[x]` implementation steps |
| **Acceptance checks** | `[ ]` / `[x]` verification gates — all required |
| **Status** | Open/partial note, or one-line completion record before archive |

### Template

```markdown
## Slice N: Title

Goal: …

Current foundation:

- …

Architecture notes:

- …

Checklist:

- [ ] …
- [ ] Owning docs updated
- [ ] Tests updated in the same change

Acceptance checks:

- [ ] …
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test executables for the changed code pass
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started
```

## Slice Records

Implement from these sections, not from chat notes. Implement only the open
slice's scope. Slice 1 is implemented (visual confirm leftover). Slice 2 is
next. Do not implement Slices 3–8 until the prior slice is done.

Scheduled order (do not skip ahead). Data deps may be narrower than schedule
order; do not pull a later slice forward unless this file is updated first.

| Slice | Data depends on | Scheduled after |
| --- | --- | --- |
| 1 Gameplay HUD | — | — |
| 2 World population | — | 1 |
| 3 Environment-driven AI | 2 | 2 |
| 4 Faction stance | 2 | 3 |
| 5 Survival / forage | 2, 4 | 4 |
| 6 Autonomous decision | 2, 3, 4, 5 | 5 |
| 7 Production minimap | 2 (4 for faction-colored dots) | 6 |
| 8 Background-tier simulation | 2, 5 | 7 |

## Slice 1: Gameplay HUD ownership and vitals cohesion

Goal: `GamePlayState` has one state-scoped owner for the persistent **action HUD** (player vitals, target frame, hotbar, harvest progress). Pause/resume no longer enumerates `HudController` widget ids. Combat HUD construction leaves `UIManager` policy and lives on `HudController`. Demo states may reuse the controller; they do not expand its contract.

Current foundation (at slice start; landed APIs are in Status):

- `HudController` already owns hotbar widgets and transient combat-target state. Hotbar init is optional (`initializeHotbarUI()`); demos may skip it.
- `UIManager::createCombatHUD()` / `updateCombatHUD()` / `destroyCombatHUD()` currently own vitals widget construction. `GamePlayState` and `AdvancedAIDemoState` both call those helpers and push target state from `HudController` every frame.
- `GamePlayState` owns session chrome: `event_log`, `time_label`, FPS (`F2`). Pause/resume hardcodes `hud_*` ids plus that chrome.
- `HarvestController` exposes `getProgress()` with no HUD widget.
- `InventoryController` / `SocialController` own overlays and already write the GamePlayState event log by id. That stays out of `HudController`.
- `src/controllers/ui/AGENTS.md`: reusable gameplay UI flow belongs in controllers; one-off status text stays on the state.

Architecture notes:

- **GamePlayState is the production HUD consumer.** Design the controller for that state. `AdvancedAIDemoState` and other demos are showcase code; they may add `HudController` and call a subset of init, but they must not justify new public APIs, extra widgets, or a “universal HUD facade.”
- **Do not kitchen-sink `HudController`.** It is not a dump for every on-screen label. Keep time, event log, and FPS on `GamePlayState` (session chrome, multiple writers, debug toggle). Keep inventory and trade on their controllers.
- `HudController` owns the action HUD subtree: HP/SP bars, target name/bar, hotbar, harvest progress. It creates those widgets via `UIManager` primitives, updates them with dirty-flag `setValue`/`setText`/visibility, and exposes `setVisible(bool)` for pause/resume of **its** subtree.
- `UIManager` stays the widget service (storage, layout, theme, hit-testing). Move combat-HUD *policy* off the manager. Existing `createCombatHUD` helpers should die or become private/test-only if design proves a thin helper is still needed; production states must not call them.
- Harvest progress is a HUD widget driven by reading `HarvestController` from `GamePlayState` (state coordinates sibling controllers) or a narrow query the state passes in. Do not make `HudController` a harvest owner.
- Shared spacing/z-order/style for the action HUD goes in `UIConstants`. Controller-local geometry stays in the HudController `.cpp`.
- Performance: no per-frame allocations; skip UI mutations when values are unchanged; pause/resume is one visibility pass over the HUD subtree, not a list of `hud_*` strings in `GamePlayState`.
- Tests: `HudControllerTests` plus any GamePlayState pause/resume coverage that currently lists combat HUD ids. `UIManagerFunctionalTests` combat-HUD cases move to the controller boundary. Visual residuals stay `[ ]` until interactive confirmation.

Checklist:

- [x] `cpp-design-specialist` locks ownership (HudController vs UIManager vs GamePlayState vs demos)
- [x] `HudController` creates and updates action HUD widgets (vitals, target, hotbar, harvest progress)
- [x] `GamePlayState` pause/resume uses `HudController::setVisible()` and only toggles state-owned chrome (`event_log`, `time_label`, `fps`)
- [x] `UIManager::createCombatHUD` / `updateCombatHUD` / `destroyCombatHUD` are removed from the production state path
- [x] `AdvancedAIDemoState` consumes the existing HudController subset without new demo-only HUD APIs
- [x] Dirty-flag vitals/target/harvest updates (no unconditional per-frame `setText`/`setValue`/`setComponentVisible`)
- [x] Owning docs updated (`docs/controllers/HudController.md`, controller README, GamePlayState HUD notes)
- [x] Tests updated in the same change (`HudControllerTests`; move combat HUD coverage off UIManager-only helpers)

Acceptance checks:

- [x] GamePlayState pause hides action HUD through the controller; resume restores it without listing `hud_*` ids
- [x] Harvest in progress shows a progress widget; cancel/complete hides it
- [x] Target frame still auto-expires; hotbar still optional when `initializeHotbarUI()` is not called
- [x] Demo states still compile and run their existing HUD subset without driving new HudController surface
- [x] `ninja -C build` passes
- [x] Targeted Boost.Test: `hud_controller_tests`, plus any moved UIManager combat-HUD cases
- [x] Slice reviewed (`cpp-review-specialist`) before commit
- [ ] Interactive visual confirmation in `VoidLight_Template` (pause/resume HUD, harvest caption at 720p/1080p, target expiry)

Status: Partial — action HUD ownership implemented and reviewed; visual/GPU confirmation remaining.

Landed (do not rebuild): `HudController::initializeActionHUD()`, `setVisible(bool)`, `setHarvestProgress(bool, float)`; `initializeHotbarUI()` optional; public `hud_*` ids; session chrome still on `GamePlayState`. Combat HUD helpers are gone from `UIManager`.

## Slice 2: World population (living settlements)

Goal: Loading a world populates it with NPCs from persisted settlement records. Overworld villages get a merchant, guards, and villagers; forest/haunted tiles outside settlements get sparse hostiles. Unload does not leave those NPCs behind. Reload after unload does not duplicate them. Pause/resume does not respawn. `GamePlayState` does not loop tiles and `createNPC`. HUD, trade, and combat work against these NPCs. The populate/clear API is keyed by `worldId` so a later dungeon world can pass different records without a new owner.

Current foundation:

- Production load is `LoadingState` → `WorldManager::loadNewWorld` (`src/gameStates/LoadingState.cpp`). GamePlayState configures a 200×200 world (`src/gameStates/GamePlayState.cpp`). `WorldManager::loadWorld(const std::string& worldId)` is a stub (“Loading saved worlds not yet implemented”) — do not wait on it.
- `loadNewWorld` already runs on a `ThreadSystem` worker. After generation it calls `WorldResourceManager::createWorld` / `setActiveWorld` then `initializeWorldResources()`, which spawns harvestables with `EntityDataManager::createHarvestable` on that same thread (`src/managers/WorldManager.cpp`).
- `WorldGenerator::generateBuildings` (`src/world/WorldGenerator.cpp`) places villages (`WorldSpawnConfig::Buildings::VILLAGE_RADIUS = 12`, density `area / 8000`, min distance 40 tiles, 3–8 buildings of 2×2) and discards `villageCenters` after generation. `WorldData` (`include/world/WorldData.hpp`) is `worldId` + tile `grid` only. Building tiles keep `buildingId` / `isTopLeftOfBuilding`.
- NPC create: `EntityDataManager::createNPCWithRaceClass(pos, race, class, sex, factionOverride)`. `GeneralMerchant` sets `FLAG_MERCHANT` from `res/data/classes.json`. Behaviors: `AIManager::assignBehavior(handle, name)` with registered names Idle/Wander/Chase/Patrol/Guard/Attack/Flee/Follow.
- `EventManager` has a persistent `MerchantSpawn` handler; `spawnMerchant` does not need GamePlayState. Prefer one spawn path with `createNPCWithRaceClass` so populate does not depend on deferred event drain (GamePlayState has not entered yet).
- `GamePlayState::enter()` currently calls `spawnMerchant("GeneralMerchant", …)` after HUD init. Debug `R` (`VOIDLIGHT_DEBUG_ONLY`) spawns a hostile Warrior — keep it.
- `GamePlayState::exit()` already calls `AIManager::destroyAllNPCsForStateTransition()` then `unloadWorld()`. `WorldManager::prepareForStateTransition()` does **not** unload the world.
- Player is created in `GamePlayState::enter()`, after load. `BackgroundSimulationManager::update()` already retier from the player/camera every 120 frames (`updateSimulationTiers`).
- Classes: Guard (`suggestedBehavior` Guard, faction 0), Villager (Wander, 0), GeneralMerchant (Idle, merchant), Warrior (Chase/Attack, faction 1).
- Tests: `tests/world/WorldManagerTests.cpp`. Village placement requires world ≥ 26×26.

Architecture notes:

- Add `SettlementRecord` to `WorldData.hpp`: `uint32_t id`, `int centerTileX`, `int centerTileY`, `int radiusTiles`, `Biome biome`, `uint8_t faction`, `uint8_t buildingCount`. `WorldData` holds `std::vector<SettlementRecord> settlements`.
- `WorldGenerator` writes each accepted village center into `settlements` (do not discard `villageCenters`). `radiusTiles = VILLAGE_RADIUS` (12). `biome` from the center tile. `faction = 0` for overworld villages. `buildingCount` = buildings actually placed. `id` is 1-based index in the vector.
- Populate runs from `WorldManager::loadNewWorld` immediately after `initializeWorldResources()`, same `worldId` as WRM. Clear the worldId registry and destroy NPCs spawned for that `worldId` from `unloadWorldLocked()`. Do not populate from `GamePlayState::enter()`. Do not implement the `loadWorld` stub in this slice; when saved-world load lands later, it must call the same populate after `WorldData` is restored if NPCs are not in the save.
- `cpp-design-specialist` names the populate type and its `worldId` register/query/clear API. Placement: world-load helper next to `initializeWorldResources`, called from `loadNewWorld` / `unloadWorldLocked`. Query API on `WorldManager` (or the named type) for later slices: settlements for the current/given `worldId`, and point-in-radius lookup (`center + radiusTiles * TILE_SIZE`).
- Spawn in pixel space like harvestables: `tile * TILE_SIZE (32) + TILE_SIZE * 0.5f`. Skip water, `obstacleType != NONE`, and `buildingId != 0` (except guards, which spawn on `isTopLeftOfBuilding` tiles).
- Per settlement, locked counts and `classes.json` behaviors:
  - 1× `Human` / `GeneralMerchant`, `assignBehavior(..., "Idle")`, at or adjacent to the village center.
  - 2× `Human` / `Guard`, `"Guard"`, on distinct `isTopLeftOfBuilding` tiles in the radius (fall back to walkable tiles near center if fewer buildings).
  - 4× `Human` / `Villager`, `"Wander"`, walkable tiles inside the radius.
- After `assignBehavior`, write the assigned `BehaviorType` as **home role** (new `uint8_t` on `CharacterData` or `NPCMemoryData` — design picks). `CharacterData.behaviorType` stays the current behavior. Slice 6 reads home role; do not leave it unset.
- Forest and haunted tiles **outside** every settlement radius: sparse hostiles. `Human` / `Warrior`, faction override 1, `"Attack"`. One candidate per 64×64 tile block that contains forest/haunted land; skip if inside any settlement radius; cap 32 hostiles per world.
- Cap total populated NPCs at 256 per `worldId` (named constant). Production 200×200 is ~5 villages → 5×7 + hostiles, well under the cap.
- Do **not** set simulation tiers from player position at populate (player does not exist yet). Leave default `Active`; `BackgroundSimulationManager` retier after GamePlayState is running.
- Populate is idempotent: if `worldId` is already registered as populated, skip. `unloadWorldLocked` clears that registration and destroys those handles. Pause/resume never calls `loadNewWorld`.
- Remove the `GamePlayState` bootstrap `spawnMerchant`. Debug `R` stays.
- Files: `include/world/WorldData.hpp`, `src/world/WorldGenerator.cpp`, `include/managers/WorldManager.hpp`, `src/managers/WorldManager.cpp`, populate implementation next to world load (design names the type), `src/gameStates/GamePlayState.cpp`, `docs/managers/WorldManager.md`, `docs/gameStates/GamePlayState.md`, `tests/world/WorldManagerTests.cpp` (and a focused populate test file if the WorldManager suite is the wrong owner).
- Out of scope: implementing `loadWorld` / save of live NPCs, dungeon GameState, faction wars, forage, decision layer, minimap.

Checklist:

- [ ] `cpp-design-specialist` names the populate type and its `worldId` register/query/clear API (called from `WorldManager::loadNewWorld` / `unloadWorldLocked`)
- [ ] `SettlementRecord` + `WorldData::settlements`; generator persists village centers (id, biome, faction 0, buildingCount, radius 12)
- [ ] Populate after `initializeWorldResources()`; destroy+clear on `unloadWorldLocked`; skip if already populated
- [ ] Per-settlement 1 merchant Idle + 2 Guard + 4 Villager Wander; sparse forest/haunted Warriors (faction 1, Attack); walkable spawn; cap 256
- [ ] Home role stored at assign time
- [ ] `GamePlayState` bootstrap merchant removed
- [ ] Settlement query by `worldId` and by point (pixel or tile)
- [ ] Owning docs updated
- [ ] Tests updated in the same change (settlements persisted on generate; populate on `loadNewWorld`; no leftover NPCs after unload+load; pause/resume does not populate; point query; second `worldId` after unload does not keep the first world’s NPCs)

Acceptance checks:

- [ ] Overworld `loadNewWorld` yields inhabited villages without `GamePlayState` calling `createNPC` in a tile loop
- [ ] Unload then `loadNewWorld` does not leave the previous world’s NPCs
- [ ] Pause/resume does not respawn
- [ ] Populate/clear APIs take `worldId`
- [ ] Distant NPCs become non-Active after `BackgroundSimulationManager` retier (not at spawn)
- [ ] Slice 1 HUD / social / combat work against spawned NPCs (merchant is tradeable; guards/hostiles are targetable)
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test: `world_manager_tests` plus the populate suite
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started.

## Slice 3: Environment-driven AI

Goal: Guard/Chase/Wander/Patrol/Flee/Attack use a per-frame environment snapshot so night and heavy weather reduce detection and change move-speed/caution. Worker batches do not call `WeatherController` or `GameTimeManager`.

Current foundation:

- `GameTimeManager` dispatches `EventTypeId::Time` and `EventTypeId::Weather`. It exposes `getGameHour()`, `getTimeOfDayName()`, `isNighttime()` — not a `TimePeriod` enum and not current weather. `WeatherController` and `DayNightController` are state-scoped and hold the live weather/period for UI/particles. AI must not call those controllers.
- `TimePeriod` bounds (`include/events/TimeEvent.hpp`): Morning 5–8, Day 8–17, Evening 17–21, Night 21–5.
- `WeatherType` (`include/events/WeatherEvent.hpp`): Clear, Cloudy, Rainy, Stormy, Foggy, Snowy, Windy, Custom. `WeatherParams.intensity` is 0–1; `visibility` is 0–1.
- `AIManager::update()` already caches player position, world bounds, and `getTotalGameTimeSeconds()` on the main thread before `processBatch()`. Guard uses `cachedDetectionRange` (`src/ai/behaviors/GuardBehavior.cpp`).
- `BehaviorContext` is the batch contract (`include/ai/BehaviorExecutors.hpp`).
- Slice 2 NPCs are the population this modifies.

Architecture notes:

- Add a compact snapshot on `AIManager` (visibility, detectionScale, moveSpeedScale, cautionScale), filled once per `update()` on the main thread. Copy it onto `BehaviorContext` for workers. Do not add `GameTimeManager` / `WeatherController` calls inside `execute*`.
- Time: derive `TimePeriod` from `GameTimeManager::getGameHour()` with the `TimePeriod` bounds above (same as `DayNightController`). Weather: `AIManager::init()` registers a persistent `EventTypeId::Weather` handler that stores last `WeatherType` + intensity/visibility. Default weather is Clear / intensity 1 / visibility 1 until the first event. Do not register this from `GamePlayState`.
- Modifier table in `src/ai/` or `BehaviorConfig.hpp` constants — not magic numbers at each call site. Combine as `timeScale * weatherScale` (clamp each output to `[0.25, 1.5]`):

  | | detection | moveSpeed | caution |
  | --- | --- | --- | --- |
  | Morning | 0.90 | 1.00 | 1.00 |
  | Day | 1.00 | 1.00 | 1.00 |
  | Evening | 0.85 | 1.00 | 1.10 |
  | Night | 0.55 | 0.90 | 1.30 |
  | Clear | 1.00 | 1.00 | 1.00 |
  | Cloudy | 0.95 | 1.00 | 1.00 |
  | Rainy | 0.80 | 0.90 | 1.15 |
  | Stormy | 0.55 | 0.75 | 1.40 |
  | Foggy | 0.45 | 0.95 | 1.25 |
  | Snowy | 0.70 | 0.70 | 1.20 |
  | Windy | 0.90 | 0.95 | 1.05 |

  `Custom` weather uses Clear scales. If `visibility < 1`, detectionScale is also multiplied by `visibility`.
- Apply `detectionScale` in Guard/Chase perception (`cachedDetectionRange` and equivalent). Apply `moveSpeedScale` to Wander/Patrol/Flee/Attack movement speeds. Apply `cautionScale` as extra dwell / slower direction change / earlier flee threshold where those behaviors already have a timer or radius. Keep existing behavior types.
- Optional shelter: only if detection/speed alone is hollow — Wander/Idle prefer `ObstacleType::BUILDING` tiles in Stormy/Snowy using world tile queries already legal on the main-thread cache.
- Files: `include/managers/AIManager.hpp`, `src/managers/AIManager.cpp`, `include/ai/BehaviorExecutors.hpp`, Guard/Chase/Wander/Patrol/Flee/Attack `.cpp`, `docs/ai/AIManager.md`, `docs/ai/BehaviorExecutionPipeline.md`, `tests/BehaviorFunctionalityTest.cpp` and/or `tests/managers/AIManagerEDMIntegrationTests.cpp`.
- Out of scope: HUD weather glyph, seasonal migration, new behavior family unless shelter is required.

Checklist:

- [ ] Environment snapshot cached in `AIManager::update()` before batches; exposed on `BehaviorContext`
- [ ] Modifier table (time × weather) with the values above
- [ ] Persistent Weather handler on `AIManager`; time from `getGameHour()`
- [ ] Guard/Chase/Wander/Patrol/Flee/Attack consume the scales
- [ ] Owning docs updated
- [ ] Tests updated in the same change (night vs noon detection, storm vs clear speed, workers do not call weather/time singletons)

Acceptance checks:

- [ ] Same NPC detects the player at shorter range at night / in heavy weather than at clear noon
- [ ] Move-speed/caution change under storm modifiers
- [ ] Worker batches do not call `WeatherController` / `GameTimeManager`
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test: `behavior_functionality_tests` and/or `ai_manager_edm_integration_tests`
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slice 2.

## Slice 4: Faction stance and territory

Goal: Attack, help, and flee-to-allies use a 16×16 stance table on `AIManager` instead of `faction == 1`. Settlements from Slice 2 supply default faction and a point-in-radius territory query. Combat death, theft, and gifts can change stance on the main thread. Player faction standing is stored beside, not instead of, `Behaviors::getRelationshipLevel`.

Current foundation:

- `CharacterData.faction`: 0 Friendly, 1 Enemy, 2 Neutral (`include/managers/EntityDataTypes.hpp`). `createNPCWithRaceClass` already applies `defaultFaction` from `classes.json` or `factionOverride`.
- `AIManager::MAX_FACTIONS = 16`; `m_factionEdmIndices`; `scanFactionInRadius()`; `setFaction()` / `onEntityFactionChanged()`.
- Guard `callForHelp` is same-faction id (`src/ai/behaviors/GuardBehavior.cpp` still has `faction == 1` for player hostility). Attack filters `faction == 0` in at least one path (`src/ai/behaviors/AttackBehavior.cpp`).
- `SocialController` gifts/theft/alerts; `Behaviors::getRelationshipLevel(npc, subject)` is per-NPC memory (`include/ai/BehaviorExecutors.hpp`).
- Collision layers from faction 0/1/2 in `EntityDataManager::applyFactionCollision`.
- Slice 2 `SettlementRecord.faction` + point-in-radius query.

Architecture notes:

- Stance enum: Allied / Neutral / Hostile. Storage: `std::array<std::array<…>, MAX_FACTIONS>` on `AIManager`. Defaults: same id → Allied; 0 vs 1 → Hostile; all other pairs → Neutral. Main-thread writes after combat commit and SocialController theft/gift.
- `scanFactionInRadius` / help / Attack target filter / Flee ally seek consult stance, not raw `==`. Replace the `faction == 1` / `faction == 0` hostility checks in Guard and Attack.
- Territory: query Slice 2 settlements (`center + radiusTiles * TILE_SIZE`) for faction at a point. No new spatial hash.
- Player standing: compact per-faction scores on the player’s EDM character/memory sidecar, updated from the same main-thread commits. Keep `getRelationshipLevel(npc, subject)` for individuals.
- Event: reuse combat/social events if they already carry enough; otherwise add `EventTypeId` (next unused; bump `COUNT`) for stance-changed so the GamePlayState event log can print a line. Do not scrape the table from UI.
- Collision: keep 0/1/2 player-facing layers unless stance vs player requires a mapping update in `applyFactionCollision` — do that in EDM, not a new system.
- Files: `include/managers/AIManager.hpp`, `src/managers/AIManager.cpp`, Guard/Attack/Flee, `src/controllers/social/SocialController.cpp`, `include/managers/EntityDataTypes.hpp` if player standing is EDM, `docs/ai/AIManager.md`, `docs/controllers/SocialController.md`, `tests/BehaviorFunctionalityTest.cpp`, `tests/controllers/SocialControllerTests.cpp`.
- Out of scope: diplomacy UI, minimap colors, scripted wars.

Checklist:

- [ ] Stance table on `AIManager`; help/attack/flee consume it (no `faction == 1` hostility)
- [ ] Settlement default faction + point-in-settlement query
- [ ] Main-thread stance updates from lethal combat, theft, gift
- [ ] Player faction standing layered on existing memory APIs
- [ ] Event log can observe settlement/faction hostility change
- [ ] Owning docs updated
- [ ] Tests updated in the same change (hostile NPCs engage, same-stance help, theft worsens stance, `getRelationshipLevel` unchanged)

Acceptance checks:

- [ ] Hostile-faction NPCs engage without `faction == 1` hardcoding
- [ ] Same-stance guards still propagate alerts; other stances do not
- [ ] Killing/theft can worsen stance; it is test-observable
- [ ] Per-NPC relationship APIs still pass
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test: `behavior_functionality_tests`, `social_controller_tests`
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slice 2. Scheduled after Slice 3.

## Slice 5: Survival and resource AI

Goal: NPCs with resource need path to WRM harvestables, deplete them through the same EDM harvest path as the player, and react to scarcity. Player `HarvestResourceEvent` participates. Workers do not query WRM every entity every frame.

Current foundation:

- `WorldResourceManager::queryHarvestablesInRadius` (`include/managers/WorldResourceManager.hpp`) is a registry/spatial index over EDM, guarded by `m_registryMutex`. `HarvestController` is player-only and already fires `HarvestResourceEvent` (`src/controllers/world/HarvestController.cpp`). `ResourceChangeEvent` already fires.
- NPC inventories exist on `CharacterData.inventoryIndex`. No need field. `BehaviorType` is Wander…Idle + Custom (`include/ai/BehaviorConfig.hpp`); no Forage. Variant state lives in per-behavior pools (`include/ai/BehaviorStateData.hpp`).
- Slice 2 NPCs and Slice 4 stance are the actors.
- EDM expansion rule (`EntityDataTypes.hpp`): fields needed every frame go on the hot/character line; “some NPCs, sometimes” go in `SparseSidecar`.

Architecture notes:

- Add need/pressure as `SparseSidecar` (0–1, decay per AI tick, threshold to forage). Clear on destroy / `prepareForStateTransition()`.
- Add `BehaviorType::Forage` immediately before `Custom`, bump `COUNT`, register `"Forage"` in `AIManager::registerDefaultBehaviors()`. Add `ForageBehaviorConfig` + `ForageStateData` pools and `executeForage` in `BehaviorExecutors` / `src/ai/behaviors/`. Path to nearest harvestable from a **sampled** WRM query: cooldown or need-crossed; reusable buffer; snapshot harvestable positions on the main thread in `AIManager::update()` if workers must not take `m_registryMutex`.
- Deplete via the existing harvestable EDM payload / `HarvestResourceEvent` so world tiles update the same way as player harvest (`HarvestController` already emits that event).
- Scarcity: when a sampled radius query returns below a threshold (named constant, e.g. fewer than 2 harvestables in 512 px), emit an event (reuse `ResourceChangeEvent` if it can carry “depleted area”; otherwise add `EventTypeId` and bump `COUNT`). Attack/Wander/Flee may `switchBehavior` using Slice 4 stance. Player harvest of the last local node must emit the same signal.
- Files: `include/managers/EntityDataTypes.hpp`, `src/managers/EntityDataManager.cpp`, `include/ai/BehaviorConfig.hpp`, `BehaviorStateData.hpp`, `BehaviorExecutors.hpp/.cpp`, new forage sources, `include/ai/BehaviorConfig.hpp` COUNT, `include/events/EventTypeId.hpp` if scarcity is new, `HarvestController.cpp` only if the player path must emit the same scarcity event, `docs/ai/BehaviorModes.md`, `docs/managers/WorldResourceManager.md`, `tests/ai/` or `BehaviorFunctionalityTest.cpp`, `tests/controllers/HarvestControllerTests.cpp` if player signal changes.
- Out of scope: crafting, shop sim, player economy HUD.

Checklist:

- [ ] EDM need/pressure sidecar with slot cleanup
- [ ] `BehaviorType::Forage` + config/state/executor; `switchBehavior` to/from it; registerDefaultBehaviors
- [ ] Sampled WRM queries or main-thread snapshot; no per-entity per-frame worker WRM
- [ ] Depletion uses existing harvestable/EDM/`HarvestResourceEvent` path
- [ ] Scarcity event; player harvest emits it too
- [ ] Owning docs updated
- [ ] Tests updated in the same change (need decay, forage depletes, scarcity on player harvest, no worker WRM spam)

Acceptance checks:

- [ ] Need-driven NPCs walk to harvestables and deplete them
- [ ] Local depletion is observable and changes NPC behavior
- [ ] Player harvesting the same node participates
- [ ] Workers do not WRM-query every entity every frame
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test: `behavior_functionality_tests`, plus harvest tests if the player signal changes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slices 2 and 4.

## Slice 6: Autonomous decision layer

Goal: In `AIManager::processBatch`, a staggered selector can `switchBehavior` from scored motives (health, emotion, memory, stance, environment snapshot, need, `PersonalityTraits`) and restore the NPC’s home role when override scores drop. No A↔B flicker on consecutive frames.

Current foundation:

- `Behaviors::switchBehavior` + `AICommandBus`. Idle/Patrol/Guard already contain hardcoded switches (`src/ai/behaviors/*`). `AIManager::commitQueuedBehaviorTransitions()` clears behavior data before `init()`; new state is set after that commit (`AGENTS.md`).
- Slice 2 writes home role at populate/assign. Slices 3–5 supply environment, stance, need.
- `PersonalityTraits` on `NPCMemoryData` (bravery, aggression, composure, loyalty) — written at spawn, read every frame.

Architecture notes:

- Read home role from the field Slice 2 stored. Populate/debug `R` / tests that `assignBehavior` without going through populate must set home role too (assign path, not only the village helper).
- Add `Behaviors::selectBehaviorIfNeeded(ctx)` called from the fused loop on stagger `edmIndex % 8 == frameCounter % 8`. Hysteresis: require override score ≥ home score + 0.15 to leave, and home ≥ override + 0.15 to return. Cooldown 2.0 s in shared or per-variant state after a switch.
- Scores (0–1, then scaled by personality): flee from fear/health; attack from Hostile stance + aggression; forage from need ≥ threshold; else home role. Bravery lowers flee weight; aggression raises attack; loyalty raises home.
- Restore home role through `switchBehavior`. Set restored config/state after the transition commit.
- Do not remove existing in-behavior switches this slice; the selector is the cross-behavior preemption. Follow stays a scripted/assign-only role (not a selector target).
- Files: `include/ai/BehaviorExecutors.hpp`, `src/ai/BehaviorExecutors.cpp`, `src/managers/AIManager.cpp` fused loop, home-role field if assign path still needs it, `docs/ai/BehaviorExecutionPipeline.md`, `tests/BehaviorFunctionalityTest.cpp`, `tests/managers/AIManagerEDMIntegrationTests.cpp`.
- Out of scope: GOAP, HTN, behavior trees, new planner types.

Checklist:

- [ ] Home role set on every `assignBehavior` path that populate uses; debug/test assigns included
- [ ] Selector in fused loop with stagger 8, hysteresis 0.15, cooldown 2 s
- [ ] Preemption flee/attack/forage; restore home role post-commit
- [ ] PersonalityTraits scale weights
- [ ] Owning docs updated
- [ ] Tests updated in the same change (no flicker, preemption, return-to-role, personality difference)

Acceptance checks:

- [ ] NPCs leave home role under threat/need and return when the override ends
- [ ] No A↔B flicker on consecutive frames under stable inputs
- [ ] Personality differences are observable in identical setups
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test: `behavior_functionality_tests`, `ai_manager_edm_integration_tests`
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slices 2–5.

## Slice 7: Production minimap

Goal: GamePlayState shows a gameplay minimap: local area, discovery grid, player marker, settlement dots from Slice 2. Discovery is world data and is saved/loaded with the existing save slot. Pause uses existing `HudController::setVisible()` or the sibling controller’s equivalent.

Current foundation:

- Slice 1: `HudController::initializeActionHUD`, `setVisible`, public `hud_*` ids. Session chrome stays on `GamePlayState`. Do not kitchen-sink unrelated widgets into the action HUD.
- OverlayDemo has demo-only `overlay_demo_minimap_panel`. `docs/ui/Minimap_Implementation.md` is an old widget-in-UIManager plan — do not implement that document’s ownership.
- Slice 2 settlements (query by worldId). Slice 4 faction for optional dot color.
- `SaveGameManager` (`docs/managers/SaveGameManager.md`) is player-slot binary (`FORGESAVE`); it has **no** world blob today. Discovery that dies on process exit is not this slice’s done state.
- `TILE_SIZE = 32`. Production world 200×200 tiles.

Architecture notes:

- Discovery: bit grid on `WorldData` keyed with the world (chunk size 8 tiles → 25×25 bits for 200×200, packed bytes, no per-frame allocation). Update from player tile position on the main thread (mark the player’s chunk and 8-neighbors explored). Clear with the world on `unloadWorldLocked`.
- Persist discovery in this slice: extend `SaveGameManager` with `worldId` + packed discovery bytes (and restore them on load into the matching `WorldData`). If the loaded worldId does not match, start unexplored.
- Widgets via `UIManager` primitives (panel + GPU vertices through the existing UI path). Ids under a `hud_minimap_*` prefix. `cpp-design-specialist` picks `HudController` vs a sibling UI controller; pause/resume must hide the minimap with one visibility call, not a new id list on `GamePlayState`.
- Markers: player from the controller’s player handle; settlements from Slice 2 records (downsampled, not all EDM NPCs). Faction-colored dots only if Slice 4 stance/faction is present; otherwise a single settlement color.
- Files: `include/world/WorldData.hpp` (discovery), `include/managers/SaveGameManager.hpp/.cpp`, `include/controllers/ui/HudController.hpp/.cpp` (or new `include/controllers/ui/` controller), `src/gameStates/GamePlayState.cpp` only for layout/init of the chosen controller, `docs/controllers/HudController.md` or the new controller doc, `docs/ui/`, `tests/controllers/HudControllerTests.cpp` (or the new controller tests), save tests.
- Out of scope: implementing `Minimap_Implementation.md`’s `MinimapWidget` class inside `UIManager`.

Checklist:

- [ ] `cpp-design-specialist` picks HudController vs sibling controller
- [ ] Discovery bit grid on `WorldData`; update from player tile; clear on unload
- [ ] Save/load discovery with `SaveGameManager` keyed by `worldId`
- [ ] Minimap widgets + player marker + settlement dots; pause/resume via one `setVisible`
- [ ] Owning docs updated
- [ ] Tests updated in the same change (discovery mark, visibility, save/load restore)

Acceptance checks:

- [ ] GamePlayState minimap tracks player motion
- [ ] Explored vs unexplored differs; reload of the same save restores discovery
- [ ] Action HUD pause still hides the minimap
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test for the HUD/minimap controller and save coverage
- [ ] Slice reviewed (`cpp-review-specialist`) before commit
- [ ] Interactive visual confirmation in `VoidLight_Template`

Status: Not started. Data depends on Slice 2; faction-colored dots depend on Slice 4. Scheduled after Slice 6.

## Slice 8: Background-tier simulation

Goal: `BackgroundSimulationManager` at 10 Hz advances patrol waypoint progress and need decay for Background-tier NPCs so returning to an area is not velocity-only freeze. No collision, pathfinding floods, or full `execute*` on that tier.

Current foundation:

- `BackgroundSimulationManager::simulateNPC` integrates `position += velocity * dt` with 0.98 velocity decay at 10 Hz (`src/managers/BackgroundSimulationManager.cpp`). `processBatch` already filters by kind/alive. WorkerBudget already applies. `prepareForStateTransition()` exists.
- Patrol persistent waypoints live in `PatrolStateData` (`patrolTargets[4]`, `currentPatrolIndex`, `patrolMoveTimer`) — not the EDM nav waypoint slot, which pathfinder overwrites (`include/ai/BehaviorStateData.hpp`).
- Slice 2 population + default Active then BSM retier. Slice 5 need sidecar.
- Hibernated tier is data-only (no updates). Keep that.

Architecture notes:

- Extend `simulateNPC` (not a new function name unless design requires it) to: (1) if the NPC’s current or home behavior is Patrol and `PatrolStateData` is present, advance `patrolMoveTimer` at 10 Hz and, when dwell expires, wrap `currentPatrolIndex` and set position/velocity toward `patrolTargets[index]` without collision or pathfinder; (2) decay Slice 5 need on the sidecar. Do not call Guard/Attack/Forage executors.
- Clamp the interpolated position to world bounds already cached by other managers; do not query `CollisionManager`.
- Files: `include/managers/BackgroundSimulationManager.hpp`, `src/managers/BackgroundSimulationManager.cpp`, `docs/managers/BackgroundSimulationManager.md`, `tests/managers/BackgroundSimulationManagerTests.cpp`.
- Out of scope: Hibernated-tier AI, a second simulation manager, stance pulses, spatial-hash rebuilds.

Checklist:

- [ ] Background tick: patrol progress (`PatrolStateData`) + need decay
- [ ] No collision/pathfinding/full executors on Background
- [ ] Unload / `prepareForStateTransition()` still clears
- [ ] Owning docs updated
- [ ] Tests updated in the same change (off-screen patrol index/need advance vs velocity-only; unload)

Acceptance checks:

- [ ] An NPC that leaves Active, spends time in Background, and returns has advanced patrol index and/or need versus velocity-only
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test: `background_simulation_manager_tests`
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slices 2 and 5. Scheduled after Slice 7.
