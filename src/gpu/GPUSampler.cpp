/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUSampler.hpp"
#include "core/Logger.hpp"
#include <format>

namespace VoidLight {

GPUSampler::GPUSampler(SDL_GPUDevice* device, SDL_GPUFilter minMagFilter,
    SDL_GPUSamplerAddressMode addressMode)
    : m_device(device) {
    if (!device) {
        GAMEENGINE_ERROR("GPUSampler: null device");
        return;
    }

    SDL_GPUSamplerCreateInfo createInfo{};
    createInfo.min_filter = minMagFilter;
    createInfo.mag_filter = minMagFilter;
    createInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    createInfo.address_mode_u = addressMode;
    createInfo.address_mode_v = addressMode;
    createInfo.address_mode_w = addressMode;
    createInfo.mip_lod_bias = 0.0f;
    createInfo.max_anisotropy = 1.0f;
    createInfo.compare_op = SDL_GPU_COMPAREOP_NEVER;
    createInfo.min_lod = 0.0f;
    createInfo.max_lod = 1000.0f;
    createInfo.enable_anisotropy = false;
    createInfo.enable_compare = false;

    m_sampler = SDL_CreateGPUSampler(device, &createInfo);

    if (!m_sampler) {
        GAMEENGINE_ERROR(std::format("Failed to create GPU sampler: {}", SDL_GetError()));
    }
}

GPUSampler::GPUSampler(SDL_GPUDevice* device, const SDL_GPUSamplerCreateInfo& createInfo)
    : m_device(device) {
    if (!device) {
        GAMEENGINE_ERROR("GPUSampler: null device");
        return;
    }

    m_sampler = SDL_CreateGPUSampler(device, &createInfo);

    if (!m_sampler) {
        GAMEENGINE_ERROR(std::format("Failed to create GPU sampler: {}", SDL_GetError()));
    }
}

GPUSampler::~GPUSampler() {
    release();
}

GPUSampler::GPUSampler(GPUSampler&& other) noexcept
    : m_sampler(other.m_sampler)
    , m_device(other.m_device) {
    other.m_sampler = nullptr;
    other.m_device = nullptr;
}

GPUSampler& GPUSampler::operator=(GPUSampler&& other) noexcept {
    if (this != &other) {
        release();

        m_sampler = other.m_sampler;
        m_device = other.m_device;

        other.m_sampler = nullptr;
        other.m_device = nullptr;
    }
    return *this;
}

void GPUSampler::release() {
    if (m_sampler && m_device) {
        SDL_ReleaseGPUSampler(m_device, m_sampler);
    }
    m_sampler = nullptr;
    m_device = nullptr;
}

GPUSampler GPUSampler::createNearest(SDL_GPUDevice* device) {
    return GPUSampler(device, SDL_GPU_FILTER_NEAREST,
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);
}

GPUSampler GPUSampler::createLinear(SDL_GPUDevice* device) {
    return GPUSampler(device, SDL_GPU_FILTER_LINEAR,
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);
}

GPUSampler GPUSampler::createLinearMipmapped(SDL_GPUDevice* device) {
    if (!device) {
        return GPUSampler();
    }

    SDL_GPUSamplerCreateInfo createInfo{};
    createInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    createInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    createInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    createInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    createInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    createInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    createInfo.mip_lod_bias = 0.0f;
    createInfo.max_anisotropy = 1.0f;
    createInfo.compare_op = SDL_GPU_COMPAREOP_NEVER;
    createInfo.min_lod = 0.0f;
    createInfo.max_lod = 1000.0f;
    createInfo.enable_anisotropy = false;
    createInfo.enable_compare = false;

    return GPUSampler(device, createInfo);
}

} // namespace VoidLight
