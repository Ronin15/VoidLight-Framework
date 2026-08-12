/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/world/HarvestController.hpp"
#include "core/Logger.hpp"
#include "entities/Player.hpp"
#include "events/HarvestResourceEvent.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "world/HarvestConfig.hpp"
#include "world/WorldData.hpp"
#include <format>
#include <limits>
#include <random>

HarvestController::HarvestController(std::shared_ptr<Player> player)
    : mp_player(player)
{
    m_harvestableIndicesBuffer.reserve(32);
}

void HarvestController::subscribe() {
    if (checkAlreadySubscribed()) {
        return;
    }

    setSubscribed(true);
    HARVEST_DEBUG("HarvestController subscribed");
}

void HarvestController::update(float deltaTime) {
    if (!m_isHarvesting) {
        return;
    }

    auto player = mp_player.lock();
    if (!player) {
        cancelHarvest();
        return;
    }

    // Check if player moved too far (cancels harvest)
    Vector2D currentPos = player->getPosition();
    float dx = currentPos.getX() - m_harvestStartPos.getX();
    float dy = currentPos.getY() - m_harvestStartPos.getY();
    float distSq = dx * dx + dy * dy;

    if (distSq > MOVEMENT_CANCEL_THRESHOLD * MOVEMENT_CANCEL_THRESHOLD) {
        HARVEST_DEBUG("Harvest cancelled - player moved");
        cancelHarvest();
        return;
    }

    // Advance timer
    m_harvestTimer += deltaTime;

    // Check completion
    if (m_harvestTimer >= m_harvestDuration) {
        completeHarvest();
    }
}

bool HarvestController::startHarvest() {
    if (m_isHarvesting) {
        return false;
    }

    auto player = mp_player.lock();
    if (!player) {
        return false;
    }

    EntityHandle handle;
    size_t staticIndex = 0;

    if (!findNearestHarvestable(handle, staticIndex)) {
        return false;
    }

    auto& edm = EntityDataManager::Instance();

    // Get harvestable data
    const auto& hot = edm.getStaticHotDataByIndex(staticIndex);
    const auto& harvestData = edm.getHarvestableData(hot.typeLocalIndex);

    if (harvestData.isDepleted) {
        HARVEST_DEBUG("Harvestable is depleted");
        return false;
    }

    // Get harvest type and configuration
    m_currentType = static_cast<VoidLight::HarvestType>(harvestData.harvestType);
    const auto& config = VoidLight::getHarvestTypeConfig(m_currentType);

    // Start harvesting
    m_isHarvesting = true;
    m_harvestTimer = 0.0f;
    m_harvestDuration = config.baseDuration;
    m_currentTarget = handle;
    m_targetStaticIndex = staticIndex;
    m_harvestStartPos = player->getPosition();
    m_targetPosition = hot.transform.position;

    HARVEST_INFO(std::format("Started {} (duration: {:.1f}s)",
                             config.actionVerb, m_harvestDuration));

    return true;
}

void HarvestController::cancelHarvest() {
    if (!m_isHarvesting) {
        return;
    }

    m_isHarvesting = false;
    m_harvestTimer = 0.0f;
    m_harvestDuration = 0.0f;
    m_currentTarget = EntityHandle{};
    m_targetStaticIndex = 0;
    m_currentType = VoidLight::HarvestType::Gathering;

    HARVEST_DEBUG("Harvest cancelled");
}

float HarvestController::getProgress() const {
    if (!m_isHarvesting || m_harvestDuration <= 0.0f) {
        return 0.0f;
    }
    return std::min(m_harvestTimer / m_harvestDuration, 1.0f);
}

std::string_view HarvestController::getActionVerb() const {
    return VoidLight::harvestTypeToActionVerb(m_currentType);
}

bool HarvestController::findNearestHarvestable(EntityHandle& outHandle, size_t& outStaticIndex) {
    auto player = mp_player.lock();
    if (!player) {
        return false;
    }

    auto& wrm = WorldResourceManager::Instance();
    auto& edm = EntityDataManager::Instance();

    Vector2D playerPos = player->getPosition();

    // Query harvestables from WRM spatial index
    m_harvestableIndicesBuffer.clear();
    if (wrm.queryHarvestablesInRadius(playerPos, HARVEST_RANGE, m_harvestableIndicesBuffer) == 0) {
        return false;
    }

    // Find closest non-depleted harvestable
    float closestDistSq = std::numeric_limits<float>::max();
    size_t closestIdx = std::numeric_limits<size_t>::max();
    EntityHandle closestHandle{};

    for (size_t idx : m_harvestableIndicesBuffer) {
        const auto& hot = edm.getStaticHotDataByIndex(idx);
        if (!hot.isAlive() || hot.kind != EntityKind::Harvestable) {
            continue;
        }

        const auto& harvestData = edm.getHarvestableData(hot.typeLocalIndex);
        if (harvestData.isDepleted) {
            continue;
        }

        const auto& pos = hot.transform.position;
        float dx = pos.getX() - playerPos.getX();
        float dy = pos.getY() - playerPos.getY();
        float distSq = dx * dx + dy * dy;

        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestIdx = idx;
            closestHandle = edm.getStaticHandle(idx);
        }
    }

    if (closestIdx == std::numeric_limits<size_t>::max()) {
        return false;
    }

    outHandle = closestHandle;
    outStaticIndex = closestIdx;
    return true;
}

void HarvestController::completeHarvest() {
    auto player = mp_player.lock();
    if (!player) {
        cancelHarvest();
        return;
    }

    auto& edm = EntityDataManager::Instance();
    auto& wrm = WorldResourceManager::Instance();

    // Validate target is still valid. Use the generation-safe handle rather than
    // the cached static index alone: static pool slots are reused with a new
    // generation on destroy, so a freed+reoccupied slot holding a different
    // harvestable could otherwise pass a bare index/isAlive()/kind check.
    if (edm.getStaticHandle(m_targetStaticIndex) != m_currentTarget) {
        HARVEST_WARN("Harvest target no longer valid");
        cancelHarvest();
        return;
    }

    const auto& hot = edm.getStaticHotDataByIndex(m_targetStaticIndex);

    // Get harvestable data (mutable for updating depleted state)
    auto& harvestData = edm.getHarvestableData(hot.typeLocalIndex);
    if (harvestData.isDepleted) {
        HARVEST_DEBUG("Harvest target already depleted");
        cancelHarvest();
        return;
    }

    // Calculate yield
    int yield = harvestData.yieldMin;
    if (harvestData.yieldMax > harvestData.yieldMin) {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(harvestData.yieldMin, harvestData.yieldMax);
        yield = dist(rng);
    }

    // Try to add directly to player inventory
    uint32_t playerInvIdx = player->getInventoryIndex();
    bool addedToInventory = false;

    HARVEST_DEBUG(std::format("Player inventory index: {}", playerInvIdx));

    if (playerInvIdx != INVALID_INVENTORY_INDEX) {
        // Get old quantity with targeted lookup (avoids full inventory scan)
        int oldQuantity = edm.getInventoryQuantity(playerInvIdx, harvestData.yieldResource);

        addedToInventory = edm.addToInventory(playerInvIdx, harvestData.yieldResource, yield);

        // Fire ResourceChangeEvent for UI updates (only if added to inventory)
        if (addedToInventory) {
            int newQuantity = oldQuantity + yield;
            auto resourceChangeEvent = std::make_shared<ResourceChangeEvent>(
                player->getHandle(),
                harvestData.yieldResource,
                oldQuantity,
                newQuantity,
                "harvested"
            );
            EventManager::Instance().dispatchEvent(resourceChangeEvent);
        }
    } else {
        HARVEST_WARN("Player has no valid inventory index!");
    }

    // If inventory full or no inventory, spawn as dropped item
    if (!addedToInventory) {
        // Spawn slightly offset from harvestable position
        Vector2D spawnPos = hot.transform.position;
        spawnPos.setX(spawnPos.getX() + 16.0f);

        const std::string& worldId = wrm.getActiveWorld();
        edm.createDroppedItem(spawnPos, harvestData.yieldResource, yield, worldId);

        HARVEST_INFO(std::format("Completed {} - {} x{} (dropped)",
                                 VoidLight::harvestTypeToString(m_currentType),
                                 harvestData.yieldResource.toString(), yield));
    } else {
        HARVEST_INFO(std::format("Completed {} - {} x{} (added to inventory)",
                                 VoidLight::harvestTypeToString(m_currentType),
                                 harvestData.yieldResource.toString(), yield));
    }

    // Mark harvestable as depleted
    harvestData.isDepleted = true;
    harvestData.currentRespawn = harvestData.respawnTime;

    // Fire HarvestResourceEvent to update world tile visuals
    // Note: Only fires for EDM harvestables. The WorldManager handler will
    // check if the tile actually has an obstacle before modifying it.
    // This allows tile-based and EDM-based harvestables to coexist.
    int tileX = static_cast<int>(m_targetPosition.getX() / VoidLight::TILE_SIZE);
    int tileY = static_cast<int>(m_targetPosition.getY() / VoidLight::TILE_SIZE);

    auto harvestEvent = std::make_shared<HarvestResourceEvent>(
        static_cast<int>(m_currentTarget.getId()),
        tileX,
        tileY,
        std::string(harvestData.yieldResource.toString())
    );
    EventManager::Instance().dispatchEvent(harvestEvent);

    HARVEST_DEBUG(std::format("Fired HarvestResourceEvent at tile ({}, {})", tileX, tileY));

    // Reset harvest state
    m_isHarvesting = false;
    m_harvestTimer = 0.0f;
    m_harvestDuration = 0.0f;
    m_currentTarget = EntityHandle{};
    m_targetStaticIndex = 0;
    m_currentType = VoidLight::HarvestType::Gathering;
}
