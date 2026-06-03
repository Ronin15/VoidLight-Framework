# EventManager

**Code:** `include/managers/EventManager.hpp`, `src/managers/EventManager.cpp`

## Overview

`EventManager` is the engine's event processing hub. It no longer owns a registry of named events or a persistent event store. Systems either:

- trigger an event immediately
- enqueue a deferred event for the main-thread drain pass
- register handlers for a specific `EventTypeId`

`EventManager` provides:

- type-indexed handler lookup
- deferred queue draining on the main thread
- batch enqueue for worker-thread producers
- built-in processing for selected engine-level events, especially combat
- token-based handler removal for state-scoped subscriptions
- deterministic queue draining for tests
- sequence-preserved ordering even though combat work is internally split from other deferred events

## Responsibility Boundary

`EventManager` is responsible for:

- handler registration and removal
- immediate dispatch
- deferred dispatch queue processing
- built-in combat result application for `DamageEvent` traffic on `EventTypeId::Combat`
- pooled helpers for frequently emitted event types

`EventManager` is not responsible for:

- storing long-lived named events
- scene-transition registration
- query APIs like `getEventsByType()`
- event compaction or persistent event activation flags

Combat-side emotion logic and behavior-message policy remain outside `EventManager`. Those stay in AI/behavior code; `EventManager` only applies the core damage result and then dispatches handlers.

## Core API

### Lifecycle

```cpp
auto& eventMgr = EventManager::Instance();
eventMgr.init();
eventMgr.update();                  // Drain deferred queue for this frame
eventMgr.drainAllDeferredEvents();  // Test helper
eventMgr.prepareForStateTransition();
eventMgr.clean();
```

### Handler Registration

```cpp
auto token = eventMgr.registerHandlerWithToken(
    EventTypeId::ResourceChange,
    [](const EventData& data) {
        // Inspect data.event here
    });

eventMgr.removeHandler(EventTypeId::ResourceChange, token);
```

Use `registerHandlerWithToken()` for state-owned subscriptions. `ControllerBase` and GameStates should store and remove tokens during teardown.

### Deferred Batch Enqueue

Worker threads should accumulate local events and flush once:

```cpp
std::vector<EventManager::DeferredEvent> deferred;
deferred.push_back({EventTypeId::Combat, combatData});
deferred.push_back({EventTypeId::BehaviorMessage, behaviorMsgData});

EventManager::Instance().enqueueBatch(std::move(deferred));
```

This is the preferred path for AI/combat worker code because it reduces lock acquisitions to one per batch.

## Deferred Queue Behavior

Deferred events are drained on the main thread, but combat events are stored in a separate internal queue so the manager can prepare combat work efficiently. Ordering still follows original enqueue sequence across both queues.

Current queue behavior:

- non-combat and combat deferred events are merged by sequence during drain
- queue overflow drops the oldest pending events first
- if an incoming batch alone exceeds queue capacity, only the newest tail is kept
- pooled events dropped by overflow are released immediately back to their pools

## Dispatch Modes

- `DispatchMode::Immediate`: process now; combat events also apply their built-in damage result before handlers run
- `DispatchMode::Deferred`: enqueue and let `update()` drain later on the main thread

Use deferred mode when the producer is off-thread, when ordering should align with frame-end processing, or when multiple systems may mutate shared state in response.

## Event Types

Current `EventTypeId` values:

```cpp
Weather, SceneChange, NPCSpawn, ParticleEffect,
ResourceChange, World, Camera, Harvest, Collision,
WorldTrigger, CollisionObstacleChanged, Custom,
Time, Combat, Entity, BehaviorMessage, MerchantSpawn
```

Key event types:

- `BehaviorMessage` covers inter-entity AI signaling such as `RAISE_ALERT`
- `MerchantSpawn` covers merchant-focused NPC spawning through `MerchantSpawnEvent`
- `DamageEvent` under `EventTypeId::Combat` is the hot path for gameplay damage
- `Collision` is a reserved legacy ID. There is no `CollisionEvent` payload in
  the current event path.
- `CollisionObstacleChanged` is the supported world-obstacle collision update
  event. Projectile hits use the persistent projectile hit sink.
- theft/social flows emit normal event traffic instead of bespoke controller-only state
- `ResourceChangeEvent` is reused heavily by inventory, harvesting, and UI sync paths

Merchant spawning should use the event helper:

```cpp
EventManager::Instance().spawnMerchant(
    merchantClass, x, y, merchantRace, count, spawnRadius, worldWide);
```

## Combat Processing Path

`EventManager` now performs core damage resolution for `DamageEvent` traffic:

1. read source/target/damage/knockback from the queued event
2. resolve the target EDM index
3. apply health reduction to `CharacterData`
4. apply knockback through EDM `SparseSidecar<KnockbackData>`
5. record NPC combat memory data through `EntityDataManager::recordCombatEvent()`
6. mark lethal results and destroy non-player targets when needed
7. update the `DamageEvent` with remaining health and lethality
8. dispatch subscribed combat handlers after the built-in processing step

Immediate combat events follow the same processing order synchronously. Deferred combat events may use WorkerBudget-guided parallel preparation before their main-thread commit step.

## Common Patterns

### State-scoped subscription

```cpp
void SomeState::registerEventHandlers() {
    auto& eventMgr = EventManager::Instance();
    m_resourceToken = eventMgr.registerHandlerWithToken(
        EventTypeId::ResourceChange,
        [this](const EventData& data) { onResourceChanged(data); });
}

void SomeState::unregisterEventHandlers() {
    auto& eventMgr = EventManager::Instance();
    eventMgr.removeHandler(EventTypeId::ResourceChange, m_resourceToken);
}
```

### AI worker event production

```cpp
// In worker batch code
threadLocalDeferredEvents.clear();
// fill threadLocalDeferredEvents...
EventManager::Instance().enqueueBatch(std::move(threadLocalDeferredEvents));
```

### Test draining

```cpp
eventMgr.triggerResourceChange(..., EventManager::DispatchMode::Deferred);
eventMgr.drainAllDeferredEvents();
```

Use this in tests when assertions depend on deferred handlers having run.

## State Transition Guidance

Before a GameState tears down AI-, world-, or UI-heavy systems:

1. remove state-owned handler tokens
2. call `prepareForStateTransition()`
3. continue manager cleanup in dependency order

This prevents stale handler callbacks from firing during shutdown.

## Related Docs

- [EventManager Quick Reference](EventManager_QuickReference.md)
- [EventManager Advanced](EventManager_Advanced.md)
- [EventFactory](EventFactory.md)
- [AI Execution Pipeline](../ai/BehaviorExecutionPipeline.md)
