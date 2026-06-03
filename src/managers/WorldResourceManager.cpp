/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/WorldResourceManager.hpp"
#include "core/Logger.hpp"
#include "managers/EntityDataManager.hpp"
#include <algorithm>
#include <format>
#include <limits>
#include <numeric>

WorldResourceManager& WorldResourceManager::Instance() {
    static WorldResourceManager instance;
    return instance;
}

WorldResourceManager::~WorldResourceManager() {
    if (m_initialized.load(std::memory_order_acquire)) {
        clean();
    }
}

bool WorldResourceManager::init() {
    if (m_initialized.load(std::memory_order_acquire)) {
        return true;
    }

    std::unique_lock lock(m_registryMutex);

    if (m_initialized.load(std::memory_order_acquire)) {
        return true;
    }

    // Clear any existing registry data
    m_inventoryRegistry.clear();
    m_harvestableRegistry.clear();
    m_inventoryToWorld.clear();
    m_harvestableToWorld.clear();

    // Clear spatial data
    m_itemSpatialIndices.clear();
    m_harvestableSpatialIndices.clear();
    m_itemToWorld.clear();
    m_harvestableSpatialToWorld.clear();
    m_activeWorld.clear();

    // Create default world for single-world scenarios
    m_inventoryRegistry["default"] = {};
    m_harvestableRegistry["default"] = {};
    m_itemSpatialIndices["default"] = SpatialIndex();
    m_harvestableSpatialIndices["default"] = SpatialIndex();

    m_stats.reset();
    m_stats.worldsTracked = 1;

    m_initialized.store(true, std::memory_order_release);

    WORLD_RESOURCE_INFO("WorldResourceManager initialized (registry-over-EDM mode)");
    return true;
}

void WorldResourceManager::clean() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return;
    }

    std::unique_lock lock(m_registryMutex);

    m_inventoryRegistry.clear();
    m_harvestableRegistry.clear();
    m_inventoryToWorld.clear();
    m_harvestableToWorld.clear();

    // Clear spatial data
    m_itemSpatialIndices.clear();
    m_harvestableSpatialIndices.clear();
    m_itemToWorld.clear();
    m_harvestableSpatialToWorld.clear();
    m_activeWorld.clear();

    m_stats.reset();
    m_initialized.store(false, std::memory_order_release);

    WORLD_RESOURCE_INFO("WorldResourceManager cleaned up");
}

void WorldResourceManager::prepareForStateTransition() {
    // Clear fast-path counters to stop queries immediately
    m_activeWorldItemCount.store(0, std::memory_order_release);
    m_activeWorldHarvestableCount.store(0, std::memory_order_release);

    // Acquire exclusive lock to wait for any in-flight queries to complete
    std::unique_lock lock(m_registryMutex);

    // Clear all registries (stale EDM indices from previous state)
    m_inventoryRegistry.clear();
    m_harvestableRegistry.clear();
    m_inventoryToWorld.clear();
    m_harvestableToWorld.clear();

    // Clear all spatial indices and reverse lookups
    m_itemSpatialIndices.clear();
    m_harvestableSpatialIndices.clear();
    m_containerSpatialIndices.clear();
    m_itemToWorld.clear();
    m_harvestableSpatialToWorld.clear();
    m_containerToWorld.clear();

    // Clear active world so createWorld() succeeds on re-entry
    m_activeWorld.clear();

    // Re-create default world entries (matches init() pattern)
    m_inventoryRegistry["default"] = {};
    m_harvestableRegistry["default"] = {};
    m_itemSpatialIndices["default"] = SpatialIndex();
    m_harvestableSpatialIndices["default"] = SpatialIndex();

    m_stats.reset();
    m_stats.worldsTracked = 1;

    WORLD_RESOURCE_DEBUG("Prepared for state transition");
}

// ============================================================================
// WORLD MANAGEMENT
// ============================================================================

bool WorldResourceManager::createWorld(const WorldId& worldId) {
    if (worldId.empty()) {
        WORLD_RESOURCE_ERROR("createWorld: Empty world ID");
        return false;
    }

    std::unique_lock lock(m_registryMutex);

    if (m_inventoryRegistry.find(worldId) != m_inventoryRegistry.end()) {
        WORLD_RESOURCE_WARN(std::format("createWorld: World already exists: {}", worldId));
        return false;
    }

    m_inventoryRegistry[worldId] = {};
    m_harvestableRegistry[worldId] = {};
    m_stats.worldsTracked.fetch_add(1, std::memory_order_relaxed);

    WORLD_RESOURCE_INFO(std::format("Created world: {}", worldId));
    return true;
}

bool WorldResourceManager::removeWorld(const WorldId& worldId) {
    if (worldId.empty()) {
        WORLD_RESOURCE_ERROR("removeWorld: Empty world ID");
        return false;
    }

    if (worldId == "default") {
        WORLD_RESOURCE_WARN("removeWorld: Cannot remove default world");
        return false;
    }

    std::unique_lock lock(m_registryMutex);

    auto invIt = m_inventoryRegistry.find(worldId);
    auto harvIt = m_harvestableRegistry.find(worldId);

    if (invIt == m_inventoryRegistry.end()) {
        WORLD_RESOURCE_WARN(std::format("removeWorld: World not found: {}", worldId));
        return false;
    }

    if (worldId == m_activeWorld) {
        m_activeWorld.clear();
        m_activeWorldItemCount.store(0, std::memory_order_relaxed);
        m_activeWorldHarvestableCount.store(0, std::memory_order_relaxed);
    }

    // Remove reverse lookups for this world's inventories
    for (uint32_t invIdx : invIt->second) {
        m_inventoryToWorld.erase(invIdx);
    }

    // Remove reverse lookups for this world's harvestables
    if (harvIt != m_harvestableRegistry.end()) {
        for (size_t harvIdx : harvIt->second) {
            m_harvestableToWorld.erase(harvIdx);
        }
        m_harvestableRegistry.erase(harvIt);
    }

    auto itemIt = m_itemSpatialIndices.find(worldId);
    if (itemIt != m_itemSpatialIndices.end()) {
        for (const auto& entry : itemIt->second.entityToCell) {
            m_itemToWorld.erase(entry.first);
        }
        m_itemSpatialIndices.erase(itemIt);
    }

    auto harvSpatialIt = m_harvestableSpatialIndices.find(worldId);
    if (harvSpatialIt != m_harvestableSpatialIndices.end()) {
        for (const auto& entry : harvSpatialIt->second.entityToCell) {
            m_harvestableSpatialToWorld.erase(entry.first);
        }
        m_harvestableSpatialIndices.erase(harvSpatialIt);
    }

    auto containerIt = m_containerSpatialIndices.find(worldId);
    if (containerIt != m_containerSpatialIndices.end()) {
        for (const auto& entry : containerIt->second.entityToCell) {
            m_containerToWorld.erase(entry.first);
        }
        m_containerSpatialIndices.erase(containerIt);
    }

    m_inventoryRegistry.erase(invIt);
    m_stats.worldsTracked.fetch_sub(1, std::memory_order_relaxed);

    WORLD_RESOURCE_INFO(std::format("Removed world: {}", worldId));
    return true;
}

bool WorldResourceManager::hasWorld(const WorldId& worldId) const {
    std::shared_lock lock(m_registryMutex);
    return m_inventoryRegistry.find(worldId) != m_inventoryRegistry.end();
}

std::vector<WorldResourceManager::WorldId> WorldResourceManager::getWorldIds() const {
    std::shared_lock lock(m_registryMutex);

    std::vector<WorldId> ids;
    ids.reserve(m_inventoryRegistry.size());

    for (const auto& [worldId, _] : m_inventoryRegistry) {
        ids.push_back(worldId);
    }

    return ids;
}

// ============================================================================
// REGISTRATION
// ============================================================================

void WorldResourceManager::registerInventory(uint32_t inventoryIndex, const WorldId& worldId) {
    if (inventoryIndex == INVALID_INVENTORY_INDEX) {
        WORLD_RESOURCE_ERROR("registerInventory: Invalid inventory index");
        return;
    }

    std::unique_lock lock(m_registryMutex);

    // Check if already registered to another world
    auto existingIt = m_inventoryToWorld.find(inventoryIndex);
    if (existingIt != m_inventoryToWorld.end()) {
        if (existingIt->second == worldId) {
            return;  // Already registered to this world
        }
        // Unregister from old world first
        auto& oldSet = m_inventoryRegistry[existingIt->second];
        oldSet.erase(inventoryIndex);
        m_stats.inventoriesRegistered.fetch_sub(1, std::memory_order_relaxed);
    }

    // Ensure world exists
    auto worldIt = m_inventoryRegistry.find(worldId);
    if (worldIt == m_inventoryRegistry.end()) {
        WORLD_RESOURCE_WARN(std::format("registerInventory: World not found: {}, creating", worldId));
        m_inventoryRegistry[worldId] = {};
        m_harvestableRegistry[worldId] = {};
        m_stats.worldsTracked.fetch_add(1, std::memory_order_relaxed);
        worldIt = m_inventoryRegistry.find(worldId);
    }

    worldIt->second.insert(inventoryIndex);
    m_inventoryToWorld[inventoryIndex] = worldId;
    m_stats.inventoriesRegistered.fetch_add(1, std::memory_order_relaxed);

    WORLD_RESOURCE_DEBUG(std::format("Registered inventory {} to world {}", inventoryIndex, worldId));
}

void WorldResourceManager::unregisterInventory(uint32_t inventoryIndex) {
    std::unique_lock lock(m_registryMutex);

    auto it = m_inventoryToWorld.find(inventoryIndex);
    if (it == m_inventoryToWorld.end()) {
        return;  // Not registered
    }

    const WorldId& worldId = it->second;
    auto worldIt = m_inventoryRegistry.find(worldId);
    if (worldIt != m_inventoryRegistry.end()) {
        worldIt->second.erase(inventoryIndex);
    }

    m_inventoryToWorld.erase(it);
    m_stats.inventoriesRegistered.fetch_sub(1, std::memory_order_relaxed);

    WORLD_RESOURCE_DEBUG(std::format("Unregistered inventory {}", inventoryIndex));
}

void WorldResourceManager::registerHarvestable(size_t edmIndex, const WorldId& worldId) {
    std::unique_lock lock(m_registryMutex);

    // Check if already registered to another world
    auto existingIt = m_harvestableToWorld.find(edmIndex);
    if (existingIt != m_harvestableToWorld.end()) {
        if (existingIt->second == worldId) {
            return;  // Already registered to this world
        }
        // Unregister from old world first
        auto& oldSet = m_harvestableRegistry[existingIt->second];
        oldSet.erase(edmIndex);
        m_stats.harvestablesRegistered.fetch_sub(1, std::memory_order_relaxed);
    }

    // Ensure world exists
    auto worldIt = m_harvestableRegistry.find(worldId);
    if (worldIt == m_harvestableRegistry.end()) {
        WORLD_RESOURCE_WARN(std::format("registerHarvestable: World not found: {}, creating", worldId));
        m_inventoryRegistry[worldId] = {};
        m_harvestableRegistry[worldId] = {};
        m_stats.worldsTracked.fetch_add(1, std::memory_order_relaxed);
        worldIt = m_harvestableRegistry.find(worldId);
    }

    worldIt->second.insert(edmIndex);
    m_harvestableToWorld[edmIndex] = worldId;
    m_stats.harvestablesRegistered.fetch_add(1, std::memory_order_relaxed);

}

void WorldResourceManager::unregisterHarvestable(size_t edmIndex) {
    std::unique_lock lock(m_registryMutex);

    auto it = m_harvestableToWorld.find(edmIndex);
    if (it == m_harvestableToWorld.end()) {
        return;  // Not registered
    }

    const WorldId& worldId = it->second;
    auto worldIt = m_harvestableRegistry.find(worldId);
    if (worldIt != m_harvestableRegistry.end()) {
        worldIt->second.erase(edmIndex);
    }

    m_harvestableToWorld.erase(it);
    m_stats.harvestablesRegistered.fetch_sub(1, std::memory_order_relaxed);

    WORLD_RESOURCE_DEBUG(std::format("Unregistered harvestable {}", edmIndex));
}

// ============================================================================
// QUERY-ONLY RESOURCE ACCESS (reads EDM directly)
// ============================================================================

WorldResourceManager::Quantity WorldResourceManager::queryInventoryTotal(
    const WorldId& worldId,
    VoidLight::ResourceHandle handle) const {

    m_stats.queryCount.fetch_add(1, std::memory_order_relaxed);

    std::shared_lock lock(m_registryMutex);

    auto worldIt = m_inventoryRegistry.find(worldId);
    if (worldIt == m_inventoryRegistry.end()) {
        return 0;
    }

    const auto& edm = EntityDataManager::Instance();
    const Quantity total = std::accumulate(
        worldIt->second.begin(),
        worldIt->second.end(),
        Quantity{0},
        [&edm, handle](Quantity sum, uint32_t invIdx) {
            return sum + edm.getInventoryQuantity(invIdx, handle);
        });

    return total;
}

WorldResourceManager::Quantity WorldResourceManager::queryHarvestableTotal(
    const WorldId& worldId,
    VoidLight::ResourceHandle handle) const {

    m_stats.queryCount.fetch_add(1, std::memory_order_relaxed);

    std::shared_lock lock(m_registryMutex);

    auto worldIt = m_harvestableRegistry.find(worldId);
    if (worldIt == m_harvestableRegistry.end()) {
        return 0;
    }

    auto& edm = EntityDataManager::Instance();
    Quantity total = 0;

    for (size_t edmIdx : worldIt->second) {
        // Get hot data and verify it's still a harvestable
        // Note: Registry should be kept clean via unregisterHarvestable on entity destruction
        const auto& hot = edm.getStaticHotDataByIndex(edmIdx);
        if (hot.kind != EntityKind::Harvestable) {
            continue;  // Entity was destroyed or changed type
        }

        const auto& harvData = edm.getHarvestableData(hot.typeLocalIndex);

        // Only count non-depleted harvestables with matching resource
        if (!harvData.isDepleted && harvData.yieldResource == handle) {
            total += harvData.yieldMax;  // Potential yield
        }
    }

    return total;
}

WorldResourceManager::Quantity WorldResourceManager::queryWorldTotal(
    const WorldId& worldId,
    VoidLight::ResourceHandle handle) const {

    return queryInventoryTotal(worldId, handle) + queryHarvestableTotal(worldId, handle);
}

bool WorldResourceManager::hasResource(
    const WorldId& worldId,
    VoidLight::ResourceHandle handle,
    Quantity minimumQuantity) const {

    return queryWorldTotal(worldId, handle) >= minimumQuantity;
}

std::unordered_map<VoidLight::ResourceHandle, WorldResourceManager::Quantity>
WorldResourceManager::getWorldResources(const WorldId& worldId) const {

    m_stats.queryCount.fetch_add(1, std::memory_order_relaxed);

    std::unordered_map<VoidLight::ResourceHandle, Quantity> totals;

    std::shared_lock lock(m_registryMutex);
    auto& edm = EntityDataManager::Instance();

    // Sum from inventories
    auto invIt = m_inventoryRegistry.find(worldId);
    if (invIt != m_inventoryRegistry.end()) {
        for (uint32_t invIdx : invIt->second) {
            auto invResources = edm.getInventoryResources(invIdx);
            for (const auto& [handle, qty] : invResources) {
                totals[handle] += qty;
            }
        }
    }

    // Sum from harvestables
    auto harvIt = m_harvestableRegistry.find(worldId);
    if (harvIt != m_harvestableRegistry.end()) {
        for (size_t edmIdx : harvIt->second) {
            // Registry is kept clean via unregisterHarvestable on entity destruction
            const auto& hot = edm.getStaticHotDataByIndex(edmIdx);
            if (hot.kind != EntityKind::Harvestable) {
                continue;  // Entity was destroyed or changed type
            }

            const auto& harvData = edm.getHarvestableData(hot.typeLocalIndex);
            if (!harvData.isDepleted && harvData.yieldResource.isValid()) {
                totals[harvData.yieldResource] += harvData.yieldMax;
            }
        }
    }

    return totals;
}

// ============================================================================
// STATISTICS
// ============================================================================

WorldResourceStats WorldResourceManager::getStats() const {
    return m_stats;
}

size_t WorldResourceManager::getInventoryCount(const WorldId& worldId) const {
    std::shared_lock lock(m_registryMutex);

    auto it = m_inventoryRegistry.find(worldId);
    return (it != m_inventoryRegistry.end()) ? it->second.size() : 0;
}

size_t WorldResourceManager::getHarvestableCount(const WorldId& worldId) const {
    std::shared_lock lock(m_registryMutex);

    auto it = m_harvestableRegistry.find(worldId);
    return (it != m_harvestableRegistry.end()) ? it->second.size() : 0;
}

// ============================================================================
// DROPPED ITEM SPATIAL REGISTRATION
// ============================================================================

void WorldResourceManager::registerDroppedItem(size_t edmIndex, const Vector2D& position, const WorldId& worldId) {
    std::unique_lock lock(m_registryMutex);

    // Check if already registered to another world
    auto existingIt = m_itemToWorld.find(edmIndex);
    if (existingIt != m_itemToWorld.end()) {
        if (existingIt->second == worldId) {
            return;  // Already registered to this world
        }
        // Unregister from old world first
        auto& oldIndex = m_itemSpatialIndices[existingIt->second];
        oldIndex.remove(edmIndex);
        // Update counter if unregistering from active world
        if (existingIt->second == m_activeWorld) {
            m_activeWorldItemCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    // Ensure world spatial index exists
    auto& spatialIndex = m_itemSpatialIndices[worldId];
    spatialIndex.insert(edmIndex, position);
    m_itemToWorld[edmIndex] = worldId;

    // Update counter if registering to active world
    if (worldId == m_activeWorld) {
        m_activeWorldItemCount.fetch_add(1, std::memory_order_relaxed);
    }

    WORLD_RESOURCE_DEBUG(std::format("Registered dropped item {} at ({:.1f}, {:.1f}) to world {}",
                                      edmIndex, position.getX(), position.getY(), worldId));
}

void WorldResourceManager::unregisterDroppedItem(size_t edmIndex) {
    std::unique_lock lock(m_registryMutex);

    auto it = m_itemToWorld.find(edmIndex);
    if (it == m_itemToWorld.end()) {
        return;  // Not registered
    }

    // Update counter if unregistering from active world
    if (it->second == m_activeWorld) {
        m_activeWorldItemCount.fetch_sub(1, std::memory_order_relaxed);
    }

    auto& spatialIndex = m_itemSpatialIndices[it->second];
    spatialIndex.remove(edmIndex);
    m_itemToWorld.erase(it);

    WORLD_RESOURCE_DEBUG(std::format("Unregistered dropped item {}", edmIndex));
}

void WorldResourceManager::registerHarvestableSpatial(size_t edmIndex, const Vector2D& position, const WorldId& worldId) {
    std::unique_lock lock(m_registryMutex);

    // Check if already registered
    auto existingIt = m_harvestableSpatialToWorld.find(edmIndex);
    if (existingIt != m_harvestableSpatialToWorld.end()) {
        if (existingIt->second == worldId) {
            return;  // Already registered
        }
        // Unregister from old world
        auto& oldIndex = m_harvestableSpatialIndices[existingIt->second];
        oldIndex.remove(edmIndex);
        // Update counter if unregistering from active world
        if (existingIt->second == m_activeWorld) {
            m_activeWorldHarvestableCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    // Add to spatial index
    auto& spatialIndex = m_harvestableSpatialIndices[worldId];
    spatialIndex.insert(edmIndex, position);
    m_harvestableSpatialToWorld[edmIndex] = worldId;

    // Update counter if registering to active world
    if (worldId == m_activeWorld) {
        m_activeWorldHarvestableCount.fetch_add(1, std::memory_order_relaxed);
    }

}

void WorldResourceManager::unregisterHarvestableSpatial(size_t edmIndex) {
    std::unique_lock lock(m_registryMutex);

    auto it = m_harvestableSpatialToWorld.find(edmIndex);
    if (it == m_harvestableSpatialToWorld.end()) {
        return;  // Not registered
    }

    // Update counter if unregistering from active world
    if (it->second == m_activeWorld) {
        m_activeWorldHarvestableCount.fetch_sub(1, std::memory_order_relaxed);
    }

    auto& spatialIndex = m_harvestableSpatialIndices[it->second];
    spatialIndex.remove(edmIndex);
    m_harvestableSpatialToWorld.erase(it);

    WORLD_RESOURCE_DEBUG(std::format("Unregistered harvestable spatial {}", edmIndex));
}

// ============================================================================
// CONTAINER SPATIAL REGISTRATION
// ============================================================================

void WorldResourceManager::registerContainerSpatial(size_t edmIndex, const Vector2D& position, const WorldId& worldId) {
    std::unique_lock lock(m_registryMutex);

    // Check if already registered
    auto existingIt = m_containerToWorld.find(edmIndex);
    if (existingIt != m_containerToWorld.end()) {
        if (existingIt->second == worldId) {
            return;  // Already registered
        }
        // Unregister from old world
        auto& oldIndex = m_containerSpatialIndices[existingIt->second];
        oldIndex.remove(edmIndex);
    }

    // Add to spatial index
    auto& spatialIndex = m_containerSpatialIndices[worldId];
    spatialIndex.insert(edmIndex, position);
    m_containerToWorld[edmIndex] = worldId;

    WORLD_RESOURCE_DEBUG(std::format("Registered container spatial {} at ({:.1f}, {:.1f}) to world {}",
                                      edmIndex, position.getX(), position.getY(), worldId));
}

void WorldResourceManager::unregisterContainerSpatial(size_t edmIndex) {
    std::unique_lock lock(m_registryMutex);

    auto it = m_containerToWorld.find(edmIndex);
    if (it == m_containerToWorld.end()) {
        return;  // Not registered
    }

    auto& spatialIndex = m_containerSpatialIndices[it->second];
    spatialIndex.remove(edmIndex);
    m_containerToWorld.erase(it);

    WORLD_RESOURCE_DEBUG(std::format("Unregistered container spatial {}", edmIndex));
}

size_t WorldResourceManager::queryContainersInRadius(const Vector2D& center, float radius,
                                                      std::vector<size_t>& outIndices) const {
    if (!m_initialized.load(std::memory_order_acquire)) {
        outIndices.clear();
        return 0;
    }

    std::shared_lock lock(m_registryMutex);

    if (m_activeWorld.empty()) {
        outIndices.clear();
        return 0;
    }

    auto it = m_containerSpatialIndices.find(m_activeWorld);
    if (it == m_containerSpatialIndices.end()) {
        outIndices.clear();
        return 0;
    }

    outIndices.clear();
    it->second.queryRadius(center, radius, outIndices);
    return outIndices.size();
}

// ============================================================================
// SPATIAL QUERIES
// ============================================================================

size_t WorldResourceManager::queryDroppedItemsInRadius(const Vector2D& center, float radius,
                                                        std::vector<size_t>& outIndices) const {
    if (!m_initialized.load(std::memory_order_acquire)) {
        outIndices.clear();
        return 0;
    }

    // Fast path: skip lock acquisition if no items in active world
    if (m_activeWorldItemCount.load(std::memory_order_relaxed) == 0) {
        outIndices.clear();
        return 0;
    }

    std::shared_lock lock(m_registryMutex);

    if (m_activeWorld.empty()) {
        outIndices.clear();
        return 0;
    }

    auto it = m_itemSpatialIndices.find(m_activeWorld);
    if (it == m_itemSpatialIndices.end()) {
        outIndices.clear();
        return 0;
    }

    outIndices.clear();
    it->second.queryRadius(center, radius, outIndices);

    // Precise distance filtering using EDM positions
    const auto& edm = EntityDataManager::Instance();
    float radiusSq = radius * radius;

    auto newEnd = std::remove_if(outIndices.begin(), outIndices.end(),
        [&](size_t idx) {
            const auto& hot = edm.getStaticHotDataByIndex(idx);
            if (!hot.isAlive()) {
                return true;  // Remove stale entries
            }
            const auto& pos = hot.transform.position;
            float dx = pos.getX() - center.getX();
            float dy = pos.getY() - center.getY();
            return (dx * dx + dy * dy) > radiusSq;
        });

    outIndices.erase(newEnd, outIndices.end());
    return outIndices.size();
}

size_t WorldResourceManager::queryHarvestablesInRadius(const Vector2D& center, float radius,
                                                        std::vector<size_t>& outIndices) const {
    if (!m_initialized.load(std::memory_order_acquire)) {
        outIndices.clear();
        return 0;
    }

    // Fast path: skip lock acquisition if no harvestables in active world
    if (m_activeWorldHarvestableCount.load(std::memory_order_relaxed) == 0) {
        outIndices.clear();
        return 0;
    }

    std::shared_lock lock(m_registryMutex);

    if (m_activeWorld.empty()) {
        outIndices.clear();
        return 0;
    }

    auto it = m_harvestableSpatialIndices.find(m_activeWorld);
    if (it == m_harvestableSpatialIndices.end()) {
        outIndices.clear();
        return 0;
    }

    outIndices.clear();
    it->second.queryRadius(center, radius, outIndices);

    // Precise distance filtering using EDM positions
    const auto& edm = EntityDataManager::Instance();
    float radiusSq = radius * radius;

    auto newEnd = std::remove_if(outIndices.begin(), outIndices.end(),
        [&](size_t idx) {
            const auto& hot = edm.getStaticHotDataByIndex(idx);
            if (!hot.isAlive()) {
                return true;  // Remove stale entries
            }
            const auto& pos = hot.transform.position;
            float dx = pos.getX() - center.getX();
            float dy = pos.getY() - center.getY();
            return (dx * dx + dy * dy) > radiusSq;
        });

    outIndices.erase(newEnd, outIndices.end());
    return outIndices.size();
}

bool WorldResourceManager::findClosestDroppedItem(const Vector2D& center, float radius, size_t& outIndex) const {
    std::vector<size_t> candidates;
    candidates.reserve(16);  // Reasonable initial capacity

    if (queryDroppedItemsInRadius(center, radius, candidates) == 0) {
        return false;
    }

    const auto& edm = EntityDataManager::Instance();
    float closestDistSq = std::numeric_limits<float>::max();
    size_t closestIdx = 0;
    bool found = false;

    for (size_t idx : candidates) {
        const auto& hot = edm.getStaticHotDataByIndex(idx);
        const auto& pos = hot.transform.position;
        float dx = pos.getX() - center.getX();
        float dy = pos.getY() - center.getY();
        float distSq = dx * dx + dy * dy;

        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestIdx = idx;
            found = true;
        }
    }

    if (found) {
        outIndex = closestIdx;
    }
    return found;
}

// ============================================================================
// ACTIVE WORLD TRACKING
// ============================================================================

void WorldResourceManager::setActiveWorld(const WorldId& worldId) {
    std::unique_lock lock(m_registryMutex);
    m_activeWorld = worldId;
    recalculateActiveWorldCounts();
    WORLD_RESOURCE_INFO(std::format("Active world set to: {}", worldId.empty() ? "(none)" : worldId));
}

void WorldResourceManager::recalculateActiveWorldCounts() {
    // Called under lock - recalculate counts for the active world
    if (m_activeWorld.empty()) {
        m_activeWorldItemCount.store(0, std::memory_order_relaxed);
        m_activeWorldHarvestableCount.store(0, std::memory_order_relaxed);
        return;
    }

    // Count items in active world
    auto itemIt = m_itemSpatialIndices.find(m_activeWorld);
    size_t itemCount = (itemIt != m_itemSpatialIndices.end()) ? itemIt->second.size() : 0;
    m_activeWorldItemCount.store(itemCount, std::memory_order_relaxed);

    // Count harvestables in active world
    auto harvIt = m_harvestableSpatialIndices.find(m_activeWorld);
    size_t harvCount = (harvIt != m_harvestableSpatialIndices.end()) ? harvIt->second.size() : 0;
    m_activeWorldHarvestableCount.store(harvCount, std::memory_order_relaxed);
}

void WorldResourceManager::clearSpatialDataForWorld(const WorldId& worldId) {
    std::unique_lock lock(m_registryMutex);

    // Reset counters if clearing active world
    if (worldId == m_activeWorld) {
        m_activeWorldItemCount.store(0, std::memory_order_relaxed);
        m_activeWorldHarvestableCount.store(0, std::memory_order_relaxed);
    }

    // Clear item spatial index
    auto itemIt = m_itemSpatialIndices.find(worldId);
    if (itemIt != m_itemSpatialIndices.end()) {
        // Remove reverse lookups for items in this world
        for (const auto& entry : itemIt->second.entityToCell) {
            m_itemToWorld.erase(entry.first);
        }
        itemIt->second.clear();
    }

    // Clear harvestable spatial index
    auto harvIt = m_harvestableSpatialIndices.find(worldId);
    if (harvIt != m_harvestableSpatialIndices.end()) {
        // Remove reverse lookups
        for (const auto& entry : harvIt->second.entityToCell) {
            m_harvestableSpatialToWorld.erase(entry.first);
        }
        harvIt->second.clear();
    }

    // Clear container spatial index
    auto containerIt = m_containerSpatialIndices.find(worldId);
    if (containerIt != m_containerSpatialIndices.end()) {
        for (const auto& entry : containerIt->second.entityToCell) {
            m_containerToWorld.erase(entry.first);
        }
        containerIt->second.clear();
    }

    WORLD_RESOURCE_INFO(std::format("Cleared spatial data for world: {}", worldId));
}
