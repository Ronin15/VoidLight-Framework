# AGENTS.md - UI Controller Public Contracts

These instructions apply to UI controller headers under `include/controllers/ui/`.
Follow the loaded parent guidance first; this file adds public API and
ownership-contract rules for this subtree.

## Public API Shape

- Headers should expose explicit controller contracts: ownership, state access,
  component IDs, and the smallest public methods needed by game states or other
  controllers.
- Put non-trivial UI construction, event handling, EDM/resource queries, input
  orchestration, and mutation logic in `.cpp` files.
- Keep APIs state- and owner-aware. Avoid compatibility overloads, nullable
  accessors, or generic UI extension hooks unless the existing controller
  boundary requires them.

## State and Constants

- Public constants should identify stable UI contracts that tests, states, or
  sibling controllers legitimately need, such as panel IDs or slot counts.
- Shared UI fonts, sizing, z-order, and layout constants belong in
  `UIConstants.hpp`. Controller-specific geometry can remain private in the
  matching `.cpp`.
- Controller state in headers should describe durable UI flow state, not mirror
  `UIManager` component internals or cached render data.

## Header Hygiene

- Keep includes narrow and prefer forward declarations where possible without
  weakening type safety.
- Keep comments focused on ownership, runtime flow, and public contracts that
  future maintainers could otherwise violate.
- Do not expose helper layers solely to make tests reach private UI details;
  prefer observable behavior through controller APIs and `UIManager` state.
