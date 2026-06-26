/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef WORLD_RESOURCE_MANAGER_HPP
#define WORLD_RESOURCE_MANAGER_HPP

/**
 * @file WorldResourceManager.hpp
 * @brief Registry-over-EDM for world resource tracking
 *
 * WorldResourceManager is a REGISTRY, not a data store. It tracks which
 * inventories and harvestables belong to which world, and queries EDM
 * for actual resource quantities.
 *
 * All resource data lives in EntityDataManager:
 * - Inventories (Player, Container, etc.)
 * - Harvestables (trees, ore nodes)
 * - Dropped items
 *
 * WorldResourceManager provides aggregate queries across all registered
 * entities for a given world.
 *
 * IMPORTANT: This is a complete rewrite. The old addResource()/removeResource()
 * API is deleted. All resource mutation goes through EDM directly.
 */

#include "utils/ResourceHandle.hpp"
#include "utils/Vector2D.hpp"
#include <algorithm>
#include <iterator>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Statistics for world resource tracking
 */
struct WorldResourceStats {
    std::atomic<uint64_t> worldsTracked{0};
    std::atomic<uint64_t> inventoriesRegistered{0};
    std::atomic<uint64_t> harvestablesRegistered{0};
    std::atomic<uint64_t> queryCount{0};

    WorldResourceStats() = default;
    WorldResourceStats(const WorldResourceStats& other)
        : worldsTracked(other.worldsTracked.load()),
          inventoriesRegistered(other.inventoriesRegistered.load()),
          harvestablesRegistered(other.harvestablesRegistered.load()),
          queryCount(other.queryCount.load()) {}

    WorldResourceStats& operator=(const WorldResourceStats& other) {
        if (this != &other) {
            worldsTracked = other.worldsTracked.load();
            inventoriesRegistered = other.inventoriesRegistered.load();
            harvestablesRegistered = other.harvestablesRegistered.load();
            queryCount = other.queryCount.load();
        }
        return *this;
    }

    void reset() {
        worldsTracked = 0;
        inventoriesRegistered = 0;
        harvestablesRegistered = 0;
        queryCount = 0;
    }
};

/**
 * @brief Lightweight spatial index for O(k) proximity queries
 *
 * Uses a simple grid hash optimized for small pickup radius queries.
 * Cell size = 64px (2x typical pickup radius of 32px).
 *
 * Per-world: Each world has its own spatial index to avoid cross-world queries.
 */
struct SpatialIndex {
    static constexpr float CELL_SIZE = 64.0f;
    static constexpr float INV_CELL_SIZE = 1.0f / CELL_SIZE;
    static constexpr size_t INITIAL_CAPACITY = 500;

    // Grid cell -> set of EDM indices in that cell
    std::unordered_map<uint64_t, std::vector<size_t>> cells;

    // Reverse lookup: EDM index -> grid cell key (for O(1) removal)
    std::unordered_map<size_t, uint64_t> entityToCell;

    SpatialIndex() {
        cells.reserve(INITIAL_CAPACITY / 4);  // ~125 cells expected
        entityToCell.reserve(INITIAL_CAPACITY);
    }

    // Pack cell coordinates into 64-bit key
    [[nodiscard]] static uint64_t makeKey(int32_t cellX, int32_t cellY) noexcept {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cellY)) << 32) |
               static_cast<uint32_t>(cellX);
    }

    // World position to cell coordinate
    [[nodiscard]] static int32_t toCell(float worldCoord) noexcept {
        return static_cast<int32_t>(std::floor(worldCoord * INV_CELL_SIZE));
    }

    // Insert entity at position
    void insert(size_t edmIndex, const Vector2D& position) {
        int32_t cellX = toCell(position.getX());
        int32_t cellY = toCell(position.getY());
        uint64_t key = makeKey(cellX, cellY);

        cells[key].push_back(edmIndex);
        entityToCell[edmIndex] = key;
    }

    // Remove entity (returns true if found)
    bool remove(size_t edmIndex) {
        auto it = entityToCell.find(edmIndex);
        if (it == entityToCell.end()) {
            return false;
        }

        uint64_t key = it->second;
        entityToCell.erase(it);

        auto cellIt = cells.find(key);
        if (cellIt != cells.end()) {
            auto& vec = cellIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), edmIndex), vec.end());
            // Remove empty cell to prevent accumulation
            if (vec.empty()) {
                cells.erase(cellIt);
            }
        }
        return true;
    }

    // Query all entities within radius of center
    void queryRadius(const Vector2D& center, float radius,
                     std::vector<size_t>& outIndices) const {
        int32_t minCellX = toCell(center.getX() - radius);
        int32_t maxCellX = toCell(center.getX() + radius);
        int32_t minCellY = toCell(center.getY() - radius);
        int32_t maxCellY = toCell(center.getY() + radius);

        // Note: radiusSq check done by caller with EDM positions for precision

        for (int32_t cy = minCellY; cy <= maxCellY; ++cy) {
            for (int32_t cx = minCellX; cx <= maxCellX; ++cx) {
                uint64_t key = makeKey(cx, cy);
                auto it = cells.find(key);
                if (it != cells.end()) {
                    std::copy(it->second.begin(), it->second.end(),
                              std::back_inserter(outIndices));
                }
            }
        }
        // Note: Caller should do distance check with EDM positions for precision
    }

    // Clear all data
    void clear() {
        cells.clear();
        entityToCell.clear();
    }

    // Get count of entities
    [[nodiscard]] size_t size() const noexcept {
        return entityToCell.size();
    }
};

/**
 * @brief Registry-over-EDM for world resource tracking
 *
 * This manager tracks which inventories and harvestables belong to each world,
 * and queries EntityDataManager for actual resource quantities.
 *
 * NO quantity storage - all data lives in EDM.
 */
class WorldResourceManager {
public:
    // World ID type
    using WorldId = std::string;
    using Quantity = int64_t;

    static WorldResourceManager& Instance();

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    [[nodiscard]] bool init();
    [[nodiscard]] bool isInitialized() const { return m_initialized.load(); }
    void clean();

    /**
     * @brief Prepare for game state transition
     *
     * Clears fast-path counters to immediately stop spatial queries.
     * Call before state cleanup to ensure no queries are in flight.
     */
    void prepareForStateTransition();

    // ========================================================================
    // WORLD MANAGEMENT
    // ========================================================================

    /**
     * @brief Create a new world for tracking
     * @param worldId Unique world identifier
     * @return true if created, false if already exists
     */
    [[nodiscard]] bool createWorld(const WorldId& worldId);

    /**
     * @brief Remove a world and all its registrations
     * @param worldId World to remove
     * @return true if removed, false if not found
     *
     * Note: This only removes the registry entries. Entity cleanup
     * should be done via EDM before calling this.
     */
    bool removeWorld(const WorldId& worldId);

    /**
     * @brief Check if a world exists
     */
    [[nodiscard]] bool hasWorld(const WorldId& worldId) const;

    /**
     * @brief Get all tracked world IDs
     */
    [[nodiscard]] std::vector<WorldId> getWorldIds() const;

    // ========================================================================
    // REGISTRATION (link entities to worlds)
    // ========================================================================

    /**
     * @brief Register an inventory with a world
     * @param inventoryIndex EDM inventory index from createInventory()
     * @param worldId World to register with
     *
     * The inventory's resources will be included in world aggregate queries.
     */
    void registerInventory(uint32_t inventoryIndex, const WorldId& worldId);

    /**
     * @brief Unregister an inventory from its world
     * @param inventoryIndex EDM inventory index
     */
    void unregisterInventory(uint32_t inventoryIndex);

    /**
     * @brief Register a harvestable entity with a world
     * @param edmIndex EDM entity index for the harvestable
     * @param worldId World to register with
     *
     * The harvestable's potential yield will be included in world queries.
     */
    void registerHarvestable(size_t edmIndex, const WorldId& worldId);

    /**
     * @brief Unregister a harvestable from its world
     * @param edmIndex EDM entity index
     */
    void unregisterHarvestable(size_t edmIndex);

    // ========================================================================
    // DROPPED ITEM SPATIAL REGISTRATION
    // ========================================================================

    /**
     * @brief Register a dropped item with spatial tracking
     * @param edmIndex EDM entity index
     * @param position World position of the item
     * @param worldId World to register with
     */
    void registerDroppedItem(size_t edmIndex, const Vector2D& position, const WorldId& worldId);

    /**
     * @brief Unregister a dropped item from spatial tracking
     * @param edmIndex EDM entity index
     */
    void unregisterDroppedItem(size_t edmIndex);

    /**
     * @brief Register a harvestable with spatial tracking
     * @param edmIndex EDM entity index
     * @param position World position
     * @param worldId World to register with
     *
     * Note: This is called automatically by EDM::createHarvestable()
     */
    void registerHarvestableSpatial(size_t edmIndex, const Vector2D& position, const WorldId& worldId);

    /**
     * @brief Unregister a harvestable from spatial tracking
     * @param edmIndex EDM entity index
     */
    void unregisterHarvestableSpatial(size_t edmIndex);

    // ========================================================================
    // CONTAINER SPATIAL REGISTRATION
    // ========================================================================

    /**
     * @brief Register a container with spatial tracking
     * @param edmIndex EDM static entity index
     * @param position World position
     * @param worldId World to register with
     *
     * Note: Called automatically by EDM::createContainer()
     */
    void registerContainerSpatial(size_t edmIndex, const Vector2D& position, const WorldId& worldId);

    /**
     * @brief Unregister a container from spatial tracking
     * @param edmIndex EDM static entity index
     */
    void unregisterContainerSpatial(size_t edmIndex);

    /**
     * @brief Query containers near a position in active world
     * @param center Query center position
     * @param radius Search radius
     * @param outIndices Output: EDM static indices of nearby containers
     * @return Number of containers found
     */
    size_t queryContainersInRadius(const Vector2D& center, float radius,
                                   std::vector<size_t>& outIndices) const;

    // ========================================================================
    // SPATIAL QUERIES (O(k) where k = cells in radius)
    // ========================================================================

    /**
     * @brief Query dropped items near a position in active world
     * @param center Query center position
     * @param radius Search radius
     * @param outIndices Output: EDM indices of nearby items
     * @return Number of items found
     *
     * Note: Returns EDM indices. Caller should validate with EDM::isAlive()
     */
    size_t queryDroppedItemsInRadius(const Vector2D& center, float radius,
                                     std::vector<size_t>& outIndices) const;

    /**
     * @brief Query harvestables near a position in active world
     * @param center Query center position
     * @param radius Search radius
     * @param outIndices Output: EDM indices of nearby harvestables
     * @return Number of harvestables found
     */
    size_t queryHarvestablesInRadius(const Vector2D& center, float radius,
                                     std::vector<size_t>& outIndices) const;

    /**
     * @brief Find closest dropped item to position
     * @param center Search center
     * @param radius Maximum distance
     * @param outIndex Output: EDM index of closest item
     * @return true if item found, false if none in radius
     */
    bool findClosestDroppedItem(const Vector2D& center, float radius, size_t& outIndex) const;

    // ========================================================================
    // ACTIVE WORLD TRACKING
    // ========================================================================

    /**
     * @brief Set the currently active world
     * @param worldId Active world identifier
     *
     * Queries without explicit worldId use this as default.
     * Called directly by WorldManager when a world becomes active.
     */
    void setActiveWorld(const WorldId& worldId);

    /**
     * @brief Get the currently active world
     * @return Active world ID (empty if none set)
     */
    [[nodiscard]] const WorldId& getActiveWorld() const { return m_activeWorld; }

    /**
     * @brief Clear all spatial data for a world (items + harvestables)
     * @param worldId World to clear
     *
     * Called directly during world teardown when needed.
     */
    void clearSpatialDataForWorld(const WorldId& worldId);

    // ========================================================================
    // QUERY-ONLY RESOURCE ACCESS (reads EDM directly)
    // ========================================================================

    /**
     * @brief Query total inventory resources in a world
     * @param worldId World to query
     * @param handle Resource type
     * @return Sum of quantities across all registered inventories
     */
    [[nodiscard]] Quantity queryInventoryTotal(const WorldId& worldId,
                                               VoidLight::ResourceHandle handle) const;

    /**
     * @brief Query total harvestable yield potential in a world
     * @param worldId World to query
     * @param handle Resource type
     * @return Sum of (yieldMax) for non-depleted harvestables
     */
    [[nodiscard]] Quantity queryHarvestableTotal(const WorldId& worldId,
                                                 VoidLight::ResourceHandle handle) const;

    /**
     * @brief Query total world resources (inventories + harvestables)
     * @param worldId World to query
     * @param handle Resource type
     * @return Combined total from inventories and harvestables
     */
    [[nodiscard]] Quantity queryWorldTotal(const WorldId& worldId,
                                           VoidLight::ResourceHandle handle) const;

    /**
     * @brief Check if a world has at least the specified quantity
     */
    [[nodiscard]] bool hasResource(const WorldId& worldId,
                                   VoidLight::ResourceHandle handle,
                                   Quantity minimumQuantity = 1) const;

    /**
     * @brief Get all resource totals for a world
     * @return Map of resource handle -> total quantity
     */
    [[nodiscard]] std::unordered_map<VoidLight::ResourceHandle, Quantity>
    getWorldResources(const WorldId& worldId) const;

    // ========================================================================
    // STATISTICS
    // ========================================================================

    [[nodiscard]] WorldResourceStats getStats() const;
    void resetStats() { m_stats.reset(); }

    /**
     * @brief Get count of inventories registered to a world
     */
    [[nodiscard]] size_t getInventoryCount(const WorldId& worldId) const;

    /**
     * @brief Get count of harvestables registered to a world
     */
    [[nodiscard]] size_t getHarvestableCount(const WorldId& worldId) const;

private:
    WorldResourceManager() = default;
    ~WorldResourceManager();

    WorldResourceManager(const WorldResourceManager&) = delete;
    WorldResourceManager& operator=(const WorldResourceManager&) = delete;

    // ========================================================================
    // REGISTRY STORAGE (no quantity data - only entity references)
    // ========================================================================

    // WorldId -> set of inventory indices
    std::unordered_map<WorldId, std::unordered_set<uint32_t>> m_inventoryRegistry;

    // WorldId -> set of EDM harvestable indices
    std::unordered_map<WorldId, std::unordered_set<size_t>> m_harvestableRegistry;

    // Reverse lookup: inventory index -> WorldId
    std::unordered_map<uint32_t, WorldId> m_inventoryToWorld;

    // Reverse lookup: harvestable EDM index -> WorldId
    std::unordered_map<size_t, WorldId> m_harvestableToWorld;

    // ========================================================================
    // SPATIAL INDEX STORAGE (per-world, for O(k) proximity queries)
    // ========================================================================

    // Per-world spatial indices for dropped items
    std::unordered_map<WorldId, SpatialIndex> m_itemSpatialIndices;

    // Per-world spatial indices for harvestables
    std::unordered_map<WorldId, SpatialIndex> m_harvestableSpatialIndices;

    // Reverse lookup: item EDM index -> WorldId (for O(1) unregistration)
    std::unordered_map<size_t, WorldId> m_itemToWorld;

    // Reverse lookup: harvestable EDM index -> WorldId (for spatial unregistration)
    std::unordered_map<size_t, WorldId> m_harvestableSpatialToWorld;

    // Per-world spatial indices for containers
    std::unordered_map<WorldId, SpatialIndex> m_containerSpatialIndices;

    // Reverse lookup: container EDM index -> WorldId (for spatial unregistration)
    std::unordered_map<size_t, WorldId> m_containerToWorld;

    // Currently active world (set via event or explicit call)
    WorldId m_activeWorld;

    // Thread safety
    // LOCK ORDERING: m_registryMutex -> EDM::m_inventoryMutex (never reverse)
    // Query methods acquire m_registryMutex then call EDM which acquires m_inventoryMutex.
    // This ordering must be maintained to prevent deadlocks.
    mutable std::shared_mutex m_registryMutex;

    // State
    mutable WorldResourceStats m_stats;
    std::atomic<bool> m_initialized{false};

    // Reusable scratch for findClosestDroppedItem candidate gathering.
    // Main-thread-only (sole caller InventoryController::attemptPickup); avoids a
    // per-call allocation on the pickup path.
    mutable std::vector<size_t> m_closestItemScratch;

    // Fast-path counters for active world (avoid lock acquisition when empty)
    // These are updated on register/unregister and when active world changes
    std::atomic<size_t> m_activeWorldItemCount{0};
    std::atomic<size_t> m_activeWorldHarvestableCount{0};

    // Helper to recalculate active world counts (called under lock)
    void recalculateActiveWorldCounts();
};

#endif // WORLD_RESOURCE_MANAGER_HPP
