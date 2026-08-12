/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_VERTEX_POOL_HPP
#define GPU_VERTEX_POOL_HPP

#include "gpu/GPUBuffer.hpp"
#include "gpu/GPUTransferBuffer.hpp"
#include <array>
#include <cstdint>

namespace VoidLight {

/**
 * Triple-buffered vertex pool for zero-allocation per-frame rendering.
 *
 * Uses cycling transfer buffers to avoid GPU stalls:
 * - Frame N: GPU reads from buffer 0
 * - Frame N+1: GPU reads from buffer 1, CPU writes to buffer 0
 * - Frame N+2: GPU reads from buffer 2, CPU writes to buffer 1
 *
 * This allows CPU vertex generation to overlap with GPU rendering.
 */
class GPUVertexPool {
public:
    static constexpr size_t FRAME_COUNT = 3;
    static constexpr size_t DEFAULT_VERTEX_CAPACITY = 150000;  // 4K + zoom headroom

    GPUVertexPool() = default;
    ~GPUVertexPool() = default;

    // Move-only (explicit to properly clear source state)
    GPUVertexPool(GPUVertexPool&& other) noexcept;
    GPUVertexPool& operator=(GPUVertexPool&& other) noexcept;
    GPUVertexPool(const GPUVertexPool&) = delete;
    GPUVertexPool& operator=(const GPUVertexPool&) = delete;

    /**
     * Initialize the vertex pool.
     * @param device GPU device
     * @param vertexSize Size of a single vertex in bytes
     * @param maxVertices Maximum number of vertices to support
     * @return true on success
     */
    [[nodiscard]] bool init(SDL_GPUDevice* device, uint32_t vertexSize,
              size_t maxVertices = DEFAULT_VERTEX_CAPACITY);

    /**
     * Shutdown and release all buffers.
     */
    void shutdown();

    /**
     * Begin a new frame. Advances the frame index and maps the transfer buffer.
     * @return Pointer to mapped memory for writing vertices
     */
    void* beginFrame();

    /**
     * End the current frame. Unmaps the buffer and records vertex count.
     * @param vertexCount Number of vertices written this frame
     */
    void endFrame(size_t vertexCount);

    /**
     * Upload vertex data to the GPU buffer.
     * Must be called during a copy pass. No-op when there is nothing to upload.
     * @param copyPass Active copy pass
     */
    void upload(SDL_GPUCopyPass* copyPass);

    // Accessors
    SDL_GPUBuffer* getGPUBuffer() const { return m_gpuBuffers[m_frameIndex].get(); }
    size_t getVertexCount() const { return m_currentVertexCount; }
    size_t getMaxVertices() const { return m_maxVertices; }
    uint32_t getVertexSize() const { return m_vertexSize; }
    bool isInitialized() const { return m_device != nullptr; }

    /**
     * Get the currently mapped pointer for vertex writes.
     * Only valid between beginFrame() and endFrame().
     * @return Mapped memory pointer, or nullptr if not mapped
     */
    void* getMappedPtr() const { return m_mappedPtr; }

    /**
     * Set the vertex count for manual vertex writing.
     * Call this if writing vertices directly to getMappedPtr() instead of using SpriteBatch.
     * @param count Number of vertices written
     */
    void setWrittenVertexCount(size_t count) { m_pendingVertexCount = count; }

    /**
     * Get the pending vertex count set by setWrittenVertexCount().
     * @return Pending vertex count, or 0 if not set
     */
    size_t getPendingVertexCount() const { return m_pendingVertexCount; }

private:
    SDL_GPUDevice* m_device{nullptr};

    // CPU-side staging (triple-buffered)
    std::array<GPUTransferBuffer, FRAME_COUNT> m_transferBuffers;

    // GPU-side buffers are triple-buffered to avoid overwriting in-flight vertices.
    std::array<GPUBuffer, FRAME_COUNT> m_gpuBuffers;

    uint32_t m_frameIndex{0};
    uint32_t m_vertexSize{0};
    size_t m_maxVertices{0};
    size_t m_currentVertexCount{0};
    size_t m_pendingVertexCount{0};
    void* m_mappedPtr{nullptr};
};

} // namespace VoidLight

#endif // GPU_VERTEX_POOL_HPP
