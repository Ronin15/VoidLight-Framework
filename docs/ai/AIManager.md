# AIManager

**Code:** `include/managers/AIManager.hpp`, `src/managers/AIManager.cpp`

## Overview

`AIManager` is a data processor and orchestrator. It does not own per-entity polymorphic behavior objects. Behavior logic lives in free functions under `Behaviors::`, while persistent state and configuration live in `EntityDataManager` (EDM).

Responsibilities:

- register known behavior names
- assign or remove behavior configs on entities
- build `BehaviorContext` for batch execution
- choose single-threaded vs multi-threaded execution through `WorkerBudget`
- collect deferred events from worker batches
- apply command-bus results and faction changes on the main thread
- maintain fast spatial scans for active entities, factions, and guards

## Architecture

### Source of Truth

- `EntityDataManager`: transform, hot data, behavior config/state, path data, NPC memory, character data
- `AIManager`: orchestration, batching, scans, player cache, update sequencing
- `BehaviorExecutors`: per-behavior execute/init functions
- `AICommandBus`: queued behavior messages, transitions, faction changes,
  ranged attacks, melee fallback equipment swaps, and Forage harvest requests
- `HarvestCommit` (`include/world/HarvestCommit.hpp`): shared main-thread
  depletion path used by `commitQueuedHarvests()` and the player's
  `HarvestController`

### Update Pipeline

1. gather active EDM indices into `m_activeIndicesBuffer`
2. cache per-frame player position, world bounds, game time, and `EnvironmentSnapshot`; then `refreshHarvestableSnapshot()` (see [Harvestable Snapshot and Survival Need](#harvestable-snapshot-and-survival-need))
3. **pre-batch main-thread commit** of the command bus (faction, melee fallback, queued transitions, messages) — required so workers see this frame's assignments
4. ask `WorkerBudgetManager` for a batch strategy against the full active workload
5. run each contiguous batch through `processBatch(...)`
6. in the per-entity fused loop, apply emotional **decay** (`edm.updateEmotionalDecay`), tick survival need (`Behaviors::tickNeed`) for entities with an `NpcNeedData` entry, switch on `BehaviorConfigRef::type`, call the typed executor, process movement, and consume knockback sidecar state. There is no emotional contagion pre-pass.
7. flush deferred `EventManager::DeferredEvent` batches
8. **post-batch** main-thread commit of command-bus outputs (ranged spawns, equipment, `commitQueuedHarvests()`, transitions, messages). Harvests commit before transitions so a successful forager's switch back to its return behavior lands in the same frame

`BehaviorContext` pre-fetches shared state needed by the typed executors, including the EDM knockback sidecar and a by-value `envSnapshot`. Worker threads may read/update their entity's behavior state, but structural behavior changes and sidecar removal are committed on the main thread. Workers must not call `GameTimeManager`, `WeatherController`, or `AIManager::getEnvironmentSnapshot()`.

## Behavior Assignment

Register built-in names once:

```cpp
AIManager::Instance().registerDefaultBehaviors();
```

Assign by name:

```cpp
aiMgr.assignBehavior(handle, "Guard");
```

Assign by full config:

```cpp
VoidLight::BehaviorConfigData cfg{};
cfg.type = BehaviorType::Attack;
aiMgr.assignBehavior(handle, cfg);
```

Do not use a `clone()`-based behavior instance model.
`assignBehavior(...)` stores the config in EDM's per-variant dense config pool and creates the matching per-variant state slot. Runtime dispatch uses `BehaviorConfigRef`, not a virtual behavior object.

## Spatial Queries

Branch-local helper APIs:

- `scanActiveHandlesInRadius(...)`
- `scanActiveIndicesInRadius(...)`
- `scanGuardsInRadius(...)`
- `scanAlliedInRadius(...)` — factions whose directed stance from the caller is Allied (includes the diagonal)
- `scanHostileInRadius(...)` — factions whose directed stance from the caller is Hostile; returns empty immediately when `factionRowHasHostile()` is false, so cost is proportional to Hostile-faction membership, not all active entities. Used by `tryEngageHostileInRange` and Attack `tryAcquireTarget`

Both faction scans read the incrementally maintained faction index (AI-registered, alive entities; not tier-filtered) and are safe for worker reads during batches.

Prefer EDM indices in behavior code to avoid repeated handle-to-index lookups.

## Faction Stance

`AIManager` owns a directed 16×16 Allied / Neutral / Hostile table between **NPC factions only**. This table is the engagement authority for NPC-vs-NPC targeting in Attack, Guard, and help-call scans. It is **not** a player-hostility bitmask, it does **not** default faction 0 vs 1 to Hostile (that default made warriors agro the player), and player actions never write it. `CharacterData.faction` is an NPC faction id; the player has none (`CharacterData::NO_FACTION`, see [Player Relations](#player-relations)). NPC `Layer_Enemy` collision grouping follows the faction's relation toward the player (`syncNpcCollisionTowardPlayer` / `syncFactionCollisionTowardPlayer`), not the stance table. EDM stores the layers via `setNpcCollisionAsEnemy`; it does not consult faction id and does not call AIManager.

Defaults after `resetFactionStances()`:

- `stance[i][i] = Allied`
- every other pair = Neutral

Public APIs (main-thread writes; workers bind a const-ref `BehaviorContext` row plus a per-faction hostile flag, or `scanAlliedInRadius` / `scanHostileInRadius`):

- `getStance` / `setStance` / `isHostileTo` / `isAlliedTo`
- `worsenStance` (Allied → Neutral → Hostile)
- `resetFactionStances()`
- `factionRowHasHostile()` — out-of-range returns false

Out-of-range gets return Neutral / false. Out-of-range or diagonal sets/worsen are no-ops so the diagonal stays Allied.

`resetFactionStances()` runs from `init()`, `prepareForStateTransition()`, `clean()`, and `resetBehaviors()`.

A persistent `EventTypeId::Combat` handler (registered in `init()` when `EventManager` is already initialized) runs after the committed `DamageEvent` (`damage > 0`):

- Player attacker → `recordPlayerIncident(wasLethal() ? Kill : Assault, player, victim)`. No stance write.
- Player victim → no stance or standing change (the NPC's retaliation comes from `memoryData.lastAttacker`).
- NPC vs NPC with different in-range factions → both directions Hostile, with the territory settlement at the victim. Same-faction hits write nothing.

The handler is not unregistered on state transition; `clean()` removes it.

Real cell mutations dispatch `EventTypeId::StanceChanged` immediately (`StanceChangedEvent`: from, toward, old, new, settlementId, `isTowardPlayer() == false`). `setStance` / `worsenStance` report settlement 0; the combat handler reports the settlement containing the victim. No-ops (already equal, out of range, diagonal) and `resetFactionStances()` emit nothing. `GamePlayState` owns the transient event-log handler; AIManager does not log.

## Territory Query

Main-thread only, same contract as `getEnvironmentSnapshot()`. Consumes Slice 2 `WorldManager::findSettlementAtPixel` / `findSettlementAtTile` (first match wins). Wilderness is not a faction. Do not put territory on `BehaviorContext`. Do not cache settlements every frame. Workers must not call WorldManager.

```cpp
struct TerritoryQueryResult {
    uint32_t settlementId{0}; // SettlementRecord.id, 1-based
    uint8_t faction{0};
};
std::optional<TerritoryQueryResult> queryTerritoryAtPixel(float worldX, float worldY) const;
std::optional<TerritoryQueryResult> queryTerritoryAtTile(int tileX, int tileY) const;
```

## Player Relations

The player is handled separately from NPC factions. `EDM::registerPlayer` sets `CharacterData::faction = CharacterData::NO_FACTION` (0xFF) and `EDM::setFaction` rejects `EntityKind::Player`. Per-faction player standing in the EDM `SparseSidecar<PlayerFactionStanding>` (keyed by the player slot; NPCMemoryData stays 448 B) is the single source of truth. `Behaviors::getRelationshipLevel` remains emotions + interaction memories; standing is not mixed into it.

```cpp
enum class PlayerIncident : uint8_t { Assault, Kill, Theft, Gift };
void recordPlayerIncident(PlayerIncident incident, EntityHandle player, EntityHandle npc);
void adjustPlayerStanding(EntityHandle playerHandle, uint8_t towardFaction, int8_t delta);
int8_t getPlayerStanding(EntityHandle playerHandle, uint8_t faction) const;
FactionStance getPlayerRelation(uint8_t faction) const; // for the current player handle
```

All writes are main-thread only. Deltas: Assault `-10` (only while the relation is not already Hostile), Kill `-30` (always), Theft `-25`, Gift `+15`. Standing is clamped to `[-100, 100]`; the relation is derived: `<= -50` Hostile, `>= +50` Allied, else Neutral. `recordPlayerIncident` is a no-op for an invalid or non-`EntityKind::Player` player, a non-`EntityKind::NPC` target, or a faction `>= MAX_FACTIONS`, and takes the settlement id from `queryTerritoryAtPixel` at the NPC. `adjustPlayerStanding` (tests, debug R) uses the same path with settlement 0.

When a delta changes the derived relation, AIManager resyncs that faction's collision (current player handle only) and emits `StanceChangedEvent` with `isTowardPlayer() == true`, `fromFaction` = the NPC faction, `towardFaction = NO_FACTION`, old/new relation, and the incident settlement. `setPlayerHandle` resyncs all 16 factions after releasing its lock.

Producers: the combat handler (Assault/Kill), `SocialController::reportTheft` (player thief → Theft), and `SocialController::recordGift` (Gift). None writes the stance table.

Workers never read standing. `update()` rebuilds `m_playerHostileByFaction` from the sidecar at the cached player slot before batches; `processBatch` sets the by-value `BehaviorContext::hostileTowardPlayer` (player valid, faction in range, and Hostile). `prepareForStateTransition()` and `clean()` clear that array; `resetFactionStances()` does not touch standing; the sidecar dies with the player slot (destroy, reuse, `prepareForStateTransition`, `clean`). A new player starts at 0.

## Environment Snapshot

`AIManager` owns `m_environmentSnapshot` (`visibility`, `detectionScale`, `moveSpeedScale`, `cautionScale`). `update()` fills it on the main thread from `hourToTimePeriod(GameTimeManager::getGameHour())` and the last `EventTypeId::Weather` payload, then copies it by value into `processBatch` (same as `gameTime`). Public `getEnvironmentSnapshot()` is main-thread only; there is no setter.

A persistent `EventTypeId::Weather` handler (also registered in `init()`, not from `GamePlayState`) stores last weather type, intensity, and visibility. Intensity is stored and ignored by combine. Default until the first event is Clear / intensity 1 / visibility 1. `prepareForStateTransition()` and `clean()` reset cached weather and the snapshot to identity. The handler stays registered across transitions and is removed only in `clean()`.

Combine is `timeScale * weatherScale`, each output clamped to `[0.25, 1.5]`, then `detectionScale *= clamp(visibility, 0, 1)` with no second clamp. `Custom` weather uses Clear scales. Tables live in `src/ai/EnvironmentModifiers.cpp`.

## Harvestable Snapshot and Survival Need

Forage workers never query `WorldResourceManager`. `AIManager` owns a
main-thread snapshot of the active world's harvestables:

- `refreshHarvestableSnapshot()` runs in `update()` after the environment
  snapshot. It reads `WorldResourceManager::getHarvestableVersion()` **before**
  copying; if the version equals the cached one it returns with no WRM traffic.
  Otherwise it copies the active world's harvestable indices into a reusable
  scratch buffer, sorts them, and gathers live, non-depleted EDM harvestables
  (`HarvestableSnapshotEntry{position, handle, staticIndex}`).
  `bucketHarvestableSnapshot()` then counting-sorts them into
  `m_harvestableSnapshot` by 512 px grid cell (`HarvestableSnapshotView::CELL_SIZE`
  = `SCARCITY_RADIUS`), ascending static index inside each cell, with
  `m_harvestableCellStarts` holding `cols × rows + 1` offsets. All buffers are
  reused members; a rebuild costs O(harvestables + cells) and happens only on a
  version change.
- The snapshot is passed to `processBatch` as a `HarvestableSnapshotView`
  (entries, cell offsets, origin, dims) and is read-only while batches run.
  Forage scans touch only the cells within 512 px (at most 3×3), not the whole
  world. `getHarvestableSnapshot()` and `getHarvestableSnapshotRebuildCount()`
  are main-thread diagnostics.
- `syncNeedForRole(edmIndex, type, defaultConfig)` runs at both
  role-assignment sites. It ensures an EDM `NpcNeedData` entry only for
  `EntityKind::NPC` with `CreatureCategory::NPC`, an Idle or Wander role, and
  the default config (the by-name, non-preset path); a new entry is seeded via
  `Behaviors::seedNeed` with a per-entity pressure stagger. Any other
  assignment (preset, explicit config, non-civilian role) removes the entry;
  Forage assignments leave it to `initForage`.
- `commitQueuedHarvests()` drains `AICommandBus` harvest requests into the
  reusable `m_pendingHarvests`, sorts by (harvestable static index, sequence),
  and rejects stale harvester handles, harvesters without an inventory, stale
  harvestable handles, and harvesters farther than
  `Behaviors::FORAGE_STALL_REACH` from the node. It then calls
  `HarvestCommit::commit(node, harvester, NPC_HARVEST_RESERVE)`. On a yield it
  adds to the NPC inventory and dispatches `ResourceChangeEvent` ("harvested"),
  or discards the yield with a debug log when the inventory is full. It then
  resets the need and switches the NPC to its `returnBehavior`.
- `prepareForStateTransition()` and `clean()` clear the snapshot, scratch, and
  pending harvests and reset the cached version; `AICommandBus::clearAll()`
  clears queued harvests. Commit-time generation checks cover leftovers.

Need growth, the forage threshold, and backoff are `Behaviors::` policy; see
[Behavior Modes](BehaviorModes.md#survival-need-and-forage).

## Combat and Memory Integration

Combat and social reactions rely on EDM-backed memory data plus AI-layer
behavior logic:

- `EventManager` applies core `DamageEvent` results and calls EDM factual
  recording through `EntityDataManager::recordCombatEvent()`
- EDM stores combat totals, last attacker/target bookkeeping, and memory records
- behavior executors consume those records and decide fear, aggression, alert,
  flee, guard, or attack responses
- `AIManager::update()` runs emotional decay and behavior execution in the
  fused batch loop. There is no contagion pre-pass.

Ranged attack behavior queues projectile work through `AICommandBus`. The main
thread validates the attacker handle/index before spawning projectiles; failed
ranged attempts feed `BehaviorMessage::RANGED_ATTACK_FAILED` back to the
attacker so the attack executor can re-evaluate positioning or fallback choices.

See [NPC Memory](NPCMemory.md).

## Threading Model

`AIManager` follows the repository threading pattern:

```cpp
auto decision = WorkerBudgetManager::Instance().shouldUseThreading(SystemType::AI, count);
// execute
WorkerBudgetManager::Instance().reportExecution(SystemType::AI, count,
    decision.shouldThread, batchCount, elapsedMs);
```

Worker threads do not mutate global AI ownership structures directly. They operate on EDM-backed entity data and emit deferred outputs.

## State Transition

Call `prepareForStateTransition()` before tearing down AI-heavy states. This prevents stale work, clears transient orchestration state, and aligns with the expected manager shutdown order.

## Related Docs

- [Behavior Execution Pipeline](BehaviorExecutionPipeline.md)
- [Behavior Modes](BehaviorModes.md)
- [Behavior Quick Reference](BehaviorQuickReference.md)
- [NPC Memory](NPCMemory.md)
