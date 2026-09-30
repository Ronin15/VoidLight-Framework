# WeatherController

**Code:** `include/controllers/world/WeatherController.hpp`, `src/controllers/world/WeatherController.cpp`, `tests/controllers/WeatherControllerTests.cpp`

**Ownership:** GameState owns the controller instance (not a singleton).

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

// In GamePlayState.hpp - controller as member
class GamePlayState : public GameState {
private:
    WeatherController m_weatherController;  // Owned by state
};

// In GamePlayState::enter()
GameTimeManager::Instance().enableAutoWeather(true);  // Enable weather checks
m_weatherController.subscribe();

// In GamePlayState::exit()
m_weatherController.unsubscribe();
```

## API Reference

### subscribe()

```cpp
void subscribe();
```

Subscribe to weather check events from `GameTimeManager`.

**Note:** Called when a world state enters, NOT in GameEngine::init().

### unsubscribe()

```cpp
void unsubscribe();
```

Unsubscribe from weather check events.

**Note:** Called when a world state exits.

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

**Returns:** Static string pointer: "Clear", "Cloudy", "Rainy", "Stormy", "Foggy", "Snowy", or "Windy"

### isSubscribed()

```cpp
bool isSubscribed() const;
```

Check if currently subscribed to weather events.

## Weather Types

```cpp
enum class WeatherType {
    Clear,    // No weather effects
    Cloudy,   // Overcast sky
    Rainy,    // Rain particles
    Stormy,   // Heavy rain + lightning
    Foggy,    // Fog overlay
    Snowy,    // Snow particles
    Windy     // Wind effects on particles
};
```

## Usage Example

```cpp
// GamePlayState.hpp
#include "controllers/world/WeatherController.hpp"

class GamePlayState : public GameState {
private:
    WeatherController m_weatherController;  // Owned by state
};

// GamePlayState.cpp
#include "managers/GameTimeManager.hpp"

bool GamePlayState::enter() {
    // Enable automatic weather in GameTimeManager
    GameTimeManager::Instance().enableAutoWeather(true);
    GameTimeManager::Instance().setWeatherCheckInterval(4.0f);  // Every 4 game hours

    // Subscribe WeatherController to handle weather checks
    m_weatherController.subscribe();

    return true;
}

void GamePlayState::update(float deltaTime) {
    // Display current weather
    WeatherType weather = m_weatherController.getCurrentWeather();
    std::string_view weatherStr = m_weatherController.getCurrentWeatherString();

    // Use in UI or game logic
    if (weather == WeatherType::Rainy || weather == WeatherType::Stormy) {
        // Reduce NPC outdoor activity
        m_outdoorActivityMultiplier = 0.5f;
    }
}

void GamePlayState::exit() {
    m_weatherController.unsubscribe();
}
```

## Integration with ParticleManager

When WeatherController calls `EventManager::changeWeather()`, ParticleManager automatically:

1. Receives the `WeatherEvent`
2. Creates appropriate weather particles (rain, snow, etc.)
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
