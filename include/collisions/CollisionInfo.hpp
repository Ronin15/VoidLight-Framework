/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef COLLISION_INFO_HPP
#define COLLISION_INFO_HPP

#include "entities/EntityHandle.hpp"  // EntityID
#include "utils/Vector2D.hpp"

namespace VoidLight {

struct CollisionInfo {
    EntityID a{0};
    EntityID b{0};
    Vector2D normal{0,0};
    float penetration{0.0f};
    bool trigger{false};

    // EDM-CENTRIC: Index semantics depend on collision type
    // - Movable-movable: both indexA and indexB are EDM indices
    // - Movable-static: indexA is EDM index (movable), indexB is storage index (static)
    size_t indexA{SIZE_MAX};
    size_t indexB{SIZE_MAX};
    bool isMovableMovable{true};  // false = movable-static collision

    // Stamped once during resolution by CollisionManager; avoids repeated isProjectileCollision() calls.
    bool projectileInvolved{false};
};

} // namespace VoidLight

#endif // COLLISION_INFO_HPP
