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
 * @brief Fired after a directed NPC-faction stance cell, or an NPC faction's
 *        derived relation toward the player, actually changes.
 *
 * Faction ↔ faction (towardPlayer false): from/toward are NPC factions and the
 * old/new cells come from the AIManager stance table.
 * Toward player (towardPlayer true): fromFaction is the NPC faction whose
 * standing-derived relation toward the player crossed a threshold;
 * towardFaction is CharacterData::NO_FACTION (0xFF).
 * settlementId is the current-world settlement containing the incident
 * (0 = wilderness or none), supplied by the emitter.
 * `resetFactionStances()` does not emit this event.
 */
class StanceChangedEvent : public Event {
public:
    StanceChangedEvent(uint8_t fromFaction,
        uint8_t towardFaction,
        FactionStance oldStance,
        FactionStance newStance,
        uint32_t settlementId = 0,
        bool towardPlayer = false)
        : m_fromFaction(fromFaction)
        , m_towardFaction(towardFaction)
        , m_oldStance(oldStance)
        , m_newStance(newStance)
        , m_settlementId(settlementId)
        , m_towardPlayer(towardPlayer) {}

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
        m_towardPlayer = false;
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
    [[nodiscard]] bool isTowardPlayer() const { return m_towardPlayer; }

private:
    uint8_t m_fromFaction{0};
    uint8_t m_towardFaction{0};
    FactionStance m_oldStance{FactionStance::Neutral};
    FactionStance m_newStance{FactionStance::Neutral};
    uint32_t m_settlementId{0};
    bool m_towardPlayer{false};
};

#endif // STANCE_CHANGED_EVENT_HPP
