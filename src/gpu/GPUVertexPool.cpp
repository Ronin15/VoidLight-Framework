/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUVertexPool.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <format>

namespace VoidLight {

bool GPUVertexPool::init(SDL_GPUDevice* device, uint32_t vertexSize, size_t maxVertices) {
    if (!device) {
        GAMEENGINE_ERROR("GPUVertexPool::init: null device");
        return false;
    }

    if (vertexSize == 0 || maxVertices == 0) {
        GAMEENGINE_ERROR(std::format("GPUVertexPool::init: invalid parameters (vertexSize={}, maxVertices={})",
                         vertexSize, maxVertices));
        return false;
    }

    m_device = device;
    m_vertexSize = vertexSize;
    m_maxVertices = maxVertices;

    const size_t bufferSize64 = static_cast<size_t>(vertexSize) * maxVertices;
    if (bufferSize64 > UINT32_MAX) {
        GAMEENGINE_ERROR(std::format(
            "GPUVertexPool::init: buffer size {} exceeds SDL GPU 32-bit limit",
            bufferSize64));
        return false;
    }

    uint32_t bufferSize = static_cast<uint32_t>(bufferSize64);

    // Create triple-buffered transfer buffers
    for (size_t i = 0; i < FRAME_COUNT; ++i) {
        m_transferBuffers[i] = GPUTransferBuffer(device,
            SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bufferSize);

        if (!m_transferBuffers[i].isValid()) {
            GAMEENGINE_ERROR(std::format("GPUVertexPool: failed to create transfer buffer {}", i));
            shutdown();
            return false;
        }
    }

    // Create triple-buffered GPU buffers to avoid overwriting in-flight vertex data.
    for (size_t i = 0; i < FRAME_COUNT; ++i) {
        m_gpuBuffers[i] = GPUBuffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, bufferSize);

        if (!m_gpuBuffers[i].isValid()) {
            GAMEENGINE_ERROR(std::format("GPUVertexPool: failed to create GPU vertex buffer {}", i));
            shutdown();
            return false;
        }
    }

    GAMEENGINE_INFO(std::format("GPUVertexPool initialized: {} vertices x {} bytes = {} KB",
                    maxVertices, vertexSize, bufferSize / 1024));
    return true;
}

void GPUVertexPool::shutdown() {
    std::generate(m_gpuBuffers.begin(), m_gpuBuffers.end(),
                  []() { return GPUBuffer(); });

    std::generate(m_transferBuffers.begin(), m_transferBuffers.end(),
                  []() { return GPUTransferBuffer(); });

    m_device = nullptr;
    m_frameIndex = 0;
    m_currentVertexCount = 0;
    m_mappedPtr = nullptr;
}

GPUVertexPool::GPUVertexPool(GPUVertexPool&& other) noexcept
    : m_device(other.m_device)
    , m_transferBuffers(std::move(other.m_transferBuffers))
    , m_gpuBuffers(std::move(other.m_gpuBuffers))
    , m_frameIndex(other.m_frameIndex)
    , m_vertexSize(other.m_vertexSize)
    , m_maxVertices(other.m_maxVertices)
    , m_currentVertexCount(other.m_currentVertexCount)
    , m_pendingVertexCount(other.m_pendingVertexCount)
    , m_mappedPtr(other.m_mappedPtr)
{
    // Clear source state to prevent double-cleanup
    other.m_device = nullptr;
    other.m_frameIndex = 0;
    other.m_vertexSize = 0;
    other.m_maxVertices = 0;
    other.m_currentVertexCount = 0;
    other.m_pendingVertexCount = 0;
    other.m_mappedPtr = nullptr;
}

GPUVertexPool& GPUVertexPool::operator=(GPUVertexPool&& other) noexcept {
    if (this != &other) {
        // Release current resources
        shutdown();

        // Move from other
        m_device = other.m_device;
        m_transferBuffers = std::move(other.m_transferBuffers);
        m_gpuBuffers = std::move(other.m_gpuBuffers);
        m_frameIndex = other.m_frameIndex;
        m_vertexSize = other.m_vertexSize;
        m_maxVertices = other.m_maxVertices;
        m_currentVertexCount = other.m_currentVertexCount;
        m_pendingVertexCount = other.m_pendingVertexCount;
        m_mappedPtr = other.m_mappedPtr;

        // Clear source state
        other.m_device = nullptr;
        other.m_frameIndex = 0;
        other.m_vertexSize = 0;
        other.m_maxVertices = 0;
        other.m_currentVertexCount = 0;
        other.m_pendingVertexCount = 0;
        other.m_mappedPtr = nullptr;
    }
    return *this;
}

void* GPUVertexPool::beginFrame() {
    if (!m_device) {
        GAMEENGINE_ERROR("GPUVertexPool::beginFrame: not initialized");
        return nullptr;
    }

    // Advance to next frame's transfer buffer
    m_frameIndex = (m_frameIndex + 1) % FRAME_COUNT;
    m_currentVertexCount = 0;
    m_pendingVertexCount = 0;

    // Map with cycle=true to handle if previous frame's upload is still in flight
    m_mappedPtr = m_transferBuffers[m_frameIndex].map(true);

    if (!m_mappedPtr) {
        GAMEENGINE_ERROR("GPUVertexPool::beginFrame: failed to map transfer buffer");
    }

    return m_mappedPtr;
}

void GPUVertexPool::endFrame(size_t vertexCount) {
    if (!m_device) {
        return;
    }

    if (vertexCount > m_maxVertices) {
        GAMEENGINE_WARN(std::format("GPUVertexPool::endFrame: vertex count {} exceeds max {}, clamping",
                        vertexCount, m_maxVertices));
        vertexCount = m_maxVertices;
    }

    m_currentVertexCount = vertexCount;
    m_transferBuffers[m_frameIndex].unmap();
    m_mappedPtr = nullptr;
}

void GPUVertexPool::upload(SDL_GPUCopyPass* copyPass) {
    if (!copyPass || m_currentVertexCount == 0) {
        return;
    }

    SDL_GPUTransferBufferLocation src = m_transferBuffers[m_frameIndex].asLocation(0);

    SDL_GPUBufferRegion dst{};
    dst.buffer = m_gpuBuffers[m_frameIndex].get();
    dst.offset = 0;
    dst.size = static_cast<uint32_t>(m_currentVertexCount * m_vertexSize);

    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
}

} // namespace VoidLight
