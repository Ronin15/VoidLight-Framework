/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "events/NPCSpawnEvent.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/WorldManager.hpp"
#include "utils/Vector2D.hpp"
#include <algorithm>
#include <format>
#include <random>

// Helper function to get player position
static Vector2D getPlayerPosition() {
  // Try to get player position from current game state
  // For now, return center of screen as placeholder
  return Vector2D(GameEngine::Instance().getWidthInPixels() / 2.0f,
                  GameEngine::Instance().getHeightInPixels() / 2.0f);
}

// Random number generation - use thread_local for thread safety
static thread_local std::mt19937 gen{std::random_device{}()};

// Helper removed: use PathfinderManager::adjustSpawnToNavigable

NPCSpawnEvent::NPCSpawnEvent(const std::string &name,
                             const std::string &npcType)
    : m_name(name) {

  // Initialize with basic parameters
  m_spawnParams.npcType = npcType;
  m_spawnParams.count = 1;
  m_spawnParams.spawnRadius = 0.0f;
}

NPCSpawnEvent::NPCSpawnEvent(const std::string &name,
                             const SpawnParameters &params)
    : m_name(name), m_spawnParams(params) {}

void NPCSpawnEvent::update() {
  // Skip update if not active or on cooldown
  if (!m_active || m_onCooldown) {
    return;
  }

  // Clean up any dead entities from the tracked list
  cleanDeadEntities();

  // Check respawn conditions if applicable
  if (m_canRespawn && areAllEntitiesDead()) {
    m_respawnTimer += 0.016f; // Assume ~60fps for now
  }

  // Update frame counter for frequency control
  m_frameCounter++;
  if (m_updateFrequency > 1 && m_frameCounter % m_updateFrequency != 0) {
    return;
  }

  // Reset frame counter to prevent overflow
  if (m_frameCounter >= 10000) {
    m_frameCounter = 0;
  }
}

void NPCSpawnEvent::execute() {
  // Check spawn count limits
  if (m_maxSpawnCount >= 0 && m_currentSpawnCount >= m_maxSpawnCount) {
    EVENT_INFO(std::format("NPCSpawnEvent: {} - Max spawn count reached ({})",
                           m_name, m_maxSpawnCount));
    return;
  }

  // Mark as triggered
  m_hasTriggered = true;

  // Start cooldown if set
  if (m_cooldownTime > 0.0f) {
    m_onCooldown = true;
    m_cooldownTimer = 0.0f;
  }

  // Determine spawn position from spawn points or default
  float spawnX = 0.0f, spawnY = 0.0f;
  if (!m_spawnPoints.empty()) {
    spawnX = m_spawnPoints[0].getX();
    spawnY = m_spawnPoints[0].getY();
  }

  // Actually spawn the NPCs
  auto handles = spawnNPCs(m_spawnParams, spawnX, spawnY);

  // Update counters
  m_currentSpawnCount += static_cast<int>(handles.size());
  m_totalSpawned += static_cast<int>(handles.size());

  // Reset respawn timer
  m_respawnTimer = 0.0f;
}

void NPCSpawnEvent::reset() {
  // Core event flags/timers
  m_onCooldown = false;
  m_cooldownTimer = 0.0f;
  m_hasTriggered = false;
  m_respawnTimer = 0.0f;

  // Clear dynamic state accumulated during dispatch/use
  // Important when this event is reused from an EventPool
  m_spawnPoints.clear();
  m_areaType = SpawnAreaType::Points;
  m_areaX1 = m_areaY1 = m_areaX2 = m_areaY2 = 0.0f;
  m_areaCenter = Vector2D(0.0f, 0.0f);
  m_areaRadius = 0.0f;
  m_useProximityTrigger = false;
  m_proximityDistance = 0.0f;
  m_useTimeOfDay = false;
  m_startHour = 0.0f;
  m_endHour = 0.0f;
  m_canRespawn = false;
  m_respawnTime = 0.0f;

  // Clear any transient conditions
  m_conditions.clear();

  // Reset area constraints
  m_constrainToArea = false;
  m_constraintMinX = m_constraintMinY = m_constraintMaxX = m_constraintMaxY =
      0.0f;

  // Clear spawned-entity tracking and counters
  clearSpawnedEntities();
  m_currentSpawnCount = 0;
  m_totalSpawned = 0;
}

void NPCSpawnEvent::clean() {
  // Clean up any resources specific to this spawn event
  m_conditions.clear();
  clearSpawnedEntities();
}

void NPCSpawnEvent::onMessage(const std::string &message) {
  // NPCSpawnEvent now serves as event coordination demonstration
  EVENT_INFO(std::format("NPCSpawnEvent received message: {}", message));
  EVENT_INFO("  - Event demonstrates messaging system coordination");
  EVENT_INFO("  - Actual entity management handled by GameStates");
}

void NPCSpawnEvent::addSpawnPoint(float x, float y) {
  addSpawnPoint(Vector2D(x, y));
}

void NPCSpawnEvent::addSpawnPoint(const Vector2D &point) {
  m_areaType = SpawnAreaType::Points;
  m_spawnPoints.push_back(point);
}

void NPCSpawnEvent::clearSpawnPoints() {
  m_spawnPoints.clear();

  // Default to circle around origin if no points defined
  if (m_areaType == SpawnAreaType::Points) {
    m_areaType = SpawnAreaType::Circle;
    m_areaCenter = Vector2D(0, 0);
    m_areaRadius = 10.0f;
  }
}

void NPCSpawnEvent::setSpawnArea(float x1, float y1, float x2, float y2) {
  m_areaType = SpawnAreaType::Rectangle;
  m_areaX1 = x1;
  m_areaY1 = y1;
  m_areaX2 = x2;
  m_areaY2 = y2;
}

void NPCSpawnEvent::setSpawnArea(float centerX, float centerY, float radius) {
  m_areaType = SpawnAreaType::Circle;
  m_areaCenter = Vector2D(centerX, centerY);
  m_areaRadius = radius;
}

bool NPCSpawnEvent::checkConditions() {
  // For demo events, only allow triggering through explicit spawn requests
  // This prevents auto-triggering while still allowing manual triggers
  if (m_name.starts_with("demo_")) {
    return false; // Demo events should only respond to onMessage, not
                  // auto-trigger
  }

  // If this is a one-time event that has already triggered, return false
  if (m_oneTimeEvent && m_hasTriggered) {
    return false;
  }

  // If respawn is enabled, check if all entities are dead and respawn timer
  // elapsed
  if (m_canRespawn) {
    if (!areAllEntitiesDead() || !checkRespawnCondition()) {
      return false;
    }
  }

  // Check max spawn count
  if (m_maxSpawnCount >= 0 && m_currentSpawnCount >= m_maxSpawnCount) {
    return false;
  }

  // Check all custom conditions using STL algorithm
  if (!std::all_of(m_conditions.begin(), m_conditions.end(),
                   [](const auto &condition) { return condition(); })) {
    return false;
  }

  // Check proximity if enabled
  if (m_useProximityTrigger && !checkProximityCondition()) {
    return false;
  }

  // Check time of day if enabled
  if (m_useTimeOfDay && !checkTimeCondition()) {
    return false;
  }

  return true;
}

void NPCSpawnEvent::addCondition(std::function<bool()> condition) {
  m_conditions.push_back(std::move(condition));
}

void NPCSpawnEvent::setProximityTrigger(float distance) {
  m_useProximityTrigger = true;
  m_proximityDistance = distance;
}

bool NPCSpawnEvent::isPlayerInProximity() const {
  return checkProximityCondition();
}

void NPCSpawnEvent::setTimeOfDayTrigger(float startHour, float endHour) {
  m_useTimeOfDay = true;
  m_startHour = startHour;
  m_endHour = endHour;
}

void NPCSpawnEvent::setRespawnTime(float seconds) {
  m_canRespawn = true;
  m_respawnTime = seconds;
  m_respawnTimer = 0.0f;
}

bool NPCSpawnEvent::canRespawn() const {
  return m_canRespawn && areAllEntitiesDead() &&
         m_respawnTimer >= m_respawnTime;
}

void NPCSpawnEvent::clearSpawnedEntities() {
  m_spawnedEntities.clear();
  m_currentSpawnCount = 0;
}

bool NPCSpawnEvent::areAllEntitiesDead() const {
  // If no entities were spawned, consider them "all dead"
  if (m_spawnedEntities.empty()) {
    return true;
  }

  // Check if all spawned entities are gone using STL algorithm
  return std::none_of(
      m_spawnedEntities.begin(), m_spawnedEntities.end(),
      [](const auto &weakEntity) { return weakEntity.lock() != nullptr; });
}

EntityHandle NPCSpawnEvent::spawnNPC(const std::string &npcType, float x,
                                      float y) {
  SpawnParameters params(npcType, 1, 0.0f);
  auto handles = spawnNPCs(params, x, y);
  return handles.empty() ? EntityHandle{} : handles[0];
}

std::vector<EntityHandle>
NPCSpawnEvent::spawnNPCs(const SpawnParameters &params, float x, float y) {
  std::vector<EntityHandle> spawnedHandles;
  spawnedHandles.reserve(static_cast<size_t>(params.count));
  auto &edm = EntityDataManager::Instance();

  // Check if we're spawning random class and/or race
  bool isRandomClass = params.npcType.empty() || params.npcType == "Random";
  bool isRandomRace = params.npcRace == "Random";

  std::vector<std::string> races;
  std::vector<std::string> classes;

  if (isRandomRace) {
    races = edm.getRaceIds();
    if (races.empty()) {
      EVENT_ERROR("No races registered for random NPC spawn");
      return spawnedHandles;
    }
  }

  if (isRandomClass) {
    classes = edm.getClassIds();
    if (classes.empty()) {
      EVENT_ERROR("No classes registered for random NPC spawn");
      return spawnedHandles;
    }
  }

  // Get world bounds if worldWide spawning is enabled
  float worldMinX = 0.0f, worldMinY = 0.0f, worldMaxX = 0.0f, worldMaxY = 0.0f;
  bool useWorldBounds = params.worldWide;
  if (useWorldBounds) {
    if (!WorldManager::Instance().getWorldBounds(worldMinX, worldMinY, worldMaxX,
                                                  worldMaxY)) {
      EVENT_ERROR("Failed to get world bounds for world-wide spawning");
      useWorldBounds = false;
    }
  }

  std::string raceLabel = isRandomRace ? "Random" : (params.npcRace.empty() ? "Human" : params.npcRace);
  std::string classLabel = isRandomClass ? "Random" : params.npcType;
  EVENT_INFO(std::format("Spawning {} {} {} NPCs {}",
                         params.count, raceLabel, classLabel,
                         useWorldBounds ? "across world" : std::format("at ({}, {})", x, y)));

  try {
    // Pre-create distributions for world-wide spawning (outside loop for efficiency)
    std::uniform_real_distribution<float> worldXDist(worldMinX, worldMaxX);
    std::uniform_real_distribution<float> worldYDist(worldMinY, worldMaxY);

    for (int i = 0; i < params.count; ++i) {
      Vector2D spawnPos;

      if (useWorldBounds) {
        // World-wide spawning: random position within world bounds
        spawnPos = Vector2D(worldXDist(gen), worldYDist(gen));
      } else {
        // Original behavior: offset from provided position
        std::uniform_real_distribution<float> offsetDist(-params.spawnRadius,
                                                         params.spawnRadius);
        float offsetX = params.spawnRadius > 0 ? offsetDist(gen) : 0.0f;
        float offsetY = params.spawnRadius > 0 ? offsetDist(gen) : 0.0f;
        spawnPos = Vector2D(x + offsetX, y + offsetY);
      }

      // Adjust to navigable position
      if (params.useAreaRect) {
        spawnPos = PathfinderManager::Instance().adjustSpawnToNavigableInRect(
            spawnPos, VoidLight::TILE_SIZE, VoidLight::TILE_SIZE, 150.0f,
            params.areaMinX, params.areaMinY, params.areaMaxX, params.areaMaxY);
      } else if (params.useAreaCircle) {
        spawnPos = PathfinderManager::Instance().adjustSpawnToNavigableInCircle(
            spawnPos, VoidLight::TILE_SIZE, VoidLight::TILE_SIZE, 150.0f,
            Vector2D(params.areaCenterX, params.areaCenterY),
            params.areaRadius);
      } else {
        spawnPos = PathfinderManager::Instance().adjustSpawnToNavigable(
            spawnPos, VoidLight::TILE_SIZE, VoidLight::TILE_SIZE, 150.0f);
      }

      // Determine race and class for this NPC
      std::string race;
      std::string npcClass;

      if (isRandomRace) {
        std::uniform_int_distribution<size_t> raceDist(0, races.size() - 1);
        race = races[raceDist(gen)];
      } else {
        race = params.npcRace.empty() ? "Human" : params.npcRace;
      }

      if (isRandomClass) {
        std::uniform_int_distribution<size_t> classDist(0, classes.size() - 1);
        npcClass = classes[classDist(gen)];
      } else {
        npcClass = params.npcType;
      }

      EntityHandle handle = edm.createNPCWithRaceClass(spawnPos, race, npcClass);

      if (handle.isValid()) {
        // Assign AI behavior - rotate through list if multiple, otherwise use single
        if (!params.aiBehaviors.empty()) {
          const auto& behavior = params.aiBehaviors[i % params.aiBehaviors.size()];
          AIManager::Instance().assignBehavior(handle, behavior);
        } else if (!params.aiBehavior.empty()) {
          AIManager::Instance().assignBehavior(handle, params.aiBehavior);
        }
        spawnedHandles.push_back(handle);
      }
    }

  } catch (const std::exception &e) {
    EVENT_ERROR(std::format("Exception spawning NPCs: {}", e.what()));
  }

  EVENT_INFO(std::format("  Spawned {} NPCs successfully", spawnedHandles.size()));
  return spawnedHandles;
}

bool NPCSpawnEvent::checkProximityCondition() const {
  if (!m_useProximityTrigger) {
    return true; // No proximity check required
  }

  Vector2D playerPos = getPlayerPosition();
  Vector2D centerPos;

  // Determine center position based on spawn area type
  switch (m_areaType) {
  case SpawnAreaType::Points:
    if (m_spawnPoints.empty()) {
      centerPos = Vector2D(0.0f, 0.0f);
    } else {
      // Use the first point as reference
      centerPos = m_spawnPoints[0];
    }
    break;

  case SpawnAreaType::Rectangle:
    // Use center of rectangle
    centerPos.setX((m_areaX1 + m_areaX2) / 2.0f);
    centerPos.setY((m_areaY1 + m_areaY2) / 2.0f);
    break;

  case SpawnAreaType::Circle:
    centerPos = m_areaCenter;
    break;
  }

  // Calculate distance from player to center
  float dx = playerPos.getX() - centerPos.getX();
  float dy = playerPos.getY() - centerPos.getY();
  float distSquared = dx * dx + dy * dy;

  return distSquared <= (m_proximityDistance * m_proximityDistance);
}

bool NPCSpawnEvent::checkTimeCondition() const {
  if (!m_useTimeOfDay) {
    return true; // No time restriction
  }

  // Get the current game time from the GameTime system
  float currentHour = GameTimeManager::Instance().getGameHour();

  if (m_startHour <= m_endHour) {
    // Simple case: start time is before end time
    return currentHour >= m_startHour && currentHour <= m_endHour;
  } else {
    // Wrapping case: start time is after end time (spans midnight)
    return currentHour >= m_startHour || currentHour <= m_endHour;
  }
}

bool NPCSpawnEvent::checkRespawnCondition() const {
  if (!m_canRespawn) {
    return false; // Respawn not enabled
  }

  return m_respawnTimer >= m_respawnTime;
}

Vector2D NPCSpawnEvent::getRandomSpawnPosition() const {
  switch (m_areaType) {
  case SpawnAreaType::Points:
    if (m_spawnPoints.empty()) {
      // Fallback to origin if no points defined
      return Vector2D(0.0f, 0.0f);
    } else {
      // Pick a random point from the list
      std::uniform_int_distribution<size_t> dist(0, m_spawnPoints.size() - 1);
      size_t index = dist(gen);

      // If spawn radius is set, randomize around the point
      if (m_spawnParams.spawnRadius > 0.0f) {
        return getRandomPointAroundPoint(m_spawnPoints[index],
                                         m_spawnParams.spawnRadius);
      } else {
        return m_spawnPoints[index];
      }
    }

  case SpawnAreaType::Rectangle:
    return getRandomPointInRectangle();

  case SpawnAreaType::Circle:
    return getRandomPointInCircle();
  }

  // Fallback
  return Vector2D(0, 0);
}

Vector2D NPCSpawnEvent::getRandomPointInRectangle() const {
  std::uniform_real_distribution<float> distX(m_areaX1, m_areaX2);
  std::uniform_real_distribution<float> distY(m_areaY1, m_areaY2);

  return Vector2D(distX(gen), distY(gen));
}

Vector2D NPCSpawnEvent::getRandomPointInCircle() const {
  // Generate random angle and distance from center
  std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * 3.14159f);
  std::uniform_real_distribution<float> distRadius(0.0f, m_areaRadius);

  float angle = distAngle(gen);
  float radius = distRadius(gen);

  float x = m_areaCenter.getX() + radius * std::cos(angle);
  float y = m_areaCenter.getY() + radius * std::sin(angle);

  return Vector2D(x, y);
}

Vector2D NPCSpawnEvent::getRandomPointAroundPoint(const Vector2D &center,
                                                  float radius) const {
  // Generate random angle and distance from center
  std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * 3.14159f);
  std::uniform_real_distribution<float> distRadius(0.0f, radius);

  float angle = distAngle(gen);
  float dist = distRadius(gen);

  float x = center.getX() + dist * std::cos(angle);
  float y = center.getY() + dist * std::sin(angle);

  return Vector2D(x, y);
}

Vector2D NPCSpawnEvent::getPlayerPosition() const {
  // This would typically get the player position from the game state
  // For now, use the helper function
  return ::getPlayerPosition();
}

EntityPtr NPCSpawnEvent::spawnSingleNPC(const Vector2D &) {
  // Base implementation returns nullptr - override in test mocks for actual
  // spawning
  return nullptr;
}

void NPCSpawnEvent::cleanDeadEntities() {
  // NPCSpawnEvent no longer tracks entities - just reset counters
  m_spawnedEntities.clear();
  // Keep m_currentSpawnCount for demonstration purposes
}
