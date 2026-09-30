---
paths:
  - "include/managers/**"
  - "src/managers/**"
  - "include/world/**"
  - "src/world/**"
---

# Manager and EDM Contracts

Applies to manager headers and source, EntityDataManager (EDM), and the
world helpers managers coordinate. Root `CLAUDE.md` still applies. AI
behavior rules are in `.claude/rules/ai.md`.

## API and Header Shape

- Manager contracts are explicit about ownership, lifetime, and thread
  context. Prefer narrow APIs over generic helper layers.
- Headers expose declarations, compact data structures, and trivial
  accessors. Orchestration, cache maintenance, and policy go in `.cpp`.
- Use stable identifiers and indices only where the caller can prove
  freshness; preserve generation checks for handles that can outlive slot
  reuse.
- No nullable pointer-return accessors unless the subsystem already uses
  them as an optional-lookup contract.
- Prefer `std::span`, `std::string_view`, typed handles, and typed
  resource handles where they fit existing call sites. Avoid broad
  includes in manager headers; forward-declare where it does not weaken
  type safety.
- Trace the runtime owner before moving logic between managers,
  controllers, game states, and entity storage.

## EntityDataManager

- EDM owns entity storage, slot lifecycle, type-local pools,
  simulation-tier membership, canonical render metadata, path data,
  behavior storage, memory records, inventory data, and sparse
  per-entity sidecars. It records facts; it does not decide AI behavior,
  own controller flow, manage GPU textures, run collision policy, or
  replace world/resource indexes owned by other managers.
- `recordCombatEvent()` and memory APIs store combat facts, totals,
  last attacker/target, and memory records. Emotion interpretation and
  behavior response belong in AI code.
- Factory auto-registration (Slice 3 contract, the only AI policy in
  EDM): `createNPCWithRaceClass` (`classes.json`, fallback `Wander`),
  `createMonster` (`monster_variants.json`, fallback `Chase`), and
  `createAnimal` (`animal_roles.json`, fallback `Wander`) each call
  `AIManager::registerEntity` with the JSON `suggestedBehavior`.
- Expansion rule (`EntityDataTypes.hpp`): every-frame fields go on the
  hot/character line; "some NPCs, sometimes" data goes in a
  `SparseSidecar`. Changes to `EntityHotData`, dense pools, sidecars, or
  parallel arrays preserve alignment, slot reuse, cleanup, and
  batch-access assumptions.
- New per-entity storage defines its cleanup path at the same time:
  default init, slot reuse, direct destruction,
  `prepareForStateTransition()`, `clean()`, and sidecar reset hooks.
- Keep static bodies separate from tiered dynamic entities unless the
  owning runtime path requires a shared representation.
- Render data is metadata only; manager-owned GPU textures are resolved
  at render submission.
- Behavior config reassignment keeps config refs, variant pools, owner
  arrays, state arrays, and stale slot cleanup in sync.

## Structural Ownership and Threading

- Structural EDM has one owner at a time. Gameplay: main thread. Load:
  the load worker only inside LoadingState's exclusive window
  (`GameEngine::setGlobalPause(true)`; EventManager stays unpaused so
  deferred `WorldLoaded` drains). Tests calling `loadNewWorld` on the test
  thread are the owner. `create*` and `processDestructionQueue` share
  `m_structuralMutex`.
- Dynamic `destroyEntity()` only enqueues (static harvestable destroy on
  unload is immediate). `processDestructionQueue()` has three legal
  callers: `GameEngine` frame-end (skipped while globally paused),
  `prepareForStateTransition()`, and public `WorldManager::unloadWorld()`
  (after locks drop). Never drain from `unloadWorldLocked`,
  `clearPopulatedNpcs`, `destroyAllNPCsForStateTransition`, or other
  load-worker code.
- `WorkerBudget` decides work shape (thread or not, batch size, worker
  count); `ThreadSystem` executes it; report completed work through the
  `WorkerBudget` path. No raw entity-count thresholds, local thread
  heuristics, private pools, raw threads, or detached async schedulers.
- Index-based hot-data access is valid only inside the batch/window that
  proved the index current. Use handles and generation checks across
  frames or async boundaries. Managers caching EDM-derived indices
  invalidate them on destruction, state transition, world unload, and
  tier/kind changes.

## Lifecycle and Integration

- Cleanup clears owned caches, reverse lookups, subscriptions, sidecars,
  and reusable buffers without relying on a later deferred event.
- Controllers request work through public manager APIs and never mutate
  AI behavior state or manager-private caches.

## Subsystem Contracts

- **AIManager environment:** owns the per-frame `EnvironmentSnapshot`
  and a persistent `EventTypeId::Weather` handler registered in `init()`
  (never from GamePlayState). Time of day comes from
  `hourToTimePeriod(GameTimeManager::getGameHour())`. Reset cached
  weather and snapshot on `prepareForStateTransition()` / `clean()`; keep
  the handler. `getEnvironmentSnapshot()` is main-thread only.
- **Territory:** `AIManager::queryTerritoryAtPixel` / `AtTile`,
  main-thread only, calling existing `WorldManager::findSettlementAt*`.
  Workers never call WorldManager. Territory is not on `BehaviorContext`.
- **Faction standing:** the player has no faction
  (`CharacterData::NO_FACTION`; `EDM::setFaction` rejects the player).
  EDM `SparseSidecar<PlayerFactionStanding>` is storage and the single
  source of NPC-faction relations toward the player; policy
  (`recordPlayerIncident`, `adjustPlayerStanding`, clamp, derived
  relation) is on `AIManager`, main thread only. Player actions never
  write the NPC-faction stance table. Workers read only
  `BehaviorContext::hostileTowardPlayer`. Not mixed into
  `Behaviors::getRelationshipLevel`; not cleared by
  `resetFactionStances()`.
- **Collision grouping:** NPC `Layer_Enemy` is AIManager policy:
  `getPlayerRelation(npcFaction) == Hostile`
  (`syncNpcCollisionTowardPlayer` / `syncFactionCollisionTowardPlayer`;
  `setPlayerHandle` resyncs all factions). The stance table does not
  drive it. EDM `setNpcCollisionAsEnemy` is a storage setter only.
- **World populate:** settlement queries (`getSettlements`,
  `findSettlementAt*`) are current-world. The populate registry is
  `worldId`-keyed (`isWorldPopulated`, `getPopulatedNpcCount`,
  `clearPopulatedNpcs`); use `clearPopulatedNpcs`, not ad-hoc
  `destroyEntity`, to remove populated NPCs. Harvest spawn policy lives in
  `WorldHarvestInit`; NPC spawn policy in `WorldPopulation` / `spawnNpc`.
  Settlement NPCs use `settlement.faction`; wilderness Warriors keep
  faction override 1.
- **Survival need:** EDM `SparseSidecar<NpcNeedData>` storage; growth,
  threshold, and backoff are `Behaviors::` policy. Entries are seeded on the
  main thread by AIManager `syncNeedForRole` (civilian Idle/Wander
  default-config roles) and by `initForage` when missing; they carry the home
  anchor and leash radius (merchants 384 px, others 0 = unleashed) and are
  removed on reassignment to any other non-Forage role.
- **Harvest commit:** `HarvestCommit::commit` (world layer, main thread) is
  the single depletion path for player and AI; WRM only versions and counts.
  AIManager `commitQueuedHarvests` arbitrates per node by lowest harvester EDM
  index and pre-checks NPC inventory capacity (`EDM::canAddToInventory`); a
  forager that cannot hold the yield abandons without depleting the node.
  Workers read AIManager's harvestable snapshot view, never WRM.
- **WorldManager stays a coordinator** (load/unload, registry,
  settlement queries). Never add environment, stance, forage, decision,
  discovery, or background-tick policy to it.
