# Behavior Quick Reference

## Model

- configs live in per-variant dense pools on EDM, addressed via `BehaviorConfigRef { type, index }`
- state lives in per-variant dense pools on EDM, sharing the same index as the config (`get<Variant>State(ref.index)`)
- `BehaviorData` on EDM holds only shared/cross-behavior fields (no tagged union)
- the batched hot path iterates `m_activeIndicesBuffer` in a single fused pass (emotional decay + behavior dispatch + SIMD movement), switching on per-entity `ref.type` to call typed executors (`Behaviors::executeWander`, etc.) directly
- transitions/messages are mediated by `AICommandBus`

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

- clone-based behavior ownership
- registration flows built around `registerBehavior(...)`
- string broadcast helpers outside `AICommandBus`
