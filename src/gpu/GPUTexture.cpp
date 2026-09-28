/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUTexture.hpp"
#include "core/Logger.hpp"
#include <format>

namespace VoidLight {

GPUTexture::GPUTexture(SDL_GPUDevice* device, uint32_t width, uint32_t height,
    SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage,
    uint32_t numLevels)
    : m_device(device)
    , m_width(width)
    , m_height(height)
    , m_format(format)
    , m_usage(usage) {
    if (!device) {
        GAMEENGINE_ERROR("GPUTexture: null device");
        return;
    }

    if (width == 0 || height == 0) {
        GAMEENGINE_ERROR(std::format("GPUTexture: invalid dimensions {}x{}", width, height));
        return;
    }

    SDL_GPUTextureCreateInfo createInfo{};
    createInfo.type = SDL_GPU_TEXTURETYPE_2D;
    createInfo.format = format;
    createInfo.usage = usage;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = numLevels;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    m_texture = SDL_CreateGPUTexture(device, &createInfo);

    if (!m_texture) {
        GAMEENGINE_ERROR(std::format("Failed to create GPU texture {}x{}: {}",
            width, height, SDL_GetError()));
    }
}

GPUTexture::~GPUTexture() {
    release();
}

GPUTexture::GPUTexture(GPUTexture&& other) noexcept
    : m_texture(other.m_texture)
    , m_device(other.m_device)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_format(other.m_format)
    , m_usage(other.m_usage) {
    other.m_texture = nullptr;
    other.m_device = nullptr;
    other.m_width = 0;
    other.m_height = 0;
}

GPUTexture& GPUTexture::operator=(GPUTexture&& other) noexcept {
    if (this != &other) {
        release();

        m_texture = other.m_texture;
        m_device = other.m_device;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_usage = other.m_usage;

        other.m_texture = nullptr;
        other.m_device = nullptr;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void GPUTexture::release() {
    if (m_texture && m_device) {
        SDL_ReleaseGPUTexture(m_device, m_texture);
    }
    m_texture = nullptr;
    m_device = nullptr;
    m_width = 0;
    m_height = 0;
}

SDL_GPUColorTargetInfo GPUTexture::asColorTarget(
    SDL_GPULoadOp loadOp,
    SDL_FColor clearColor,
    SDL_GPUStoreOp storeOp) const {
    SDL_GPUColorTargetInfo info{};
    info.texture = m_texture;
    info.mip_level = 0;
    info.layer_or_depth_plane = 0;
    info.clear_color = clearColor;
    info.load_op = loadOp;
    info.store_op = storeOp;
    info.cycle = false;
    return info;
}

SDL_GPUTextureSamplerBinding GPUTexture::asSamplerBinding(SDL_GPUSampler* sampler) const {
    if (!m_texture) {
        GAMEENGINE_WARN("GPUTexture::asSamplerBinding() called on invalid texture");
    }
    if (!sampler) {
        GAMEENGINE_WARN("GPUTexture::asSamplerBinding() called with null sampler");
    }
    SDL_GPUTextureSamplerBinding binding{};
    binding.texture = m_texture;
    binding.sampler = sampler;
    return binding;
}

} // namespace VoidLight
