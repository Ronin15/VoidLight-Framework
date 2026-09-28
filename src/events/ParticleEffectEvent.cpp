/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "events/ParticleEffectEvent.hpp"
#include "core/Logger.hpp"
#include "managers/ParticleManager.hpp"
#include <format>

ParticleEffectEvent::ParticleEffectEvent(const std::string& name,
    ParticleEffectType effectType,
    const Vector2D& position,
    float intensity, float duration,
    const std::string& groupTag,
    const std::string& soundEffect)
    : m_name(name), m_effectType(effectType), m_position(position), m_intensity(intensity), m_duration(duration), m_groupTag(groupTag), m_soundEffect(soundEffect), m_effectId(0), m_hasExecuted(false) {
    // Set default active state
    setActive(true);
}

ParticleEffectEvent::ParticleEffectEvent(const std::string& name,
    ParticleEffectType effectType, float x,
    float y, float intensity,
    float duration,
    const std::string& groupTag,
    const std::string& soundEffect)
    : ParticleEffectEvent(name, effectType, Vector2D(x, y), intensity, duration,
          groupTag, soundEffect) {
    // Delegating constructor
}

void ParticleEffectEvent::update() {
    // Update cooldown timer if applicable
    updateCooldown(0.016f); // Assume ~60 FPS for cooldown updates
    // Effect lifetime is managed by ParticleManager internally
}

void ParticleEffectEvent::execute() {
    // Check if we should execute (conditions, cooldown, one-time restrictions)
    if (!checkConditions()) {
        return;
    }

    if (isOnCooldown()) {
        return;
    }

    if (isOneTime() && hasTriggered()) {
        return;
    }

    // Mark as executed - actual effect creation is done by handlers
    // (events are data carriers, handlers do the work)
    m_hasExecuted = true;

    // Start cooldown if configured
    if (getCooldown() > 0.0f) {
        startCooldown();
    }

    EVENT_DEBUG(std::format("ParticleEffectEvent '{}' marked as executed at ({}, {})",
        m_name, m_position.getX(), m_position.getY()));
}

void ParticleEffectEvent::reset() {
    // Stop any active effect
    stopEffect();

    // Reset execution state
    m_hasExecuted = false;
    m_effectId = 0;

    // Reset cooldown
    resetCooldown();

    // Clear all effect parameters for pool reuse
    m_effectType = ParticleEffectType::Fire; // Default type
    m_position = Vector2D(0.0f, 0.0f);
    m_intensity = 1.0f;
    m_duration = -1.0f;
    m_groupTag.clear();
    m_soundEffect.clear();
}

void ParticleEffectEvent::clean() {
    // Stop any active effect
    stopEffect();

    // Clean up state
    m_hasExecuted = false;
    m_effectId = 0;

    EVENT_INFO(std::format("ParticleEffectEvent '{}' cleaned up", m_name));
}

bool ParticleEffectEvent::checkConditions() {
    // Basic condition: must be active
    if (!isActive()) {
        return false;
    }

    // Check if ParticleManager is available
    const ParticleManager& particleMgr = ParticleManager::Instance();
    if (!particleMgr.isInitialized() || particleMgr.isShutdown()) {
        return false;
    }

    // Check if effect type is valid
    if (static_cast<uint8_t>(m_effectType) >=
        static_cast<uint8_t>(ParticleEffectType::COUNT)) {
        return false;
    }

    // All conditions met
    return true;
}

void ParticleEffectEvent::stopEffect() {
    if (m_effectId != 0) {
        try {
            ParticleManager& particleMgr = ParticleManager::Instance();
            if (particleMgr.isInitialized() && !particleMgr.isShutdown()) {
                // Try stopping as independent effect first
                if (particleMgr.isIndependentEffect(m_effectId)) {
                    particleMgr.stopIndependentEffect(m_effectId);
                    EVENT_INFO(std::format("Stopped independent particle effect ID: {}",
                        m_effectId));
                } else {
                    // Stop as regular effect
                    particleMgr.stopEffect(m_effectId);
                    EVENT_INFO(std::format("Stopped particle effect ID: {}",
                        m_effectId));
                }
            }
        } catch (const std::exception& e) {
            EVENT_ERROR(std::format("ParticleEffectEvent::stopEffect() - Exception: {}",
                e.what()));
        } catch (...) {
            EVENT_ERROR("ParticleEffectEvent::stopEffect() - Unknown exception");
        }

        m_effectId = 0;
    }
}

void ParticleEffectEvent::setEffectType(ParticleEffectType effectType) {
    m_effectType = effectType;
}

ParticleEffectType ParticleEffectEvent::getEffectType() const {
    return m_effectType;
}

std::string ParticleEffectEvent::getEffectName() const {
    // Create a map for effect type to string conversion since the method is
    // private
    switch (m_effectType) {
        case ParticleEffectType::Rain:
            return "Rain";
        case ParticleEffectType::HeavyRain:
            return "HeavyRain";
        case ParticleEffectType::Snow:
            return "Snow";
        case ParticleEffectType::HeavySnow:
            return "HeavySnow";
        case ParticleEffectType::Fog:
            return "Fog";
        case ParticleEffectType::Cloudy:
            return "Cloudy";
        case ParticleEffectType::Fire:
            return "Fire";
        case ParticleEffectType::Smoke:
            return "Smoke";
        case ParticleEffectType::Sparks:
            return "Sparks";
        case ParticleEffectType::Magic:
            return "Magic";
        case ParticleEffectType::Custom:
            return "Custom";
        default:
            return "Unknown";
    }
}

ParticleEffectType
ParticleEffectEvent::stringToEffectType(const std::string& effectName) {
    if (effectName == "Rain")
        return ParticleEffectType::Rain;
    if (effectName == "HeavyRain")
        return ParticleEffectType::HeavyRain;
    if (effectName == "Snow")
        return ParticleEffectType::Snow;
    if (effectName == "HeavySnow")
        return ParticleEffectType::HeavySnow;
    if (effectName == "Fog")
        return ParticleEffectType::Fog;
    if (effectName == "Cloudy")
        return ParticleEffectType::Cloudy;
    if (effectName == "Fire")
        return ParticleEffectType::Fire;
    if (effectName == "Smoke")
        return ParticleEffectType::Smoke;
    if (effectName == "Sparks")
        return ParticleEffectType::Sparks;
    if (effectName == "Magic")
        return ParticleEffectType::Magic;
    if (effectName == "Custom")
        return ParticleEffectType::Custom;

    // Default fallback
    return ParticleEffectType::Fire;
}
