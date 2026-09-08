# CollisionManager

**Code:** `include/managers/CollisionManager.hpp`, `src/managers/CollisionManager.cpp`, `include/collisions/CollisionBody.hpp`

## Role

`CollisionManager` is EDM-backed collision detection and response. Positions, half-sizes, and velocity live in `EntityDataManager`. `m_storage` holds **static and trigger** bodies only. Movables have no storage slot; each frame they are built from EDM into `pools.movableAABBs`.

It is **not** a quantity store, not a second position owner, and not an EventManager collision-event hub for gameplay pairs.

## Body types and layers

`include/collisions/CollisionBody.hpp`:

```cpp
enum class BodyType : uint8_t { STATIC, KINEMATIC, DYNAMIC };

enum CollisionLayer : uint32_t {
    Layer_Default     = 1u << 0,
    Layer_Player      = 1u << 1,
    Layer_Enemy       = 1u << 2,
    Layer_Environment = 1u << 3,
    Layer_Projectile  = 1u << 4,
    Layer_Trigger     = 1u << 5,
    Layer_Pet         = 1u << 6,
};
```

There is no `BodyType::TRIGGER`. Triggers are bodies with `isTrigger` / `TriggerTag` / `TriggerType` (EventOnly vs Physical).

## Spatial model

Two hashes, rebuilt when the world changes — not every frame:

| Hash | Contents | Queried by |
|------|----------|------------|
| `m_staticSpatialHash` | World geometry (buildings, obstacles); excludes EventOnly triggers | Static world queries / rebuild |
| `m_eventOnlySpatialHash` | EventOnly triggers (water, lava, portals) | Per-entity trigger queries |

There is **no** dynamic spatial hash. Hot broadphase is sweep-and-prune:

1. Build movable AABBs from EDM Active-tier entities with collision enabled
2. Movable-vs-movable: SIMD sweep-and-prune on that pool
3. Movable-vs-static: SAP against a cached/sorted static AABB list
4. Narrowphase computes contacts
5. EventOnly detection queries the event-only hash separately

Static/trigger `HotData` is cached AABB + `edmIndex` + layer/type flags. Movable pose is read from EDM.

## Projectile hits

Projectile contacts use a manager-owned sink, not a state-owned callback and not `EventTypeId::Collision` payloads:

```cpp
CollisionManager::Instance().setProjectileHitSink(sink);  // ProjectileManager::init()
```

`resolve()` skips positional pushback for projectile contacts. `ProjectileManager` converts valid hits into deferred `DamageEvent`s. Game states must not register collision callbacks.

World-trigger and obstacle-change traffic still uses EventManager. General non-projectile pairs stay on the collision hot path.

## Threading

`update(dt)` asks `WorkerBudgetManager` (`SystemType::Collision`) whether to thread. There are no hardcoded 500/100 body cutoffs.

## Lifecycle

- `init()` registers persistent world handlers (`subscribeWorldEvents()`). Do not re-subscribe on state transition.
- `prepareForStateTransition()` **always clears all collision bodies**. States call this in AI-heavy `exit()`; `GameStateManager` does **not** call it.
- `WorldUnloaded` is dispatched **Immediate**. Bodies are already cleared in `prepareForStateTransition`; the unload handler is not the teardown owner.
- `rebuildStaticFromWorld()` runs from the persistent `WorldLoaded` handler after a new world is ready.

```cpp
auto& cm = CollisionManager::Instance();
BOOST_REQUIRE(cm.init());
cm.setProjectileHitSink(...);           // ProjectileManager, not a game state
cm.prepareForStateTransition();         // state exit, not GSM
cm.clean();
```

Resolution: `update()` runs broadphase → narrowphase → `resolve()`. `resolve()` writes corrected EDM `position` / `velocity` (movable-movable split push, movable-static full push; skips triggers and projectile hits). Movement integration is `AIManager` (then projectiles); CollisionManager does not integrate movement.

## Queries

`overlaps`, `queryArea`, `queryAreaHasStaticOverlap`, `getBodyCenter`, `isDynamic` / `isKinematic` / `isStatic` / `isTrigger`. World helpers: `rebuildStaticFromWorld`, `createStaticObstacleBodies`, `createTriggersForWaterTiles`, `createTriggersForObstacles`.

Triggers are created through `EDM::createTrigger()` (`createTriggerArea` / `createTriggerAreaAt`).

## Related docs

- [ProjectileManager](ProjectileManager.md) — hit sink → `DamageEvent`
- [EntityDataManager](EntityDataManager.md) — position/halfSize source of truth
- [WorkerBudget](../core/WorkerBudget.md)
- [EventManager](../events/EventManager.md) — persistent world handlers, not projectile hits
