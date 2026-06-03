# Behavior Execution Pipeline

## Overview

The behavior system uses a data-oriented pipeline:

1. `AIManager` gathers active EDM indices into `m_activeIndicesBuffer`
2. a single `getBatchStrategy` call against the full workload chooses batch count/size
3. each worker batch runs `processBatch` over a contiguous slice of the index buffer
4. inside `processBatch`, one fused loop runs emotional decay + behavior dispatch + SIMD movement per entity — a `switch` on the per-entity `BehaviorConfigRef::type` calls the direct typed executor (`Behaviors::executeIdle`, `executeWander`, ...) with the variant's dense config and state pool entries
5. worker code emits command-bus changes and deferred events
6. the main thread commits transitions and drains deferred event batches

## BehaviorContext

`BehaviorContext` provides lock-free access to the data a behavior needs for one update:

- transform and hot entity data
- EDM index
- cached player handle, position, and velocity
- pre-fetched `BehaviorData`, `PathData`, `NPCMemoryData`, and `CharacterData`
- cached world bounds
- cached game time

The goal is to avoid repeated singleton lookups and scattered map access during the hot loop.

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

## Event Emission

Behavior code can produce deferred gameplay events by building `EventManager::DeferredEvent` values and flushing them in one batch. This is the preferred path for combat and alert propagation generated inside worker batches.

## Threading Rules

- main-thread code may queue behavior messages directly
- worker-thread code should defer messages and event batches
- data that must survive frames belongs in EDM, never in temporary locals
