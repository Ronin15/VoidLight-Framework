/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "events/SceneChangeEvent.hpp"
#include "utils/Vector2D.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <ostream>
#include <format>

// Stream operator for TransitionType (moved from header)
std::ostream& operator<<(std::ostream& os, const TransitionType& type) {
    switch (type) {
        case TransitionType::Fade: os << "Fade"; break;
        case TransitionType::Dissolve: os << "Dissolve"; break;
        case TransitionType::Wipe: os << "Wipe"; break;
        case TransitionType::Slide: os << "Slide"; break;
        case TransitionType::Instant: os << "Instant"; break;
        case TransitionType::Custom: os << "Custom"; break;
        default: os << "Unknown"; break;
    }
    return os;
}

// TransitionParams constructor (moved from header)
TransitionParams::TransitionParams(float durationIn, TransitionType type)
    : duration(durationIn) {
    switch (type) {
        case TransitionType::Fade:
            transitionEffect = "fade";
            break;
        case TransitionType::Dissolve:
            transitionEffect = "dissolve";
            break;
        case TransitionType::Wipe:
            transitionEffect = "wipe";
            break;
        case TransitionType::Slide:
            transitionEffect = "slide";
            break;
        case TransitionType::Instant:
        case TransitionType::Custom:
        default:
            transitionEffect = "fade";
            break;
    }
}

// Helper function to get player position
static Vector2D getPlayerPosition() {
    // This would typically come from the player entity in the game
    // For now, return a placeholder position
    return Vector2D(0.0f, 0.0f);
}

// Helper function to check player input
static bool isKeyPressed(const std::string& /* keyName */) {
    // This would typically check the input system
    // For now, return a placeholder value
    return false;
}

SceneChangeEvent::SceneChangeEvent(const std::string& name, const std::string& targetSceneID)
    : m_name(name), m_targetSceneID(targetSceneID) {

    // Default transition parameters
    m_transitionType = TransitionType::Fade;
    m_transitionParams.duration = 1.0f;
    m_transitionParams.transitionEffect = "fade";
    m_transitionParams.playSound = true;
    m_transitionParams.soundEffect = "scene_transition";
    m_transitionParams.soundVolume = 0.8f;
}

void SceneChangeEvent::update() {
    // Skip update if not active or on cooldown
    if (!m_active || m_onCooldown) {
        return;
    }

    // Update transition if in progress
    if (m_inTransition) {
        m_transitionProgress += 0.016f; // Assume ~60fps for now

        if (m_transitionProgress >= m_transitionParams.duration) {
            m_inTransition = false;
            m_transitionProgress = 0.0f;

            // Transition complete, scene should be fully changed now
            EVENT_INFO(std::format("Scene transition to {} complete", m_targetSceneID));
        }
    }

    // Update timer if active
    if (m_useTimer && m_timerActive) {
        m_timerElapsed += 0.016f; // Assume ~60fps for now
    }

    // Update frame counter for frequency control
    m_frameCounter++;
    if (m_updateFrequency > 1 && m_frameCounter % m_updateFrequency != 0) {
        return;
    }

    // Reset frame counter to prevent overflow
    if (m_frameCounter >= 10000) {
        m_frameCounter = 0;
    }
}

void SceneChangeEvent::execute() {
    // Mark as triggered
    m_hasTriggered = true;

    // Start cooldown if set
    if (m_cooldownTime > 0.0f) {
        m_onCooldown = true;
        m_cooldownTimer = 0.0f;
    }

    // Begin transition to new scene
    m_inTransition = true;
    m_transitionProgress = 0.0f;

    // Log the scene change
    EVENT_INFO(std::format("Changing scene to: {} using transition: {} (duration: {}s)", m_targetSceneID, static_cast<int>(m_transitionType), m_transitionParams.duration));

    // Play transition sound if enabled
    EVENT_INFO_IF(m_transitionParams.playSound && !m_transitionParams.soundEffect.empty(),
        std::format("Playing transition sound: {} at volume: {}", m_transitionParams.soundEffect, m_transitionParams.soundVolume));

    // In a real implementation, this would trigger the actual scene change
    // in the game engine, possibly via the GameStateManager
}

void SceneChangeEvent::reset() {
    // Base event state
    m_onCooldown = false;
    m_cooldownTimer = 0.0f;
    m_hasTriggered = false;

    // Scene change specific state
    m_targetSceneID.clear();
    m_transitionType = TransitionType::Fade;
    m_transitionParams = TransitionParams{};

    // Conditions
    m_conditions.clear();

    // Trigger zone
    m_zoneType = ZoneType::None;
    m_zoneCenter = Vector2D(0, 0);
    m_zoneRadius = 0.0f;
    m_zoneX1 = m_zoneY1 = m_zoneX2 = m_zoneY2 = 0.0f;

    // Input triggers
    m_requirePlayerInput = false;
    m_inputKeyName.clear();

    // Timer
    m_useTimer = false;
    m_timerActive = false;
    m_timerDuration = 0.0f;
    m_timerElapsed = 0.0f;

    // Transition state
    m_inTransition = false;
    m_transitionProgress = 0.0f;
}

void SceneChangeEvent::clean() {
    // Clean up any resources specific to this scene change event
    m_conditions.clear();
}

void SceneChangeEvent::setTransitionType(TransitionType type) {
    m_transitionType = type;

    // Update transition effect based on type
    switch (type) {
        case TransitionType::Fade:
            m_transitionParams.transitionEffect = "fade";
            break;
        case TransitionType::Dissolve:
            m_transitionParams.transitionEffect = "dissolve";
            break;
        case TransitionType::Wipe:
            m_transitionParams.transitionEffect = "wipe";
            break;
        case TransitionType::Slide:
            m_transitionParams.transitionEffect = "slide";
            break;
        case TransitionType::Instant:
            m_transitionParams.transitionEffect = "instant";
            m_transitionParams.duration = 0.0f;
            break;
        case TransitionType::Custom:
            // Keep current effect for custom type
            break;
    }
}

bool SceneChangeEvent::checkConditions() {
    // For demo events, only allow triggering through explicit requests
    // This prevents auto-triggering while still allowing manual triggers
    if (m_name.starts_with("demo_")) {
        return false; // Demo events should only respond to onMessage, not auto-trigger
    }

    // If this is a one-time event that has already triggered, return false
    if (m_oneTimeEvent && m_hasTriggered) {
        return false;
    }

    // Check all custom conditions using STL algorithm
    if (!std::all_of(m_conditions.begin(), m_conditions.end(),
            [](const auto& condition) { return condition(); })) {
        return false;
    }

    // Check trigger zone if specified
    if (m_zoneType != ZoneType::None && !checkZoneCondition()) {
        return false;
    }

    // Check input if required
    if (m_requirePlayerInput && !checkInputCondition()) {
        return false;
    }

    // Check timer if used
    if (m_useTimer && !checkTimerCondition()) {
        return false;
    }

    // All conditions passed
    return true;
}

void SceneChangeEvent::setTriggerZone(float x, float y, float radius) {
    m_zoneType = ZoneType::Circle;
    m_zoneCenter = Vector2D(x, y);
    m_zoneRadius = radius;
}

void SceneChangeEvent::setTriggerZone(float x1, float y1, float x2, float y2) {
    m_zoneType = ZoneType::Rectangle;
    m_zoneX1 = x1;
    m_zoneY1 = y1;
    m_zoneX2 = x2;
    m_zoneY2 = y2;
}

bool SceneChangeEvent::isPlayerInTriggerZone() const {
    return checkZoneCondition();
}

void SceneChangeEvent::addCondition(std::function<bool()> condition) {
    m_conditions.push_back(std::move(condition));
}

bool SceneChangeEvent::isPlayerInputTriggered() const {
    return checkInputCondition();
}

void SceneChangeEvent::setTimerTrigger(float seconds) {
    m_useTimer = true;
    m_timerDuration = seconds;
    m_timerElapsed = 0.0f;
    m_timerActive = false;
}

void SceneChangeEvent::startTimer() {
    if (m_useTimer) {
        m_timerActive = true;
        m_timerElapsed = 0.0f;
    }
}

void SceneChangeEvent::stopTimer() {
    m_timerActive = false;
}

bool SceneChangeEvent::isTimerComplete() const {
    if (!m_useTimer || !m_timerActive) {
        return false;
    }

    return m_timerElapsed >= m_timerDuration;
}

void SceneChangeEvent::forceSceneChange(const std::string&, TransitionType, float) {
    // This would typically call into a game system that manages scenes/states
}

bool SceneChangeEvent::checkZoneCondition() const {
    if (m_zoneType == ZoneType::None) {
        return true; // No zone restriction
    }

    Vector2D playerPos = getPlayerPosition();

    if (m_zoneType == ZoneType::Circle) {
        // Calculate distance from player to zone center
        float dx = playerPos.getX() - m_zoneCenter.getX();
        float dy = playerPos.getY() - m_zoneCenter.getY();
        float distSquared = dx * dx + dy * dy;

        return distSquared <= (m_zoneRadius * m_zoneRadius);
    } else if (m_zoneType == ZoneType::Rectangle) {
        // Check if player is within rectangle bounds
        return playerPos.getX() >= m_zoneX1 && playerPos.getX() <= m_zoneX2 &&
            playerPos.getY() >= m_zoneY1 && playerPos.getY() <= m_zoneY2;
    }

    return false;
}

bool SceneChangeEvent::checkInputCondition() const {
    if (!m_requirePlayerInput) {
        return true; // No input required
    }

    if (m_inputKeyName.empty()) {
        return false; // Input required but no key specified
    }

    return isKeyPressed(m_inputKeyName);
}

bool SceneChangeEvent::checkTimerCondition() const {
    if (!m_useTimer) {
        return true; // No timer condition
    }

    if (!m_timerActive) {
        return false; // Timer not started
    }

    return m_timerElapsed >= m_timerDuration;
}

Vector2D SceneChangeEvent::getPlayerPosition() const {
    // This would typically get the player position from the game state
    // For now, use the helper function
    return ::getPlayerPosition();
}
