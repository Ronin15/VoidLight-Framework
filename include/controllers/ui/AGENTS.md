# AGENTS.md — UI Controller Public Contracts

Subtree rules for `include/controllers/ui/`. Root `AGENTS.md` still
applies; this file adds public API and ownership-contract rules for UI
controller headers. On conflict, this file wins.

## Public API Shape

- Headers should expose explicit controller contracts: ownership, state
  access, component IDs, and the smallest public methods needed by game
  states or other controllers.
- Put non-trivial UI construction, event handling, EDM/resource queries,
  input orchestration, and mutation logic in `.cpp` files.
- Keep APIs state- and owner-aware. Avoid generic UI extension hooks
  unless the existing controller boundary requires them.

## State and Constants

- Public constants should identify stable UI contracts that tests, states,
  or sibling controllers legitimately need, such as panel IDs or slot
  counts.
- Shared UI fonts, sizing, z-order, and layout constants belong in
  `UIConstants.hpp`. Controller-specific geometry can remain private in
  the matching `.cpp`.
- Controller state in headers should describe durable UI flow state, not
  mirror `UIManager` component internals or cached render data.

## Header Hygiene

- Keep includes narrow and prefer forward declarations where they do not
  weaken type safety.
- Keep comments focused on ownership, runtime flow, and public contracts
  that future maintainers could otherwise violate.
- Do not expose helper layers solely to make tests reach private UI
  details; prefer observable behavior through controller APIs and
  `UIManager` state.
