/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef SPRITE_BATCH_HPP
#define SPRITE_BATCH_HPP

#include "gpu/GPUTypes.hpp"
#include "gpu/GPUBuffer.hpp"
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <vector>

namespace VoidLight {

/**
 * Batched sprite renderer for GPU rendering.
 *
 * Works with GPURenderer's vertex pool system:
 * 1. During beginFrame(), vertex pool is mapped
 * 2. SpriteBatch writes sprites to the mapped buffer
 * 3. During GPURenderer::beginScenePass(), vertices are uploaded via the frame copy pass
 * 4. During render pass, draw calls are issued
 *
 * Usage:
 *   // Before render pass (during vertex recording phase)
 *   batch.begin(texture, sampler, texWidth, texHeight);
 *   batch.draw(srcRect, dstRect);
 *   batch.draw(srcRect, dstRect);
 *   size_t vertexCount = batch.end();  // Returns vertex count for pool
 *
 *   // During render pass
 *   batch.render(pass, pipeline, vertexPool);
 */
class SpriteBatch {
public:
    // 4K @ 32px = 120x68 tiles = 8160 per layer, 2 layers + NPCs + effects = ~40k sprites
    static constexpr size_t MAX_SPRITES = 50000;
    static constexpr size_t VERTICES_PER_SPRITE = 4;
    static constexpr size_t INDICES_PER_SPRITE = 6;
    static constexpr size_t MAX_VERTICES = MAX_SPRITES * VERTICES_PER_SPRITE;
    static constexpr size_t MAX_INDICES = MAX_SPRITES * INDICES_PER_SPRITE;

    SpriteBatch() = default;
    ~SpriteBatch() = default;

    // Non-copyable
    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // Movable (explicit to properly clear source state)
    SpriteBatch(SpriteBatch&& other) noexcept;
    SpriteBatch& operator=(SpriteBatch&& other) noexcept;

    /**
     * Initialize the sprite batch.
     * @param device GPU device
     * @return true on success
     */
    [[nodiscard]] bool init(SDL_GPUDevice* device, const char* name = "SpriteBatch");

    /**
     * Shutdown and release resources.
     */
    void shutdown();

    /**
     * Begin recording sprites (call before render pass).
     * @param writePtr Pointer to mapped vertex buffer (from vertex pool)
     * @param maxVertices Maximum vertices that can be written
     * @param texture Texture for the batch
     * @param sampler Sampler for the batch
     * @param textureWidth Width of texture in pixels
     * @param textureHeight Height of texture in pixels
     * @param targetHeight Height of the render target in pixels
     */
    void begin(SpriteVertex* writePtr, size_t maxVertices,
               SDL_GPUTexture* texture, SDL_GPUSampler* sampler,
               float textureWidth, float textureHeight,
               float targetHeight);

    /**
     * Draw a sprite from atlas coordinates.
     * @param srcX Source X in texture (pixels)
     * @param srcY Source Y in texture (pixels)
     * @param srcW Source width (pixels)
     * @param srcH Source height (pixels)
     * @param dstX Destination X (world/screen coords)
     * @param dstY Destination Y (world/screen coords)
     * @param dstW Destination width
     * @param dstH Destination height
     * @param r Red tint (0-255)
     * @param g Green tint (0-255)
     * @param b Blue tint (0-255)
     * @param a Alpha (0-255)
     */
    void draw(float srcX, float srcY, float srcW, float srcH,
              float dstX, float dstY, float dstW, float dstH,
              uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);

    /**
     * Draw a sprite using normalized texture coordinates.
     */
    void drawUV(float u0, float v0, float u1, float v1,
                float dstX, float dstY, float dstW, float dstH,
                uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);

    /**
     * Draw a sprite using normalized texture coordinates, rotated around its center.
     * @param angleRad Rotation angle in radians (0 = right-facing, positive = clockwise in Y-down engine space)
     */
    void drawUVRotated(float u0, float v0, float u1, float v1,
                       float dstX, float dstY, float dstW, float dstH,
                       float angleRad,
                       uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);

    /**
     * End recording and return vertex count.
     * @return Number of vertices written (for vertex pool endFrame)
     */
    size_t end();

    /**
     * Issue the draw call during render pass.
     * @param pass Active render pass
     * @param pipeline Pipeline to use
     * @param vertexBuffer GPU vertex buffer (from vertex pool)
     */
    void render(SDL_GPURenderPass* pass,
                SDL_GPUGraphicsPipeline* pipeline,
                SDL_GPUBuffer* vertexBuffer);

    /**
     * Get current sprite count.
     */
    size_t getSpriteCount() const { return m_spriteCount; }

    /**
     * Get current vertex count.
     */
    size_t getVertexCount() const { return m_vertexCount; }

    /**
     * Get bound texture.
     */
    SDL_GPUTexture* getTexture() const { return m_texture; }

    /**
     * Get bound sampler.
     */
    SDL_GPUSampler* getSampler() const { return m_sampler; }

    /**
     * Check if batch has any sprites.
     */
    bool hasSprites() const { return m_spriteCount > 0; }

    /**
     * Get the index buffer for rendering.
     */
    SDL_GPUBuffer* getIndexBuffer() const { return m_indexBuffer.get(); }

private:
    void addQuad(float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    void addQuadRotated(float cx, float cy, float halfW, float halfH,
                        float cosA, float sinA,
                        float u0, float v0, float u1, float v1,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    SDL_GPUDevice* m_device{nullptr};
    SDL_GPUTexture* m_texture{nullptr};
    SDL_GPUSampler* m_sampler{nullptr};

    // Pre-built index buffer
    GPUBuffer m_indexBuffer;

    // Write pointer to mapped vertex buffer
    SpriteVertex* m_writePtr{nullptr};
    size_t m_maxVertices{0};

    // Texture dimensions for UV calculation
    float m_textureWidth{1.0f};
    float m_textureHeight{1.0f};
    float m_targetHeight{1.0f};

    size_t m_spriteCount{0};
    size_t m_vertexCount{0};
    bool m_recording{false};
    bool m_initialized{false};
};

} // namespace VoidLight

#endif // SPRITE_BATCH_HPP
