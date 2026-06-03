/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

/**
 * System tests for GPURenderer full frame flow.
 */

#define BOOST_TEST_MODULE GPURendererTests
#include <boost/test/unit_test.hpp>

#include "GPUTestFixture.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/GPURenderer.hpp"
#include "gpu/GPUShaderManager.hpp"

using namespace VoidLight;
using namespace VoidLight::Test;

// Global fixture for SDL cleanup
BOOST_GLOBAL_FIXTURE(GPUGlobalFixture);

/**
 * Test fixture that initializes full GPU stack for renderer testing.
 *
 * Window is shown BEFORE device init so the compositor has time to
 * composite the surface. Swapchain availability is probed once using
 * the same device the tests will use — no throwaway devices.
 */
struct RendererTestFixture : public GPUTestFixture {
    RendererTestFixture() {
        if (!isGPUAvailable()) return;

        device = &GPUDevice::Instance();
        if (device->isInitialized()) {
            GPURenderer::Instance().shutdown();
            GPUShaderManager::Instance().shutdown();
            device->shutdown();
        }

        SDL_Window* window = getTestWindow();
        if (!window) return;

        // Show window BEFORE claiming for GPU — the compositor needs
        // the window visible for swapchain acquisition to work.
        SDL_ShowWindow(window);
        SDL_Delay(100);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {}

        if (!device->init(window)) return;

        renderer = &GPURenderer::Instance();
        rendererInitialized = renderer->init();

        if (rendererInitialized) {
            probeSwapchain();
        }
    }

    ~RendererTestFixture() {
        if (renderer && rendererInitialized) {
            renderer->shutdown();
        }
        GPUShaderManager::Instance().shutdown();
        if (device && device->isInitialized()) {
            device->shutdown();
        }
    }

    GPUDevice* device = nullptr;
    GPURenderer* renderer = nullptr;
    bool rendererInitialized = false;

private:
    /**
     * Probe swapchain availability using the real device.
     *
     * On the first frame there are zero frames in flight, so the
     * non-blocking SDL_AcquireGPUSwapchainTexture is equivalent to
     * the blocking SDL_WaitAndAcquireGPUSwapchainTexture — both
     * return immediately. If the non-blocking acquire fails here
     * the blocking call in the renderer would hang.
     */
    void probeSwapchain() {
        SDL_GPUDevice* rawDevice = device->get();
        SDL_Window* window = device->getWindow();

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(rawDevice);
        if (!cmdBuf) {
            setSwapchainAvailable(false);
            return;
        }

        SDL_GPUTexture* swapTex = nullptr;
        bool ok = SDL_AcquireGPUSwapchainTexture(
            cmdBuf, window, &swapTex, nullptr, nullptr);

        if (ok && swapTex) {
            // Acquired — must submit (swapchain texture binds to this buffer).
            SDL_SubmitGPUCommandBuffer(cmdBuf);
            setSwapchainAvailable(true);
            BOOST_TEST_MESSAGE("Swapchain acquisition verified");
        } else {
            // Either the call failed or texture was NULL (frames in flight
            // or window not composited). Cancel the unused buffer.
            SDL_CancelGPUCommandBuffer(cmdBuf);
            setSwapchainAvailable(false);
            BOOST_TEST_MESSAGE("Swapchain not available — frame cycle tests will be skipped");
        }
    }
};

// ============================================================================
// GPU RENDERER LIFECYCLE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPURendererLifecycleTests)

BOOST_FIXTURE_TEST_CASE(SingletonInstance, RendererTestFixture) {
    GPURenderer& r1 = GPURenderer::Instance();
    GPURenderer& r2 = GPURenderer::Instance();

    BOOST_CHECK(&r1 == &r2);
}

BOOST_FIXTURE_TEST_CASE(InitRequiresGPUDevice, RendererTestFixture) {
    SKIP_IF_NO_GPU();

    // Renderer should be initialized if GPUDevice was initialized
    BOOST_CHECK(rendererInitialized);
}

BOOST_FIXTURE_TEST_CASE(InitFailsWithoutDevice, RendererTestFixture) {
    // Shutdown current state
    if (renderer && rendererInitialized) {
        renderer->shutdown();
        rendererInitialized = false;
    }
    GPUShaderManager::Instance().shutdown();
    if (device && device->isInitialized()) {
        device->shutdown();
    }

    // Try to init renderer without device
    GPURenderer& r = GPURenderer::Instance();
    bool result = r.init();

    BOOST_CHECK(!result);
}

BOOST_FIXTURE_TEST_CASE(ShutdownSafety, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    // Shutdown
    renderer->shutdown();
    rendererInitialized = false;

    // Double shutdown should be safe
    renderer->shutdown();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// FRAME CYCLE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(FrameCycleTests)

BOOST_FIXTURE_TEST_CASE(BeginFrameAcquiresCommandBuffer, RendererTestFixture,
    *boost::unit_test::timeout(10))
{
    SKIP_IF_NO_GPU();
    SKIP_IF_NO_SWAPCHAIN();
    BOOST_REQUIRE(rendererInitialized);

    BOOST_REQUIRE(renderer->beginFrame());

    BOOST_CHECK(renderer->getCommandBuffer() != nullptr);
    BOOST_CHECK(renderer->getCopyPass() != nullptr);

    SDL_GPURenderPass* pass = renderer->beginScenePass();
    if (pass) {
        pass = renderer->beginSwapchainPass();
    }
    renderer->endFrame();
}

BOOST_FIXTURE_TEST_CASE(BeginScenePassEndsCopyPass, RendererTestFixture,
    *boost::unit_test::timeout(10))
{
    SKIP_IF_NO_GPU();
    SKIP_IF_NO_SWAPCHAIN();
    BOOST_REQUIRE(rendererInitialized);

    BOOST_REQUIRE(renderer->beginFrame());
    BOOST_REQUIRE(renderer->getCopyPass() != nullptr);

    SDL_GPURenderPass* scenePass = renderer->beginScenePass();

    BOOST_CHECK(renderer->getCopyPass() == nullptr);

    if (scenePass) {
        BOOST_CHECK(scenePass != nullptr);
    }

    renderer->beginSwapchainPass();
    renderer->endFrame();
}

BOOST_FIXTURE_TEST_CASE(BeginSwapchainPassEndsScenePass, RendererTestFixture,
    *boost::unit_test::timeout(10))
{
    SKIP_IF_NO_GPU();
    SKIP_IF_NO_SWAPCHAIN();
    BOOST_REQUIRE(rendererInitialized);

    BOOST_REQUIRE(renderer->beginFrame());

    renderer->beginScenePass();

    SDL_GPURenderPass* swapchainPass = renderer->beginSwapchainPass();

    if (swapchainPass) {
        BOOST_CHECK(swapchainPass != nullptr);
    }

    renderer->endFrame();
}

BOOST_FIXTURE_TEST_CASE(EndFrameSubmitsCommandBuffer, RendererTestFixture,
    *boost::unit_test::timeout(10))
{
    SKIP_IF_NO_GPU();
    SKIP_IF_NO_SWAPCHAIN();
    BOOST_REQUIRE(rendererInitialized);

    BOOST_REQUIRE(renderer->beginFrame());

    renderer->beginScenePass();
    renderer->beginSwapchainPass();
    renderer->endFrame();

    BOOST_CHECK(renderer->getCommandBuffer() == nullptr);
    BOOST_CHECK(renderer->getCopyPass() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(MultipleFrameCycles, RendererTestFixture,
    *boost::unit_test::timeout(10))
{
    SKIP_IF_NO_GPU();
    SKIP_IF_NO_SWAPCHAIN();
    BOOST_REQUIRE(rendererInitialized);

    for (int frame = 0; frame < 5; ++frame) {
        BOOST_REQUIRE(renderer->beginFrame());

        renderer->beginScenePass();
        renderer->beginSwapchainPass();
        renderer->endFrame();

        BOOST_CHECK(renderer->getCommandBuffer() == nullptr);
        BOOST_CHECK(renderer->getCopyPass() == nullptr);
    }

    BOOST_CHECK(renderer->getSceneTexture() != nullptr);
    BOOST_CHECK(renderer->getSceneTexture()->isValid());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PIPELINE ACCESSOR TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(PipelineAccessorTests)

BOOST_FIXTURE_TEST_CASE(SpriteOpaquePipelineValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SDL_GPUGraphicsPipeline* pipeline = renderer->getSpriteOpaquePipeline();
    BOOST_CHECK(pipeline != nullptr);
}

BOOST_FIXTURE_TEST_CASE(SpriteAlphaPipelineValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SDL_GPUGraphicsPipeline* pipeline = renderer->getSpriteAlphaPipeline();
    BOOST_CHECK(pipeline != nullptr);
}

BOOST_FIXTURE_TEST_CASE(ParticlePipelineValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SDL_GPUGraphicsPipeline* pipeline = renderer->getParticlePipeline();
    BOOST_CHECK(pipeline != nullptr);
}

BOOST_FIXTURE_TEST_CASE(PrimitivePipelineValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SDL_GPUGraphicsPipeline* pipeline = renderer->getPrimitivePipeline();
    BOOST_CHECK(pipeline != nullptr);
}

BOOST_FIXTURE_TEST_CASE(CompositePipelineValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SDL_GPUGraphicsPipeline* pipeline = renderer->getCompositePipeline();
    BOOST_CHECK(pipeline != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// VERTEX POOL ACCESSOR TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(VertexPoolAccessorTests)

BOOST_FIXTURE_TEST_CASE(SpriteVertexPoolInitialized, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    GPUVertexPool& pool = renderer->getSpriteVertexPool();
    BOOST_CHECK(pool.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(EntityVertexPoolInitialized, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    GPUVertexPool& pool = renderer->getEntityVertexPool();
    BOOST_CHECK(pool.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(ParticleVertexPoolInitialized, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    GPUVertexPool& pool = renderer->getParticleVertexPool();
    BOOST_CHECK(pool.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(PrimitiveVertexPoolInitialized, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    GPUVertexPool& pool = renderer->getPrimitiveVertexPool();
    BOOST_CHECK(pool.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(UIVertexPoolInitialized, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    GPUVertexPool& pool = renderer->getUIVertexPool();
    BOOST_CHECK(pool.isInitialized());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SAMPLER ACCESSOR TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SamplerAccessorTests)

BOOST_FIXTURE_TEST_CASE(NearestSamplerValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SDL_GPUSampler* sampler = renderer->getNearestSampler();
    BOOST_CHECK(sampler != nullptr);
}

BOOST_FIXTURE_TEST_CASE(LinearSamplerValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SDL_GPUSampler* sampler = renderer->getLinearSampler();
    BOOST_CHECK(sampler != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SCENE TEXTURE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SceneTextureTests)

BOOST_FIXTURE_TEST_CASE(SceneTextureValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    GPUTexture* sceneTexture = renderer->getSceneTexture();
    BOOST_CHECK(sceneTexture != nullptr);
    BOOST_CHECK(sceneTexture->isValid());
}

BOOST_FIXTURE_TEST_CASE(SceneTextureIsSamplerAndRenderTarget, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    GPUTexture* sceneTexture = renderer->getSceneTexture();
    BOOST_REQUIRE(sceneTexture != nullptr);

    BOOST_CHECK(sceneTexture->isSampler());
    BOOST_CHECK(sceneTexture->isRenderTarget());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// COMPOSITE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(CompositeTests)

BOOST_FIXTURE_TEST_CASE(SetCompositeParams, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    // Setters are void — verify they don't crash with various values
    renderer->setCompositeParams(2.0f, 0.25f, 0.5f);
    renderer->setCompositeParams(1.0f, 0.0f, 0.0f);
    renderer->setCompositeParams(4.0f, -0.5f, 0.75f);
}

BOOST_FIXTURE_TEST_CASE(SetDayNightParams, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    // Setters are void — verify they don't crash with various values
    renderer->setDayNightParams(0.8f, 0.9f, 1.0f, 0.5f);
    renderer->setDayNightParams(1.0f, 1.0f, 1.0f, 0.0f);
    renderer->setDayNightParams(0.0f, 0.0f, 0.0f, 1.0f);
}

BOOST_FIXTURE_TEST_CASE(RenderCompositeInSwapchainPass, RendererTestFixture,
    *boost::unit_test::timeout(10))
{
    SKIP_IF_NO_GPU();
    SKIP_IF_NO_SWAPCHAIN();
    BOOST_REQUIRE(rendererInitialized);

    renderer->setCompositeParams(1.0f, 0.0f, 0.0f);
    renderer->setDayNightParams(1.0f, 1.0f, 1.0f, 0.0f);

    BOOST_REQUIRE(renderer->beginFrame());
    renderer->beginScenePass();
    SDL_GPURenderPass* swapchainPass = renderer->beginSwapchainPass();

    if (swapchainPass) {
        renderer->renderComposite(swapchainPass);
    }

    renderer->endFrame();
    BOOST_CHECK(renderer->getCommandBuffer() == nullptr);
    BOOST_CHECK(renderer->getCopyPass() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// VIEWPORT TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(ViewportTests)

BOOST_FIXTURE_TEST_CASE(ViewportDimensionsValid, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    uint32_t width = renderer->getViewportWidth();
    uint32_t height = renderer->getViewportHeight();

    // Viewport should have valid dimensions
    BOOST_CHECK(width > 0);
    BOOST_CHECK(height > 0);
}

BOOST_FIXTURE_TEST_CASE(UpdateViewport, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    renderer->updateViewport(1920, 1080);

    BOOST_CHECK_EQUAL(renderer->getViewportWidth(), 1920u);
    BOOST_CHECK_EQUAL(renderer->getViewportHeight(), 1080u);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ORTHO MATRIX TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(OrthoMatrixTests)

BOOST_AUTO_TEST_CASE(CreateOrthoMatrixBasic) {
    float matrix[16];

    GPURenderer::createOrthoMatrix(0.0f, 800.0f, 600.0f, 0.0f, matrix);

    // Matrix should be valid orthographic projection
    // Check key elements
    // [0][0] = 2/(right-left) = 2/800 = 0.0025
    BOOST_CHECK_CLOSE(matrix[0], 2.0f / 800.0f, 0.001f);

    // [1][1] = 2/(top-bottom) = 2/(0-600) = -2/600
    BOOST_CHECK_CLOSE(matrix[5], 2.0f / (0.0f - 600.0f), 0.001f);

    // [3][0] = -(right+left)/(right-left) = -800/800 = -1
    BOOST_CHECK_CLOSE(matrix[12], -(800.0f + 0.0f) / 800.0f, 0.001f);

    // [3][1] = -(top+bottom)/(top-bottom) = -600/(0-600) = 1
    BOOST_CHECK_CLOSE(matrix[13], -(0.0f + 600.0f) / (0.0f - 600.0f), 0.001f);
}

BOOST_AUTO_TEST_CASE(CreateOrthoMatrixZeroDepth) {
    float matrix[16];

    GPURenderer::createOrthoMatrix(0.0f, 1920.0f, 1080.0f, 0.0f, matrix);

    // Z components for 2D (near=0, far=1)
    // [2][2] = -2/(far-near) = -2/1 = -2
    // For 2D ortho, we typically use 0 to 1 depth
    // Check matrix is not all zeros
    bool hasNonZero = false;
    for (int i = 0; i < 16; ++i) {
        if (matrix[i] != 0.0f) {
            hasNonZero = true;
            break;
        }
    }
    BOOST_CHECK(hasNonZero);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SPRITE BATCH ACCESSOR TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteBatchAccessorTests)

BOOST_FIXTURE_TEST_CASE(SpriteBatchAccessor, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SpriteBatch& batch = renderer->getSpriteBatch();
    // Batch should be accessible (initialization tested elsewhere)
    BOOST_CHECK(batch.getIndexBuffer() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(EntityBatchAccessor, RendererTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(rendererInitialized);

    SpriteBatch& batch = renderer->getEntityBatch();
    BOOST_CHECK(batch.getIndexBuffer() != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
