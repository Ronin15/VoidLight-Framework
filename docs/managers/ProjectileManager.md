# ProjectileManager

## Role

`ProjectileManager` owns **Projectile simulation**:

- Integrates projectile movement (SIMD 4-wide where possible)
- Manages projectile lifetime (timeouts + embedded projectiles)
- Bridges projectile hit sink input to `Damage` via the existing deferred `EventManager` pipeline

Projectiles are regular EDM entities created via `EntityDataManager::createProjectile()` and live in the Active simulation tier.

## Runtime Contract

- Small projectiles are **overlap-driven hit bodies**, not blocking physics bodies.
- `CollisionManager` detects projectile overlaps and delivers `CollisionInfo` directly through the projectile hit sink.
- `CollisionManager::resolve()` skips positional pushback and velocity cancellation when a projectile is involved, so projectile contacts do not block movement.
- `ProjectileManager` applies owner immunity at event-handling time using the stored projectile owner handle.
- Any other health-bearing target can receive projectile damage, regardless of faction.
- Non-health targets (for example world geometry) cause the projectile to embed and stop participating in further hit detection.

## Event Flow (Projectile Hit Sink → Damage)

1. `CollisionManager` detects projectile hits during broadphase/narrowphase.
2. After resolving each collision, `CollisionManager::update()` stamps `CollisionInfo::projectileInvolved` and invokes `m_projectileHitSink` directly (set by `ProjectileManager::init()`).
3. `ProjectileManager::handleProjectileCollision()` receives the `CollisionInfo` directly on the main thread, applies owner immunity, embeds on non-health targets, and converts valid health-target hits into a deferred `DamageEvent` (same combat queue NPC melee/ranged combat uses).

Non-projectile collision traffic remains manager-owned infrastructure for world triggers and obstacle-change event traffic. General non-projectile collision pairs are not forwarded through `EventManager`, and projectile contacts do not use a collision-event object API.

This keeps `CollisionManager` agnostic of higher-layer managers while preserving the non-blocking "small projectile" gameplay rule.

## Threading Model

`ProjectileManager::update(dt)` follows the same adaptive threading pattern as other hot-path managers:

- `WorkerBudget` decides whether to thread (`SystemType::ProjectileSim`)
- Work is split into batches over the active projectile index list
- Each batch uses its own destroy queue, merged on the main thread after futures complete

## Lifecycle

- `init()` requires `EntityDataManager` to already be initialized.
- `prepareForStateTransition()`:
  - waits for any pending batch work to complete
  - destroys all active projectiles
  - clears transient state (destroy queues, futures, buffers) while preserving capacity
  - keeps its collision handler registered (it is a persistent `EventManager` handler)

## Related Docs

- [CollisionManager](CollisionManager.md)
- [EntityDataManager](EntityDataManager.md)
- [EventManager](../events/EventManager.md)
- [WorkerBudget](../core/WorkerBudget.md)
