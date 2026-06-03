/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef EVENT_TYPE_ID_HPP
#define EVENT_TYPE_ID_HPP

#include <cstdint>

// Strongly typed event type enumeration for fast lookups
enum class EventTypeId : uint8_t {
  Weather = 0,
  SceneChange = 1,
  NPCSpawn = 2,
  ParticleEffect = 3,
  ResourceChange = 4,
  World = 5,
  Camera = 6,
  Harvest = 7,
  Collision = 8,          // Reserved legacy ID; non-projectile collisions are not emitted
  WorldTrigger = 9,
  CollisionObstacleChanged = 10,
  Custom = 11,
  Time = 12,
  Combat = 13,
  Entity = 14,            // EntityEvents: Damage, Death, Spawn
  BehaviorMessage = 15,   // Inter-entity behavior messages (RAISE_ALERT, etc.)
  MerchantSpawn = 16,     // Merchant-focused NPC spawning
  COUNT = 17
};

#endif // EVENT_TYPE_ID_HPP
