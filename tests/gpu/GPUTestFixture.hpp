/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_TEST_FIXTURE_HPP
#define GPU_TEST_FIXTURE_HPP

#include <SDL3/SDL.h>
#include "gpu/GPUPlatformConfig.hpp"
#include "utils/ResourcePath.hpp"
#include <boost/test/unit_test.hpp>

namespace VoidLight {
namespace Test {

/**
 * Skip macro for tests requiring GPU availability.
 * Gracefully skips tests in headless CI environments.
 */
#define SKIP_IF_NO_GPU() \
    do { \
        if (!GPUTestFixture::isGPUAvailable()) { \
            BOOST_TEST_MESSAGE("Skipping test: No GPU available"); \
            return; \
        } \
    } while (0)

/**
 * Skip macro for tests requiring swapchain availability.
 * Prevents frame cycle tests from hanging when the compositor
 * cannot provide a swapchain (headless, minimized, etc.).
 */
#define SKIP_IF_NO_SWAPCHAIN() \
    do { \
        if (!GPUTestFixture::isSwapchainAvailable()) { \
            BOOST_TEST_MESSAGE("Skipping test: Swapchain not available"); \
            return; \
        } \
    } while (0)

/**
 * Common test fixture for GPU tests.
 * Handles SDL initialization and GPU device availability detection.
 *
 * GPU availability is checked by creating a temporary device (no window
 * claim). Swapchain availability is set by RendererTestFixture after it
 * initializes the real device and probes the swapchain — this avoids
 * creating throwaway devices that claim the shared test window.
 */
class GPUTestFixture {
public:
    GPUTestFixture() {
        if (!s_sdlInitialized) {
            if (!SDL_Init(SDL_INIT_VIDEO)) {
                BOOST_TEST_MESSAGE("SDL video initialization failed: " << SDL_GetError());
                s_gpuAvailable = false;
            } else {
                s_sdlInitialized = true;
                VoidLight::ResourcePath::init();
                checkGPUAvailability();
            }
        }
    }

    virtual ~GPUTestFixture() {
        // SDL cleanup handled by global fixture
    }

    static bool isGPUAvailable() {
        return s_gpuAvailable;
    }

    static bool isSwapchainAvailable() {
        return s_swapchainAvailable;
    }

    /**
     * Set swapchain availability. Called by RendererTestFixture after
     * probing with the real device — never by the base fixture.
     */
    static void setSwapchainAvailable(bool available) {
        s_swapchainAvailable = available;
    }

    /**
     * Get the test window (creates if needed).
     * Window is hidden and minimal size for testing.
     */
    static SDL_Window* getTestWindow() {
        if (!s_testWindow && s_sdlInitialized) {
            s_testWindow = SDL_CreateWindow(
                "GPU Test Window",
                64, 64,
                SDL_WINDOW_HIDDEN);
            if (!s_testWindow) {
                BOOST_TEST_MESSAGE("Failed to create test window: " << SDL_GetError());
            }
        }
        return s_testWindow;
    }

    /**
     * Clean up test resources.
     */
    static void cleanup() {
        if (s_testWindow) {
            SDL_DestroyWindow(s_testWindow);
            s_testWindow = nullptr;
        }
        if (s_sdlInitialized) {
            SDL_Quit();
            s_sdlInitialized = false;
        }
        s_gpuAvailable = false;
        s_swapchainAvailable = false;
    }

protected:
    /**
     * Check if a GPU device can be created on this system.
     * Creates a temporary device (no window claim) and destroys it.
     */
    static void checkGPUAvailability() {
        SDL_GPUDevice* device = SDL_CreateGPUDevice(
            VoidLight::GPUPlatformConfig::getRequestedShaderFormats(),
            false,
            VoidLight::GPUPlatformConfig::getPreferredDriverName());

        if (!device) {
            BOOST_TEST_MESSAGE("Cannot create GPU device: " << SDL_GetError());
            s_gpuAvailable = false;
            return;
        }

        SDL_DestroyGPUDevice(device);
        s_gpuAvailable = true;
        BOOST_TEST_MESSAGE("GPU is available for testing");
    }

    static inline bool s_sdlInitialized = false;
    static inline bool s_gpuAvailable = false;
    static inline bool s_swapchainAvailable = false;
    static inline SDL_Window* s_testWindow = nullptr;
};

/**
 * Global fixture for GPU test cleanup.
 * Ensures SDL resources are cleaned up after all tests.
 */
struct GPUGlobalFixture {
    ~GPUGlobalFixture() {
        GPUTestFixture::cleanup();
    }
};

} // namespace Test
} // namespace VoidLight

#endif // GPU_TEST_FIXTURE_HPP
