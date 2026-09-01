# EventManager Remediation Plan

**Code:** `include/managers/EventManager.hpp`, `src/managers/EventManager.cpp`, `include/core/GameEngine.hpp`, `src/core/GameEngine.cpp`

**Status:** Resolved — superseded by shipped design, verified against current code 2026-07-03
**Priority:** ~~High~~ (was accurate when written; no longer reflects open work)
**Scope:** Event delivery contract, handler safety model, lifecycle integration

## Goal

Make `EventManager` the authoritative communication hub for cross-manager coordination while preserving the engine's race-to-idle design.

This plan intentionally separates:

- **Hub contract fixes**: when events run, in what order, and on what thread
- **Handler fixes**: making subscribers conform to that contract
- **Performance restoration**: reintroducing parallelism only where it is proven safe

## Current Problems

### 1. Deferred dispatch contract is underspecified

The current implementation mixes three semantics:

- "run later in the event phase"
- "run in priority order"
- "run on worker threads when budget allows"

Handlers were not written consistently against those rules, so correctness now depends on implicit assumptions.

### 2. Blanket threaded dispatch is unsafe

A number of current handlers do more than consume data:

- trigger gameplay mutations
- mutate manager-owned registries
- call audio/particle/world subsystems
- rely on main-thread lifecycle assumptions

Threading all non-combat deferred events by default is therefore too broad.

### 3. Ordering matters for lifecycle events

Some deferred events are not "eventually consistent" notifications. They are part of a required sequence:

- world unload/load
- static collider readiness
- pathfinding invalidation/rebuild
- resource/world registry transitions

Reordering them changes engine behavior.

### 4. State transitions are not consistently routed through EventManager cleanup

Some state exits skip `EventManager::prepareForStateTransition()` or call dependent manager teardown in an order that lets deferred callbacks outlive partially-cleared state.

### 5. Some handlers are architecturally too heavy

Several handlers directly perform side effects that should be mediated more explicitly:

- immediate world mutation
- cross-manager lifecycle work
- controller-layer AI policy

The hub contract needs to make these boundaries explicit.

## Target Contract

### Immediate dispatch

- Runs synchronously in the caller's thread
- Preserves direct call-site semantics
- Used only when inline reaction is actually required

### Deferred dispatch

- Runs during the `EventManager` phase of `GameEngine::update()`
- Is deterministic by default
- Preserves enqueue order within the same dispatch lane unless a specific event type documents stronger ordering rules
- Executes on the main thread by default

### Parallel deferred dispatch

Allowed only for explicitly certified event types.

Certification criteria:

- handler is data-only or otherwise provably thread-safe
- no SDL/main-thread-only APIs
- no manager lifecycle mutations
- no dependency on relative ordering with other deferred events
- no dependence on state-transition teardown ordering

## Fix Phases

### Phase 1: Lock the contract down

1. Restore a correctness-first deferred dispatch path:
   - deterministic ordering
   - no blanket threaded dispatch
   - clear separation between immediate and deferred semantics

2. Re-enable all currently expected delivery paths:
   - type-based handlers
   - any still-supported named-handler delivery, or remove the API if it is no longer a supported pattern

3. Ensure pooled event release still happens after all handler work completes.

### Phase 2: Audit and classify handlers

Create a handler matrix with these buckets:

- `MAIN_THREAD_ONLY`
- `ORDER_SENSITIVE`
- `PARALLEL_SAFE`
- `SHOULD_BE_REWRITTEN`

Initial expected classifications:

- `ParticleEffect`, `Weather`, `Harvest`, `World`, `Camera`, `Time`, `ResourceChange`
  - likely `MAIN_THREAD_ONLY` or `ORDER_SENSITIVE`
- `Combat`
  - special-case already exists; must be re-evaluated against the final contract
- future pure analytics/debug events
  - possible `PARALLEL_SAFE` candidates

### Phase 3: Rewrite unsafe handlers

For each `SHOULD_BE_REWRITTEN` handler:

1. Move heavyweight side effects behind a manager-owned API if needed
2. Keep event handlers as translators/coordinators rather than unstructured work sinks
3. Move policy logic into the correct layer
   - example: controller -> AI behavior boundary

### Phase 4: Fix lifecycle integration

Standardize teardown order in states that own AI/world/event-driven entities:

1. `AIManager`
2. `BackgroundSimulationManager`
3. `WorldResourceManager`
4. `EventManager`
5. `CollisionManager`
6. `PathfinderManager`
7. `EntityDataManager`

State-specific tokens that capture state objects should be removed before or during state teardown, but global event-hub cleanup must still happen in the standard manager order.

### Phase 5: Reintroduce safe threading deliberately

After the audit:

1. Add an explicit per-event-type dispatch policy
2. Default all event types to serial deferred dispatch
3. Opt in only audited-safe event types
4. Keep profiling/WorkerBudget instrumentation to measure actual gains

Potential policy model:

- `ImmediateOnly`
- `DeferredSerial`
- `DeferredSerialOrdered`
- `DeferredParallelSafe`

## Non-Goals

- Rewriting the entire gameplay architecture in one pass
- Converting all direct manager calls to events immediately
- Preserving unsafe parallel dispatch in the name of throughput

## Acceptance Criteria

- Deferred event behavior is deterministic and documented
- No event handler that assumes main-thread/lifecycle safety runs on worker threads unless explicitly certified
- State transitions cannot leave deferred callbacks running against torn-down state
- EventManager is clearly the communication hub, with an execution contract that subscribers can rely on
- Any restored parallelism is explicit, measured, and safe

## Implementation Order

1. EventManager contract fix
2. Handler classification table
3. Unsafe handler rewrites
4. State transition cleanup standardization
5. Opt-in parallel deferred dispatch for certified event types only

## Validation

- Targeted tests for deferred ordering and state transitions
- Regression coverage for world load/unload + collider/pathfinder coordination
- Handler lifecycle tests for manager reinit/cleanup
- Profiling before and after any opt-in parallel dispatch

## Related Docs

- [EventManager](../events/EventManager.md)
- [GameEngine](../core/GameEngine.md)
- [ThreadSystem](../core/ThreadSystem.md)

## Resolution (verified 2026-07-03)

Every "Current Problem" and every item in "Acceptance Criteria" is satisfied by shipped code today — but via a simpler mechanism than this plan proposed, not the literal per-event-type policy enum / handler classification table described below. Those two constructs (`DispatchPolicy` enum, `MAIN_THREAD_ONLY`/`ORDER_SENSITIVE`/`PARALLEL_SAFE`/`SHOULD_BE_REWRITTEN` table) do not exist anywhere in the codebase — grepped across `include/`, `src/`, `docs/` and found zero occurrences outside this doc. They were never needed because the actual design makes *all* handler execution main-thread by construction, so no per-handler certification step exists to formalize.

**Problem-by-problem status:**

1. **Deferred contract underspecified** — Resolved. `DispatchMode` is just `enum class DispatchMode : uint8_t { Deferred = 0, Immediate = 1 }` (`EventManager.hpp:209`). A single drain path, `drainDispatchQueueWithBudget()`, runs only from `EventManager::update()`, called directly from `GameEngine`'s main-thread manager sequence (`GameEngine.cpp:1112-1113`) — not enqueued as worker work.
2. **Blanket threaded dispatch unsafe** — Resolved. No handler ever runs on a worker thread. Non-combat handlers execute inline on the main thread under a `shared_lock` (`EventManager.cpp:1118-1163`). The only `enqueueTaskWithResult` call in the dispatch path is combat *preparation* (read-only EDM lookup), never handler execution.
3. **Ordering matters for lifecycle** — Resolved. Every deferred event carries a sequence number; the drain merges the combat and non-combat queues back into enqueue order before dispatch (`EventManager.cpp:992-1019`), documented in `EventManager_Advanced.md`.
4. **State transitions not consistently routed through cleanup** — Resolved (mechanism verified). `prepareForStateTransition()` (`EventManager.cpp:182-211`) waits all pending batch futures, then `clearTransientHandlers()` → `clearPendingDispatchQueues()` → `clearEventPools()`, in that order — persistent (manager-level) handlers survive, transient (state-level) ones don't. Not independently re-verified: every `GameState::exit()` actually calls this in the right relative order against other manager teardown (see [State Transitions and Events](../../AGENTS.md#state-transitions-and-events) for the documented required order).
5. **Combat special-case needs re-evaluation** — Resolved and safe as implemented. `prepareCombatEvent`/`prepareCombatBatch` run on worker threads but only *read* EDM into a `PreparedCombatEvent` (disjoint per-batch output ranges); all mutation — health, knockback, combat-memory recording, lethal/destroy — happens in `commitPreparedCombatEvent()` on the main thread after the worker sync point (`EventManager.cpp:829-964`). This is the same mechanism verified in [cross-entity-write-race-condition.md](cross-entity-write-race-condition.md).

`docs/events/EventManager.md` and `docs/events/EventManager_Advanced.md` already document this shipped contract and supersede this plan as the authoritative reference — read those for current behavior, not this doc.
