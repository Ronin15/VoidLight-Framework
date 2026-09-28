/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/world/WeatherController.hpp"
#include "events/TimeEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "core/Logger.hpp"

void WeatherController::subscribe() {
    if (checkAlreadySubscribed()) {
        return;
    }

    auto& eventMgr = EventManager::Instance();

    // Subscribe to Time events to handle WeatherCheckEvent
    auto token = eventMgr.registerHandlerWithToken(
        EventTypeId::Time,
        [this](const EventData& data) { onTimeEvent(data); });
    addHandlerToken(token);

    setSubscribed(true);
    WEATHER_INFO("Subscribed to time events for automatic weather");
}

void WeatherController::onTimeEvent(const EventData& data) {
    if (!data.event) {
        return;
    }

    // Use TimeEventType enum to filter (no RTTI overhead)
    // We registered for EventTypeId::Time, so we know it's a TimeEvent
    const auto* timeEvent = static_cast<const TimeEvent*>(data.event.get());
    if (timeEvent->getTimeEventType() != TimeEventType::WeatherCheck) {
        return;  // Not a weather check event, ignore
    }

    // Safe static_cast - we verified the subtype via enum
    const auto* weatherCheck = static_cast<const WeatherCheckEvent*>(timeEvent);

    // Get recommended weather from the event (based on season probabilities)
    WeatherType recommended = weatherCheck->getRecommendedWeather();

    // Skip if weather hasn't changed - avoid duplicate event log messages
    if (recommended == m_currentWeather) {
        return;
    }

    // Track current weather for status bar display
    m_currentWeather = recommended;

    // Trigger weather change using the string getter (non-blocking, deferred dispatch)
    // This fires a WeatherEvent that ParticleManager subscribes to
    std::string weatherName(getCurrentWeatherString());
    EventManager::Instance().changeWeather(weatherName, 2.0f,
        EventManager::DispatchMode::Deferred);

    WEATHER_DEBUG(weatherName);
}

std::string_view WeatherController::getCurrentWeatherString() const {
    switch (m_currentWeather) {
        case WeatherType::Clear: return "Clear";
        case WeatherType::Cloudy: return "Cloudy";
        case WeatherType::Rainy: return "Rainy";
        case WeatherType::Stormy: return "Stormy";
        case WeatherType::Foggy: return "Foggy";
        case WeatherType::Snowy: return "Snowy";
        case WeatherType::Windy: return "Windy";
        default: return "Clear";
    }
}

std::string_view WeatherController::getCurrentWeatherDescription() const {
    switch (m_currentWeather) {
        case WeatherType::Clear: return "Clear skies";
        case WeatherType::Cloudy: return "Clouds gather";
        case WeatherType::Rainy: return "Rain begins";
        case WeatherType::Stormy: return "Storm approaches";
        case WeatherType::Foggy: return "Fog rolls in";
        case WeatherType::Snowy: return "Snow falls";
        case WeatherType::Windy: return "Wind picks up";
        default: return "Weather changes";
    }
}
