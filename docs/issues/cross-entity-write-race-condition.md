# Cross-Entity Write Race Condition in AttackBehavior

**Status:** Resolved — verified against current code 2026-07-03
**Priority:** Medium (rare in practice, but architecturally incorrect)
**File:** `src/ai/behaviors/AttackBehavior.cpp` (line numbers in this doc below are stale; `applyDamageToTarget()` is currently ~line 239)

## Problem

During parallel batch processing in `AIManager::processBatch()`, `AttackBehavior::applyDamageToTarget()` directly writes to the **victim's** EDM data from the **attacker's** batch thread. This is a race condition.

## The 3 Problematic Writes

```cpp
// Line 937 - Writing to victim's CharacterData
charData.health = std::max(0.0f, charData.health - damage);

// Line 938 - Writing to victim's TransformData
hotData.transform.velocity = hotData.transform.velocity + scaledKnockback;

// Lines 947-949 - Writing to victim's MemoryData
memData.lastAttacker = attackerHandle;
memData.lastCombatTime = 0.0f;
```

## What is NOT a Race Condition

These calls are already thread-safe and do NOT need to be changed:
- `AIManager::assignBehavior()` - uses internal locking
- `AIManager::broadcastMessage()` - uses lock-free message queue
- `EntityDataManager::destroyEntity()` - queues for deferred destruction

## Minimal Fix

1. Create a simple `DamageRecord` struct with only:
   - `victimIdx` (size_t)
   - `attackerIdx` (size_t)
   - `damage` (float)
   - `knockbackX`, `knockbackY` (float)

2. Add a thread-local damage buffer to `processBatch()`

3. Modify `applyDamageToTarget()` to append to buffer instead of direct writes

4. After sync point (where futures are awaited), apply all deferred damage:
   - Apply health changes (can use SIMD for batches of 4)
   - Apply knockback
   - Update memory

5. Keep flee/broadcast/death logic in `applyDamageToTarget()` - just move it to execute AFTER the deferred writes are applied

## Why This Was Considered "Safe" Before

The original code comment states:
> "Safe because: victim entities in attack range are typically not in the same batch as their attackers (different spatial positions). Concurrent writes to the same memData from different batches would be a race, but this is extremely rare in practice."

This is a known architectural compromise, not a proper solution.

## Files to Modify

1. `include/ai/AIBehavior.hpp` - Add DamageRecord struct
2. `include/managers/AIManager.hpp` - Update processBatch signature if needed
3. `src/managers/AIManager.cpp` - Add damage buffer, apply after sync
4. `src/ai/behaviors/AttackBehavior.cpp` - Use buffer instead of direct writes

## Resolution (verified 2026-07-03)

The race is gone. `AttackBehavior::applyDamageToTarget()` (`src/ai/behaviors/AttackBehavior.cpp:239`) no longer touches EDM at all — it acquires a pooled `DamageEvent`, tags it `EventTypeId::Combat`, and appends it to a `thread_local std::vector<EventManager::DeferredEvent> t_deferredDamageEvents` (line 29). No cross-thread write happens at the point of damage calculation.

The actual fix implemented is a **prepare/commit split**, not quite the doc's original `DamageRecord`-buffer proposal but functionally equivalent and more general (it also applies to `ProjectileManager` and `CombatController` damage sources, not just `AttackBehavior`):

1. Each `AIManager::processBatch()` worker call gets its own per-batch output vector (`m_batchEventBuffers[i]`); `Behaviors::collectDeferredDamageEvents()` drains the thread-local buffer into it at the end of the batch (`AIManager.cpp:1773`).
2. After `future.get()` on all batch futures (the sync barrier, `AIManager.cpp:463-467`), all batches' events are merged into `m_allDamageEvents` on the main thread and handed to `EventManager::Instance().enqueueBatch()` (`AIManager.cpp:499-501`) — this is the "apply after sync point" step the minimal fix called for.
3. `EventManager::prepareCombatBatch()` does a genuinely parallel-safe **read-only** pass (EDM index/damage/knockback lookup into a `PreparedCombatEvent`) across worker threads (`EventManager.cpp:1086-1096`), synced with `future.get()` before any mutation.
4. `EventManager::commitPreparedCombatEvent()` (`EventManager.cpp:879`) performs the actual mutation — `charData.health -=`, `edm.applyKnockback()`, `edm.recordCombatEvent()` (the memory/last-attacker update), and lethal/destroy handling — strictly in a serial loop on the main thread, after the worker sync point.

Checked for other cross-entity writes in the file (AoE damage, flee/broadcast paths): all AoE targets route through the same `applyDamageToTarget()` deferred path (`AttackBehavior.cpp:1000`); no other direct writes to another entity's `hotData`/`charData`/memory data exist in the file.

This also resolves the "Combat special-case... must be re-evaluated against the final contract" item from the [EventManager Remediation Plan](event-manager-remediation-plan.md) — the prepare/commit split is exactly the "parallel deferred dispatch, opt-in per event type, main-thread commit" contract that plan proposes, already implemented for `Combat`.
