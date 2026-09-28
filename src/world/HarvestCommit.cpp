/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "world/HarvestCommit.hpp"
#include "core/Logger.hpp"
#include "events/HarvestResourceEvent.hpp"
#include "events/ScarcityEvent.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "world/WorldData.hpp"
#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <random>
#include <string>

namespace VoidLight::HarvestCommit {

std::optional<HarvestYield> commit(EntityHandle harvestable,
    EntityHandle harvester,
    size_t minRemaining) {
    auto& edm = EntityDataManager::Instance();

    // Generation-safe validation: static pool slots are reused with a new
    // generation on destroy, so a bare index/isAlive()/kind check could accept
    // a different harvestable that now occupies the slot.
    const size_t staticIndex = edm.getIndex(harvestable);
    if (harvestable.kind != EntityKind::Harvestable || staticIndex == SIZE_MAX ||
        edm.getStaticHandle(staticIndex) != harvestable) {
        HARVEST_DEBUG("HarvestCommit: stale harvestable handle");
        return std::nullopt;
    }

    const auto& hot = edm.getStaticHotDataByIndex(staticIndex);
    const uint32_t typeLocalIndex = hot.typeLocalIndex;
    const HarvestableData& data = edm.getHarvestableData(typeLocalIndex);
    if (data.isDepleted) {
        HARVEST_DEBUG("HarvestCommit: harvestable already depleted");
        return std::nullopt;
    }

    const Vector2D position = hot.transform.position;
    auto& wrm = WorldResourceManager::Instance();

    // Count includes this node (any harvestable kind in the area).
    const size_t available = wrm.countAvailableHarvestablesInRadius(position, SCARCITY_RADIUS);
    const size_t remaining = available > 0 ? available - 1 : 0;
    if (remaining < minRemaining) {
        HARVEST_DEBUG(std::format("HarvestCommit: reserve {} would be violated ({} remaining)",
            minRemaining, remaining));
        return std::nullopt;
    }

    HarvestYield yield{data.yieldResource, data.yieldMin, position};
    if (data.yieldMax > data.yieldMin) {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(data.yieldMin, data.yieldMax);
        yield.quantity = dist(rng);
    }

    edm.markHarvestableDepleted(typeLocalIndex);
    wrm.notifyHarvestableStateChanged();

    // Tile visual update. The WorldManager handler checks whether the tile
    // actually has an obstacle before modifying it, so tile-based and
    // EDM-based harvestables coexist.
    const int tileX = static_cast<int>(position.getX() / TILE_SIZE);
    const int tileY = static_cast<int>(position.getY() / TILE_SIZE);
    auto& eventMgr = EventManager::Instance();
    eventMgr.dispatchEvent(std::make_shared<HarvestResourceEvent>(
        static_cast<int>(harvestable.getId()),
        tileX,
        tileY,
        std::string(yield.resource.toString())));

    if (remaining < SCARCITY_THRESHOLD) {
        const auto count = static_cast<uint16_t>(
            std::min<size_t>(remaining, std::numeric_limits<uint16_t>::max()));
        eventMgr.dispatchEvent(std::make_shared<ScarcityEvent>(
            position, SCARCITY_RADIUS, count, yield.resource, harvester));
    }

    return yield;
}

} // namespace VoidLight::HarvestCommit
