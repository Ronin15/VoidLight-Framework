/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/EnvironmentModifiers.hpp"
#include "events/TimeEvent.hpp"
#include "events/WeatherEvent.hpp"
#include <algorithm>
#include <cstdint>

static_assert(static_cast<uint8_t>(TimePeriod::Night) == 3,
              "TimePeriod table is indexed Morning=0 .. Night=3");
static_assert(static_cast<uint8_t>(WeatherType::Custom) == 7,
              "WeatherType table is indexed Clear=0 .. Custom=7");

namespace {

struct EnvironmentScaleRow {
    float detection;
    float moveSpeed;
    float caution;
};

constexpr EnvironmentScaleRow kTimeScales[] = {
    {0.90f, 1.00f, 1.00f}, // Morning
    {1.00f, 1.00f, 1.00f}, // Day
    {0.85f, 1.00f, 1.10f}, // Evening
    {0.55f, 0.90f, 1.30f}, // Night
};

constexpr EnvironmentScaleRow kWeatherScales[] = {
    {1.00f, 1.00f, 1.00f}, // Clear
    {0.95f, 1.00f, 1.00f}, // Cloudy
    {0.80f, 0.90f, 1.15f}, // Rainy
    {0.55f, 0.75f, 1.40f}, // Stormy
    {0.45f, 0.95f, 1.25f}, // Foggy
    {0.70f, 0.70f, 1.20f}, // Snowy
    {0.90f, 0.95f, 1.05f}, // Windy
    {1.00f, 1.00f, 1.00f}, // Custom = Clear
};

static_assert(sizeof(kTimeScales) / sizeof(kTimeScales[0]) == 4);
static_assert(sizeof(kWeatherScales) / sizeof(kWeatherScales[0]) == 8);

[[nodiscard]] float clampEnvironmentScale(float value)
{
    return std::clamp(value, kEnvironmentScaleMin, kEnvironmentScaleMax);
}

} // namespace

EnvironmentSnapshot combineEnvironmentScales(TimePeriod period, WeatherType weather,
                                             float visibility)
{
    const auto timeIndex = static_cast<uint8_t>(period);
    const auto weatherIndex = static_cast<uint8_t>(weather);
    const EnvironmentScaleRow& timeScales =
        (timeIndex <= static_cast<uint8_t>(TimePeriod::Night))
            ? kTimeScales[timeIndex]
            : kTimeScales[static_cast<uint8_t>(TimePeriod::Day)];
    const EnvironmentScaleRow& weatherScales =
        (weatherIndex <= static_cast<uint8_t>(WeatherType::Custom))
            ? kWeatherScales[weatherIndex]
            : kWeatherScales[static_cast<uint8_t>(WeatherType::Clear)];

    EnvironmentSnapshot out;
    out.visibility = std::clamp(visibility, 0.0f, 1.0f);
    out.detectionScale = clampEnvironmentScale(timeScales.detection * weatherScales.detection);
    out.moveSpeedScale = clampEnvironmentScale(timeScales.moveSpeed * weatherScales.moveSpeed);
    out.cautionScale = clampEnvironmentScale(timeScales.caution * weatherScales.caution);
    out.detectionScale *= out.visibility;
    return out;
}
