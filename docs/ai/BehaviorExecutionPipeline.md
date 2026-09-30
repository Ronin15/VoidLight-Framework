# Behavior Execution Pipeline

## Overview

The behavior system uses a data-oriented pipeline:

1. `AIManager` gathers active EDM indices into `m_activeIndicesBuffer`
2. **pre-batch** main-thread command-bus commit (faction, transitions, messages) so workers see this frame's assignments, then the environment snapshot and the harvestable snapshot (`refreshHarvestableSnapshot()`, rebuilt only when `WorldResourceManager::getHarvestableVersion()` changed)
3. a single `getBatchStrategy` call against the full workload chooses batch count/size
4. each worker batch runs `processBatch` over a contiguous slice of the index buffer
5. inside `processBatch`, one fused loop runs emotional decay + survival need tick (`Behaviors::tickNeed` on the entity's own `NpcNeedData` entry, if present) + behavior dispatch + SIMD movement per entity — a `switch` on the per-entity `BehaviorConfigRef::type` calls the direct typed executor (`Behaviors::executeIdle`, `executeWander`, ...) with the variant's dense config and state pool entries
6. worker code emits command-bus changes and deferred events
7. **post-batch** main-thread commit of remaining command-bus outputs (ranged attacks, then harvests via `commitQueuedHarvests()`, then behavior transitions) and deferred event drain

## BehaviorContext

`BehaviorContext` provides lock-free access to the data a behavior needs for one update:

- transform and hot entity data
- EDM index
- cached player handle, position, and velocity
- pre-fetched `BehaviorData`, `PathData`, `NPCMemoryData`, and `CharacterData`
- cached world bounds
- cached game time
- cached `envSnapshot` (visibility, detectionScale, moveSpeedScale, cautionScale)
- `knockback` and `needs`: required references to EDM's `SparseSidecar<KnockbackData>` and `SparseSidecar<NpcNeedData>`. Workers only mutate their own entity's entry via `get()`; entries are created on the main thread only
- `harvestables`: `HarvestableSnapshotView` over the active world's non-depleted harvestables (entries of position, handle, static index, grouped by 512 px grid cell with per-cell offsets), defaulting to empty

The goal is to avoid repeated singleton lookups and scattered map access during the hot loop. `AIManager::update()` fills `envSnapshot` on the main thread; worker batches read the by-value copy and must not call `GameTimeManager` or `WeatherController`. The harvestable view points at `AIManager`'s snapshot, which is rebuilt only before batches and is read-only while they run; workers never query `WorldResourceManager`.

## Dispatch and Initialization

- The hot path is `Behaviors::executeIdle / executeWander / ...`, called directly from `AIManager::processBatch`'s per-entity switch. Each typed executor receives the per-variant config and state from EDM's dense pools via `BehaviorConfigRef`.
- `Behaviors::init(edmIndex, configData)` initializes EDM-backed state when a behavior is assigned.

Behavior switching must preserve EDM state correctly; see the transition tests in `tests/BehaviorFunctionalityTest.cpp`.

## AICommandBus

`AICommandBus` queues:

- behavior messages
- behavior transitions
- faction changes
- melee fallback equipment swaps
- ranged attack projectile requests
- harvest requests (`enqueueHarvest` / `drainHarvests`) from Forage

This keeps worker-thread logic from mutating shared orchestration state directly.
Commands are drained and committed on the main thread in deterministic sequence
order. Commit code validates the target handle/index before applying structural
changes, so outdated worker output is discarded instead of mutating the wrong
entity.

Ranged attack requests are also command-bus work. Behaviors enqueue the attack
intent from the batch path; `AIManager` commits the projectile spawn on the main
thread. If the request cannot be committed, the attacker receives
`BehaviorMessage::RANGED_ATTACK_FAILED` and can re-evaluate equipment or
positioning on the next behavior pass.

Harvest requests follow the same pattern. `commitQueuedHarvests()` sorts by
(harvestable static index, harvester EDM index), rejects stale harvesters,
harvesters without an inventory, and harvesters beyond `FORAGE_STALL_REACH`,
abandons (without depleting) when `EDM::canAddToInventory` says the largest
yield would not fit, then calls `HarvestCommit::commit` with
`NPC_HARVEST_RESERVE`. Same-frame duplicates on one node are rejected by the
depleted check, so the lowest harvester index wins; the losing forager
retargets.

## Event Emission

Behavior code can produce deferred gameplay events by building `EventManager::DeferredEvent` values and flushing them in one batch. This is the preferred path for combat and alert propagation generated inside worker batches.

## Threading Rules

- main-thread code may queue behavior messages directly
- worker-thread code should defer messages and event batches
- data that must survive frames belongs in EDM, never in temporary locals
