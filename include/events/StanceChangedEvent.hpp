/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef STANCE_CHANGED_EVENT_HPP
#define STANCE_CHANGED_EVENT_HPP

#include "ai/FactionStance.hpp"
#include "events/Event.hpp"
#include <cstdint>
#include <string>

/**
 * @brief Fired after a directed faction-stance cell actually mutates.
 *
 * Payload is the from/toward pair, the old and new cells, and the first
 * current-world settlement whose faction equals from or toward (0 if none).
 * `resetFactionStances()` does not emit this event.
 */
class StanceChangedEvent : public Event {
public:
    StanceChangedEvent(uint8_t fromFaction,
                       uint8_t towardFaction,
                       FactionStance oldStance,
                       FactionStance newStance,
                       uint32_t settlementId = 0)
        : m_fromFaction(fromFaction)
        , m_towardFaction(towardFaction)
        , m_oldStance(oldStance)
        , m_newStance(newStance)
        , m_settlementId(settlementId) {}

    ~StanceChangedEvent() override = default;

    void update() override {}
    void execute() override {}
    void reset() override {
        Event::resetCooldown();
        m_fromFaction = 0;
        m_towardFaction = 0;
        m_oldStance = FactionStance::Neutral;
        m_newStance = FactionStance::Neutral;
        m_settlementId = 0;
    }
    void clean() override {}
    std::string getName() const override { return "StanceChanged"; }
    bool checkConditions() override { return true; }
    std::string getType() const override { return EVENT_TYPE; }
    std::string getTypeName() const override { return "StanceChangedEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::StanceChanged; }

    inline static const std::string EVENT_TYPE = "StanceChangedEvent";

    [[nodiscard]] uint8_t getFromFaction() const { return m_fromFaction; }
    [[nodiscard]] uint8_t getTowardFaction() const { return m_towardFaction; }
    [[nodiscard]] FactionStance getOldStance() const { return m_oldStance; }
    [[nodiscard]] FactionStance getNewStance() const { return m_newStance; }
    [[nodiscard]] uint32_t getSettlementId() const { return m_settlementId; }

    void set(uint8_t fromFaction,
             uint8_t towardFaction,
             FactionStance oldStance,
             FactionStance newStance,
             uint32_t settlementId) {
        m_fromFaction = fromFaction;
        m_towardFaction = towardFaction;
        m_oldStance = oldStance;
        m_newStance = newStance;
        m_settlementId = settlementId;
    }

private:
    uint8_t m_fromFaction{0};
    uint8_t m_towardFaction{0};
    FactionStance m_oldStance{FactionStance::Neutral};
    FactionStance m_newStance{FactionStance::Neutral};
    uint32_t m_settlementId{0};
};

#endif // STANCE_CHANGED_EVENT_HPP
