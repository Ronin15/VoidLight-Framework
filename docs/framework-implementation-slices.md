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

## Slice 1: Gameplay HUD ownership and vitals cohesion

Goal: `GamePlayState` has one state-scoped owner for the persistent **action HUD** (player vitals, target frame, hotbar, harvest progress). Pause/resume no longer enumerates `HudController` widget ids. Combat HUD construction leaves `UIManager` policy and lives on `HudController`. Demo states may reuse the controller; they do not expand its contract.

Current foundation:

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

## Slice 2: World population (living settlements)

Goal: Loading a world populates it with NPCs from persisted settlement records. Overworld villages get guards, villagers, and a merchant; forest/haunted biomes get sparse hostiles. Unload/reload does not duplicate NPCs. Pause/resume does not respawn. `GamePlayState` does not loop tiles and `createNPC`. HUD, trade, and combat work against these NPCs. The populate API is keyed by `worldId` so a later dungeon world can pass different records without a new census owner.

Current foundation:

- `WorldGenerator` (`src/world/WorldGenerator.cpp`, `WorldSpawnConfig::Buildings`) places village buildings (`VILLAGE_RADIUS = 12`) and discards `villageCenters` after generation.
- `WorldData` (`include/world/WorldData.hpp`) is `worldId` + tile `grid` only.
- `WorldManager::loadWorld` calls `WorldResourceManager::createWorld` / `setActiveWorld` then `initializeWorldResources()` (`src/managers/WorldManager.cpp`).
- NPC create: `EntityDataManager::createNPCWithRaceClass`. Behaviors: `AIManager::assignBehavior(handle, name)`. Merchant primitive: `EventManager::spawnMerchant`.
- `GamePlayState::enter()` currently calls `spawnMerchant("GeneralMerchant", …)` after HUD init. Debug `R` spawns a hostile Warrior.
- Classes in `res/data/classes.json`: Guard, Villager, GeneralMerchant, Warrior.
- Tests: `tests/world/WorldManagerTests.cpp`.

Architecture notes:

- Add `SettlementRecord` to `WorldData.hpp`: `id`, `centerTileX`, `centerTileY`, `radiusTiles`, `biome`, `faction`, `buildingCount`. `WorldData` holds `std::vector<SettlementRecord> settlements`.
- `WorldGenerator` writes each village center into `settlements` (do not discard `villageCenters`). Radius = `VILLAGE_RADIUS`. Faction default 0 for overworld villages.
- Populate runs from `WorldManager::loadWorld` after `initializeWorldResources()`, and clears on `unloadWorld` / `prepareForStateTransition()`. Same `worldId` as WRM. Do not populate from `GamePlayState::enter()`.
- Per settlement, spawn in world/pixel space (`TILE_SIZE = 32`): 1× `GeneralMerchant` via `spawnMerchant`; 2–3× `Human`/`Guard` with `assignBehavior(..., "Patrol")` or `"Guard"` at buildings; 3–5× `Human`/`Villager` with `"Wander"` or `"Idle"` inside radius. Cap total overworld NPCs in the low hundreds.
- Forest and haunted biomes: additional hostiles (`createNPCWithRaceClass`, faction 1, `"Attack"` or `"Wander"`), sparse, not one-per-tile.
- After spawn, set simulation tier from distance to the load/player reference so far settlements are not all Active.
- Remove or fold the `GamePlayState` bootstrap `spawnMerchant` so it does not double with populate. Debug `R` may stay.
- Files: `include/world/WorldData.hpp`, `src/world/WorldGenerator.cpp`, `include/managers/WorldManager.hpp`, `src/managers/WorldManager.cpp`, populate implementation next to world load (design names the type), `src/gameStates/GamePlayState.cpp`, `docs/managers/WorldManager.md`, `docs/gameStates/GamePlayState.md`, `tests/world/WorldManagerTests.cpp` (and a focused populate test file if the WorldManager suite is the wrong owner).
- Out of scope: dungeon GameState, faction wars, forage, decision layer, minimap.

Checklist:

- [ ] `cpp-design-specialist` names the populate type and its `worldId` register/query/clear API (called from `WorldManager::loadWorld` / unload)
- [ ] `SettlementRecord` + `WorldData::settlements`; generator persists village centers
- [ ] Populate after `initializeWorldResources()`; clear on unload / `prepareForStateTransition()`
- [ ] Per-settlement merchant + guards + villagers; sparse forest/haunted hostiles
- [ ] Simulation tiers set at spawn
- [ ] `GamePlayState` bootstrap merchant removed or folded into populate
- [ ] Owning docs updated
- [ ] Tests updated in the same change (populate on load, no duplicate on reload, no respawn on pause/resume, settlement query by point, second `worldId` does not require GamePlayState)

Acceptance checks:

- [ ] Overworld load yields inhabited villages without `GamePlayState` calling `createNPC` in a tile loop
- [ ] Unload + load of the same `worldId` does not duplicate NPCs
- [ ] Pause/resume does not respawn
- [ ] Populate/clear APIs take `worldId` (dungeon records can use them later)
- [ ] Distant NPCs are not all Active-tier
- [ ] Slice 1 HUD / social / combat work against spawned NPCs
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test for the changed code passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started.

## Slice 3: Environment-driven AI

Goal: Guard/Chase/Wander/Patrol/Flee/Attack use a per-frame environment snapshot so night and heavy weather reduce detection and change move-speed/caution. Worker batches do not call `WeatherController` or `GameTimeManager`.

Current foundation:

- `GameTimeManager` dispatches `EventTypeId::Time` and `EventTypeId::Weather`. `WeatherController`, `DayNightController`, and `ParticleManager` consume them. AI does not.
- `AIManager::update()` already caches player position, world bounds, and game time on the main thread before `processBatch()`. Guard uses `cachedDetectionRange` (`src/ai/behaviors/GuardBehavior.cpp`).
- `BehaviorContext` is the batch contract (`include/ai/AIBehavior.hpp` / `BehaviorExecutors.hpp`).
- Slice 2 must be done (NPCs to modify).

Architecture notes:

- Add a compact snapshot on `AIManager` (visibility, detectionScale, moveSpeedScale, cautionScale), filled once per `update()` on the main thread from current time period + weather. Copy/read it from `BehaviorContext` in workers.
- Data: a table of modifiers keyed by time period × weather intensity in `src/ai/` or `BehaviorConfig.hpp` constants — not magic numbers at each call site.
- `AIManager::init()` registers a persistent Time/Weather handler if the snapshot cannot be read from `GameTimeManager` without events. Do not register this from `GamePlayState`.
- Apply `detectionScale` in Guard/Chase perception; apply `moveSpeedScale` / `cautionScale` in Wander/Patrol/Flee/Attack movement. Keep existing behavior types.
- Optional: Wander/Idle prefer building tiles in heavy weather using existing world tile queries already legal on the main-thread cache — only if detection/speed alone is hollow.
- Files: `include/managers/AIManager.hpp`, `src/managers/AIManager.cpp`, `include/ai/AIBehavior.hpp` / `BehaviorExecutors.hpp`, Guard/Chase/Wander/Patrol/Flee/Attack `.cpp`, `docs/ai/AIManager.md`, `docs/ai/BehaviorExecutionPipeline.md`, `tests/BehaviorFunctionalityTest.cpp` and/or `tests/managers/AIManagerEDMIntegrationTests.cpp`.
- Out of scope: HUD weather glyph, seasonal migration, new behavior family unless shelter is required.

Checklist:

- [ ] Environment snapshot cached in `AIManager::update()` before batches; exposed on `BehaviorContext`
- [ ] Modifier table (time × weather)
- [ ] Guard/Chase/Wander/Patrol/Flee/Attack consume the scales
- [ ] Persistent AIManager subscription if needed
- [ ] Owning docs updated
- [ ] Tests updated in the same change (night vs noon detection, storm vs clear speed, workers do not call weather/time singletons)

Acceptance checks:

- [ ] Same NPC detects the player at shorter range at night / in heavy weather than at clear noon
- [ ] Move-speed/caution change under storm modifiers
- [ ] Worker batches do not call `WeatherController` / `GameTimeManager`
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test for the changed code passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slice 2.

## Slice 4: Faction stance and territory

Goal: Attack, help, and flee-to-allies use a 16×16 stance table on `AIManager` instead of `faction == 1`. Settlements from Slice 2 supply default faction and a point-in-radius territory query. Combat death, theft, and gifts can change stance on the main thread. Player faction standing is stored beside, not instead of, `Behaviors::getRelationshipLevel`.

Current foundation:

- `CharacterData.faction`; `AIManager::MAX_FACTIONS = 16`; `m_factionEdmIndices`; `scanFactionInRadius()`; `setFaction()` / `onEntityFactionChanged()`.
- Guard `callForHelp` is same-faction id.
- `SocialController` gifts/theft/alerts; `getRelationshipLevel` is per-NPC memory.
- Collision layers from faction 0/1/2 in `EntityDataManager::applyFactionCollision`.

Architecture notes:

- Stance enum: Allied / Neutral / Hostile. Storage: `std::array<std::array<…>, MAX_FACTIONS>` (or equivalent) on `AIManager`. Main-thread writes after combat commit and SocialController theft/gift.
- `scanFactionInRadius` / help / Attack target filter / Flee ally seek consult stance, not raw `==`.
- Territory: query Slice 2 `settlements` (center + `radiusTiles` × `TILE_SIZE`) for faction at a point. No new spatial hash.
- Player standing: compact per-faction scores on the player’s EDM character/memory sidecar, updated from the same main-thread commits. Keep `getRelationshipLevel(npc, subject)` for individuals.
- Event: reuse combat/social events if they already carry enough; otherwise add `EventTypeId` for stance-changed so the GamePlayState event log can print a line. Do not scrape the table from UI.
- Collision: keep 0/1/2 player-facing layers unless stance vs player requires a mapping update in `applyFactionCollision` — do that in EDM, not a new system.
- Files: `include/managers/AIManager.hpp`, `src/managers/AIManager.cpp`, Guard/Attack/Flee, `src/controllers/social/SocialController.cpp`, `include/managers/EntityDataTypes.hpp` if player standing is EDM, `docs/ai/AIManager.md`, `docs/controllers/SocialController.md`, `tests/BehaviorFunctionalityTest.cpp`, `tests/controllers/SocialControllerTests.cpp`.
- Out of scope: diplomacy UI, minimap colors, scripted wars.

Checklist:

- [ ] Stance table on `AIManager`; help/attack/flee consume it
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
- [ ] Targeted Boost.Test for the changed code passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slice 2.

## Slice 5: Survival and resource AI

Goal: NPCs with resource need path to WRM harvestables, deplete them through the same EDM harvest path as the player, and react to scarcity. Player `HarvestResourceEvent` participates. Workers do not query WRM every entity every frame.

Current foundation:

- `WorldResourceManager::queryHarvestablesInRadius`. `HarvestController` is player-only. `HarvestResourceEvent` / `ResourceChangeEvent` already fire.
- NPC inventories exist on `CharacterData.inventoryIndex`. No need field. No forage behavior (`BehaviorType` has Wander…Idle, no Forage).
- Slice 2 NPCs and Slice 4 stance are the actors.

Architecture notes:

- Add need/pressure to EDM (field on character data or `SparseSidecar`): 0–1, decay per AI tick, threshold to forage. Clear on destroy / `prepareForStateTransition()`.
- Add `BehaviorType::Forage` (or a Wander mode with its own dense config/state pool if design keeps the enum closed — prefer a real type + `executeForage` in `BehaviorExecutors` / `src/ai/behaviors/`). Path to nearest harvestable from a **sampled** WRM query (cooldown or need-crossed; reusable buffer; main-thread snapshot if WRM is not worker-safe).
- Deplete via existing harvestable EDM payload / `HarvestResourceEvent` so world tiles update the same way as player harvest.
- Scarcity: when a radius query returns below a threshold, emit an event; Attack/Wander/Flee may `switchBehavior` using Slice 4 stance.
- Files: `include/managers/EntityDataTypes.hpp`, `src/managers/EntityDataManager.cpp`, `include/ai/BehaviorConfig.hpp`, `BehaviorExecutors.hpp/.cpp`, new forage sources, `include/events/` + `EventTypeId` if scarcity is new, `HarvestController.cpp` only if the player path must emit the same scarcity event, `docs/ai/BehaviorModes.md`, `docs/managers/WorldResourceManager.md`, `tests/ai/` or `BehaviorFunctionalityTest.cpp`, `tests/controllers/HarvestControllerTests.cpp` if player signal changes.
- Out of scope: crafting, shop sim, player economy HUD.

Checklist:

- [ ] EDM need/pressure with slot cleanup
- [ ] Forage executor + config/state; `switchBehavior` to/from it
- [ ] Sampled WRM queries; no per-entity per-frame worker WRM
- [ ] Depletion uses existing harvestable/EDM/event path
- [ ] Scarcity event; player harvest emits it too
- [ ] Owning docs updated
- [ ] Tests updated in the same change (need decay, forage depletes, scarcity on player harvest, no worker WRM spam)

Acceptance checks:

- [ ] Need-driven NPCs walk to harvestables and deplete them
- [ ] Local depletion is observable and changes NPC behavior
- [ ] Player harvesting the same node participates
- [ ] Workers do not WRM-query every entity every frame
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test for the changed code passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slices 2 and 4.

## Slice 6: Autonomous decision layer

Goal: In `AIManager::processBatch`, a staggered selector can `switchBehavior` from scored motives (health, emotion, memory, stance, environment snapshot, need, `PersonalityTraits`) and restore the NPC’s home role when override scores drop. No A↔B flicker on consecutive frames.

Current foundation:

- `Behaviors::switchBehavior` + `AICommandBus`. Idle/Patrol/Guard already contain hardcoded switches (`src/ai/behaviors/*`).
- Slice 2 spawn assignment is the home role. Slices 3–5 supply environment, stance, need.
- `PersonalityTraits` on memory/character data.

Architecture notes:

- Store `homeBehavior` (type + config ref or name) on the NPC when Slice 2 assigns the initial behavior (EDM field or settlement membership).
- Add `Behaviors::selectBehaviorIfNeeded(ctx)` called from the fused loop on a stagger (e.g. `edmIndex % N == frame % N`) with hysteresis/cooldown fields in shared or per-variant state.
- Scores: flee from fear/health, attack from stance+aggression, forage from need, else home role. Personality scales weights.
- Restore home role through `switchBehavior` (or a command-bus restore if commit clears state — set post-commit as `AGENTS.md` already requires).
- Files: `include/ai/BehaviorExecutors.hpp`, `src/ai/BehaviorExecutors.cpp`, `src/managers/AIManager.cpp` fused loop, EDM field for home role, `docs/ai/BehaviorExecutionPipeline.md`, `tests/BehaviorFunctionalityTest.cpp`, `tests/managers/AIManagerEDMIntegrationTests.cpp`.
- Out of scope: GOAP, HTN, behavior trees, new planner types.

Checklist:

- [ ] Home role stored at populate/assign time
- [ ] Selector in fused loop with stagger, hysteresis, cooldown
- [ ] Preemption flee/attack/forage; restore home role
- [ ] PersonalityTraits scale weights
- [ ] Owning docs updated
- [ ] Tests updated in the same change (no flicker, preemption, return-to-role)

Acceptance checks:

- [ ] NPCs leave home role under threat/need and return when the override ends
- [ ] No A↔B flicker on consecutive frames under stable inputs
- [ ] Personality differences are observable in identical setups
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test for the changed code passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slices 2–5.

## Slice 7: Production minimap

Goal: `HudController` (or a sibling UI controller if the action HUD subtree would mix unrelated widgets) draws a gameplay minimap: local area, discovery grid, player marker, settlement dots from Slice 2. Discovery is world data and save/load if `SaveGameManager` already has a world blob. Pause uses existing `setVisible()`.

Current foundation:

- Slice 1: `HudController::initializeActionHUD`, `setVisible`, public `hud_*` ids. Session chrome stays on `GamePlayState`.
- OverlayDemo has demo-only `overlay_demo_minimap_panel`. `docs/ui/Minimap_Implementation.md` is an old widget-in-UIManager plan — do not implement that document’s ownership.
- Slice 2 settlements; Slice 4 faction for optional dot color.

Architecture notes:

- Discovery: bit grid or chunk mask on `WorldData` (or save sidecar keyed by `worldId`), updated from player tile position on the main thread. Do not allocate per frame.
- Widgets via `UIManager` primitives (panel + GPU vertices through the existing UI path). Ids under a `hud_minimap_*` prefix. `setVisible` hides them with the action HUD.
- Markers: player from `HudController`’s player handle; settlements from Slice 2 records (downsampled). Do not iterate all EDM NPCs every frame.
- Files: `include/world/WorldData.hpp` (discovery), `include/controllers/ui/HudController.hpp/.cpp` (or new `include/controllers/ui/` controller), `src/gameStates/GamePlayState.cpp` only if session chrome layout must move, `docs/controllers/HudController.md`, `docs/ui/`, `tests/controllers/HudControllerTests.cpp`, save tests if persistence is in scope.
- Out of scope: implementing `Minimap_Implementation.md`’s `MinimapWidget` class inside `UIManager`.

Checklist:

- [ ] Discovery storage on world/save; update from player position
- [ ] Minimap widgets; pause/resume via `setVisible`
- [ ] Player marker + settlement dots
- [ ] Owning docs updated
- [ ] Tests updated in the same change (discovery, visibility, save/load if in scope)

Acceptance checks:

- [ ] GamePlayState minimap tracks player motion
- [ ] Explored vs unexplored differs; reload restores discovery if save is in scope
- [ ] Action HUD pause still hides the minimap
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test for the changed code passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit
- [ ] Interactive visual confirmation in `VoidLight_Template`

Status: Not started. Depends on Slice 2; faction-colored dots depend on Slice 4.

## Slice 8: Background-tier simulation

Goal: `BackgroundSimulationManager` at 10 Hz advances patrol waypoint progress and need decay for Background-tier NPCs so returning to an area is not velocity-only freeze. No collision, pathfinding floods, or full `execute*` on that tier.

Current foundation:

- `BackgroundSimulationManager` integrates `position += velocity * dt` at 10 Hz (`include/managers/BackgroundSimulationManager.hpp`). WorkerBudget already applies. `prepareForStateTransition()` exists.
- Slice 2 population + tiers; Slice 5 need; patrol state in EDM path/behavior pools.

Architecture notes:

- Extend `processBackgroundEntity` (or the batch equivalent) to tick patrol index/timer and need decay using EDM fields already written by Slices 2 and 5. Do not call Guard/Attack/Forage executors.
- Scarcity/faction table pulses only if they are already global main-thread data (Slice 4/5 events) — do not rebuild spatial hashes on this tier.
- Files: `include/managers/BackgroundSimulationManager.hpp`, `src/managers/BackgroundSimulationManager.cpp`, `docs/managers/BackgroundSimulationManager.md`, existing background-sim tests under `tests/`.
- Out of scope: Hibernated-tier AI, a second simulation manager.

Checklist:

- [ ] Background tick: patrol progress + need decay
- [ ] No collision/pathfinding/full executors on Background
- [ ] Unload / `prepareForStateTransition()` still clears
- [ ] Owning docs updated
- [ ] Tests updated in the same change (off-screen progress, unload)

Acceptance checks:

- [ ] An NPC that leaves Active, spends time in Background, and returns has advanced patrol/need versus velocity-only
- [ ] `ninja -C build` passes
- [ ] Targeted Boost.Test for the changed code passes
- [ ] Slice reviewed (`cpp-review-specialist`) before commit

Status: Not started. Depends on Slices 2, 4, and 5.
