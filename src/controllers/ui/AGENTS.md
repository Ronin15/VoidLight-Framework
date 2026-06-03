# AGENTS.md - UI Controller Source

These instructions apply to UI controller implementation code under
`src/controllers/ui/`. Follow the loaded parent guidance first; this file adds
UI-controller-specific ownership and runtime rules.

## Development Stance

- Keep this subtree flexible for new gameplay UI flows. Prefer stable
  ownership and data-flow rules over component-by-component checklists.
- Match existing controller patterns before adding new helpers or abstractions.
- Treat UI update paths as performance-sensitive: avoid per-frame allocations,
  repeated singleton lookups, and unnecessary string or container churn.

## Ownership and Data Flow

- UI controllers own state-scoped gameplay UI flow: HUD state, inventory
  presentation, drag/drop state, visibility, input orchestration, and
  event-driven refresh.
- `UIManager` owns component storage, theme/style policy, layout, hit testing,
  tooltip policy, render batching, and state-transition cleanup. Do not
  duplicate those responsibilities in controllers.
- Use `UIConstants.hpp` for shared UI fonts, sizing, z-order, and layout
  constants. Keep controller-local geometry constants local when they only
  apply to that controller.
- Use existing `UIManager` creation, positioning, visibility, style, and
  relayout APIs before adding new UI plumbing.
- Gameplay data remains in its owning systems. Controllers may read canonical
  state and emit existing events or commands, but they should not become a
  second source of truth for inventory, equipment, resources, combat, or world
  state.

## Input, Events, and Rendering

- Keep SDL event polling and UI hit testing in the established manager paths.
  Controllers should consume `InputManager` command/state APIs or `UIManager`
  component state rather than reimplementing input dispatch.
- Register controller event subscriptions through `ControllerBase` token
  ownership and keep UI refresh synchronized with the relevant event contract.
- Game states coordinate controller setup and call controller update/input
  methods. Controllers should not own game-state transitions, frame clearing,
  render pass lifecycle, command-buffer submission, or present.
- Preserve the current central hover and tooltip policy: passive mouse hover
  highlighting and tooltip activation are controlled by `UIStyle` and
  `UIManager`, not by one-off controller patches.

## Adding or Changing UI Flow

- Keep reusable gameplay UI behavior in controllers when it spans state setup,
  input, events, and refresh. Keep one-off state labels or simple status text in
  the state when no reusable controller behavior is needed.
- Prefer explicit component IDs and small controller-local helpers for repeated
  ID construction. Avoid broad compatibility overloads or generic UI builder
  layers unless the existing controller boundary clearly needs them.
- When behavior changes, update focused controller or `UIManager` functional
  tests in the same change.
