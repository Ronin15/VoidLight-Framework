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
#include "world/HarvestType.hpp"
#include "world/HarvestConfig.hpp"
#include "managers/EventManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "../events/EventManagerTestAccess.hpp"
#include <string>
#include <memory>

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
    // Verify constants are reasonable
    BOOST_CHECK_GT(HarvestController::HARVEST_RANGE, 0.0f);
    BOOST_CHECK_GT(HarvestController::MOVEMENT_CANCEL_THRESHOLD, 0.0f);
    BOOST_CHECK_LT(HarvestController::MOVEMENT_CANCEL_THRESHOLD, HarvestController::HARVEST_RANGE);
}

BOOST_AUTO_TEST_SUITE_END()
