/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef EVENT_HPP
#define EVENT_HPP

/**
 * @file Event.hpp
 * @brief Base class for all event types in the game
 *
 * Events represent game occurrences that can be triggered based on various conditions:
 * - Proximity to locations or objects
 * - Time of day or elapsed time
 * - Player actions or state changes
 * - Environmental conditions (weather, etc.)
 * - Quest or storyline progression
 */

#include <string>
#include <memory>
#include "events/EventTypeId.hpp"

// Forward declarations
class Event;

// Define standard smart pointer types for Event
using EventPtr = std::shared_ptr<Event>;
using EventWeakPtr = std::weak_ptr<Event>;

class Event {
public:
    virtual ~Event() = default;

    // Core event methods
    virtual void update() = 0;
    virtual void execute() = 0;
    virtual void reset() = 0;
    virtual void clean() = 0;

    // Event identification
    virtual std::string getName() const = 0;
    virtual std::string getType() const = 0;
    virtual std::string getTypeName() const = 0;
    virtual EventTypeId getTypeId() const = 0;

    // Event state access
    virtual bool isActive() const { return m_active; }
    virtual void setActive(bool active) { m_active = active; }

    // Condition checking
    virtual bool checkConditions() = 0;

    // Frequency control
    virtual void setUpdateFrequency(int framesPerUpdate) { m_updateFrequency = framesPerUpdate; }
    virtual int getUpdateFrequency() const { return m_updateFrequency; }

    // Optional message handling for event communication
    virtual void onMessage([[maybe_unused]] const std::string& message) {}

    // Update control
    virtual bool shouldUpdate() const;

    // Cooldown management
    virtual void setCooldown(float seconds) { m_cooldownTime = seconds; }
    virtual float getCooldown() const { return m_cooldownTime; }
    virtual bool isOnCooldown() const { return m_onCooldown; }
    virtual void startCooldown();
    virtual void resetCooldown() {
        m_onCooldown = false;
        m_cooldownTimer = 0.0f;
    }
    virtual void updateCooldown(float deltaTime);

    // One-time event settings
    virtual bool isOneTime() const { return m_oneTimeEvent; }
    virtual void setOneTime(bool oneTime) { m_oneTimeEvent = oneTime; }
    virtual bool hasTriggered() const { return m_hasTriggered; }

protected:
    bool m_active{true};
    int m_updateFrequency{1}; // How often to update (1 = every frame, 2 = every other frame, etc.)

    // Cooldown system
    bool m_onCooldown{false};
    float m_cooldownTime{0.0f}; // In seconds
    float m_cooldownTimer{0.0f};

    // One-time event tracking
    bool m_oneTimeEvent{false};
    bool m_hasTriggered{false};

    // Frame counter for update frequency
    mutable int m_frameCounter{0};
};

#endif // EVENT_HPP
