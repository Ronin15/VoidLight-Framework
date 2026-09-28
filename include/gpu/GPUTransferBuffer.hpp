/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_TRANSFER_BUFFER_HPP
#define GPU_TRANSFER_BUFFER_HPP

#include <SDL3/SDL_gpu.h>
#include <cstdint>

namespace VoidLight {

/**
 * RAII wrapper for SDL_GPUTransferBuffer.
 *
 * Used for CPU-to-GPU data transfers (uploads) and GPU-to-CPU reads.
 * Supports mapping/unmapping for direct CPU access.
 */
class GPUTransferBuffer {
public:
    GPUTransferBuffer() = default;

    /**
     * Create a transfer buffer.
     * @param device GPU device
     * @param usage UPLOAD for CPU->GPU, DOWNLOAD for GPU->CPU
     * @param size Buffer size in bytes
     */
    GPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBufferUsage usage,
        uint32_t size);

    ~GPUTransferBuffer();

    // Move-only
    GPUTransferBuffer(GPUTransferBuffer&& other) noexcept;
    GPUTransferBuffer& operator=(GPUTransferBuffer&& other) noexcept;
    GPUTransferBuffer(const GPUTransferBuffer&) = delete;
    GPUTransferBuffer& operator=(const GPUTransferBuffer&) = delete;

    SDL_GPUTransferBuffer* get() const { return m_buffer; }
    uint32_t getSize() const { return m_size; }
    bool isValid() const { return m_buffer != nullptr; }
    bool isMapped() const { return m_mapped; }

    /**
     * Map the buffer for CPU access.
     * @param cycle If true, allows reusing the buffer before previous operations complete
     * @return Pointer to mapped memory, or nullptr on failure
     */
    void* map(bool cycle = true);

    /**
     * Unmap the buffer. Must be called before using in copy operations.
     */
    void unmap();

    /**
     * Create transfer buffer location for copy operations.
     * @param offset Byte offset into the buffer
     */
    SDL_GPUTransferBufferLocation asLocation(uint32_t offset = 0) const;

private:
    void release();

    SDL_GPUTransferBuffer* m_buffer{nullptr};
    SDL_GPUDevice* m_device{nullptr};
    uint32_t m_size{0};
    bool m_mapped{false};
};

} // namespace VoidLight

#endif // GPU_TRANSFER_BUFFER_HPP
