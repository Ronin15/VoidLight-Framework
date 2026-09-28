/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef COLLISION_OBSTACLE_CHANGED_EVENT_HPP
#define COLLISION_OBSTACLE_CHANGED_EVENT_HPP

#include "events/Event.hpp"
#include "utils/Vector2D.hpp"
#include <string>

/**
 * @brief Event fired when collision obstacles are added, removed, or modified
 * 
 * This event allows other systems (especially PathfinderManager) to be notified
 * when the collision world changes, enabling selective cache invalidation and
 * grid updates instead of full rebuilds.
 */
class CollisionObstacleChangedEvent : public Event {
public:
    enum class ChangeType {
        ADDED,      // New obstacle added
        REMOVED,    // Existing obstacle removed
        MODIFIED    // Existing obstacle properties changed
    };

    /**
     * @brief Constructor for collision obstacle change event
     * @param changeType Type of change (added/removed/modified)
     * @param position World position of the obstacle
     * @param radius Approximate radius of affected area
     * @param description Optional description of the change
     */
    CollisionObstacleChangedEvent(ChangeType changeType,
        const Vector2D& position,
        float radius = 64.0f,
        const std::string& description = "")
        : m_changeType(changeType), m_position(position), m_radius(radius), m_description(description) {}

    /**
     * @brief Gets the type of change
     * @return ChangeType indicating what happened
     */
    ChangeType getChangeType() const { return m_changeType; }

    /**
     * @brief Gets the world position of the change
     * @return Vector2D position in world coordinates
     */
    const Vector2D& getPosition() const { return m_position; }

    /**
     * @brief Gets the radius of the affected area
     * @return Radius in world units
     */
    float getRadius() const { return m_radius; }

    /**
     * @brief Gets the description of the change
     * @return String description (for debugging/logging)
     */
    const std::string& getDescription() const { return m_description; }

    // Setters for pool reuse
    void setChangeType(ChangeType type) { m_changeType = type; }
    void setPosition(const Vector2D& pos) { m_position = pos; }
    void setRadius(float r) { m_radius = r; }
    void setDescription(const std::string& desc) { m_description = desc; }

    /**
     * @brief Configure all fields at once for pool reuse
     */
    void configure(ChangeType type, const Vector2D& pos, float radius, const std::string& desc) {
        m_changeType = type;
        m_position = pos;
        m_radius = radius;
        m_description = desc;
    }

    /**
     * @brief Converts change type to string for logging
     * @param changeType The change type to convert
     * @return String representation
     */
    static std::string changeTypeToString(ChangeType changeType) {
        switch (changeType) {
            case ChangeType::ADDED: return "ADDED";
            case ChangeType::REMOVED: return "REMOVED";
            case ChangeType::MODIFIED: return "MODIFIED";
            default: return "UNKNOWN";
        }
    }

    // Required Event interface implementations
    void update() override {} // No per-frame update needed for this event
    void execute() override {} // Execution is handled by the event system
    void clean() override {} // No cleanup needed
    bool checkConditions() override { return true; } // Always ready to fire

    std::string getName() const override { return "collision_obstacle_changed"; }
    std::string getType() const override { return "CollisionObstacleChanged"; }
    std::string getTypeName() const override { return "CollisionObstacleChangedEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::CollisionObstacleChanged; }

    /**
     * @brief Reset event for reuse in event pools
     */
    void reset() override {
        setActive(false);
        m_changeType = ChangeType::ADDED;
        m_position = Vector2D(0, 0);
        m_radius = 64.0f;
        m_description.clear();
    }

private:
    ChangeType m_changeType;
    Vector2D m_position;
    float m_radius;
    std::string m_description;
};

#endif // COLLISION_OBSTACLE_CHANGED_EVENT_HPP
