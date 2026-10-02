/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file HarvestControllerTests.cpp
 * @brief Tests for HarvestController
 *
 * Tests progress-based harvesting, HarvestType configs, and harvest flow.
 */

#define BOOST_TEST_MODULE HarvestControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/world/HarvestController.hpp"
#include "entities/Player.hpp"
#include "events/HarvestResourceEvent.hpp"
#include "events/ResourceChangeEvent.hpp"
#include "events/ScarcityEvent.hpp"
#include "world/HarvestCommit.hpp"
#include "world/HarvestType.hpp"
#include "world/HarvestConfig.hpp"
#include "world/WorldData.hpp"
#include "managers/EventManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "../events/EventManagerTestAccess.hpp"
#include <string>
#include <memory>
#include <vector>

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Fixture for HarvestController tests
 *
 * Creates a mock Player-like object since HarvestController requires a player.
 * For unit tests, we create a minimal setup without full Player.
 */
class HarvestControllerTestFixture {
public:
    HarvestControllerTestFixture() {
        // Reset EventManager to clean state
        EventManagerTestAccess::reset();
        BOOST_REQUIRE(EventManager::Instance().init());

        // Initialize EntityDataManager
        BOOST_REQUIRE(EntityDataManager::Instance().init());

        // Initialize WorldResourceManager
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
    }

    ~HarvestControllerTestFixture() {
        WorldResourceManager::Instance().clean();
        EntityDataManager::Instance().clean();
        EventManager::Instance().clean();
    }

    // Non-copyable
    HarvestControllerTestFixture(const HarvestControllerTestFixture&) = delete;
    HarvestControllerTestFixture& operator=(const HarvestControllerTestFixture&) = delete;
};

// ============================================================================
// HarvestType Enum Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(HarvestTypeTests)

BOOST_AUTO_TEST_CASE(TestHarvestTypeToString) {
    using namespace VoidLight;

    BOOST_CHECK_EQUAL(harvestTypeToString(HarvestType::Gathering), "Gathering");
    BOOST_CHECK_EQUAL(harvestTypeToString(HarvestType::Chopping), "Chopping");
    BOOST_CHECK_EQUAL(harvestTypeToString(HarvestType::Mining), "Mining");
    BOOST_CHECK_EQUAL(harvestTypeToString(HarvestType::Quarrying), "Quarrying");
    BOOST_CHECK_EQUAL(harvestTypeToString(HarvestType::Fishing), "Fishing");
}

BOOST_AUTO_TEST_CASE(TestHarvestTypeActionVerb) {
    using namespace VoidLight;

    BOOST_CHECK_EQUAL(harvestTypeToActionVerb(HarvestType::Gathering), "Gathering...");
    BOOST_CHECK_EQUAL(harvestTypeToActionVerb(HarvestType::Chopping), "Chopping...");
    BOOST_CHECK_EQUAL(harvestTypeToActionVerb(HarvestType::Mining), "Mining...");
    BOOST_CHECK_EQUAL(harvestTypeToActionVerb(HarvestType::Quarrying), "Quarrying...");
    BOOST_CHECK_EQUAL(harvestTypeToActionVerb(HarvestType::Fishing), "Fishing...");
}

BOOST_AUTO_TEST_CASE(TestHarvestTypeCount) {
    using namespace VoidLight;

    // Verify COUNT matches expected number of types
    BOOST_CHECK_EQUAL(static_cast<int>(HarvestType::COUNT), 5);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// HarvestConfig Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(HarvestConfigTests)

BOOST_AUTO_TEST_CASE(TestHarvestTypeConfigDurations) {
    using namespace VoidLight;

    // Gathering should be fastest
    const auto& gatherConfig = getHarvestTypeConfig(HarvestType::Gathering);
    BOOST_CHECK_LT(gatherConfig.baseDuration, 1.0f);

    // Chopping moderate
    const auto& chopConfig = getHarvestTypeConfig(HarvestType::Chopping);
    BOOST_CHECK_GT(chopConfig.baseDuration, 1.0f);
    BOOST_CHECK_LT(chopConfig.baseDuration, 5.0f);

    // Mining slower
    const auto& mineConfig = getHarvestTypeConfig(HarvestType::Mining);
    BOOST_CHECK_GE(mineConfig.baseDuration, 2.0f);

    // All should have valid action verbs
    BOOST_CHECK(!gatherConfig.actionVerb.empty());
    BOOST_CHECK(!chopConfig.actionVerb.empty());
    BOOST_CHECK(!mineConfig.actionVerb.empty());
}

BOOST_AUTO_TEST_CASE(TestIsHarvestableObstacle) {
    using namespace VoidLight;

    // Harvestable obstacles
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::TREE));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::ROCK));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::IRON_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::GOLD_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::COPPER_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::COAL_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::MITHRIL_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::LIMESTONE_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::EMERALD_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::RUBY_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::SAPPHIRE_DEPOSIT));
    BOOST_CHECK(isHarvestableObstacle(ObstacleType::DIAMOND_DEPOSIT));

    // Non-harvestable obstacles
    BOOST_CHECK(!isHarvestableObstacle(ObstacleType::NONE));
    BOOST_CHECK(!isHarvestableObstacle(ObstacleType::WATER));
    BOOST_CHECK(!isHarvestableObstacle(ObstacleType::BUILDING));
}

BOOST_AUTO_TEST_CASE(TestDepositConfigs) {
    using namespace VoidLight;

    // Check ore deposits have valid configs
    const auto& ironConfig = getDepositConfig(ObstacleType::IRON_DEPOSIT);
    BOOST_CHECK_EQUAL(ironConfig.resourceId, "iron_ore");
    BOOST_CHECK_GT(ironConfig.yieldMin, 0);
    BOOST_CHECK_GE(ironConfig.yieldMax, ironConfig.yieldMin);
    BOOST_CHECK_GT(ironConfig.respawnTime, 0.0f);
    BOOST_CHECK(ironConfig.harvestType == HarvestType::Mining);

    // Check tree uses chopping
    const auto& treeConfig = getDepositConfig(ObstacleType::TREE);
    BOOST_CHECK_EQUAL(treeConfig.resourceId, "wood");
    BOOST_CHECK(treeConfig.harvestType == HarvestType::Chopping);

    // Check rock uses quarrying
    const auto& rockConfig = getDepositConfig(ObstacleType::ROCK);
    BOOST_CHECK_EQUAL(rockConfig.resourceId, "stone");
    BOOST_CHECK(rockConfig.harvestType == HarvestType::Quarrying);

    // Check gem deposits
    const auto& diamondConfig = getDepositConfig(ObstacleType::DIAMOND_DEPOSIT);
    BOOST_CHECK_EQUAL(diamondConfig.resourceId, "rough_diamond");
    BOOST_CHECK(diamondConfig.harvestType == HarvestType::Mining);
}

BOOST_AUTO_TEST_CASE(TestGetHarvestTypeForObstacle) {
    using namespace VoidLight;

    BOOST_CHECK(getHarvestTypeForObstacle(ObstacleType::TREE) == HarvestType::Chopping);
    BOOST_CHECK(getHarvestTypeForObstacle(ObstacleType::ROCK) == HarvestType::Quarrying);
    BOOST_CHECK(getHarvestTypeForObstacle(ObstacleType::IRON_DEPOSIT) == HarvestType::Mining);
    BOOST_CHECK(getHarvestTypeForObstacle(ObstacleType::GOLD_DEPOSIT) == HarvestType::Mining);
    BOOST_CHECK(getHarvestTypeForObstacle(ObstacleType::LIMESTONE_DEPOSIT) == HarvestType::Quarrying);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// HarvestController State Tests (No Player Required)
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(HarvestControllerStateTests, HarvestControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestHarvestControllerName) {
    // Create with nullptr player (for basic state tests)
    HarvestController controller(nullptr);

    BOOST_CHECK_EQUAL(controller.getName(), "HarvestController");
}

BOOST_AUTO_TEST_CASE(TestInitialState) {
    HarvestController controller(nullptr);

    BOOST_CHECK(!controller.isHarvesting());
    BOOST_CHECK_EQUAL(controller.getProgress(), 0.0f);
    BOOST_CHECK(controller.getCurrentType() == VoidLight::HarvestType::Gathering);
}

BOOST_AUTO_TEST_CASE(TestStartHarvestWithoutPlayer) {
    HarvestController controller(nullptr);

    // Should fail gracefully without player
    bool result = controller.startHarvest();
    BOOST_CHECK(!result);
    BOOST_CHECK(!controller.isHarvesting());
}

BOOST_AUTO_TEST_CASE(TestCancelHarvestWhenNotHarvesting) {
    HarvestController controller(nullptr);

    // Should not crash when cancelling without active harvest
    controller.cancelHarvest();
    BOOST_CHECK(!controller.isHarvesting());
}

BOOST_AUTO_TEST_CASE(TestUpdateWithoutHarvesting) {
    HarvestController controller(nullptr);

    // Update should be safe when not harvesting
    controller.update(0.016f);
    BOOST_CHECK(!controller.isHarvesting());
    BOOST_CHECK_EQUAL(controller.getProgress(), 0.0f);
}

BOOST_AUTO_TEST_CASE(TestActionVerbDefault) {
    HarvestController controller(nullptr);

    // Default action verb should be Gathering
    std::string_view verb = controller.getActionVerb();
    BOOST_CHECK_EQUAL(verb, "Gathering...");
}

BOOST_AUTO_TEST_CASE(TestConstants) {
    // Verify constants are reasonable; reach is shared with AI foragers.
    BOOST_CHECK_GT(VoidLight::HarvestCommit::HARVEST_RANGE, 0.0f);
    BOOST_CHECK_GT(HarvestController::MOVEMENT_CANCEL_THRESHOLD, 0.0f);
    BOOST_CHECK_LT(HarvestController::MOVEMENT_CANCEL_THRESHOLD, VoidLight::HarvestCommit::HARVEST_RANGE);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// HarvestCommit + Player Path Tests (Slice 6)
// ============================================================================

/**
 * @brief Real Player, a WRM world, and EDM harvestables.
 *
 * Deferred events are recorded by transient handlers and drained with
 * EventManager::update(). Harvestables use a fixed yield (2..2) so the
 * rolled quantity is deterministic.
 */
class HarvestCommitFixture {
public:
    static constexpr int YIELD = 2;
    static constexpr float DT = 0.1f;

    HarvestCommitFixture() {
        EventManagerTestAccess::reset();
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(WorldResourceManager::Instance().init());

        auto& wrm = WorldResourceManager::Instance();
        BOOST_REQUIRE(wrm.createWorld(WORLD_ID));
        wrm.setActiveWorld(WORLD_ID);

        oreHandle = ResourceTemplateManager::Instance().getHandleById("iron_ore");
        BOOST_REQUIRE(oreHandle.isValid());

        player = std::make_shared<Player>();
        player->initializeInventory();
        BOOST_REQUIRE(player->getHandle().isValid());
        BOOST_REQUIRE(player->getInventoryIndex() != INVALID_INVENTORY_INDEX);
        player->setPosition(Vector2D(1000.0f, 1000.0f));

        auto& events = EventManager::Instance();
        events.registerHandler(EventTypeId::Harvest, [this](const EventData& data) {
            if (auto* e = dynamic_cast<const HarvestResourceEvent*>(data.event.get())) {
                harvestEvents.push_back({e->getEntityId(), e->getTargetX(), e->getTargetY()});
            }
        });
        events.registerHandler(EventTypeId::Scarcity, [this](const EventData& data) {
            if (auto* e = dynamic_cast<const ScarcityEvent*>(data.event.get())) {
                scarcityEvents.push_back({e->getCenter(), e->getRadius(), e->getAvailableCount(),
                    e->getResource(), e->getHarvester()});
            }
        });
        events.registerHandler(EventTypeId::ResourceChange, [this](const EventData& data) {
            if (auto* e = dynamic_cast<const ResourceChangeEvent*>(data.event.get())) {
                resourceEvents.push_back({e->getOwnerHandle(), e->getResourceHandle(),
                    e->getQuantityChange()});
            }
        });
    }

    ~HarvestCommitFixture() {
        player.reset();
        WorldResourceManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        EventManager::Instance().clean();
    }

    HarvestCommitFixture(const HarvestCommitFixture&) = delete;
    HarvestCommitFixture& operator=(const HarvestCommitFixture&) = delete;

protected:
    struct HarvestRecord {
        int entityId;
        int tileX;
        int tileY;
    };
    struct ScarcityRecord {
        Vector2D center;
        float radius;
        uint16_t availableCount;
        VoidLight::ResourceHandle resource;
        EntityHandle harvester;
    };
    struct ResourceRecord {
        EntityHandle owner;
        VoidLight::ResourceHandle resource;
        int delta;
    };

    inline static const std::string WORLD_ID = "harvest_commit_world";

    EntityHandle createNode(const Vector2D& position) {
        EntityHandle handle = EntityDataManager::Instance().createHarvestable(
            position, oreHandle, YIELD, YIELD, 30.0f, WORLD_ID);
        BOOST_REQUIRE(handle.isValid());
        return handle;
    }

    static bool isDepleted(EntityHandle node) {
        const auto& edm = EntityDataManager::Instance();
        const size_t index = edm.getIndex(node);
        BOOST_REQUIRE(index != SIZE_MAX);
        return edm.getHarvestableData(edm.getStaticHotDataByIndex(index).typeLocalIndex).isDepleted;
    }

    int playerQuantity() const {
        return EntityDataManager::Instance().getInventoryQuantity(
            player->getInventoryIndex(), oreHandle);
    }

    static void drainEvents() { EventManager::Instance().update(); }

    // Drives the real controller until it stops harvesting (bounded).
    static void runHarvest(HarvestController& controller) {
        for (int step = 0; step < 200 && controller.isHarvesting(); ++step) {
            controller.update(DT);
        }
    }

    std::shared_ptr<Player> player;
    VoidLight::ResourceHandle oreHandle;
    std::vector<HarvestRecord> harvestEvents;
    std::vector<ScarcityRecord> scarcityEvents;
    std::vector<ResourceRecord> resourceEvents;
};

BOOST_FIXTURE_TEST_SUITE(HarvestCommitTests, HarvestCommitFixture)

BOOST_AUTO_TEST_CASE(CommitMarksDepletedBumpsVersionAndEmitsHarvestEvent) {
    using namespace VoidLight::HarvestCommit;
    const Vector2D nodePos(1000.0f, 1000.0f);
    EntityHandle node = createNode(nodePos);
    createNode(Vector2D(1100.0f, 1000.0f));
    createNode(Vector2D(1000.0f, 1100.0f));

    const uint64_t versionBefore = WorldResourceManager::Instance().getHarvestableVersion();
    const auto yield = commit(node, player->getHandle(), 0);
    BOOST_REQUIRE(yield.has_value());
    BOOST_CHECK(yield->resource == oreHandle);
    BOOST_CHECK_EQUAL(yield->quantity, YIELD);
    BOOST_CHECK_EQUAL(yield->position.getX(), nodePos.getX());
    BOOST_CHECK_EQUAL(yield->position.getY(), nodePos.getY());
    BOOST_CHECK(isDepleted(node));
    BOOST_CHECK_GT(WorldResourceManager::Instance().getHarvestableVersion(), versionBefore);

    // Commit does not touch inventories; the caller owns the yield.
    BOOST_CHECK_EQUAL(playerQuantity(), 0);

    drainEvents();
    BOOST_REQUIRE_EQUAL(harvestEvents.size(), 1u);
    BOOST_CHECK_EQUAL(harvestEvents[0].entityId, static_cast<int>(node.getId()));
    BOOST_CHECK_EQUAL(harvestEvents[0].tileX, static_cast<int>(nodePos.getX() / VoidLight::TILE_SIZE));
    BOOST_CHECK_EQUAL(harvestEvents[0].tileY, static_cast<int>(nodePos.getY() / VoidLight::TILE_SIZE));
}

BOOST_AUTO_TEST_CASE(CommitRejectsStaleHandleAndDepleted) {
    using namespace VoidLight::HarvestCommit;
    EntityHandle node = createNode(Vector2D(1000.0f, 1000.0f));

    // Invalid handle and wrong generation are rejected without side effects.
    BOOST_CHECK(!commit(EntityHandle{}, player->getHandle(), 0).has_value());
    const EntityHandle wrongGeneration{node.getId(), node.kind, node.generation + 1};
    BOOST_CHECK(!commit(wrongGeneration, player->getHandle(), 0).has_value());
    BOOST_CHECK(!isDepleted(node));

    // Destroyed harvestable: handle is stale.
    EntityHandle doomed = createNode(Vector2D(1200.0f, 1000.0f));
    EntityDataManager::Instance().destroyEntity(doomed);
    EntityDataManager::Instance().processDestructionQueue();
    BOOST_CHECK(!commit(doomed, player->getHandle(), 0).has_value());

    // Depleted: first commit succeeds, second is rejected.
    BOOST_REQUIRE(commit(node, player->getHandle(), 0).has_value());
    const uint64_t versionAfterFirst = WorldResourceManager::Instance().getHarvestableVersion();
    BOOST_CHECK(!commit(node, player->getHandle(), 0).has_value());
    BOOST_CHECK_EQUAL(WorldResourceManager::Instance().getHarvestableVersion(), versionAfterFirst);

    drainEvents();
    BOOST_CHECK_EQUAL(harvestEvents.size(), 1u);
}

BOOST_AUTO_TEST_CASE(CommitEnforcesReserve) {
    using namespace VoidLight::HarvestCommit;
    EntityHandle first = createNode(Vector2D(1000.0f, 1000.0f));
    EntityHandle last = createNode(Vector2D(1200.0f, 1000.0f));

    // Two available: taking one leaves 1, which satisfies the NPC reserve.
    BOOST_REQUIRE(commit(first, player->getHandle(), NPC_HARVEST_RESERVE).has_value());
    // One available: an NPC may not take the last node in the area.
    BOOST_CHECK(!commit(last, player->getHandle(), NPC_HARVEST_RESERVE).has_value());
    BOOST_CHECK(!isDepleted(last));
    // The player (reserve 0) may.
    BOOST_CHECK(commit(last, player->getHandle(), 0).has_value());
    BOOST_CHECK(isDepleted(last));
}

BOOST_AUTO_TEST_CASE(CommitEmitsScarcityOnLastLocalNode) {
    using namespace VoidLight::HarvestCommit;
    const Vector2D firstPos(1000.0f, 1000.0f);
    const Vector2D lastPos(1200.0f, 1000.0f);
    EntityHandle first = createNode(firstPos);
    EntityHandle last = createNode(lastPos);
    // Outside SCARCITY_RADIUS: does not count toward the area.
    createNode(Vector2D(1000.0f + SCARCITY_RADIUS * 3.0f, 1000.0f));

    BOOST_REQUIRE(commit(first, player->getHandle(), 0).has_value());
    drainEvents();
    BOOST_REQUIRE_EQUAL(scarcityEvents.size(), 1u);
    BOOST_CHECK_EQUAL(scarcityEvents[0].availableCount, 1u);
    BOOST_CHECK_EQUAL(scarcityEvents[0].center.getX(), firstPos.getX());
    BOOST_CHECK_EQUAL(scarcityEvents[0].center.getY(), firstPos.getY());
    BOOST_CHECK_EQUAL(scarcityEvents[0].radius, SCARCITY_RADIUS);
    BOOST_CHECK(scarcityEvents[0].resource == oreHandle);
    BOOST_CHECK(scarcityEvents[0].harvester == player->getHandle());

    BOOST_REQUIRE(commit(last, player->getHandle(), 0).has_value());
    drainEvents();
    BOOST_REQUIRE_EQUAL(scarcityEvents.size(), 2u);
    BOOST_CHECK_EQUAL(scarcityEvents[1].availableCount, 0u);
    BOOST_CHECK_EQUAL(scarcityEvents[1].center.getX(), lastPos.getX());
}

BOOST_AUTO_TEST_CASE(NoScarcityWhenEnoughNodesRemain) {
    using namespace VoidLight::HarvestCommit;
    EntityHandle node = createNode(Vector2D(1000.0f, 1000.0f));
    for (size_t i = 0; i < SCARCITY_THRESHOLD; ++i) {
        createNode(Vector2D(1100.0f + 64.0f * static_cast<float>(i), 1000.0f));
    }

    BOOST_REQUIRE(commit(node, player->getHandle(), 0).has_value());
    drainEvents();
    BOOST_CHECK_EQUAL(harvestEvents.size(), 1u);
    BOOST_CHECK(scarcityEvents.empty());
}

BOOST_AUTO_TEST_CASE(PlayerHarvestDepletesViaSharedCommit) {
    using namespace VoidLight::HarvestCommit;
    EntityHandle node = createNode(Vector2D(1020.0f, 1000.0f));
    // Two more in the area (outside reach) so no scarcity is emitted.
    createNode(Vector2D(1300.0f, 1000.0f));
    createNode(Vector2D(1000.0f, 1300.0f));

    HarvestController controller(player);
    const uint64_t versionBefore = WorldResourceManager::Instance().getHarvestableVersion();
    BOOST_REQUIRE(controller.startHarvest());
    runHarvest(controller);

    BOOST_CHECK(!controller.isHarvesting());
    BOOST_CHECK(isDepleted(node));
    BOOST_CHECK_EQUAL(playerQuantity(), YIELD);
    BOOST_CHECK_GT(WorldResourceManager::Instance().getHarvestableVersion(), versionBefore);

    drainEvents();
    BOOST_REQUIRE_EQUAL(harvestEvents.size(), 1u);
    BOOST_CHECK_EQUAL(harvestEvents[0].entityId, static_cast<int>(node.getId()));
    BOOST_REQUIRE_EQUAL(resourceEvents.size(), 1u);
    BOOST_CHECK(resourceEvents[0].owner == player->getHandle());
    BOOST_CHECK(resourceEvents[0].resource == oreHandle);
    BOOST_CHECK_EQUAL(resourceEvents[0].delta, YIELD);
    BOOST_CHECK(scarcityEvents.empty());

    // Node is depleted: nothing left in reach.
    BOOST_CHECK(!controller.startHarvest());
}

BOOST_AUTO_TEST_CASE(PlayerHarvestOfLastNodeEmitsScarcity) {
    EntityHandle node = createNode(Vector2D(1020.0f, 1000.0f));

    HarvestController controller(player);
    BOOST_REQUIRE(controller.startHarvest());
    runHarvest(controller);

    BOOST_CHECK(isDepleted(node));
    BOOST_CHECK_EQUAL(playerQuantity(), YIELD);

    drainEvents();
    BOOST_REQUIRE_EQUAL(scarcityEvents.size(), 1u);
    BOOST_CHECK_EQUAL(scarcityEvents[0].availableCount, 0u);
    BOOST_CHECK(scarcityEvents[0].harvester == player->getHandle());
    BOOST_CHECK(scarcityEvents[0].resource == oreHandle);
}

BOOST_AUTO_TEST_CASE(PlayerAndNpcHarvestSameNodeOnlyOnce) {
    using namespace VoidLight::HarvestCommit;
    // Stand-in harvester for the AI path: AIManager's harvest commit calls
    // commit() with NPC_HARVEST_RESERVE; the harvester handle is payload only.
    auto npcHarvester = std::make_shared<Player>();
    BOOST_REQUIRE(npcHarvester->getHandle().isValid());

    EntityHandle contested = createNode(Vector2D(1020.0f, 1000.0f));
    createNode(Vector2D(1300.0f, 1000.0f));
    createNode(Vector2D(1000.0f, 1300.0f));

    // NPC commits first while the player is mid-harvest: player's completion
    // is rejected and awards nothing.
    HarvestController controller(player);
    BOOST_REQUIRE(controller.startHarvest());
    BOOST_REQUIRE(commit(contested, npcHarvester->getHandle(), NPC_HARVEST_RESERVE).has_value());
    runHarvest(controller);
    BOOST_CHECK(!controller.isHarvesting());
    BOOST_CHECK_EQUAL(playerQuantity(), 0);

    drainEvents();
    BOOST_CHECK_EQUAL(harvestEvents.size(), 1u);
    BOOST_CHECK(resourceEvents.empty());

    // Reverse order: player completes first, NPC commit is rejected.
    EntityHandle second = createNode(Vector2D(980.0f, 1000.0f));
    BOOST_REQUIRE(controller.startHarvest());
    runHarvest(controller);
    BOOST_CHECK(isDepleted(second));
    BOOST_CHECK_EQUAL(playerQuantity(), YIELD);
    BOOST_CHECK(!commit(second, npcHarvester->getHandle(), NPC_HARVEST_RESERVE).has_value());

    drainEvents();
    BOOST_CHECK_EQUAL(harvestEvents.size(), 2u);
}

BOOST_AUTO_TEST_SUITE_END()
