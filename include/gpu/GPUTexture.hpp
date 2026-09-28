/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_TEXTURE_HPP
#define GPU_TEXTURE_HPP

#include <SDL3/SDL_gpu.h>
#include <cstdint>

namespace VoidLight {

/**
 * RAII wrapper for SDL_GPUTexture.
 *
 * Handles texture creation, destruction, and provides helper methods
 * for render target usage.
 */
class GPUTexture {
public:
    GPUTexture() = default;

    /**
     * Create a 2D texture with specified parameters.
     * @param device GPU device
     * @param width Texture width in pixels
     * @param height Texture height in pixels
     * @param format Pixel format
     * @param usage Usage flags (SAMPLER, COLOR_TARGET, etc.)
     * @param numLevels Number of mip levels (1 for no mipmaps)
     */
    GPUTexture(SDL_GPUDevice* device, uint32_t width, uint32_t height,
        SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage,
        uint32_t numLevels = 1);

    ~GPUTexture();

    // Move-only (no copy)
    GPUTexture(GPUTexture&& other) noexcept;
    GPUTexture& operator=(GPUTexture&& other) noexcept;
    GPUTexture(const GPUTexture&) = delete;
    GPUTexture& operator=(const GPUTexture&) = delete;

    // Accessors
    SDL_GPUTexture* get() const { return m_texture; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    SDL_GPUTextureFormat getFormat() const { return m_format; }
    SDL_GPUTextureUsageFlags getUsage() const { return m_usage; }
    bool isValid() const { return m_texture != nullptr; }

    bool isRenderTarget() const {
        return (m_usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) != 0;
    }

    bool isSampler() const {
        return (m_usage & SDL_GPU_TEXTUREUSAGE_SAMPLER) != 0;
    }

    /**
     * Create color target info for use in render passes.
     * @param loadOp Load operation (CLEAR, LOAD, DONT_CARE)
     * @param clearColor Clear color if loadOp is CLEAR
     * @param storeOp Store operation (STORE, DONT_CARE)
     * @return Configured color target info
     */
    SDL_GPUColorTargetInfo asColorTarget(
        SDL_GPULoadOp loadOp = SDL_GPU_LOADOP_CLEAR,
        SDL_FColor clearColor = {0.0f, 0.0f, 0.0f, 0.0f},
        SDL_GPUStoreOp storeOp = SDL_GPU_STOREOP_STORE) const;

    /**
     * Create texture-sampler binding for shader binding.
     */
    SDL_GPUTextureSamplerBinding asSamplerBinding(SDL_GPUSampler* sampler) const;

private:
    void release();

    SDL_GPUTexture* m_texture{nullptr};
    SDL_GPUDevice* m_device{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    SDL_GPUTextureFormat m_format{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUTextureUsageFlags m_usage{0};
};

} // namespace VoidLight

#endif // GPU_TEXTURE_HPP
