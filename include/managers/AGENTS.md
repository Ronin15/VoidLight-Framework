# AGENTS.md - Manager Public Contracts

These instructions apply to manager headers under `include/managers/`. Follow
the loaded parent guidance first; this file adds manager API and data-contract
rules.

## Public API Shape

- Keep manager contracts explicit about ownership, lifetime, and thread context.
  Prefer narrow APIs over generic helper layers or compatibility overloads.
- Headers should expose declarations, compact data structures, and trivial
  accessors. Put non-trivial orchestration, cache maintenance, and policy in
  `.cpp` files.
- Use stable identifiers and indices only where the caller can prove freshness.
  Preserve generation checks for handles that can outlive entity slot reuse.
- Do not expose nullable pointer-return accessors unless the current subsystem
  already uses them as an optional lookup contract.

## EntityDataManager Contracts

- Treat `EntityDataManager` as storage and canonical per-entity state, not a
  gameplay policy layer. AI decisions, controller flow, collision policy, world
  resource indexing, and render ownership stay in their owning systems.
- Keep hot entity data compact and deliberate. Changes to `EntityHotData`,
  dense pools, sidecars, or parallel arrays must preserve alignment, slot reuse,
  cleanup, and batch-access assumptions.
- Cross-frame per-entity state may live in EDM when it is canonical entity data:
  transform, character/item/projectile data, path data, behavior config/state,
  memory records, inventory slots, render metadata, and sparse sidecars.
- Behavior config/state accessors expose EDM storage for AI execution, but
  behavior transitions still flow through `Behaviors::switchBehavior()` and the
  `AIManager` commit path.
- Render data stored here is metadata only. Manager-owned GPU texture objects
  are resolved at render submission, with raw pointers materialized only at the
  final GPU API boundary.

## Header Hygiene

- Prefer `std::span`, `std::string_view`, typed handles, and typed resource
  handles where they fit existing call sites without extra churn.
- Avoid broad includes in manager headers. Use forward declarations when they do
  not weaken type safety or force fragile incomplete-type assumptions.
- Keep comments focused on contracts future contributors could violate:
  ownership, threading, slot reuse, invalidation, and data-flow boundaries.
