# Architecture

**Code:** `README.md`, `docs/core/GameEngine.md`, `docs/events/EventManager.md`, `docs/managers/EntityDataManager.md`, `docs/managers/WorldResourceManager.md`, `docs/core/TimestepManager.md`, `docs/gpu/GPURendering.md`

## Overview

The engine is layered as:

- Core
- Managers
- GameStates
- Entities / Controllers

**Code dependency direction:** `Core → Managers → GameStates → Entities/Controllers` (higher layers may include lower layers; managers must not depend on states).

### Managers serve the states

Runtime ownership is the reverse of “managers run the game”:

| Role | Owns | Does not own |
|------|------|----------------|
| **GameStates** | Screen lifecycle (`enter` / `exit` / pause / resume), which managers to use and when, state-scoped controllers, event tokens, what UI that screen shows | Permanent manager infrastructure, frame present, worker pools |
| **Domain managers** (AI, World, UI, Collision, …) | Systems, data, and APIs states call (`prepareForStateTransition()`, load, query, update) | Screen flow, which state is active, menu/gameplay policy |
| **GameStateManager** | Active state **stack** and transition order | Gameplay rules; it is **state-machine infrastructure** (lives under `managers/` for history, but is not a domain manager) |
| **GameEngine** | Frame loop, init/shutdown, driving the state machine | Per-screen content |

**Rule of thumb:** states *drive*; managers *serve*. Domain managers never advance the state graph or invent screen policy. States (or `GameStateManager` on their behalf for stack transitions) call manager services.

Current branch contracts:

- `EventManager` is the central event hub with main-thread deferred draining, built-in combat processing, and sequence-preserved ordering across combat and non-combat queues
- AI is EDM-backed and executed through `BehaviorExecutors` + `AICommandBus`
- `WorldResourceManager` is a registry/spatial index over EDM, not a quantity store
- GPU rendering uses explicit swapchain acquisition
- `TimestepManager` accepts real display refresh data for cadence snapping
- `GameOverState` is part of the state graph

## Update Flow

Typical frame shape:

1. `GameEngine` starts the frame
2. input and deferred events are processed
3. active GameState updates UI/controllers/state logic (states use manager services as needed)
4. managers such as AI, collision, particles, and background simulation run (engine-ordered services for the active play context)
5. render path draws scene/UI
6. present completes the frame

## Rendering Path

```text
beginFrame
state.recordGPUVertices()
beginScenePass         (internally acquires the swapchain texture on first use)
state.renderGPUScene()
beginSwapchainPass
renderComposite         (scene texture -> swapchain, zoom applied here)
state.renderGPUUI()
endFrame                (called from GameEngine::present(), separate from render())
```

States provide `recordGPUVertices()`/`renderGPUScene()`/`renderGPUUI()` hooks, but frame lifetime, swapchain acquisition, compositing, and presentation remain engine/GPURenderer-owned.

## GameState Flow

Key states include:

- `LogoState`
- `MainMenuState`
- `SettingsMenuState`
- `LoadingState`
- `GamePlayState`
- `PauseState`
- demo states (`AIDemoState`, `AdvancedAIDemoState`, `EventDemoState`, `OverlayDemoState`, `UIDemoState`)
- `GameOverState`

Important transitions:

- `MainMenuState -> LoadingState -> GamePlayState`
- `GamePlayState -> GameOverState`
- AI-heavy demos may also route to `GameOverState`

## State Transitions (UI + lifecycle)

The transition model is **standard exit-then-enter**. Full-screen UI is cleared in the transition and rebuilt in `enter()`.

```text
Full-screen replace (changeState when stack empties, or changeStateClearingStack):
  exit(old state(s))
  → UIManager::prepareForStateTransition()   // state machine uses UI service once
  → enter(new state)                           // rebuild that screen's UI

Overlay (pushState / popState):
  pause(under) → enter(overlay)                // no full UI clear
  exit(overlay) → resume(under)                // overlay removes only its widgets
```

`GameStateManager` policy:

1. `exit()` the leaving state(s) — state tears down world/AI/controllers and calls manager services; overlay states remove only their own widgets
2. If the stack is empty after those exits (**full-screen replace**), clear global UI once via `UIManager::prepareForStateTransition()` (UI manager **serves** the transition; it does not choose the next state)
3. `enter()` the destination — rebuild UI for full-screen owners; overlay states only add their widgets
4. If `enter()` fails, restore the previous state(s) when possible

Stacked replace without emptying the stack (e.g. Pause → Settings while GamePlay remains underneath) does **not** full-clear UI so the underlying HUD stays registered.

Full-screen `enter()` may still defensively wipe-first; that is optional belt-and-suspenders, not a second ownership model.

## State Teardown

AI/world-heavy states follow this pattern in `exit()` (state drives; managers serve):

1. destroy state-owned NPCs and unregister state-owned handlers
2. call `prepareForStateTransition()` on active managers in dependency order — this is also where world unload (`WorldManager::unloadWorld()`) and EntityDataManager teardown happen, mid-sequence
3. clear controllers / cameras / player handles (full-screen UI is cleared by `GameStateManager` after `exit()`, using `UIManager` as a service)
4. reset remaining cached state (init flags, etc.)

This matters because deferred events, AI command commits, and WRM spatial indices all participate in runtime state now. Order: world/entity teardown in `exit()` → transition UI clear (if full-screen) → destination `enter()` — see `GamePlayState::exit()`.

## Related Docs

- [GameEngine](core/GameEngine.md)
- [GameStateManager](managers/GameStateManager.md)
- [GameStates overview](gameStates/README.md)
- [EventManager](events/EventManager.md)
- [EntityDataManager](managers/EntityDataManager.md)
- [WorldResourceManager](managers/WorldResourceManager.md)
- [TimestepManager](core/TimestepManager.md)
