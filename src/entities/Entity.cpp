/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "entities/Entity.hpp"
#include "managers/EntityDataManager.hpp"

// ============================================================================
// TRANSFORM ACCESSORS - EntityDataManager is the single source of truth
// Entity must be registered before these are called (see registerWithDataManager)
// ============================================================================

Vector2D Entity::getPosition() const {
    return EntityDataManager::Instance().getTransform(m_handle).position;
}

Vector2D Entity::getPreviousPosition() const {
    return EntityDataManager::Instance().getTransform(m_handle).previousPosition;
}

Vector2D Entity::getVelocity() const {
    return EntityDataManager::Instance().getTransform(m_handle).velocity;
}

Vector2D Entity::getAcceleration() const {
    return EntityDataManager::Instance().getTransform(m_handle).acceleration;
}

bool Entity::isInActiveTier() const {
    if (!m_handle.isValid()) {
        return true; // No handle = legacy entity, assume active for safety
    }
    const auto& hot = EntityDataManager::Instance().getHotData(m_handle);
    return hot.tier == SimulationTier::Active;
}

Vector2D Entity::getInterpolatedPosition(float alpha) const {
    const auto& transform = EntityDataManager::Instance().getTransform(m_handle);
    return Vector2D(
        transform.previousPosition.getX() + (transform.position.getX() - transform.previousPosition.getX()) * alpha,
        transform.previousPosition.getY() + (transform.position.getY() - transform.previousPosition.getY()) * alpha);
}

void Entity::storePositionForInterpolation() {
    auto& transform = EntityDataManager::Instance().getTransform(m_handle);
    transform.previousPosition = transform.position;
}

void Entity::updatePositionFromMovement(const Vector2D& position) {
    EntityDataManager::Instance().getTransform(m_handle).position = position;
}

void Entity::setPosition(const Vector2D& position) {
    auto& transform = EntityDataManager::Instance().getTransform(m_handle);
    transform.position = position;
    transform.previousPosition = position; // Prevents interpolation sliding
}

void Entity::setVelocity(const Vector2D& velocity) {
    EntityDataManager::Instance().getTransform(m_handle).velocity = velocity;
}

void Entity::setAcceleration(const Vector2D& acceleration) {
    EntityDataManager::Instance().getTransform(m_handle).acceleration = acceleration;
}

// ============================================================================
// REGISTRATION (Player class only - NPCs use createNPCWithRaceClass())
// ============================================================================

void Entity::registerWithDataManager(const Vector2D& position, float halfWidth,
    float halfHeight, EntityKind kind) {
    auto& edm = EntityDataManager::Instance();
    if (!edm.isInitialized()) {
        return; // EDM not ready, skip registration
    }

    // Only Player uses Entity class - NPCs are data-driven
    if (kind == EntityKind::Player) {
        EntityHandle handle = edm.registerPlayer(m_id, position, halfWidth, halfHeight);
        setHandle(handle);
    }
}

// ============================================================================
// ANIMATION
// ============================================================================

void Entity::playAnimation(const std::string& animName) {
    auto it = m_animationMap.find(animName);
    if (it != m_animationMap.end()) {
        const auto& config = it->second;
        m_currentRow = config.row + 1; // TextureManager uses 1-based rows
        m_numFrames = config.frameCount;
        m_animSpeed = config.speed;
        m_animationLoops = config.loop;
        m_currentFrame = 0;
    }
}
