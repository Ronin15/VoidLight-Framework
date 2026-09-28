/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef WORLD_EVENT_HPP
#define WORLD_EVENT_HPP

#include "events/Event.hpp"
#include "utils/Vector2D.hpp"
#include <string>

/**
 * @brief Event types for world-related changes
 */
enum class WorldEventType {
    WorldLoaded, // New world has been loaded
    WorldUnloaded, // World has been unloaded
    TileChanged, // A specific tile has been modified
    WorldGenerated, // World generation completed
    WorldSaved, // World has been saved
    ChunkLoaded, // A chunk of the world has been loaded
    ChunkUnloaded, // A chunk of the world has been unloaded
    StaticCollidersReady // Static collision bodies have been built
};

/**
 * @brief Base class for all world-related events
 */
class WorldEvent : public Event {
public:
    explicit WorldEvent(WorldEventType eventType)
        : Event(), m_eventType(eventType) {}

    ~WorldEvent() override = default;

    WorldEventType getEventType() const { return m_eventType; }

    std::string getTypeName() const override { return "WorldEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::World; }

    // Required Event interface implementations
    void update() override {}
    void execute() override {}
    void clean() override {}
    std::string getName() const override { return getTypeName(); }
    std::string getType() const override { return getTypeName(); }
    bool checkConditions() override { return true; }

    void reset() override {
        Event::resetCooldown();
        m_hasTriggered = false;
        // Keep event type as it's immutable
    }

protected:
    WorldEventType m_eventType;
};

/**
 * @brief Event fired when a world is loaded
 */
class WorldLoadedEvent : public WorldEvent {
public:
    WorldLoadedEvent(const std::string& worldId, int width, int height)
        : WorldEvent(WorldEventType::WorldLoaded), m_worldId(worldId), m_width(width), m_height(height) {}

    const std::string& getWorldId() const { return m_worldId; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    std::string getTypeName() const override { return "WorldLoadedEvent"; }
    std::string getName() const override { return "WorldLoadedEvent"; }
    std::string getType() const override { return "WorldLoadedEvent"; }

    void reset() override {
        WorldEvent::reset();
        m_worldId.clear();
        m_width = 0;
        m_height = 0;
    }

private:
    std::string m_worldId;
    int m_width{0};
    int m_height{0};
};

/**
 * @brief Event fired when a world is unloaded
 */
class WorldUnloadedEvent : public WorldEvent {
public:
    explicit WorldUnloadedEvent(const std::string& worldId)
        : WorldEvent(WorldEventType::WorldUnloaded), m_worldId(worldId) {}

    const std::string& getWorldId() const { return m_worldId; }

    std::string getTypeName() const override { return "WorldUnloadedEvent"; }
    std::string getName() const override { return "WorldUnloadedEvent"; }
    std::string getType() const override { return "WorldUnloadedEvent"; }

    void reset() override {
        WorldEvent::reset();
        m_worldId.clear();
    }

private:
    std::string m_worldId;
};

/**
 * @brief Event fired when a tile in the world changes
 */
class TileChangedEvent : public WorldEvent {
public:
    TileChangedEvent(int x, int y, const std::string& changeType)
        : WorldEvent(WorldEventType::TileChanged), m_position(x, y), m_changeType(changeType) {}

    const Vector2D& getPosition() const { return m_position; }
    int getX() const { return m_position.getX(); }
    int getY() const { return m_position.getY(); }
    const std::string& getChangeType() const { return m_changeType; }

    std::string getTypeName() const override { return "TileChangedEvent"; }
    std::string getName() const override { return "TileChangedEvent"; }
    std::string getType() const override { return "TileChangedEvent"; }

    void reset() override {
        WorldEvent::reset();
        m_position = Vector2D{0, 0};
        m_changeType.clear();
    }

private:
    Vector2D m_position;
    std::string m_changeType;
};

/**
 * @brief Event fired when world generation is completed
 */
class WorldGeneratedEvent : public WorldEvent {
public:
    WorldGeneratedEvent(const std::string& worldId, int width, int height,
        float generationTime)
        : WorldEvent(WorldEventType::WorldGenerated), m_worldId(worldId), m_width(width), m_height(height), m_generationTime(generationTime) {}

    const std::string& getWorldId() const { return m_worldId; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    float getGenerationTime() const { return m_generationTime; }

    std::string getTypeName() const override { return "WorldGeneratedEvent"; }
    std::string getName() const override { return "WorldGeneratedEvent"; }
    std::string getType() const override { return "WorldGeneratedEvent"; }

    void reset() override {
        WorldEvent::reset();
        m_worldId.clear();
        m_width = 0;
        m_height = 0;
        m_generationTime = 0.0f;
    }

private:
    std::string m_worldId;
    int m_width{0};
    int m_height{0};
    float m_generationTime{0.0f};
};

/**
 * @brief Event fired when world is saved
 */
class WorldSavedEvent : public WorldEvent {
public:
    WorldSavedEvent(const std::string& worldId, const std::string& savePath)
        : WorldEvent(WorldEventType::WorldSaved), m_worldId(worldId), m_savePath(savePath) {}

    const std::string& getWorldId() const { return m_worldId; }
    const std::string& getSavePath() const { return m_savePath; }

    std::string getTypeName() const override { return "WorldSavedEvent"; }
    std::string getName() const override { return "WorldSavedEvent"; }
    std::string getType() const override { return "WorldSavedEvent"; }

    void reset() override {
        WorldEvent::reset();
        m_worldId.clear();
        m_savePath.clear();
    }

private:
    std::string m_worldId;
    std::string m_savePath;
};

/**
 * @brief Event fired when static collision bodies have been built for the world
 *
 * This event signals that CollisionManager has finished creating all static
 * collision bodies (buildings, obstacles, triggers) after a world load.
 * Systems that depend on collision data (e.g., PathfinderManager) should wait
 * for this event before building their data structures.
 */
class StaticCollidersReadyEvent : public WorldEvent {
public:
    StaticCollidersReadyEvent(size_t solidBodyCount, size_t triggerCount)
        : WorldEvent(WorldEventType::StaticCollidersReady), m_solidBodyCount(solidBodyCount), m_triggerCount(triggerCount) {}

    size_t getSolidBodyCount() const { return m_solidBodyCount; }
    size_t getTriggerCount() const { return m_triggerCount; }
    size_t getTotalBodyCount() const { return m_solidBodyCount + m_triggerCount; }

    std::string getTypeName() const override { return "StaticCollidersReadyEvent"; }
    std::string getName() const override { return "StaticCollidersReadyEvent"; }
    std::string getType() const override { return "StaticCollidersReadyEvent"; }

    void reset() override {
        WorldEvent::reset();
        m_solidBodyCount = 0;
        m_triggerCount = 0;
    }

private:
    size_t m_solidBodyCount{0};
    size_t m_triggerCount{0};
};

#endif // WORLD_EVENT_HPP
