/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUBuffer.hpp"
#include "core/Logger.hpp"
#include <format>

namespace VoidLight {

GPUBuffer::GPUBuffer(SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage, uint32_t size)
    : m_device(device)
    , m_size(size)
    , m_usage(usage) {
    if (!device) {
        GAMEENGINE_ERROR("GPUBuffer: null device");
        return;
    }

    if (size == 0) {
        GAMEENGINE_ERROR("GPUBuffer: invalid size 0");
        return;
    }

    SDL_GPUBufferCreateInfo createInfo{};
    createInfo.usage = usage;
    createInfo.size = size;

    m_buffer = SDL_CreateGPUBuffer(device, &createInfo);

    if (!m_buffer) {
        GAMEENGINE_ERROR(std::format("Failed to create GPU buffer ({} bytes): {}",
            size, SDL_GetError()));
    }
}

GPUBuffer::~GPUBuffer() {
    release();
}

GPUBuffer::GPUBuffer(GPUBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_device(other.m_device)
    , m_size(other.m_size)
    , m_usage(other.m_usage) {
    other.m_buffer = nullptr;
    other.m_device = nullptr;
    other.m_size = 0;
}

GPUBuffer& GPUBuffer::operator=(GPUBuffer&& other) noexcept {
    if (this != &other) {
        release();

        m_buffer = other.m_buffer;
        m_device = other.m_device;
        m_size = other.m_size;
        m_usage = other.m_usage;

        other.m_buffer = nullptr;
        other.m_device = nullptr;
        other.m_size = 0;
    }
    return *this;
}

void GPUBuffer::release() {
    if (m_buffer && m_device) {
        SDL_ReleaseGPUBuffer(m_device, m_buffer);
    }
    m_buffer = nullptr;
    m_device = nullptr;
    m_size = 0;
}

SDL_GPUBufferBinding GPUBuffer::asBinding(uint32_t offset) const {
    if (!m_buffer) {
        GAMEENGINE_WARN("GPUBuffer::asBinding() called on invalid buffer");
    }
    SDL_GPUBufferBinding binding{};
    binding.buffer = m_buffer;
    binding.offset = offset;
    return binding;
}

SDL_GPUBufferRegion GPUBuffer::asRegion(uint32_t offset, uint32_t size) const {
    if (!m_buffer) {
        GAMEENGINE_WARN("GPUBuffer::asRegion() called on invalid buffer");
    }
    if (offset > m_size) {
        GAMEENGINE_WARN(std::format("GPUBuffer::asRegion() offset {} exceeds buffer size {}", offset, m_size));
        offset = m_size;
    }

    SDL_GPUBufferRegion region{};
    region.buffer = m_buffer;
    region.offset = offset;

    if (size == 0) {
        region.size = m_size - offset;
    } else {
        // Validate that offset + size doesn't exceed buffer size
        if (offset + size > m_size) {
            GAMEENGINE_WARN(std::format("GPUBuffer::asRegion() region [{}, {}] exceeds buffer size {}, clamping",
                offset, offset + size, m_size));
            region.size = m_size - offset;
        } else {
            region.size = size;
        }
    }

    return region;
}

} // namespace VoidLight
