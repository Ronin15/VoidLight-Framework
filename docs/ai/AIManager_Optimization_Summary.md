# AIManager Architecture Summary

**Code:** `include/managers/AIManager.hpp`, `src/managers/AIManager.cpp`, `src/ai/behaviors/`

## Overview

`AIManager` is a data-oriented orchestrator over EDM-backed entity state. It
does not own per-entity `AIBehavior` instances, cloned behavior objects, or an
`AIEntityData` mirror. The pipeline keeps hot entity data in
`EntityDataManager`, executes typed behavior functions in batches, and commits
structural outputs on the main thread.

This page intentionally avoids fixed performance claims. Use the benchmark and
profiling scripts for measurements on the active build and machine.

## Model

- active entities are gathered as EDM indices into `m_activeIndicesBuffer`
- behavior configs and variant state live in dense EDM pools addressed by
  `BehaviorConfigRef`
- `BehaviorData` stores only shared cross-behavior fields such as movement
  cache, message slots, and crowd analysis data
- `BehaviorExecutors` contains direct typed functions such as
  `executeIdle(...)`, `executeGuard(...)`, and `executeAttack(...)`
- `WorkerBudget` chooses whether and how to split the active workload
- worker batches emit deferred events and command-bus requests
- `AIManager` drains and commits command-bus results on the main thread

## Hot-Path Shape

```cpp
AIManager::update(dt)
    -> gather active EDM indices
    -> cache player/world/game-time state
    -> choose WorkerBudget batch strategy
    -> processBatch(dt, start, end)
        -> build BehaviorContext
        -> switch on BehaviorConfigRef::type
        -> call typed executor with dense config/state slot
        -> accumulate movement and consume knockback sidecar state
    -> flush deferred EventManager batches
    -> commit AICommandBus outputs
```

`BehaviorContext` pre-fetches the data needed by behavior code so the batch loop
does not repeatedly query singletons or maps.

## Command-Bus Boundary

Worker-thread behavior code must not mutate shared AI ownership structures
directly. It queues intent through `AICommandBus`:

- behavior messages
- behavior transitions
- faction changes
- ranged attack projectile requests
- melee fallback equipment swaps

The main thread drains these queues after behavior batches complete. Commit code
validates handles and EDM indices before applying changes, which prevents
outdated worker output from landing on a recycled entity slot.

## EDM Ownership

EDM owns storage, not behavior policy:

- transforms, hot data, character data, path data, memory data
- dense behavior config/state pools
- `SparseSidecar<KnockbackData>` for transient knockback
- factual combat memory through `recordCombatEvent()`

AI and behavior executors own interpretation:

- behavior selection and transition policy
- emotional decay and combat/social reactions
- movement intent and path refresh decisions
- worker-safe event/message production

## Related Docs

- [AIManager](AIManager.md)
- [Behavior Execution Pipeline](BehaviorExecutionPipeline.md)
- [Behavior Quick Reference](BehaviorQuickReference.md)
- [EntityDataManager](../managers/EntityDataManager.md)
