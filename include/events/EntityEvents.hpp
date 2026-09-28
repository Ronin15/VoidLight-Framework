/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef ENTITY_EVENTS_HPP
#define ENTITY_EVENTS_HPP

/**
 * @file EntityEvents.hpp
 * @brief Entity lifecycle and state change events using EntityHandle
 *
 * EntityEvents enable the controller event pattern where controllers
 * fire events for state changes rather than mutating entities directly.
 * Event handlers process events and mutate via EntityDataManager.
 *
 * Event flow:
 * 1. Controller fires DamageEvent with EntityHandle
 * 2. DamageHandler processes: EntityDataManager::getCharacterData().health -= damage
 * 3. If health <= 0, handler fires DeathEvent
 * 4. DeathHandler processes death logic
 *
 * Benefits:
 * - Controllers remain read-only query + event firing
 * - All mutation happens in handlers (single responsibility)
 * - Other systems can subscribe to react to damage/death
 */

#include "Event.hpp"
#include "entities/EntityHandle.hpp"
#include "utils/ResourceHandle.hpp"
#include "utils/Vector2D.hpp"
#include <string>

// Forward declarations
class Entity;

// ============================================================================
// ENTITY EVENT TYPES
// ============================================================================

enum class EntityEventType : uint8_t {
    // Damage events
    DamageIntent, // Request to deal damage (CombatController fires this)
    DamageApplied, // Damage was applied (after handler processes)

    // Death events
    DeathIntent, // Entity about to die
    DeathCompleted, // Entity death processed

    // Spawn events
    SpawnRequest, // Request to spawn entity
    SpawnCompleted, // Entity was spawned

    // State changes
    StateChanged, // Entity state transition
    TierChanged, // Simulation tier changed

    // Crime events
    TheftDetected // Entity stole from another entity
};

// ============================================================================
// DAMAGE EVENT
// ============================================================================

/**
 * @brief Event for damage intent and application
 *
 * Used by CombatController to request damage without direct mutation.
 * DamageHandler processes and mutates via EntityDataManager.
 */
class DamageEvent : public Event {
public:
    /**
     * @brief Default constructor for pool allocation
     */
    DamageEvent()
        : m_name("DamageEvent")
        , m_eventType(EntityEventType::DamageIntent) {
    }

    /**
     * @brief Construct a damage event
     * @param eventType DamageIntent or DamageApplied
     * @param source EntityHandle of damage source (attacker)
     * @param target EntityHandle of damage target
     * @param damage Damage amount
     * @param knockback Optional knockback force
     */
    DamageEvent(EntityEventType eventType, EntityHandle source, EntityHandle target,
        float damage, const Vector2D& knockback = Vector2D(0.0f, 0.0f))
        : m_name("DamageEvent")
        , m_eventType(eventType)
        , m_source(source)
        , m_target(target)
        , m_damage(damage)
        , m_knockback(knockback) {
    }

    ~DamageEvent() override = default;

    /**
     * @brief Configure event for pool reuse
     * @param source EntityHandle of damage source (attacker)
     * @param target EntityHandle of damage target
     * @param damage Damage amount
     * @param knockback Knockback force vector
     */
    void configure(EntityHandle source, EntityHandle target,
        float damage, const Vector2D& knockback) {
        m_eventType = EntityEventType::DamageIntent;
        m_source = source;
        m_target = target;
        m_damage = damage;
        m_knockback = knockback;
        m_remainingHealth = 0.0f;
        m_wasLethal = false;
    }

    // Core event methods (immediate events, minimal implementation)
    void update() override {}
    void execute() override {}
    void reset() override {}
    void clean() override {}

    // Event identification
    std::string getName() const override { return m_name; }
    std::string getType() const override { return "Entity"; }
    std::string getTypeName() const override { return "DamageEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Combat; }

    // Always ready (immediate event)
    bool checkConditions() override { return true; }

    // Damage-specific accessors
    [[nodiscard]] EntityEventType getEntityEventType() const { return m_eventType; }
    [[nodiscard]] EntityHandle getSource() const { return m_source; }
    [[nodiscard]] EntityHandle getTarget() const { return m_target; }
    [[nodiscard]] float getDamage() const { return m_damage; }
    [[nodiscard]] const Vector2D& getKnockback() const { return m_knockback; }
    [[nodiscard]] float getRemainingHealth() const { return m_remainingHealth; }
    [[nodiscard]] bool wasLethal() const { return m_wasLethal; }

    // Set by handler after processing
    void setRemainingHealth(float health) { m_remainingHealth = health; }
    void setWasLethal(bool lethal) { m_wasLethal = lethal; }

private:
    std::string m_name;
    EntityEventType m_eventType;
    EntityHandle m_source;
    EntityHandle m_target;
    float m_damage{0.0f};
    Vector2D m_knockback;
    float m_remainingHealth{0.0f}; // Set after processing
    bool m_wasLethal{false}; // Set after processing
};

// ============================================================================
// DEATH EVENT
// ============================================================================

/**
 * @brief Event for entity death
 *
 * Fired when an entity's health reaches zero.
 * DeathHandler processes cleanup, drops, etc.
 */
class DeathEvent : public Event {
public:
    /**
     * @brief Construct a death event
     * @param eventType DeathIntent or DeathCompleted
     * @param entity EntityHandle of dying entity
     * @param killer EntityHandle of killer (if any)
     */
    DeathEvent(EntityEventType eventType, EntityHandle entity,
        EntityHandle killer = EntityHandle{})
        : m_name("DeathEvent")
        , m_eventType(eventType)
        , m_entity(entity)
        , m_killer(killer) {
    }

    ~DeathEvent() override = default;

    void update() override {}
    void execute() override {}
    void reset() override {}
    void clean() override {}

    std::string getName() const override { return m_name; }
    std::string getType() const override { return "Entity"; }
    std::string getTypeName() const override { return "DeathEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Entity; }

    bool checkConditions() override { return true; }

    [[nodiscard]] EntityEventType getEntityEventType() const { return m_eventType; }
    [[nodiscard]] EntityHandle getEntity() const { return m_entity; }
    [[nodiscard]] EntityHandle getKiller() const { return m_killer; }
    [[nodiscard]] const Vector2D& getDeathPosition() const { return m_deathPosition; }

    void setDeathPosition(const Vector2D& pos) { m_deathPosition = pos; }

private:
    std::string m_name;
    EntityEventType m_eventType;
    EntityHandle m_entity;
    EntityHandle m_killer;
    Vector2D m_deathPosition;
};

// ============================================================================
// SPAWN EVENT
// ============================================================================

/**
 * @brief Event for entity spawning
 *
 * Used to request entity creation through the event system.
 * SpawnHandler processes and creates via EntityDataManager.
 */
class SpawnEvent : public Event {
public:
    /**
     * @brief Construct a spawn event
     * @param eventType SpawnRequest or SpawnCompleted
     * @param kind EntityKind to spawn
     * @param position Spawn position
     */
    SpawnEvent(EntityEventType eventType, EntityKind kind, const Vector2D& position)
        : m_name("SpawnEvent")
        , m_eventType(eventType)
        , m_kind(kind)
        , m_position(position) {
    }

    ~SpawnEvent() override = default;

    void update() override {}
    void execute() override {}
    void reset() override {}
    void clean() override {}

    std::string getName() const override { return m_name; }
    std::string getType() const override { return "Entity"; }
    std::string getTypeName() const override { return "SpawnEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Entity; }

    bool checkConditions() override { return true; }

    [[nodiscard]] EntityEventType getEntityEventType() const { return m_eventType; }
    [[nodiscard]] EntityKind getKind() const { return m_kind; }
    [[nodiscard]] const Vector2D& getPosition() const { return m_position; }
    [[nodiscard]] EntityHandle getSpawnedEntity() const { return m_spawnedEntity; }

    // Set by handler after spawning
    void setSpawnedEntity(EntityHandle handle) { m_spawnedEntity = handle; }

private:
    std::string m_name;
    EntityEventType m_eventType;
    EntityKind m_kind;
    Vector2D m_position;
    EntityHandle m_spawnedEntity; // Set after spawning
};

// ============================================================================
// THEFT EVENT
// ============================================================================

/**
 * @brief Event for theft detection
 *
 * Fired when an entity steals from another entity.
 * Guards can subscribe to respond to theft alerts.
 */
class TheftEvent : public Event {
public:
    /**
     * @brief Construct a theft event
     * @param thief EntityHandle of the thief
     * @param victim EntityHandle of the victim
     * @param stolenItem ResourceHandle of the stolen item
     * @param quantity Number of items stolen
     * @param location World position where theft occurred
     */
    TheftEvent(EntityHandle thief, EntityHandle victim,
        VoidLight::ResourceHandle stolenItem, int quantity,
        const Vector2D& location)
        : m_name("TheftEvent")
        , m_thief(thief)
        , m_victim(victim)
        , m_stolenItem(stolenItem)
        , m_quantity(quantity)
        , m_location(location) {
    }

    ~TheftEvent() override = default;

    void update() override {}
    void execute() override {}
    void reset() override {}
    void clean() override {}

    std::string getName() const override { return m_name; }
    std::string getType() const override { return "Entity"; }
    std::string getTypeName() const override { return "TheftEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Entity; }

    bool checkConditions() override { return true; }

    [[nodiscard]] EntityHandle getThief() const { return m_thief; }
    [[nodiscard]] EntityHandle getVictim() const { return m_victim; }
    [[nodiscard]] VoidLight::ResourceHandle getStolenItem() const { return m_stolenItem; }
    [[nodiscard]] int getQuantity() const { return m_quantity; }
    [[nodiscard]] const Vector2D& getLocation() const { return m_location; }

private:
    std::string m_name;
    EntityHandle m_thief;
    EntityHandle m_victim;
    VoidLight::ResourceHandle m_stolenItem;
    int m_quantity;
    Vector2D m_location;
};

#endif // ENTITY_EVENTS_HPP
