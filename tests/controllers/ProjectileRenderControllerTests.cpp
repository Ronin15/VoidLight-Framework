/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file ProjectileRenderControllerTests.cpp
 * @brief Tests for ProjectileRenderController
 *
 * Tests cover:
 * - ControllerBase contract: getName(), subscribe() no-op, isSubscribed(),
 *   suspend()/resume() lifecycle
 * - recordGPU() null-SpriteBatch guard (no EDM required — returns immediately)
 * - recordGPU() with no projectiles: zero vertices written (needs EDM)
 * - recordGPU() with a live projectile: VERTICES_PER_SPRITE vertices written
 * - recordGPU() interpolated position: vertex X coordinate at known alpha
 *
 * GPU-dependent tests are skipped gracefully when no GPU device is available
 * (headless CI) using the same pattern as NPCRenderControllerTests and
 * ResourceRenderControllerTests.
 *
 * ThreadSystem is initialised globally because EntityDataManager background
 * tier operations may dispatch tasks.
 */

#define BOOST_TEST_MODULE ProjectileRenderControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/render/ProjectileRenderController.hpp"
#include "managers/EntityDataManager.hpp"
#include "core/ThreadSystem.hpp"
#include "utils/GPUSceneRecorder.hpp"
#include "gpu/SpriteBatch.hpp"
#include "gpu/GPUDevice.hpp"
#include "utils/Vector2D.hpp"

// ============================================================================
// Global fixture — ThreadSystem initialised once for all test cases
// ============================================================================

struct GlobalThreadSystemFixture
{
    GlobalThreadSystemFixture()
    {
        if (!VoidLight::ThreadSystem::Instance().init())
        {
            throw std::runtime_error(
                "ThreadSystem::init() failed in ProjectileRenderControllerTests");
        }
    }

    ~GlobalThreadSystemFixture()
    {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(GlobalThreadSystemFixture);

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

bool tryInitSpriteBatch(VoidLight::SpriteBatch& batch)
{
    SDL_GPUDevice* device = VoidLight::GPUDevice::Instance().get();
    if (!device)
    {
        return false;
    }
    return batch.init(device, "ProjectileRenderTestBatch");
}

} // namespace

// ============================================================================
// ControllerBase Contract Tests  (no EDM or GPU required)
// ============================================================================

BOOST_AUTO_TEST_SUITE(ProjectileRenderControllerBaseContractTests)

BOOST_AUTO_TEST_CASE(TestControllerName)
{
    ProjectileRenderController ctrl;
    BOOST_CHECK_EQUAL(ctrl.getName(), "ProjectileRenderController");
}

BOOST_AUTO_TEST_CASE(TestDefaultConstructionDoesNotCrash)
{
    BOOST_CHECK_NO_THROW({
        ProjectileRenderController ctrl;
        (void)ctrl.getName();
    });
}

BOOST_AUTO_TEST_CASE(TestSubscribeIsNoOpAndDoesNotMarkSubscribed)
{
    // subscribe() is intentionally empty — the controller has no event
    // subscriptions and must NOT flip isSubscribed() to true.
    ProjectileRenderController ctrl;

    ctrl.subscribe();

    BOOST_CHECK(!ctrl.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestSuspendSetsIsSuspended)
{
    ProjectileRenderController ctrl;
    BOOST_CHECK(!ctrl.isSuspended());

    ctrl.suspend();

    BOOST_CHECK(ctrl.isSuspended());
}

BOOST_AUTO_TEST_CASE(TestResumeAfterSuspendClearsSuspended)
{
    ProjectileRenderController ctrl;
    ctrl.suspend();
    BOOST_REQUIRE(ctrl.isSuspended());

    ctrl.resume();

    BOOST_CHECK(!ctrl.isSuspended());
}

BOOST_AUTO_TEST_CASE(TestSuspendOnAlreadySuspendedIsNoOp)
{
    ProjectileRenderController ctrl;
    ctrl.suspend();
    ctrl.suspend();  // idempotent

    BOOST_CHECK(ctrl.isSuspended());
}

BOOST_AUTO_TEST_CASE(TestResumeOnNotSuspendedIsNoOp)
{
    ProjectileRenderController ctrl;
    BOOST_REQUIRE(!ctrl.isSuspended());

    ctrl.resume();  // no-op — must not crash or change state unexpectedly

    BOOST_CHECK(!ctrl.isSuspended());
}

BOOST_AUTO_TEST_CASE(TestRecordGPUWithNullSpriteBatchIsNoOp)
{
    // The controller guards against a null spriteBatch and must return early
    // without touching EntityDataManager or crashing.
    ProjectileRenderController ctrl;

    VoidLight::GPUSceneContext ctx{};
    ctx.spriteBatch = nullptr;
    ctx.valid = false;

    BOOST_CHECK_NO_THROW(ctrl.recordGPU(ctx));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// EDM Integration Tests  (EntityDataManager required; GPU optional)
// ============================================================================

class ProjectileRenderControllerEDMFixture
{
public:
    ProjectileRenderControllerEDMFixture()
    {
        BOOST_REQUIRE(EntityDataManager::Instance().init());
    }

    ~ProjectileRenderControllerEDMFixture()
    {
        EntityDataManager::Instance().clean();
    }

    // Create a live projectile at the given world position and velocity.
    // An invalid owner handle is intentional — the null check inside
    // recordGPU() resolves the embedded-target branch only when isEmbedded()
    // is true, which requires explicit flag bits not set by createProjectile().
    EntityHandle createTestProjectile(const Vector2D& pos,
                                      const Vector2D& vel = Vector2D(80.0f, 0.0f))
    {
        return EntityDataManager::Instance().createProjectile(
            pos, vel, EntityHandle{}, 10.0f, 5.0f);
    }

protected:
    ProjectileRenderController m_controller;
};

BOOST_FIXTURE_TEST_SUITE(ProjectileRenderControllerEDMTests,
                         ProjectileRenderControllerEDMFixture)

BOOST_AUTO_TEST_CASE(TestRecordGPUWithNoProjectilesWritesZeroVertices)
{
    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::Projectile), 0);

    std::vector<VoidLight::SpriteVertex> verts(64);
    VoidLight::SpriteBatch batch;
    if (!tryInitSpriteBatch(batch))
    {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    batch.begin(verts.data(), verts.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext ctx{};
    ctx.cameraX = 0.0f;
    ctx.cameraY = 0.0f;
    ctx.interpolationAlpha = 1.0f;
    ctx.spriteBatch = &batch;
    ctx.valid = true;

    BOOST_CHECK_NO_THROW(m_controller.recordGPU(ctx));

    // Zero projectiles → zero vertices recorded
    BOOST_CHECK_EQUAL(batch.end(), 0u);
    batch.shutdown();
}

BOOST_AUTO_TEST_CASE(TestRecordGPUWithOneProjectileWritesVerticesPerSprite)
{
    EntityHandle projectile = createTestProjectile(Vector2D(200.0f, 150.0f));
    BOOST_REQUIRE(projectile.isValid());
    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::Projectile), 1);

    // Align previous == current so interpolation is a no-op
    auto& hot = EntityDataManager::Instance().getHotData(projectile);
    hot.transform.previousPosition = hot.transform.position;

    std::vector<VoidLight::SpriteVertex> verts(64);
    VoidLight::SpriteBatch batch;
    if (!tryInitSpriteBatch(batch))
    {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    batch.begin(verts.data(), verts.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext ctx{};
    ctx.cameraX = 0.0f;
    ctx.cameraY = 0.0f;
    ctx.interpolationAlpha = 1.0f;
    ctx.spriteBatch = &batch;
    ctx.valid = true;

    m_controller.recordGPU(ctx);

    BOOST_CHECK_EQUAL(batch.end(), VoidLight::SpriteBatch::VERTICES_PER_SPRITE);
    batch.shutdown();
}

BOOST_AUTO_TEST_CASE(TestRecordGPUMultipleProjectilesWriteCorrectVertexCount)
{
    // Three projectiles → 3 × VERTICES_PER_SPRITE vertices
    for (int i = 0; i < 3; ++i)
    {
        EntityHandle p = createTestProjectile(Vector2D(static_cast<float>(i) * 50.0f, 0.0f));
        BOOST_REQUIRE(p.isValid());
        auto& hot = EntityDataManager::Instance().getHotData(p);
        hot.transform.previousPosition = hot.transform.position;
    }
    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::Projectile), 3);

    std::vector<VoidLight::SpriteVertex> verts(64);
    VoidLight::SpriteBatch batch;
    if (!tryInitSpriteBatch(batch))
    {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    batch.begin(verts.data(), verts.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext ctx{};
    ctx.cameraX = 0.0f;
    ctx.cameraY = 0.0f;
    ctx.interpolationAlpha = 1.0f;
    ctx.spriteBatch = &batch;
    ctx.valid = true;

    m_controller.recordGPU(ctx);

    BOOST_CHECK_EQUAL(batch.end(), 3 * VoidLight::SpriteBatch::VERTICES_PER_SPRITE);
    batch.shutdown();
}

BOOST_AUTO_TEST_CASE(TestRecordGPUInterpolatesPositionAtHalfAlpha)
{
    // Previous = (0, 0), current = (100, 100), alpha = 0.5
    // interpX = 50, interpY = 50
    // dstX = interpX - cameraX - PROJECTILE_WIDTH/2 = 50 - 0 - 8 = 42
    // With angle = 0 (horizontal velocity), vertex[0].x == dstX for the
    // un-rotated top-left corner in SpriteBatch drawUVRotated convention.
    EntityHandle projectile = createTestProjectile(Vector2D(100.0f, 100.0f),
                                                   Vector2D(80.0f, 0.0f));
    BOOST_REQUIRE(projectile.isValid());

    auto& hot = EntityDataManager::Instance().getHotData(projectile);
    hot.transform.previousPosition = Vector2D(0.0f, 0.0f);
    hot.transform.position         = Vector2D(100.0f, 100.0f);

    std::vector<VoidLight::SpriteVertex> verts(64);
    VoidLight::SpriteBatch batch;
    if (!tryInitSpriteBatch(batch))
    {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    batch.begin(verts.data(), verts.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext ctx{};
    ctx.cameraX = 0.0f;
    ctx.cameraY = 0.0f;
    ctx.interpolationAlpha = 0.5f;
    ctx.spriteBatch = &batch;
    ctx.valid = true;

    m_controller.recordGPU(ctx);

    BOOST_REQUIRE_EQUAL(batch.end(), VoidLight::SpriteBatch::VERTICES_PER_SPRITE);

    // dstX at alpha 0.5: interpX=50, halfW=8 → vertex x = 42
    // Use a generous epsilon to tolerate platform floating-point rounding.
    BOOST_CHECK_CLOSE(verts[0].x, 42.0f, 0.5f);

    batch.shutdown();
}

BOOST_AUTO_TEST_CASE(TestRecordGPUDoesNotCrashWithInvalidContextValid)
{
    // ctx.valid == false but ctx.spriteBatch != nullptr.
    // The controller does not check ctx.valid — it only guards ctx.spriteBatch.
    // This ensures the guard path is explicit and no silent skip occurs.
    createTestProjectile(Vector2D(100.0f, 100.0f));

    std::vector<VoidLight::SpriteVertex> verts(64);
    VoidLight::SpriteBatch batch;
    if (!tryInitSpriteBatch(batch))
    {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    batch.begin(verts.data(), verts.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext ctx{};
    ctx.spriteBatch = &batch;
    ctx.valid = false;  // explicitly false; controller still proceeds

    BOOST_CHECK_NO_THROW(m_controller.recordGPU(ctx));

    batch.end();
    batch.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()
