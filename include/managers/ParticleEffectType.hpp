/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PARTICLE_EFFECT_TYPE_HPP
#define PARTICLE_EFFECT_TYPE_HPP

#include <cstdint>

/**
 * @brief Particle effect type enumeration for fast dispatch
 */
enum class ParticleEffectType : uint8_t {
  Rain = 0,
  HeavyRain = 1,
  Snow = 2,
  HeavySnow = 3,
  Fog = 4,
  Cloudy = 5,
  Fire = 6,
  Smoke = 7,
  Sparks = 8,
  Magic = 9,
  Custom = 10,
  Windy = 11,
  WindyDust = 12,
  WindyStorm = 13,
  AmbientDust = 14,
  AmbientFirefly = 15,
  COUNT = 16
};

#endif // PARTICLE_EFFECT_TYPE_HPP
