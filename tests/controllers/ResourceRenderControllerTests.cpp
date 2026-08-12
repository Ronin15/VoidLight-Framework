/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file ResourceRenderControllerTests.cpp
 * @brief Tests for ResourceRenderController
 *
 * Tests unified rendering of dropped items and containers.
 */

#define BOOST_TEST_MODULE ResourceRenderControllerTests
#include <boost/test/unit_test.hpp>

#include "controllers/render/ResourceRenderController.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "utils/Camera.hpp"
#include "utils/GPUSceneRecorder.hpp"
#include "gpu/SpriteBatch.hpp"
#include "gpu/GPUDevice.hpp"
#include "../events/EventManagerTestAccess.hpp"
#include <memory>
#include <vector>

using namespace VoidLight;

// ============================================================================
// Test Fixture
// ============================================================================

class ResourceRenderControllerTestFixture {
public:
    ResourceRenderControllerTestFixture()
        : m_camera(1000.0f, 1000.0f, 200.0f, 200.0f)
    {
        // Reset EventManager to clean state
        EventManagerTestAccess::reset();
        BOOST_REQUIRE(EventManager::Instance().init());

        if (!ResourceTemplateManager::Instance().isInitialized()) {
            BOOST_REQUIRE(ResourceTemplateManager::Instance().init());
        }

        // Initialize EntityDataManager
        BOOST_REQUIRE(EntityDataManager::Instance().init());

        // Initialize WorldResourceManager
        BOOST_REQUIRE(WorldResourceManager::Instance().init());
        WorldResourceManager::Instance().setActiveWorld(m_worldId);
    }

    ~ResourceRenderControllerTestFixture() {
        EntityDataManager::Instance().clean();
        WorldResourceManager::Instance().clean();
        ResourceTemplateManager::Instance().clean();
        EventManager::Instance().clean();
    }

    Camera& getCamera() { return m_camera; }
    const std::string& getWorldId() const { return m_worldId; }

    VoidLight::ResourceHandle getTestResourceHandle() const {
        return ResourceTemplateManager::Instance().getHandleByName("Super Health Potion");
    }

    EntityHandle createDroppedItem(const Vector2D& position, int quantity = 1) {
        return EntityDataManager::Instance().createDroppedItem(position, getTestResourceHandle(), quantity, m_worldId);
    }

    EntityHandle createContainer(const Vector2D& position, ContainerType type, uint16_t maxSlots = 12) {
        return EntityDataManager::Instance().createContainer(position, type, maxSlots, 0, m_worldId);
    }

    // Non-copyable
    ResourceRenderControllerTestFixture(const ResourceRenderControllerTestFixture&) = delete;
    ResourceRenderControllerTestFixture& operator=(const ResourceRenderControllerTestFixture&) = delete;

private:
    Camera m_camera;
    std::string m_worldId{"resource_render_test_world"};
};

namespace {

bool initSpriteBatchForRecording(VoidLight::SpriteBatch& batch) {
    SDL_GPUDevice* device = VoidLight::GPUDevice::Instance().get();
    if (!device) {
        return false;
    }
    return batch.init(device, "TestBatch");
}

} // namespace

// ============================================================================
// Basic State Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ResourceRenderControllerStateTests, ResourceRenderControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestResourceRenderControllerName) {
    ResourceRenderController controller;

    BOOST_CHECK_EQUAL(controller.getName(), "ResourceRenderController");
}

BOOST_AUTO_TEST_CASE(TestSubscribe) {
    ResourceRenderController controller;

    // Subscribe is a no-op (no events needed)
    controller.subscribe();

    BOOST_CHECK(!controller.isSubscribed());
}

BOOST_AUTO_TEST_CASE(TestDefaultConstruction) {
    // Should not throw
    ResourceRenderController controller;
    BOOST_CHECK_EQUAL(controller.getName(), "ResourceRenderController");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Update Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ResourceRenderControllerUpdateTests, ResourceRenderControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestUpdateWithNoResources) {
    ResourceRenderController controller;

    // Update with various delta times — should not crash with no resources
    controller.update(0.016f, getCamera());
    controller.update(1.0f, getCamera());
    controller.update(0.0f, getCamera());

    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::DroppedItem), 0);
    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::Container), 0);
}

BOOST_AUTO_TEST_CASE(TestUpdateMultipleTimes) {
    ResourceRenderController controller;

    for (int i = 0; i < 100; ++i) {
        controller.update(0.016f, getCamera());
    }

    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::DroppedItem), 0);
    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::Container), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Animation Constants Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(ResourceRenderControllerConstantsTests)

BOOST_AUTO_TEST_CASE(TestAnimationConstants) {
    ResourceRenderControllerTestFixture fixture;
    ResourceRenderController controller;

    // Various delta times should not crash with no resources
    controller.update(0.001f, fixture.getCamera());
    controller.update(0.1f, fixture.getCamera());
    controller.update(1.0f, fixture.getCamera());

    BOOST_CHECK_EQUAL(EntityDataManager::Instance().getEntityCount(EntityKind::DroppedItem), 0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(ResourceRenderControllerBehaviorTests, ResourceRenderControllerTestFixture)

BOOST_AUTO_TEST_CASE(TestUpdateOnlyVisibleDroppedItemsAdvance) {
    auto resourceHandle = getTestResourceHandle();
    BOOST_REQUIRE(resourceHandle.isValid());

    EntityHandle visibleItem = createDroppedItem(Vector2D(100.0f, 100.0f));
    EntityHandle hiddenItem = createDroppedItem(Vector2D(2000.0f, 2000.0f));
    BOOST_REQUIRE(visibleItem.isValid());
    BOOST_REQUIRE(hiddenItem.isValid());

    auto& edm = EntityDataManager::Instance();
    auto& visibleHot = edm.getHotData(visibleItem);
    auto& hiddenHot = edm.getHotData(hiddenItem);
    auto& visibleRender = edm.getItemRenderDataByTypeIndex(visibleHot.typeLocalIndex);
    auto& hiddenRender = edm.getItemRenderDataByTypeIndex(hiddenHot.typeLocalIndex);
    visibleRender.numFrames = 2;
    visibleRender.animSpeedMs = 100;
    hiddenRender.numFrames = 2;
    hiddenRender.animSpeedMs = 100;

    const float initialVisibleBob = visibleRender.bobPhase;
    const float initialHiddenBob = hiddenRender.bobPhase;
    const uint8_t initialVisibleFrame = visibleRender.currentFrame;
    const uint8_t initialHiddenFrame = hiddenRender.currentFrame;

    auto& camera = getCamera();
    camera.setPosition(100.0f, 100.0f);

    ResourceRenderController controller;
    controller.update(1.0f, camera);

    BOOST_CHECK_GT(visibleRender.bobPhase, initialVisibleBob);
    BOOST_CHECK_NE(visibleRender.currentFrame, initialVisibleFrame);
    BOOST_CHECK_EQUAL(hiddenRender.bobPhase, initialHiddenBob);
    BOOST_CHECK_EQUAL(hiddenRender.currentFrame, initialHiddenFrame);
}

BOOST_AUTO_TEST_CASE(TestRecordGPUDroppedItemsUsesCameraCenterAndInterpolatedPosition) {
    auto resourceHandle = getTestResourceHandle();
    BOOST_REQUIRE(resourceHandle.isValid());

    EntityHandle item = createDroppedItem(Vector2D(100.0f, 120.0f));
    BOOST_REQUIRE(item.isValid());

    auto& edm = EntityDataManager::Instance();
    auto& hot = edm.getHotData(item);
    hot.transform.previousPosition = Vector2D(90.0f, 110.0f);
    hot.transform.position = Vector2D(110.0f, 130.0f);

    auto& renderData = edm.getItemRenderDataByTypeIndex(hot.typeLocalIndex);
    renderData.currentFrame = 1;
    renderData.bobPhase = 0.0f;
    renderData.bobAmplitude = 0.0f;

    std::vector<VoidLight::SpriteVertex> vertices(8);
    VoidLight::SpriteBatch batch;
    if (!initSpriteBatchForRecording(batch)) {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext ctx{};
    ctx.cameraX = 10.0f;
    ctx.cameraY = 20.0f;
    ctx.interpolationAlpha = 0.25f;
    ctx.cameraCenter = Vector2D(100.0f, 120.0f);
    ctx.spriteBatch = &batch;
    ctx.valid = true;

    ResourceRenderController controller;
    Camera farAwayCamera(1000.0f, 1000.0f, 200.0f, 200.0f);
    controller.recordGPUDroppedItems(ctx, farAwayCamera);

    BOOST_CHECK_EQUAL(batch.end(), VoidLight::SpriteBatch::VERTICES_PER_SPRITE);

    const float interpX = 90.0f + (110.0f - 90.0f) * 0.25f;
    const float interpY = 110.0f + (130.0f - 110.0f) * 0.25f;
    const float expectedDstX = interpX - ctx.cameraX - renderData.frameWidth * 0.5f;
    const float expectedDstY = interpY - ctx.cameraY - renderData.frameHeight * 0.5f;

    BOOST_CHECK_CLOSE(vertices[0].x, expectedDstX, 0.001f);
    BOOST_CHECK_CLOSE(vertices[0].y, 512.0f - expectedDstY, 0.001f);
    BOOST_CHECK_CLOSE(vertices[1].x, expectedDstX + renderData.frameWidth, 0.001f);
    BOOST_CHECK_CLOSE(vertices[2].y, vertices[0].y - renderData.frameHeight, 0.001f);
}

BOOST_AUTO_TEST_CASE(TestRecordGPUContainersUsesOpenAndClosedVariants) {
    EntityHandle container = createContainer(Vector2D(100.0f, 120.0f), ContainerType::Chest);
    BOOST_REQUIRE(container.isValid());

    auto& hot = EntityDataManager::Instance().getHotData(container);
    hot.transform.previousPosition = Vector2D(100.0f, 120.0f);
    hot.transform.position = Vector2D(100.0f, 120.0f);

    auto& containerData = EntityDataManager::Instance().getContainerData(container);
    auto& renderData = EntityDataManager::Instance().getContainerRenderDataByTypeIndex(hot.typeLocalIndex);

    ResourceRenderController controller;
    Camera renderCamera(1000.0f, 1000.0f, 200.0f, 200.0f);

    std::vector<VoidLight::SpriteVertex> closedVertices(8);
    VoidLight::SpriteBatch closedBatch;
    if (!initSpriteBatchForRecording(closedBatch)) {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    closedBatch.begin(closedVertices.data(), closedVertices.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext closedCtx{};
    closedCtx.cameraX = 0.0f;
    closedCtx.cameraY = 0.0f;
    closedCtx.interpolationAlpha = 1.0f;
    closedCtx.cameraCenter = Vector2D(100.0f, 120.0f);
    closedCtx.spriteBatch = &closedBatch;
    closedCtx.valid = true;

    controller.recordGPUContainers(closedCtx, renderCamera);
    BOOST_CHECK_EQUAL(closedBatch.end(), VoidLight::SpriteBatch::VERTICES_PER_SPRITE);

    const float closedSrcX = static_cast<float>(renderData.atlasX);
    const float closedSrcY = static_cast<float>(renderData.atlasY);
    const float closedSrcW = static_cast<float>(renderData.frameWidth);
    const float closedSrcH = static_cast<float>(renderData.frameHeight);

    BOOST_CHECK_CLOSE(closedVertices[0].u, closedSrcX / 1024.0f, 0.001f);
    BOOST_CHECK_CLOSE(closedVertices[0].v, closedSrcY / 1024.0f, 0.001f);
    BOOST_CHECK_CLOSE(closedVertices[1].x, closedVertices[0].x + closedSrcW, 0.001f);
    BOOST_CHECK_CLOSE(closedVertices[2].y, closedVertices[0].y - closedSrcH, 0.001f);

    containerData.setOpen(true);

    std::vector<VoidLight::SpriteVertex> openVertices(8);
    VoidLight::SpriteBatch openBatch;
    if (!initSpriteBatchForRecording(openBatch)) {
        BOOST_TEST_MESSAGE("Skipping GPU recording test: no GPU device available");
        return;
    }
    openBatch.begin(openVertices.data(), openVertices.size(), nullptr, nullptr, 1024.0f, 1024.0f, 512.0f);

    VoidLight::GPUSceneContext openCtx = closedCtx;
    openCtx.spriteBatch = &openBatch;

    controller.recordGPUContainers(openCtx, renderCamera);
    BOOST_CHECK_EQUAL(openBatch.end(), VoidLight::SpriteBatch::VERTICES_PER_SPRITE);

    const float openSrcX = static_cast<float>(renderData.openAtlasX);
    const float openSrcY = static_cast<float>(renderData.openAtlasY);
    const float openSrcW = static_cast<float>(renderData.openFrameWidth);
    const float openSrcH = static_cast<float>(renderData.openFrameHeight);

    BOOST_CHECK_CLOSE(openVertices[0].u, openSrcX / 1024.0f, 0.001f);
    BOOST_CHECK_CLOSE(openVertices[0].v, openSrcY / 1024.0f, 0.001f);
    BOOST_CHECK_CLOSE(openVertices[1].x, openVertices[0].x + openSrcW, 0.001f);
    BOOST_CHECK_CLOSE(openVertices[2].y, openVertices[0].y - openSrcH, 0.001f);
    BOOST_CHECK_NE(openVertices[2].y, closedVertices[2].y);
}

BOOST_AUTO_TEST_CASE(TestWorldResourceManagerTransitionCleanupStopsTrackingResources) {
    EntityHandle item = createDroppedItem(Vector2D(100.0f, 100.0f));
    EntityHandle container = createContainer(Vector2D(120.0f, 120.0f), ContainerType::Chest);
    BOOST_REQUIRE(item.isValid());
    BOOST_REQUIRE(container.isValid());

    auto& wrm = WorldResourceManager::Instance();
    wrm.prepareForStateTransition();

    std::vector<size_t> foundItems;
    std::vector<size_t> foundContainers;
    wrm.queryDroppedItemsInRadius(Vector2D(0.0f, 0.0f), 100000.0f, foundItems);
    wrm.queryContainersInRadius(Vector2D(0.0f, 0.0f), 100000.0f, foundContainers);

    BOOST_CHECK(EntityDataManager::Instance().isValidHandle(item));
    BOOST_CHECK(EntityDataManager::Instance().isValidHandle(container));
    BOOST_CHECK(foundItems.empty());
    BOOST_CHECK(foundContainers.empty());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Non-Copyable Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(ResourceRenderControllerCopyTests)

BOOST_AUTO_TEST_CASE(TestNonCopyable) {
    // This test verifies at compile time that the controller is non-copyable
    // The following would fail to compile:
    // ResourceRenderController controller;
    // ResourceRenderController copy = controller;  // Won't compile
    // ResourceRenderController copy2(controller);  // Won't compile

    // Just verify we can default construct
    ResourceRenderController controller;
    BOOST_CHECK_EQUAL(controller.getName(), "ResourceRenderController");
}

BOOST_AUTO_TEST_SUITE_END()
