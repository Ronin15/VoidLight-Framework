/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef COLLISION_BODY_HPP
#define COLLISION_BODY_HPP

#include <cstdint>

namespace VoidLight {

// Body type classifications for collision physics
enum class BodyType : uint8_t {
    STATIC,      // Immovable objects (world geometry, buildings)
    KINEMATIC,   // Script-controlled movement (NPCs, moving platforms)
    DYNAMIC      // Physics-simulated (player, projectiles)
};

// Bitmask collision layers (combine via bitwise OR)
enum CollisionLayer : uint32_t {
    Layer_Default = 1u << 0,
    Layer_Player = 1u << 1,
    Layer_Enemy = 1u << 2,
    Layer_Environment = 1u << 3,
    Layer_Projectile = 1u << 4,
    Layer_Trigger = 1u << 5,
    Layer_Pet = 1u << 6,
};

} // namespace VoidLight

#endif // COLLISION_BODY_HPP
