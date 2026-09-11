/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef ENVIRONMENT_MODIFIERS_HPP
#define ENVIRONMENT_MODIFIERS_HPP

#include <cstdint>

enum class TimePeriod : uint8_t;
enum class WeatherType;

inline constexpr float kEnvironmentScaleMin = 0.25f;
inline constexpr float kEnvironmentScaleMax = 1.5f;

struct EnvironmentSnapshot {
    float visibility{1.0f};
    float detectionScale{1.0f};
    float moveSpeedScale{1.0f};
    float cautionScale{1.0f};
};

[[nodiscard]] EnvironmentSnapshot combineEnvironmentScales(
    TimePeriod period, WeatherType weather, float visibility);

[[nodiscard]] inline float applyCautionScale(float value, float cautionScale)
{
    return value * cautionScale;
}

#endif // ENVIRONMENT_MODIFIERS_HPP
