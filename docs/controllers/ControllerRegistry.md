# ControllerRegistry

**Code:** `include/controllers/ControllerRegistry.hpp`, `include/controllers/IUpdatable.hpp`

## Overview

`ControllerRegistry` is a type-erased container for managing GameState controllers. It provides:

- **Heterogeneous storage** of different controller types
- **Batch operations** for subscribe/unsubscribe/suspend/resume
- **Automatic IUpdatable detection** for per-frame updates
- **Type-safe retrieval** via `get<T>()`

## Design Pattern

```
GameState owns ControllerRegistry owns Controllers

┌─────────────────────────────────────────────────┐
│ GamePlayState                                   │
│  └─► ControllerRegistry                         │
│       ├─► WeatherController                     │
│       ├─► DayNightController                    │
│       └─► CombatController                      │
└─────────────────────────────────────────────────┘
```

## Public API Reference

### Adding Controllers

```cpp
/**
 * @brief Add a controller of type T
 * @tparam T Controller type (must derive from ControllerBase)
 * @tparam Args Constructor argument types
 * @param args Arguments forwarded to T's constructor
 * @return Reference to the created controller
 */
template<typename T, typename... Args>
T& add(Args&&... args);
```

If a controller of type T already exists, returns the existing one. Automatically detects `IUpdatable` interface and adds to update list.

### Retrieving Controllers

```cpp
/**
 * @brief Get a controller of type T
 * @return Pointer to controller, or nullptr if not found
 */
template<typename T>
T* get();

template<typename T>
const T* get() const;

/**
 * @brief Check if a controller of type T is registered
 */
template<typename T>
[[nodiscard]] bool has() const;
```

### Batch Operations

```cpp
void subscribeAll();    // Called after controllers are added in GameState::enter()
void unsubscribeAll();  // Removes subscriptions, but keeps controller objects alive
void suspendAll();      // Called in GameState::pause()
void resumeAll();       // Called in GameState::resume()

/**
 * @brief Update all IUpdatable controllers
 * Only calls update() on controllers that:
 * 1. Implement IUpdatable interface
 * 2. Are not currently suspended
 */
void updateAll(float deltaTime);
```

### Utility Methods

```cpp
[[nodiscard]] size_t size() const;
[[nodiscard]] bool empty() const;
void clear();  // Unsubscribes first, then destroys controller objects
```

Use `clear()` as the normal `GameState::exit()` endpoint when the state is
leaving or re-entering with fresh references. `unsubscribeAll()` is useful when
controller objects must stay alive, but it is not a full teardown.

## IUpdatable Interface

Controllers that need per-frame updates should implement `IUpdatable`:

```cpp
// include/controllers/IUpdatable.hpp
class IUpdatable {
public:
    virtual ~IUpdatable() = default;
    virtual void update(float deltaTime) = 0;
};
```

The registry automatically detects this interface at compile-time:

```cpp
if constexpr (std::is_base_of_v<IUpdatable, T>) {
    m_updatables.push_back({static_cast<IUpdatable*>(&ref), static_cast<ControllerBase*>(&ref)});
}
```

## Usage Examples

### Basic GameState Integration

```cpp
class GamePlayState : public GameState {
private:
    ControllerRegistry m_controllers;

public:
    bool enter() override {
        // Add controllers (constructor args forwarded)
        m_controllers.add<WeatherController>();
        m_controllers.add<DayNightController>();
        m_controllers.add<CombatController>(mp_player);

        // Subscribe all to events
        m_controllers.subscribeAll();
        return true;
    }

    void update(float dt) override {
        // Update all IUpdatable controllers
        m_controllers.updateAll(dt);

        // Game logic...
    }

    void pause() override {
        m_controllers.suspendAll();
    }

    void resume() override {
        m_controllers.resumeAll();
    }

    bool exit() override {
        m_controllers.clear();
        return true;
    }
};
```

### Accessing Controllers

```cpp
void GamePlayState::render() {
    // Get controller for queries
    auto* weather = m_controllers.get<WeatherController>();
    if (weather) {
        auto currentWeather = weather->getCurrentWeather();
        renderWeatherEffects(currentWeather);
    }

    // Check if controller exists
    if (auto* hud = m_controllers.get<HudController>()) {
        updateTargetFrame(hud->hasActiveTarget(),
                          hud->getTargetLabel(),
                          hud->getTargetHealth());
    }
}
```

### Controller with Constructor Arguments

```cpp
// CombatController needs player reference
m_controllers.add<CombatController>(mp_player);

// InventoryController needs a player reference
m_controllers.add<InventoryController>(mp_player);

// ResourceRenderController has no constructor arguments
m_controllers.add<ResourceRenderController>();
```

### Creating an IUpdatable Controller

```cpp
class CombatController : public ControllerBase, public IUpdatable {
public:
    explicit CombatController(std::shared_ptr<Player> player)
        : m_player(player) {}

    void subscribe() override {
        // Subscribe to combat events...
    }

    // IUpdatable implementation
    void update(float deltaTime) override {
        // Per-frame combat logic
        updateCombatTimers(deltaTime);
        processPendingAttacks();
    }

private:
    std::shared_ptr<Player> m_player;
};

// In GameState - automatically gets updateAll() calls
m_controllers.add<CombatController>(mp_player);
```

## Ownership Examples

### Manual Ownership

```cpp
class GamePlayState : public GameState {
private:
    std::unique_ptr<WeatherController> mp_weatherController;
    std::unique_ptr<DayNightController> mp_dayNightController;
    std::unique_ptr<CombatController> mp_combatController;

    bool enter() override {
        mp_weatherController = std::make_unique<WeatherController>();
        mp_dayNightController = std::make_unique<DayNightController>();
        mp_combatController = std::make_unique<CombatController>(mp_player);

        mp_weatherController->subscribe();
        mp_dayNightController->subscribe();
        mp_combatController->subscribe();
        return true;
    }

    void update(float dt) override {
        mp_combatController->update(dt);
    }

    bool exit() override {
        mp_weatherController->unsubscribe();
        mp_dayNightController->unsubscribe();
        mp_combatController->unsubscribe();
        return true;
    }
};
```

### Registry Ownership

```cpp
class GamePlayState : public GameState {
private:
    ControllerRegistry m_controllers;

    bool enter() override {
        m_controllers.add<WeatherController>();
        m_controllers.add<DayNightController>();
        m_controllers.add<CombatController>(mp_player);
        m_controllers.subscribeAll();
        return true;
    }

    void update(float dt) override {
        m_controllers.updateAll(dt);
    }

    bool exit() override {
        m_controllers.clear();
        return true;
    }
};
```

## Internal Implementation

### Storage

```cpp
std::vector<std::unique_ptr<ControllerBase>> m_controllers;
std::vector<UpdatableEntry> m_updatables;  // Cached IUpdatable pointers
std::unordered_map<std::type_index, size_t> m_typeToIndex;
```

### UpdatableEntry

```cpp
struct UpdatableEntry {
    IUpdatable* updatable;    // For calling update()
    ControllerBase* base;     // For checking isSuspended()
};
```

The dual-pointer approach avoids repeated `dynamic_cast` during `updateAll()`.

## Thread Safety

`ControllerRegistry` is **NOT thread-safe**. All operations should be called from the main thread, typically within GameState lifecycle methods.

## Related Documentation

- **[Controllers Overview](README.md)** - Controller pattern and lifecycle
- `include/controllers/ControllerBase.hpp` - Base class for all controllers
- **[GameStateManager](../managers/GameStateManager.md)** - State lifecycle
