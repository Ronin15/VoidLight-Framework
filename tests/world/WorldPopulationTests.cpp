/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE WorldPopulationTests
#include <boost/test/unit_test.hpp>

#include "ai/BehaviorConfig.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "world/WorldData.hpp"
#include "world/WorldPopulation.hpp"

#include <vector>

using namespace VoidLight;

namespace {

WorldGenerationConfig makePopulatedWorldConfig(int seed)
{
    WorldGenerationConfig config;
    config.width = 100;
    config.height = 100;
    config.seed = seed;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.2f;
    config.mountainLevel = 0.9f;
    return config;
}

struct NpcSnapshot {
    EntityHandle handle;
    bool merchant{false};
    uint8_t homeRole{0};
    uint8_t behaviorType{0};
    uint8_t faction{0};
};

[[nodiscard]] std::vector<NpcSnapshot> collectNpcs()
{
    auto& edm = EntityDataManager::Instance();
    auto npcSpan = edm.getIndicesByKind(EntityKind::NPC);
    std::vector<NpcSnapshot> npcs;
    npcs.reserve(npcSpan.size());
    for (size_t idx : npcSpan)
    {
        EntityHandle handle = edm.getHandle(idx);
        if (!handle.isValid() || edm.getIndex(handle) == SIZE_MAX)
        {
            continue;
        }
        const auto& charData = edm.getCharacterDataByIndex(idx);
        npcs.push_back({handle, charData.isMerchant(), charData.homeRole,
                        charData.behaviorType, charData.faction});
    }
    return npcs;
}

} // namespace

struct ThreadSystemFixture {
    ThreadSystemFixture()
    {
        if (!ThreadSystem::Instance().init()) {
            throw std::runtime_error("Failed to initialize ThreadSystem for world population tests");
        }
    }
    ~ThreadSystemFixture()
    {
        ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

struct WorldPopulationFixture {
    WorldPopulationFixture()
    {
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(AIManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        BOOST_REQUIRE(WorldManager::Instance().init());
    }

    ~WorldPopulationFixture()
    {
        WorldManager::Instance().clean();
        WorldResourceManager::Instance().clean();
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        EventManager::Instance().clean();
    }
};

BOOST_FIXTURE_TEST_SUITE(WorldPopulationTestSuite, WorldPopulationFixture)

BOOST_AUTO_TEST_CASE(TestLoadNewWorldPopulatesSettlementNpcs)
{
    auto& worldMgr = WorldManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const std::string worldId = worldMgr.getCurrentWorldId();
    BOOST_REQUIRE(!worldId.empty());
    BOOST_CHECK(worldMgr.isWorldPopulated(worldId));
    BOOST_CHECK_GT(worldMgr.getPopulatedNpcCount(worldId), 0u);
    BOOST_CHECK_LE(worldMgr.getPopulatedNpcCount(worldId),
                   WorldPopulation::MAX_POPULATED_NPCS_PER_WORLD);

    const auto npcs = collectNpcs();
    BOOST_REQUIRE(!npcs.empty());

    const auto settlements = worldMgr.getSettlements();
    BOOST_REQUIRE(!settlements.empty());

    size_t merchantCount = 0;
    size_t idleCount = 0;
    size_t guardCount = 0;
    size_t wanderCount = 0;
    size_t hostileCount = 0;
    for (const auto& npc : npcs)
    {
        BOOST_CHECK_EQUAL(npc.behaviorType, npc.homeRole);
        BOOST_CHECK_NE(npc.homeRole, static_cast<uint8_t>(BehaviorType::None));
        if (npc.merchant)
        {
            ++merchantCount;
        }
        if (npc.homeRole == static_cast<uint8_t>(BehaviorType::Idle))
        {
            ++idleCount;
        }
        if (npc.homeRole == static_cast<uint8_t>(BehaviorType::Guard))
        {
            ++guardCount;
        }
        if (npc.homeRole == static_cast<uint8_t>(BehaviorType::Wander))
        {
            ++wanderCount;
        }
        if (npc.homeRole == static_cast<uint8_t>(BehaviorType::Attack))
        {
            BOOST_CHECK_EQUAL(npc.faction, 1);
            ++hostileCount;
        }
    }

    BOOST_CHECK_EQUAL(merchantCount, settlements.size());
    BOOST_CHECK_EQUAL(idleCount, settlements.size());
    BOOST_CHECK_EQUAL(guardCount, 2 * settlements.size());
    BOOST_CHECK_EQUAL(wanderCount, 4 * settlements.size());
    BOOST_CHECK_LE(hostileCount, WorldPopulation::MAX_HOSTILES_PER_WORLD);
}

BOOST_AUTO_TEST_CASE(TestUnloadThenReloadReplacesPopulatedNpcs)
{
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();

    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));
    const std::string firstWorldId = worldMgr.getCurrentWorldId();
    BOOST_REQUIRE(worldMgr.isWorldPopulated(firstWorldId));

    const auto firstNpcs = collectNpcs();
    BOOST_REQUIRE(!firstNpcs.empty());
    std::vector<EntityHandle> firstHandles;
    firstHandles.reserve(firstNpcs.size());
    for (const auto& npc : firstNpcs)
    {
        firstHandles.push_back(npc.handle);
    }

    worldMgr.unloadWorld();
    BOOST_CHECK(!worldMgr.isWorldPopulated(firstWorldId));
    BOOST_CHECK_EQUAL(worldMgr.getPopulatedNpcCount(firstWorldId), 0u);
    BOOST_CHECK(worldMgr.getSettlements().empty());
    for (const EntityHandle& handle : firstHandles)
    {
        BOOST_CHECK_EQUAL(edm.getIndex(handle), SIZE_MAX);
    }

    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(77777)));
    const std::string secondWorldId = worldMgr.getCurrentWorldId();
    BOOST_REQUIRE(!secondWorldId.empty());
    BOOST_CHECK(worldMgr.isWorldPopulated(secondWorldId));
    BOOST_CHECK_EQUAL(worldMgr.getPopulatedNpcCount(firstWorldId), 0u);
    BOOST_CHECK_GT(worldMgr.getPopulatedNpcCount(secondWorldId), 0u);

    const auto secondNpcs = collectNpcs();
    BOOST_REQUIRE(!secondNpcs.empty());
    for (const EntityHandle& handle : firstHandles)
    {
        BOOST_CHECK_EQUAL(edm.getIndex(handle), SIZE_MAX);
    }
    for (const auto& npc : secondNpcs)
    {
        BOOST_CHECK_NE(edm.getIndex(npc.handle), SIZE_MAX);
    }
}

BOOST_AUTO_TEST_CASE(TestSettlementPointQueries)
{
    auto& worldMgr = WorldManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const auto settlements = worldMgr.getSettlements();
    BOOST_REQUIRE(!settlements.empty());

    const SettlementRecord& record = settlements.front();
    const auto atCenterTile = worldMgr.findSettlementAtTile(record.centerTileX, record.centerTileY);
    BOOST_REQUIRE(atCenterTile.has_value());
    BOOST_CHECK_EQUAL(atCenterTile->id, record.id);

    const float centerPixelX =
        (static_cast<float>(record.centerTileX) + 0.5f) * TILE_SIZE;
    const float centerPixelY =
        (static_cast<float>(record.centerTileY) + 0.5f) * TILE_SIZE;
    const auto atCenterPixel = worldMgr.findSettlementAtPixel(centerPixelX, centerPixelY);
    BOOST_REQUIRE(atCenterPixel.has_value());
    BOOST_CHECK_EQUAL(atCenterPixel->id, record.id);

    const int outsideTileX = record.centerTileX + record.radiusTiles + 1;
    const int outsideTileY = record.centerTileY;
    BOOST_CHECK(!worldMgr.findSettlementAtTile(outsideTileX, outsideTileY).has_value());

    const float outsidePixelX = centerPixelX + static_cast<float>(record.radiusTiles + 1) * TILE_SIZE;
    BOOST_CHECK(!worldMgr.findSettlementAtPixel(outsidePixelX, centerPixelY).has_value());
}

BOOST_AUTO_TEST_CASE(TestPauseDoesNotPopulate)
{
    // GamePlayState::pause/resume never call loadNewWorld. This suite
    // asserts the populate registry is unchanged under AI pause.
    auto& worldMgr = WorldManager::Instance();
    auto& ai = AIManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const std::string worldId = worldMgr.getCurrentWorldId();
    const size_t countAfterLoad = worldMgr.getPopulatedNpcCount(worldId);
    BOOST_REQUIRE_GT(countAfterLoad, 0u);

    ai.setGlobalPause(true);
    BOOST_CHECK_EQUAL(worldMgr.getPopulatedNpcCount(worldId), countAfterLoad);
    ai.setGlobalPause(false);
    BOOST_CHECK_EQUAL(worldMgr.getPopulatedNpcCount(worldId), countAfterLoad);
}

BOOST_AUTO_TEST_CASE(TestDistantPopulatedNpcsRetier)
{
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const auto npcs = collectNpcs();
    BOOST_REQUIRE(!npcs.empty());

    edm.updateSimulationTiers(Vector2D(0.0f, 0.0f), 50.0f, 100.0f);

    bool sawNonActive = false;
    for (const auto& npc : npcs)
    {
        const size_t idx = edm.getIndex(npc.handle);
        BOOST_REQUIRE_NE(idx, SIZE_MAX);
        if (edm.getHotDataByIndex(idx).tier != SimulationTier::Active)
        {
            sawNonActive = true;
            break;
        }
    }
    BOOST_CHECK(sawNonActive);
}

BOOST_AUTO_TEST_CASE(TestClearPopulatedNpcsKeepsWorld)
{
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const std::string worldId = worldMgr.getCurrentWorldId();
    BOOST_REQUIRE(worldMgr.isWorldPopulated(worldId));
    BOOST_REQUIRE(!worldMgr.getSettlements().empty());

    worldMgr.clearPopulatedNpcs(worldId);
    edm.processDestructionQueue();

    BOOST_CHECK(!worldMgr.isWorldPopulated(worldId));
    BOOST_CHECK_EQUAL(worldMgr.getPopulatedNpcCount(worldId), 0u);
    BOOST_CHECK(worldMgr.hasActiveWorld());
    BOOST_CHECK(!worldMgr.getSettlements().empty());
    BOOST_CHECK(collectNpcs().empty());
}

BOOST_AUTO_TEST_SUITE_END()
