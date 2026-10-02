/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef SCARCITY_EVENT_HPP
#define SCARCITY_EVENT_HPP

#include "entities/EntityHandle.hpp"
#include "events/Event.hpp"
#include "utils/ResourceHandle.hpp"
#include "utils/Vector2D.hpp"
#include <cstdint>
#include <string>

/**
 * @brief Fired by HarvestCommit when a depletion leaves few available
 *        harvestables in the surrounding area.
 *
 * Payload: area center (the depleted node's position), area radius, the
 * number of live non-depleted harvestables of any kind still in the area,
 * the depleted node's resource, and the harvester (player or NPC).
 * Emitted once per qualifying depletion; not pooled.
 */
class ScarcityEvent : public Event {
public:
    ScarcityEvent(const Vector2D& center,
        float radius,
        uint16_t availableCount,
        VoidLight::ResourceHandle resource,
        EntityHandle harvester)
        : m_center(center)
        , m_radius(radius)
        , m_availableCount(availableCount)
        , m_resource(resource)
        , m_harvester(harvester) {}

    ~ScarcityEvent() override = default;

    void update() override {}
    void execute() override {}
    void reset() override {
        Event::resetCooldown();
        m_center = Vector2D{0.0f, 0.0f};
        m_radius = 0.0f;
        m_availableCount = 0;
        m_resource = VoidLight::ResourceHandle{};
        m_harvester = EntityHandle{};
    }
    void clean() override {}
    std::string getName() const override { return "Scarcity"; }
    bool checkConditions() override { return true; }
    std::string getType() const override { return EVENT_TYPE; }
    std::string getTypeName() const override { return "ScarcityEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Scarcity; }

    inline static const std::string EVENT_TYPE = "ScarcityEvent";

    [[nodiscard]] const Vector2D& getCenter() const { return m_center; }
    [[nodiscard]] float getRadius() const { return m_radius; }
    [[nodiscard]] uint16_t getAvailableCount() const { return m_availableCount; }
    [[nodiscard]] VoidLight::ResourceHandle getResource() const { return m_resource; }
    [[nodiscard]] EntityHandle getHarvester() const { return m_harvester; }

private:
    Vector2D m_center{0.0f, 0.0f};
    float m_radius{0.0f};
    uint16_t m_availableCount{0};
    VoidLight::ResourceHandle m_resource{};
    EntityHandle m_harvester{};
};

#endif // SCARCITY_EVENT_HPP
