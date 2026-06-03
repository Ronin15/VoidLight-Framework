# Architecture Checklist

Use this checklist after tracing the runtime path.

## Ownership

- Is persistent cross-frame entity state stored in EDM?
- Is behavior logic owned by the correct manager or behavior layer?
- Is the controller acting as a state-scoped feature orchestrator instead of a hidden owner of another subsystem's state?
- Is there only one canonical source of truth?

## EDM Boundary

- New fields in EDM represent persistent state, not policy.
- Manager-local scratch is not being used to hold state that render, collision, save/load, or future frames need.
- AI memory, emotion, and behavior state are not being mutated from arbitrary controllers.

## Manager / Controller Boundary

- Managers own caches, registries, scheduling, pathing, transitions, and subsystem cleanup.
- Controllers can choose outcomes and trigger actions for a state feature, but they should not absorb manager-owned cleanup or AI-state ownership.
- Render controllers should not own destruction or lifecycle teardown.
- Controllers should use established registry and event-bridge patterns instead of cached manager/controller members.

## State Transition Coverage

- `prepareForStateTransition()` is called where required.
- Full exit and loading-transition paths clean up consistently.
- `GamePlayState::exit()` clears `ControllerRegistry`, not only subscriptions.
- Persistent handlers remain persistent; transient handlers are not manually re-managed.
- `clearAllHandlers()` is reserved for shutdown, not normal state transitions.
- Manager teardown order still matches repo guidance.

## World Lifecycle

- World-owned spatial indices and reverse lookups are removed when the world unloads.
- Active-world bookkeeping is cleared or updated consistently.
- No stale handles or cache entries survive a world change.
- Transition cleanup clears world-geometry caches directly where needed instead of relying only on deferred unload events.

## Event Contracts

- Systems that depend on an event still receive it after the change.
- UI-dirtying or log-update paths are not bypassed by direct state mutation.
- Immediate vs deferred dispatch still matches the established contract.
- Collision callbacks remain manager-owned infrastructure; game states do not register them directly.
- Projectile collisions continue through the persistent projectile hit sink.

## Rendering / GameState

- `GameEngine` owns frame lifecycle, including clear, command submission, present, and `endFrame()`.
- Game states restrict GPU work to recording/submitting through `recordGPUVertices()`, `renderGPUScene()`, and `renderGPUUI()`.
- Render data resolves manager-owned textures at submission and only materializes raw GPU pointers at the final API boundary.
- UI and render controllers read canonical state and use public layout/render APIs rather than owning teardown or reaching back into the engine for relayout.

## C++ API Boundary

- New contracts avoid raw-pointer ownership, nullable raw-pointer parameters/returns, and raw-pointer optional out-parameters.
- Optional or nullable semantics use references, values, `std::optional`, handles, or smart pointers as appropriate.
- Text constants and parameters avoid `const char*` and C-string APIs; use `std::string_view` or `std::string` according to lifetime needs.
- C-style code appears only at unavoidable SDL or other C API boundaries, and that boundary is isolated.

## Test Expectations

- Tests prove the intended owner boundary, not just a local outcome.
- Add at least one regression when fixing drift.
- Prefer targeted executables:
  - controller tests for controller-boundary changes
  - manager tests for owner cleanup/caches
  - integration tests when the contract only appears across multiple systems
