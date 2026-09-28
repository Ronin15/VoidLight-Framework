/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

/**
 * Integration tests for GPU resource wrappers.
 * Tests GPUBuffer, GPUTexture, GPUTransferBuffer, and GPUSampler.
 */

#define BOOST_TEST_MODULE GPUResourceTests
#include <boost/test/unit_test.hpp>

#include "GPUTestFixture.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/GPUBuffer.hpp"
#include "gpu/GPUTexture.hpp"
#include "gpu/GPUTransferBuffer.hpp"
#include "gpu/GPUSampler.hpp"
#include "gpu/GPUTypes.hpp"

using namespace VoidLight;
using namespace VoidLight::Test;

// Global fixture for SDL cleanup
BOOST_GLOBAL_FIXTURE(GPUGlobalFixture);

/**
 * Shuts the shared GPUDevice down once at the end of the run. Registered
 * after GPUGlobalFixture so it tears down first (before the window and SDL).
 */
struct ResourceDeviceTeardown {
    ~ResourceDeviceTeardown() {
        if (GPUDevice::Instance().isInitialized()) {
            GPUDevice::Instance().shutdown();
        }
    }
};
BOOST_GLOBAL_FIXTURE(ResourceDeviceTeardown);

/**
 * Test fixture that provides an initialized GPUDevice for resource testing.
 * The device is shared across tests (device lifecycle is covered by
 * GPUDeviceTests); every resource wrapper here is RAII-released per test.
 */
struct ResourceTestFixture : public GPUTestFixture {
    ResourceTestFixture() {
        if (!isGPUAvailable()) return;

        device = &GPUDevice::Instance();
        if (device->isInitialized()) return;

        SDL_Window* window = getTestWindow();
        if (window) {
            BOOST_REQUIRE(device->init(window));
        }
    }

    GPUDevice* device = nullptr;
};

// ============================================================================
// GPU BUFFER TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUBufferTests)

BOOST_FIXTURE_TEST_CASE(DefaultConstructorInvalid, ResourceTestFixture) {
    GPUBuffer buffer;

    BOOST_CHECK(!buffer.isValid());
    BOOST_CHECK(buffer.get() == nullptr);
    BOOST_CHECK_EQUAL(buffer.getSize(), 0u);
}

BOOST_FIXTURE_TEST_CASE(CreateVertexBuffer, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const uint32_t bufferSize = 1024;
    GPUBuffer buffer(device->get(), SDL_GPU_BUFFERUSAGE_VERTEX, bufferSize);

    BOOST_CHECK(buffer.isValid());
    BOOST_CHECK(buffer.get() != nullptr);
    BOOST_CHECK_EQUAL(buffer.getSize(), bufferSize);
    BOOST_CHECK(buffer.getUsage() == SDL_GPU_BUFFERUSAGE_VERTEX);
}

BOOST_FIXTURE_TEST_CASE(CreateIndexBuffer, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const uint32_t bufferSize = 512;
    GPUBuffer buffer(device->get(), SDL_GPU_BUFFERUSAGE_INDEX, bufferSize);

    BOOST_CHECK(buffer.isValid());
    BOOST_CHECK_EQUAL(buffer.getSize(), bufferSize);
    BOOST_CHECK(buffer.getUsage() == SDL_GPU_BUFFERUSAGE_INDEX);
}

BOOST_FIXTURE_TEST_CASE(BufferMoveSemantics, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const uint32_t bufferSize = 256;
    GPUBuffer buffer1(device->get(), SDL_GPU_BUFFERUSAGE_VERTEX, bufferSize);
    BOOST_REQUIRE(buffer1.isValid());

    SDL_GPUBuffer* rawPtr = buffer1.get();

    // Move construct
    GPUBuffer buffer2(std::move(buffer1));

    BOOST_CHECK(!buffer1.isValid());
    BOOST_CHECK(buffer2.isValid());
    BOOST_CHECK(buffer2.get() == rawPtr);
}

BOOST_FIXTURE_TEST_CASE(BufferMoveAssignment, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUBuffer buffer1(device->get(), SDL_GPU_BUFFERUSAGE_VERTEX, 256);
    BOOST_REQUIRE(buffer1.isValid());

    SDL_GPUBuffer* rawPtr = buffer1.get();

    GPUBuffer buffer2;
    buffer2 = std::move(buffer1);

    BOOST_CHECK(!buffer1.isValid());
    BOOST_CHECK(buffer2.isValid());
    BOOST_CHECK(buffer2.get() == rawPtr);
}

BOOST_FIXTURE_TEST_CASE(BufferAsBinding, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUBuffer buffer(device->get(), SDL_GPU_BUFFERUSAGE_VERTEX, 1024);
    BOOST_REQUIRE(buffer.isValid());

    SDL_GPUBufferBinding binding = buffer.asBinding(0);
    BOOST_CHECK(binding.buffer == buffer.get());
    BOOST_CHECK_EQUAL(binding.offset, 0u);

    // Test with offset
    SDL_GPUBufferBinding bindingWithOffset = buffer.asBinding(256);
    BOOST_CHECK_EQUAL(bindingWithOffset.offset, 256u);
}

BOOST_FIXTURE_TEST_CASE(BufferAsRegion, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUBuffer buffer(device->get(), SDL_GPU_BUFFERUSAGE_VERTEX, 1024);
    BOOST_REQUIRE(buffer.isValid());

    // Full buffer region
    SDL_GPUBufferRegion region = buffer.asRegion();
    BOOST_CHECK(region.buffer == buffer.get());
    BOOST_CHECK_EQUAL(region.offset, 0u);
    BOOST_CHECK_EQUAL(region.size, 1024u);

    // Partial region
    SDL_GPUBufferRegion partial = buffer.asRegion(256, 512);
    BOOST_CHECK_EQUAL(partial.offset, 256u);
    BOOST_CHECK_EQUAL(partial.size, 512u);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GPU TEXTURE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUTextureTests)

BOOST_FIXTURE_TEST_CASE(DefaultConstructorInvalid, ResourceTestFixture) {
    GPUTexture texture;

    BOOST_CHECK(!texture.isValid());
    BOOST_CHECK(texture.get() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(CreateSamplerTexture, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTexture texture(
        device->get(),
        256, 256,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER);

    BOOST_CHECK(texture.isValid());
    BOOST_CHECK(texture.get() != nullptr);
    BOOST_CHECK_EQUAL(texture.getWidth(), 256u);
    BOOST_CHECK_EQUAL(texture.getHeight(), 256u);
    BOOST_CHECK(texture.getFormat() == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    BOOST_CHECK(texture.isSampler());
    BOOST_CHECK(!texture.isRenderTarget());
}

BOOST_FIXTURE_TEST_CASE(CreateRenderTargetTexture, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTexture texture(
        device->get(),
        1920, 1080,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);

    BOOST_CHECK(texture.isValid());
    BOOST_CHECK_EQUAL(texture.getWidth(), 1920u);
    BOOST_CHECK_EQUAL(texture.getHeight(), 1080u);
    BOOST_CHECK(texture.isRenderTarget());
    BOOST_CHECK(!texture.isSampler());
}

BOOST_FIXTURE_TEST_CASE(CreateCombinedUsageTexture, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    // Scene texture needs both sampler and color target
    GPUTexture texture(
        device->get(),
        800, 600,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);

    BOOST_CHECK(texture.isValid());
    BOOST_CHECK(texture.isSampler());
    BOOST_CHECK(texture.isRenderTarget());
}

BOOST_FIXTURE_TEST_CASE(TextureMoveSemantics, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTexture tex1(
        device->get(), 128, 128,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER);
    BOOST_REQUIRE(tex1.isValid());

    SDL_GPUTexture* rawPtr = tex1.get();

    GPUTexture tex2(std::move(tex1));

    BOOST_CHECK(!tex1.isValid());
    BOOST_CHECK(tex2.isValid());
    BOOST_CHECK(tex2.get() == rawPtr);
}

BOOST_FIXTURE_TEST_CASE(TextureAsColorTarget, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTexture texture(
        device->get(), 800, 600,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    BOOST_REQUIRE(texture.isValid());

    SDL_FColor clearColor = {0.2f, 0.3f, 0.4f, 1.0f};
    SDL_GPUColorTargetInfo targetInfo = texture.asColorTarget(
        SDL_GPU_LOADOP_CLEAR,
        clearColor,
        SDL_GPU_STOREOP_STORE);

    BOOST_CHECK(targetInfo.texture == texture.get());
    BOOST_CHECK(targetInfo.load_op == SDL_GPU_LOADOP_CLEAR);
    BOOST_CHECK(targetInfo.store_op == SDL_GPU_STOREOP_STORE);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GPU TRANSFER BUFFER TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUTransferBufferTests)

BOOST_FIXTURE_TEST_CASE(DefaultConstructorInvalid, ResourceTestFixture) {
    GPUTransferBuffer buffer;

    BOOST_CHECK(!buffer.isValid());
    BOOST_CHECK(buffer.get() == nullptr);
    BOOST_CHECK(!buffer.isMapped());
}

BOOST_FIXTURE_TEST_CASE(CreateUploadBuffer, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const uint32_t bufferSize = 4096;
    GPUTransferBuffer buffer(
        device->get(),
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        bufferSize);

    BOOST_CHECK(buffer.isValid());
    BOOST_CHECK(buffer.get() != nullptr);
    BOOST_CHECK_EQUAL(buffer.getSize(), bufferSize);
    BOOST_CHECK(!buffer.isMapped());
}

BOOST_FIXTURE_TEST_CASE(MapAndUnmap, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTransferBuffer buffer(
        device->get(),
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        1024);
    BOOST_REQUIRE(buffer.isValid());

    // Map buffer
    void* ptr = buffer.map(true);
    BOOST_CHECK(ptr != nullptr);
    BOOST_CHECK(buffer.isMapped());

    // Write some data
    memset(ptr, 0xAB, 1024);

    // Unmap buffer
    buffer.unmap();
    BOOST_CHECK(!buffer.isMapped());
}

BOOST_FIXTURE_TEST_CASE(MapWithCycleParameter, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTransferBuffer buffer(
        device->get(),
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        512);
    BOOST_REQUIRE(buffer.isValid());

    // Map with cycle=true (allows buffer reuse)
    void* ptr1 = buffer.map(true);
    BOOST_CHECK(ptr1 != nullptr);
    buffer.unmap();

    // Map with cycle=false
    void* ptr2 = buffer.map(false);
    BOOST_CHECK(ptr2 != nullptr);
    buffer.unmap();
}

BOOST_FIXTURE_TEST_CASE(TransferBufferAsLocation, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTransferBuffer buffer(
        device->get(),
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        2048);
    BOOST_REQUIRE(buffer.isValid());

    SDL_GPUTransferBufferLocation loc = buffer.asLocation(0);
    BOOST_CHECK(loc.transfer_buffer == buffer.get());
    BOOST_CHECK_EQUAL(loc.offset, 0u);

    SDL_GPUTransferBufferLocation locWithOffset = buffer.asLocation(512);
    BOOST_CHECK_EQUAL(locWithOffset.offset, 512u);
}

BOOST_FIXTURE_TEST_CASE(TransferBufferMoveSemantics, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUTransferBuffer buf1(
        device->get(),
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        1024);
    BOOST_REQUIRE(buf1.isValid());

    SDL_GPUTransferBuffer* rawPtr = buf1.get();

    GPUTransferBuffer buf2(std::move(buf1));

    BOOST_CHECK(!buf1.isValid());
    BOOST_CHECK(buf2.isValid());
    BOOST_CHECK(buf2.get() == rawPtr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GPU SAMPLER TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUSamplerTests)

BOOST_FIXTURE_TEST_CASE(DefaultConstructorInvalid, ResourceTestFixture) {
    GPUSampler sampler;

    BOOST_CHECK(!sampler.isValid());
    BOOST_CHECK(sampler.get() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(CreateNearestSampler, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUSampler sampler = GPUSampler::createNearest(device->get());

    BOOST_CHECK(sampler.isValid());
    BOOST_CHECK(sampler.get() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(CreateLinearSampler, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUSampler sampler = GPUSampler::createLinear(device->get());

    BOOST_CHECK(sampler.isValid());
    BOOST_CHECK(sampler.get() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(CreateLinearMipmappedSampler, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUSampler sampler = GPUSampler::createLinearMipmapped(device->get());

    BOOST_CHECK(sampler.isValid());
    BOOST_CHECK(sampler.get() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(CreateCustomSampler, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUSampler sampler(
        device->get(),
        SDL_GPU_FILTER_LINEAR,
        SDL_GPU_SAMPLERADDRESSMODE_REPEAT);

    BOOST_CHECK(sampler.isValid());
}

BOOST_FIXTURE_TEST_CASE(SamplerMoveSemantics, ResourceTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUSampler sampler1 = GPUSampler::createNearest(device->get());
    BOOST_REQUIRE(sampler1.isValid());

    SDL_GPUSampler* rawPtr = sampler1.get();

    GPUSampler sampler2(std::move(sampler1));

    BOOST_CHECK(!sampler1.isValid());
    BOOST_CHECK(sampler2.isValid());
    BOOST_CHECK(sampler2.get() == rawPtr);
}

BOOST_AUTO_TEST_SUITE_END()
