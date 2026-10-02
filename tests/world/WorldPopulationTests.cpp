/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE WorldPopulationTests
#include <boost/test/unit_test.hpp>

#include "ai/BehaviorConfig.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "ai/FactionStance.hpp"
#include "core/ThreadSystem.hpp"
#include "events/EntityEvents.hpp"
#include "events/StanceChangedEvent.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "world/NpcSpawn.hpp"
#include "world/WorldData.hpp"
#include "world/WorldPopulation.hpp"

#include <memory>
#include <optional>
#include <vector>

using namespace VoidLight;

namespace {

WorldGenerationConfig makePopulatedWorldConfig(int seed) {
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

[[nodiscard]] std::vector<NpcSnapshot> collectNpcs() {
    auto& edm = EntityDataManager::Instance();
    auto npcSpan = edm.getIndicesByKind(EntityKind::NPC);
    std::vector<NpcSnapshot> npcs;
    npcs.reserve(npcSpan.size());
    for (size_t idx : npcSpan) {
        EntityHandle handle = edm.getHandle(idx);
        if (!handle.isValid() || edm.getIndex(handle) == SIZE_MAX) {
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
    ThreadSystemFixture() {
        if (!ThreadSystem::Instance().init()) {
            throw std::runtime_error("Failed to initialize ThreadSystem for world population tests");
        }
    }
    ~ThreadSystemFixture() {
        ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

struct WorldPopulationFixture {
    WorldPopulationFixture() {
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(AIManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        BOOST_REQUIRE(WorldManager::Instance().init());
    }

    ~WorldPopulationFixture() {
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

BOOST_AUTO_TEST_CASE(TestLoadNewWorldPopulatesSettlementNpcs) {
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
    size_t wildernessCount = 0;
    for (const auto& npc : npcs) {
        BOOST_CHECK_EQUAL(npc.behaviorType, npc.homeRole);
        BOOST_CHECK_NE(npc.homeRole, static_cast<uint8_t>(BehaviorType::None));
        if (npc.merchant) {
            ++merchantCount;
        }
        if (npc.homeRole == static_cast<uint8_t>(BehaviorType::Idle)) {
            ++idleCount;
        }
        if (npc.homeRole == static_cast<uint8_t>(BehaviorType::Guard)) {
            ++guardCount;
        }
        if (npc.homeRole == static_cast<uint8_t>(BehaviorType::Wander)) {
            ++wanderCount;
        }
        if (npc.faction == 1) {
            BOOST_CHECK_EQUAL(npc.homeRole, static_cast<uint8_t>(BehaviorType::Chase));
            BOOST_CHECK(AIManager::Instance().getStance(1, 0) == FactionStance::Neutral);
            ++wildernessCount;
        }
    }

    BOOST_CHECK_EQUAL(merchantCount, settlements.size());
    BOOST_CHECK_EQUAL(idleCount, settlements.size());
    BOOST_CHECK_EQUAL(guardCount, 2 * settlements.size());
    BOOST_CHECK_EQUAL(wanderCount, 4 * settlements.size());
    BOOST_CHECK_LE(wildernessCount, WorldPopulation::MAX_WILDERNESS_NPCS_PER_WORLD);
}

BOOST_AUTO_TEST_CASE(TestUnloadThenReloadReplacesPopulatedNpcs) {
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();

    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));
    const std::string firstWorldId = worldMgr.getCurrentWorldId();
    BOOST_REQUIRE(worldMgr.isWorldPopulated(firstWorldId));

    const auto firstNpcs = collectNpcs();
    BOOST_REQUIRE(!firstNpcs.empty());
    std::vector<EntityHandle> firstHandles;
    firstHandles.reserve(firstNpcs.size());
    for (const auto& npc : firstNpcs) {
        firstHandles.push_back(npc.handle);
    }

    worldMgr.unloadWorld();
    BOOST_CHECK(!worldMgr.isWorldPopulated(firstWorldId));
    BOOST_CHECK_EQUAL(worldMgr.getPopulatedNpcCount(firstWorldId), 0u);
    BOOST_CHECK(worldMgr.getSettlements().empty());
    for (const EntityHandle& handle : firstHandles) {
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
    for (const EntityHandle& handle : firstHandles) {
        BOOST_CHECK_EQUAL(edm.getIndex(handle), SIZE_MAX);
    }
    for (const auto& npc : secondNpcs) {
        BOOST_CHECK_NE(edm.getIndex(npc.handle), SIZE_MAX);
    }
}

BOOST_AUTO_TEST_CASE(TestSettlementPointQueries) {
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

BOOST_AUTO_TEST_CASE(TestPauseDoesNotPopulate) {
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

BOOST_AUTO_TEST_CASE(TestDistantPopulatedNpcsRetier) {
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const auto npcs = collectNpcs();
    BOOST_REQUIRE(!npcs.empty());

    edm.updateSimulationTiers(Vector2D(0.0f, 0.0f), 50.0f, 100.0f);

    bool sawNonActive = false;
    for (const auto& npc : npcs) {
        const size_t idx = edm.getIndex(npc.handle);
        BOOST_REQUIRE_NE(idx, SIZE_MAX);
        if (edm.getHotDataByIndex(idx).tier != SimulationTier::Active) {
            sawNonActive = true;
            break;
        }
    }
    BOOST_CHECK(sawNonActive);
}

BOOST_AUTO_TEST_CASE(TestUnloadDestroysHarvestablesWithoutEdmTransition) {
    // Public unload drains NPCs and immediately destroys static harvestables.
    // No EntityDataManager::prepareForStateTransition is required.
    auto& worldMgr = WorldManager::Instance();
    auto& wrm = WorldResourceManager::Instance();
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const std::string worldId = worldMgr.getCurrentWorldId();
    BOOST_REQUIRE_GT(wrm.getHarvestableCount(worldId), 0u);
    BOOST_REQUIRE_GT(edm.getEntityCount(EntityKind::Harvestable), 0u);

    worldMgr.unloadWorld();

    BOOST_CHECK_EQUAL(wrm.getHarvestableCount(worldId), 0u);
    BOOST_CHECK_EQUAL(edm.getEntityCount(EntityKind::Harvestable), 0u);
    BOOST_CHECK(!worldMgr.isWorldPopulated(worldId));
}

BOOST_AUTO_TEST_CASE(TestSpawnNpcHelperBehaviorOverride) {
    auto& edm = EntityDataManager::Instance();

    EntityHandle guard = spawnNpc(Vector2D(32.0f, 32.0f), "Human", "Guard");
    BOOST_REQUIRE(guard.isValid());
    const size_t guardIdx = edm.getIndex(guard);
    BOOST_REQUIRE_NE(guardIdx, SIZE_MAX);
    const auto& guardData = edm.getCharacterDataByIndex(guardIdx);
    BOOST_CHECK_EQUAL(guardData.homeRole, static_cast<uint8_t>(BehaviorType::Guard));
    BOOST_CHECK_EQUAL(guardData.behaviorType, static_cast<uint8_t>(BehaviorType::Guard));

    EntityHandle warrior = spawnNpc(Vector2D(64.0f, 32.0f), "Human", "Warrior",
        Sex::Unknown, 1, "Attack");
    BOOST_REQUIRE(warrior.isValid());
    const size_t warriorIdx = edm.getIndex(warrior);
    BOOST_REQUIRE_NE(warriorIdx, SIZE_MAX);
    const auto& warriorData = edm.getCharacterDataByIndex(warriorIdx);
    BOOST_CHECK_EQUAL(warriorData.homeRole, static_cast<uint8_t>(BehaviorType::Attack));
    BOOST_CHECK_EQUAL(warriorData.behaviorType, static_cast<uint8_t>(BehaviorType::Attack));
    BOOST_CHECK_EQUAL(warriorData.faction, 1);
}

BOOST_AUTO_TEST_CASE(TestClearPopulatedNpcsKeepsWorld) {
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

BOOST_AUTO_TEST_CASE(TestPopulateFalseLoadsWorldWithoutNpcs) {
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();
    auto& wrm = WorldResourceManager::Instance();

    WorldGenerationConfig config = makePopulatedWorldConfig(55555);
    config.populate = false;
    BOOST_REQUIRE(worldMgr.loadNewWorld(config));

    const std::string worldId = worldMgr.getCurrentWorldId();
    BOOST_REQUIRE(!worldId.empty());
    BOOST_CHECK(!worldMgr.isWorldPopulated(worldId));
    BOOST_CHECK_EQUAL(worldMgr.getPopulatedNpcCount(worldId), 0u);
    BOOST_CHECK_EQUAL(edm.getEntityCount(EntityKind::NPC), 0u);
    BOOST_CHECK(collectNpcs().empty());
    BOOST_CHECK_GT(wrm.getHarvestableCount(worldId), 0u);
    BOOST_CHECK_GT(edm.getEntityCount(EntityKind::Harvestable), 0u);
    BOOST_CHECK(!worldMgr.getSettlements().empty());
}

BOOST_AUTO_TEST_CASE(TestPopulatedMerchantHasLeashedNeed) {
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    // No AI update has run: each need entry's home is the NPC's spawn position.
    size_t leashedMerchants = 0;
    size_t unleashedCivilians = 0;
    for (const auto& npc : collectNpcs()) {
        const size_t idx = edm.getIndex(npc.handle);
        const NpcNeedData* need = edm.npcNeedSidecar().get(static_cast<uint32_t>(idx));
        if (npc.merchant) {
            BOOST_REQUIRE(need != nullptr);
        }
        if (need == nullptr) {
            continue;
        }
        const Vector2D position = edm.getTransformByIndex(idx).position;
        BOOST_CHECK_EQUAL(need->home.getX(), position.getX());
        BOOST_CHECK_EQUAL(need->home.getY(), position.getY());
        if (npc.merchant) {
            BOOST_CHECK_EQUAL(need->leashRadius, Behaviors::MERCHANT_FORAGE_LEASH_RADIUS);
            ++leashedMerchants;
        } else {
            BOOST_CHECK_EQUAL(need->leashRadius, 0.0f);
            ++unleashedCivilians;
        }
    }
    BOOST_CHECK_GT(leashedMerchants, 0u);
    BOOST_CHECK_GT(unleashedCivilians, 0u);
}

BOOST_AUTO_TEST_CASE(TestPopulateUsesSettlementFaction) {
    WorldData world;
    world.worldId = "settlement-faction";
    constexpr int kSize = 16;
    world.grid.assign(static_cast<size_t>(kSize), std::vector<Tile>(static_cast<size_t>(kSize)));

    SettlementRecord record;
    record.id = 1;
    record.centerTileX = 8;
    record.centerTileY = 8;
    record.radiusTiles = 5;
    record.faction = 3;
    world.settlements.push_back(record);

    std::vector<EntityHandle> handles;
    WorldPopulation::populate(world, handles);
    BOOST_REQUIRE(!handles.empty());

    auto& edm = EntityDataManager::Instance();
    for (const EntityHandle& handle : handles) {
        const size_t idx = edm.getIndex(handle);
        BOOST_REQUIRE_NE(idx, SIZE_MAX);
        BOOST_CHECK_EQUAL(edm.getCharacterDataByIndex(idx).faction, record.faction);
    }
}

BOOST_AUTO_TEST_CASE(TestQueryTerritoryAtVillageCenter) {
    auto& worldMgr = WorldManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    const auto settlements = worldMgr.getSettlements();
    BOOST_REQUIRE(!settlements.empty());
    const SettlementRecord& record = settlements.front();

    const float centerPixelX =
        (static_cast<float>(record.centerTileX) + 0.5f) * TILE_SIZE;
    const float centerPixelY =
        (static_cast<float>(record.centerTileY) + 0.5f) * TILE_SIZE;
    const auto atCenter =
        AIManager::Instance().queryTerritoryAtPixel(centerPixelX, centerPixelY);
    BOOST_REQUIRE(atCenter.has_value());
    BOOST_CHECK_EQUAL(atCenter->settlementId, record.id);
    BOOST_CHECK_EQUAL(atCenter->faction, record.faction);

    const auto atTile = AIManager::Instance().queryTerritoryAtTile(
        record.centerTileX, record.centerTileY);
    BOOST_REQUIRE(atTile.has_value());
    BOOST_CHECK_EQUAL(atTile->settlementId, record.id);
    BOOST_CHECK_EQUAL(atTile->faction, record.faction);

    const float outsidePixelX =
        centerPixelX + static_cast<float>(record.radiusTiles + 1) * TILE_SIZE;
    BOOST_CHECK(!AIManager::Instance()
            .queryTerritoryAtPixel(outsidePixelX, centerPixelY)
            .has_value());
}

BOOST_AUTO_TEST_CASE(TestPlayerIncidentsOnPopulatedWorldIsolateFactions) {
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();
    auto& ai = AIManager::Instance();
    auto& eventMgr = EventManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    // Populated settlement merchant (Idle, settlement faction).
    EntityHandle merchant{};
    for (const auto& npc : collectNpcs()) {
        if (npc.merchant) {
            merchant = npc.handle;
            break;
        }
    }
    BOOST_REQUIRE(merchant.isValid());
    const size_t merchantIdx = edm.getIndex(merchant);
    BOOST_REQUIRE_NE(merchantIdx, SIZE_MAX);
    BOOST_REQUIRE(edm.getBehaviorConfigRef(merchantIdx).type == BehaviorType::Idle);
    const uint8_t villageFaction = edm.getCharacterDataByIndex(merchantIdx).faction;
    BOOST_REQUIRE_NE(villageFaction, 1);
    const Vector2D merchantPos = edm.getHotDataByIndex(merchantIdx).transform.position;
    const auto village = ai.queryTerritoryAtPixel(merchantPos.getX(), merchantPos.getY());
    BOOST_REQUIRE(village.has_value());
    BOOST_REQUIRE_NE(village->settlementId, 0u);

    const EntityHandle player =
        edm.registerPlayer(990001, merchantPos + Vector2D(80.0f, 0.0f));
    BOOST_REQUIRE(player.isValid());
    ai.setPlayerHandle(player);

    // Faction-1 Warrior right next to the merchant.
    const EntityHandle warrior = spawnNpc(merchantPos + Vector2D(40.0f, 0.0f),
        "Human", "Warrior", Sex::Unknown, 1);
    BOOST_REQUIRE(warrior.isValid());

    int factionEvents = 0;
    int towardPlayerEvents = 0;
    uint8_t eventFaction = 255;
    uint32_t eventSettlement = 0;
    eventMgr.registerHandler(EventTypeId::StanceChanged, [&](const EventData& data) {
        const auto* event = dynamic_cast<const StanceChangedEvent*>(data.event.get());
        if (!event) {
            return;
        }
        if (!event->isTowardPlayer()) {
            ++factionEvents;
            return;
        }
        ++towardPlayerEvents;
        eventFaction = event->getFromFaction();
        eventSettlement = event->getSettlementId();
    });

    // Player hits the Warrior: standing with faction 1 only; no stance write.
    auto hit = std::make_shared<DamageEvent>(
        EntityEventType::DamageIntent, player, warrior, 10.0f);
    eventMgr.dispatchEvent(hit, EventManager::DispatchMode::Immediate);
    BOOST_CHECK(ai.getStance(villageFaction, 1) == FactionStance::Neutral);
    BOOST_CHECK(ai.getStance(1, villageFaction) == FactionStance::Neutral);
    BOOST_CHECK_EQUAL(ai.getPlayerStanding(player, 1), AIManager::PLAYER_STANDING_ASSAULT_DELTA);
    BOOST_CHECK_EQUAL(ai.getPlayerStanding(player, villageFaction), 0);
    BOOST_CHECK_EQUAL(factionEvents, 0);

    edm.updateSimulationTiers(merchantPos, 1500.0f, 3000.0f);
    for (int i = 0; i < 10; ++i) {
        ai.update(0.016f);
    }
    BOOST_CHECK(edm.getBehaviorConfigRef(merchantIdx).type == BehaviorType::Idle);
    BOOST_CHECK(ai.getStance(villageFaction, 1) == FactionStance::Neutral);
    BOOST_CHECK(ai.getStance(1, villageFaction) == FactionStance::Neutral);

    // Theft and gift against the merchant touch only the village faction's standing.
    ai.recordPlayerIncident(AIManager::PlayerIncident::Theft, player, merchant);
    ai.recordPlayerIncident(AIManager::PlayerIncident::Gift, player, merchant);
    BOOST_CHECK_EQUAL(ai.getPlayerStanding(player, villageFaction),
        AIManager::PLAYER_STANDING_THEFT_DELTA + AIManager::PLAYER_STANDING_GIFT_DELTA);
    BOOST_CHECK_EQUAL(ai.getPlayerStanding(player, 1), AIManager::PLAYER_STANDING_ASSAULT_DELTA);
    BOOST_CHECK_EQUAL(towardPlayerEvents, 0);

    // Crossing the Hostile threshold inside the village reports that settlement.
    while (ai.getPlayerRelation(villageFaction) != FactionStance::Hostile) {
        ai.recordPlayerIncident(AIManager::PlayerIncident::Theft, player, merchant);
    }
    BOOST_CHECK_EQUAL(towardPlayerEvents, 1);
    BOOST_CHECK_EQUAL(eventFaction, villageFaction);
    BOOST_CHECK_EQUAL(eventSettlement, village->settlementId);
    BOOST_CHECK_EQUAL(factionEvents, 0);
    BOOST_CHECK(ai.getPlayerRelation(1) == FactionStance::Neutral);
}

BOOST_AUTO_TEST_CASE(TestNpcCombatStanceEventCarriesTerritorySettlement) {
    auto& worldMgr = WorldManager::Instance();
    auto& edm = EntityDataManager::Instance();
    auto& ai = AIManager::Instance();
    auto& eventMgr = EventManager::Instance();
    BOOST_REQUIRE(worldMgr.loadNewWorld(makePopulatedWorldConfig(55555)));

    EntityHandle merchant{};
    for (const auto& npc : collectNpcs()) {
        if (npc.merchant) {
            merchant = npc.handle;
            break;
        }
    }
    BOOST_REQUIRE(merchant.isValid());
    const size_t merchantIdx = edm.getIndex(merchant);
    BOOST_REQUIRE_NE(merchantIdx, SIZE_MAX);
    const uint8_t villageFaction = edm.getCharacterDataByIndex(merchantIdx).faction;
    BOOST_REQUIRE_NE(villageFaction, 1);
    const Vector2D merchantPos = edm.getHotDataByIndex(merchantIdx).transform.position;
    const auto village = ai.queryTerritoryAtPixel(merchantPos.getX(), merchantPos.getY());
    BOOST_REQUIRE(village.has_value());
    BOOST_REQUIRE_NE(village->settlementId, 0u);

    std::vector<uint32_t> factionEventSettlements;
    int towardPlayerEvents = 0;
    eventMgr.registerHandler(EventTypeId::StanceChanged, [&](const EventData& data) {
        const auto* event = dynamic_cast<const StanceChangedEvent*>(data.event.get());
        if (!event) {
            return;
        }
        if (event->isTowardPlayer()) {
            ++towardPlayerEvents;
            return;
        }
        factionEventSettlements.push_back(event->getSettlementId());
    });

    // A faction-1 NPC hits the village merchant inside the village: both
    // directed cells turn Hostile and both events carry the village id.
    const EntityHandle raider = spawnNpc(merchantPos + Vector2D(20.0f, 0.0f),
        "Human", "Warrior", Sex::Unknown, 1);
    BOOST_REQUIRE(raider.isValid());
    auto villageHit = std::make_shared<DamageEvent>(
        EntityEventType::DamageIntent, raider, merchant, 10.0f);
    eventMgr.dispatchEvent(villageHit, EventManager::DispatchMode::Immediate);
    BOOST_CHECK(ai.isHostileTo(villageFaction, 1));
    BOOST_CHECK(ai.isHostileTo(1, villageFaction));
    BOOST_REQUIRE_EQUAL(factionEventSettlements.size(), 2u);
    BOOST_CHECK_EQUAL(factionEventSettlements[0], village->settlementId);
    BOOST_CHECK_EQUAL(factionEventSettlements[1], village->settlementId);

    // A wilderness hit (victim outside every settlement) carries 0.
    std::optional<Vector2D> wildernessPos;
    for (int ty = 1; ty < 99 && !wildernessPos; ty += 7) {
        for (int tx = 1; tx < 99; tx += 7) {
            const Vector2D pos((static_cast<float>(tx) + 0.5f) * TILE_SIZE,
                (static_cast<float>(ty) + 0.5f) * TILE_SIZE);
            if (!ai.queryTerritoryAtPixel(pos.getX(), pos.getY()).has_value()) {
                wildernessPos = pos;
                break;
            }
        }
    }
    BOOST_REQUIRE(wildernessPos.has_value());
    const EntityHandle wildAttacker = spawnNpc(*wildernessPos, "Human", "Warrior",
        Sex::Unknown, 2);
    const EntityHandle wildVictim = spawnNpc(*wildernessPos + Vector2D(20.0f, 0.0f),
        "Human", "Warrior", Sex::Unknown, 3);
    BOOST_REQUIRE(wildAttacker.isValid());
    BOOST_REQUIRE(wildVictim.isValid());
    const size_t wildVictimIdx = edm.getIndex(wildVictim);
    BOOST_REQUIRE_NE(wildVictimIdx, SIZE_MAX);
    const Vector2D wildVictimPos = edm.getHotDataByIndex(wildVictimIdx).transform.position;
    BOOST_REQUIRE(!ai.queryTerritoryAtPixel(wildVictimPos.getX(), wildVictimPos.getY())
            .has_value());

    factionEventSettlements.clear();
    auto wildHit = std::make_shared<DamageEvent>(
        EntityEventType::DamageIntent, wildAttacker, wildVictim, 10.0f);
    eventMgr.dispatchEvent(wildHit, EventManager::DispatchMode::Immediate);
    BOOST_CHECK(ai.isHostileTo(3, 2));
    BOOST_CHECK(ai.isHostileTo(2, 3));
    BOOST_REQUIRE_EQUAL(factionEventSettlements.size(), 2u);
    BOOST_CHECK_EQUAL(factionEventSettlements[0], 0u);
    BOOST_CHECK_EQUAL(factionEventSettlements[1], 0u);
    BOOST_CHECK_EQUAL(towardPlayerEvents, 0);
}

BOOST_AUTO_TEST_SUITE_END()
