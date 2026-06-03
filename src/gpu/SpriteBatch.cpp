/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/SpriteBatch.hpp"
#include "gpu/GPUTypes.hpp"
#include "gpu/GPUTransferBuffer.hpp"
#include "core/Logger.hpp"
#include <cmath>
#include <cstring>
#include <format>

namespace VoidLight {

bool SpriteBatch::init(SDL_GPUDevice* device, const char* name) {
    if (!device) {
        GAMEENGINE_ERROR("SpriteBatch::init: null device");
        return false;
    }

    m_device = device;

    // Build index buffer (all quads share same index pattern)
    // Use 32-bit indices to support >10k sprites (4K needs ~20k sprites)
    std::vector<uint32_t> indices;
    indices.reserve(MAX_INDICES);

    for (size_t i = 0; i < MAX_SPRITES; ++i) {
        uint32_t baseVertex = static_cast<uint32_t>(i * VERTICES_PER_SPRITE);
        // Two triangles per quad: 0-1-2, 2-3-0
        indices.push_back(baseVertex + 0);
        indices.push_back(baseVertex + 1);
        indices.push_back(baseVertex + 2);
        indices.push_back(baseVertex + 2);
        indices.push_back(baseVertex + 3);
        indices.push_back(baseVertex + 0);
    }

    // Create index buffer
    uint32_t indexBufferSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
    m_indexBuffer = GPUBuffer(device, SDL_GPU_BUFFERUSAGE_INDEX, indexBufferSize);

    if (!m_indexBuffer.isValid()) {
        GAMEENGINE_ERROR("SpriteBatch: failed to create index buffer");
        return false;
    }

    // Upload indices via transfer buffer
    GPUTransferBuffer transferBuffer(device, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, indexBufferSize);
    if (!transferBuffer.isValid()) {
        GAMEENGINE_ERROR("SpriteBatch: failed to create transfer buffer for indices");
        return false;
    }

    void* mapped = transferBuffer.map(false);
    if (!mapped) {
        GAMEENGINE_ERROR("SpriteBatch: failed to map transfer buffer");
        return false;
    }

    std::memcpy(mapped, indices.data(), indexBufferSize);
    transferBuffer.unmap();

    // Upload to GPU (need a one-time copy pass)
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        GAMEENGINE_ERROR(std::format("SpriteBatch: failed to acquire command buffer: {}", SDL_GetError()));
        return false;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
    if (!copyPass) {
        GAMEENGINE_ERROR(std::format("SpriteBatch: failed to begin copy pass: {}", SDL_GetError()));
        SDL_CancelGPUCommandBuffer(cmd);
        return false;
    }

    SDL_GPUTransferBufferLocation src = transferBuffer.asLocation(0);
    SDL_GPUBufferRegion dst{};
    dst.buffer = m_indexBuffer.get();
    dst.offset = 0;
    dst.size = indexBufferSize;

    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        GAMEENGINE_ERROR(std::format("SpriteBatch: failed to submit index upload command buffer: {}",
                                     SDL_GetError()));
        return false;
    }

    m_initialized = true;
    GAMEENGINE_INFO(std::format("{} initialized: max {} sprites, {} KB index buffer",
                    name, MAX_SPRITES, indexBufferSize / 1024));
    return true;
}

void SpriteBatch::shutdown() {
    m_indexBuffer = GPUBuffer();
    m_device = nullptr;
    m_initialized = false;
}

SpriteBatch::SpriteBatch(SpriteBatch&& other) noexcept
    : m_device(other.m_device)
    , m_texture(other.m_texture)
    , m_sampler(other.m_sampler)
    , m_indexBuffer(std::move(other.m_indexBuffer))
    , m_writePtr(other.m_writePtr)
    , m_maxVertices(other.m_maxVertices)
    , m_textureWidth(other.m_textureWidth)
    , m_textureHeight(other.m_textureHeight)
    , m_targetHeight(other.m_targetHeight)
    , m_spriteCount(other.m_spriteCount)
    , m_vertexCount(other.m_vertexCount)
    , m_recording(other.m_recording)
    , m_initialized(other.m_initialized)
{
    // Clear source state to prevent double-cleanup
    other.m_device = nullptr;
    other.m_texture = nullptr;
    other.m_sampler = nullptr;
    other.m_writePtr = nullptr;
    other.m_maxVertices = 0;
    other.m_targetHeight = 1.0f;
    other.m_spriteCount = 0;
    other.m_vertexCount = 0;
    other.m_recording = false;
    other.m_initialized = false;
}

SpriteBatch& SpriteBatch::operator=(SpriteBatch&& other) noexcept {
    if (this != &other) {
        // Release current resources
        shutdown();

        // Move from other
        m_device = other.m_device;
        m_texture = other.m_texture;
        m_sampler = other.m_sampler;
        m_indexBuffer = std::move(other.m_indexBuffer);
        m_writePtr = other.m_writePtr;
        m_maxVertices = other.m_maxVertices;
        m_textureWidth = other.m_textureWidth;
        m_textureHeight = other.m_textureHeight;
        m_targetHeight = other.m_targetHeight;
        m_spriteCount = other.m_spriteCount;
        m_vertexCount = other.m_vertexCount;
        m_recording = other.m_recording;
        m_initialized = other.m_initialized;

        // Clear source state
        other.m_device = nullptr;
        other.m_texture = nullptr;
        other.m_sampler = nullptr;
        other.m_writePtr = nullptr;
        other.m_maxVertices = 0;
        other.m_targetHeight = 1.0f;
        other.m_spriteCount = 0;
        other.m_vertexCount = 0;
        other.m_recording = false;
        other.m_initialized = false;
    }
    return *this;
}

void SpriteBatch::begin(SpriteVertex* writePtr, size_t maxVertices,
                        SDL_GPUTexture* texture, SDL_GPUSampler* sampler,
                        float textureWidth, float textureHeight,
                        float targetHeight) {
    if (!m_initialized) {
        GAMEENGINE_ERROR("SpriteBatch::begin: not initialized");
        return;
    }

    if (m_recording) {
        GAMEENGINE_WARN("SpriteBatch::begin: already recording, calling end()");
        end();
    }

    m_writePtr = writePtr;
    m_maxVertices = maxVertices;
    m_texture = texture;
    m_sampler = sampler;
    m_textureWidth = (textureWidth > 0) ? textureWidth : 1.0f;
    m_textureHeight = (textureHeight > 0) ? textureHeight : 1.0f;
    m_targetHeight = (targetHeight > 0) ? targetHeight : 1.0f;
    m_spriteCount = 0;
    m_vertexCount = 0;
    m_recording = true;
}

void SpriteBatch::draw(float srcX, float srcY, float srcW, float srcH,
                       float dstX, float dstY, float dstW, float dstH,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Convert pixel coords to normalized UVs
    float u0 = srcX / m_textureWidth;
    float v0 = srcY / m_textureHeight;
    float u1 = (srcX + srcW) / m_textureWidth;
    float v1 = (srcY + srcH) / m_textureHeight;

    drawUV(u0, v0, u1, v1, dstX, dstY, dstW, dstH, r, g, b, a);
}

void SpriteBatch::drawUV(float u0, float v0, float u1, float v1,
                         float dstX, float dstY, float dstW, float dstH,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!m_recording) {
        return;
    }

    // Check capacity
    if (m_vertexCount + VERTICES_PER_SPRITE > m_maxVertices) {
        GAMEENGINE_WARN("SpriteBatch: vertex capacity exceeded");
        return;
    }

    if (m_spriteCount >= MAX_SPRITES) {
        GAMEENGINE_WARN("SpriteBatch: sprite capacity exceeded");
        return;
    }

    // Convert engine top-left screen/world coordinates to SDL_GPU's Y-up space.
    float x0 = dstX;
    float x1 = dstX + dstW;
    float y0 = m_targetHeight - dstY;
    float y1 = y0 - dstH;

    addQuad(x0, y0, x1, y1, u0, v0, u1, v1, r, g, b, a);
}

void SpriteBatch::addQuad(float x0, float y0, float x1, float y1,
                          float u0, float v0, float u1, float v1,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!m_writePtr) {
        return;
    }

    // Write 4 vertices directly to mapped buffer
    SpriteVertex* v = m_writePtr + m_vertexCount;

    // Vertex 0: top-left
    v[0] = {x0, y0, u0, v0, r, g, b, a};
    // Vertex 1: top-right
    v[1] = {x1, y0, u1, v0, r, g, b, a};
    // Vertex 2: bottom-right
    v[2] = {x1, y1, u1, v1, r, g, b, a};
    // Vertex 3: bottom-left
    v[3] = {x0, y1, u0, v1, r, g, b, a};

    m_vertexCount += VERTICES_PER_SPRITE;
    ++m_spriteCount;
}

void SpriteBatch::drawUVRotated(float u0, float v0, float u1, float v1,
                                float dstX, float dstY, float dstW, float dstH,
                                float angleRad,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!m_recording) { return; }

    if (m_vertexCount + VERTICES_PER_SPRITE > m_maxVertices) {
        GAMEENGINE_WARN("SpriteBatch: vertex capacity exceeded");
        return;
    }
    if (m_spriteCount >= MAX_SPRITES) {
        GAMEENGINE_WARN("SpriteBatch: sprite capacity exceeded");
        return;
    }

    // Center in engine Y-down space, then convert to GPU Y-up space
    float halfW = dstW * 0.5f;
    float halfH = dstH * 0.5f;
    float gpuCX = dstX + halfW;
    float gpuCY = m_targetHeight - (dstY + halfH);

    // Negate angle: engine Y-down clockwise → GPU Y-up
    float cosA = std::cos(-angleRad);
    float sinA = std::sin(-angleRad);

    addQuadRotated(gpuCX, gpuCY, halfW, halfH, cosA, sinA,
                   u0, v0, u1, v1, r, g, b, a);
}

void SpriteBatch::addQuadRotated(float cx, float cy, float halfW, float halfH,
                                 float cosA, float sinA,
                                 float u0, float v0, float u1, float v1,
                                 uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!m_writePtr) { return; }

    SpriteVertex* v = m_writePtr + m_vertexCount;

    // Corner offsets in GPU Y-up space, matching addQuad() winding:
    //   0: top-left     (-halfW, +halfH)
    //   1: top-right    (+halfW, +halfH)
    //   2: bottom-right (+halfW, -halfH)
    //   3: bottom-left  (-halfW, -halfH)
    struct { float dx, dy; } corners[4] = {
        {-halfW, +halfH},
        {+halfW, +halfH},
        {+halfW, -halfH},
        {-halfW, -halfH},
    };

    float uvs[4][2] = {
        {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}
    };

    for (int i = 0; i < 4; ++i) {
        float rx = corners[i].dx * cosA - corners[i].dy * sinA;
        float ry = corners[i].dx * sinA + corners[i].dy * cosA;
        v[i] = {cx + rx, cy + ry, uvs[i][0], uvs[i][1], r, g, b, a};
    }

    m_vertexCount += VERTICES_PER_SPRITE;
    ++m_spriteCount;
}

size_t SpriteBatch::end() {
    if (!m_recording) {
        return 0;
    }

    size_t result = m_vertexCount;
    m_writePtr = nullptr;
    m_recording = false;

    return result;
}

void SpriteBatch::render(SDL_GPURenderPass* pass,
                         SDL_GPUGraphicsPipeline* pipeline,
                         SDL_GPUBuffer* vertexBuffer) {
    if (!pass || !pipeline || !vertexBuffer || m_spriteCount == 0) {
        return;
    }

    // Bind pipeline
    SDL_BindGPUGraphicsPipeline(pass, pipeline);

    // Bind texture and sampler
    if (m_texture && m_sampler) {
        SDL_GPUTextureSamplerBinding texSampler{};
        texSampler.texture = m_texture;
        texSampler.sampler = m_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &texSampler, 1);
    }

    // Bind vertex buffer
    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = vertexBuffer;
    vertexBinding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);

    // Bind index buffer
    SDL_GPUBufferBinding indexBinding{};
    indexBinding.buffer = m_indexBuffer.get();
    indexBinding.offset = 0;
    SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // Issue indexed draw call
    uint32_t indexCount = static_cast<uint32_t>(m_spriteCount * INDICES_PER_SPRITE);
    SDL_DrawGPUIndexedPrimitives(pass, indexCount, 1, 0, 0, 0);
}

} // namespace VoidLight
