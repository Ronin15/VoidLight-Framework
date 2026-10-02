/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "collisions/AABB.hpp"

namespace VoidLight {

bool AABB::intersects(const AABB& other) const {
    // Use epsilon tolerance for cross-platform floating-point consistency
    const float epsilon = 0.001f;
    if (right() < other.left() - epsilon || other.right() < left() - epsilon) return false;
    if (bottom() < other.top() - epsilon || other.bottom() < top() - epsilon) return false;
    return true;
}

bool AABB::contains(const Vector2D& p) const {
    return p.getX() >= left() && p.getX() <= right() &&
        p.getY() >= top() && p.getY() <= bottom();
}

Vector2D AABB::closestPoint(const Vector2D& p) const {
    float clampedX = p.getX();
    if (clampedX < left()) clampedX = left();
    if (clampedX > right()) clampedX = right();
    float clampedY = p.getY();
    if (clampedY < top()) clampedY = top();
    if (clampedY > bottom()) clampedY = bottom();
    return Vector2D{clampedX, clampedY};
}

} // namespace VoidLight
