/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_DEVICE_HPP
#define GPU_DEVICE_HPP

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>

namespace VoidLight {

/**
 * Singleton wrapper for SDL_GPUDevice.
 *
 * Manages GPU device lifecycle and window swapchain claim.
 * Must be initialized AFTER SDL_CreateWindow, BEFORE any GPU rendering.
 */
class GPUDevice {
public:
    static GPUDevice& Instance();

    /**
     * Initialize GPU device and claim window for swapchain.
     * @param window SDL window to claim for GPU rendering
     * @return true on success, false on failure
     */
    [[nodiscard]] bool init(SDL_Window* window);

    /**
     * Shutdown GPU device and release window claim.
     */
    void shutdown();

    // Accessors
    SDL_GPUDevice* get() const { return m_device; }
    SDL_Window* getWindow() const { return m_window; }
    bool isInitialized() const { return m_device != nullptr; }

    /**
     * Get supported shader formats for this device.
     */
    SDL_GPUShaderFormat getShaderFormats() const;

    /**
     * Get the swapchain texture format for the claimed window.
     */
    SDL_GPUTextureFormat getSwapchainFormat() const;

    /**
     * Query if a texture format is supported with given usage flags.
     */
    bool supportsFormat(SDL_GPUTextureFormat format,
        SDL_GPUTextureUsageFlags usage) const;

    /**
     * Get the device driver name (e.g., "vulkan", "metal", "direct3d12").
     */
    const char* getDriverName() const;

private:
    GPUDevice() = default;
    ~GPUDevice();

    // Non-copyable, non-movable
    GPUDevice(const GPUDevice&) = delete;
    GPUDevice& operator=(const GPUDevice&) = delete;
    GPUDevice(GPUDevice&&) = delete;
    GPUDevice& operator=(GPUDevice&&) = delete;

    SDL_GPUDevice* m_device{nullptr};
    SDL_Window* m_window{nullptr};
};

} // namespace VoidLight

#endif // GPU_DEVICE_HPP
