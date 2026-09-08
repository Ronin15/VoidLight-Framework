# EntityStateManager

**Code:** `include/entities/EntityStateManager.hpp`, `src/entities/EntityStateManager.cpp`

This is an entity utility, not a domain manager. The page lives under `docs/managers/` for historical layout only.

## Overview

The `EntityStateManager` is a utility class for managing a set of named `EntityState` objects for a single entity. It allows you to add, remove, and switch between states, and ensures only one state is active at a time. This is useful for implementing state machines for NPCs, players, or other game entities.

## Key Features
- Named state registration and lookup
- Safe state transitions (only one active at a time)
- State update delegation
- Ownership and memory management via smart pointers

## API Reference

### Construction & Destruction
```cpp
EntityStateManager();
~EntityStateManager();
```

### State Management
```cpp
void addState(const std::string& stateName, std::unique_ptr<EntityState> state);
    // Registers a new state with the given name. Takes ownership of the state.

void setState(const std::string& stateName);
    // Switches to the named state. Only one state is active at a time.

std::string getCurrentStateName() const;
    // Returns the name of the currently active state, or an empty string if none.

bool hasState(const std::string& stateName) const;
    // Returns true if a state with the given name exists.

void removeState(const std::string& stateName);
    // Removes the named state. If it was active, no state will be active after removal.
```

### State Update
```cpp
void update(float deltaTime);
    // Calls update on the currently active state (if any).
```

## Usage Example
```cpp
EntityStateManager stateMgr;
stateMgr.addState("Idle", std::make_unique<IdleState>());
stateMgr.addState("Attack", std::make_unique<AttackState>());
stateMgr.setState("Idle");

// In your entity's update loop:
stateMgr.update(deltaTime);

// Switch state on some event:
stateMgr.setState("Attack");
```

## Best Practices
- Use descriptive state names (e.g., "Idle", "Attack", "Flee").
- Always check `hasState()` before switching or removing states.
- States should be lightweight and reusable.
- Clean up unused states to avoid memory leaks.

## Thread Safety
This class is **not** thread-safe. All state changes and updates should occur on the main/game thread.

## See Also
- `EntityState` (base class for states)
- `Player` and `NPC` (example users of state managers)
