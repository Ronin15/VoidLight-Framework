/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef WORLD_TRIGGER_EVENT_HPP
#define WORLD_TRIGGER_EVENT_HPP

#include "events/Event.hpp"
#include "events/EventTypeId.hpp"
#include "collisions/TriggerTag.hpp"
#include "entities/EntityHandle.hpp" // for EntityID alias
#include "utils/Vector2D.hpp"

enum class TriggerPhase { Enter = 0, Exit = 1 };

class WorldTriggerEvent : public Event {
public:
  WorldTriggerEvent(EntityID playerId, EntityID triggerId,
                    VoidLight::TriggerTag tag, const Vector2D &position,
                    TriggerPhase phase = TriggerPhase::Enter)
      : m_playerId(playerId), m_triggerId(triggerId), m_tag(tag),
        m_position(position), m_phase(phase) {}

  void update() override {}
  void execute() override {}
  void clean() override {}
  bool checkConditions() override { return true; }
  void reset() override { Event::resetCooldown(); m_consumed = false; }

  std::string getName() const override { return "WorldTriggerEvent"; }
  std::string getType() const override { return "WorldTriggerEvent"; }
  std::string getTypeName() const override { return "WorldTriggerEvent"; }
  EventTypeId getTypeId() const override { return EventTypeId::WorldTrigger; }

  EntityID getPlayerId() const { return m_playerId; }
  EntityID getTriggerId() const { return m_triggerId; }
  VoidLight::TriggerTag getTag() const { return m_tag; }
  const Vector2D &getPosition() const { return m_position; }
  TriggerPhase getPhase() const { return m_phase; }
  bool isConsumed() const { return m_consumed; }
  void setConsumed(bool c) { m_consumed = c; }

private:
  EntityID m_playerId{0};
  EntityID m_triggerId{0};
  VoidLight::TriggerTag m_tag{VoidLight::TriggerTag::None};
  Vector2D m_position{}; // Typically player's contact position (use player center)
  TriggerPhase m_phase{TriggerPhase::Enter};
  bool m_consumed{false};
};

#endif // WORLD_TRIGGER_EVENT_HPP
