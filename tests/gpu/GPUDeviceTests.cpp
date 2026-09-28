/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

/**
 * Integration tests for GPUDevice singleton.
 * These tests require GPU availability.
 */

#define BOOST_TEST_MODULE GPUDeviceTests
#include <boost/test/unit_test.hpp>

#include "GPUTestFixture.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/GPUPlatformConfig.hpp"
#include <SDL3/SDL.h>
#include <string>

using namespace VoidLight;
using namespace VoidLight::Test;

// Global fixture for SDL cleanup
BOOST_GLOBAL_FIXTURE(GPUGlobalFixture);

// ============================================================================
// GPU DEVICE LIFECYCLE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUDeviceLifecycleTests)

BOOST_FIXTURE_TEST_CASE(SingletonInstance, GPUTestFixture) {
    // GPUDevice should be a singleton
    GPUDevice& device1 = GPUDevice::Instance();
    GPUDevice& device2 = GPUDevice::Instance();

    BOOST_CHECK(&device1 == &device2);
}

BOOST_FIXTURE_TEST_CASE(InitWithValidWindow, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    // Ensure clean state
    if (device.isInitialized()) {
        device.shutdown();
    }

    // Create test window
    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);

    // Initialize should succeed
    bool result = device.init(window);
    BOOST_CHECK(result);
    BOOST_CHECK(device.isInitialized());
    BOOST_CHECK(device.get() != nullptr);
    BOOST_CHECK(device.getWindow() == window);

    // Cleanup
    device.shutdown();
    BOOST_CHECK(!device.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(InitWithNullWindow, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    // Init with null window should fail
    bool result = device.init(nullptr);
    BOOST_CHECK(!result);
    BOOST_CHECK(!device.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(ShutdownWithoutInit, GPUTestFixture) {
    GPUDevice& device = GPUDevice::Instance();

    // Ensure not initialized
    if (device.isInitialized()) {
        device.shutdown();
    }

    // Shutdown without init should be safe (no-op)
    device.shutdown();
    BOOST_CHECK(!device.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(DoubleInitSafety, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);

    // First init
    bool result1 = device.init(window);
    BOOST_REQUIRE(result1);

    SDL_GPUDevice* firstDevice = device.get();

    // Second init should return true but not create new device
    bool result2 = device.init(window);
    BOOST_CHECK(result2);
    BOOST_CHECK(device.get() == firstDevice);

    device.shutdown();
}

BOOST_FIXTURE_TEST_CASE(DoubleShutdownSafety, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);

    BOOST_REQUIRE(device.init(window));
    BOOST_REQUIRE(device.isInitialized());

    // First shutdown
    device.shutdown();
    BOOST_CHECK(!device.isInitialized());

    // Second shutdown should be safe
    device.shutdown();
    BOOST_CHECK(!device.isInitialized());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GPU DEVICE QUERY TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUDeviceQueryTests)

BOOST_FIXTURE_TEST_CASE(GetShaderFormats, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);
    BOOST_REQUIRE(device.init(window));

    SDL_GPUShaderFormat formats = device.getShaderFormats();

    // Should support at least one shader format
    BOOST_CHECK(formats != 0);

    // Check for common formats
    bool hasSPIRV = (formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0;
    bool hasMSL = (formats & SDL_GPU_SHADERFORMAT_MSL) != 0;
    bool hasDXBC = (formats & SDL_GPU_SHADERFORMAT_DXBC) != 0;
    bool hasDXIL = (formats & SDL_GPU_SHADERFORMAT_DXIL) != 0;

    // Should have at least one of these
    BOOST_CHECK(hasSPIRV || hasMSL || hasDXBC || hasDXIL);

    const SDL_GPUShaderFormat requestedFormats =
        VoidLight::GPUPlatformConfig::getRequestedShaderFormats();
    BOOST_CHECK((formats & requestedFormats) == requestedFormats);

    BOOST_TEST_MESSAGE("Shader formats - SPIRV: " << hasSPIRV
                                                  << ", MSL: " << hasMSL
                                                  << ", DXBC: " << hasDXBC
                                                  << ", DXIL: " << hasDXIL);

    device.shutdown();
}

BOOST_FIXTURE_TEST_CASE(GetSwapchainFormat, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);
    BOOST_REQUIRE(device.init(window));

    SDL_GPUTextureFormat format = device.getSwapchainFormat();

    // Format should be valid
    BOOST_CHECK(format != SDL_GPU_TEXTUREFORMAT_INVALID);

    // Common swapchain formats
    bool isCommonFormat =
        format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB ||
        format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;

    BOOST_CHECK(isCommonFormat);
    BOOST_TEST_MESSAGE("Swapchain format: " << static_cast<int>(format));

    device.shutdown();
}

BOOST_FIXTURE_TEST_CASE(GetDriverName, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);
    BOOST_REQUIRE(device.init(window));

    const char* driverName = device.getDriverName();

    // Driver name should be non-null and non-empty
    BOOST_CHECK(driverName != nullptr);
    BOOST_CHECK(strlen(driverName) > 0);
    BOOST_CHECK_EQUAL(std::string(driverName),
        std::string(VoidLight::GPUPlatformConfig::getPreferredDriverName()));

    BOOST_TEST_MESSAGE("GPU driver: " << driverName);

    device.shutdown();
}

BOOST_FIXTURE_TEST_CASE(SupportsCommonFormats, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);
    BOOST_REQUIRE(device.init(window));

    // Test common texture formats for sampler usage
    bool supportsRGBA = device.supportsFormat(
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER);
    BOOST_CHECK(supportsRGBA);

    // Test color target support
    bool supportsColorTarget = device.supportsFormat(
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    BOOST_CHECK(supportsColorTarget);

    // Test combined sampler + color target (for render-to-texture)
    bool supportsBoth = device.supportsFormat(
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    BOOST_CHECK(supportsBoth);

    device.shutdown();
}

BOOST_FIXTURE_TEST_CASE(QueryWhenNotInitialized, GPUTestFixture) {
    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    // Queries on uninitialized device should return safe defaults
    BOOST_CHECK(device.getShaderFormats() == 0);
    BOOST_CHECK(device.getSwapchainFormat() == SDL_GPU_TEXTUREFORMAT_INVALID);
    BOOST_CHECK(device.getDriverName() == nullptr || strlen(device.getDriverName()) == 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GPU DEVICE ACCESSOR TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUDeviceAccessorTests)

BOOST_FIXTURE_TEST_CASE(GetReturnsNullWhenNotInitialized, GPUTestFixture) {
    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    BOOST_CHECK(device.get() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(GetWindowReturnsNullWhenNotInitialized, GPUTestFixture) {
    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    BOOST_CHECK(device.getWindow() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(IsInitializedAccuracy, GPUTestFixture) {
    SKIP_IF_NO_GPU();

    GPUDevice& device = GPUDevice::Instance();

    if (device.isInitialized()) {
        device.shutdown();
    }

    BOOST_CHECK(!device.isInitialized());

    SDL_Window* window = getTestWindow();
    BOOST_REQUIRE(window != nullptr);

    BOOST_REQUIRE(device.init(window));
    BOOST_CHECK(device.isInitialized());

    device.shutdown();
    BOOST_CHECK(!device.isInitialized());
}

BOOST_AUTO_TEST_SUITE_END()
