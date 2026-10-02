/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef HARVEST_RESOURCE_EVENT_HPP
#define HARVEST_RESOURCE_EVENT_HPP

#include "events/Event.hpp"
#include "utils/Vector2D.hpp"
#include <string>

/**
 * @brief Event fired when an entity attempts to harvest a resource from the world
 * 
 * This event is typically triggered by player or NPC actions and should be handled
 * by the WorldManager to:
 * 1. Update the tile at the target coordinates (e.g., remove a tree)
 * 2. Notify the WorldResourceManager to decrement the corresponding resource count
 * 3. Fire a TileChangedEvent to notify other systems
 */
class HarvestResourceEvent : public Event {
public:
    /**
     * @brief Constructs a harvest resource event
     * @param entityId ID of the entity performing the harvest
     * @param targetX X coordinate of the tile to harvest
     * @param targetY Y coordinate of the tile to harvest
     * @param resourceType Optional hint about what resource is expected
     */
    HarvestResourceEvent(int entityId, int targetX, int targetY,
        const std::string& resourceType = "")
        : Event(), m_entityId(entityId), m_targetPosition(targetX, targetY), m_resourceType(resourceType) {}

    ~HarvestResourceEvent() override = default;

    // Event interface implementation
    void update() override {}
    void execute() override {}
    void clean() override {}
    std::string getName() const override { return "HarvestResource"; }
    std::string getType() const override { return EVENT_TYPE; }
    std::string getTypeName() const override { return "HarvestResourceEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Harvest; }
    bool checkConditions() override { return true; }

    void reset() override {
        Event::resetCooldown();
        m_hasTriggered = false;
        m_entityId = -1;
        m_targetPosition = Vector2D{0, 0};
        m_resourceType.clear();
    }

    // Event data accessors
    int getEntityId() const { return m_entityId; }
    const Vector2D& getTargetPosition() const { return m_targetPosition; }
    int getTargetX() const { return m_targetPosition.getX(); }
    int getTargetY() const { return m_targetPosition.getY(); }
    const std::string& getResourceType() const { return m_resourceType; }

    static const std::string EVENT_TYPE;

private:
    int m_entityId{-1}; // Entity performing the harvest
    Vector2D m_targetPosition; // Target tile coordinates
    std::string m_resourceType; // Optional resource type hint
};

#endif // HARVEST_RESOURCE_EVENT_HPP
