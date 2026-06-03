# Behavior Modes

This page catalogs the behavior families and the configuration style. Modes are data in EDM-backed config structs, not separate heap-owned behavior instances.

## Behavior Families

- `Idle`
  - modes include stationary, subtle sway, occasional turn, light fidget
- `Wander`
  - broad roaming presets such as small, large, and event-area wandering
- `Chase`
  - pursuit settings for line-of-sight, catch radius, and path refresh
- `Patrol`
  - waypoint or route-driven guard movement
- `Guard`
  - alert, suspicious, and defensive area control
- `Attack`
  - melee/ranged aggression settings plus target engagement rules
- `Flee`
  - panic/retreat behavior with recovery thresholds
- `Follow`
  - formation or distance-preserving follower behavior

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
- behavior switching is initialized through typed `Behaviors::init...` helpers after `reassignBehaviorConfig(...)`, not by constructing a new class instance
