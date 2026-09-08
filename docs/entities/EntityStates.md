# Entity States

**Code:** `include/entities/EntityState.hpp`, `include/entities/EntityStateManager.hpp`, `include/entities/playerStates/PlayerIdleState.hpp`, `include/entities/playerStates/PlayerRunningState.hpp`, `include/entities/playerStates/PlayerAttackingState.hpp`, `include/entities/playerStates/PlayerHurtState.hpp`, `include/entities/playerStates/PlayerDyingState.hpp`

## Overview

The current codebase only includes player entity state classes. The old NPC state-machine subtree is no longer present in `include/entities/` or `src/entities/`, so this document tracks the actual player-state set and the `EntityStateManager` lifecycle they use.

## Core Pattern

`EntityState` defines the `enter()`, `update(float)`, and `exit()` lifecycle. `EntityStateManager` owns the active state and handles transitions.

```cpp
stateManager.addState("idle", std::make_unique<PlayerIdleState>(*player));
stateManager.addState("running", std::make_unique<PlayerRunningState>(*player));
stateManager.setState("idle");

// Player wraps the named API:
player->changeState("running");
```

## Current Player States

### `PlayerIdleState`

- Code: `include/entities/playerStates/PlayerIdleState.hpp`, `src/entities/playerStates/PlayerIdleState.cpp`
- Default state when the player is not moving or attacking
- Handles input checks and transition triggers

### `PlayerRunningState`

- Code: `include/entities/playerStates/PlayerRunningState.hpp`, `src/entities/playerStates/PlayerRunningState.cpp`
- Handles movement input and running animation
- Uses `handleMovementInput()` and `handleRunningAnimation()` helpers

### `PlayerAttackingState`

- Code: `include/entities/playerStates/PlayerAttackingState.hpp`, `src/entities/playerStates/PlayerAttackingState.cpp`
- Handles the attack animation window and cooldown timing
- Uses a fixed `ATTACK_ANIMATION_TIME`

### `PlayerHurtState`

- Code: `include/entities/playerStates/PlayerHurtState.hpp`, `src/entities/playerStates/PlayerHurtState.cpp`
- Represents the post-hit reaction window
- Tracks elapsed time for the hurt animation/state duration

### `PlayerDyingState`

- Code: `include/entities/playerStates/PlayerDyingState.hpp`, `src/entities/playerStates/PlayerDyingState.cpp`
- Final player state before game-over or respawn handling

## Notes

- If you are looking for NPC behavior, use the AI/behavior docs instead of entity state-machine docs.
- Any future entity state additions should follow the same `EntityStateManager` lifecycle pattern used by the player states.

## Related Documentation

- [EntityHandle](EntityHandle.md)
- [EntityDataManager](../managers/EntityDataManager.md)
- [AIManager](../ai/AIManager.md)
- [Behavior Execution Pipeline](../ai/BehaviorExecutionPipeline.md)
