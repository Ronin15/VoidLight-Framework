/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/world/HarvestController.hpp"
#include "core/Logger.hpp"
#include "entities/Player.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "world/HarvestCommit.hpp"
#include "world/HarvestConfig.hpp"
#include <format>
#include <limits>

HarvestController::HarvestController(std::shared_ptr<Player> player)
    : mp_player(player) {
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

    const auto& wrm = WorldResourceManager::Instance();
    auto& edm = EntityDataManager::Instance();

    Vector2D playerPos = player->getPosition();

    // Query harvestables from WRM spatial index
    m_harvestableIndicesBuffer.clear();
    if (wrm.queryHarvestablesInRadius(playerPos, VoidLight::HarvestCommit::HARVEST_RANGE, m_harvestableIndicesBuffer) == 0) {
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

    // Shared depletion path (generation check, EDM depletion, WRM version,
    // HarvestResourceEvent, ScarcityEvent). The player may take the last node.
    const auto yield = VoidLight::HarvestCommit::commit(
        m_currentTarget, player->getHandle(), 0);
    if (!yield) {
        HARVEST_DEBUG("Harvest target no longer valid or already depleted");
        cancelHarvest();
        return;
    }

    auto& edm = EntityDataManager::Instance();

    // Try to add directly to player inventory
    uint32_t playerInvIdx = player->getInventoryIndex();
    bool addedToInventory = false;

    if (playerInvIdx != INVALID_INVENTORY_INDEX) {
        // Get old quantity with targeted lookup (avoids full inventory scan)
        int oldQuantity = edm.getInventoryQuantity(playerInvIdx, yield->resource);

        addedToInventory = edm.addToInventory(playerInvIdx, yield->resource, yield->quantity);

        // Fire ResourceChangeEvent for UI updates (only if added to inventory)
        if (addedToInventory) {
            auto resourceChangeEvent = std::make_shared<ResourceChangeEvent>(
                player->getHandle(),
                yield->resource,
                oldQuantity,
                oldQuantity + yield->quantity,
                "harvested");
            EventManager::Instance().dispatchEvent(resourceChangeEvent);
        }
    } else {
        HARVEST_WARN("Player has no valid inventory index!");
    }

    // If inventory full or no inventory, spawn as dropped item
    if (!addedToInventory) {
        // Spawn slightly offset from harvestable position
        Vector2D spawnPos = yield->position;
        spawnPos.setX(spawnPos.getX() + 16.0f);

        const std::string& worldId = WorldResourceManager::Instance().getActiveWorld();
        edm.createDroppedItem(spawnPos, yield->resource, yield->quantity, worldId);

        HARVEST_INFO(std::format("Completed {} - {} x{} (dropped)",
            VoidLight::harvestTypeToString(m_currentType),
            yield->resource.toString(), yield->quantity));
    } else {
        HARVEST_INFO(std::format("Completed {} - {} x{} (added to inventory)",
            VoidLight::harvestTypeToString(m_currentType),
            yield->resource.toString(), yield->quantity));
    }

    // Reset harvest state
    m_isHarvesting = false;
    m_harvestTimer = 0.0f;
    m_harvestDuration = 0.0f;
    m_currentTarget = EntityHandle{};
    m_currentType = VoidLight::HarvestType::Gathering;
}
