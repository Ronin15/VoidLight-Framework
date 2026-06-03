/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file KnockbackSidecarTest.cpp
 * @brief Integration test for the knockback SparseSidecar migration.
 *
 * Scenario: spawn 100 NPCs with active knockback, destroy every 3rd via the
 * normal entity-destruction path, simulate 8 frames of decay, assert remaining
 * 67 decayed correctly and EDM::knockbackActiveCount() == 67.
 *
 * Key correctness checks:
 * - freeSlot() correctly calls m_knockback.removeAllFor() for destroyed entities.
 * - Swap-pop inside removeAllFor() leaves no stale sparse entries.
 * - Surviving entities retain correct impulse/framesRemaining values.
 * - knockbackActiveCount() == 67 after destruction.
 * - After 8 frames of decay each surviving entity's framesRemaining decrements
 *   correctly (no cross-contamination from destroyed entries).
 */

#define BOOST_TEST_MODULE KnockbackSidecarTests
#include <boost/test/unit_test.hpp>

#include "ai/BehaviorExecutors.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "entities/EntityHandle.hpp"
#include "events/EntityEvents.hpp"
#include "utils/Vector2D.hpp"
#include <cstdint>
#include <memory>
#include <vector>

// ============================================================================
// Global fixture — ThreadSystem lives for the entire module
// ============================================================================

struct KBThreadSystemFixture
{
    KBThreadSystemFixture()
    {
        if (!VoidLight::ThreadSystem::Instance().init())
        {
            throw std::runtime_error("ThreadSystem::init() failed in KnockbackSidecarTest");
        }
    }
    ~KBThreadSystemFixture()
    {
        VoidLight::ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(KBThreadSystemFixture);

// ============================================================================
// Per-test fixture — managers reset between tests
// ============================================================================

struct KBFixture
{
    KBFixture()
    {
        BOOST_REQUIRE(EntityDataManager::Instance().init());
        EventManager::Instance().init();
        CollisionManager::Instance().init();
        PathfinderManager::Instance().init();
        AIManager::Instance().init();
    }

    ~KBFixture()
    {
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        EventManager::Instance().clean();
        EntityDataManager::Instance().clean();
    }
};

// ============================================================================
// TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(KnockbackSidecarIntegration, KBFixture)

/**
 * Core destroy-during-knockback scenario.
 *
 * - Spawn 100 NPCs.
 * - Apply knockback (framesRemaining = 20 so they survive 8 decay frames).
 * - Destroy every 3rd NPC (indices 2, 5, 8, … → 33 destroyed, 67 surviving).
 * - Assert knockbackActiveCount() == 67.
 * - Verify no stale entries: each surviving entity's knockback is present and
 *   the destroyed entities' knockback is absent.
 */
BOOST_AUTO_TEST_CASE(DestroyDuringKnockbackCleansUpSidecar)
{
    auto& edm = EntityDataManager::Instance();

    constexpr int TOTAL_NPCS = 100;
    // framesRemaining > 8 so entities survive the decay loop below
    constexpr uint8_t INITIAL_FRAMES = 20;
    constexpr float IMPULSE_X = 50.0f;
    constexpr float IMPULSE_Y = 25.0f;

    std::vector<EntityHandle> handles;
    handles.reserve(TOTAL_NPCS);

    // Spawn NPCs
    for (int i = 0; i < TOTAL_NPCS; ++i)
    {
        EntityHandle h = edm.createNPCWithRaceClass(
            Vector2D(static_cast<float>(i * 20), 100.0f), "Human", "Guard");
        BOOST_REQUIRE(h.isValid());
        handles.push_back(h);
    }

    BOOST_CHECK_EQUAL(edm.getEntityCount(), TOTAL_NPCS);

    // Apply knockback to all NPCs
    for (const auto& h : handles)
    {
        const size_t idx = edm.getIndex(h);
        BOOST_REQUIRE(idx != SIZE_MAX);

        auto& kb = edm.applyKnockback(idx);
        kb.impulseX = IMPULSE_X;
        kb.impulseY = IMPULSE_Y;
        kb.framesRemaining = INITIAL_FRAMES;
    }

    BOOST_CHECK_EQUAL(edm.knockbackActiveCount(), static_cast<size_t>(TOTAL_NPCS));

    // Destroy every 3rd NPC (0-indexed: indices 2, 5, 8, ...) — 33 destroyed, 67 surviving
    std::vector<EntityHandle> surviving;
    std::vector<EntityHandle> destroyed;
    surviving.reserve(67);
    destroyed.reserve(33);

    for (int i = 0; i < TOTAL_NPCS; ++i)
    {
        if ((i + 1) % 3 == 0)   // 3rd, 6th, 9th, ... (1-based)
        {
            destroyed.push_back(handles[i]);
            edm.destroyEntity(handles[i]);
        }
        else
        {
            surviving.push_back(handles[i]);
        }
    }

    // processDestructionQueue triggers freeSlot → m_knockback.removeAllFor()
    edm.processDestructionQueue();

    BOOST_CHECK_EQUAL(destroyed.size(), 33u);
    BOOST_CHECK_EQUAL(surviving.size(), 67u);

    // Core assertion: knockbackActiveCount must match surviving count
    BOOST_CHECK_EQUAL(edm.knockbackActiveCount(), 67u);

    // Surviving entities must still have knockback with correct values
    for (const auto& h : surviving)
    {
        const size_t idx = edm.getIndex(h);
        BOOST_REQUIRE(idx != SIZE_MAX);
        BOOST_CHECK(edm.hasKnockback(idx));

        const KnockbackData* kb = edm.getKnockback(idx);
        BOOST_REQUIRE(kb != nullptr);
        BOOST_CHECK_EQUAL(kb->framesRemaining, INITIAL_FRAMES);
        BOOST_CHECK_CLOSE(kb->impulseX, IMPULSE_X, 0.001f);
        BOOST_CHECK_CLOSE(kb->impulseY, IMPULSE_Y, 0.001f);
    }
}

/**
 * Simulate 8 frames of knockback decay on surviving entities.
 * Uses Knockback::DECAY directly (mirrors what AIManager does).
 * After 8 frames: framesRemaining = INITIAL_FRAMES - 8, impulses decayed by DECAY^8.
 */
BOOST_AUTO_TEST_CASE(SurvivingEntitiesDecayCorrectlyAfterPartialDestruction)
{
    auto& edm = EntityDataManager::Instance();

    constexpr int TOTAL_NPCS = 100;
    constexpr uint8_t INITIAL_FRAMES = 20;   // > 8 so none expire during the loop
    constexpr float IMPULSE_X = 100.0f;
    constexpr float IMPULSE_Y =  50.0f;
    constexpr int DECAY_FRAMES = 8;

    std::vector<EntityHandle> handles;
    handles.reserve(TOTAL_NPCS);

    for (int i = 0; i < TOTAL_NPCS; ++i)
    {
        EntityHandle h = edm.createNPCWithRaceClass(
            Vector2D(static_cast<float>(i * 20), 200.0f), "Human", "Guard");
        BOOST_REQUIRE(h.isValid());
        handles.push_back(h);
    }

    // Apply knockback to all
    for (const auto& h : handles)
    {
        const size_t idx = edm.getIndex(h);
        auto& kb = edm.applyKnockback(idx);
        kb.impulseX = IMPULSE_X;
        kb.impulseY = IMPULSE_Y;
        kb.framesRemaining = INITIAL_FRAMES;
    }

    // Destroy every 3rd
    for (int i = 2; i < TOTAL_NPCS; i += 3)
    {
        edm.destroyEntity(handles[i]);
    }
    edm.processDestructionQueue();

    BOOST_CHECK_EQUAL(edm.knockbackActiveCount(), 67u);

    // Compute expected impulse after DECAY_FRAMES applications of Knockback::DECAY
    float expectedX = IMPULSE_X;
    float expectedY = IMPULSE_Y;
    for (int f = 0; f < DECAY_FRAMES; ++f)
    {
        expectedX *= Knockback::DECAY;
        expectedY *= Knockback::DECAY;
    }
    constexpr uint8_t EXPECTED_FRAMES = INITIAL_FRAMES - static_cast<uint8_t>(DECAY_FRAMES);

    // Manually drive 8 decay frames on surviving entities (mirrors AIManager batch logic)
    auto& sidecar = edm.knockbackSidecar();
    for (int frame = 0; frame < DECAY_FRAMES; ++frame)
    {
        auto dense  = sidecar.dense();
        for (size_t i = 0; i < dense.size(); ++i)
        {
            auto& kb = dense[i];
            kb.impulseX *= Knockback::DECAY;
            kb.impulseY *= Knockback::DECAY;
            --kb.framesRemaining;
            // framesRemaining > 0 guaranteed by INITIAL_FRAMES=20, DECAY_FRAMES=8
        }
    }

    // Verify all 67 surviving entries decayed correctly
    BOOST_CHECK_EQUAL(edm.knockbackActiveCount(), 67u);

    for (int i = 0; i < TOTAL_NPCS; ++i)
    {
        if ((i + 1) % 3 == 0) { continue; }   // destroyed

        const size_t idx = edm.getIndex(handles[i]);
        BOOST_REQUIRE(idx != SIZE_MAX);

        const KnockbackData* kb = edm.getKnockback(idx);
        BOOST_REQUIRE(kb != nullptr);

        BOOST_CHECK_EQUAL(kb->framesRemaining, EXPECTED_FRAMES);
        BOOST_CHECK_CLOSE(kb->impulseX, expectedX, 0.01f);
        BOOST_CHECK_CLOSE(kb->impulseY, expectedY, 0.01f);
    }
}

/**
 * Verify sidecar owners table is internally consistent after partial destruction.
 * For every entry in dense(), owners()[i] must point to an edmIdx whose
 * sidecar.get() returns exactly &dense[i].
 */
BOOST_AUTO_TEST_CASE(SidecarOwnersTableConsistentAfterPartialDestruction)
{
    auto& edm = EntityDataManager::Instance();

    constexpr int TOTAL_NPCS = 50;

    std::vector<EntityHandle> handles;
    handles.reserve(TOTAL_NPCS);

    for (int i = 0; i < TOTAL_NPCS; ++i)
    {
        EntityHandle h = edm.createNPCWithRaceClass(
            Vector2D(static_cast<float>(i * 10), 300.0f), "Human", "Guard");
        BOOST_REQUIRE(h.isValid());
        handles.push_back(h);
    }

    // Apply knockback to all
    for (const auto& h : handles)
    {
        const size_t idx = edm.getIndex(h);
        auto& kb = edm.applyKnockback(idx);
        kb.impulseX = 10.0f;
        kb.impulseY = 5.0f;
        kb.framesRemaining = 15;
    }

    // Destroy every other entity (indices 1, 3, 5, ...)
    for (int i = 1; i < TOTAL_NPCS; i += 2)
    {
        edm.destroyEntity(handles[i]);
    }
    edm.processDestructionQueue();

    const size_t expectedActive = static_cast<size_t>(TOTAL_NPCS) / 2;
    BOOST_CHECK_EQUAL(edm.knockbackActiveCount(), expectedActive);

    // Cross-check owners/dense consistency
    const auto& sidecar = edm.knockbackSidecar();
    auto owners = sidecar.owners();
    auto dense  = sidecar.dense();

    BOOST_REQUIRE_EQUAL(owners.size(), dense.size());
    BOOST_REQUIRE_EQUAL(owners.size(), expectedActive);

    for (size_t i = 0; i < owners.size(); ++i)
    {
        uint32_t edmIdx = owners[i];
        const KnockbackData* ptr = edm.getKnockback(static_cast<size_t>(edmIdx));
        BOOST_CHECK(ptr != nullptr);
        BOOST_CHECK_EQUAL(ptr, &dense[i]);
    }
}

/**
 * Clearing knockback (framesRemaining hits 0) removes the entry from the sidecar.
 */
BOOST_AUTO_TEST_CASE(ClearKnockbackRemovesEntry)
{
    auto& edm = EntityDataManager::Instance();

    EntityHandle h = edm.createNPCWithRaceClass(Vector2D(50.0f, 50.0f), "Human", "Guard");
    BOOST_REQUIRE(h.isValid());

    const size_t idx = edm.getIndex(h);
    auto& kb = edm.applyKnockback(idx);
    kb.impulseX = 1.0f;
    kb.impulseY = 1.0f;
    kb.framesRemaining = 1;

    BOOST_CHECK_EQUAL(edm.knockbackActiveCount(), 1u);

    // Simulate one frame decay that expires the knockback
    KnockbackData* p = edm.getKnockback(idx);
    BOOST_REQUIRE(p != nullptr);
    p->impulseX *= Knockback::DECAY;
    p->impulseY *= Knockback::DECAY;
    if (--p->framesRemaining == 0)
    {
        edm.clearKnockback(idx);
    }

    BOOST_CHECK_EQUAL(edm.knockbackActiveCount(), 0u);
    BOOST_CHECK(!edm.hasKnockback(idx));
    BOOST_CHECK(edm.getKnockback(idx) == nullptr);
}

BOOST_AUTO_TEST_CASE(DamageEventsAccumulateKnockbackAndRefreshFrames)
{
    auto& edm = EntityDataManager::Instance();

    EntityHandle playerHandle = edm.registerPlayer(9901, Vector2D(75.0f, 75.0f));
    BOOST_REQUIRE(playerHandle.isValid());

    auto& playerData = edm.getCharacterData(playerHandle);
    playerData.maxHealth = 100.0f;
    playerData.health = 100.0f;
    playerData.mass = 2.0f;

    const size_t idx = edm.getIndex(playerHandle);
    BOOST_REQUIRE(idx != SIZE_MAX);

    auto firstHit = std::make_shared<DamageEvent>(
        EntityEventType::DamageIntent,
        EntityHandle{},
        playerHandle,
        1.0f,
        Vector2D(10.0f, -4.0f));

    BOOST_CHECK(EventManager::Instance().dispatchEvent(
        firstHit, EventManager::DispatchMode::Immediate));

    KnockbackData* kb = edm.getKnockback(idx);
    BOOST_REQUIRE(kb != nullptr);
    BOOST_CHECK_CLOSE(kb->impulseX, 5.0f, 0.001f);
    BOOST_CHECK_CLOSE(kb->impulseY, -2.0f, 0.001f);
    BOOST_CHECK_EQUAL(kb->framesRemaining, static_cast<uint8_t>(Knockback::FRAMES));

    kb->framesRemaining = 1;

    auto secondHit = std::make_shared<DamageEvent>(
        EntityEventType::DamageIntent,
        EntityHandle{},
        playerHandle,
        1.0f,
        Vector2D(6.0f, 8.0f));

    BOOST_CHECK(EventManager::Instance().dispatchEvent(
        secondHit, EventManager::DispatchMode::Immediate));

    kb = edm.getKnockback(idx);
    BOOST_REQUIRE(kb != nullptr);
    BOOST_CHECK_CLOSE(kb->impulseX, 8.0f, 0.001f);
    BOOST_CHECK_CLOSE(kb->impulseY, 2.0f, 0.001f);
    BOOST_CHECK_EQUAL(kb->framesRemaining, static_cast<uint8_t>(Knockback::FRAMES));
}

BOOST_AUTO_TEST_SUITE_END()
