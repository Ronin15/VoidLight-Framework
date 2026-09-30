# Behavior Modes

This page catalogs the behavior families and the configuration style. Modes are data in EDM-backed config structs, not separate heap-owned behavior instances.

## Behavior Families

- `Idle`
  - modes include stationary, subtle sway, occasional turn, light fidget
- `Wander`
  - broad roaming presets such as small, large, and event-area wandering
  - movement speed uses `envSnapshot.moveSpeedScale`; direction-change interval uses `cautionScale`
- `Chase`
  - pursuit settings for line-of-sight, catch radius, and path refresh
  - `maxChaseRange` is multiplied by `envSnapshot.detectionScale` at the check; chase movement is not scaled
- `Patrol`
  - waypoint or route-driven guard movement
  - movement speed uses `moveSpeedScale`; waypoint dwell uses `cautionScale`
- `Guard`
  - alert, suspicious, and defensive area control
  - player auto-detect and lastAttacker/lastTarget/memory threat classification consult `AIManager` faction stance (`Hostile`), not `faction == 1`
  - help / alarm / all-clear scans use `scanAlliedInRadius` (Allied row, including same faction)
  - `cachedDetectionRange` is mode-only; player detection multiplies it by `envSnapshot.detectionScale` at the check. lastAttacker/lastTarget/memory are not scaled. Guard movement is not scaled.
- `Attack`
  - melee/ranged aggression settings plus target engagement rules
  - auto-acquire and player fallback require directed `Hostile` stance; lastTarget / lastAttacker / explicitTarget stay alive-only (unfiltered by stance)
  - AOE friendly-fire skip is Allied, not raw faction id
  - movement uses `moveSpeedScale`; `tryAcquireTarget` / help-call radius are not scaled
- `Flee`
  - panic/retreat behavior with recovery thresholds
  - distress broadcast filters allies by Allied stance rather than `faction != myFaction`
  - movement uses `moveSpeedScale`; `safeDistance` and the 1.2× exit radius use `cautionScale`
- `Follow`
  - formation or distance-preserving follower behavior
- `Forage`
  - need-driven harvesting for civilian NPCs (see [Survival Need and Forage](#survival-need-and-forage))
  - phases: `Searching` → `Moving` → `Harvesting` → `AwaitingCommit`
  - preamble matches Patrol: messages, fear, recent-attack handling, then `tryEngageHostileInRange`; no forage-specific Chase/Flee scarcity reaction
  - movement uses `moveSpeedScale`
  - no preset and no `classes.json` `suggestedBehavior`; reached only through need or `assignBehavior(handle, "Forage")`

## Assignment Pattern

```cpp
VoidLight::BehaviorConfigData config{};
config.type = BehaviorType::Idle;
config.idle = VoidLight::IdleBehaviorConfig::createSubtleSway();

AIManager::Instance().assignBehavior(handle, config);
```

`assignBehavior(...)` moves the selected variant into EDM's dense config/state pools and stores a compact `BehaviorConfigRef` on the entity. Executors receive the typed config and typed state from those pools during `AIManager::processBatch()`.

Or use a registered behavior name:

```cpp
AIManager::Instance().assignBehavior(handle, "Guard");
```

## Survival Need and Forage

EDM stores `NpcNeedData` in a `SparseSidecar` (pressure, retry cooldown, fail count, return behavior). All policy is in `Behaviors::` (`include/ai/BehaviorExecutors.hpp`):

| Constant | Value | Meaning |
| --- | --- | --- |
| `NEED_PRESSURE_PER_SECOND` | `1/180` | pressure growth per second (clamped to 1) |
| `FORAGE_ENTER_THRESHOLD` | `0.7` | pressure at which Idle/Wander consider Forage |
| `FORAGE_RETRY_COOLDOWN` | `15 s` | base retry cooldown after a failed attempt |
| `FORAGE_MAX_BACKOFF_SHIFT` | `4` | backoff cap (`15 s << 4` = 240 s) |
| `FORAGE_STALL_REACH` | `1.5 × HARVEST_RANGE` | stalled-arrival tolerance and commit reach |
| `FORAGE_MAX_FAILED_ATTEMPTS` | `3` | rejected commits / far stalls per Forage episode before giving up |
| `NEED_ENTRY_STAGGER_SECONDS` | `30 s` | max growth a new entry is seeded with (per-entity, deterministic) |

- **Who has need:** `AIManager::syncNeedForRole` runs on the main thread at both `assignBehavior` overloads. A humanoid NPC (`CreatureCategory::NPC`) assigned Idle or Wander **by base name with the default config** (including `classes.json` `suggestedBehavior`) gets an entry; `initForage` also ensures one. Every other assignment removes the entry: animals, monsters, Guard/Patrol/Chase roles, named presets (`SmallWander`, `LargeWander`, `EventWander`), and explicit `BehaviorConfigData` configs. Forage returns through `switchBehavior()`, which rebuilds the default config, so only default-config roles may forage; assigned presets and authored configs are never lost. AIDemo's base-`Wander` Villagers do forage (production workload); its preset hotkeys (4-6) disable need for the reassigned NPCs.
- **Entry stagger:** a new entry is seeded with `NEED_PRESSURE_PER_SECOND × NEED_ENTRY_STAGGER_SECONDS × (entityId % 1024) / 1024`, so NPCs created on one frame (world population, AIDemo spawns) reach the threshold spread over 30 s. `initForage` also staggers the first search by `entityId % 200` ms.
- **Growth:** `Behaviors::tickNeed` runs in the fused AI batch loop for entities with an entry. It only grows pressure and counts `retryCooldown` down; it never switches behavior. Background-tier NPCs do not tick need (Slice 9).
- **Entry:** `executeIdle` / `executeWander` call `Behaviors::shouldStartForage(ctx, currentType)` after their combat/fear preamble. Patrol is excluded so authored waypoints survive. When pressure ≥ threshold, no cooldown is active, and the harvestable snapshot has a **candidate** within `SCARCITY_RADIUS` of the NPC, it records `returnBehavior` and enqueues `switchBehavior(Forage)`. Otherwise it applies backoff: `retryCooldown = FORAGE_RETRY_COOLDOWN << min(failCount, 4)`, `++failCount` (saturating). Empty areas therefore cause no Forage/Wander churn.
- **Candidate rule:** a snapshot node is a candidate only if at least `NPC_HARVEST_RESERVE` other snapshot nodes lie within `SCARCITY_RADIUS` of **that node**, the same count `HarvestCommit` enforces at commit. Isolated nodes (for example two nodes 530 px apart) are never targeted, so the entry pre-check and Searching cannot pick a node the commit must reject. `searchRadius` is `static_assert`-tied to `SCARCITY_RADIUS`.
- **Searching:** nearest candidate in `searchRadius` (default `SCARCITY_RADIUS` = 512 px) that is not `lastFailedTarget`, throttled and staggered. If none qualifies, backoff is applied and the NPC switches back to `returnBehavior` (the observable scarcity reaction).
- **Moving / arrival:** Patrol-style path request, cooldown, and stall handling. Arrival is `dist ≤ HarvestCommit::HARVEST_RANGE` (48 px, shared with the player) or stalled within `FORAGE_STALL_REACH`; a stall further out counts as a failed attempt.
- **Harvesting / commit:** after `harvestDuration` the worker enqueues `AICommandBus::enqueueHarvest`. `AIManager::commitQueuedHarvests()` commits on the main thread through `HarvestCommit::commit(..., NPC_HARVEST_RESERVE)`. On success the need resets (`pressure = 0`, `failCount = 0`, `retryCooldown = 0`) and the NPC switches to `returnBehavior`. A rejected commit leaves the NPC in `AwaitingCommit`; the next pass counts a failed attempt.
- **Failure bound:** each rejected commit or far stall increments `ForageStateData::failedAttempts`. Below `FORAGE_MAX_FAILED_ATTEMPTS` the forager marks `lastFailedTarget` and searches again; on the third it applies backoff and switches to `returnBehavior`. Repeated rejections (for example a harvester without an inventory) or alternating unreachable nodes therefore end the episode, and the retry cooldown keeps it out of Forage. A target that disappears from the snapshot (someone else took it) retargets without counting, since each disappearance consumes a node.
- **Area reserve:** NPC commits never leave fewer than `NPC_HARVEST_RESERVE` (1) available harvestables within 512 px. The player passes 0 and may take the last node. There is no respawn until Slice 6.1.
- **Full inventory:** the NPC yield is discarded with a debug log; the node is still depleted.
- **Exit target:** `returnBehavior` (Idle or Wander), not `homeRole`. Home-role restore is Slice 7.

`BehaviorType::Forage = 8` shifted `Custom` to 9 and `COUNT` to 10. Any persisted raw `uint8_t` behavior values from before this change need remapping.

## Messages and Transitions

Behaviors react through queued messages and command-bus transitions instead of direct cross-controller mutation.

Important message IDs:

- `ATTACK_TARGET`
- `RETREAT`
- `RANGED_ATTACK_FAILED`
- `PANIC`
- `CALM_DOWN`
- `DISTRESS`
- `RAISE_ALERT`

Use:

- `Behaviors::queueBehaviorMessage(...)` from the main thread
- `Behaviors::deferBehaviorMessage(...)` from worker-thread code

## Notes

- persistent behavior state must live in EDM
- variant-specific state lives in the matching dense state pool, not in `BehaviorData`
- per-frame locals must not be used for path/state that should survive updates
- behavior switching is `Behaviors::switchBehavior()` (enqueue) then `AIManager::commitQueuedBehaviorTransitions()` (clears, then `Behaviors::init`). Do not call `reassignBehaviorConfig` from gameplay/controllers.
- Attack, Guard, and help-call scans consult the `AIManager` directed NPC-faction stance table for NPC targets; the player target reads the by-value `ctx.hostileTowardPlayer` (standing-derived, filled on the main thread). `Behaviors::isHostileTowardTarget` routes between the two; never look the player up in the stance row (the player has no faction)
- Attack and Chase keep a remembered `lastTarget` only while `Behaviors::shouldKeepCombatTarget` holds: it is the entity's `memoryData.lastAttacker` (retaliation is exempt) or still hostile. Otherwise the target is cleared, so gifts that lift standing out of Hostile de-escalate attackers
- Idle / Wander / Patrol / Forage / Chase (no current target) call `tryEngageHostileInRange` after recent-attack / fear checks; it returns immediately unless the row has a Hostile cell or `ctx.hostileTowardPlayer`
- `Behaviors::getRelationshipLevel` remains per-NPC memory (emotions + interaction memories) and is unchanged by faction stance or player standing scores
- Player standing is read on the main thread only (`AIManager::getPlayerStanding` / `getPlayerRelation`); workers see only `ctx.hostileTowardPlayer`
