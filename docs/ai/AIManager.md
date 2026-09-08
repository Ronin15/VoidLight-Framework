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
  ranged attacks, and melee fallback equipment swaps

### Update Pipeline

1. gather active EDM indices into `m_activeIndicesBuffer`
2. cache per-frame player position, world bounds, and game time
3. **pre-batch main-thread commit** of the command bus (faction, melee fallback, queued transitions, messages) — required so workers see this frame's assignments
4. ask `WorkerBudgetManager` for a batch strategy against the full active workload
5. run each contiguous batch through `processBatch(...)`
6. in the per-entity fused loop, apply emotional **decay** (`edm.updateEmotionalDecay`), switch on `BehaviorConfigRef::type`, call the typed executor, process movement, and consume knockback sidecar state. There is no emotional contagion pre-pass.
7. flush deferred `EventManager::DeferredEvent` batches
8. **post-batch** main-thread commit of command-bus outputs (ranged spawns, equipment, transitions, messages)

`BehaviorContext` pre-fetches shared state needed by the typed executors, including the EDM knockback sidecar. Worker threads may read/update their entity's behavior state, but structural behavior changes and sidecar removal are committed on the main thread.

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
- `scanFactionInRadius(...)`
- `scanAlliedInRadius(...)` — factions whose directed stance from the caller is Allied (includes the diagonal)

Prefer EDM indices in behavior code to avoid repeated handle-to-index lookups.

## Faction Stance

`AIManager` owns a directed 16×16 Allied / Neutral / Hostile table. This table is the only engagement authority for Attack, Guard, and help-call scans. It is **not** a player-hostility bitmask, and it does **not** default faction 0 vs 1 to Hostile (that default made warriors agro the player).

Defaults after `resetFactionStances()`:

- `stance[i][i] = Allied`
- every other pair = Neutral

Public APIs (main-thread writes; workers read a copied `BehaviorContext` row or `scanAlliedInRadius`):

- `getStance` / `setStance` / `isHostileTo` / `isAlliedTo`
- `worsenStance` (Allied → Neutral → Hostile)
- `improveStance` (Hostile → Neutral → Allied)
- `resetFactionStances()`

Out-of-range gets return Neutral / false. Out-of-range or diagonal sets/worsen/improve are no-ops so the diagonal stays Allied.

`resetFactionStances()` runs from `init()`, `prepareForStateTransition()`, `clean()`, and `resetBehaviors()`.

A persistent `EventTypeId::Combat` handler (registered in `init()` when `EventManager` is already initialized) writes mutual Hostile after a committed `DamageEvent` with `damage > 0` and different in-range factions. Same-faction hits do not write the table. The handler is not unregistered on state transition; `clean()` removes it.

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
