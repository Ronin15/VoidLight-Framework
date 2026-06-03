# AGENTS.md - Manager Implementations

These instructions apply to manager implementation code under `src/managers/`.
Follow the loaded parent guidance first; this file adds manager-specific
runtime rules.

## Development Stance

- Keep manager code flexible for ongoing systems work. Prefer durable ownership,
  lifecycle, and data-flow rules over feature-by-feature checklists.
- Trace the runtime owner before moving logic between managers, controllers,
  game states, and entity storage.
- Preserve data-oriented hot paths: reserve when sizes are known, reuse buffers,
  avoid per-frame allocations, and avoid repeated map/string lookups in batch
  loops.

## EntityDataManager Ownership

- `EntityDataManager` owns entity storage, slot lifecycle, type-local pools,
  simulation-tier membership, canonical render metadata, path data, behavior
  storage, memory records, inventory data, and sparse per-entity sidecars.
- EDM records facts and state. It does not decide AI behavior, own controller
  flow, manage GPU textures, run collision policy, or replace world/resource
  indexes owned by other managers.
- `recordCombatEvent()` and memory APIs may store combat facts, totals, last
  attacker/target fields, and memory records. Emotion interpretation and
  behavior response belong in AI or behavior execution code.
- When adding per-entity storage, define the cleanup path at the same time:
  creation/default initialization, slot reuse, direct destruction,
  `prepareForStateTransition()`, `clean()`, and any sidecar reset hooks.
- Keep static bodies separate from tiered dynamic entities unless the owning
  runtime path explicitly requires a shared representation.

## Threading and Index Safety

- Managers that schedule, split, or parallelize work must use `WorkerBudget`
  and the existing worker-budget manager path to decide threading, batch size,
  and worker count. Do not add raw entity-count thresholds, local thread-count
  heuristics, or manager-private budgeting policy.
- Managers must schedule parallel work through `ThreadSystem`. Do not create raw
  threads, private worker pools, detached async work, or manager-local task
  schedulers.
- `WorkerBudget` decides the work shape; `ThreadSystem` executes the work.
  Keep those responsibilities separate when adding or changing manager update
  paths.
- Report completed work through the existing `WorkerBudget` path after manager
  work completes, so future frames adapt from real measured cost.
- Futures or scheduled batches must complete before dependent manager state,
  EDM data, events, rendering inputs, or cleanup can observe their results.
- Structural EDM operations are main-thread operations. Worker code should use
  pre-cached indices and non-overlapping batch ranges.
- Index-based hot-data access is valid only inside the batch/window that proved
  the index current. Use handles and generation checks across frames or
  asynchronous boundaries.
- Do not add non-`thread_local` mutable static state to manager hot paths.
- If a manager caches EDM-derived indices, add or preserve invalidation on
  destruction, state transition, world unload, and tier/kind changes.

## Lifecycle and Integration

- Manager cleanup must clear owned caches, reverse lookups, subscriptions,
  sidecars, and reusable buffers without relying on a later deferred event.
- Persistent manager handlers are registered from `init()` and survive state
  transitions. State-owned transient handlers are cleared by transition cleanup.
- Keep controller-facing mutations at the correct owner boundary. Controllers
  may request work through public manager APIs, but must not directly mutate AI
  behavior state or manager-private caches.
- Behavior config reassignment must keep config refs, variant pools, owner
  arrays, state arrays, and stale slot cleanup in sync.

## Verification

- For manager changes, run the most focused direct Boost executable first and
  use `--list_content` before relying on a filtered `--run_test`.
- For EDM changes, include focused coverage for slot reuse, destruction,
  transition cleanup, and the caller-facing behavior that motivated the change.
