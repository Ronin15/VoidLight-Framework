/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUTransferBuffer.hpp"
#include "core/Logger.hpp"
#include <format>

namespace VoidLight {

GPUTransferBuffer::GPUTransferBuffer(SDL_GPUDevice* device,
    SDL_GPUTransferBufferUsage usage,
    uint32_t size)
    : m_device(device)
    , m_size(size) {
    if (!device) {
        GAMEENGINE_ERROR("GPUTransferBuffer: null device");
        return;
    }

    if (size == 0) {
        GAMEENGINE_ERROR("GPUTransferBuffer: invalid size 0");
        return;
    }

    SDL_GPUTransferBufferCreateInfo createInfo{};
    createInfo.usage = usage;
    createInfo.size = size;

    m_buffer = SDL_CreateGPUTransferBuffer(device, &createInfo);

    if (!m_buffer) {
        GAMEENGINE_ERROR(std::format("Failed to create GPU transfer buffer ({} bytes): {}",
            size, SDL_GetError()));
    }
}

GPUTransferBuffer::~GPUTransferBuffer() {
    release();
}

GPUTransferBuffer::GPUTransferBuffer(GPUTransferBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_device(other.m_device)
    , m_size(other.m_size)
    , m_mapped(other.m_mapped) {
    other.m_buffer = nullptr;
    other.m_device = nullptr;
    other.m_size = 0;
    other.m_mapped = false;
}

GPUTransferBuffer& GPUTransferBuffer::operator=(GPUTransferBuffer&& other) noexcept {
    if (this != &other) {
        release();

        m_buffer = other.m_buffer;
        m_device = other.m_device;
        m_size = other.m_size;
        m_mapped = other.m_mapped;

        other.m_buffer = nullptr;
        other.m_device = nullptr;
        other.m_size = 0;
        other.m_mapped = false;
    }
    return *this;
}

void GPUTransferBuffer::release() {
    if (m_buffer && m_device) {
        if (m_mapped) {
            unmap();
        }
        SDL_ReleaseGPUTransferBuffer(m_device, m_buffer);
    }
    m_buffer = nullptr;
    m_device = nullptr;
    m_size = 0;
    m_mapped = false;
}

void* GPUTransferBuffer::map(bool cycle) {
    if (!m_buffer || !m_device) {
        GAMEENGINE_ERROR("GPUTransferBuffer::map: invalid buffer");
        return nullptr;
    }

    if (m_mapped) {
        GAMEENGINE_WARN("GPUTransferBuffer::map: already mapped");
        return nullptr;
    }

    void* ptr = SDL_MapGPUTransferBuffer(m_device, m_buffer, cycle);

    if (!ptr) {
        GAMEENGINE_ERROR(std::format("Failed to map GPU transfer buffer: {}", SDL_GetError()));
        return nullptr;
    }

    m_mapped = true;
    return ptr;
}

void GPUTransferBuffer::unmap() {
    if (!m_buffer || !m_device) {
        return;
    }

    if (!m_mapped) {
        return;
    }

    SDL_UnmapGPUTransferBuffer(m_device, m_buffer);
    m_mapped = false;
}

SDL_GPUTransferBufferLocation GPUTransferBuffer::asLocation(uint32_t offset) const {
    SDL_GPUTransferBufferLocation location{};
    location.transfer_buffer = m_buffer;
    location.offset = offset;
    return location;
}

} // namespace VoidLight
