# GameTimeManager

**Code:** `include/managers/GameTimeManager.hpp`, `src/managers/GameTimeManager.cpp`, `include/events/TimeEvent.hpp`

**Singleton Access:** Use `GameTimeManager::Instance()` to access the time system.

## Overview

`GameTimeManager` is the singleton that manages all time-related functionality in the game, including day/night cycles, a fantasy calendar system, seasons with environmental configurations, and automatic weather triggering. It provides event-driven notifications for time changes that controllers and game states can subscribe to.

## Table of Contents

- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
- [Calendar System](#calendar-system)
- [Season System](#season-system)
- [Weather System](#weather-system)
- [Time Events](#time-events)
- [API Reference](#api-reference)
- [Integration Patterns](#integration-patterns)
- [Best Practices](#best-practices)
- [Examples](#examples)

## Quick Start

### Basic Setup

```cpp
#include "managers/GameTimeManager.hpp"

// Initialize in GameEngine (already done by default)
auto& gameTimeManager = GameTimeManager::Instance();
gameTimeManager.init(12.0f, 60.0f);  // Start at noon, 60x time scale

// Update in your game state
void GamePlayState::update(float deltaTime) {
    GameTimeManager::Instance().update(deltaTime);
}

// Query time information
float hour = gameTimeManager.getGameHour();           // 0-23.999
int day = gameTimeManager.getGameDay();               // 1-based
Season season = gameTimeManager.getSeason();          // Spring, Summer, Fall, Winter
bool isNight = gameTimeManager.isNighttime();
```

### Subscribe to Time Events

```cpp
#include "events/TimeEvent.hpp"
#include "managers/EventManager.hpp"

void GamePlayState::enter() {
    auto& eventMgr = EventManager::Instance();

    m_timeEventToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Time,
        [this](const EventData& data) {
            auto timeEvent = std::static_pointer_cast<TimeEvent>(data.event);

            switch (timeEvent->getTimeEventType()) {
                case TimeEventType::HourChanged:
                    onHourChanged(timeEvent);
                    break;
                case TimeEventType::SeasonChanged:
                    onSeasonChanged(timeEvent);
                    break;
                // ... handle other event types
            }
        }
    );
}

void GamePlayState::exit() {
    EventManager::Instance().removeHandler(m_timeEventToken);
}
```

## Core Concepts

### Time Progression

GameTimeManager tracks time using a configurable time scale that converts real-time to game-time:

| Time Scale | 1 Real Second = | 1 Real Minute = |
|------------|-----------------|-----------------|
| 1.0x       | 1 game second   | 1 game minute   |
| 60.0x      | 1 game minute   | 1 game hour     |
| 1440.0x    | 24 game minutes | 1 game day      |

**Default:** 60x time scale (1 real second = 1 game minute)

### Time of Day Periods

Bounds are owned by free `hourToTimePeriod()` in `include/events/TimeEvent.hpp`. `getTimeOfDayName()` switches on that enum.

| Period  | Hours       | Description |
|---------|-------------|-------------|
| Morning | 5:00 - 8:00 | Dawn, warming light |
| Day     | 8:00 - 17:00| Full daylight |
| Evening | 17:00 - 21:00| Sunset, cooling light |
| Night   | 21:00 - 5:00 | Darkness |

### Pause/Resume

```cpp
auto& gameTimeManager = GameTimeManager::Instance();

gameTimeManager.pause();    // Stop time progression
gameTimeManager.resume();   // Resume time (resets internal timing to avoid jumps)
bool paused = gameTimeManager.isPaused();
```

## Calendar System

### Default Fantasy Calendar

GameTimeManager uses a 4-month fantasy calendar by default:

| Month       | Days | Season | Description |
|-------------|------|--------|-------------|
| Bloomtide   | 30   | Spring | New growth, awakening |
| Sunpeak     | 30   | Summer | Peak warmth, longest days |
| Harvestmoon | 30   | Fall   | Harvest time, cooling |
| Frosthold   | 30   | Winter | Cold, shortest days |

**Total:** 120 days per year

### Calendar Queries

```cpp
auto& gameTimeManager = GameTimeManager::Instance();

int month = gameTimeManager.getCurrentMonth();            // 0-based index
std::string_view monthName = gameTimeManager.getCurrentMonthName();  // "Bloomtide"
int dayOfMonth = gameTimeManager.getDayOfMonth();         // 1-based (1-30)
int year = gameTimeManager.getGameYear();                 // Starts at 1
int daysInMonth = gameTimeManager.getDaysInCurrentMonth();
```

### Custom Calendar Configuration

```cpp
CalendarConfig customCalendar;
customCalendar.months = {
    {"FirstMoon", 28, Season::Spring},
    {"SecondMoon", 28, Season::Spring},
    {"ThirdMoon", 28, Season::Summer},
    {"FourthMoon", 28, Season::Summer},
    {"FifthMoon", 28, Season::Fall},
    {"SixthMoon", 28, Season::Fall},
    {"SeventhMoon", 28, Season::Winter},
    {"EighthMoon", 28, Season::Winter}
};

GameTimeManager::Instance().setCalendarConfig(customCalendar);
// Total: 224 days per year
```

## Season System

### Type-Safe Season Enum

```cpp
enum class Season : uint8_t {
    Spring = 0,
    Summer = 1,
    Fall = 2,
    Winter = 3
};
```

### Season Configuration

Each season has environmental parameters:

```cpp
struct SeasonConfig {
    float sunriseHour;           // When sun rises
    float sunsetHour;            // When sun sets
    float minTemperature;        // Coldest (at 4 AM)
    float maxTemperature;        // Warmest (at 2 PM)
    WeatherProbabilities weatherProbs;
};
```

### Default Season Parameters

| Season | Sunrise | Sunset | Min Temp | Max Temp |
|--------|---------|--------|----------|----------|
| Spring | 6:00    | 19:00  | 45°F     | 70°F     |
| Summer | 5:00    | 21:00  | 70°F     | 95°F     |
| Fall   | 6:30    | 18:00  | 40°F     | 65°F     |
| Winter | 7:30    | 17:00  | 20°F     | 45°F     |

### Season Queries

```cpp
auto& gameTimeManager = GameTimeManager::Instance();

Season season = gameTimeManager.getSeason();              // Type-safe enum
std::string_view name = gameTimeManager.getSeasonName();  // "Spring", "Summer", etc.
const SeasonConfig& config = gameTimeManager.getSeasonConfig();
float temp = gameTimeManager.getCurrentTemperature();     // Interpolated by time of day
```

### Temperature Calculation

Temperature follows a cosine curve throughout the day:
- **Coldest:** 4:00 AM (minimum temperature)
- **Warmest:** 2:00 PM (maximum temperature)

```cpp
// Temperature is interpolated between min and max based on hour
float temp = gameTimeManager.getCurrentTemperature();  // Returns current temp in Fahrenheit
```

## Weather System

### Weather Types

```cpp
enum class WeatherType {
    Clear,
    Cloudy,
    Rainy,
    Stormy,
    Foggy,
    Snowy,
    Windy
};
```

### Weather Probabilities by Season

| Weather | Spring | Summer | Fall | Winter |
|---------|--------|--------|------|--------|
| Clear   | 35%    | 45%    | 30%  | 25%    |
| Cloudy  | 25%    | 20%    | 30%  | 30%    |
| Rainy   | 20%    | 15%    | 20%  | 10%    |
| Stormy  | 5%     | 10%    | 5%   | 5%     |
| Foggy   | 10%    | 5%     | 10%  | 10%    |
| Snowy   | 0%     | 0%     | 0%   | 15%    |
| Windy   | 5%     | 5%     | 5%   | 5%     |

### Automatic Weather System

```cpp
auto& gameTimeManager = GameTimeManager::Instance();

// Enable automatic weather changes
gameTimeManager.enableAutoWeather(true);

// Set check interval (default: 4 game hours)
gameTimeManager.setWeatherCheckInterval(4.0f);

// Manual weather roll
WeatherType weather = gameTimeManager.rollWeatherForSeason();         // Current season
WeatherType winter = gameTimeManager.rollWeatherForSeason(Season::Winter);  // Specific season
```

### Weather Event Flow

```
GameTimeManager::update()
  → checkWeatherUpdate() (every N game hours)
    → rollWeatherForSeason()
      → WeatherCheckEvent dispatched (Deferred)
        → WeatherController receives event
          → EventManager::changeWeather()
            → WeatherEvent dispatched
              → ParticleManager creates weather particles
```

## Time Events

All time events inherit from `TimeEvent` and are dispatched via `EventManager` with `Deferred` mode.

### Event Types

| Event Type | Trigger | Key Properties |
|------------|---------|----------------|
| `HourChangedEvent` | Every game hour | `hour`, `isNight` |
| `DayChangedEvent` | New day begins | `day`, `dayOfMonth`, `month`, `monthName` |
| `MonthChangedEvent` | New month begins | `month`, `monthName`, `season` |
| `SeasonChangedEvent` | Season changes | `season`, `previousSeason`, `seasonName` |
| `YearChangedEvent` | New year begins | `year` |
| `WeatherCheckEvent` | Weather roll triggered | `season`, `recommendedWeather` |
| `TimePeriodChangedEvent` | Time period changes | `period`, `previousPeriod`, `visuals` |

### Event Handling Pattern

```cpp
void onTimeEvent(const EventData& data) {
    auto timeEvent = std::static_pointer_cast<TimeEvent>(data.event);

    // Use enum for efficient filtering (no dynamic_cast)
    switch (timeEvent->getTimeEventType()) {
        case TimeEventType::HourChanged: {
            auto hourEvent = std::static_pointer_cast<HourChangedEvent>(data.event);
            int hour = hourEvent->getHour();
            bool isNight = hourEvent->isNight();
            break;
        }
        case TimeEventType::SeasonChanged: {
            auto seasonEvent = std::static_pointer_cast<SeasonChangedEvent>(data.event);
            Season newSeason = seasonEvent->getSeason();
            Season oldSeason = seasonEvent->getPreviousSeason();
            break;
        }
        // ... other cases
    }
}
```

### TimePeriodVisuals

Visual configuration for time-of-day overlay tints:

```cpp
struct TimePeriodVisuals {
    uint8_t overlayR, overlayG, overlayB, overlayA;

    // Factory methods
    static TimePeriodVisuals getMorning();   // {255, 140, 80, 30}  Red-orange dawn
    static TimePeriodVisuals getDay();       // {255, 255, 200, 8}  Slight yellow
    static TimePeriodVisuals getEvening();   // {255, 80, 40, 40}   Orange-red sunset
    static TimePeriodVisuals getNight();     // {20, 20, 60, 90}    Dark blue/purple
};
```

## API Reference

### Initialization & Control

```cpp
static GameTimeManager& Instance();             // Singleton access
bool init(float startHour = 12.0f, float timeScale = 1.0f);
void update(float deltaTime);                   // Call from game state
void pause();
void resume();
bool isPaused() const;
```

### Time Queries

```cpp
float getGameHour() const;                      // 0-23.999
int getGameDay() const;                         // 1-based
float getTotalGameTimeSeconds() const;          // Cumulative
float getTimeScale() const;
bool isDaytime() const;
bool isNighttime() const;
std::string_view getTimeOfDayName() const;      // "Morning"/"Day"/"Evening"/"Night" via hourToTimePeriod
std::string_view formatCurrentTime(bool use24Hour = true);  // "14:30" or "2:30 PM"
```

### Time Modification

```cpp
void setTimeScale(float scale);
void setGameHour(float hour);                   // 0-23.999
void setGameDay(int day);                       // Updates calendar state
void setDaylightHours(float sunrise, float sunset);
```

### Calendar Methods

```cpp
void setCalendarConfig(const CalendarConfig& config);
int getCurrentMonth() const;                    // 0-based
int getDayOfMonth() const;                      // 1-based
int getGameYear() const;                        // Starts at 1
std::string_view getCurrentMonthName() const;
int getDaysInCurrentMonth() const;
```

### Season Methods

```cpp
Season getSeason() const;                       // Type-safe enum
std::string_view getSeasonName() const;
const SeasonConfig& getSeasonConfig() const;
float getCurrentTemperature() const;            // Interpolated
int getCurrentSeason(int daysPerSeason = 30) const;  // Legacy method
```

### Weather Methods

```cpp
void enableAutoWeather(bool enable);
bool isAutoWeatherEnabled() const;
void setWeatherCheckInterval(float gameHours);
WeatherType rollWeatherForSeason() const;
WeatherType rollWeatherForSeason(Season season) const;
```

## Integration Patterns

### With Controllers

The recommended pattern uses Controllers for state-specific time handling:

```cpp
// GamePlayState::enter()
GameTimeManager::Instance().enableAutoWeather(true);
m_weatherController.subscribe();
m_dayNightController.subscribe();

// GamePlayState::exit()
m_weatherController.unsubscribe();
m_dayNightController.unsubscribe();
```

See: `docs/controllers/README.md` for Controller pattern details.

### With WorldManager (Seasonal Textures)

```cpp
// WorldManager subscribes to SeasonChangedEvent
auto seasonEvent = std::static_pointer_cast<SeasonChangedEvent>(data.event);
Season newSeason = seasonEvent->getSeason();

// Invalidate texture cache and reload seasonal textures
m_seasonTexturesDirty.store(true, std::memory_order_release);
```

### With ParticleManager (Weather Effects)

```cpp
// WeatherController converts WeatherCheckEvent to actual weather change
void WeatherController::onTimeEvent(const EventData& data) {
    auto weatherCheck = std::dynamic_pointer_cast<WeatherCheckEvent>(data.event);
    if (weatherCheck) {
        // Apply the recommended weather
        EventManager::Instance().changeWeather(
            weatherCheck->getRecommendedWeather()
        );
    }
}
```

## Best Practices

### 1. Update from Game State, Not GameEngine

```cpp
// CORRECT: State controls when time updates
void GamePlayState::update(float deltaTime) {
    GameTimeManager::Instance().update(deltaTime);
}

// WRONG: Don't update from GameEngine directly
// (Time should only advance in appropriate states)
```

### 2. Use Controllers for Event Handling

```cpp
// GOOD: Use Controllers for common patterns
m_weatherController.subscribe();      // Weather events
m_dayNightController.subscribe();     // Time period changes

// AVOID: Manual event subscription for common cases
// (Controllers handle zero-allocation, proper lifecycle)
```

### 3. Filter Events Efficiently

```cpp
// GOOD: Use TimeEventType enum for filtering
if (timeEvent->getTimeEventType() != TimeEventType::HourChanged) {
    return;  // Early exit
}

// AVOID: Expensive dynamic_cast chains
auto hourEvent = std::dynamic_pointer_cast<HourChangedEvent>(data.event);
if (!hourEvent) { ... }
```

### 4. Pause During Menus

```cpp
void PauseState::enter() {
    GameTimeManager::Instance().pause();
}

void PauseState::exit() {
    GameTimeManager::Instance().resume();
}
```

## Examples

### Display Time HUD

```cpp
void updateTimeDisplay() {
    auto& gameTimeManager = GameTimeManager::Instance();

    // Format: "Day 15 Bloomtide, Year 1 | 14:30 | Day"
    std::string status = std::format(
        "Day {} {}, Year {} | {} | {}",
        gameTimeManager.getDayOfMonth(),
        gameTimeManager.getCurrentMonthName(),
        gameTimeManager.getGameYear(),
        gameTimeManager.formatCurrentTime(true),
        gameTimeManager.getTimeOfDayName()
    );

    UIManager::Instance().setText("time_label", status);
}
```

### Season-Based Game Logic

```cpp
void updateSeasonalBehavior() {
    Season season = GameTimeManager::Instance().getSeason();

    switch (season) {
        case Season::Winter:
            // Reduce NPC outdoor activity
            m_outdoorActivityMultiplier = 0.5f;
            // Enable ice hazards
            m_iceHazardsActive = true;
            break;

        case Season::Summer:
            // Normal activity
            m_outdoorActivityMultiplier = 1.0f;
            // Disable ice hazards
            m_iceHazardsActive = false;
            break;

        // ... other seasons
    }
}
```

### Custom Time Scale for Debugging

```cpp
void setupDebugTimeControls() {
    // Fast-forward time for testing
    if (InputManager::Instance().isKeyPressed(SDL_SCANCODE_PERIOD)) {
        GameTimeManager::Instance().setTimeScale(600.0f);  // 10x faster
    }

    // Normal speed
    if (InputManager::Instance().isKeyPressed(SDL_SCANCODE_COMMA)) {
        GameTimeManager::Instance().setTimeScale(60.0f);   // Default
    }

    // Pause
    if (InputManager::Instance().isKeyPressed(SDL_SCANCODE_P)) {
        auto& gameTimeManager = GameTimeManager::Instance();
        gameTimeManager.isPaused() ? gameTimeManager.resume() : gameTimeManager.pause();
    }
}
```

---

## Related Documentation

- **Controllers:** `docs/controllers/README.md` - State-scoped event handling
- **WeatherController:** `docs/controllers/WeatherController.md` - Weather coordination
- **DayNightController:** `docs/controllers/DayNightController.md` - Visual time periods
- **EventManager:** `docs/events/EventManager.md` - Event system reference
- **TimeEvents:** `docs/events/TimeEvents.md` - Time event type reference

---

*This documentation reflects the GameTimeManager system introduced in the world_time branch, providing comprehensive time management with calendar, seasons, weather, and event-driven updates.*
