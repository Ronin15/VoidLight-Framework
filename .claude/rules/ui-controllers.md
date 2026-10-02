---
paths:
  - "include/controllers/ui/**"
  - "src/controllers/ui/**"
---

# UI Controller Contracts and Implementation

Applies to UI controller headers and source. Root `CLAUDE.md` still
applies (UI layout/helper and render-lifecycle rules live there).

## Ownership and Data Flow

- UI controllers own state-scoped gameplay UI flow: HUD state, inventory
  presentation, drag/drop, visibility, input orchestration, and
  event-driven refresh.
- `UIManager` owns component storage, theme/style policy, layout, hit
  testing, tooltip policy, render batching, and transition cleanup.
  Controllers never duplicate those.
- Gameplay data stays in its owning systems. Controllers read canonical
  state and emit existing events or commands; they are never a second
  source of truth for inventory, equipment, resources, combat, or world
  state.
- Reusable gameplay UI behavior (spanning setup, input, events, refresh)
  belongs in a controller. One-off state labels or simple status text
  stay on the state.

## API and Header Shape

- Headers expose explicit controller contracts: ownership, state access,
  component IDs, and the smallest public methods states or sibling
  controllers need. Construction, event handling, EDM/resource queries,
  input orchestration, and mutation go in `.cpp`.
- Avoid generic UI extension hooks or builder layers unless the existing
  controller boundary requires them. Prefer explicit component IDs and
  small controller-local helpers for repeated ID construction.
- Public constants only for stable UI contracts (panel IDs, slot counts).
  Shared fonts, sizing, z-order, and layout constants go in
  `UIConstants.hpp`; controller-specific geometry stays private in the
  `.cpp`.
- Header state describes durable UI flow, not `UIManager` component
  internals or cached render data.
- Do not expose helpers just so tests can reach private UI details; test
  through controller APIs and `UIManager` state.

## Input, Events, and Rendering

- SDL polling and UI hit testing stay in the manager paths. Controllers
  consume `InputManager` command/state APIs or `UIManager` component
  state; never reimplement input dispatch.
- Register subscriptions through `ControllerBase` token ownership; keep
  UI refresh synchronized with the event contract.
- States coordinate controller setup and call update/input. Controllers
  never own state transitions, frame clearing, render-pass lifecycle,
  command-buffer submission, or present.
- Hover highlighting and tooltip activation are controlled centrally by
  `UIStyle` / `UIManager`, not one-off controller patches.
- UI update paths are performance-sensitive: no repeated singleton
  lookups, no unnecessary string/container churn. Skip UI mutations when
  values are unchanged.
- Behavior changes update focused controller or `UIManager` functional
  tests in the same change.
