/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

/**
 * Integration tests for GPUShaderManager.
 * These tests require GPU availability.
 */

#define BOOST_TEST_MODULE GPUShaderManagerTests
#include <boost/test/unit_test.hpp>

#include "GPUTestFixture.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/GPUPlatformConfig.hpp"
#include "gpu/GPUShaderManager.hpp"
#include "utils/ResourcePath.hpp"

using namespace VoidLight;
using namespace VoidLight::Test;

// Global fixture for SDL cleanup
BOOST_GLOBAL_FIXTURE(GPUGlobalFixture);

/**
 * Test fixture that initializes GPUDevice for shader testing.
 */
struct ShaderTestFixture : public GPUTestFixture {
    ShaderTestFixture() {
        if (!isGPUAvailable()) return;

        device = &GPUDevice::Instance();
        if (device->isInitialized()) {
            device->shutdown();
        }

        SDL_Window* window = getTestWindow();
        if (window) {
            BOOST_REQUIRE(device->init(window));
        }

        shaderMgr = &GPUShaderManager::Instance();
        if (device->isInitialized()) {
            BOOST_REQUIRE(shaderMgr->init(device->get()));
        }
    }

    ~ShaderTestFixture() {
        if (shaderMgr) {
            shaderMgr->shutdown();
        }
        if (device && device->isInitialized()) {
            device->shutdown();
        }
    }

    GPUDevice* device = nullptr;
    GPUShaderManager* shaderMgr = nullptr;
};

// ============================================================================
// SHADER LOADING TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(ShaderLoadingTests)

BOOST_FIXTURE_TEST_CASE(LoadSpriteVertexShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    ShaderInfo info{};
    info.numUniformBuffers = 1; // ViewProjection UBO

    SDL_GPUShader* shader = shaderMgr->loadShader(
        "res/shaders/sprite.vert",
        SDL_GPU_SHADERSTAGE_VERTEX,
        info);

    BOOST_CHECK(shader != nullptr);
    BOOST_TEST_MESSAGE("Sprite vertex shader loaded successfully");
}

BOOST_FIXTURE_TEST_CASE(LoadSpriteFragmentShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    ShaderInfo info{};
    info.numSamplers = 1; // Texture sampler

    SDL_GPUShader* shader = shaderMgr->loadShader(
        "res/shaders/sprite.frag",
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        info);

    BOOST_CHECK(shader != nullptr);
    BOOST_TEST_MESSAGE("Sprite fragment shader loaded successfully");
}

BOOST_FIXTURE_TEST_CASE(LoadColorVertexShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    ShaderInfo info{};
    info.numUniformBuffers = 1;

    SDL_GPUShader* shader = shaderMgr->loadShader(
        "res/shaders/color.vert",
        SDL_GPU_SHADERSTAGE_VERTEX,
        info);

    BOOST_CHECK(shader != nullptr);
    BOOST_TEST_MESSAGE("Color vertex shader loaded successfully");
}

BOOST_FIXTURE_TEST_CASE(LoadColorFragmentShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    ShaderInfo info{};
    // Color fragment shader has no samplers

    SDL_GPUShader* shader = shaderMgr->loadShader(
        "res/shaders/color.frag",
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        info);

    BOOST_CHECK(shader != nullptr);
    BOOST_TEST_MESSAGE("Color fragment shader loaded successfully");
}

BOOST_FIXTURE_TEST_CASE(LoadCompositeVertexShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    ShaderInfo info{};
    info.numUniformBuffers = 1;

    SDL_GPUShader* shader = shaderMgr->loadShader(
        "res/shaders/composite.vert",
        SDL_GPU_SHADERSTAGE_VERTEX,
        info);

    BOOST_CHECK(shader != nullptr);
    BOOST_TEST_MESSAGE("Composite vertex shader loaded successfully");
}

BOOST_FIXTURE_TEST_CASE(LoadCompositeFragmentShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    ShaderInfo info{};
    info.numSamplers = 1;
    info.numUniformBuffers = 1; // Composite UBO

    SDL_GPUShader* shader = shaderMgr->loadShader(
        "res/shaders/composite.frag",
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        info);

    BOOST_CHECK(shader != nullptr);
    BOOST_TEST_MESSAGE("Composite fragment shader loaded successfully");
}

BOOST_FIXTURE_TEST_CASE(LoadNonExistentShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    ShaderInfo info{};

    SDL_GPUShader* shader = shaderMgr->loadShader(
        "res/shaders/nonexistent.vert",
        SDL_GPU_SHADERSTAGE_VERTEX,
        info);

    // Should return nullptr for missing shader
    BOOST_CHECK(shader == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SHADER CACHING TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(ShaderCachingTests)

BOOST_FIXTURE_TEST_CASE(HasShaderAfterLoad, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const std::string shaderPath = "res/shaders/sprite.vert";
    ShaderInfo info{};
    info.numUniformBuffers = 1;

    // Initially should not have shader
    BOOST_CHECK(!shaderMgr->hasShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, info));

    // Load shader
    shaderMgr->loadShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, info);

    // Now should have shader
    BOOST_CHECK(shaderMgr->hasShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, info));
}

BOOST_FIXTURE_TEST_CASE(GetShaderReturnsSamePointer, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const std::string shaderPath = "res/shaders/sprite.frag";

    ShaderInfo info{};
    info.numSamplers = 1;

    // Load shader
    SDL_GPUShader* shader1 = shaderMgr->loadShader(
        shaderPath, SDL_GPU_SHADERSTAGE_FRAGMENT, info);
    BOOST_REQUIRE(shader1 != nullptr);

    // Get cached shader
    SDL_GPUShader* shader2 = shaderMgr->getShader(shaderPath, SDL_GPU_SHADERSTAGE_FRAGMENT, info);

    // Should return same pointer
    BOOST_CHECK(shader1 == shader2);
}

BOOST_FIXTURE_TEST_CASE(GetShaderReturnsNullForUnloaded, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    // Should return nullptr for shader not loaded
    SDL_GPUShader* shader = shaderMgr->getShader("res/shaders/not_loaded.vert",
        SDL_GPU_SHADERSTAGE_VERTEX,
        ShaderInfo{});
    BOOST_CHECK(shader == nullptr);
}

BOOST_FIXTURE_TEST_CASE(ShutdownClearsCachedShaders, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const std::string shaderPath = "res/shaders/color.vert";

    ShaderInfo info{};
    info.numUniformBuffers = 1;

    // Load shader
    shaderMgr->loadShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, info);
    BOOST_CHECK(shaderMgr->hasShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, info));

    // Shutdown clears cache
    shaderMgr->shutdown();
    BOOST_CHECK(!shaderMgr->hasShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, info));

    // Re-init for fixture cleanup
    BOOST_REQUIRE(shaderMgr->init(device->get()));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ShaderCacheKeyTests)

BOOST_FIXTURE_TEST_CASE(DifferentBindingLayoutsDoNotReuseCachedShader, ShaderTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const std::string shaderPath = "res/shaders/sprite.vert";

    ShaderInfo infoA{};
    infoA.numUniformBuffers = 1;

    ShaderInfo infoB{};
    infoB.numUniformBuffers = 2;

    SDL_GPUShader* shaderA = shaderMgr->loadShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, infoA);
    BOOST_REQUIRE(shaderA != nullptr);

    BOOST_CHECK(shaderMgr->hasShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, infoA));
    BOOST_CHECK(!shaderMgr->hasShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, infoB));
    BOOST_CHECK(shaderMgr->getShader(shaderPath, SDL_GPU_SHADERSTAGE_VERTEX, infoB) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SHADER PATH TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(ShaderPathTests)

BOOST_AUTO_TEST_CASE(PlatformShaderFilesExist) {
    VoidLight::ResourcePath::init();

    const std::string ext = VoidLight::GPUPlatformConfig::getShaderBinaryExtension();
    std::vector<std::string> shaderFiles = {
        "res/shaders/sprite.vert" + ext,
        "res/shaders/sprite.frag" + ext,
        "res/shaders/text_alpha.frag" + ext,
        "res/shaders/text_sdf.frag" + ext,
        "res/shaders/color.vert" + ext,
        "res/shaders/color.frag" + ext,
        "res/shaders/composite.vert" + ext,
        "res/shaders/composite.frag" + ext};

    for (const auto& path : shaderFiles) {
        const std::string resolvedPath = VoidLight::ResourcePath::resolve(path);
        const bool exists = VoidLight::ResourcePath::exists(path);

        BOOST_TEST_MESSAGE("Checking platform shader: " << resolvedPath);
        BOOST_CHECK_MESSAGE(exists, "Missing platform shader: " << resolvedPath);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SHADER MANAGER LIFECYCLE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(ShaderManagerLifecycleTests)

BOOST_FIXTURE_TEST_CASE(SingletonInstance, ShaderTestFixture) {
    GPUShaderManager& mgr1 = GPUShaderManager::Instance();
    GPUShaderManager& mgr2 = GPUShaderManager::Instance();

    BOOST_CHECK(&mgr1 == &mgr2);
}

BOOST_FIXTURE_TEST_CASE(InitWithNullDevice, ShaderTestFixture) {
    GPUShaderManager& mgr = GPUShaderManager::Instance();
    mgr.shutdown();

    // Init with null device should fail
    bool result = mgr.init(nullptr);
    BOOST_CHECK(!result);

    // Re-init with valid device for fixture cleanup
    if (device && device->isInitialized()) {
        BOOST_REQUIRE(mgr.init(device->get()));
    }
}

BOOST_FIXTURE_TEST_CASE(ShutdownWithoutInit, ShaderTestFixture) {
    GPUShaderManager& mgr = GPUShaderManager::Instance();
    mgr.shutdown();

    // Shutdown without init should be safe
    mgr.shutdown();

    // Re-init for fixture cleanup
    if (device && device->isInitialized()) {
        BOOST_REQUIRE(mgr.init(device->get()));
    }
}

BOOST_AUTO_TEST_SUITE_END()
