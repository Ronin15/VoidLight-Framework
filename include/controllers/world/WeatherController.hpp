/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef WEATHER_CONTROLLER_HPP
#define WEATHER_CONTROLLER_HPP

/**
 * @file WeatherController.hpp
 * @brief Lightweight controller that bridges GameTime weather checks to actual weather changes
 *
 * WeatherController subscribes to WeatherCheckEvent (from GameTimeManager) and triggers actual
 * weather changes via EventManager::changeWeather(). This is a controller, not a manager -
 * it's an event subscriber that reacts to time events, not a system initialized in GameEngine.
 *
 * Ownership: GameState owns the controller instance (not a singleton).
 *
 * Event flow:
 *   GameTimeManager::checkWeatherUpdate() -> WeatherCheckEvent (Deferred)
 *     -> WeatherController handles it
 *     -> EventManager::changeWeather() (Deferred)
 *     -> WeatherEvent dispatched
 *     -> ParticleManager handles it -> Visual effects rendered
 */

#include "controllers/ControllerBase.hpp"
#include <string_view>

// Forward declaration
enum class WeatherType;

class WeatherController : public ControllerBase {
public:
    WeatherController() = default;
    ~WeatherController() override = default;

    // Movable (inherited from base)
    WeatherController(WeatherController&&) noexcept = default;
    WeatherController& operator=(WeatherController&&) noexcept = default;

    /**
     * @brief Subscribe to weather check events
     * @note Called by ControllerRegistry::subscribeAll()
     */
    void subscribe() override;

    /**
     * @brief Get controller name for debugging
     * @return "WeatherController"
     */
    [[nodiscard]] std::string_view getName() const override { return "WeatherController"; }

    /**
     * @brief Get the current weather type
     * @return Current WeatherType enum value (defaults to Clear)
     */
    [[nodiscard]] WeatherType getCurrentWeather() const { return m_currentWeather; }

    /**
     * @brief Get current weather as string (zero allocation)
     * @return String view: "Clear", "Rainy", etc.
     */
    [[nodiscard]] std::string_view getCurrentWeatherString() const;

    /**
     * @brief Get descriptive weather message for event log (zero allocation)
     * @return String view: "Fog rolls in", "Storm approaches", etc.
     */
    [[nodiscard]] std::string_view getCurrentWeatherDescription() const;

private:
    /**
     * @brief Handler for time events - filters for WeatherCheckEvent
     * @param data Event data containing the time event
     */
    void onTimeEvent(const EventData& data);

    WeatherType m_currentWeather{}; // Initialized in cpp to avoid header dependency
};

#endif // WEATHER_CONTROLLER_HPP
