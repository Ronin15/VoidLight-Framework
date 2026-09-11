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
- behavior switching is `Behaviors::switchBehavior()` (enqueue) then `AIManager::commitQueuedBehaviorTransitions()` (clears, then `Behaviors::init`). Do not call `reassignBehaviorConfig` from gameplay/controllers.
- Attack, Guard, and help-call scans consult the `AIManager` directed stance table; Idle / Wander / Patrol / Chase (no current target) call `tryEngageHostileInRange` after recent-attack / fear checks
- `Behaviors::getRelationshipLevel` remains per-NPC memory and is unchanged by faction stance
