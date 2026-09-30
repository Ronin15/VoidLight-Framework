/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "events/WeatherEvent.hpp"
#include "managers/GameTimeManager.hpp"
#include "core/Logger.hpp"
#include "managers/WorldManager.hpp"
#include "world/WorldData.hpp"
#include "utils/Vector2D.hpp"
#include <algorithm>
#include <cctype>
#include <format>
#include <ostream>
#include <random>

// Stream operator for WeatherType (moved from header)
std::ostream& operator<<(std::ostream& os, const WeatherType& type) {
    switch (type) {
        case WeatherType::Clear: os << "Clear"; break;
        case WeatherType::Cloudy: os << "Cloudy"; break;
        case WeatherType::Rainy: os << "Rainy"; break;
        case WeatherType::Stormy: os << "Stormy"; break;
        case WeatherType::Foggy: os << "Foggy"; break;
        case WeatherType::Snowy: os << "Snowy"; break;
        case WeatherType::Windy: os << "Windy"; break;
        case WeatherType::Custom: os << "Custom"; break;
        default: os << "Unknown"; break;
    }
    return os;
}

// Helper for getting current game time (hour of day)
// Helper for getting current game time of day (0-24)
static float getCurrentGameTime() {
    // Use the GameTime system for simulated game time
    // Add safety check to prevent segfault if GameTime not initialized
    try {
        return GameTimeManager::Instance().getGameHour();
    } catch (...) {
        // Return default time if GameTime is not available
        return 12.0f; // Default to noon
    }
}

// Helper for getting current player position
static Vector2D getPlayerPosition() {
    // This would typically come from the player entity
    // For now, return a placeholder position
    return Vector2D(0.0f, 0.0f);
}

// Helper for getting current season
static int
getCurrentSeason() { // Use the GameTime system for simulated game seasons
    // 0=spring, 1=summer, 2=fall, 3=winter
    // Add safety check to prevent segfault if GameTime not initialized
    try {
        return GameTimeManager::Instance().getCurrentSeason();
    } catch (...) {
        // Return default season if GameTime is not available
        return 0; // Default to spring
    }
}

WeatherEvent::WeatherEvent(const std::string& name, WeatherType type)
    : m_name(name), m_weatherType(type) {
    applyDefaultParamsForType();
}

WeatherEvent::WeatherEvent(const std::string& name,
    const std::string& customType)
    : m_name(name), m_weatherType(WeatherType::Custom), m_customType(customType) {
    applyDefaultParamsForType();
}

void WeatherEvent::applyDefaultParamsForType() {
    m_params = WeatherParams{};

    switch (m_weatherType) {
        case WeatherType::Clear:
            m_params.intensity = 0.0f;
            m_params.visibility = 1.0f;
            m_params.windSpeed = 0.1f;
            break;
        case WeatherType::Cloudy:
            m_params.intensity = 0.5f;
            m_params.visibility = 0.8f;
            m_params.windSpeed = 0.3f;
            m_params.particleEffect = "Cloudy";
            break;
        case WeatherType::Rainy:
            m_params.intensity = 0.7f;
            m_params.visibility = 0.6f;
            m_params.windSpeed = 0.5f;
            m_params.particleEffect = "Rain";
            m_params.soundEffect = "rain_ambient";
            break;
        case WeatherType::Stormy:
            m_params.intensity = 1.0f;
            m_params.visibility = 0.3f;
            m_params.windSpeed = 0.9f;
            m_params.particleEffect = "HeavyRain";
            m_params.soundEffect = "thunder_storm";
            break;
        case WeatherType::Foggy:
            m_params.intensity = 0.6f;
            m_params.visibility = 0.2f;
            m_params.windSpeed = 0.1f;
            m_params.particleEffect = "Fog";
            break;
        case WeatherType::Snowy:
            m_params.intensity = 0.7f;
            m_params.visibility = 0.5f;
            m_params.windSpeed = 0.4f;
            m_params.particleEffect = "Snow";
            m_params.soundEffect = "snow_ambient";
            break;
        case WeatherType::Windy:
            m_params.intensity = 0.6f;
            m_params.visibility = 0.9f;
            m_params.windSpeed = 1.0f;
            m_params.soundEffect = "wind_ambient";
            break;
        case WeatherType::Custom:
            m_params.intensity = 0.5f;
            m_params.visibility = 0.8f;
            m_params.windSpeed = 0.3f;
            break;
    }
}

void WeatherEvent::update() {
    // Skip update if not active or on cooldown
    if (!m_active || m_onCooldown) {
        return;
    }

    // Update transition if in progress
    if (m_inTransition) {
        // Transition logic would be implemented here
        // This would gradually blend between weather states
    }

    // Update frame counter for frequency control
    m_frameCounter++;
    if (m_updateFrequency > 1 && m_frameCounter % m_updateFrequency != 0) {
        return;
    }

    // Reset frame counter to prevent overflow
    if (m_frameCounter >= 10000) {
        m_frameCounter = 0;
    }
}

void WeatherEvent::execute() {
    // Mark as triggered
    m_hasTriggered = true;

    // Start cooldown if set
    if (m_cooldownTime > 0.0f) {
        m_onCooldown = true;
        m_cooldownTimer = 0.0f;
    }

    // Begin transition to this weather
    m_inTransition = true;
    m_transitionProgress = 0.0f;

    // Log weather change - actual ParticleManager triggering is done by handlers
    // (events are data carriers, handlers do the work)
    EVENT_INFO(std::format("Weather changing to: {} (Intensity: {:.2f}, Visibility: {:.2f})",
        getWeatherTypeString(), m_params.intensity, m_params.visibility));

    // Play sound effects if specified
    EVENT_INFO_IF(!m_params.soundEffect.empty(),
        std::format("Playing sound effect: {}", m_params.soundEffect));
}

void WeatherEvent::reset() {
    // Base event state
    m_onCooldown = false;
    m_cooldownTimer = 0.0f;
    m_hasTriggered = false;

    // Weather specific state
    m_weatherType = WeatherType::Clear;
    m_customType.clear();
    m_params = WeatherParams{};

    // Conditions
    m_conditions.clear();

    // Time-based parameters
    m_startHour = -1.0f;
    m_endHour = -1.0f;
    m_season = -1;

    // Geographic parameters
    m_regionName.clear();
    m_useGeographicBounds = false;
    m_boundX1 = m_boundY1 = m_boundX2 = m_boundY2 = 0.0f;

    // Transition state
    m_inTransition = false;
    m_transitionProgress = 0.0f;
}

void WeatherEvent::clean() {
    // Clean up any resources specific to this weather event
    m_conditions.clear();

    // Reset time conditions
    m_startHour = -1.0f;
    m_endHour = -1.0f;
    m_season = -1;

    // Reset location conditions
    m_regionName.clear();
    m_useGeographicBounds = false;

    // Reset state
    m_inTransition = false;
    m_transitionProgress = 0.0f;
    m_hasTriggered = false;
}

std::string WeatherEvent::getWeatherTypeString() const {
    if (m_weatherType == WeatherType::Custom) {
        return m_customType;
    }

    switch (m_weatherType) {
        case WeatherType::Clear:
            return "Clear";
        case WeatherType::Cloudy:
            return "Cloudy";
        case WeatherType::Rainy:
            return "Rainy";
        case WeatherType::Stormy:
            return "Stormy";
        case WeatherType::Foggy:
            return "Foggy";
        case WeatherType::Snowy:
            return "Snowy";
        case WeatherType::Windy:
            return "Windy";
        default:
            return "Unknown";
    }
}

void WeatherEvent::setWeatherType(WeatherType type) {
    m_weatherType = type;
    m_customType.clear(); // Clear custom type when setting a standard type
}

void WeatherEvent::setWeatherType(const std::string& weatherTypeStr) {
    if (weatherTypeStr.empty()) {
        m_weatherType = WeatherType::Custom;
        m_customType = weatherTypeStr;
        return;
    }

    switch (weatherTypeStr[0]) {
        case 'C':
            if (weatherTypeStr == "Clear") {
                m_weatherType = WeatherType::Clear;
                break;
            }
            if (weatherTypeStr == "Cloudy") {
                m_weatherType = WeatherType::Cloudy;
                break;
            }
            m_weatherType = WeatherType::Custom;
            m_customType = weatherTypeStr;
            return;
        case 'R':
            if (weatherTypeStr == "Rainy") {
                m_weatherType = WeatherType::Rainy;
                break;
            }
            m_weatherType = WeatherType::Custom;
            m_customType = weatherTypeStr;
            return;
        case 'S':
            if (weatherTypeStr == "Stormy") {
                m_weatherType = WeatherType::Stormy;
                break;
            }
            if (weatherTypeStr == "Snowy") {
                m_weatherType = WeatherType::Snowy;
                break;
            }
            m_weatherType = WeatherType::Custom;
            m_customType = weatherTypeStr;
            return;
        case 'F':
            if (weatherTypeStr == "Foggy") {
                m_weatherType = WeatherType::Foggy;
                break;
            }
            m_weatherType = WeatherType::Custom;
            m_customType = weatherTypeStr;
            return;
        case 'W':
            if (weatherTypeStr == "Windy") {
                m_weatherType = WeatherType::Windy;
                break;
            }
            m_weatherType = WeatherType::Custom;
            m_customType = weatherTypeStr;
            return;
        default:
            m_weatherType = WeatherType::Custom;
            m_customType = weatherTypeStr;
            return;
    }
    m_customType.clear();
}

bool WeatherEvent::checkConditions() {
    // If there are no conditions at all, return false
    if (m_conditions.empty() && !m_useGeographicBounds && m_startHour < 0 &&
        m_season < 0 && m_regionName.empty()) {
        return false;
    }

    // Check custom conditions first - if any fail, return false
    if (!std::all_of(m_conditions.begin(), m_conditions.end(),
            [](const auto& condition) { return condition(); })) {
        return false;
    }

    // If we only have custom conditions (no environmental ones),
    // and all have passed (we've reached this point), return true
    if (!m_conditions.empty() && !m_useGeographicBounds && m_startHour < 0 &&
        m_season < 0 && m_regionName.empty()) {
        return true;
    }

    // Check time condition
    if (m_startHour >= 0 && !checkTimeCondition()) {
        return false;
    }

    // Check location condition
    if ((m_useGeographicBounds || !m_regionName.empty()) &&
        !checkLocationCondition()) {
        return false;
    }

    // Check season if specified
    if (m_season >= 0) {
        try {
            if (m_season != getCurrentSeason()) {
                return false;
            }
        } catch (...) {
            // If GameTime is not available, ignore season check
            EVENT_WARN("GameTime not available for season check - ignoring season condition");
        }
    }

    // All conditions passed
    return true;
}

void WeatherEvent::addTimeCondition(std::function<bool()> condition) {
    // Clear existing conditions first to make tests more predictable
    m_conditions.clear();
    // Add the new condition
    m_conditions.push_back(std::move(condition));
}

void WeatherEvent::addLocationCondition(std::function<bool()> condition) {
    m_conditions.push_back(std::move(condition));
}

void WeatherEvent::addRandomChanceCondition(float probability) {
    // Create a condition that returns true with the given probability
    // Use thread-safe random generation instead of static variables
    m_conditions.push_back([probability]() {
        thread_local std::random_device rd;
        thread_local std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        return dis(gen) < probability;
    });
}

void WeatherEvent::setTimeOfDay(float startHour, float endHour) {
    m_startHour = startHour;
    m_endHour = endHour;
}

void WeatherEvent::setSeasonalEffect(int season) { m_season = season; }

void WeatherEvent::setGeographicRegion(const std::string& regionName) {
    m_regionName = regionName;
}

void WeatherEvent::setBoundingArea(float x1, float y1, float x2, float y2) {
    m_useGeographicBounds = true;
    m_boundX1 = x1;
    m_boundY1 = y1;
    m_boundX2 = x2;
    m_boundY2 = y2;
}

void WeatherEvent::forceWeatherChange(WeatherType, float) {
    // This would typically call into a game system that manages weather
}

void WeatherEvent::forceWeatherChange(const std::string&, float) {
    // This would typically call into a game system that manages weather
}

bool WeatherEvent::checkTimeCondition() const {
    if (m_startHour < 0 || m_endHour < 0) {
        return true; // No time restriction
    }

    float currentHour = getCurrentGameTime();

    if (m_startHour <= m_endHour) {
        // Simple case: start time is before end time
        return currentHour >= m_startHour && currentHour <= m_endHour;
    } else {
        // Wrapping case: start time is after end time (spans midnight)
        return currentHour >= m_startHour || currentHour <= m_endHour;
    }
}

bool WeatherEvent::checkLocationCondition() const {
    if (!m_useGeographicBounds && m_regionName.empty()) {
        return true; // No location restriction
    }

    // Enforce region gating if specified
    if (!m_regionName.empty() && !isInRegion()) {
        return false;
    }

    // Then check bounding area if enabled
    if (m_useGeographicBounds && !isInBounds()) {
        return false;
    }

    return true;
}

bool WeatherEvent::isInRegion() const {
    // No region restriction
    if (m_regionName.empty()) {
        return true;
    }

    // If WorldManager is unavailable, permit by default to avoid false negatives
    // in contexts where the world isn't loaded yet
    bool worldAvailable = false;
    try {
        const auto& worldManager = WorldManager::Instance();
        worldAvailable = worldManager.isInitialized() && worldManager.hasActiveWorld();
    } catch (...) {
        worldAvailable = false;
    }
    if (!worldAvailable) {
        return true;
    }

    // Determine player tile position (current helper returns 0,0)
    Vector2D playerPos = getPlayerPosition();
    int tx = static_cast<int>(playerPos.getX());
    int ty = static_cast<int>(playerPos.getY());

    const auto tileBiome = WorldManager::Instance().getTileBiomeAt(tx, ty);
    if (!tileBiome.has_value()) {
        return false;
    }

    // Map biome to canonical uppercase string
    auto biomeToString = [](VoidLight::Biome b) -> std::string {
        switch (b) {
            case VoidLight::Biome::DESERT: return "DESERT";
            case VoidLight::Biome::FOREST: return "FOREST";
            case VoidLight::Biome::MOUNTAIN: return "MOUNTAIN";
            case VoidLight::Biome::SWAMP: return "SWAMP";
            case VoidLight::Biome::HAUNTED: return "HAUNTED";
            case VoidLight::Biome::CELESTIAL: return "CELESTIAL";
            case VoidLight::Biome::OCEAN: return "OCEAN";
            default: return "";
        }
    };

    std::string currentRegion = biomeToString(*tileBiome);

    // Normalize m_regionName to uppercase for comparison
    std::string desired = m_regionName;
    std::transform(desired.begin(), desired.end(), desired.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    return !currentRegion.empty() && currentRegion == desired;
}

bool WeatherEvent::isInBounds() const {
    if (!m_useGeographicBounds) {
        return true; // No bounds restriction
    }

    Vector2D playerPos = getPlayerPosition();

    return playerPos.getX() >= m_boundX1 && playerPos.getX() <= m_boundX2 &&
        playerPos.getY() >= m_boundY1 && playerPos.getY() <= m_boundY2;
}
