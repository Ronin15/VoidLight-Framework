# EventManager Quick Reference

## Core Calls

```cpp
auto& eventMgr = EventManager::Instance();

eventMgr.init();
eventMgr.update();
eventMgr.drainAllDeferredEvents();
eventMgr.prepareForStateTransition();
eventMgr.clean();
```

## Handler API

```cpp
uint64_t registerHandlerWithToken(EventTypeId, FastEventHandler);
void registerHandler(EventTypeId, FastEventHandler);
void removeHandler(EventTypeId, uint64_t token);
```

Use tokens for GameStates, controllers, and any subscription with explicit teardown.

## Deferred Queue API

```cpp
struct EventManager::DeferredEvent {
    EventTypeId typeId;
    EventData data;
};

void enqueueBatch(std::vector<DeferredEvent>&& events);
void update();
void drainAllDeferredEvents();
```

Runtime notes:

- combat deferred events are internally queued separately from other event types
- enqueue order is preserved across both queues with sequence numbers
- overflow drops the oldest pending events first
- oversized incoming batches keep only the newest tail that fits

## EventTypeId

```cpp
Weather, SceneChange, NPCSpawn, ParticleEffect,
ResourceChange, World, Camera, Harvest, Collision,
WorldTrigger, CollisionObstacleChanged, Custom,
Time, Combat, Entity, BehaviorMessage, MerchantSpawn
```

## Current Usage Rules

- Do not use removed APIs such as `registerEvent`, `createSceneChangeEvent`, `getEventsByType`, or compaction helpers.
- Use deferred dispatch for worker-thread producers and cross-system frame coordination.
- Use immediate dispatch only when the caller owns timing and thread-safety.
- `EventTypeId::Combat` / `DamageEvent` applies damage results inside `EventManager` before subscribed handlers run.
- `EventTypeId::Collision` is reserved and has no `CollisionEvent` payload.
- Use `CollisionObstacleChanged` for world obstacle changes and the projectile
  hit sink for projectile collisions.
- Use `EventManager::spawnMerchant(...)` for merchant-focused NPC spawning; it dispatches `EventTypeId::MerchantSpawn`.
- Use `drainAllDeferredEvents()` only in tests or controlled synchronization points.
