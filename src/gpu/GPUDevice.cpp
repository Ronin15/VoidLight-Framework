/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUDevice.hpp"
#include "gpu/GPUPlatformConfig.hpp"
#include "core/Logger.hpp"
#include <format>

namespace VoidLight {

GPUDevice& GPUDevice::Instance() {
    static GPUDevice instance;
    return instance;
}

GPUDevice::~GPUDevice() {
    shutdown();
}

bool GPUDevice::init(SDL_Window* window) {
    if (m_device) {
        GAMEENGINE_WARN("GPUDevice already initialized");
        return true;
    }

    if (!window) {
        GAMEENGINE_ERROR("GPUDevice::init called with null window");
        return false;
    }

    const SDL_GPUShaderFormat requestedFormats =
        GPUPlatformConfig::getRequestedShaderFormats();
    if (requestedFormats == SDL_GPU_SHADERFORMAT_INVALID) {
        GAMEENGINE_ERROR("GPUDevice::init: no shader formats were compiled for this platform");
        return false;
    }

    const char* const preferredDriver = GPUPlatformConfig::getPreferredDriverName();

    // Enable GPU validation in debug builds only; disabled elsewhere for performance.
    bool debugMode = false;
    VOIDLIGHT_DEBUG_ONLY(debugMode = true;)

    // Create the GPU device using the platform-native backend this build targets.
    m_device = SDL_CreateGPUDevice(
        requestedFormats,
        debugMode,
        preferredDriver
    );

    if (!m_device) {
        GAMEENGINE_ERROR(std::format("Failed to create GPU device for preferred driver {}: {}",
                                     preferredDriver, SDL_GetError()));
        return false;
    }

    // Claim window for swapchain
    if (!SDL_ClaimWindowForGPUDevice(m_device, window)) {
        GAMEENGINE_ERROR(std::format("Failed to claim window for GPU: {}", SDL_GetError()));
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
        return false;
    }

    m_window = window;

    // Configure swapchain with VSYNC (standard for gaming)
    // VSYNC: Waits for vertical blank, no tearing, predictable frame pacing
    bool swapchainConfigured = SDL_SetGPUSwapchainParameters(
        m_device, window,
        SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
        SDL_GPU_PRESENTMODE_VSYNC);

    if (!swapchainConfigured) {
        GAMEENGINE_WARN(std::format("Failed to configure swapchain VSYNC: {}",
                                     SDL_GetError()));
    }

    const char* driver = SDL_GetGPUDeviceDriver(m_device);
    SDL_GPUShaderFormat formats = getShaderFormats();

    GAMEENGINE_INFO("GPUDevice initialized successfully");
    GAMEENGINE_INFO(std::format("  Driver: {}", driver ? driver : "unknown"));
    GAMEENGINE_INFO(std::format("  Preferred driver: {}", preferredDriver));
    GAMEENGINE_INFO(std::format("  Present mode: {}", swapchainConfigured ? "VSYNC" : "default"));
    GAMEENGINE_INFO(std::format("  Requested shader formats: SPIRV={}, MSL={}, DXBC={}, DXIL={}",
        (requestedFormats & SDL_GPU_SHADERFORMAT_SPIRV) != 0,
        (requestedFormats & SDL_GPU_SHADERFORMAT_MSL) != 0,
        (requestedFormats & SDL_GPU_SHADERFORMAT_DXBC) != 0,
        (requestedFormats & SDL_GPU_SHADERFORMAT_DXIL) != 0));
    GAMEENGINE_INFO(std::format("  Shader formats: SPIRV={}, MSL={}, DXBC={}, DXIL={}",
        (formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0,
        (formats & SDL_GPU_SHADERFORMAT_MSL) != 0,
        (formats & SDL_GPU_SHADERFORMAT_DXBC) != 0,
        (formats & SDL_GPU_SHADERFORMAT_DXIL) != 0));

    return true;
}

void GPUDevice::shutdown() {
    if (m_device) {
        if (m_window) {
            SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            m_window = nullptr;
        }
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
        GAMEENGINE_INFO("GPUDevice shutdown complete");
    }
}

SDL_GPUShaderFormat GPUDevice::getShaderFormats() const {
    if (!m_device) {
        return SDL_GPU_SHADERFORMAT_INVALID;
    }
    return SDL_GetGPUShaderFormats(m_device);
}

SDL_GPUTextureFormat GPUDevice::getSwapchainFormat() const {
    if (!m_device || !m_window) {
        return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
    return SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
}

bool GPUDevice::supportsFormat(SDL_GPUTextureFormat format,
                               SDL_GPUTextureUsageFlags usage) const {
    if (!m_device) {
        return false;
    }
    return SDL_GPUTextureSupportsFormat(m_device, format,
                                        SDL_GPU_TEXTURETYPE_2D, usage);
}

const char* GPUDevice::getDriverName() const {
    if (!m_device) {
        return nullptr;
    }
    return SDL_GetGPUDeviceDriver(m_device);
}

} // namespace VoidLight
