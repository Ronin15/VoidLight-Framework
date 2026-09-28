# Behavior Quick Reference

## Model

- configs live in per-variant dense pools on EDM, addressed via `BehaviorConfigRef { type, index }`
- state lives in per-variant dense pools on EDM, sharing the same index as the config (`get<Variant>State(ref.index)`)
- `BehaviorData` on EDM holds only shared/cross-behavior fields (no tagged union)
- the batched hot path iterates `m_activeIndicesBuffer` in a single fused pass (emotional decay + behavior dispatch + SIMD movement), switching on per-entity `ref.type` to call typed executors (`Behaviors::executeWander`, etc.) directly
- transitions/messages are mediated by `AICommandBus`
- the fused pass also ticks survival need (`Behaviors::tickNeed`) for entities with an `NpcNeedData` entry; `executeIdle` / `executeWander` call `Behaviors::shouldStartForage(ctx, type)` after their preamble to enter `Forage`
- `BehaviorContext` carries `needs` (EDM `SparseSidecar<NpcNeedData>&`) and `harvestables` (read-only, grid-bucketed `HarvestableSnapshotView` of AIManager's harvestable snapshot); workers never query `WorldResourceManager`
- Forage harvests go through `AICommandBus::enqueueHarvest` and are committed on the main thread by `AIManager::commitQueuedHarvests()` via `HarvestCommit::commit`

## AIManager Calls

```cpp
registerDefaultBehaviors();
hasBehavior(name);
assignBehavior(handle, name);
assignBehavior(handle, config);
unassignBehavior(handle);
hasBehavior(handle);
```

## Query Helpers

```cpp
scanActiveHandlesInRadius(...)
scanActiveIndicesInRadius(...)
scanGuardsInRadius(...)
scanFactionInRadius(...)
```

## Behavior Messages

```cpp
BehaviorMessage::ATTACK_TARGET
BehaviorMessage::RETREAT
BehaviorMessage::RANGED_ATTACK_FAILED
BehaviorMessage::PANIC
BehaviorMessage::CALM_DOWN
BehaviorMessage::DISTRESS
BehaviorMessage::RAISE_ALERT
```

## Unsupported Patterns

- worker-thread `WorldResourceManager` queries or EDM harvestable depletion (use the snapshot view and `enqueueHarvest`)
- clone-based behavior ownership
- registration flows built around `registerBehavior(...)`
- string broadcast helpers outside `AICommandBus`
