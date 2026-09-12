# AGENTS.md — Manager Public Contracts

Subtree rules for `include/managers/`. Root `AGENTS.md` still applies;
this file adds API and data-contract rules for manager headers. On
conflict, this file wins.

## Public API Shape

- Keep manager contracts explicit about ownership, lifetime, and thread
  context. Prefer narrow APIs over generic helper layers.
- Headers should expose declarations, compact data structures, and
  trivial accessors. Put non-trivial orchestration, cache maintenance,
  and policy in `.cpp` files.
- Use stable identifiers and indices only where the caller can prove
  freshness. Preserve generation checks for handles that can outlive
  entity slot reuse.
- `AIManager` owns the per-frame `EnvironmentSnapshot` and a persistent
  `EventTypeId::Weather` handler registered from `init()`. Time of day
  comes from `hourToTimePeriod(GameTimeManager::getGameHour())`. Do not
  register this handler from GamePlayState. Reset cached weather and the
  snapshot on `prepareForStateTransition()` / `clean()`; keep the handler
  across transitions. `getEnvironmentSnapshot()` is main-thread only.
- Territory consume is on `AIManager` (`queryTerritoryAtPixel` /
  `queryTerritoryAtTile`), main-thread only. It calls existing
  `WorldManager::findSettlementAt*` — no new WorldManager API. Workers
  must not call WorldManager. Do not put territory on `BehaviorContext`.
- Player faction standing is an EDM `SparseSidecar<PlayerFactionStanding>`
  (storage only). Policy (`adjustPlayerStanding`, clamp, combat/theft/gift
  deltas) lives on `AIManager`. Do not mix standing scores into
  `Behaviors::getRelationshipLevel`. Do not clear standing from
  `resetFactionStances()`.
- NPC `Layer_Enemy` collision grouping is AIManager policy from directed
  Hostile toward the player faction (`syncNpcCollisionFromStance` /
  `syncFactionCollisionTowardPlayer`). EDM exposes
  `setNpcCollisionAsEnemy` as a storage setter and does not consult
  faction id or call AIManager.
- World populate: settlement queries (`getSettlements`,
  `findSettlementAt*`) are current-world, like `getTileCopyAt`. The
  populate registry is `worldId`-keyed (`isWorldPopulated`,
  `getPopulatedNpcCount`, `clearPopulatedNpcs`). Callers that need those
  NPCs gone without unloading tiles use `clearPopulatedNpcs`, not
  ad-hoc `destroyEntity` on EDM handles.
- Harvest spawn policy lives in `WorldHarvestInit`; NPC spawn policy
  lives in `WorldPopulation` / `spawnNpc`. Settlement merchants/guards/
  villagers spawn with `settlement.faction`; wilderness Warriors keep
  faction override 1. WorldManager stays the
  coordinator (load/unload, registry, settlement queries). Do not dump
  environment/stance/forage/decision, discovery, or background-tick
  policy into WorldManager.
- Do not expose nullable pointer-return accessors unless the current
  subsystem already uses them as an optional lookup contract.

## EntityDataManager Contracts

- Treat `EntityDataManager` as storage and canonical per-entity state,
  not a gameplay policy layer. AI decisions, controller flow, collision
  policy, world resource indexing, and render ownership stay in their
  owning systems.
- Structural EDM has one owner at a time. Gameplay = main thread. Load
  worker = owner only inside LoadingState's exclusive window. Tests that
  call `loadNewWorld` on the test thread are the owner. `create*` and
  `processDestructionQueue` share `m_structuralMutex`. Dynamic
  `destroyEntity` only enqueues. Legal drain callers: GameEngine
  frame-end (skipped when globally paused), public `unloadWorld`,
  `prepareForStateTransition`.
- Keep hot entity data compact and deliberate. Changes to
  `EntityHotData`, dense pools, sidecars, or parallel arrays must
  preserve alignment, slot reuse, cleanup, and batch-access assumptions.
- Cross-frame per-entity state may live in EDM when it is canonical
  entity data: transform, character/item/projectile data, path data,
  behavior config/state, memory records, inventory slots, render
  metadata, and sparse sidecars.
- Behavior config/state accessors expose EDM storage for AI execution,
  but behavior transitions still flow through
  `Behaviors::switchBehavior()` and the `AIManager` commit path.
- Render data stored here is metadata only. Manager-owned GPU texture
  objects are resolved at render submission, with raw pointers
  materialized only at the final GPU API boundary.

## Header Hygiene

- Prefer `std::span`, `std::string_view`, typed handles, and typed
  resource handles where they fit existing call sites without extra
  churn.
- Avoid broad includes in manager headers. Use forward declarations when
  they do not weaken type safety or force fragile incomplete-type
  assumptions.
- Keep comments focused on contracts future contributors could violate:
  ownership, threading, slot reuse, invalidation, and data-flow
  boundaries.
