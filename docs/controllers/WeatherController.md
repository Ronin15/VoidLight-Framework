# WeatherController

**Code:** `include/controllers/world/WeatherController.hpp`, `src/controllers/world/WeatherController.cpp`, `tests/controllers/WeatherControllerTests.cpp`

**Ownership:** GameState owns the controller through its `ControllerRegistry` (`m_controllers.add<WeatherController>()`; not a singleton).

## Overview

WeatherController is a lightweight controller that bridges `GameTimeManager` weather checks to actual weather changes. It subscribes to `WeatherCheckEvent` (dispatched by `GameTimeManager`) and triggers actual weather changes via `EventManager::changeWeather()`, which then dispatches `WeatherEvent` for visual effects.

## Event Flow

```
GameTimeManager::checkWeatherUpdate()
  → WeatherCheckEvent (Deferred)
    → WeatherController handles it
      → EventManager::changeWeather() (Deferred)
        → WeatherEvent dispatched
          → ParticleManager handles it → Visual effects rendered
```

## Quick Start

```cpp
#include "controllers/world/WeatherController.hpp"
#include "managers/GameTimeManager.hpp"

// In GamePlayState::enter()
m_controllers.add<WeatherController>();
m_controllers.subscribeAll();
GameTimeManager::Instance().enableAutoWeather(true);  // Enable weather checks

// In GamePlayState::exit()
m_controllers.clear();  // unsubscribes and destroys controllers
```

## API Reference

### subscribe()

```cpp
void subscribe();
```

Subscribe to weather check events from `GameTimeManager`.

**Note:** Called by `ControllerRegistry::subscribeAll()` when a world state enters, NOT in GameEngine::init().

### unsubscribe()

```cpp
void unsubscribe();
```

Unsubscribe from weather check events (inherited from `ControllerBase`).

**Note:** Called through `ControllerRegistry` when a world state exits.

### getCurrentWeather()

```cpp
WeatherType getCurrentWeather() const;
```

Get the current weather type.

**Returns:** Current `WeatherType` enum value (defaults to Clear)

### getCurrentWeatherString()

```cpp
std::string_view getCurrentWeatherString() const;
```

Get current weather as a string (zero allocation).

**Returns:** String view: "Clear", "Cloudy", "Rainy", "Stormy", "Foggy", "Snowy", or "Windy"

### getCurrentWeatherDescription()

```cpp
std::string_view getCurrentWeatherDescription() const;
```

Descriptive message for the event log (zero allocation), e.g. "Fog rolls in", "Storm approaches".

### isSubscribed()

```cpp
bool isSubscribed() const;
```

Check if currently subscribed to weather events.

## Weather Types

```cpp
enum class WeatherType {
    Clear,    // No weather particles
    Cloudy,   // Cloud particles
    Rainy,    // Heavy rain particles
    Stormy,   // Heavy rain particles
    Foggy,    // Fog particles
    Snowy,    // Heavy snow particles
    Windy,    // Wind storm particles
    Custom    // Named custom weather (variant chosen by custom name)
};
```

## Usage Example

```cpp
// GamePlayState.cpp
#include "controllers/world/WeatherController.hpp"
#include "managers/GameTimeManager.hpp"

bool GamePlayState::enter() {
    m_controllers.add<WeatherController>();
    m_controllers.subscribeAll();

    // Enable automatic weather in GameTimeManager
    GameTimeManager::Instance().enableAutoWeather(true);
    GameTimeManager::Instance().setWeatherCheckInterval(4.0f);  // Every 4 game hours
    return true;
}

void GamePlayState::update(float deltaTime) {
    // Local reference; do not cache controller pointers as members
    if (auto* weatherCtrl = m_controllers.get<WeatherController>()) {
        WeatherType weather = weatherCtrl->getCurrentWeather();
        std::string_view weatherStr = weatherCtrl->getCurrentWeatherString();
        // Use in UI or game logic
    }
}

bool GamePlayState::exit() {
    m_controllers.clear();
    return true;
}
```

## Integration with ParticleManager

When WeatherController calls `EventManager::changeWeather()`, ParticleManager automatically:

1. Receives the `WeatherEvent` (through the owning state's `EventTypeId::Weather` handler, which calls `ParticleManager::handleWeatherEvent()`)
2. Starts the particle variant for the event's `WeatherType` (see "Weather Variant Selection" in `docs/managers/ParticleManager.md`); intensity does not pick the variant
3. Manages particle lifecycle until weather changes

You don't need to manually create weather particles - just use WeatherController.

## Manual Weather Control

If you want to override automatic weather:

```cpp
// Disable auto weather
GameTimeManager::Instance().enableAutoWeather(false);

// Manually trigger weather change (type name, transition seconds, dispatch mode)
EventManager::Instance().changeWeather("Stormy", 2.0f,
    EventManager::DispatchMode::Deferred);
```

## Performance Characteristics

- **Per-frame cost:** Zero (event-driven only)
- **Memory:** Minimal (handler tokens, current weather state)
- **Allocations:** Zero per-frame

## Related Documentation

- **Controller Pattern:** `docs/controllers/README.md`
- **GameTimeManager:** `../managers/GameTimeManager.md`
- **ParticleManager:** `docs/managers/ParticleManager.md`
- **EventManager:** `docs/events/EventManager.md`
