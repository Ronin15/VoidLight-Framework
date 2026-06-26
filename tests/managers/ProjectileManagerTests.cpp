/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file ProjectileManagerTests.cpp
 * @brief Tests for ProjectileManager integration with EntityDataManager
 *
 * Covers:
 * - Projectile creation and EDM field verification
 * - Position integration (movement, boundary, lifetime)
 * - Direct projectile collision-to-damage wiring
 * - State transition cleanup
 * - Global pause and perf stats
 */

#define BOOST_TEST_MODULE ProjectileManagerTests
#include <boost/test/unit_test.hpp>

#include "collisions/CollisionBody.hpp"
#include "collisions/CollisionInfo.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "events/EntityEvents.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ProjectileManager.hpp"
#include <atomic>
#include <memory>
#include <vector>

namespace {

struct ThreadSystemTestLifetime {
    ThreadSystemTestLifetime() {
        VOIDLIGHT_ENABLE_BENCHMARK_MODE();
        BOOST_REQUIRE_MESSAGE(VoidLight::ThreadSystem::Instance().init(),
                              "Failed to initialize ThreadSystem");
    }
    ~ThreadSystemTestLifetime() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

ThreadSystemTestLifetime g_threadSystemTestLifetime{};

} // namespace

struct ProjectileTestFixture {
    ProjectileTestFixture() {
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(PathfinderManager::Instance().init());
        BOOST_REQUIRE(CollisionManager::Instance().init());
        BOOST_REQUIRE(EventManager::Instance().init());
        BOOST_REQUIRE(ProjectileManager::Instance().init());
    }

    ~ProjectileTestFixture() {
        ProjectileManager::Instance().clean();
        EventManager::Instance().clean();
        CollisionManager::Instance().clean();
        PathfinderManager::Instance().clean();
        EntityDataManager::Instance().clean();
    }

    void prepareForTest() {
        ProjectileManager::Instance().prepareForStateTransition();
        EventManager::Instance().prepareForStateTransition();
        CollisionManager::Instance().prepareForStateTransition();
        EntityDataManager::Instance().prepareForStateTransition();
    }

    // Helper: process EDM destruction queue (simulates end-of-frame in GameEngine)
    void processDestructions() {
        EntityDataManager::Instance().processDestructionQueue();
    }

    void handleProjectileCollision(const VoidLight::CollisionInfo& info) {
        ProjectileManager::Instance().handleProjectileCollision(info);
    }

    // Helper: create a player entity to serve as projectile owner
    EntityHandle createPlayerOwner(const Vector2D& pos = Vector2D(500.0f, 500.0f)) {
        auto& edm = EntityDataManager::Instance();
        return edm.registerPlayer(1, pos);
    }

    EntityHandle createFactionedNPC(const Vector2D& pos, uint8_t faction) {
        auto& edm = EntityDataManager::Instance();
        return edm.createNPCWithRaceClass(pos, "Human", "Guard", Sex::Unknown, faction);
    }

    // Helper: create an NPC target
    EntityHandle createNPCTarget(const Vector2D& pos = Vector2D(600.0f, 500.0f)) {
        return createFactionedNPC(pos, 0);
    }

    EntityHandle createNeutralTarget(const Vector2D& pos = Vector2D(700.0f, 500.0f)) {
        return createFactionedNPC(pos, 2);
    }
};


// ============================================================================
// SUITE: Lifecycle Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(LifecycleTests, ProjectileTestFixture)

BOOST_AUTO_TEST_CASE(ProjectileCreation)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();

    EntityHandle owner = createPlayerOwner();
    Vector2D pos(100.0f, 200.0f);
    Vector2D vel(150.0f, 0.0f);
    float damage = 25.0f;
    float lifetime = 3.0f;

    EntityHandle proj = edm.createProjectile(pos, vel, owner, damage, lifetime);

    // Handle validity
    BOOST_REQUIRE(proj.isValid());
    BOOST_CHECK(proj.isProjectile());
    BOOST_CHECK_EQUAL(static_cast<int>(proj.kind), static_cast<int>(EntityKind::Projectile));

    // Hot data
    size_t idx = edm.getIndex(proj);
    BOOST_REQUIRE_NE(idx, SIZE_MAX);
    const auto& hot = edm.getHotDataByIndex(idx);
    BOOST_CHECK(hot.isAlive());
    BOOST_CHECK_EQUAL(static_cast<int>(hot.kind), static_cast<int>(EntityKind::Projectile));
    BOOST_CHECK_EQUAL(static_cast<int>(hot.tier), static_cast<int>(SimulationTier::Active));
    BOOST_CHECK_CLOSE(hot.transform.position.getX(), 100.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.position.getY(), 200.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.velocity.getX(), 150.0f, 0.01f);
    BOOST_CHECK_CLOSE(hot.transform.velocity.getY(), 0.0f, 0.01f);

    // Collision layers
    BOOST_CHECK(hot.collisionLayers & VoidLight::CollisionLayer::Layer_Projectile);

    // ProjectileData
    const auto& projData = edm.getProjectileData(proj);
    BOOST_CHECK(projData.owner == owner);
    BOOST_CHECK_CLOSE(projData.damage, 25.0f, 0.01f);
    BOOST_CHECK_CLOSE(projData.lifetime, 3.0f, 0.01f);
    BOOST_CHECK_CLOSE(projData.speed, 150.0f, 0.5f);
    BOOST_CHECK_EQUAL(projData.flags, 0);
}

BOOST_AUTO_TEST_CASE(OwnerCollisionMask)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();

    // Projectile masks are uniform; owner immunity is enforced by ProjectileManager.
    EntityHandle player = createPlayerOwner();
    EntityHandle playerProj = edm.createProjectile(
        Vector2D(100.0f, 100.0f), Vector2D(100.0f, 0.0f), player, 10.0f);
    size_t playerProjIdx = edm.getIndex(playerProj);
    BOOST_REQUIRE_NE(playerProjIdx, SIZE_MAX);
    const auto& playerProjHot = edm.getHotDataByIndex(playerProjIdx);
    BOOST_CHECK(playerProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Player);
    BOOST_CHECK(playerProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Enemy);
    BOOST_CHECK(playerProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Default);
    BOOST_CHECK(playerProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Environment);
    BOOST_CHECK(!(playerProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Projectile));

    // NPC-owned projectiles use the same collision mask.
    EntityHandle npc = createNPCTarget(Vector2D(200.0f, 200.0f));
    EntityHandle npcProj = edm.createProjectile(
        Vector2D(200.0f, 200.0f), Vector2D(100.0f, 0.0f), npc, 10.0f);
    size_t npcProjIdx = edm.getIndex(npcProj);
    BOOST_REQUIRE_NE(npcProjIdx, SIZE_MAX);
    const auto& npcProjHot = edm.getHotDataByIndex(npcProjIdx);
    BOOST_CHECK(npcProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Player);
    BOOST_CHECK(npcProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Enemy);
    BOOST_CHECK(npcProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Default);
    BOOST_CHECK(npcProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Environment);
    BOOST_CHECK(!(npcProjHot.collisionMask & VoidLight::CollisionLayer::Layer_Projectile));
}

BOOST_AUTO_TEST_CASE(StateTransitionCleanup)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    EntityHandle owner = createPlayerOwner();

    // Create multiple projectiles
    for (int i = 0; i < 10; ++i) {
        edm.createProjectile(
            Vector2D(100.0f + i * 10.0f, 100.0f),
            Vector2D(100.0f, 0.0f), owner, 10.0f, 5.0f);
    }
    BOOST_CHECK_EQUAL(edm.getEntityCount(EntityKind::Projectile), 10u);

    ProjectileManager::Instance().prepareForStateTransition();
    processDestructions();

    // All projectiles should be destroyed
    BOOST_CHECK_EQUAL(edm.getIndicesByKind(EntityKind::Projectile).size(), 0u);
}

BOOST_AUTO_TEST_SUITE_END()


// ============================================================================
// SUITE: Movement Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(MovementTests, ProjectileTestFixture)

BOOST_AUTO_TEST_CASE(ProjectileMovement)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    EntityHandle owner = createPlayerOwner();

    Vector2D startPos(500.0f, 500.0f);
    Vector2D velocity(200.0f, 0.0f);
    EntityHandle proj = edm.createProjectile(startPos, velocity, owner, 10.0f, 10.0f);

    // Update for 0.1 seconds — should move ~20 pixels in X
    ProjectileManager::Instance().update(0.1f);

    size_t idx = edm.getIndex(proj);
    BOOST_REQUIRE_NE(idx, SIZE_MAX);
    const auto& transform = edm.getHotDataByIndex(idx).transform;
    float expectedX = 500.0f + 200.0f * 0.1f;  // 520
    BOOST_CHECK_CLOSE(transform.position.getX(), expectedX, 1.0f);
    BOOST_CHECK_CLOSE(transform.position.getY(), 500.0f, 1.0f);
}

BOOST_AUTO_TEST_CASE(LifetimeExpiry)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    EntityHandle owner = createPlayerOwner();

    EntityHandle proj = edm.createProjectile(
        Vector2D(500.0f, 500.0f), Vector2D(10.0f, 0.0f), owner, 10.0f, 0.05f);
    BOOST_REQUIRE(edm.isValidHandle(proj));

    // Update with dt that exceeds lifetime
    ProjectileManager::Instance().update(0.1f);
    processDestructions();

    BOOST_CHECK(!edm.isValidHandle(proj));
}

BOOST_AUTO_TEST_CASE(BoundaryDestruction)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    EntityHandle owner = createPlayerOwner();

    // Place projectile near the world edge moving fast toward it
    // Default world bounds fallback is 32000x32000
    EntityHandle proj = edm.createProjectile(
        Vector2D(31995.0f, 500.0f), Vector2D(5000.0f, 0.0f), owner, 10.0f, 10.0f);
    BOOST_REQUIRE(edm.isValidHandle(proj));

    // One update should push past boundary, embedding the projectile instead of destroying it.
    ProjectileManager::Instance().update(0.1f);

    BOOST_REQUIRE(edm.isValidHandle(proj));
    const auto& projectile = edm.getProjectileData(proj);
    const auto& hot = edm.getHotDataByIndex(edm.getIndex(proj));
    BOOST_CHECK(projectile.isEmbedded());
    BOOST_CHECK(!hot.hasCollision());
    BOOST_CHECK_SMALL(hot.transform.velocity.lengthSquared(), 0.001f);
}

BOOST_AUTO_TEST_CASE(GlobalPause)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    EntityHandle owner = createPlayerOwner();

    Vector2D startPos(500.0f, 500.0f);
    EntityHandle proj = edm.createProjectile(
        startPos, Vector2D(200.0f, 0.0f), owner, 10.0f, 10.0f);

    // Pause and update — position should NOT change
    ProjectileManager::Instance().setGlobalPause(true);
    ProjectileManager::Instance().update(1.0f);

    size_t idx = edm.getIndex(proj);
    BOOST_REQUIRE_NE(idx, SIZE_MAX);
    BOOST_CHECK_CLOSE(edm.getHotDataByIndex(idx).transform.position.getX(), 500.0f, 0.01f);

    // Unpause and update — position should change
    ProjectileManager::Instance().setGlobalPause(false);
    ProjectileManager::Instance().update(0.1f);

    idx = edm.getIndex(proj);
    BOOST_REQUIRE_NE(idx, SIZE_MAX);
    BOOST_CHECK_GT(edm.getHotDataByIndex(idx).transform.position.getX(), 510.0f);
}

BOOST_AUTO_TEST_SUITE_END()


// ============================================================================
// SUITE: Collision Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(CollisionTests, ProjectileTestFixture)

BOOST_AUTO_TEST_CASE(CollisionDamage)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle target = createNPCTarget(Vector2D(200.0f, 100.0f));

    float projSpeed = 200.0f;
    float projDamage = 30.0f;
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(projSpeed, 0.0f), owner, projDamage, 5.0f);

    size_t projIdx = edm.getIndex(proj);
    size_t targetIdx = edm.getIndex(target);
    BOOST_REQUIRE_NE(projIdx, SIZE_MAX);
    BOOST_REQUIRE_NE(targetIdx, SIZE_MAX);

    // Register a handler to capture the resulting DamageEvent
    std::atomic<bool> damageReceived{false};
    float receivedDamage = 0.0f;
    float receivedKnockbackX = 0.0f;
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            auto dmgEvent = std::dynamic_pointer_cast<DamageEvent>(data.event);
            if (dmgEvent) {
                receivedDamage = dmgEvent->getDamage();
                receivedKnockbackX = dmgEvent->getKnockback().getX();
                damageReceived.store(true, std::memory_order_release);
            }
        });

    // Dispatch a direct projectile collision between projectile and target.
    VoidLight::CollisionInfo info{};
    info.indexA = projIdx;
    info.indexB = targetIdx;
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = true;

    handleProjectileCollision(info);
    processDestructions();
    eventMgr.update();

    BOOST_CHECK(damageReceived.load(std::memory_order_acquire));
    BOOST_CHECK_CLOSE(receivedDamage, projDamage, 0.01f);

    // Knockback should be speed-proportional: base(30) + speed(200) * factor(0.1) = 50
    float expectedKnockback = 30.0f + projSpeed * 0.1f;
    BOOST_CHECK_CLOSE(receivedKnockbackX, expectedKnockback, 1.0f);

    // Non-piercing projectile should embed on impact, then fade out later.
    BOOST_REQUIRE(edm.isValidHandle(proj));
    const auto& embeddedProjectile = edm.getProjectileData(proj);
    BOOST_CHECK(embeddedProjectile.isEmbedded());
    BOOST_CHECK(embeddedProjectile.embeddedTarget == target);
    BOOST_CHECK_CLOSE(embeddedProjectile.embeddedOffsetX, -10.0f, 0.01f);
    BOOST_CHECK_CLOSE(embeddedProjectile.embeddedOffsetY, 0.0f, 0.01f);
    BOOST_CHECK_SMALL(edm.getHotDataByIndex(projIdx).transform.velocity.lengthSquared(), 0.001f);

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(CollisionDamageAfterVelocityCancellation)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle target = createNPCTarget(Vector2D(200.0f, 100.0f));

    constexpr float projSpeed = 220.0f;
    constexpr float projDamage = 18.0f;
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(projSpeed, 0.0f), owner, projDamage, 5.0f);

    const size_t projIdx = edm.getIndex(proj);
    const size_t targetIdx = edm.getIndex(target);
    BOOST_REQUIRE_NE(projIdx, SIZE_MAX);
    BOOST_REQUIRE_NE(targetIdx, SIZE_MAX);

    std::atomic<bool> damageReceived{false};
    float receivedDamage = 0.0f;
    float receivedKnockbackX = 0.0f;
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            auto dmgEvent = std::dynamic_pointer_cast<DamageEvent>(data.event);
            if (dmgEvent) {
                receivedDamage = dmgEvent->getDamage();
                receivedKnockbackX = dmgEvent->getKnockback().getX();
                damageReceived.store(true, std::memory_order_release);
            }
        });

    VoidLight::CollisionInfo info{};
    info.indexA = projIdx;
    info.indexB = targetIdx;
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = true;

    // Match the real frame order: CollisionManager::resolve() may already have
    // cancelled the projectile velocity before queued combat is processed.
    edm.getHotDataByIndex(projIdx).transform.velocity = Vector2D(0.0f, 0.0f);

    handleProjectileCollision(info);
    processDestructions();
    eventMgr.update();

    BOOST_CHECK(damageReceived.load(std::memory_order_acquire));
    BOOST_CHECK_CLOSE(receivedDamage, projDamage, 0.01f);

    const float expectedKnockback = 30.0f + projSpeed * 0.1f;
    BOOST_CHECK_CLOSE(receivedKnockbackX, expectedKnockback, 1.0f);

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(CollisionDamageToNeutralTarget)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle target = createNeutralTarget(Vector2D(200.0f, 100.0f));

    constexpr float projDamage = 12.0f;
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(180.0f, 0.0f), owner, projDamage, 5.0f);

    std::atomic<bool> damageReceived{false};
    float receivedDamage = 0.0f;
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            auto dmgEvent = std::dynamic_pointer_cast<DamageEvent>(data.event);
            if (dmgEvent) {
                receivedDamage = dmgEvent->getDamage();
                damageReceived.store(true, std::memory_order_release);
            }
        });

    VoidLight::CollisionInfo info{};
    info.indexA = edm.getIndex(proj);
    info.indexB = edm.getIndex(target);
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = true;

    handleProjectileCollision(info);
    processDestructions();
    eventMgr.update();

    BOOST_CHECK(damageReceived.load(std::memory_order_acquire));
    BOOST_CHECK_CLOSE(receivedDamage, projDamage, 0.01f);

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(PiercingFlag)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle target = createNPCTarget(Vector2D(200.0f, 100.0f));

    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(200.0f, 0.0f), owner, 15.0f, 5.0f);

    // Set piercing flag
    auto& projData = edm.getProjectileData(proj);
    projData.flags |= ProjectileData::FLAG_PIERCING;

    size_t projIdx = edm.getIndex(proj);
    size_t targetIdx = edm.getIndex(target);

    VoidLight::CollisionInfo info{};
    info.indexA = projIdx;
    info.indexB = targetIdx;
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = true;

    handleProjectileCollision(info);

    // Piercing projectile should survive
    BOOST_CHECK(edm.isValidHandle(proj));
}

BOOST_AUTO_TEST_CASE(NonPiercingProjectileOnlyDamagesOnceAcrossDuplicateCollisions)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle targetA = createNPCTarget(Vector2D(200.0f, 100.0f));
    EntityHandle targetB = createNPCTarget(Vector2D(220.0f, 100.0f));
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(200.0f, 0.0f), owner, 15.0f, 5.0f);

    std::atomic<int> combatEvents{0};
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            if (std::dynamic_pointer_cast<DamageEvent>(data.event)) {
                combatEvents.fetch_add(1, std::memory_order_release);
            }
        });

    VoidLight::CollisionInfo firstHit{};
    firstHit.indexA = edm.getIndex(proj);
    firstHit.indexB = edm.getIndex(targetA);
    firstHit.normal = Vector2D(1.0f, 0.0f);
    firstHit.penetration = 1.0f;
    firstHit.isMovableMovable = true;

    VoidLight::CollisionInfo secondHit = firstHit;
    secondHit.indexB = edm.getIndex(targetB);

    handleProjectileCollision(firstHit);
    handleProjectileCollision(secondHit);
    processDestructions();
    eventMgr.update();

    BOOST_CHECK_EQUAL(combatEvents.load(std::memory_order_acquire), 1);
    BOOST_REQUIRE(edm.isValidHandle(proj));
    BOOST_CHECK(edm.getProjectileData(proj).isEmbedded());

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(ProjectileAsSecondCollisionParticipantInvertsKnockback)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle target = createNPCTarget(Vector2D(200.0f, 100.0f));
    EntityHandle proj = edm.createProjectile(
        Vector2D(210.0f, 100.0f), Vector2D(-200.0f, 0.0f), owner, 15.0f, 5.0f);

    float receivedKnockbackX = 0.0f;
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            auto dmgEvent = std::dynamic_pointer_cast<DamageEvent>(data.event);
            if (dmgEvent) {
                receivedKnockbackX = dmgEvent->getKnockback().getX();
            }
        });

    VoidLight::CollisionInfo info{};
    info.indexA = edm.getIndex(target);
    info.indexB = edm.getIndex(proj);
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = true;

    handleProjectileCollision(info);
    processDestructions();
    eventMgr.update();

    BOOST_CHECK_LT(receivedKnockbackX, 0.0f);

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(MovableStaticProjectileHitEmbedsProjectileWithoutCombatDamage)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(200.0f, 0.0f), owner, 15.0f, 5.0f);

    std::atomic<int> combatEvents{0};
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            if (std::dynamic_pointer_cast<DamageEvent>(data.event)) {
                combatEvents.fetch_add(1, std::memory_order_release);
            }
        });

    VoidLight::CollisionInfo info{};
    info.indexA = edm.getIndex(proj);
    info.indexB = 0; // static storage index placeholder
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = false;

    handleProjectileCollision(info);
    eventMgr.update();

    BOOST_CHECK_EQUAL(combatEvents.load(std::memory_order_acquire), 0);
    BOOST_REQUIRE(edm.isValidHandle(proj));
    BOOST_CHECK(edm.getProjectileData(proj).isEmbedded());
    BOOST_CHECK(!edm.getHotDataByIndex(edm.getIndex(proj)).hasCollision());

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(ProjectileIgnoresOwnerCollision)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle proj = edm.createProjectile(
        Vector2D(100.0f, 100.0f), Vector2D(0.0f, 0.0f), owner, 15.0f, 5.0f);

    std::atomic<int> combatEvents{0};
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            if (std::dynamic_pointer_cast<DamageEvent>(data.event)) {
                combatEvents.fetch_add(1, std::memory_order_release);
            }
        });

    const Vector2D ownerStartPos = edm.getHotDataByIndex(edm.getIndex(owner)).transform.position;
    const Vector2D projStartPos = edm.getHotDataByIndex(edm.getIndex(proj)).transform.position;

    CollisionManager::Instance().update(0.0f);
    eventMgr.update();
    processDestructions();
    eventMgr.update();

    BOOST_CHECK_EQUAL(combatEvents.load(std::memory_order_acquire), 0);
    BOOST_REQUIRE(edm.isValidHandle(proj));
    BOOST_CHECK(!edm.getProjectileData(proj).isEmbedded());

    const auto& ownerTransform = edm.getHotDataByIndex(edm.getIndex(owner)).transform;
    const auto& projTransform = edm.getHotDataByIndex(edm.getIndex(proj)).transform;
    BOOST_CHECK_CLOSE(ownerTransform.position.getX(), ownerStartPos.getX(), 0.01f);
    BOOST_CHECK_CLOSE(ownerTransform.position.getY(), ownerStartPos.getY(), 0.01f);
    BOOST_CHECK_CLOSE(projTransform.position.getX(), projStartPos.getX(), 0.01f);
    BOOST_CHECK_CLOSE(projTransform.position.getY(), projStartPos.getY(), 0.01f);
    BOOST_CHECK(edm.getHotDataByIndex(edm.getIndex(proj)).hasCollision());

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(ProjectileCollisionDoesNotDisplaceTargetMovement)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle target = createNPCTarget(Vector2D(190.0f, 100.0f));
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(150.0f, 0.0f), owner, 15.0f, 5.0f);

    std::atomic<int> combatEvents{0};
    auto combatToken = eventMgr.registerHandlerWithToken(
        EventTypeId::Combat,
        [&](const EventData& data) {
            if (std::dynamic_pointer_cast<DamageEvent>(data.event)) {
                combatEvents.fetch_add(1, std::memory_order_release);
            }
        });

    const Vector2D targetStartPos = edm.getHotDataByIndex(edm.getIndex(target)).transform.position;
    const Vector2D projStartPos = edm.getHotDataByIndex(edm.getIndex(proj)).transform.position;

    CollisionManager::Instance().update(0.0f);

    const auto& targetAfterCollision = edm.getHotDataByIndex(edm.getIndex(target)).transform.position;
    const auto& projAfterCollision = edm.getHotDataByIndex(edm.getIndex(proj)).transform.position;
    BOOST_CHECK_CLOSE(targetAfterCollision.getX(), targetStartPos.getX(), 0.01f);
    BOOST_CHECK_CLOSE(targetAfterCollision.getY(), targetStartPos.getY(), 0.01f);
    BOOST_CHECK_CLOSE(projAfterCollision.getX(), projStartPos.getX(), 0.01f);
    BOOST_CHECK_CLOSE(projAfterCollision.getY(), projStartPos.getY(), 0.01f);

    eventMgr.update();
    processDestructions();
    eventMgr.update();

    BOOST_CHECK_EQUAL(combatEvents.load(std::memory_order_acquire), 1);
    BOOST_REQUIRE(edm.isValidHandle(proj));
    BOOST_CHECK(edm.getProjectileData(proj).isEmbedded());

    eventMgr.removeHandler(combatToken);
}

BOOST_AUTO_TEST_CASE(TriggerCollisionDoesNotEmbedProjectile)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(200.0f, 0.0f), owner, 15.0f, 5.0f);

    VoidLight::CollisionInfo info{};
    info.indexA = edm.getIndex(proj);
    info.indexB = 0;
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.trigger = true;
    info.isMovableMovable = false;

    handleProjectileCollision(info);

    BOOST_REQUIRE(edm.isValidHandle(proj));
    BOOST_CHECK(!edm.getProjectileData(proj).isEmbedded());
    BOOST_CHECK(edm.getHotDataByIndex(edm.getIndex(proj)).hasCollision());
}

BOOST_AUTO_TEST_CASE(EmbeddedProjectileExpiresAfterFadeLifetime)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    auto& eventMgr = EventManager::Instance();

    EntityHandle owner = createPlayerOwner(Vector2D(100.0f, 100.0f));
    EntityHandle target = createNPCTarget(Vector2D(200.0f, 100.0f));
    EntityHandle proj = edm.createProjectile(
        Vector2D(190.0f, 100.0f), Vector2D(200.0f, 0.0f), owner, 15.0f, 5.0f);

    VoidLight::CollisionInfo info{};
    info.indexA = edm.getIndex(proj);
    info.indexB = edm.getIndex(target);
    info.normal = Vector2D(1.0f, 0.0f);
    info.penetration = 1.0f;
    info.isMovableMovable = true;

    handleProjectileCollision(info);
    eventMgr.update();

    BOOST_REQUIRE(edm.isValidHandle(proj));
    BOOST_CHECK(edm.getProjectileData(proj).isEmbedded());

    ProjectileManager::Instance().update(ProjectileData::EMBEDDED_LIFETIME_SECONDS + 0.05f);
    processDestructions();

    BOOST_CHECK(!edm.isValidHandle(proj));
}

BOOST_AUTO_TEST_SUITE_END()


// ============================================================================
// SUITE: Performance Stats
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(PerfStatsTests, ProjectileTestFixture)

BOOST_AUTO_TEST_CASE(PerfStatsTracking)
{
    prepareForTest();
    auto& edm = EntityDataManager::Instance();
    EntityHandle owner = createPlayerOwner();

    // Create a batch of projectiles
    for (int i = 0; i < 50; ++i) {
        edm.createProjectile(
            Vector2D(500.0f + i * 10.0f, 500.0f),
            Vector2D(100.0f, 0.0f), owner, 10.0f, 999.0f);
    }

    // Run several updates
    for (int i = 0; i < 5; ++i) {
        ProjectileManager::Instance().update(0.016f);
    }

    const auto& stats = ProjectileManager::Instance().getPerfStats();
    BOOST_CHECK_GT(stats.totalUpdates, 0u);
    BOOST_CHECK_EQUAL(stats.lastEntitiesProcessed, 50u);
    BOOST_CHECK_GT(stats.lastUpdateMs, 0.0);
    BOOST_CHECK_GT(stats.avgUpdateMs, 0.0);
}

BOOST_AUTO_TEST_SUITE_END()
