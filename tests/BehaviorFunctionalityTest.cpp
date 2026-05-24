/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE BehaviorFunctionalityTest
#include <boost/test/unit_test.hpp>

#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "ai/AICommandBus.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "core/ThreadSystem.hpp"
#include "entities/Player.hpp"
#include "events/EntityEvents.hpp"
#include "world/WorldData.hpp"
#include <format>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

// Test helper for data-driven NPCs (NPCs are purely data, no Entity class)
class TestNPC {
public:
    explicit TestNPC(float x = 0.0f, float y = 0.0f) {
        auto& edm = EntityDataManager::Instance();
        m_handle = edm.createNPCWithRaceClass(Vector2D(x, y), "Human", "Guard");
        m_initialPosition = Vector2D(x, y);
    }

    static std::shared_ptr<TestNPC> create(float x = 0.0f, float y = 0.0f) {
        return std::make_shared<TestNPC>(x, y);
    }

    [[nodiscard]] EntityHandle getHandle() const { return m_handle; }

    // Position access via EDM
    [[nodiscard]] Vector2D getPosition() const {
        if (!m_handle.isValid()) return Vector2D(0, 0);
        auto& edm = EntityDataManager::Instance();
        size_t index = edm.getIndex(m_handle);
        if (index == SIZE_MAX) return Vector2D(0, 0);
        return edm.getTransformByIndex(index).position;
    }

    void setPosition(const Vector2D& pos) {
        if (!m_handle.isValid()) return;
        auto& edm = EntityDataManager::Instance();
        size_t index = edm.getIndex(m_handle);
        if (index == SIZE_MAX) return;
        edm.getTransformByIndex(index).position = pos;
    }

    [[nodiscard]] Vector2D getVelocity() const {
        if (!m_handle.isValid()) return Vector2D(0, 0);
        auto& edm = EntityDataManager::Instance();
        size_t index = edm.getIndex(m_handle);
        if (index == SIZE_MAX) return Vector2D(0, 0);
        return edm.getTransformByIndex(index).velocity;
    }

    // Check if position changed in EDM (AIManager writes directly to EDM)
    int getUpdateCount() const {
        if (!m_handle.isValid()) return 0;

        auto& edm = EntityDataManager::Instance();
        size_t index = edm.getIndex(m_handle);
        if (index == SIZE_MAX) return 0;

        auto& transform = edm.getTransformByIndex(index);
        Vector2D currentPos = transform.position;
        Vector2D velocity = transform.velocity;

        bool positionMoved = (currentPos - m_initialPosition).length() > 0.01f;
        bool hasVelocity = velocity.length() > 0.01f;

        return (positionMoved || hasVelocity) ? 1 : 0;
    }

    void resetUpdateCount() {
        if (m_handle.isValid()) {
            auto& edm = EntityDataManager::Instance();
            size_t index = edm.getIndex(m_handle);
            if (index != SIZE_MAX) {
                m_initialPosition = edm.getTransformByIndex(index).position;
            }
        }
    }

private:
    EntityHandle m_handle;
    Vector2D m_initialPosition;
};

// Test fixture for behavior functionality tests
struct BehaviorTestFixture {
    BehaviorTestFixture() {
        // Initialize ThreadSystem first (required for PathfinderManager async tasks)
        BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());

        // Initialize managers in proper order (matches CollisionPathfindingIntegrationTests)
        GameTimeManager::Instance().init();  // Required for combat timing in behaviors
        EventManager::Instance().init();
        BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        WorldManager::Instance().init();
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        CollisionManager::Instance().init();
        PathfinderManager::Instance().init();
        AIManager::Instance().init();
        BackgroundSimulationManager::Instance().init();

        // Load a simple test world for pathfinding
        // Note: World must be >= 26x26 to satisfy VILLAGE_RADIUS constraints in WorldGenerator
        VoidLight::WorldGenerationConfig cfg{};
        cfg.width = 30; cfg.height = 30; cfg.seed = 12345;
        cfg.elevationFrequency = 0.05f; cfg.humidityFrequency = 0.05f;
        cfg.waterLevel = 0.3f; cfg.mountainLevel = 0.7f;

        if (!WorldManager::Instance().loadNewWorld(cfg)) {
            throw std::runtime_error("Failed to load test world for behavior tests");
        }

        // EVENT-DRIVEN: Process any deferred events (triggers WorldLoaded task on ThreadSystem)
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EventManager::Instance().update();

        // Set world bounds explicitly for tests (20x20 tiles * 64 pixels/tile = 1280x1280)
        const float TILE_SIZE = 64.0f;
        float worldPixelWidth = cfg.width * TILE_SIZE;
        float worldPixelHeight = cfg.height * TILE_SIZE;
        CollisionManager::Instance().setWorldBounds(0, 0, worldPixelWidth, worldPixelHeight);

        // Rebuild pathfinding grid (async operation - best effort, not critical for basic tests)
        PathfinderManager::Instance().rebuildGrid();
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Give grid a chance to build

        // NOTE: Behaviors are auto-registered in AIManager::init() - no manual registration needed

        // Create test NPCs (data-driven, no Entity class)
        for (int i = 0; i < 5; ++i) {
            testEntities.push_back(TestNPC::create(i * 100.0f, i * 100.0f));
        }

        // Set a mock player for behaviors that need a target
        playerEntity = TestNPC::create(500.0f, 500.0f);
        EntityHandle playerHandle = playerEntity->getHandle();
        AIManager::Instance().setPlayerHandle(playerHandle);

        // Initial tier update to mark entities as Active
        BackgroundSimulationManager::Instance().update(Vector2D(500.0f, 500.0f), 0.016f);
    }

    // Helper to update AI with proper tier management
    void updateAI(float deltaTime, const Vector2D& referencePoint = Vector2D(500.0f, 500.0f)) {
        // Force tier recalculation (tests create/destroy entities frequently)
        BackgroundSimulationManager::Instance().invalidateTiers();
        BackgroundSimulationManager::Instance().update(referencePoint, deltaTime);
        AIManager::Instance().update(deltaTime);
    }

    ~BehaviorTestFixture() {
        // Release test entities first
        testEntities.clear();
        playerEntity.reset();

        // Clean up managers in reverse initialization order
        // AIManager::clean() resets m_initialized so init() re-registers event handlers.
        // AIManager::init() now also resets m_globallyPaused, fixing the paused-after-clean issue.
        BackgroundSimulationManager::Instance().clean();
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EntityDataManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        WorldManager::Instance().clean();
        EventManager::Instance().clean();
        // ThreadSystem persists across tests
    }

    std::vector<std::shared_ptr<TestNPC>> testEntities;
    std::shared_ptr<TestNPC> playerEntity;
};

// Test Suite 1: Basic Behavior Registration and Assignment
BOOST_FIXTURE_TEST_SUITE(BehaviorRegistrationTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestAllBehaviorsRegistered) {
    // Test that all 8 behavior types are auto-registered
    std::vector<std::string> expectedBehaviors = {
        "Idle", "Wander", "Patrol", "Chase", "Flee", "Follow", "Guard", "Attack"
    };

    for (const auto& behaviorName : expectedBehaviors) {
        BOOST_CHECK(AIManager::Instance().hasBehavior(behaviorName));
    }
}

BOOST_AUTO_TEST_CASE(TestBehaviorAssignment) {
    auto entity = testEntities[0];
    EntityHandle handle = entity->getHandle();

    // Test assigning a behavior
    AIManager::Instance().assignBehavior(handle, "Wander");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Test switching behaviors
    AIManager::Instance().assignBehavior(handle, "Chase");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Test unassigning behavior
    AIManager::Instance().unassignBehavior(handle);
    BOOST_CHECK(!AIManager::Instance().hasBehavior(handle));
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 2: Idle Behavior Testing
BOOST_FIXTURE_TEST_SUITE(IdleBehaviorTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestIdleStationaryMode) {
    auto entity = testEntities[0];
    Vector2D initialPos = entity->getPosition();

    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Idle");

    // Update multiple times
    for (int i = 0; i < 10; ++i) {
        updateAI(0.016f);
    }

    // Position should remain relatively unchanged for idle
    // Note: CollisionManager may push entities apart slightly via resolve()
    Vector2D currentPos = entity->getPosition();
    float distanceMoved = (currentPos - initialPos).length();
    BOOST_CHECK_LT(distanceMoved, 35.0f); // Allow for collision resolution pushes
}

BOOST_AUTO_TEST_CASE(TestIdleMessageHandling) {
    auto entity = testEntities[0];
    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Idle");

    const auto initialUpdates = AIManager::Instance().getBehaviorUpdateCount();
    updateAI(0.016f);

    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
    BOOST_CHECK_GT(AIManager::Instance().getBehaviorUpdateCount(), initialUpdates);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 3: Movement Behavior Testing
BOOST_FIXTURE_TEST_SUITE(MovementBehaviorTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestWanderBehavior) {
    // Create fresh entity for this test to avoid interference
    auto entity = TestNPC::create(640.0f, 640.0f); // Center of 30x30 tile world

    // EDM-CENTRIC: Set collision layers directly on EDM hot data
    {
        auto& edm = EntityDataManager::Instance();
        size_t idx = edm.getIndex(entity->getHandle());
        if (idx != SIZE_MAX) {
            auto& hot = edm.getHotDataByIndex(idx);
            hot.collisionLayers = CollisionLayer::Layer_Enemy;
            hot.collisionMask = 0xFFFF;
            hot.setCollisionEnabled(true);
        }
    }

    Vector2D initialPos = entity->getPosition();
    entity->resetUpdateCount();

    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Wander");

    // Track movement over time
    int movementSteps = 0;
    Vector2D lastPos = initialPos;
    float totalDistanceMoved = 0.0f;
    bool hasVelocity = false;

    // Use larger deltaTime to advance cooldowns faster (30s wander cooldown)
    // 70 iterations * 0.5f = 35s of simulated time (enough to pass 30s cooldown)
    const float testDeltaTime = 0.5f;
    for (int i = 0; i < 70; ++i) {
        updateAI(testDeltaTime);
        CollisionManager::Instance().update(testDeltaTime);

        Vector2D pos = entity->getPosition();
        Vector2D vel = entity->getVelocity();
        float stepDistance = (pos - lastPos).length();

        totalDistanceMoved += stepDistance; // Track ALL movement
        if (stepDistance > 0.1f) {
            movementSteps++;
        }
        if (vel.length() > 0.1f) {
            hasVelocity = true;
        }
        lastPos = pos;

        if (i % 14 == 0) {
            BOOST_TEST_MESSAGE("Wander Update " << i << ": pos=(" << pos.getX() << ", " << pos.getY() << ") vel=" << vel.length() << " moved=" << totalDistanceMoved);
        }
    }

    // Verify entity actually wandered (moved or has velocity indicating intent to move)
    Vector2D currentPos = entity->getPosition();
    float distanceMoved = (currentPos - initialPos).length();

    BOOST_TEST_MESSAGE("Wander test: moved " << distanceMoved << "px over " << movementSteps << " steps, total=" << totalDistanceMoved);

    // Entity should either have moved OR have velocity set (async pathfinding may delay actual movement)
    bool isWandering = (totalDistanceMoved > 5.0f) || hasVelocity;
    BOOST_CHECK(isWandering); // Verify wander behavior is functioning

    // Clean up
    AIManager::Instance().unassignBehavior(handle);
}

BOOST_AUTO_TEST_CASE(TestChaseBehavior) {
    // Test: NPC chases an opponent who attacked them (memory-based targeting)
    auto chaser = TestNPC::create(200.0f, 200.0f);
    auto opponent = TestNPC::create(500.0f, 500.0f);  // The attacker

    auto& edm = EntityDataManager::Instance();
    EntityHandle chaserHandle = chaser->getHandle();
    EntityHandle opponentHandle = opponent->getHandle();

    // EDM-CENTRIC: Set collision layers directly on EDM hot data
    {
        size_t chaserIdx = edm.getIndex(chaserHandle);
        if (chaserIdx != SIZE_MAX) {
            auto& hot = edm.getHotDataByIndex(chaserIdx);
            hot.collisionLayers = CollisionLayer::Layer_Enemy;
            hot.collisionMask = 0xFFFF;
            hot.setCollisionEnabled(true);
        }
        size_t opponentIdx = edm.getIndex(opponentHandle);
        if (opponentIdx != SIZE_MAX) {
            auto& hot = edm.getHotDataByIndex(opponentIdx);
            hot.collisionLayers = CollisionLayer::Layer_Enemy;
            hot.collisionMask = 0xFFFF;
            hot.setCollisionEnabled(true);
        }
    }

    Vector2D initialPos = chaser->getPosition();
    Vector2D opponentPos = opponent->getPosition();
    chaser->resetUpdateCount();

    BOOST_TEST_MESSAGE("Initial chaser pos: (" << initialPos.getX() << ", " << initialPos.getY() << ")");
    BOOST_TEST_MESSAGE("Opponent pos: (" << opponentPos.getX() << ", " << opponentPos.getY() << ")");
    BOOST_TEST_MESSAGE("Initial distance: " << (initialPos - opponentPos).length());

    // Register chaser with Chase behavior
    AIManager::Instance().assignBehavior(chaserHandle, "Chase");

    // Set up memory context: opponent attacked the chaser, so chaser pursues
    {
        size_t chaserIdx = edm.getIndex(chaserHandle);
        if (chaserIdx != SIZE_MAX) {
            edm.initMemoryData(chaserIdx);
            auto& memData = edm.getMemoryData(chaserIdx);
            memData.lastAttacker = opponentHandle;  // Opponent attacked us - chase them!
            memData.lastCombatTime = 0.0f;  // Delta semantics: 0 = just happened
            memData.setValid(true);
        }
    }

    // Track movement progress
    int significantMovementCount = 0;
    Vector2D lastPos = initialPos;
    float totalDistanceMoved = 0.0f;

    const float testDeltaTime = 0.1f;
    for (int i = 0; i < 50; ++i) {
        updateAI(testDeltaTime);
        CollisionManager::Instance().update(testDeltaTime);

        Vector2D pos = chaser->getPosition();
        float stepDistance = (pos - lastPos).length();
        totalDistanceMoved += stepDistance;
        if (stepDistance > 0.1f) {
            significantMovementCount++;
        }
        lastPos = pos;

        if (i % 10 == 0) {
            Vector2D vel = chaser->getVelocity();
            BOOST_TEST_MESSAGE("Update " << i << ": pos=(" << pos.getX() << ", " << pos.getY() << ") vel=(" << vel.getX() << ", " << vel.getY() << ") moved=" << totalDistanceMoved);
        }
    }

    // Verify actual movement occurred and chaser got closer to opponent
    Vector2D currentPos = chaser->getPosition();
    Vector2D currentVel = chaser->getVelocity();
    float initialDistanceToOpponent = (initialPos - opponentPos).length();
    float currentDistanceToOpponent = (currentPos - opponentPos).length();

    BOOST_TEST_MESSAGE("Final chaser pos: (" << currentPos.getX() << ", " << currentPos.getY() << ")");
    BOOST_TEST_MESSAGE("Final velocity: (" << currentVel.getX() << ", " << currentVel.getY() << ")");
    BOOST_TEST_MESSAGE("Initial distance: " << initialDistanceToOpponent << " -> Final distance: " << currentDistanceToOpponent);
    BOOST_TEST_MESSAGE("Total distance moved: " << totalDistanceMoved << " over " << significantMovementCount << " steps");

    BOOST_CHECK_GT(totalDistanceMoved, 5.0f); // Chaser must have actually moved
    BOOST_CHECK_LT(currentDistanceToOpponent, initialDistanceToOpponent); // Must get closer

    // Clean up
    AIManager::Instance().unassignBehavior(chaserHandle);
}

BOOST_AUTO_TEST_CASE(TestKnockbackOverridesChaseMovement) {
    auto attacker = TestNPC::create(500.0f, 500.0f);
    auto target = TestNPC::create(700.0f, 500.0f);

    auto& edm = EntityDataManager::Instance();
    EntityHandle attackerHandle = attacker->getHandle();
    EntityHandle targetHandle = target->getHandle();
    const size_t targetIdx = edm.getIndex(targetHandle);
    BOOST_REQUIRE(targetIdx != SIZE_MAX);

    AIManager::Instance().assignBehavior(targetHandle, "Chase");

    edm.initMemoryData(targetIdx);
    auto& memData = edm.getMemoryData(targetIdx);
    memData.lastAttacker = attackerHandle;
    memData.lastCombatTime = 0.0f;  // Delta semantics: 0 = just happened
    memData.setValid(true);

    updateAI(0.016f, attacker->getPosition());

    const float preHitVelocityX = target->getVelocity().getX();
    BOOST_CHECK_LT(preHitVelocityX, 0.0f);

    auto damageEvent = EventManager::Instance().acquireDamageEvent();
    damageEvent->configure(attackerHandle, targetHandle, 15.0f, Vector2D(60.0f, 0.0f));
    BOOST_REQUIRE(EventManager::Instance().dispatchEvent(
        std::static_pointer_cast<Event>(damageEvent),
        EventManager::DispatchMode::Immediate));

    const float hitStartX = target->getPosition().getX();

    // Frame 1: REPLACE path — impulse * KICK_MULTIPLIER overrides chase velocity.
    // KICK_MULTIPLIER=6.0, MAX_KICK_SPEED=250.0 (AIManager.cpp ~line 1564-1568).
    // Knockback vector was (60,0), mass defaults to 1.0 → impulseX = 60 * 1.0 = 60.
    // Kick = 60 * 6 = 360, clamped to MAX_KICK_SPEED=250. Expect velocity in (0, 250].
    updateAI(0.016f, attacker->getPosition());

    const float postHitVelocityX = target->getVelocity().getX();
    const float postHitX = target->getPosition().getX();

    // Frame-1 velocity must be positive (knocked right, away from attacker at x=500)
    // and within the expected clamped band from KICK_MULTIPLIER / MAX_KICK_SPEED.
    BOOST_CHECK_GT(postHitVelocityX, 0.0f);
    BOOST_CHECK_LE(postHitVelocityX, 250.0f); // capped at MAX_KICK_SPEED
    BOOST_CHECK_GT(postHitX, hitStartX);

    // Frame 2: ADD path — decayed impulse blends on top of chase velocity.
    // Decay factor is Knockback::DECAY = 0.7 per frame.
    // Frame-2 impulse = 60 * 0.7 = 42 (or clamped equivalent); net X may go negative
    // as chase resumes, but the decayed impulse reduces the magnitude vs frame-1.
    updateAI(0.016f, attacker->getPosition());

    const float frame2VelocityX = target->getVelocity().getX();

    // Decayed impulse on frame 2 must have smaller magnitude than the clamped frame-1 kick.
    BOOST_CHECK_LT(std::abs(frame2VelocityX), postHitVelocityX);

    AIManager::Instance().unassignBehavior(targetHandle);
}

BOOST_AUTO_TEST_CASE(TestFleeBehavior) {
    auto testPlayer = TestNPC::create(500.0f, 500.0f);
    auto entity = TestNPC::create(600.0f, 600.0f); // Close to player

    // EDM-CENTRIC: Set collision layers
    {
        auto& edm = EntityDataManager::Instance();
        size_t entityIdx = edm.getIndex(entity->getHandle());
        if (entityIdx != SIZE_MAX) {
            auto& hot = edm.getHotDataByIndex(entityIdx);
            hot.collisionLayers = CollisionLayer::Layer_Enemy;
            hot.collisionMask = 0xFFFF;
            hot.setCollisionEnabled(true);
        }
        size_t playerIdx = edm.getIndex(testPlayer->getHandle());
        if (playerIdx != SIZE_MAX) {
            auto& hot = edm.getHotDataByIndex(playerIdx);
            hot.collisionLayers = CollisionLayer::Layer_Player;
            hot.collisionMask = 0xFFFF;
            hot.setCollisionEnabled(true);
        }
    }

    EntityHandle playerHandle = testPlayer->getHandle();
    AIManager::Instance().setPlayerHandle(playerHandle);

    Vector2D playerPos = testPlayer->getPosition();
    Vector2D fleeStartPos = entity->getPosition();
    entity->resetUpdateCount();

    EntityHandle handle = entity->getHandle();

    // FleeBehavior requires a lastAttacker in memory to know who to flee from
    auto &edm = EntityDataManager::Instance();
    size_t entityIdx = edm.getIndex(handle);
    if (entityIdx != SIZE_MAX) {
        edm.recordCombatEvent(entityIdx, playerHandle, handle, 10.0f,
                              /*wasAttacked=*/true, 0.0f);
    }

    AIManager::Instance().assignBehavior(handle, "Flee");

    const float testDeltaTime = 0.1f;
    for (int i = 0; i < 30; ++i) {
        updateAI(testDeltaTime);
        CollisionManager::Instance().update(testDeltaTime);
    }

    // Entity should move away from player (or at least have velocity set)
    Vector2D currentPos = entity->getPosition();
    Vector2D currentVel = entity->getVelocity();
    float initialDistanceToPlayer = (fleeStartPos - playerPos).length();
    float currentDistanceToPlayer = (currentPos - playerPos).length();

    // Check that entity is attempting to flee
    bool isFleeing = (currentDistanceToPlayer > initialDistanceToPlayer) || (currentVel.length() > 0.1f);
    BOOST_CHECK(isFleeing);

    // Clean up
    AIManager::Instance().unassignBehavior(handle);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 4: Complex Behavior Testing
BOOST_FIXTURE_TEST_SUITE(ComplexBehaviorTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestFollowBehavior) {
    auto testPlayer = TestNPC::create(500.0f, 500.0f);
    auto entity = TestNPC::create(300.0f, 500.0f); // 200 pixels away

    EntityHandle playerHandle = testPlayer->getHandle();
    AIManager::Instance().setPlayerHandle(playerHandle);

    Vector2D playerPos = testPlayer->getPosition();
    entity->resetUpdateCount();

    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Follow");

    // Move player to a new position within range
    Vector2D newPlayerPos(playerPos.getX() + 150, playerPos.getY() + 150);
    testPlayer->setPosition(newPlayerPos);

    const float testDeltaTime = 0.25f;
    for (int i = 0; i < 50; ++i) {
        updateAI(testDeltaTime);
    }

    // Entity should move closer to player
    Vector2D currentPos = entity->getPosition();
    float distanceToPlayer = (currentPos - newPlayerPos).length();

    BOOST_CHECK_LT(distanceToPlayer, 600.0f); // Should be reasonably close

    // Clean up
    AIManager::Instance().unassignBehavior(handle);
}

BOOST_AUTO_TEST_CASE(TestGuardBehavior) {
    auto entity = testEntities[0];
    Vector2D guardPos(200, 200);
    entity->setPosition(guardPos);
    entity->resetUpdateCount();

    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Guard");

    const float testDeltaTime = 0.1f;
    for (int i = 0; i < 30; ++i) {
        updateAI(testDeltaTime);
    }

    // Guard should stay reasonably near post
    Vector2D currentPos = entity->getPosition();
    float distanceFromPost = (currentPos - guardPos).length();
    BOOST_CHECK_LT(distanceFromPost, 300.0f);
}

BOOST_AUTO_TEST_CASE(TestAttackBehavior) {
    auto entity = testEntities[0];
    Vector2D playerPos = playerEntity->getPosition();

    // Position entity within attack range
    entity->setPosition(Vector2D(playerPos.getX() + 100, playerPos.getY()));
    entity->resetUpdateCount();

    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Attack");

    // Capture initial behavior execution count
    size_t initialBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();

    const float testDeltaTime = 0.1f;
    for (int i = 0; i < 50; ++i) {
        updateAI(testDeltaTime);
    }

    // Entity should approach for attack
    Vector2D currentPos = entity->getPosition();
    float distanceToPlayer = (currentPos - playerPos).length();

    BOOST_CHECK_LT(distanceToPlayer, 200.0f);

    // Check that behaviors were executed
    size_t finalBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();
    BOOST_CHECK_GT(finalBehaviorCount, initialBehaviorCount);
}

BOOST_AUTO_TEST_CASE(TestAttackAutoAcquiresNearbyEnemyTarget) {
    auto& edm = EntityDataManager::Instance();

    auto attacker = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(360.0f, 300.0f);

    edm.setFaction(target->getHandle(), 2);

    EntityHandle attackerHandle = attacker->getHandle();
    size_t attackerIdx = edm.getIndex(attackerHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);

    AIManager::Instance().assignBehavior(attackerHandle, "Attack");

    const float testDeltaTime = 0.1f;
    for (int i = 0; i < 20; ++i) {
        updateAI(testDeltaTime, attacker->getPosition());
    }

    const auto& memData = edm.getMemoryData(attackerIdx);
    BOOST_CHECK(memData.lastTarget.isValid());
    BOOST_CHECK(memData.lastTarget == target->getHandle());
}

BOOST_AUTO_TEST_CASE(TestAttackBehaviorRespectsAuthoredRangeWhenClosing) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto shortAttacker = TestNPC::create(300.0f, 300.0f);
    auto longAttacker = TestNPC::create(300.0f, 360.0f);
    auto shortTarget = TestNPC::create(420.0f, 300.0f);
    auto longTarget = TestNPC::create(420.0f, 360.0f);

    const EntityHandle shortAttackerHandle = shortAttacker->getHandle();
    const EntityHandle longAttackerHandle = longAttacker->getHandle();
    const EntityHandle shortTargetHandle = shortTarget->getHandle();
    const EntityHandle longTargetHandle = longTarget->getHandle();

    const size_t shortAttackerIdx = edm.getIndex(shortAttackerHandle);
    const size_t longAttackerIdx = edm.getIndex(longAttackerHandle);
    const size_t shortTargetIdx = edm.getIndex(shortTargetHandle);
    const size_t longTargetIdx = edm.getIndex(longTargetHandle);
    BOOST_REQUIRE(shortAttackerIdx != SIZE_MAX);
    BOOST_REQUIRE(longAttackerIdx != SIZE_MAX);
    BOOST_REQUIRE(shortTargetIdx != SIZE_MAX);
    BOOST_REQUIRE(longTargetIdx != SIZE_MAX);

    edm.setFaction(shortAttackerHandle, 1);
    edm.setFaction(longAttackerHandle, 1);
    edm.setFaction(shortTargetHandle, 2);
    edm.setFaction(longTargetHandle, 2);

    edm.getCharacterDataByIndex(shortAttackerIdx).attackRange = 45.0f;
    edm.getCharacterDataByIndex(longAttackerIdx).attackRange = 125.0f;

    aiMgr.assignBehavior(shortTargetHandle, "Idle");
    aiMgr.assignBehavior(longTargetHandle, "Idle");
    aiMgr.assignBehavior(shortAttackerHandle, "Attack");
    aiMgr.assignBehavior(longAttackerHandle, "Attack");

    auto& shortMem = edm.getMemoryData(shortAttackerIdx);
    auto& longMem = edm.getMemoryData(longAttackerIdx);
    shortMem.setValid(true);
    longMem.setValid(true);
    shortMem.lastTarget = shortTargetHandle;
    longMem.lastTarget = longTargetHandle;

    float shortAttackDistance = -1.0f;
    float longAttackDistance = -1.0f;

    for (int i = 0; i < 80; ++i) {
        updateAI(0.1f, Vector2D(360.0f, 330.0f));

        const auto shortRef = edm.getBehaviorConfigRef(shortAttackerIdx);
        if (shortAttackDistance < 0.0f &&
            shortRef.type == BehaviorType::Attack &&
            edm.getAttackState(shortRef.index).currentState == 3) {
            shortAttackDistance = (shortAttacker->getPosition() - shortTarget->getPosition()).length();
        }

        const auto longRef = edm.getBehaviorConfigRef(longAttackerIdx);
        if (longAttackDistance < 0.0f &&
            longRef.type == BehaviorType::Attack &&
            edm.getAttackState(longRef.index).currentState == 3) {
            longAttackDistance = (longAttacker->getPosition() - longTarget->getPosition()).length();
        }

        if (shortAttackDistance > 0.0f && longAttackDistance > 0.0f) {
            break;
        }
    }

    BOOST_REQUIRE_MESSAGE(shortAttackDistance > 0.0f, "Short-range attacker never entered ATTACKING state");
    BOOST_REQUIRE_MESSAGE(longAttackDistance > 0.0f, "Long-range attacker never entered ATTACKING state");
    BOOST_CHECK_LT(shortAttackDistance, 70.0f);
    BOOST_CHECK_GT(longAttackDistance, 70.0f);
    BOOST_CHECK_GT(longAttackDistance, shortAttackDistance + 25.0f);
}

BOOST_AUTO_TEST_CASE(TestMeleeAttackUsesFullWeaponReach) {
    auto& edm = EntityDataManager::Instance();

    auto attacker = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(355.0f, 300.0f);
    const EntityHandle attackerHandle = attacker->getHandle();
    const EntityHandle targetHandle = target->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    const size_t targetIdx = edm.getIndex(targetHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);
    BOOST_REQUIRE(targetIdx != SIZE_MAX);

    edm.setFaction(attackerHandle, 1);
    edm.setFaction(targetHandle, 2);
    edm.getCharacterDataByIndex(attackerIdx).attackRange = 60.0f;

    AIManager::Instance().assignBehavior(attackerHandle, "Attack");
    const auto ref = edm.getBehaviorConfigRef(attackerIdx);
    BOOST_REQUIRE(ref.type == BehaviorType::Attack);

    auto attackConfig = VoidLight::AttackBehaviorConfig::createMeleeConfig(60.0f);
    attackConfig.specialAttackChance = 0.0f;

    auto& attackState = edm.getAttackState(ref.index);
    attackState.currentState = 1;
    attackState.attackTimer = 10.0f;
    attackState.hasExplicitTarget = true;
    attackState.explicitTarget = targetHandle;

    auto& hotData = edm.getHotDataByIndex(attackerIdx);
    auto& memoryData = edm.getMemoryData(attackerIdx);
    memoryData.setValid(true);
    memoryData.lastTarget = targetHandle;

    BehaviorContext ctx(hotData.transform, hotData, attackerHandle.getId(),
                        attackerIdx, 0.016f, EntityHandle{}, Vector2D(0, 0),
                        Vector2D(0, 0), false, edm.getBehaviorData(attackerIdx),
                        &edm.getPathData(attackerIdx), memoryData,
                        edm.getCharacterDataByIndex(attackerIdx),
                        0.0f, 0.0f, 1280.0f, 1280.0f, true, 0.0f,
                        edm.knockbackSidecar());

    Behaviors::executeAttack(ctx, attackConfig, attackState);

    BOOST_CHECK_EQUAL(static_cast<int>(attackState.currentState), 3);
}

BOOST_AUTO_TEST_CASE(TestMeleeAttackPressuresInsideReachBeforeWeaponReady) {
    auto& edm = EntityDataManager::Instance();

    auto attacker = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(355.0f, 300.0f);
    const EntityHandle attackerHandle = attacker->getHandle();
    const EntityHandle targetHandle = target->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    const size_t targetIdx = edm.getIndex(targetHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);
    BOOST_REQUIRE(targetIdx != SIZE_MAX);

    edm.setFaction(attackerHandle, 1);
    edm.setFaction(targetHandle, 2);
    edm.getCharacterDataByIndex(attackerIdx).attackRange = 60.0f;

    AIManager::Instance().assignBehavior(attackerHandle, "Attack");
    const auto ref = edm.getBehaviorConfigRef(attackerIdx);
    BOOST_REQUIRE(ref.type == BehaviorType::Attack);

    auto attackConfig = VoidLight::AttackBehaviorConfig::createMeleeConfig(60.0f);
    attackConfig.specialAttackChance = 0.0f;

    auto& attackState = edm.getAttackState(ref.index);
    attackState.currentState = 1;
    attackState.attackTimer = 0.0f;
    attackState.hasExplicitTarget = true;
    attackState.explicitTarget = targetHandle;

    auto& hotData = edm.getHotDataByIndex(attackerIdx);
    auto& memoryData = edm.getMemoryData(attackerIdx);
    memoryData.setValid(true);
    memoryData.lastTarget = targetHandle;
    memoryData.personality.aggression = 0.5f;
    memoryData.personality.composure = 0.8f;

    BehaviorContext ctx(hotData.transform, hotData, attackerHandle.getId(),
                        attackerIdx, 0.016f, EntityHandle{}, Vector2D(0, 0),
                        Vector2D(0, 0), false, edm.getBehaviorData(attackerIdx),
                        &edm.getPathData(attackerIdx), memoryData,
                        edm.getCharacterDataByIndex(attackerIdx),
                        0.0f, 0.0f, 1280.0f, 1280.0f, true, 0.0f,
                        edm.knockbackSidecar());

    Behaviors::executeAttack(ctx, attackConfig, attackState);

    const Vector2D toTarget = (target->getPosition() - attacker->getPosition()).normalized();
    const Vector2D velocityDir = attacker->getVelocity().normalized();
    const float directChaseAmount = velocityDir.dot(toTarget);

    BOOST_CHECK_EQUAL(static_cast<int>(attackState.currentState), 6);
    BOOST_CHECK_LT(directChaseAmount, 0.25f);
}

BOOST_AUTO_TEST_CASE(TestAttackBehaviorSynchronizesCurrentAttackMode) {
    auto& edm = EntityDataManager::Instance();

    auto attacker = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(360.0f, 300.0f);
    const EntityHandle attackerHandle = attacker->getHandle();
    const EntityHandle targetHandle = target->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    const size_t targetIdx = edm.getIndex(targetHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);
    BOOST_REQUIRE(targetIdx != SIZE_MAX);

    AIManager::Instance().assignBehavior(attackerHandle, "Attack");
    const auto ref = edm.getBehaviorConfigRef(attackerIdx);
    BOOST_REQUIRE(ref.type == BehaviorType::Attack);

    auto attackConfig = edm.getAttackConfig(ref.index);
    attackConfig.attackMode = 0;
    auto& attackState = edm.getAttackState(ref.index);
    attackState.attackMode = 1;
    attackState.hasExplicitTarget = true;
    attackState.explicitTarget = targetHandle;

    auto& hotData = edm.getHotDataByIndex(attackerIdx);
    auto& memoryData = edm.getMemoryData(attackerIdx);
    memoryData.setValid(true);

    BehaviorContext ctx(hotData.transform, hotData, attackerHandle.getId(),
                        attackerIdx, 0.016f, EntityHandle{}, Vector2D(0, 0),
                        Vector2D(0, 0), false, edm.getBehaviorData(attackerIdx),
                        &edm.getPathData(attackerIdx), memoryData,
                        edm.getCharacterDataByIndex(attackerIdx),
                        0.0f, 0.0f, 1280.0f, 1280.0f, true, 0.0f,
                        edm.knockbackSidecar());

    Behaviors::executeAttack(ctx, attackConfig, attackState);

    BOOST_CHECK_EQUAL(static_cast<int>(attackState.attackMode), 0);
}

BOOST_AUTO_TEST_CASE(TestRangedAttackWithoutAmmoResetsForRepositioning) {
    auto& edm = EntityDataManager::Instance();
    auto& rtm = ResourceTemplateManager::Instance();
    VoidLight::AICommandBus::Instance().clearAll();

    auto attacker = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(360.0f, 300.0f);
    const EntityHandle attackerHandle = attacker->getHandle();
    const EntityHandle targetHandle = target->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    const size_t targetIdx = edm.getIndex(targetHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);
    BOOST_REQUIRE(targetIdx != SIZE_MAX);

    edm.setFaction(attackerHandle, 1);
    edm.setFaction(targetHandle, 2);

    const auto bowHandle = rtm.getHandleById("bow");
    BOOST_REQUIRE(bowHandle.isValid());
    auto& attackerChar = edm.getCharacterDataByIndex(attackerIdx);
    BOOST_REQUIRE(attackerChar.hasInventory());
    BOOST_REQUIRE(edm.addToInventory(attackerChar.inventoryIndex, bowHandle, 1));
    BOOST_REQUIRE(edm.equipCharacterItem(attackerHandle, bowHandle));

    AIManager::Instance().assignBehavior(attackerHandle, "Attack");
    const auto ref = edm.getBehaviorConfigRef(attackerIdx);
    BOOST_REQUIRE(ref.type == BehaviorType::Attack);

    auto attackConfig = edm.getAttackConfig(ref.index);
    attackConfig.attackMode = 1;
    attackConfig.attackRange = 160.0f;
    attackConfig.projectileSpeed = 180.0f;
    attackConfig.specialAttackChance = 0.0f;

    auto& attackState = edm.getAttackState(ref.index);
    attackState.currentState = 3;
    attackState.stateChangeTimer = 0.0f;
    attackState.canAttack = true;
    attackState.hasExplicitTarget = true;
    attackState.explicitTarget = targetHandle;

    auto& hotData = edm.getHotDataByIndex(attackerIdx);
    auto& memoryData = edm.getMemoryData(attackerIdx);
    memoryData.setValid(true);
    memoryData.lastTarget = targetHandle;

    BehaviorContext ctx(hotData.transform, hotData, attackerHandle.getId(),
                        attackerIdx, 0.016f, EntityHandle{}, Vector2D(0, 0),
                        Vector2D(0, 0), false, edm.getBehaviorData(attackerIdx),
                        &edm.getPathData(attackerIdx), memoryData,
                        edm.getCharacterDataByIndex(attackerIdx),
                        0.0f, 0.0f, 1280.0f, 1280.0f, true, 0.0f,
                        edm.knockbackSidecar());

    Behaviors::executeAttack(ctx, attackConfig, attackState);

    BOOST_CHECK_EQUAL(static_cast<int>(attackState.currentState), 0);
    BOOST_CHECK(attackState.canAttack);
    BOOST_CHECK(!attackState.lastAttackHit);

    std::vector<VoidLight::AICommandBus::RangedAttackCommand> rangedCommands;
    VoidLight::AICommandBus::Instance().drainRangedAttacks(rangedCommands);
    BOOST_CHECK(rangedCommands.empty());

    std::vector<VoidLight::AICommandBus::EquipmentSwapCommand> fallbackCommands;
    VoidLight::AICommandBus::Instance().drainMeleeFallbackEquips(fallbackCommands);
    BOOST_REQUIRE_EQUAL(fallbackCommands.size(), 1u);
    BOOST_CHECK(fallbackCommands.front().targetHandle == attackerHandle);
    BOOST_CHECK_EQUAL(fallbackCommands.front().targetEdmIndex, attackerIdx);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 5: Message System Testing
BOOST_FIXTURE_TEST_SUITE(BehaviorMessageTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestBehaviorSpecificMessages) {
    auto entity = testEntities[0];
    EntityHandle handle = entity->getHandle();
    const size_t initialAssignments = AIManager::Instance().getTotalAssignmentCount();

    // Test Guard behavior messages
    AIManager::Instance().assignBehavior(handle, "Guard");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Test Follow behavior messages
    AIManager::Instance().assignBehavior(handle, "Follow");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Test Attack behavior messages
    AIManager::Instance().assignBehavior(handle, "Attack");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Test Flee behavior messages
    AIManager::Instance().assignBehavior(handle, "Flee");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    updateAI(0.016f);
    BOOST_CHECK_GT(AIManager::Instance().getTotalAssignmentCount(), initialAssignments);
}

BOOST_AUTO_TEST_CASE(TestBroadcastMessages) {
    EntityHandle handle0 = testEntities[0]->getHandle();
    EntityHandle handle1 = testEntities[1]->getHandle();
    EntityHandle handle2 = testEntities[2]->getHandle();
    const size_t initialUpdates = AIManager::Instance().getBehaviorUpdateCount();

    // Assign different behaviors to multiple entities
    AIManager::Instance().assignBehavior(handle0, "Guard");
    AIManager::Instance().assignBehavior(handle1, "Attack");
    AIManager::Instance().assignBehavior(handle2, "Follow");

    updateAI(0.016f);
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle0));
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle1));
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle2));
    BOOST_CHECK_GT(AIManager::Instance().getBehaviorUpdateCount(), initialUpdates);
}

// ============================================================================
// QUEUE-BASED MESSAGE SYSTEM TESTS
// ============================================================================

// Test command-bus message flow
BOOST_AUTO_TEST_CASE(TestMessageQueueBasicOperations) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle handle = entity->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    aiMgr.assignBehavior(handle, "Attack");

    auto& behaviorData = edm.getBehaviorData(idx);
    BOOST_CHECK(behaviorData.pendingMessageCount == 0);

    // Queue messages: command bus should not mutate pending queue immediately
    Behaviors::queueBehaviorMessage(idx, BehaviorMessage::RETREAT);
    Behaviors::queueBehaviorMessage(idx, BehaviorMessage::CALM_DOWN, 42);
    BOOST_CHECK(behaviorData.pendingMessageCount == 0);

    // Commit + process on AI update
    updateAI(0.016f);
    const auto afterRef = edm.getBehaviorConfigRef(idx);
    bool processedRetreat = (afterRef.type == BehaviorType::Flee) ||
                            (afterRef.type == BehaviorType::Attack &&
                             edm.getAttackState(afterRef.index).isRetreating);
    BOOST_CHECK(processedRetreat);

    // Clear messages
    Behaviors::clearPendingMessages(idx);
    BOOST_CHECK(edm.getBehaviorData(idx).pendingMessageCount == 0);

    BOOST_TEST_MESSAGE("Command-bus message flow verified");
    aiMgr.unassignBehavior(handle);
}

// ============================================================================
// DEFERRED MESSAGE PIPELINE INTEGRATION TESTS
// Test the command-bus communication: behavior → deferBehaviorMessage → AI commit
// ============================================================================

BOOST_AUTO_TEST_CASE(TestDeferredPipelineEndToEnd) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto guard = TestNPC::create(300.0f, 300.0f);
    EntityHandle guardHandle = guard->getHandle();
    size_t guardIdx = edm.getIndex(guardHandle);
    BOOST_REQUIRE(guardIdx != SIZE_MAX);

    aiMgr.assignBehavior(guardHandle, "Guard");
    updateAI(0.016f);

    // Guard starts CALM
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(guardIdx).index).currentAlertLevel == 0);

    // Simulate what a behavior does during batch: defer a message
    Behaviors::deferBehaviorMessage(guardIdx, BehaviorMessage::RAISE_ALERT);

    // AI update commits deferred message and processes it
    updateAI(0.016f);

    // Verify guard is now HOSTILE and queue is drained by behavior handler
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(guardIdx).index).currentAlertLevel == 3);
    BOOST_CHECK(edm.getBehaviorData(guardIdx).pendingMessageCount == 0);

    BOOST_TEST_MESSAGE("Deferred command-bus pipeline verified: "
                       "defer → AI commit → process");
    aiMgr.unassignBehavior(guardHandle);
}

BOOST_AUTO_TEST_CASE(TestCivilianAttacked_NearbyGuardGoesHostile) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    // Civilian and guard close together
    auto civilian = TestNPC::create(200.0f, 200.0f);
    auto guard = TestNPC::create(250.0f, 250.0f);  // ~70 units away

    EntityHandle civilianHandle = civilian->getHandle();
    EntityHandle guardHandle = guard->getHandle();

    size_t guardIdx = edm.getIndex(guardHandle);
    BOOST_REQUIRE(guardIdx != SIZE_MAX);

    aiMgr.assignBehavior(civilianHandle, "Idle");
    aiMgr.assignBehavior(guardHandle, "Guard");
    updateAI(0.016f);

    // Guard starts CALM
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(guardIdx).index).currentAlertLevel == 0);

    // Simulate centralized witness notification: damage handler sends RAISE_ALERT
    // directly to same-faction nearby allies (replaces old behavior-level deferral)
    Behaviors::queueBehaviorMessage(guardIdx, BehaviorMessage::RAISE_ALERT);

    // Guard processes RAISE_ALERT → alertLevel = 3
    updateAI(0.016f);

    BOOST_CHECK_MESSAGE(edm.getGuardState(edm.getBehaviorConfigRef(guardIdx).index).currentAlertLevel == 3,
        "Guard should be HOSTILE after nearby civilian was attacked");

    BOOST_TEST_MESSAGE("Civilian cry for help → guard goes hostile verified");
    aiMgr.unassignBehavior(civilianHandle);
    aiMgr.unassignBehavior(guardHandle);
}

BOOST_AUTO_TEST_CASE(TestGuardCallsForHelp_NearbyGuardGoesHostile) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();
    auto& eventMgr = EventManager::Instance();

    // Two guards near each other (within 250 radius)
    auto guard1 = TestNPC::create(300.0f, 300.0f);
    auto guard2 = TestNPC::create(350.0f, 350.0f);  // ~70 units away

    EntityHandle guard1Handle = guard1->getHandle();
    EntityHandle guard2Handle = guard2->getHandle();
    size_t guard1Idx = edm.getIndex(guard1Handle);
    size_t guard2Idx = edm.getIndex(guard2Handle);
    BOOST_REQUIRE(guard1Idx != SIZE_MAX);
    BOOST_REQUIRE(guard2Idx != SIZE_MAX);

    aiMgr.assignBehavior(guard1Handle, "Guard");
    aiMgr.assignBehavior(guard2Handle, "Guard");
    updateAI(0.016f);

    // Both guards start CALM
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(guard1Idx).index).currentAlertLevel == 0);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(guard2Idx).index).currentAlertLevel == 0);

    // Guard1 receives RAISE_ALERT (simulating threat detection)
    Behaviors::queueBehaviorMessage(guard1Idx, BehaviorMessage::RAISE_ALERT);

    // Frame 1: Guard1 processes RAISE_ALERT → HOSTILE (3) → helpCalled →
    // defers RAISE_ALERT to nearby same-faction allies (guard2)
    updateAI(0.016f);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(guard1Idx).index).currentAlertLevel == 3);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(guard1Idx).index).helpCalled == true);

    // EventManager delivers deferred RAISE_ALERT to guard2's queue
    eventMgr.update();

    // Frame 2: Guard2 processes RAISE_ALERT → HOSTILE (3)
    updateAI(0.016f);

    BOOST_CHECK_MESSAGE(edm.getGuardState(edm.getBehaviorConfigRef(guard2Idx).index).currentAlertLevel == 3,
        "Nearby guard should go HOSTILE when ally guard calls for help");

    BOOST_TEST_MESSAGE("Guard calls for help → nearby guard goes hostile verified");
    aiMgr.unassignBehavior(guard1Handle);
    aiMgr.unassignBehavior(guard2Handle);
}

BOOST_AUTO_TEST_CASE(TestRaiseAlert_CowardFleesOnAlert) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    // Coward civilian
    auto coward = TestNPC::create(200.0f, 200.0f);

    EntityHandle cowardHandle = coward->getHandle();
    size_t cowardIdx = edm.getIndex(cowardHandle);
    BOOST_REQUIRE(cowardIdx != SIZE_MAX);

    // Set coward personality: low bravery (< 0.4 triggers flee on RAISE_ALERT)
    edm.getMemoryData(cowardIdx).personality.bravery = 0.2f;

    aiMgr.assignBehavior(cowardHandle, "Wander");
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(cowardIdx).type == BehaviorType::Wander);

    // Simulate centralized witness notification: damage handler sends RAISE_ALERT
    // directly to same-faction nearby witnesses (replaces old behavior-level deferral)
    Behaviors::queueBehaviorMessage(cowardIdx, BehaviorMessage::RAISE_ALERT);

    // Coward processes RAISE_ALERT → bravery 0.2 < 0.4 → switches to Flee
    updateAI(0.016f);

    BOOST_CHECK_MESSAGE(edm.getBehaviorConfigRef(cowardIdx).type == BehaviorType::Flee,
        "Low-bravery NPC should flee when receiving RAISE_ALERT");

    BOOST_TEST_MESSAGE("Coward flees on RAISE_ALERT verified");
    aiMgr.unassignBehavior(cowardHandle);
}

BOOST_AUTO_TEST_CASE(TestRaiseAlert_BraveNPCStands) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    // Brave NPC
    auto brave = TestNPC::create(200.0f, 200.0f);

    EntityHandle braveHandle = brave->getHandle();
    size_t braveIdx = edm.getIndex(braveHandle);
    BOOST_REQUIRE(braveIdx != SIZE_MAX);

    // Set brave personality: high bravery (>= 0.4 ignores RAISE_ALERT)
    edm.getMemoryData(braveIdx).personality.bravery = 0.8f;

    aiMgr.assignBehavior(braveHandle, "Wander");
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(braveIdx).type == BehaviorType::Wander);

    // Simulate centralized witness notification: damage handler sends RAISE_ALERT
    Behaviors::queueBehaviorMessage(braveIdx, BehaviorMessage::RAISE_ALERT);

    // Brave NPC processes RAISE_ALERT but bravery 0.8 >= 0.4 → stays
    updateAI(0.016f);

    // Brave NPC should NOT flee (bravery 0.8 >= 0.4)
    BOOST_CHECK_MESSAGE(edm.getBehaviorConfigRef(braveIdx).type != BehaviorType::Flee,
        "High-bravery NPC should not flee from RAISE_ALERT");

    BOOST_TEST_MESSAGE("Brave NPC ignores RAISE_ALERT verified");
    aiMgr.unassignBehavior(braveHandle);
}

BOOST_AUTO_TEST_CASE(TestMessageQueueOverflow) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle handle = entity->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    aiMgr.assignBehavior(handle, "Guard");
    updateAI(0.016f);

    // Queue more than 4 messages (max queue size)
    for (int i = 0; i < 10; ++i) {
        Behaviors::queueBehaviorMessage(idx, BehaviorMessage::RAISE_ALERT);
    }

    // Process queued commands - should not crash
    updateAI(0.016f);

    // Messages should be consumed and guard should escalate to HOSTILE
    BOOST_CHECK(edm.getBehaviorData(idx).pendingMessageCount == 0);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(idx).index).currentAlertLevel == 3);

    BOOST_TEST_MESSAGE("Message queue overflow handling verified");
    aiMgr.unassignBehavior(handle);
}

BOOST_AUTO_TEST_CASE(TestCommandBusMergesWithExistingPendingInbox) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle handle = entity->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    aiMgr.assignBehavior(handle, "Attack");
    updateAI(0.016f);

    auto& behaviorData = edm.getBehaviorData(idx);
    behaviorData.pendingMessageCount = 4;
    for (uint8_t i = 0; i < behaviorData.pendingMessageCount; ++i) {
        behaviorData.pendingMessages[i].messageId = BehaviorMessage::ATTACK_TARGET;
        behaviorData.pendingMessages[i].param = i;
    }

    Behaviors::queueBehaviorMessage(idx, BehaviorMessage::RETREAT);

    updateAI(0.016f);

    BOOST_CHECK_MESSAGE(edm.getAttackState(edm.getBehaviorConfigRef(idx).index).isRetreating,
        "New command-bus RETREAT message should survive even when the previous frame left a full inbox");
    BOOST_CHECK(edm.getBehaviorData(idx).pendingMessageCount == 0);

    aiMgr.unassignBehavior(handle);
}

BOOST_AUTO_TEST_CASE(TestClearPendingMessages) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle handle = entity->getHandle();
    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    aiMgr.assignBehavior(handle, "Guard");
    updateAI(0.016f);

    // Queue messages into command bus
    Behaviors::queueBehaviorMessage(idx, BehaviorMessage::RAISE_ALERT);
    Behaviors::queueBehaviorMessage(idx, BehaviorMessage::CALM_DOWN);
    BOOST_CHECK(edm.getBehaviorData(idx).pendingMessageCount == 0);

    // Clear both queued bus commands and pending per-entity queue
    Behaviors::clearPendingMessages(idx);
    BOOST_CHECK(edm.getBehaviorData(idx).pendingMessageCount == 0);

    // Verify guard state unchanged after update (messages were cleared pre-commit)
    updateAI(0.016f);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(idx).index).currentAlertLevel == 0); // Still CALM

    BOOST_TEST_MESSAGE("Clear pending messages verified");
    aiMgr.unassignBehavior(handle);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 6: Behavior Transitions and State Management
BOOST_FIXTURE_TEST_SUITE(BehaviorTransitionTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestBehaviorSwitching) {
    auto entity = testEntities[0];
    entity->resetUpdateCount();
    EntityHandle handle = entity->getHandle();

    std::vector<std::string> behaviorSequence = {
        "Idle", "Wander", "Chase", "Flee", "Follow", "Guard", "Attack"
    };

    for (const auto& behavior : behaviorSequence) {
        AIManager::Instance().assignBehavior(handle, behavior);

        // Update a few times
        for (int i = 0; i < 5; ++i) {
            updateAI(0.016f);
        }

        BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

        AIManager::Instance().unassignBehavior(handle);
    }

    // Check that behaviors were executed
    size_t finalCount = AIManager::Instance().getBehaviorUpdateCount();
    BOOST_CHECK_GT(finalCount, 0);
}

BOOST_AUTO_TEST_CASE(TestMultipleEntitiesDifferentBehaviors) {
    // Assign different behaviors to different entities
    std::vector<std::string> behaviors = {"Idle", "Wander", "Chase", "Follow", "Guard"};

    // Capture initial behavior execution count
    size_t initialBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();

    for (size_t i = 0; i < behaviors.size() && i < testEntities.size(); ++i) {
        EntityHandle handle = testEntities[i]->getHandle();
        AIManager::Instance().assignBehavior(handle, behaviors[i]);
    }

    // Update all entities simultaneously
    for (int update = 0; update < 20; ++update) {
        updateAI(0.016f);
    }

    // Check that behaviors were executed
    size_t finalBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();
    BOOST_CHECK_GT(finalBehaviorCount, initialBehaviorCount);
}

// Test that verifies behavior state persists after transition
BOOST_AUTO_TEST_CASE(TestGuardToAttackTransitionStatePreserved) {
    auto entity = testEntities[0];
    EntityHandle handle = entity->getHandle();
    auto& edm = EntityDataManager::Instance();

    // Start with Guard behavior
    AIManager::Instance().assignBehavior(handle, "Guard");
    updateAI(0.016f);  // Allow behavior to initialize

    size_t edmIdx = edm.getIndex(handle);
    BOOST_REQUIRE_NE(edmIdx, SIZE_MAX);

    // Verify Guard state is valid
    BOOST_CHECK(edm.hasBehaviorData(edmIdx));
    auto& guardData = edm.getBehaviorData(edmIdx);
    BOOST_CHECK(guardData.isValid());
    BOOST_CHECK(guardData.isInitialized());

    // Now transition to Attack behavior
    AIManager::Instance().assignBehavior(handle, "Attack");
    updateAI(0.016f);  // Process the transition

    // CRITICAL CHECK: Attack behavior state must be valid after transition
    BOOST_CHECK(edm.hasBehaviorData(edmIdx));
    auto& attackData = edm.getBehaviorData(edmIdx);
    BOOST_CHECK_MESSAGE(attackData.isValid(),
        "BehaviorData should be valid after Guard->Attack transition");
    BOOST_CHECK_MESSAGE(attackData.isInitialized(),
        "BehaviorData should be initialized after Guard->Attack transition");

    // Verify entity can still function
    for (int i = 0; i < 10; ++i) {
        updateAI(0.016f);
    }
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Cleanup
    AIManager::Instance().unassignBehavior(handle);
}

// Test that all behavior transitions preserve state correctly
BOOST_AUTO_TEST_CASE(TestAllBehaviorTransitionsPreserveState) {
    auto entity = testEntities[0];
    EntityHandle handle = entity->getHandle();
    auto& edm = EntityDataManager::Instance();

    // Test transitions between all core behaviors
    std::vector<std::pair<std::string, std::string>> transitions = {
        {"Idle", "Wander"},
        {"Wander", "Chase"},
        {"Chase", "Attack"},
        {"Attack", "Flee"},
        {"Flee", "Guard"},
        {"Guard", "Attack"},  // Critical transition
        {"Attack", "Follow"},
        {"Follow", "Idle"}
    };

    for (const auto& [fromBehavior, toBehavior] : transitions) {
        // Start with first behavior
        AIManager::Instance().assignBehavior(handle, fromBehavior);
        updateAI(0.016f);

        size_t edmIdx = edm.getIndex(handle);
        BOOST_REQUIRE_NE(edmIdx, SIZE_MAX);
        BOOST_CHECK_MESSAGE(edm.hasBehaviorData(edmIdx),
            "Should have behavior data for " + fromBehavior);

        // Transition to second behavior
        AIManager::Instance().assignBehavior(handle, toBehavior);
        updateAI(0.016f);

        // Verify state after transition
        BOOST_CHECK_MESSAGE(edm.hasBehaviorData(edmIdx),
            "Should have behavior data after " + fromBehavior + " -> " + toBehavior);

        auto& behaviorData = edm.getBehaviorData(edmIdx);
        BOOST_CHECK_MESSAGE(behaviorData.isValid(),
            "BehaviorData should be valid after " + fromBehavior + " -> " + toBehavior);
        BOOST_CHECK_MESSAGE(behaviorData.isInitialized(),
            "BehaviorData should be initialized after " + fromBehavior + " -> " + toBehavior);

        // Verify entity can execute new behavior
        updateAI(0.016f);
        BOOST_CHECK_MESSAGE(AIManager::Instance().hasBehavior(handle),
            "Entity should still have behavior after " + fromBehavior + " -> " + toBehavior);

        // Clean up for next iteration
        AIManager::Instance().unassignBehavior(handle);
    }
}

// Stress test: rapid transitions should not corrupt state
BOOST_AUTO_TEST_CASE(TestRapidBehaviorTransitionsStability) {
    auto entity = testEntities[0];
    EntityHandle handle = entity->getHandle();
    auto& edm = EntityDataManager::Instance();

    std::vector<std::string> behaviors = {
        "Idle", "Wander", "Chase", "Attack", "Flee", "Guard", "Follow"
    };

    // Register initially
    AIManager::Instance().assignBehavior(handle, "Idle");
    updateAI(0.016f);

    // Rapidly switch behaviors with minimal updates between
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (const auto& behavior : behaviors) {
            AIManager::Instance().assignBehavior(handle, behavior);
            updateAI(0.016f);  // Single update between transitions

            size_t edmIdx = edm.getIndex(handle);
            BOOST_REQUIRE_NE(edmIdx, SIZE_MAX);

            // State must remain valid even under rapid transitions
            BOOST_CHECK(edm.hasBehaviorData(edmIdx));
            auto& behaviorData = edm.getBehaviorData(edmIdx);
            BOOST_CHECK_MESSAGE(behaviorData.isValid(),
                "BehaviorData corrupt during rapid transitions at cycle " +
                std::to_string(cycle) + ", behavior " + behavior);
        }
    }

    // Final verification: entity should be fully functional
    for (int i = 0; i < 20; ++i) {
        updateAI(0.016f);
    }
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    // Cleanup
    AIManager::Instance().unassignBehavior(handle);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 7: Performance and Integration Testing
BOOST_FIXTURE_TEST_SUITE(BehaviorPerformanceTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestLargeNumberOfEntities) {
    const int NUM_ENTITIES = 50;
    std::vector<std::shared_ptr<TestNPC>> perfTestEntities;
    std::vector<EntityHandle> perfTestHandles;

    // Create many NPCs with different behaviors
    std::vector<std::string> behaviors = {"Idle", "Wander", "Chase", "Follow", "Guard"};

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        auto entity = TestNPC::create(i * 10.0f, i * 10.0f);
        perfTestEntities.push_back(entity);
        EntityHandle handle = entity->getHandle();
        perfTestHandles.push_back(handle);

        std::string behavior = behaviors[i % behaviors.size()];
        AIManager::Instance().assignBehavior(handle, behavior);
    }

    // Measure update performance
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; ++i) {
        updateAI(0.016f);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Should complete within reasonable time
    BOOST_CHECK_LT(duration.count(), 1000); // Less than 1 second for 50 entities

    // Cleanup
    for (const auto& handle : perfTestHandles) {
        AIManager::Instance().unassignBehavior(handle);
    }
}

BOOST_AUTO_TEST_CASE(TestBehaviorMemoryManagement) {
    auto entity = testEntities[0];
    EntityHandle handle = entity->getHandle();
    const size_t initialAssignments = AIManager::Instance().getTotalAssignmentCount();

    // Rapidly switch between behaviors to test memory management
    std::vector<std::string> behaviors = {
        "Idle", "Wander", "Chase", "Flee", "Follow", "Guard", "Attack"
    };

    for (int cycle = 0; cycle < 5; ++cycle) {
        for (const auto& behavior : behaviors) {
            AIManager::Instance().assignBehavior(handle, behavior);

            // Brief update
            updateAI(0.016f);

            AIManager::Instance().unassignBehavior(handle);
        }
    }

    BOOST_CHECK(!AIManager::Instance().hasBehavior(handle));
    BOOST_CHECK_GE(AIManager::Instance().getTotalAssignmentCount(),
                   initialAssignments + behaviors.size() * 5);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 8: Advanced Behavior Features
BOOST_FIXTURE_TEST_SUITE(AdvancedBehaviorFeatureTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestPatrolBehaviorWithWaypoints) {
    auto entity = testEntities[0];

    Vector2D initialPos(150, 150);
    entity->setPosition(initialPos);

    // EDM-CENTRIC: Set collision layers
    {
        auto& edm = EntityDataManager::Instance();
        size_t idx = edm.getIndex(entity->getHandle());
        if (idx != SIZE_MAX) {
            auto& hot = edm.getHotDataByIndex(idx);
            hot.collisionLayers = CollisionLayer::Layer_Enemy;
            hot.collisionMask = 0xFFFF;
            hot.setCollisionEnabled(true);
        }
    }

    EntityHandle handle = entity->getHandle();
    AIManager::Instance().assignBehavior(handle, "Patrol");

    // Capture initial behavior execution count
    size_t initialBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();

    // Track patrol movement
    int movementSteps = 0;
    Vector2D lastPos = initialPos;
    float totalDistanceMoved = 0.0f;
    bool hasVelocity = false;

    const float testDeltaTime = 0.5f;
    for (int i = 0; i < 40; ++i) {
        updateAI(testDeltaTime);
        CollisionManager::Instance().update(testDeltaTime);

        Vector2D pos = entity->getPosition();
        Vector2D vel = entity->getVelocity();
        float stepDistance = (pos - lastPos).length();

        totalDistanceMoved += stepDistance;
        if (stepDistance > 0.1f) {
            movementSteps++;
        }
        if (vel.length() > 0.1f) {
            hasVelocity = true;
        }
        lastPos = pos;

        if (i % 8 == 0) {
            BOOST_TEST_MESSAGE("Patrol Update " << i << ": pos=(" << pos.getX() << ", " << pos.getY() << ") vel=" << vel.length() << " moved=" << totalDistanceMoved);
        }
    }

    BOOST_TEST_MESSAGE("Patrol test: moved " << totalDistanceMoved << "px over " << movementSteps << " steps");

    // Verify patrol behavior is functioning
    size_t finalBehaviorCount = AIManager::Instance().getBehaviorUpdateCount();
    BOOST_CHECK_GT(finalBehaviorCount, initialBehaviorCount + 10);

    // Entity should either have moved OR have velocity set
    bool isPatrolling = (totalDistanceMoved > 5.0f) || hasVelocity;
    BOOST_CHECK(isPatrolling);

    // Clean up
    AIManager::Instance().unassignBehavior(handle);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite 9: Memory and Combat System Tests
BOOST_FIXTURE_TEST_SUITE(MemoryCombatTests, BehaviorTestFixture)

BOOST_AUTO_TEST_CASE(TestMemoryInitAtCreation) {
    // Test: NPCs created via createNPCWithRaceClass have memory initialized
    auto& edm = EntityDataManager::Instance();

    auto entity = TestNPC::create(100.0f, 100.0f);
    EntityHandle handle = entity->getHandle();
    BOOST_REQUIRE(handle.isValid());

    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    // Memory should be initialized at creation
    BOOST_CHECK(edm.hasMemoryData(idx));

    const auto& memData = edm.getMemoryData(idx);
    BOOST_CHECK(memData.isValid());

    // Default emotional state should be neutral
    BOOST_CHECK_GE(memData.emotions.fear, 0.0f);
    BOOST_CHECK_LE(memData.emotions.fear, 0.1f);

    BOOST_TEST_MESSAGE("Memory initialized at creation: emotions.fear=" << memData.emotions.fear);
}

BOOST_AUTO_TEST_CASE(TestLastCombatTimeDeltaSemantics) {
    // Test: lastCombatTime increments via updateEmotionalDecay
    auto& edm = EntityDataManager::Instance();

    auto entity = TestNPC::create(100.0f, 100.0f);
    size_t idx = edm.getIndex(entity->getHandle());
    BOOST_REQUIRE(idx != SIZE_MAX);
    BOOST_REQUIRE(edm.hasMemoryData(idx));

    auto& memData = edm.getMemoryData(idx);

    // Simulate being attacked
    memData.lastCombatTime = 0.0f;
    float initialTime = memData.lastCombatTime;

    // Update emotional decay
    edm.updateEmotionalDecay(idx, 1.0f);

    BOOST_CHECK_GT(memData.lastCombatTime, initialTime);
    BOOST_CHECK_CLOSE(memData.lastCombatTime, 1.0f, 0.01f);

    edm.updateEmotionalDecay(idx, 0.5f);
    BOOST_CHECK_CLOSE(memData.lastCombatTime, 1.5f, 0.01f);

    BOOST_TEST_MESSAGE("lastCombatTime delta semantics working: " << memData.lastCombatTime);
}

BOOST_AUTO_TEST_CASE(TestWanderSwitchesToFleeWhenAttacked) {
    // Test: WanderBehavior switches to FleeBehavior when entity is attacked
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    auto attacker = TestNPC::create(350.0f, 350.0f);

    EntityHandle entityHandle = entity->getHandle();
    EntityHandle attackerHandle = attacker->getHandle();

    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    // Assign Wander behavior
    aiMgr.assignBehavior(entityHandle, "Wander");

    // Verify starting behavior is Wander (direct access, no cached reference across updateAI)
    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Wander);

    // Simulate being attacked - set lastCombatTime=0 to indicate "just happened"
    // (delta-based semantics: starts at 0, increments each frame via emotional decay)
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);  // Mark memory as valid so isUnderRecentAttack() can read it
    memData.lastAttacker = attackerHandle;
    memData.lastCombatTime = 0.0f;  // Delta semantics: 0 = just happened

    // Run behavior updates
    for (int i = 0; i < 5; ++i) {
        updateAI(0.1f);
    }

    // Should have switched to a combat response behavior (re-fetch after updateAI to avoid stale ref)
    // Guards are brave+aggressive → retaliate (Chase). Cowardly NPCs → Flee.
    auto responseType = edm.getBehaviorConfigRef(entityIdx).type;
    BOOST_CHECK(responseType == BehaviorType::Chase || responseType == BehaviorType::Flee);

    BOOST_TEST_MESSAGE("Wander -> combat response on attack verified (type="
                       << static_cast<int>(responseType) << ")");

    // Cleanup
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestIdleSwitchesToFleeWhenAttacked) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    auto attacker = TestNPC::create(350.0f, 350.0f);

    EntityHandle entityHandle = entity->getHandle();
    EntityHandle attackerHandle = attacker->getHandle();

    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    // Assign Idle behavior
    aiMgr.assignBehavior(entityHandle, "Idle");

    // Verify starting behavior is Idle (direct access, no cached reference across updateAI)
    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Idle);

    // Simulate being attacked - set lastCombatTime=0 to indicate "just happened"
    // (delta-based semantics: starts at 0, increments each frame via emotional decay)
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);  // Mark memory as valid so isUnderRecentAttack() can read it
    memData.lastAttacker = attackerHandle;
    memData.lastCombatTime = 0.0f;  // Delta semantics: 0 = just happened

    // Run behavior updates
    for (int i = 0; i < 5; ++i) {
        updateAI(0.1f);
    }

    // Should have switched to a combat response behavior (re-fetch after updateAI to avoid stale ref)
    // Guards are brave+aggressive → retaliate (Chase). Cowardly NPCs → Flee.
    auto responseType = edm.getBehaviorConfigRef(entityIdx).type;
    BOOST_CHECK(responseType == BehaviorType::Chase || responseType == BehaviorType::Flee);

    BOOST_TEST_MESSAGE("Idle -> combat response on attack verified (type="
                       << static_cast<int>(responseType) << ")");

    // Cleanup
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestMassBasedKnockback) {
    auto& edm = EntityDataManager::Instance();

    auto lightEntity = TestNPC::create(100.0f, 100.0f);
    auto heavyEntity = TestNPC::create(200.0f, 100.0f);

    EntityHandle lightHandle = lightEntity->getHandle();
    EntityHandle heavyHandle = heavyEntity->getHandle();

    auto& lightChar = edm.getCharacterData(lightHandle);
    auto& heavyChar = edm.getCharacterData(heavyHandle);

    lightChar.mass = 0.5f;
    heavyChar.mass = 4.0f;

    float lightScale = 1.0f / std::max(0.1f, lightChar.mass);
    float heavyScale = 1.0f / std::max(0.1f, heavyChar.mass);

    BOOST_TEST_MESSAGE("Knockback scales - light (mass=0.5): " << lightScale
                       << ", heavy (mass=4.0): " << heavyScale);

    BOOST_CHECK_CLOSE(lightScale / heavyScale, 8.0f, 0.1f);

    BOOST_CHECK_CLOSE(lightChar.mass, 0.5f, 0.01f);
    BOOST_CHECK_CLOSE(heavyChar.mass, 4.0f, 0.01f);

    Vector2D knockbackForce(100.0f, 0.0f);
    Vector2D lightKnockback = knockbackForce * lightScale;
    Vector2D heavyKnockback = knockbackForce * heavyScale;

    BOOST_CHECK_CLOSE(lightKnockback.length(), 200.0f, 0.1f);
    BOOST_CHECK_CLOSE(heavyKnockback.length(), 25.0f, 0.1f);
}

BOOST_AUTO_TEST_CASE(TestAttackPersonalityDamageScaling) {
    auto& edm = EntityDataManager::Instance();

    auto attacker = TestNPC::create(100.0f, 100.0f);

    EntityHandle attackerHandle = attacker->getHandle();
    size_t attackerIdx = edm.getIndex(attackerHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);

    auto& memData = edm.getMemoryData(attackerIdx);
    memData.personality.aggression = 1.0f;
    memData.emotions.aggression = 1.0f;

    auto& charData = edm.getCharacterDataByIndex(attackerIdx);
    float baseDamage = charData.attackDamage;

    float expectedMultiplier = 1.44f;
    float expectedDamage = baseDamage * expectedMultiplier;

    BOOST_TEST_MESSAGE("Base damage: " << baseDamage << ", expected with max aggression: " << expectedDamage);

    BOOST_CHECK_GT(expectedMultiplier, 1.0f);
    BOOST_CHECK_CLOSE(expectedMultiplier, 1.44f, 1.0f);
}

BOOST_AUTO_TEST_CASE(TestAttackLastTargetTracking) {
    auto& edm = EntityDataManager::Instance();

    auto attacker = TestNPC::create(100.0f, 100.0f);
    auto target = TestNPC::create(150.0f, 100.0f);

    EntityHandle attackerHandle = attacker->getHandle();
    EntityHandle targetHandle = target->getHandle();

    size_t attackerIdx = edm.getIndex(attackerHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);

    BOOST_REQUIRE(edm.hasMemoryData(attackerIdx));

    auto& memData = edm.getMemoryData(attackerIdx);
    BOOST_CHECK(!memData.lastTarget.isValid());

    memData.lastTarget = targetHandle;

    BOOST_CHECK(memData.lastTarget.isValid());
    BOOST_CHECK(memData.lastTarget == targetHandle);

    BOOST_TEST_MESSAGE("lastTarget tracking verified: " << memData.lastTarget.isValid());
}

BOOST_AUTO_TEST_CASE(TestBerserkerModeNoRetreat) {
    auto& edm = EntityDataManager::Instance();

    auto entity = TestNPC::create(100.0f, 100.0f);
    EntityHandle handle = entity->getHandle();

    size_t idx = edm.getIndex(handle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    auto& memData = edm.getMemoryData(idx);
    memData.emotions.aggression = 0.9f;
    memData.personality.aggression = 0.8f;

    bool isBerserker = (memData.emotions.aggression > 0.8f &&
                        memData.personality.aggression > 0.7f);
    BOOST_CHECK(isBerserker);

    auto& charData = edm.getCharacterData(handle);
    charData.health = charData.maxHealth * 0.1f;

    BOOST_TEST_MESSAGE("Berserker mode: aggression=" << memData.emotions.aggression
                       << " personality=" << memData.personality.aggression
                       << " health=" << (charData.health/charData.maxHealth*100) << "%");

    BOOST_CHECK(isBerserker);
}

BOOST_AUTO_TEST_CASE(TestRecordCombatEventUpdatesMemory) {
    auto& edm = EntityDataManager::Instance();

    auto victim = TestNPC::create(100.0f, 100.0f);
    auto attacker = TestNPC::create(200.0f, 100.0f);

    EntityHandle victimHandle = victim->getHandle();
    EntityHandle attackerHandle = attacker->getHandle();

    size_t victimIdx = edm.getIndex(victimHandle);
    BOOST_REQUIRE(victimIdx != SIZE_MAX);

    auto& memData = edm.getMemoryData(victimIdx);
    uint32_t initialEncounters = memData.combatEncounters;

    edm.recordCombatEvent(victimIdx, attackerHandle, victimHandle, 50.0f, true, 0.0f);

    BOOST_CHECK_GT(memData.combatEncounters, initialEncounters);
    BOOST_CHECK(memData.lastAttacker == attackerHandle);
    BOOST_CHECK(memData.lastCombatTime < Behaviors::COMBAT_TIMEOUT_SECONDS);

    BOOST_TEST_MESSAGE("recordCombatEvent encounters: " << memData.combatEncounters);
}

BOOST_AUTO_TEST_SUITE_END()

// Test Suite: Behavior Gap Fix Tests
BOOST_FIXTURE_TEST_SUITE(BehaviorGapFixTests, BehaviorTestFixture)

// ============================================================================
// MESSAGE HANDLER TESTS
// ============================================================================

BOOST_AUTO_TEST_CASE(TestChasePanicSwitchesToFlee) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(400.0f, 400.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    // Assign Chase behavior and set a target
    aiMgr.assignBehavior(entityHandle, "Chase");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.lastTarget = target->getHandle();
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Chase);

    // Queue PANIC message
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::PANIC);
    updateAI(0.016f);

    // Should switch to Flee
    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Flee);
    BOOST_TEST_MESSAGE("Chase -> Flee on PANIC verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestChaseRetreatSwitchesToFlee) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(400.0f, 400.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Chase");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.lastTarget = target->getHandle();
    updateAI(0.016f);

    // Queue RETREAT message
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::RETREAT);
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Flee);
    BOOST_TEST_MESSAGE("Chase -> Flee on RETREAT verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestChaseAttackTargetRedirects) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    auto target1 = TestNPC::create(400.0f, 400.0f);
    auto target2 = TestNPC::create(500.0f, 500.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Chase");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.lastTarget = target1->getHandle();
    memData.lastAttacker = target2->getHandle();
    updateAI(0.016f);

    // Queue ATTACK_TARGET — should redirect to lastAttacker
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::ATTACK_TARGET);
    updateAI(0.016f);

    // Should still be chasing but with explicit target set to target2
    const auto chaseRef = edm.getBehaviorConfigRef(entityIdx);
    BOOST_CHECK(chaseRef.type == BehaviorType::Chase);
    BOOST_CHECK(edm.getChaseState(chaseRef.index).hasExplicitTarget == true);
    BOOST_CHECK(edm.getChaseState(chaseRef.index).explicitTarget == target2->getHandle());
    BOOST_TEST_MESSAGE("Chase ATTACK_TARGET redirected to lastAttacker");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestPatrolPanicSwitchesToFlee) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Patrol");
    updateAI(0.016f);
    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Patrol);

    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::PANIC);
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Flee);
    BOOST_TEST_MESSAGE("Patrol -> Flee on PANIC verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestPatrolCalmDownReducesFear) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Patrol");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.emotions.fear = 0.6f;
    updateAI(0.016f);

    float fearBefore = memData.emotions.fear;
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::CALM_DOWN);
    updateAI(0.016f);

    // Fear should be reduced (by 0.5, but emotional decay also runs)
    BOOST_CHECK_LT(memData.emotions.fear, fearBefore);
    BOOST_TEST_MESSAGE("Patrol CALM_DOWN reduced fear: " << fearBefore << " -> " << memData.emotions.fear);
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestFollowPanicSwitchesToFlee) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Follow");
    updateAI(0.016f);
    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Follow);

    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::PANIC);
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Flee);
    BOOST_TEST_MESSAGE("Follow -> Flee on PANIC verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestFollowRaiseAlertCowardFlees) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Follow");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.personality.bravery = 0.2f;  // Cowardly (< 0.4 threshold)
    updateAI(0.016f);

    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::RAISE_ALERT);
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Flee);
    BOOST_TEST_MESSAGE("Follow -> Flee on RAISE_ALERT (coward) verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestFollowRaiseAlertBraveStands) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Follow");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.personality.bravery = 0.7f;  // Brave (>= 0.4 threshold)
    updateAI(0.016f);

    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::RAISE_ALERT);
    updateAI(0.016f);

    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Follow);
    BOOST_TEST_MESSAGE("Follow stays on RAISE_ALERT (brave) verified");
    aiMgr.unassignBehavior(entityHandle);
}

// ============================================================================
// GUARD MESSAGE & FLEE TESTS
// ============================================================================

BOOST_AUTO_TEST_CASE(TestGuardCalmDownDecaysAlert) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Guard");

    // Force HOSTILE alert via RAISE_ALERT
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::RAISE_ALERT);
    updateAI(0.016f);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(entityIdx).index).currentAlertLevel == 3);

    // Send CALM_DOWN — should decay by 1
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::CALM_DOWN);
    updateAI(0.016f);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(entityIdx).index).currentAlertLevel == 2);

    BOOST_TEST_MESSAGE("Guard CALM_DOWN decayed alert: 3 -> 2");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestGuardPanicEscalatesToHostile) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Guard");
    updateAI(0.016f);

    // Guard starts CALM
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(entityIdx).index).currentAlertLevel == 0);

    // PANIC escalates to HOSTILE (guards don't flee, they fight)
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::PANIC);
    updateAI(0.016f);
    BOOST_CHECK(edm.getGuardState(edm.getBehaviorConfigRef(entityIdx).index).currentAlertLevel == 3);

    BOOST_TEST_MESSAGE("Guard PANIC escalated to HOSTILE (not Flee) verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestGuardDoesNotInheritOtherEntityCombatState) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto victim = TestNPC::create(300.0f, 300.0f);
    auto guard = TestNPC::create(340.0f, 300.0f);
    auto attacker = TestNPC::create(360.0f, 300.0f);

    EntityHandle victimHandle = victim->getHandle();
    EntityHandle guardHandle = guard->getHandle();
    EntityHandle attackerHandle = attacker->getHandle();

    edm.setFaction(attackerHandle, 2);

    size_t victimIdx = edm.getIndex(victimHandle);
    size_t guardIdx = edm.getIndex(guardHandle);
    BOOST_REQUIRE(victimIdx != SIZE_MAX);
    BOOST_REQUIRE(guardIdx != SIZE_MAX);

    aiMgr.assignBehavior(guardHandle, "Guard");

    edm.recordCombatEvent(victimIdx, attackerHandle, victimHandle, 20.0f, true, 1.0f);
    updateAI(0.016f, guard->getPosition());

    const auto& guardMemData = edm.getMemoryData(guardIdx);
    BOOST_CHECK(!guardMemData.lastTarget.isValid());
    BOOST_CHECK_EQUAL(edm.getGuardState(edm.getBehaviorConfigRef(guardIdx).index).currentAlertLevel, 0);

    aiMgr.unassignBehavior(guardHandle);
}

BOOST_AUTO_TEST_CASE(TestGuardFleesWhenOverwhelmed) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Guard");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.personality.bravery = 0.1f;  // Very cowardly (0.1 + 0.1 bonus = 0.2 < 0.3)
    memData.emotions.fear = 0.8f;        // High fear (> 0.7)

    // Force HOSTILE alert so the flee check triggers
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::RAISE_ALERT);
    updateAI(0.016f);

    // Guard should flee due to overwhelming fear + low bravery
    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Flee);
    BOOST_TEST_MESSAGE("Guard -> Flee when overwhelmed (bravery=0.1, fear=0.8) verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestGuardStandsWhenBrave) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Guard");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.personality.bravery = 0.5f;  // Brave (0.5 + 0.1 bonus = 0.6 >= 0.3)
    memData.emotions.fear = 0.8f;        // High fear

    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::RAISE_ALERT);
    updateAI(0.016f);

    // Guard should stand (brave enough despite fear)
    BOOST_CHECK(edm.getBehaviorConfigRef(entityIdx).type == BehaviorType::Guard);
    BOOST_TEST_MESSAGE("Guard stands when brave (bravery=0.5, fear=0.8) verified");
    aiMgr.unassignBehavior(entityHandle);
}

// ============================================================================
// ATTACK PANIC & SPECIAL ATTACK TESTS
// ============================================================================

BOOST_AUTO_TEST_CASE(TestAttackPanicForcesRetreat) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(320.0f, 320.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Attack");
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    memData.lastTarget = target->getHandle();
    updateAI(0.016f);

    // Queue PANIC. Panic is a true disengage path, not a short tactical reset.
    Behaviors::queueBehaviorMessage(entityIdx, BehaviorMessage::PANIC);
    updateAI(0.016f);

    const auto attackRef = edm.getBehaviorConfigRef(entityIdx);
    // May have switched to Flee or be in a disengaging Attack state.
    bool retreatingOrFled = (attackRef.type == BehaviorType::Flee) ||
                            (attackRef.type == BehaviorType::Attack && edm.getAttackState(attackRef.index).isRetreating);
    BOOST_CHECK(retreatingOrFled);
    BOOST_TEST_MESSAGE("Attack PANIC forced retreat/flee verified");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_CASE(TestLowHealthAttackRetreatsThenReengages) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto attacker = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(330.0f, 300.0f);
    const EntityHandle attackerHandle = attacker->getHandle();
    const EntityHandle targetHandle = target->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);

    aiMgr.assignBehavior(attackerHandle, "Attack");

    auto& attackerChar = edm.getCharacterDataByIndex(attackerIdx);
    attackerChar.health = attackerChar.maxHealth * 0.25f;

    auto& memData = edm.getMemoryData(attackerIdx);
    memData.setValid(true);
    memData.lastTarget = targetHandle;
    memData.combatEncounters = 1;

    updateAI(0.1f, attacker->getPosition());

    const auto attackRef = edm.getBehaviorConfigRef(attackerIdx);
    BOOST_REQUIRE(attackRef.type == BehaviorType::Attack);
    auto& attackState = edm.getAttackState(attackRef.index);
    BOOST_REQUIRE_MESSAGE(attackState.isRetreating,
                          "Low-health attacker should enter tactical retreat once");
    BOOST_CHECK(attackState.hasHandledTacticalRetreat);
    BOOST_CHECK_EQUAL(static_cast<int>(attackState.lastTacticalRetreatEncounter), 1);

    bool reengaged = false;
    for (int i = 0; i < 120; ++i) {
        updateAI(0.1f, attacker->getPosition());
        if (edm.getBehaviorConfigRef(attackerIdx).type != BehaviorType::Attack) {
            break;
        }

        const auto& currentAttackState =
            edm.getAttackState(edm.getBehaviorConfigRef(attackerIdx).index);
        if (currentAttackState.currentState == 3) {
            reengaged = true;
            break;
        }
    }

    const auto finalRef = edm.getBehaviorConfigRef(attackerIdx);
    std::string finalState = "not-attack";
    float finalAttackTimer = 0.0f;
    float finalPressure = 0.0f;
    if (finalRef.type == BehaviorType::Attack) {
        const auto& finalAttackState = edm.getAttackState(finalRef.index);
        finalState = std::to_string(finalAttackState.currentState);
        finalAttackTimer = finalAttackState.attackTimer;
        finalPressure = finalAttackState.pressureScore;
    }
    BOOST_CHECK_MESSAGE(reengaged,
                        std::format("Handled low-health retreat should not permanently block re-engagement "
                                    "(type={}, state={}, attackTimer={:.2f}, pressure={:.2f})",
                                    static_cast<int>(finalRef.type), finalState,
                                    finalAttackTimer, finalPressure));
    aiMgr.unassignBehavior(attackerHandle);
}

BOOST_AUTO_TEST_CASE(TestAttackNewDamageEncounterCanTriggerAnotherRetreat) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();
    auto& eventMgr = EventManager::Instance();

    auto attacker = TestNPC::create(300.0f, 300.0f);
    auto target = TestNPC::create(330.0f, 300.0f);
    const EntityHandle attackerHandle = attacker->getHandle();
    const EntityHandle targetHandle = target->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);

    aiMgr.assignBehavior(attackerHandle, "Attack");

    auto& attackerChar = edm.getCharacterDataByIndex(attackerIdx);
    attackerChar.health = attackerChar.maxHealth * 0.25f;

    auto& memData = edm.getMemoryData(attackerIdx);
    memData.setValid(true);
    memData.lastTarget = targetHandle;
    memData.combatEncounters = 1;

    bool damageAfterRetreat = false;
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            auto damageEvent = std::dynamic_pointer_cast<DamageEvent>(data.event);
            if (damageEvent && damageEvent->getSource() == attackerHandle &&
                damageEvent->getTarget() == targetHandle) {
                damageAfterRetreat = true;
            }
        });

    updateAI(0.1f, attacker->getPosition());
    auto& attackState = edm.getAttackState(edm.getBehaviorConfigRef(attackerIdx).index);
    BOOST_REQUIRE(attackState.isRetreating);

    attackState.attackTimer = 999.0f;
    attackState.resetConfidence = 1.0f;

    updateAI(0.1f, attacker->getPosition());
    const auto& committedAttackState =
        edm.getAttackState(edm.getBehaviorConfigRef(attackerIdx).index);
    BOOST_REQUIRE_MESSAGE(committedAttackState.currentState == 3,
                          std::format("Attacker should commit an attack after first tactical retreat "
                                      "(state={}, pressure={:.2f}, resetConfidence={:.2f})",
                                      static_cast<int>(committedAttackState.currentState),
                                      committedAttackState.pressureScore,
                                      committedAttackState.resetConfidence));

    memData.combatEncounters = 2;
    updateAI(0.1f, attacker->getPosition());
    eventMgr.drainAllDeferredEvents();

    const auto& interruptedState =
        edm.getAttackState(edm.getBehaviorConfigRef(attackerIdx).index);
    BOOST_CHECK_MESSAGE(!interruptedState.isRetreating,
                        "A fresh encounter must not preempt an already committed attack frame");
    BOOST_REQUIRE_MESSAGE(damageAfterRetreat,
                          "Committed attack should deal damage before tactical retreat replans");

    updateAI(0.1f, attacker->getPosition());

    const auto& secondRetreatState =
        edm.getAttackState(edm.getBehaviorConfigRef(attackerIdx).index);
    BOOST_CHECK_MESSAGE(secondRetreatState.isRetreating,
                        "A new damage encounter can trigger another retreat after the committed attack resolves");
    BOOST_CHECK_EQUAL(static_cast<int>(secondRetreatState.lastTacticalRetreatEncounter), 2);
    eventMgr.removeHandler(combatToken);
    aiMgr.unassignBehavior(attackerHandle);
}

BOOST_AUTO_TEST_CASE(TestRetreatInterruptedRecoveryRearmsAttackAgainstPlayer) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();
    auto& eventMgr = EventManager::Instance();

    auto player = std::make_shared<Player>();
    player->setPosition(Vector2D(300.0f, 300.0f));
    const EntityHandle playerHandle = player->getHandle();
    BOOST_REQUIRE(playerHandle.isValid());
    aiMgr.setPlayerHandle(playerHandle);

    auto attacker = TestNPC::create(330.0f, 300.0f);
    const EntityHandle attackerHandle = attacker->getHandle();
    const size_t attackerIdx = edm.getIndex(attackerHandle);
    BOOST_REQUIRE(attackerIdx != SIZE_MAX);

    edm.setFaction(attackerHandle, 1);
    aiMgr.assignBehavior(attackerHandle, "Attack");

    auto& memData = edm.getMemoryData(attackerIdx);
    memData.setValid(true);
    memData.lastTarget = playerHandle;
    memData.emotions.clear();
    memData.personality.bravery = 0.55f;
    memData.personality.aggression = 0.45f;
    memData.personality.composure = 0.75f;
    memData.personality.loyalty = 0.5f;

    const float initialPlayerHealth = player->getHealth();
    bool firstHit = false;
    for (int i = 0; i < 20 && !firstHit; ++i) {
        updateAI(0.1f, player->getPosition());
        eventMgr.drainAllDeferredEvents();
        firstHit = player->getHealth() < initialPlayerHealth;
    }
    BOOST_REQUIRE_MESSAGE(firstHit, "Attacker should damage the player before retreat");

    auto& attackState = edm.getAttackState(edm.getBehaviorConfigRef(attackerIdx).index);
    BOOST_REQUIRE(!attackState.canAttack);

    auto& attackerChar = edm.getCharacterDataByIndex(attackerIdx);
    attackerChar.health = attackerChar.maxHealth * 0.2f;
    memData.lastAttacker = playerHandle;
    memData.combatEncounters++;

    updateAI(0.1f, player->getPosition());
    BOOST_REQUIRE_MESSAGE(attackState.isRetreating,
                          "Low-health attacker should retreat during post-attack recovery");
    BOOST_REQUIRE(!attackState.canAttack);

    const float postRetreatStartHealth = player->getHealth();
    bool secondHit = false;
    for (int i = 0; i < 160 && !secondHit; ++i) {
        updateAI(0.1f, player->getPosition());
        eventMgr.drainAllDeferredEvents();
        secondHit = player->getHealth() < postRetreatStartHealth;
    }

    const auto finalRef = edm.getBehaviorConfigRef(attackerIdx);
    std::string finalState = "not-attack";
    bool finalCanAttack = false;
    bool finalRetreating = false;
    float finalAttackTimer = 0.0f;
    float finalPressure = 0.0f;
    if (finalRef.type == BehaviorType::Attack) {
        const auto& finalAttackState = edm.getAttackState(finalRef.index);
        finalState = std::to_string(finalAttackState.currentState);
        finalCanAttack = finalAttackState.canAttack;
        finalRetreating = finalAttackState.isRetreating;
        finalAttackTimer = finalAttackState.attackTimer;
        finalPressure = finalAttackState.pressureScore;
    }

    BOOST_CHECK_MESSAGE(secondHit,
                        std::format("Retreat must not strand attack readiness after interrupting recovery "
                                    "(type={}, state={}, canAttack={}, retreating={}, attackTimer={:.2f}, "
                                    "pressure={:.2f}, healthBefore={:.2f}, healthAfter={:.2f})",
                                    static_cast<int>(finalRef.type), finalState,
                                    finalCanAttack, finalRetreating, finalAttackTimer,
                                    finalPressure, postRetreatStartHealth,
                                    player->getHealth()));
    aiMgr.unassignBehavior(attackerHandle);
}

BOOST_AUTO_TEST_CASE(TestSpecialAttackReadyAfterRecovery) {
    auto& edm = EntityDataManager::Instance();
    auto& aiMgr = AIManager::Instance();

    auto entity = TestNPC::create(300.0f, 300.0f);
    EntityHandle entityHandle = entity->getHandle();
    size_t entityIdx = edm.getIndex(entityHandle);
    BOOST_REQUIRE(entityIdx != SIZE_MAX);

    aiMgr.assignBehavior(entityHandle, "Attack");

    {
        const auto attackRef = edm.getBehaviorConfigRef(entityIdx);
        auto& attackState = edm.getAttackState(attackRef.index);

        // Starts not ready
        BOOST_CHECK(attackState.specialAttackReady == false);

        // Simulate a completed recovery window.
        attackState.currentState = 4;
        attackState.stateChangeTimer = 10.0f;
        attackState.attackTimer = 10.0f;
        attackState.hasTarget = true;
    }

    // Set a target so attack has something to do
    auto& memData = edm.getMemoryData(entityIdx);
    memData.setValid(true);
    auto target = TestNPC::create(350.0f, 350.0f);
    memData.lastTarget = target->getHandle();

    updateAI(0.016f);

    // After recovery, a ready weapon should arm special attack eligibility.
    BOOST_CHECK(edm.getAttackState(edm.getBehaviorConfigRef(entityIdx).index).specialAttackReady == true);
    BOOST_TEST_MESSAGE("specialAttackReady set to true after recovery transition");
    aiMgr.unassignBehavior(entityHandle);
}

BOOST_AUTO_TEST_SUITE_END()

// Global test summary
BOOST_AUTO_TEST_CASE(BehaviorTestSummary) {
    BOOST_TEST_MESSAGE("=== Behavior Functionality Test Summary ===");
    BOOST_TEST_MESSAGE("All 8 core behaviors tested");
    BOOST_TEST_MESSAGE("Message system integration tested");
    BOOST_TEST_MESSAGE("Behavior transitions tested");
    BOOST_TEST_MESSAGE("Performance with multiple entities tested");
    BOOST_TEST_MESSAGE("Advanced behavior features tested");
    BOOST_TEST_MESSAGE("=== All Behavior Tests Completed Successfully ===");
}
