/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_SAMPLER_HPP
#define GPU_SAMPLER_HPP

#include <SDL3/SDL_gpu.h>

namespace VoidLight {

/**
 * RAII wrapper for SDL_GPUSampler.
 *
 * Provides preset samplers (nearest, linear) and custom sampler creation.
 */
class GPUSampler {
public:
    GPUSampler() = default;

    /**
     * Create sampler with specified filter modes.
     * @param device GPU device
     * @param minMagFilter Filter mode for min/mag filtering
     * @param addressMode Address mode for U/V coordinates
     */
    GPUSampler(SDL_GPUDevice* device, SDL_GPUFilter minMagFilter,
        SDL_GPUSamplerAddressMode addressMode = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);

    /**
     * Create sampler with full control over all parameters.
     */
    GPUSampler(SDL_GPUDevice* device, const SDL_GPUSamplerCreateInfo& createInfo);

    ~GPUSampler();

    // Move-only
    GPUSampler(GPUSampler&& other) noexcept;
    GPUSampler& operator=(GPUSampler&& other) noexcept;
    GPUSampler(const GPUSampler&) = delete;
    GPUSampler& operator=(const GPUSampler&) = delete;

    SDL_GPUSampler* get() const { return m_sampler; }
    bool isValid() const { return m_sampler != nullptr; }

    /**
     * Create a nearest-neighbor sampler (pixel-perfect for 2D).
     */
    static GPUSampler createNearest(SDL_GPUDevice* device);

    /**
     * Create a linear filtering sampler (smooth for zoom).
     */
    static GPUSampler createLinear(SDL_GPUDevice* device);

    /**
     * Create a linear sampler with mipmapping.
     */
    static GPUSampler createLinearMipmapped(SDL_GPUDevice* device);

private:
    void release();

    SDL_GPUSampler* m_sampler{nullptr};
    SDL_GPUDevice* m_device{nullptr};
};

} // namespace VoidLight

#endif // GPU_SAMPLER_HPP
