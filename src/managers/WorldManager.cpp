/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/WorldManager.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "utils/Camera.hpp"
#include "utils/ResourcePath.hpp"
#include "core/ThreadSystem.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "events/TimeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/TextureManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "utils/JsonReader.hpp"
#include "world/HarvestConfig.hpp"

#include "events/HarvestResourceEvent.hpp"
#include <algorithm>
#include <cmath>
#include <format>

#include "gpu/GPURenderer.hpp"
#include "gpu/SpriteBatch.hpp"

bool WorldManager::init() {
  if (m_initialized.load(std::memory_order_acquire)) {
    WORLD_MANAGER_WARN("WorldManager already initialized");
    return true;
  }

  std::lock_guard<std::shared_mutex> lock(m_worldMutex);

  if (m_initialized.load(std::memory_order_acquire)) {
    return true; // Double-check after acquiring lock
  }

  try {
    m_tileRenderer = std::make_unique<VoidLight::TileRenderer>();
    m_isShutdown = false;
    m_initialized.store(true, std::memory_order_release);
    WORLD_MANAGER_INFO("WorldManager initialized successfully");
    return true;
  } catch (const std::exception &ex) {
    WORLD_MANAGER_ERROR(
        std::format("WorldManager::init - Exception: {}", ex.what()));
    return false;
  }
}

void WorldManager::setupEventHandlers() {
  if (!m_initialized.load(std::memory_order_acquire)) {
    WORLD_MANAGER_ERROR(
        "WorldManager not initialized - cannot setup event handlers");
    return;
  }

  if (!EventManager::Instance().isInitialized()) {
    WORLD_MANAGER_ERROR(
        "EventManager not initialized - cannot setup WorldManager event handlers");
    return;
  }

  if (m_tileRenderer) {
    m_tileRenderer->subscribeToSeasonEvents();
  }
}

void WorldManager::clean() {
  std::lock_guard<std::mutex> operationLock(m_loadMutex);

  if (!m_initialized.load(std::memory_order_acquire) || m_isShutdown) {
    return;
  }

  std::optional<std::string> unloadedWorldId;

  {
    std::lock_guard<std::shared_mutex> lock(m_worldMutex);

    // Unsubscribe TileRenderer from season events before cleanup
    if (m_tileRenderer) {
      m_tileRenderer->unsubscribeFromSeasonEvents();
    }

    unloadedWorldId = unloadWorldLocked();
    m_tileRenderer.reset();
    m_initialized.store(false, std::memory_order_release);
    m_isShutdown = true;
  }

  if (unloadedWorldId) {
    fireWorldUnloadedEvent(*unloadedWorldId);
  }

  WORLD_MANAGER_INFO("WorldManager cleaned up");
}

void WorldManager::prepareForStateTransition() {
  if (!m_initialized.load(std::memory_order_acquire) || m_isShutdown) {
    return;
  }

  // TileRenderer's season handler is persistent — no unsubscribe needed.
}

bool WorldManager::loadNewWorld(
    const VoidLight::WorldGenerationConfig &config,
    const VoidLight::WorldGenerationProgressCallback &progressCallback) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    WORLD_MANAGER_ERROR("WorldManager not initialized");
    return false;
  }

  std::lock_guard<std::mutex> operationLock(m_loadMutex);
  if (m_isShutdown) {
    WORLD_MANAGER_ERROR("WorldManager is shut down");
    return false;
  }

  std::optional<std::string> unloadedWorldId;

  try {
    auto newWorld =
        VoidLight::WorldGenerator::generateWorld(config, progressCallback);
    if (!newWorld) {
      WORLD_MANAGER_ERROR("Failed to generate new world");
      return false;
    }

    {
      std::lock_guard<std::shared_mutex> lock(m_worldMutex);

      // Unload current world if it exists
      WORLD_MANAGER_INFO_IF(
          m_currentWorld,
          std::format("Unloading current world: {}", m_currentWorld->worldId));
      if (m_currentWorld) {
        unloadedWorldId = unloadWorldLocked();
      }
    }

    if (unloadedWorldId) {
      fireWorldUnloadedEvent(*unloadedWorldId);
    }

    {
      std::lock_guard<std::shared_mutex> lock(m_worldMutex);

      // Set new world
      m_currentWorld = std::move(newWorld);

      // Register world with WorldResourceManager and set as active immediately
      // (Must set active BEFORE initializing resources so spatial queries work)
      auto& wrm = WorldResourceManager::Instance();
      wrm.createWorld(m_currentWorld->worldId);
      wrm.setActiveWorld(m_currentWorld->worldId);

      // Initialize world resources based on world data
      initializeWorldResources();

      // Season handler is persistent — survives state transitions.
      // Only wire up on first world load (handler not yet registered).
      if (m_tileRenderer && !m_tileRenderer->isSubscribedToSeasons() &&
          EventManager::Instance().isInitialized()) {
        setupEventHandlers();
      }

      WORLD_MANAGER_INFO(std::format("Successfully loaded new world: {}",
                                     m_currentWorld->worldId));

      // Schedule world loaded event for next frame using ThreadSystem
      // Don't fire event while holding world mutex - use low priority to avoid
      // blocking critical tasks
      std::string worldIdCopy = m_currentWorld->worldId;
      // Schedule world loaded event for next frame using ThreadSystem to avoid
      // deadlocks Use high priority to ensure it executes quickly for tests
      WORLD_MANAGER_INFO(std::format(
          "Enqueuing WorldLoadedEvent task for world: {}", worldIdCopy));
      VoidLight::ThreadSystem::Instance().enqueueTask(
          [worldIdCopy, this]() {
            WORLD_MANAGER_INFO(std::format(
                "Executing WorldLoadedEvent task for world: {}", worldIdCopy));
            fireWorldLoadedEvent(worldIdCopy);
          },
          VoidLight::TaskPriority::High,
          std::format("WorldLoadedEvent_{}", worldIdCopy));
    }

    return true;
  } catch (const std::exception &ex) {
    WORLD_MANAGER_ERROR(
        std::format("WorldManager::loadNewWorld - Exception: {}", ex.what()));
    return false;
  }
}

bool WorldManager::loadWorld(const std::string& /*worldId*/) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    WORLD_MANAGER_ERROR("WorldManager not initialized");
    return false;
  }

  WORLD_MANAGER_WARN(
      "WorldManager::loadWorld - Loading saved worlds not yet implemented");
  return false;
}

void WorldManager::unloadWorld() {
  std::lock_guard<std::mutex> operationLock(m_loadMutex);
  std::optional<std::string> unloadedWorldId;

  // Take exclusive lock to ensure atomic unload operation
  // This prevents render thread from accessing world data during deallocation
  {
    std::lock_guard<std::shared_mutex> lock(m_worldMutex);

    unloadedWorldId = unloadWorldLocked();
  }

  if (unloadedWorldId) {
    fireWorldUnloadedEvent(*unloadedWorldId);
  }
}

std::optional<std::string> WorldManager::unloadWorldLocked() {
  // Internal method - caller must already hold m_worldMutex.
  if (!m_currentWorld) {
    return std::nullopt;
  }

  std::string worldId = m_currentWorld->worldId;
  WORLD_MANAGER_INFO(std::format("Unloading world: {}", worldId));

  // Clear chunk cache to prevent stale textures when new world loads
  // Uses deferred clearing (thread-safe) - actual clear happens on render
  // thread
  if (m_tileRenderer) {
    m_tileRenderer->clearChunkCache();
  }

  WorldResourceManager::Instance().removeWorld(worldId);

  m_currentWorld.reset();
  return worldId;
}

std::optional<VoidLight::Tile> WorldManager::getTileCopyAt(int x, int y) const {
  std::shared_lock<std::shared_mutex> lock(m_worldMutex);

  if (!m_currentWorld || !isValidPosition(x, y)) {
    return std::nullopt;
  }

  return m_currentWorld->grid[y][x];
}

std::optional<VoidLight::Biome> WorldManager::getTileBiomeAt(int x, int y) const {
  std::shared_lock<std::shared_mutex> lock(m_worldMutex);

  if (!m_currentWorld || !isValidPosition(x, y)) {
    return std::nullopt;
  }

  return m_currentWorld->grid[y][x].biome;
}

std::optional<VoidLight::ObstacleType>
WorldManager::getTileObstacleTypeAt(int x, int y) const {
  std::shared_lock<std::shared_mutex> lock(m_worldMutex);

  if (!m_currentWorld || !isValidPosition(x, y)) {
    return std::nullopt;
  }

  return m_currentWorld->grid[y][x].obstacleType;
}

bool WorldManager::isValidPosition(int x, int y) const {
  if (!m_currentWorld) {
    return false;
  }

  const int height = static_cast<int>(m_currentWorld->grid.size());
  const int width =
      height > 0 ? static_cast<int>(m_currentWorld->grid[0].size()) : 0;

  return x >= 0 && x < width && y >= 0 && y < height;
}

std::string WorldManager::getCurrentWorldId() const {
  std::shared_lock<std::shared_mutex> lock(m_worldMutex);
  return m_currentWorld ? m_currentWorld->worldId : "";
}

bool WorldManager::hasActiveWorld() const {
  std::shared_lock<std::shared_mutex> lock(m_worldMutex);
  return m_currentWorld != nullptr;
}

void WorldManager::update() {
  if (!m_initialized.load(std::memory_order_acquire) || !m_currentWorld) {
    return;
  }

  // Currently no per-frame world updates needed
  // This could be extended for dynamic world changes, weather effects, etc.
}

bool WorldManager::handleHarvestResource(int entityId, int targetX,
                                         int targetY) {
  if (!m_initialized.load(std::memory_order_acquire) || !m_currentWorld) {
    WORLD_MANAGER_ERROR("WorldManager not initialized or no active world");
    return false;
  }

  std::lock_guard<std::shared_mutex> lock(m_worldMutex);

  if (!isValidPosition(targetX, targetY)) {
    WORLD_MANAGER_ERROR(
        std::format("Invalid harvest position: ({}, {})", targetX, targetY));
    return false;
  }

  const VoidLight::Tile &tile = m_currentWorld->grid[targetY][targetX];

  if (tile.obstacleType == VoidLight::ObstacleType::NONE) {
    // This is expected for EDM-based harvestables that don't have tile obstacles
    WORLD_MANAGER_DEBUG(std::format(
        "No tile obstacle at position: ({}, {}) - EDM harvestable only", targetX, targetY));
    return false;
  }

  VoidLight::Tile updatedTile = tile;
  updatedTile.obstacleType = VoidLight::ObstacleType::NONE;
  updatedTile.resourceHandle = VoidLight::ResourceHandle{};
  if (!applyTileUpdateLocked(targetX, targetY, updatedTile)) {
    return false;
  }

  // Notify WorldResourceManager about resource depletion
  // This is a placeholder - actual resource tracking would need proper resource
  // handles

  WORLD_MANAGER_INFO(std::format("Resource harvested at ({}, {}) by entity {}",
                                 targetX, targetY, entityId));
  return true;
}

bool WorldManager::updateTile(int x, int y, const VoidLight::Tile &newTile) {
  if (!m_initialized.load(std::memory_order_acquire) || !m_currentWorld) {
    WORLD_MANAGER_ERROR("WorldManager not initialized or no active world");
    return false;
  }

  std::lock_guard<std::shared_mutex> lock(m_worldMutex);
  return applyTileUpdateLocked(x, y, newTile);
}

bool WorldManager::modifyTile(
    int x, int y, const std::function<void(VoidLight::Tile&)>& mutator) {
  if (!m_initialized.load(std::memory_order_acquire) || !m_currentWorld) {
    WORLD_MANAGER_ERROR("WorldManager not initialized or no active world");
    return false;
  }

  std::lock_guard<std::shared_mutex> lock(m_worldMutex);
  if (!isValidPosition(x, y)) {
    WORLD_MANAGER_ERROR(std::format("Invalid tile position: ({}, {})", x, y));
    return false;
  }

  VoidLight::Tile updatedTile = m_currentWorld->grid[y][x];
  mutator(updatedTile);
  return applyTileUpdateLocked(x, y, updatedTile);
}

bool WorldManager::applyTileUpdateLocked(int x, int y,
                                         const VoidLight::Tile &newTile) {
  if (!isValidPosition(x, y)) {
    WORLD_MANAGER_ERROR(std::format("Invalid tile position: ({}, {})", x, y));
    return false;
  }

  const VoidLight::Tile &oldTile = m_currentWorld->grid[y][x];

  // Skip invalidation entirely if tile data hasn't changed
  if (oldTile.biome == newTile.biome &&
      oldTile.obstacleType == newTile.obstacleType &&
      oldTile.decorationType == newTile.decorationType &&
      oldTile.buildingId == newTile.buildingId &&
      oldTile.buildingSize == newTile.buildingSize &&
      oldTile.isTopLeftOfBuilding == newTile.isTopLeftOfBuilding &&
      oldTile.isWater == newTile.isWater) {
    return true;
  }

  const bool hasOverhangChange =
      (oldTile.obstacleType != newTile.obstacleType ||
       oldTile.buildingId != newTile.buildingId ||
       oldTile.buildingSize != newTile.buildingSize ||
       oldTile.isTopLeftOfBuilding != newTile.isTopLeftOfBuilding);

  m_currentWorld->grid[y][x] = newTile;

  if (m_tileRenderer) {
    constexpr int chunkSize = 16;
    constexpr int overhangTiles = 2;

    const int chunkX = x / chunkSize;
    const int chunkY = y / chunkSize;

    m_tileRenderer->invalidateChunk(chunkX, chunkY);

    if (hasOverhangChange) {
      const int localX = x % chunkSize;
      const int localY = y % chunkSize;

      const bool nearLeft = localX < overhangTiles;
      const bool nearRight = localX >= (chunkSize - overhangTiles);
      const bool nearTop = localY < overhangTiles;
      const bool nearBottom = localY >= (chunkSize - overhangTiles);

      if (nearLeft) {
        m_tileRenderer->invalidateChunk(chunkX - 1, chunkY);
      }
      if (nearRight) {
        m_tileRenderer->invalidateChunk(chunkX + 1, chunkY);
      }
      if (nearTop) {
        m_tileRenderer->invalidateChunk(chunkX, chunkY - 1);
      }
      if (nearBottom) {
        m_tileRenderer->invalidateChunk(chunkX, chunkY + 1);
      }
      if (nearLeft && nearTop) {
        m_tileRenderer->invalidateChunk(chunkX - 1, chunkY - 1);
      }
      if (nearRight && nearTop) {
        m_tileRenderer->invalidateChunk(chunkX + 1, chunkY - 1);
      }
      if (nearLeft && nearBottom) {
        m_tileRenderer->invalidateChunk(chunkX - 1, chunkY + 1);
      }
      if (nearRight && nearBottom) {
        m_tileRenderer->invalidateChunk(chunkX + 1, chunkY + 1);
      }
    }
  }

  fireTileChangedEvent(x, y, newTile);
  return true;
}

void WorldManager::fireTileChangedEvent(int x, int y,
                                        const VoidLight::Tile &tile) {
  // Increment world version for change tracking by other systems
  // (PathfinderManager, etc.)
  m_worldVersion.fetch_add(1, std::memory_order_release);

  try {
    // Use tile information to determine change type based on tile properties
    std::string changeType = "tile_modified";
    if (tile.isWater) {
      changeType = "water_tile_changed";
    } else if (tile.biome == VoidLight::Biome::FOREST) {
      changeType = "forest_tile_changed";
    } else if (tile.biome == VoidLight::Biome::MOUNTAIN) {
      changeType = "mountain_tile_changed";
    }

    // Trigger world tile changed through EventManager (no registration)
    const EventManager &eventMgr = EventManager::Instance();
    eventMgr.triggerTileChanged(x, y, changeType,
                                      EventManager::DispatchMode::Deferred);

    WORLD_MANAGER_DEBUG(
        std::format("TileChangedEvent fired for tile at ({}, {})", x, y));
  } catch (const std::exception &ex) {
    WORLD_MANAGER_ERROR(
        std::format("Failed to fire TileChangedEvent: {}", ex.what()));
  }
}

void WorldManager::fireWorldLoadedEvent(const std::string &worldId) {
  // Increment world version when world is loaded (major change for other
  // systems)
  m_worldVersion.fetch_add(1, std::memory_order_release);

  try {
    // Get world dimensions
    int width, height;
    if (!getWorldDimensions(width, height)) {
      width = 0;
      height = 0;
    }

    // Trigger a world loaded event via EventManager (no registration)
    const EventManager &eventMgr = EventManager::Instance();
    eventMgr.triggerWorldLoaded(worldId, width, height,
                                      EventManager::DispatchMode::Deferred);

    WORLD_MANAGER_INFO(std::format(
        "WorldLoadedEvent registered and executed for world: {} ({}x{})",
        worldId, width, height));
  } catch (const std::exception &ex) {
    WORLD_MANAGER_ERROR(
        std::format("Failed to fire WorldLoadedEvent: {}", ex.what()));
  }
}

void WorldManager::fireWorldUnloadedEvent(const std::string &worldId) {
  try {
    // Trigger world unloaded via EventManager (no registration)
    const EventManager &eventMgr = EventManager::Instance();
    eventMgr.triggerWorldUnloaded(worldId,
                                        EventManager::DispatchMode::Immediate);

    WORLD_MANAGER_INFO(
        std::format("WorldUnloadedEvent fired for world: {}", worldId));
  } catch (const std::exception &ex) {
    WORLD_MANAGER_ERROR(
        std::format("Failed to fire WorldUnloadedEvent: {}", ex.what()));
  }
}

bool WorldManager::getWorldDimensions(int &width, int &height) const {
  std::shared_lock<std::shared_mutex> lock(m_worldMutex);

  if (!m_currentWorld || m_currentWorld->grid.empty()) {
    width = 0;
    height = 0;
    return false;
  }

  height = static_cast<int>(m_currentWorld->grid.size());
  width = height > 0 ? static_cast<int>(m_currentWorld->grid[0].size()) : 0;

  return width > 0 && height > 0;
}

bool WorldManager::getWorldBounds(float &minX, float &minY, float &maxX,
                                  float &maxY) const {
  int width, height;
  if (!getWorldDimensions(width, height)) {
    // Set default bounds if no world is loaded
    minX = 0.0f;
    minY = 0.0f;
    maxX = 1000.0f;
    maxY = 1000.0f;
    return false;
  }

  // World bounds in pixel coordinates - convert from tile count to pixels
  minX = 0.0f;
  minY = 0.0f;
  maxX = static_cast<float>(width) *
         VoidLight::TILE_SIZE; // Convert tiles to pixels
  maxY = static_cast<float>(height) *
         VoidLight::TILE_SIZE; // Convert tiles to pixels

  return true;
}

void WorldManager::initializeWorldResources() {
  if (!m_currentWorld || m_currentWorld->grid.empty()) {
    WORLD_MANAGER_WARN("Cannot initialize resources - no world loaded");
    return;
  }

  WORLD_MANAGER_INFO(std::format("Initializing world resources for world: {}",
                                 m_currentWorld->worldId));

  // Get ResourceTemplateManager to access available resources
  const auto &resourceMgr = ResourceTemplateManager::Instance();

  // Count resources to distribute based on biomes and elevation
  int totalTiles = 0;
  int forestTiles = 0;
  int mountainTiles = 0;
  int swampTiles = 0;
  int celestialTiles = 0;
  int highElevationTiles = 0;

  // First pass: count tile types
  for (const auto &row : m_currentWorld->grid) {
    for (const auto &tile : row) {
      if (!tile.isWater) {
        totalTiles++;

        switch (tile.biome) {
        case VoidLight::Biome::FOREST:
          forestTiles++;
          break;
        case VoidLight::Biome::MOUNTAIN:
          mountainTiles++;
          break;
        case VoidLight::Biome::SWAMP:
          swampTiles++;
          break;
        case VoidLight::Biome::CELESTIAL:
          celestialTiles++;
          break;
        default:
          // Desert, Ocean, and Haunted biomes don't affect base resource
          // calculations
          break;
        }

        if (tile.elevation > 0.7f) {
          highElevationTiles++;
        }
      }
    }
  }

  if (totalTiles == 0) {
    WORLD_MANAGER_WARN("No land tiles found for resource initialization");
    return;
  }

  try {
    auto& edm = EntityDataManager::Instance();
    const std::string& worldId = m_currentWorld->worldId;

    // Helper to spawn harvestables AT tiles with matching obstacles
    // This ensures EDM harvestable position = tile obstacle position
    // When harvested, both EDM entity and tile obstacle are updated together
    // EVERY obstacle gets a harvestable for visual/gameplay coherence
    auto spawnHarvestablesAtObstacles = [&](const char* resourceId,
                                            VoidLight::ResourceHandle handle,
                                            VoidLight::ObstacleType targetObstacle,
                                            int yieldMin, int yieldMax,
                                            float respawnTime) {
      if (!handle.isValid()) {
        WORLD_MANAGER_ERROR(std::format("Invalid resource handle for obstacle type {}",
                                        VoidLight::obstacleTypeToString(targetObstacle)));
        return;
      }

      int spawned = 0;
      const size_t gridHeight = m_currentWorld->grid.size();
      if (gridHeight == 0) return;
      const size_t gridWidth = m_currentWorld->grid[0].size();

      // Spawn harvestables at ALL tiles that have the matching obstacle
      for (size_t y = 0; y < gridHeight; ++y) {
        for (size_t x = 0; x < gridWidth; ++x) {
          const auto& tile = m_currentWorld->grid[y][x];
          if (tile.obstacleType != targetObstacle) continue;

          Vector2D pos(static_cast<float>(x) * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f,
                       static_cast<float>(y) * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f);

          auto harvestType = VoidLight::getHarvestTypeForResource(resourceId);
          EntityHandle h = edm.createHarvestable(pos, handle, yieldMin, yieldMax, respawnTime, worldId, harvestType);
          if (h.isValid()) {
            ++spawned;
          }
        }
      }
      WORLD_MANAGER_INFO(std::format("Spawned {} harvestables of {} at {} obstacles",
                                     spawned, handle.toString(),
                                     VoidLight::obstacleTypeToString(targetObstacle)));
    };

    // Helper for biome-based resources (for things without tile obstacles)
    auto spawnHarvestablesInBiome = [&](const char* resourceId,
                                        VoidLight::ResourceHandle handle,
                                        VoidLight::Biome targetBiome,
                                        int count, int yieldMin, int yieldMax,
                                        float respawnTime) {
      if (!handle.isValid()) {
        WORLD_MANAGER_ERROR(std::format("Invalid resource handle for biome {}",
                                        VoidLight::biomeToString(targetBiome)));
        return;
      }
      if (count <= 0) return;

      int spawned = 0;
      const size_t gridHeight = m_currentWorld->grid.size();
      if (gridHeight == 0) return;
      const size_t gridWidth = m_currentWorld->grid[0].size();

      // Distribute harvestables across the world
      for (size_t y = 0; y < gridHeight && spawned < count; ++y) {
        for (size_t x = 0; x < gridWidth && spawned < count; ++x) {
          const auto& tile = m_currentWorld->grid[y][x];
          if (tile.isWater) continue;
          if (tile.biome != targetBiome) continue;
          // Skip tiles that already have obstacles (those are handled by spawnHarvestablesAtObstacles)
          if (tile.obstacleType != VoidLight::ObstacleType::NONE) continue;

          // Skip some tiles for natural distribution (every ~10 tiles)
          if ((x + y * 7) % 10 != 0) continue;

          Vector2D pos(static_cast<float>(x) * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f,
                       static_cast<float>(y) * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f);

          auto harvestType = VoidLight::getHarvestTypeForResource(resourceId);
          EntityHandle h = edm.createHarvestable(pos, handle, yieldMin, yieldMax, respawnTime, worldId, harvestType);
          if (h.isValid()) {
            ++spawned;
          }
        }
      }
      const auto harvestType = VoidLight::getHarvestTypeForResource(resourceId);
      WORLD_MANAGER_INFO(std::format("Spawned {} harvestables of type {} ({}) in {} biome",
                                     spawned, handle.toString(),
                                     VoidLight::harvestTypeToString(harvestType),
                                     VoidLight::biomeToString(targetBiome)));
    };

    // Helper for high-elevation resources
    auto spawnHarvestablesAtElevation = [&](const char* resourceId,
                                            VoidLight::ResourceHandle handle,
                                            float minElevation, int count,
                                            int yieldMin, int yieldMax,
                                            float respawnTime) {
      if (!handle.isValid()) {
        WORLD_MANAGER_ERROR(std::format("Invalid resource handle for elevation >= {}",
                                        minElevation));
        return;
      }
      if (count <= 0) return;

      int spawned = 0;
      const size_t gridHeight = m_currentWorld->grid.size();
      if (gridHeight == 0) return;
      const size_t gridWidth = m_currentWorld->grid[0].size();

      for (size_t y = 0; y < gridHeight && spawned < count; ++y) {
        for (size_t x = 0; x < gridWidth && spawned < count; ++x) {
          const auto& tile = m_currentWorld->grid[y][x];
          if (tile.isWater || tile.elevation < minElevation) continue;
          // Skip tiles with obstacles (those are handled by spawnHarvestablesAtObstacles)
          if (tile.obstacleType != VoidLight::ObstacleType::NONE) continue;

          // Skip some tiles for natural distribution
          if ((x + y * 11) % 12 != 0) continue;

          Vector2D pos(static_cast<float>(x) * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f,
                       static_cast<float>(y) * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f);

          // createHarvestable auto-registers with WRM using worldId
          // Use HarvestConfig to derive the correct HarvestType from resource
          auto harvestType = VoidLight::getHarvestTypeForResource(resourceId);
          EntityHandle h = edm.createHarvestable(pos, handle, yieldMin, yieldMax, respawnTime, worldId, harvestType);
          if (h.isValid()) {
            ++spawned;
          }
        }
      }
      const auto harvestType = VoidLight::getHarvestTypeForResource(resourceId);
      WORLD_MANAGER_INFO(std::format("Spawned {} high-elevation harvestables of type {} ({})",
                                     spawned, handle.toString(),
                                     VoidLight::harvestTypeToString(harvestType)));
    };

    // Basic resources - spawn AT tile obstacles for visual coherence
    // When harvested, both EDM entity and tile obstacle are updated
    // All obstacles of matching type get harvestables (no count limit)
    auto woodHandle = resourceMgr.getHandleById("wood");
    spawnHarvestablesAtObstacles("wood", woodHandle, VoidLight::ObstacleType::TREE, 1, 3, 60.0f);

    auto stoneHandle = resourceMgr.getHandleById("stone");
    spawnHarvestablesAtObstacles("stone", stoneHandle, VoidLight::ObstacleType::ROCK, 1, 3, 90.0f);

    // Ore deposits
    auto ironHandle = resourceMgr.getHandleById("iron_ore");
    spawnHarvestablesAtObstacles("iron_ore", ironHandle, VoidLight::ObstacleType::IRON_DEPOSIT, 2, 5, 90.0f);

    auto goldHandle = resourceMgr.getHandleById("gold_ore");
    spawnHarvestablesAtObstacles("gold_ore", goldHandle, VoidLight::ObstacleType::GOLD_DEPOSIT, 1, 3, 150.0f);

    auto coalHandle = resourceMgr.getHandleById("coal");
    spawnHarvestablesAtObstacles("coal", coalHandle, VoidLight::ObstacleType::COAL_DEPOSIT, 3, 6, 75.0f);

    auto copperHandle = resourceMgr.getHandleById("copper_ore");
    spawnHarvestablesAtObstacles("copper_ore", copperHandle, VoidLight::ObstacleType::COPPER_DEPOSIT, 2, 4, 60.0f);

    auto mithrilHandle = resourceMgr.getHandleById("mithril_ore");
    spawnHarvestablesAtObstacles("mithril_ore", mithrilHandle, VoidLight::ObstacleType::MITHRIL_DEPOSIT, 1, 2, 300.0f);

    auto limestoneHandle = resourceMgr.getHandleById("limestone");
    spawnHarvestablesAtObstacles("limestone", limestoneHandle, VoidLight::ObstacleType::LIMESTONE_DEPOSIT, 2, 4, 120.0f);

    // Gem deposits
    auto emeraldHandle = resourceMgr.getHandleById("rough_emerald");
    spawnHarvestablesAtObstacles("rough_emerald", emeraldHandle, VoidLight::ObstacleType::EMERALD_DEPOSIT, 1, 2, 180.0f);

    auto rubyHandle = resourceMgr.getHandleById("rough_ruby");
    spawnHarvestablesAtObstacles("rough_ruby", rubyHandle, VoidLight::ObstacleType::RUBY_DEPOSIT, 1, 2, 180.0f);

    auto sapphireHandle = resourceMgr.getHandleById("rough_sapphire");
    spawnHarvestablesAtObstacles("rough_sapphire", sapphireHandle, VoidLight::ObstacleType::SAPPHIRE_DEPOSIT, 1, 2, 180.0f);

    auto diamondHandle = resourceMgr.getHandleById("rough_diamond");
    spawnHarvestablesAtObstacles("rough_diamond", diamondHandle, VoidLight::ObstacleType::DIAMOND_DEPOSIT, 1, 1, 360.0f);

    // Rare biome-based resources (no tile obstacles - for resources without visual tiles)
    if (forestTiles > 0) {
      auto enchantedWoodHandle = resourceMgr.getHandleById("enchanted_wood");
      spawnHarvestablesInBiome("enchanted_wood", enchantedWoodHandle, VoidLight::Biome::FOREST,
                               std::max(1, forestTiles / 40), 1, 2, 120.0f);
    }

    if (celestialTiles > 0) {
      auto crystalHandle = resourceMgr.getHandleById("crystal_essence");
      spawnHarvestablesInBiome("crystal_essence", crystalHandle, VoidLight::Biome::CELESTIAL,
                               std::max(1, celestialTiles / 30), 1, 2, 150.0f);
    }

    if (swampTiles > 0) {
      auto voidSilkHandle = resourceMgr.getHandleById("void_silk");
      spawnHarvestablesInBiome("void_silk", voidSilkHandle, VoidLight::Biome::SWAMP,
                               std::max(1, swampTiles / 60), 1, 1, 200.0f);
    }

    // Mountain biome gets extra stone deposits
    if (mountainTiles > 0) {
      auto mountainStoneHandle = resourceMgr.getHandleById("stone");
      spawnHarvestablesInBiome("stone", mountainStoneHandle, VoidLight::Biome::MOUNTAIN,
                               std::max(1, mountainTiles / 25), 2, 5, 90.0f);
    }

    // High elevation resources
    if (highElevationTiles > 0) {
      auto enchantedStoneHandle = resourceMgr.getHandleById("enchanted_stone");
      spawnHarvestablesAtElevation("enchanted_stone", enchantedStoneHandle, 0.7f,
                                   std::max(1, highElevationTiles / 30),
                                   1, 3, 90.0f);
    }

    WORLD_MANAGER_INFO(std::format(
        "World harvestable initialization completed for {} ({} tiles processed)",
        m_currentWorld->worldId, totalTiles));

  } catch (const std::exception &ex) {
    WORLD_MANAGER_ERROR(std::format(
        "Error during world resource initialization: {}", ex.what()));
  }
}

// ============================================================================
// Chunk Cache Management (WorldManager delegates to TileRenderer)
// ============================================================================

void WorldManager::invalidateChunk(int chunkX, int chunkY) {
  if (m_tileRenderer) {
    m_tileRenderer->invalidateChunk(chunkX, chunkY);
  }
}

void WorldManager::clearChunkCache() {
  if (m_tileRenderer) {
    m_tileRenderer->clearChunkCache();
  }
}

// ============================================================================
// Season Management (WorldManager delegates to TileRenderer)
// ============================================================================

void WorldManager::subscribeToSeasonEvents() {
  if (m_tileRenderer) {
    m_tileRenderer->subscribeToSeasonEvents();
  }
}

void WorldManager::unsubscribeFromSeasonEvents() {
  if (m_tileRenderer) {
    m_tileRenderer->unsubscribeFromSeasonEvents();
  }
}

Season WorldManager::getCurrentSeason() const {
  if (m_tileRenderer) {
    return m_tileRenderer->getCurrentSeason();
  }
  return Season::Spring; // Default if no renderer
}

void WorldManager::setCurrentSeason(Season season) {
  if (m_tileRenderer) {
    m_tileRenderer->setCurrentSeason(season);
  }
}

// TileRenderer Implementation

VoidLight::TileRenderer::TileRenderer()
    : m_currentSeason(Season::Spring), m_subscribedToSeasons(false) {
  m_gpuDecoBuffer.reserve(512);
  m_gpuObstacleBuffer.reserve(512);

  // Load world object definitions from JSON
  loadWorldObjects();

  // Get atlas pointer and pre-load source rect coords from JSON
  initAtlasCoords();

  // Initialize cached texture pointers/coords for current season
  updateCachedTextureIDs();
}

void VoidLight::TileRenderer::loadWorldObjects() {
  JsonReader reader;
  if (!reader.loadFromFile(ResourcePath::resolve("res/data/world_objects.json"))) {
    WORLD_MANAGER_WARN(std::format(
        "Could not load world_objects.json: {} - using hardcoded defaults",
        reader.getLastError()));
    return;
  }

  const auto& root = reader.getRoot();
  if (!root.isObject()) {
    WORLD_MANAGER_WARN("world_objects.json root is not an object");
    return;
  }

  // Parse version
  if (root.hasKey("version") && root["version"].isString()) {
    m_worldObjects.version = root["version"].asString();
  }

  // Helper to parse a single object definition from a key-value pair
  auto parseObjectDef = [](const std::string& id, const JsonValue& obj) -> WorldObjectDef {
    WorldObjectDef def;
    def.id = id;
    if (obj.hasKey("name") && obj["name"].isString()) {
      def.name = obj["name"].asString();
    }
    if (obj.hasKey("textureId") && obj["textureId"].isString()) {
      def.textureId = obj["textureId"].asString();
    }
    if (obj.hasKey("seasonal") && obj["seasonal"].isBool()) {
      def.seasonal = obj["seasonal"].asBool();
    }
    if (obj.hasKey("blocking") && obj["blocking"].isBool()) {
      def.blocking = obj["blocking"].asBool();
    }
    if (obj.hasKey("harvestable") && obj["harvestable"].isBool()) {
      def.harvestable = obj["harvestable"].asBool();
    }
    if (obj.hasKey("buildingSize") && obj["buildingSize"].isNumber()) {
      def.buildingSize = obj["buildingSize"].asInt();
    }
    return def;
  };

  // Helper to parse a category (object format: { "key": { ... }, ... })
  auto parseCategory = [&parseObjectDef](const JsonValue& root, const std::string& category,
                                          std::unordered_map<std::string, WorldObjectDef>& target) {
    if (!root.hasKey(category) || !root[category].isObject()) {
      return;
    }
    const auto& catObj = root[category].asObject();
    for (const auto& [key, value] : catObj) {
      if (value.isObject()) {
        target[key] = parseObjectDef(key, value);
      }
    }
  };

  // Parse all categories (object format for tool compatibility)
  parseCategory(root, "biomes", m_worldObjects.biomes);
  parseCategory(root, "obstacles", m_worldObjects.obstacles);
  parseCategory(root, "decorations", m_worldObjects.decorations);
  parseCategory(root, "buildings", m_worldObjects.buildings);

  m_worldObjects.loaded = true;
  WORLD_MANAGER_INFO(std::format(
      "Loaded world_objects.json v{}: {} biomes, {} obstacles, {} decorations, {} buildings",
      m_worldObjects.version,
      m_worldObjects.biomes.size(),
      m_worldObjects.obstacles.size(),
      m_worldObjects.decorations.size(),
      m_worldObjects.buildings.size()));
}

void VoidLight::TileRenderer::updateCachedTextureIDs() {
  if (!m_useAtlas) {
    WORLD_MANAGER_WARN("TileRenderer: atlas texture unavailable in GPU-only mode");
  }
}

void VoidLight::TileRenderer::setCurrentSeason(Season season) {
  if (m_currentSeason != season) {
    m_currentSeason = season;
    updateCachedTextureIDs();
  }
}

void VoidLight::TileRenderer::invalidateChunk(int, int) {
}

void VoidLight::TileRenderer::clearChunkCache() {
  // GPU path renders directly from world data each frame.
}

VoidLight::TileRenderer::~TileRenderer() { unsubscribeFromSeasonEvents(); }

void VoidLight::TileRenderer::initAtlasCoords() {
  auto gpuTex = TextureManager::Instance().getGPUTextureData("atlas");
  if (!gpuTex || !gpuTex->texture) {
    WORLD_MANAGER_INFO("Atlas texture not found for GPU rendering");
    m_useAtlas = false;
    return;
  }

  m_atlasGPUOwner = gpuTex->texture;
  m_atlasGPUPtr = m_atlasGPUOwner.get();

  // Load atlas.json for source rect coordinates
  JsonReader atlasReader;
  if (!atlasReader.loadFromFile(ResourcePath::resolve("res/data/atlas.json"))) {
    WORLD_MANAGER_WARN("Could not load atlas.json - using individual textures");
    m_useAtlas = false;
    return;
  }

  const auto& atlasRoot = atlasReader.getRoot();
  if (!atlasRoot.isObject() || !atlasRoot.hasKey("regions")) {
    WORLD_MANAGER_WARN("Invalid atlas.json format - using individual textures");
    m_useAtlas = false;
    return;
  }

  const auto& regions = atlasRoot["regions"].asObject();

  // Load world_objects.json to get texture IDs for each object type
  JsonReader worldReader;
  if (!worldReader.loadFromFile(ResourcePath::resolve("res/data/world_objects.json"))) {
    WORLD_MANAGER_WARN("Could not load world_objects.json - using individual textures");
    m_useAtlas = false;
    return;
  }

  const auto& worldRoot = worldReader.getRoot();

  // Helper to get coords from atlas regions
  auto getCoords = [&regions](const std::string& id) -> AtlasCoords {
    auto it = regions.find(id);
    if (it == regions.end()) {
      return {.x = 0, .y = 0, .w = 0, .h = 0};
    }
    const auto& r = it->second;
    return {
      .x = static_cast<float>(r["x"].asNumber()),
      .y = static_cast<float>(r["y"].asNumber()),
      .w = static_cast<float>(r["w"].asNumber()),
      .h = static_cast<float>(r["h"].asNumber())
    };
  };

  // Helper to get textureId from world_objects.json
  auto getTextureId = [&worldRoot](const std::string& category, const std::string& key) -> std::string {
    if (!worldRoot.hasKey(category)) return "";
    const auto& cat = worldRoot[category];
    if (!cat.isObject() || !cat.hasKey(key)) return "";
    const auto& obj = cat[key];
    if (!obj.hasKey("textureId")) return "";
    return obj["textureId"].asString();
  };

  // Helper to check if object is seasonal
  auto isSeasonal = [&worldRoot](const std::string& category, const std::string& key) -> bool {
    if (!worldRoot.hasKey(category)) return false;
    const auto& cat = worldRoot[category];
    if (!cat.isObject() || !cat.hasKey(key)) return false;
    const auto& obj = cat[key];
    if (!obj.hasKey("seasonal")) return false;
    return obj["seasonal"].asBool();
  };

  // Season prefixes
  static const char* seasonPrefixes[] = {"spring_", "summer_", "fall_", "winter_"};

  // Pre-load coords for all seasons
  for (int s = 0; s < 4; ++s) {
    const std::string prefix = seasonPrefixes[s];
    auto& coords = m_seasonalCoords[s];

    // Biomes - get textureId from JSON, apply seasonal prefix
    auto loadBiome = [&](AtlasCoords& target, const std::string& key) {
      std::string texId = getTextureId("biomes", key);
      if (texId.empty()) texId = "biome_" + key;  // Fallback
      target = isSeasonal("biomes", key) ? getCoords(prefix + texId) : getCoords(texId);
    };

    loadBiome(coords.biome_default, "default");
    loadBiome(coords.biome_desert, "desert");
    loadBiome(coords.biome_forest, "forest");
    loadBiome(coords.biome_plains, "plains");
    loadBiome(coords.biome_mountain, "mountain");
    loadBiome(coords.biome_swamp, "swamp");
    loadBiome(coords.biome_haunted, "haunted");
    loadBiome(coords.biome_celestial, "celestial");
    loadBiome(coords.biome_ocean, "ocean");

    // Obstacles - get textureId from JSON
    auto loadObstacle = [&](AtlasCoords& target, const std::string& key) {
      std::string texId = getTextureId("obstacles", key);
      if (texId.empty()) texId = "obstacle_" + key;
      target = isSeasonal("obstacles", key) ? getCoords(prefix + texId) : getCoords(texId);
    };

    loadObstacle(coords.obstacle_water, "water");
    loadObstacle(coords.obstacle_tree, "tree");
    loadObstacle(coords.obstacle_rock, "rock");

    // Buildings - get textureId from JSON
    auto loadBuilding = [&](AtlasCoords& target, const std::string& key) {
      std::string texId = getTextureId("buildings", key);
      if (texId.empty()) texId = "building_" + key;
      target = isSeasonal("buildings", key) ? getCoords(prefix + texId) : getCoords(texId);
    };

    loadBuilding(coords.building_hut, "hut");
    loadBuilding(coords.building_house, "house");
    loadBuilding(coords.building_large, "large");
    loadBuilding(coords.building_cityhall, "cityhall");

    // Ore deposits (non-seasonal)
    loadObstacle(coords.obstacle_iron_deposit, "iron_deposit");
    loadObstacle(coords.obstacle_gold_deposit, "gold_deposit");
    loadObstacle(coords.obstacle_copper_deposit, "copper_deposit");
    loadObstacle(coords.obstacle_mithril_deposit, "mithril_deposit");
    loadObstacle(coords.obstacle_limestone_deposit, "limestone_deposit");
    loadObstacle(coords.obstacle_coal_deposit, "coal_deposit");
    // Gem deposits (non-seasonal)
    loadObstacle(coords.obstacle_emerald_deposit, "emerald_deposit");
    loadObstacle(coords.obstacle_ruby_deposit, "ruby_deposit");
    loadObstacle(coords.obstacle_sapphire_deposit, "sapphire_deposit");
    loadObstacle(coords.obstacle_diamond_deposit, "diamond_deposit");

    // Decorations - special handling for seasonal availability
    auto loadDecoration = [&](AtlasCoords& target, const std::string& key) {
      if (!worldRoot.hasKey("decorations")) {
        target = {.x = 0, .y = 0, .w = 0, .h = 0};
        return;
      }
      const auto& decorations = worldRoot["decorations"];
      if (!decorations.hasKey(key)) {
        target = {.x = 0, .y = 0, .w = 0, .h = 0};
        return;
      }
      const auto& obj = decorations[key];

      // Check if this decoration has season restrictions
      if (obj.hasKey("seasons")) {
        const auto& seasons = obj["seasons"].asArray();
        bool availableThisSeason = false;
        for (const auto& seasonVal : seasons) {
          const std::string& seasonName = seasonVal.asString();
          if ((s == 0 && seasonName == "spring") ||
              (s == 1 && seasonName == "summer") ||
              (s == 2 && seasonName == "fall") ||
              (s == 3 && seasonName == "winter")) {
            availableThisSeason = true;
            break;
          }
        }
        if (!availableThisSeason) {
          target = {.x = 0, .y = 0, .w = 0, .h = 0};
          return;
        }
      }

      std::string texId = obj.hasKey("textureId") ? obj["textureId"].asString() : key;

      // Check for fallback seasons
      if (obj.hasKey("fallbackSeasons")) {
        const auto& fallbacks = obj["fallbackSeasons"];
        const char* seasonNames[] = {"spring", "summer", "fall", "winter"};
        if (fallbacks.hasKey(seasonNames[s])) {
          texId = fallbacks[seasonNames[s]].asString();
          target = getCoords(texId);
          return;
        }
      }

      // Check for seasonal textures override
      if (obj.hasKey("seasonTextures")) {
        const auto& seasonTex = obj["seasonTextures"];
        const char* seasonNames[] = {"spring", "summer", "fall", "winter"};
        if (seasonTex.hasKey(seasonNames[s])) {
          texId = seasonTex[seasonNames[s]].asString();
          target = getCoords(texId);
          return;
        }
      }

      // Apply seasonal prefix if marked seasonal
      bool seasonal = obj.hasKey("seasonal") && obj["seasonal"].asBool();
      target = seasonal ? getCoords(prefix + texId) : getCoords(texId);
    };

    loadDecoration(coords.decoration_flower_blue, "flower_blue");
    loadDecoration(coords.decoration_flower_pink, "flower_pink");
    loadDecoration(coords.decoration_flower_white, "flower_white");
    loadDecoration(coords.decoration_flower_yellow, "flower_yellow");
    loadDecoration(coords.decoration_mushroom_purple, "mushroom_purple");
    loadDecoration(coords.decoration_mushroom_tan, "mushroom_tan");
    loadDecoration(coords.decoration_grass_small, "grass_small");
    loadDecoration(coords.decoration_grass_large, "grass_large");
    loadDecoration(coords.decoration_bush, "bush");
    loadDecoration(coords.decoration_stump_small, "stump_small");
    loadDecoration(coords.decoration_stump_medium, "stump_medium");
    loadDecoration(coords.decoration_rock_small, "rock_small");
    loadDecoration(coords.decoration_dead_log_hz, "dead_log_hz");
    loadDecoration(coords.decoration_dead_log_vertical, "dead_log_vertical");
    loadDecoration(coords.decoration_lily_pad, "lily_pad");
    loadDecoration(coords.decoration_water_flower, "water_flower");
  }

  m_useAtlas = true;
  WORLD_MANAGER_INFO("TileRenderer: Atlas coords loaded from world_objects.json");
}

void VoidLight::TileRenderer::subscribeToSeasonEvents() {
  if (m_subscribedToSeasons) {
    unsubscribeFromSeasonEvents();
  }

  auto &eventMgr = EventManager::Instance();
  m_seasonToken = eventMgr.registerPersistentHandlerWithToken(
      EventTypeId::Time,
      [this](const EventData &data) { onSeasonChange(data); });
  m_subscribedToSeasons = true;

  // Initialize with current season from GameTime
  m_currentSeason = GameTimeManager::Instance().getSeason();
  WORLD_MANAGER_INFO(std::format(
      "TileRenderer subscribed to season events, current season: {}",
      GameTimeManager::Instance().getSeasonName()));
}

void VoidLight::TileRenderer::unsubscribeFromSeasonEvents() {
  if (!m_subscribedToSeasons) {
    return;
  }

  auto &eventMgr = EventManager::Instance();
  eventMgr.removeHandler(m_seasonToken);
  m_subscribedToSeasons = false;
  WORLD_MANAGER_DEBUG("TileRenderer unsubscribed from season events");
}

void VoidLight::TileRenderer::onSeasonChange(const EventData &data) {
  if (!data.event) {
    return;
  }

  // Match TimeController pattern - use static_pointer_cast + getTimeEventType()
  const auto timeEvent = std::static_pointer_cast<TimeEvent>(data.event);
  if (timeEvent->getTimeEventType() != TimeEventType::SeasonChanged) {
    return;
  }

  const auto seasonEvent =
      std::static_pointer_cast<SeasonChangedEvent>(data.event);
  const Season newSeason = seasonEvent->getSeason();
  if (newSeason != m_currentSeason) {
    WORLD_MANAGER_INFO(std::format("TileRenderer: Season changed to {}",
                                   seasonEvent->getSeasonName()));
    setCurrentSeason(newSeason); // This updates m_currentSeason AND refreshes
                                 // cached texture IDs
  }
}

VoidLight::GPUTexture*
VoidLight::TileRenderer::getAtlasGPUTexture() const {
  if (!m_useAtlas) {
    return nullptr;
  }
  // Get GPU texture from TextureManager
  if (!m_atlasGPUPtr) {
    if (auto atlasData = TextureManager::Instance().getGPUTextureData("atlas");
        atlasData && atlasData->texture) {
      const_cast<TileRenderer*>(this)->m_atlasGPUOwner = atlasData->texture;
      const_cast<TileRenderer*>(this)->m_atlasGPUPtr =
          const_cast<TileRenderer*>(this)->m_atlasGPUOwner.get();
    }
  }
  return m_atlasGPUPtr;
}

void VoidLight::TileRenderer::recordGPUTiles(
    SpriteBatch& spriteBatch, float cameraX, float cameraY,
    float viewportWidth, float viewportHeight, float zoom,
    Season season) {

  // GPU rendering doesn't require chunk grid (m_gridInitialized)
  // It only requires atlas coords to be loaded (m_useAtlas)
  if (!m_useAtlas) {
    return;
  }

  WorldManager::Instance().withWorldDataRead([&](const VoidLight::WorldData* worldData) {
    if (!worldData || worldData->grid.empty()) {
      return;
    }

    // 2D grid dimensions
    const int worldHeight = static_cast<int>(worldData->grid.size());
    const int worldWidth = static_cast<int>(worldData->grid[0].size());

    // Calculate visible tile range (with padding for partially visible tiles)
    const float effectiveViewWidth = viewportWidth / zoom;
    const float effectiveViewHeight = viewportHeight / zoom;

    const int startTileX = std::max(0, static_cast<int>(cameraX / TILE_SIZE) - VIEWPORT_PADDING);
    const int startTileY = std::max(0, static_cast<int>(cameraY / TILE_SIZE) - VIEWPORT_PADDING);
    const int endTileX = std::min(worldWidth,
                                  static_cast<int>((cameraX + effectiveViewWidth) / TILE_SIZE) + VIEWPORT_PADDING + 1);
    const int endTileY = std::min(worldHeight,
                                  static_cast<int>((cameraY + effectiveViewHeight) / TILE_SIZE) + VIEWPORT_PADDING + 1);

    // Early exit if no visible tiles
    if (startTileX >= endTileX || startTileY >= endTileY) {
      return;
    }

    // Pre-fetch seasonal coords (avoids per-tile lookups)
    const auto& sc = m_seasonalCoords[static_cast<int>(season)];

    // Render at 1x scale.
    // Zoom is handled in the composite shader, not by scaling tile positions
    const float scaledTileSize = TILE_SIZE;  // 1x scale, no zoom multiplier
    const float baseCameraX = cameraX;       // No zoom multiplier
    const float baseCameraY = cameraY;
    const float startScreenX = startTileX * scaledTileSize - baseCameraX;

  // Debug: Log tile rendering params on viewport change (uses member vars to avoid static)
  if (viewportWidth != m_lastGPUViewportW || viewportHeight != m_lastGPUViewportH) {
    WORLD_MANAGER_INFO(std::format(
        "GPU tile params: viewport={}x{}, zoom={}, tileSize={}, "
        "scaledTileSize={}, visibleTiles={}x{}, cameraY={}, effectiveH={}, endTileY={}",
        viewportWidth, viewportHeight, zoom, TILE_SIZE,
        scaledTileSize, endTileX - startTileX, endTileY - startTileY,
        cameraY, effectiveViewHeight, endTileY));
    m_lastGPUViewportW = viewportWidth;
    m_lastGPUViewportH = viewportHeight;
  }

  // Build biome lookup table for O(1) access (enum value -> coords pointer)
  // Avoids switch overhead in inner loop
  const AtlasCoords* biomeLUT[8] = {
      &sc.biome_desert, &sc.biome_forest, &sc.biome_plains, &sc.biome_mountain,
      &sc.biome_swamp, &sc.biome_haunted, &sc.biome_celestial, &sc.biome_ocean
  };

  // Build obstacle lookup table (enum value -> coords pointer)
  // Index 0 = NONE (unused), indices 1-14 match ObstacleType enum values
  const AtlasCoords* obstacleLUT[15] = {
      &sc.biome_default,          // NONE (never used)
      &sc.obstacle_rock,          // ROCK
      &sc.obstacle_tree,          // TREE
      &sc.obstacle_water,         // WATER
      &sc.building_hut,           // BUILDING
      &sc.obstacle_iron_deposit,  // IRON_DEPOSIT
      &sc.obstacle_gold_deposit,  // GOLD_DEPOSIT
      &sc.obstacle_copper_deposit, // COPPER_DEPOSIT
      &sc.obstacle_mithril_deposit, // MITHRIL_DEPOSIT
      &sc.obstacle_limestone_deposit, // LIMESTONE_DEPOSIT
      &sc.obstacle_coal_deposit,  // COAL_DEPOSIT
      &sc.obstacle_emerald_deposit, // EMERALD_DEPOSIT
      &sc.obstacle_ruby_deposit,  // RUBY_DEPOSIT
      &sc.obstacle_sapphire_deposit, // SAPPHIRE_DEPOSIT
      &sc.obstacle_diamond_deposit // DIAMOND_DEPOSIT
  };

  // Build decoration lookup table (enum value -> coords pointer)
  // Index 0 = NONE (null), indices 1-16 match DecorationType enum values
  const AtlasCoords* decorationLUT[17] = {
      nullptr,                          // NONE
      &sc.decoration_flower_blue,       // FLOWER_BLUE
      &sc.decoration_flower_pink,       // FLOWER_PINK
      &sc.decoration_flower_white,      // FLOWER_WHITE
      &sc.decoration_flower_yellow,     // FLOWER_YELLOW
      &sc.decoration_mushroom_purple,   // MUSHROOM_PURPLE
      &sc.decoration_mushroom_tan,      // MUSHROOM_TAN
      &sc.decoration_grass_small,       // GRASS_SMALL
      &sc.decoration_grass_large,       // GRASS_LARGE
      &sc.decoration_bush,              // BUSH
      &sc.decoration_stump_small,       // STUMP_SMALL
      &sc.decoration_stump_medium,      // STUMP_MEDIUM
      &sc.decoration_rock_small,        // ROCK_SMALL
      &sc.decoration_dead_log_hz,       // DEAD_LOG_HZ
      &sc.decoration_dead_log_vertical, // DEAD_LOG_VERTICAL
      &sc.decoration_lily_pad,          // LILY_PAD
      &sc.decoration_water_flower       // WATER_FLOWER
  };

  // Clear reusable member buffers (avoids per-frame allocations)
  m_gpuDecoBuffer.clear();
  m_gpuObstacleBuffer.clear();

  // PASS 1: Render biomes, collect decorations + obstacles
  for (int tileY = startTileY; tileY < endTileY; ++tileY) {
    const auto& row = worldData->grid[tileY];
    const float screenY = tileY * scaledTileSize - baseCameraY;
    float screenX = startScreenX;

    for (int tileX = startTileX; tileX < endTileX; ++tileX) {
      const auto& tile = row[tileX];

      // Draw biome tile immediately
      const AtlasCoords* biomeCoords = tile.isWater
          ? &sc.obstacle_water
          : biomeLUT[static_cast<int>(tile.biome)];

      spriteBatch.draw(
          biomeCoords->x, biomeCoords->y, biomeCoords->w, biomeCoords->h,
          screenX, screenY, scaledTileSize, scaledTileSize);

      // Collect decoration (if any, not blocked by non-water obstacle)
      const auto decoIdx = static_cast<size_t>(tile.decorationType);
      if (decoIdx != 0 &&
          (tile.obstacleType == ObstacleType::NONE ||
           tile.obstacleType == ObstacleType::WATER)) {
        const AtlasCoords* decoCoords = (decoIdx < 17) ? decorationLUT[decoIdx] : nullptr;
        if (decoCoords && decoCoords->w > 0) {
          const float offsetX = (scaledTileSize - decoCoords->w) * 0.5f;
          const float offsetY = scaledTileSize - decoCoords->h;
          m_gpuDecoBuffer.push_back({
              screenX + offsetX, screenY + offsetY,
              static_cast<float>(decoCoords->x), static_cast<float>(decoCoords->y),
              static_cast<float>(decoCoords->w), static_cast<float>(decoCoords->h),
              static_cast<float>(decoCoords->w), static_cast<float>(decoCoords->h)
          });
        }
      }

      // Collect obstacle for Y-sorting
      if (tile.obstacleType != ObstacleType::NONE &&
          tile.obstacleType != ObstacleType::WATER) {

        // Handle buildings specially to preserve current world draw semantics.
        if (tile.obstacleType == ObstacleType::BUILDING) {
          if (tile.isTopLeftOfBuilding) {
            // Get building coords by size
            const AtlasCoords* buildingCoords = nullptr;
            switch (tile.buildingSize) {
              case 0:
              case 1: buildingCoords = &sc.building_hut; break;
              case 2: buildingCoords = &sc.building_house; break;
              case 3: buildingCoords = &sc.building_large; break;
              case 4: buildingCoords = &sc.building_cityhall; break;
              default: buildingCoords = &sc.building_hut; break;
            }

            if (buildingCoords && buildingCoords->w > 0) {
              const float spriteW = static_cast<float>(buildingCoords->w);
              const float spriteH = static_cast<float>(buildingCoords->h);
              // Buildings render at tile position without offsets.
              // sortY = bottom of building (tile.buildingSize tiles down)
              const float sortY = screenY + (tile.buildingSize * scaledTileSize);

              m_gpuObstacleBuffer.push_back({
                  {screenX, screenY,
                   static_cast<float>(buildingCoords->x), static_cast<float>(buildingCoords->y),
                   static_cast<float>(buildingCoords->w), static_cast<float>(buildingCoords->h),
                   spriteW, spriteH},
                  sortY
              });
            }
          }
          // Skip to next tile (don't render non-top-left building tiles)
          screenX += scaledTileSize;
          continue;
        }

        // Regular obstacles
        const auto obstacleIdx = static_cast<int>(tile.obstacleType);
        const AtlasCoords* obstacleCoords = obstacleLUT[obstacleIdx];

        if (obstacleCoords && obstacleCoords->w > 0) {
          const float spriteW = static_cast<float>(obstacleCoords->w);
          const float spriteH = static_cast<float>(obstacleCoords->h);
          const float offsetX = (scaledTileSize - spriteW) * 0.5f;
          const float offsetY = scaledTileSize - spriteH;

          m_gpuObstacleBuffer.push_back({
              {screenX + offsetX, screenY + offsetY,
               static_cast<float>(obstacleCoords->x), static_cast<float>(obstacleCoords->y),
               static_cast<float>(obstacleCoords->w), static_cast<float>(obstacleCoords->h),
               spriteW, spriteH},
              screenY + scaledTileSize  // sortY = tile bottom
          });
        }
      }

      screenX += scaledTileSize;
    }
  }

  // PASS 2: Render collected decorations, then sorted obstacles

  // Decorations (no sorting needed, just deferred to render after all biomes)
  for (const auto& deco : m_gpuDecoBuffer) {
    spriteBatch.draw(
        deco.srcX, deco.srcY, deco.srcW, deco.srcH,
        deco.screenX, deco.screenY, deco.dstW, deco.dstH);
  }

  // Obstacles (Y-sorted for correct overlap)
  if (!m_gpuObstacleBuffer.empty()) {
    std::sort(m_gpuObstacleBuffer.begin(), m_gpuObstacleBuffer.end(),
              [](const GPUYSortedSprite& a, const GPUYSortedSprite& b) {
                if (a.sortY != b.sortY) return a.sortY < b.sortY;
                if (a.screenX != b.screenX) return a.screenX < b.screenX;
                return a.screenY < b.screenY;
              });

    for (const auto& obs : m_gpuObstacleBuffer) {
      spriteBatch.draw(
          obs.srcX, obs.srcY, obs.srcW, obs.srcH,
          obs.screenX, obs.screenY, obs.dstW, obs.dstH);
    }
  }
    // Batch end() is called by GPUSceneRecorder, not here
  });
}

// WorldManager GPU method - delegates to TileRenderer
void WorldManager::recordGPU(VoidLight::SpriteBatch& spriteBatch,
                             float cameraX, float cameraY,
                             float viewWidth, float viewHeight, float zoom) {
  if (!m_initialized.load(std::memory_order_acquire) || !m_renderingEnabled) {
    return;
  }

  if (!m_currentWorld || !m_tileRenderer) {
    return;
  }

  // Get current season
  auto season = getCurrentSeason();

  // Delegate to TileRenderer - batch recording is already begin()-ed by GPUSceneRecorder
  m_tileRenderer->recordGPUTiles(spriteBatch, cameraX, cameraY,
                                 viewWidth, viewHeight, zoom, season);
}
