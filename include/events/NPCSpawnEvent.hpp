/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef NPC_SPAWN_EVENT_HPP
#define NPC_SPAWN_EVENT_HPP

/**
 * @file NPCSpawnEvent.hpp
 * @brief Event implementation for NPC spawning based on various conditions
 *
 * NPCSpawnEvent allows the game to spawn NPCs based on:
 * - Player proximity to spawn points or areas
 * - Time-based spawning (day/night cycles, etc.)
 * - Story or quest progression
 * - Random encounters
 */

#include "Event.hpp"
#include "entities/EntityHandle.hpp"
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>
#include "utils/Vector2D.hpp"

// Forward declarations
class Entity;
using EntityPtr = std::shared_ptr<Entity>;
using EntityWeakPtr = std::weak_ptr<Entity>;

struct SpawnParameters {
    std::string npcType; // Type/class of NPC to spawn (or "Random" for random class)
    std::string npcRace; // Race of NPC to spawn (empty = "Human", "Random" for random race)
    std::string npcID; // Optional unique ID for the spawned NPC
    int count{1}; // Number of NPCs to spawn
    float spawnRadius{0.0f}; // Radius around spawn point (0 = exact point)
    bool facingPlayer{false}; // Whether NPCs should face the player when spawned
    float minDistanceApart{0.0f}; // Minimum distance between spawned NPCs

    // Spawn behavior
    bool fadeIn{false}; // Whether NPCs should fade in
    float fadeTime{1.0f}; // Time to fade in (seconds)
    bool playSpawnEffect{false}; // Whether to play spawn visual effect
    std::string spawnEffectID; // Effect to play on spawn
    std::string spawnSoundID; // Sound to play on spawn

    // Lifecycle behavior
    float despawnTime{-1.0f}; // Time until despawn (-1 = never)
    float despawnDistance{-1.0f}; // Distance at which NPC despawns (-1 = never)

    // AI behavior assignment
    std::string aiBehavior; // Single AI behavior for all NPCs
    std::vector<std::string> aiBehaviors; // Multiple behaviors to rotate through

    // Custom properties to set on spawned NPCs
    std::unordered_map<std::string, std::string> properties;

    // Default constructor
    SpawnParameters() = default;

    // Constructor with commonly used parameters
    explicit SpawnParameters(const std::string& type, int count = 1, float radius = 0.0f,
        const std::string& race = "")
        : npcType(type), npcRace(race), count(count), spawnRadius(radius) {}

    // Optional area constraints for spawns
    bool useAreaRect{false};
    float areaMinX{0.0f}, areaMinY{0.0f}, areaMaxX{0.0f}, areaMaxY{0.0f};
    bool useAreaCircle{false};
    float areaCenterX{0.0f}, areaCenterY{0.0f}, areaRadius{0.0f};

    // World-wide spawning (queries WorldManager for bounds)
    bool worldWide{false};
};

class NPCSpawnEvent : public Event {
public:
    NPCSpawnEvent(const std::string& name, const std::string& npcType);
    NPCSpawnEvent(const std::string& name, const SpawnParameters& params);
    ~NPCSpawnEvent() override = default;

    // Core event methods implementation
    void update() override;
    void execute() override;
    void reset() override;
    void clean() override;

    // Event identification
    std::string getName() const override { return m_name; }
    std::string getType() const override { return "NPCSpawn"; }
    std::string getTypeName() const override { return "NPCSpawnEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::NPCSpawn; }

    // Message handling for spawn requests
    void onMessage(const std::string& message) override;

    // Spawn-specific methods
    void setSpawnParameters(const SpawnParameters& params) { m_spawnParams = params; }
    const SpawnParameters& getSpawnParameters() const { return m_spawnParams; }

    // Spawn locations
    void addSpawnPoint(float x, float y);
    void addSpawnPoint(const Vector2D& point);
    void clearSpawnPoints();
    // Access spawn points
    const std::vector<Vector2D>& getSpawnPoints() const { return m_spawnPoints; }
    void setSpawnArea(float x1, float y1, float x2, float y2); // Rectangular area
    void setSpawnArea(float centerX, float centerY, float radius); // Circular area

    // Condition checking
    bool checkConditions() override;

    // Add custom spawn conditions
    void addCondition(std::function<bool()> condition);

    // Proximity trigger
    void setProximityTrigger(float distance);
    bool isPlayerInProximity() const;

    // Time-based triggers
    void setTimeOfDayTrigger(float startHour, float endHour);
    void setRespawnTime(float seconds);
    bool canRespawn() const;

    // Limit number of spawns
    void setMaxSpawnCount(int count) { m_maxSpawnCount = count; }
    int getCurrentSpawnCount() const { return m_currentSpawnCount; }
    int getMaxSpawnCount() const { return m_maxSpawnCount; }

    // Access to spawned entities
    const std::vector<EntityWeakPtr>& getSpawnedEntities() const { return m_spawnedEntities; }
    void clearSpawnedEntities();
    bool areAllEntitiesDead() const;

    // Spawn NPCs - handles "Random" type for random race/class selection
    static EntityHandle spawnNPC(const std::string& npcType, float x, float y);
    static std::vector<EntityHandle> spawnNPCs(const SpawnParameters& params, float x, float y);

    // Area constraint configuration for the spawn event
    void setAreaConstraints(float minX, float minY, float maxX, float maxY) {
        m_constrainToArea = true;
        m_constraintMinX = minX;
        m_constraintMinY = minY;
        m_constraintMaxX = maxX;
        m_constraintMaxY = maxY;
    }

    void enableAreaConstraints(bool enable) { m_constrainToArea = enable; }
    bool hasAreaConstraints() const { return m_constrainToArea; }

private:
    std::string m_name;
    SpawnParameters m_spawnParams;

    // Condition tracking
    std::vector<std::function<bool()>> m_conditions;

    // Spawn locations
    enum class SpawnAreaType {
        Points,
        Rectangle,
        Circle
    };

    SpawnAreaType m_areaType{SpawnAreaType::Points};
    std::vector<Vector2D> m_spawnPoints;
    float m_areaX1{0.0f}, m_areaY1{0.0f}, m_areaX2{0.0f}, m_areaY2{0.0f}; // Rectangle
    Vector2D m_areaCenter{0.0f, 0.0f}; // Circle
    float m_areaRadius{0.0f}; // Circle

    // Proximity trigger
    bool m_useProximityTrigger{false};
    float m_proximityDistance{0.0f};

    // Time triggers
    bool m_useTimeOfDay{false};
    float m_startHour{0.0f};
    float m_endHour{0.0f};

    // Respawn control
    bool m_canRespawn{false};
    float m_respawnTime{0.0f};
    float m_respawnTimer{0.0f};

    // Spawn counting
    int m_maxSpawnCount{-1}; // -1 = unlimited
    int m_currentSpawnCount{0};
    int m_totalSpawned{0};

    // Tracking spawned entities (for counting/statistics only - not ownership)
    std::vector<EntityWeakPtr> m_spawnedEntities;

    // Area constraint system for village/event confinement
    bool m_constrainToArea{false};
    float m_constraintMinX{0.0f}, m_constraintMinY{0.0f};
    float m_constraintMaxX{0.0f}, m_constraintMaxY{0.0f};

    // Helper methods
    bool checkProximityCondition() const;
    bool checkTimeCondition() const;
    bool checkRespawnCondition() const;

    Vector2D getRandomSpawnPosition() const;
    Vector2D getRandomPointInRectangle() const;
    Vector2D getRandomPointInCircle() const;
    Vector2D getRandomPointAroundPoint(const Vector2D& center, float radius) const;

    // Helper to get player position
    Vector2D getPlayerPosition() const;

    // Spawn implementation
    void cleanDeadEntities();

protected:
    // Virtual method for test overrides - base implementation returns nullptr
    virtual EntityPtr spawnSingleNPC(const Vector2D& position);
};

#endif // NPC_SPAWN_EVENT_HPP
