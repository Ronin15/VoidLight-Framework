/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef AABB_HPP
#define AABB_HPP

#include "utils/Vector2D.hpp"

namespace VoidLight {

struct AABB {
    Vector2D center;   // world center
    Vector2D halfSize; // half extents (w/2, h/2)

    AABB() = default;
    AABB(float cx, float cy, float hw, float hh) : center(cx, cy), halfSize(hw, hh) {}

    float left() const { return center.getX() - halfSize.getX(); }
    float right() const { return center.getX() + halfSize.getX(); }
    float top() const { return center.getY() - halfSize.getY(); }
    float bottom() const { return center.getY() + halfSize.getY(); }

    bool intersects(const AABB& other) const;
    bool contains(const Vector2D& p) const;
    Vector2D closestPoint(const Vector2D& p) const;
};

} // namespace VoidLight

#endif // AABB_HPP
