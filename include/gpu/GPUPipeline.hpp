/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_PIPELINE_HPP
#define GPU_PIPELINE_HPP

#include <SDL3/SDL_gpu.h>
#include <array>
#include <cstdint>

namespace VoidLight {

/**
 * Pipeline type identifiers for render sorting.
 */
enum class PipelineType : uint8_t {
    SpriteOpaque = 0,   // Depth write, no blend
    SpriteAlpha,        // Depth test, alpha blend
    Particle,           // No depth, additive/alpha blend
    Composite,          // Fullscreen quad composite
    Primitive,          // Colored primitives (UI backgrounds)
    Text,               // Text rendering
    COUNT
};

/**
 * Configuration for creating a graphics pipeline.
 * Uses value semantics with embedded arrays for thread-safety.
 */
struct PipelineConfig {
    SDL_GPUShader* vertexShader{nullptr};
    SDL_GPUShader* fragmentShader{nullptr};

    // Embedded vertex format data (value semantics, no pointers)
    std::array<SDL_GPUVertexBufferDescription, 1> vertexBuffers{};
    std::array<SDL_GPUVertexAttribute, 4> vertexAttributes{};
    uint32_t vertexBufferCount{0};
    uint32_t vertexAttributeCount{0};

    // Primitive type
    SDL_GPUPrimitiveType primitiveType{SDL_GPU_PRIMITIVETYPE_TRIANGLELIST};

    // Depth/stencil state
    bool enableDepthTest{false};
    bool enableDepthWrite{false};
    SDL_GPUCompareOp depthCompareOp{SDL_GPU_COMPAREOP_LESS};

    // Blending state
    bool enableBlend{true};
    SDL_GPUBlendFactor srcColorFactor{SDL_GPU_BLENDFACTOR_SRC_ALPHA};
    SDL_GPUBlendFactor dstColorFactor{SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA};
    SDL_GPUBlendFactor srcAlphaFactor{SDL_GPU_BLENDFACTOR_ONE};
    SDL_GPUBlendFactor dstAlphaFactor{SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA};
    SDL_GPUBlendOp colorBlendOp{SDL_GPU_BLENDOP_ADD};
    SDL_GPUBlendOp alphaBlendOp{SDL_GPU_BLENDOP_ADD};

    // Color target format (typically swapchain format)
    SDL_GPUTextureFormat colorFormat{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};

    // Rasterizer state
    SDL_GPUFillMode fillMode{SDL_GPU_FILLMODE_FILL};
    SDL_GPUCullMode cullMode{SDL_GPU_CULLMODE_NONE};
    SDL_GPUFrontFace frontFace{SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE};
};

/**
 * RAII wrapper for SDL_GPUGraphicsPipeline.
 */
class GPUPipeline {
public:
    GPUPipeline() = default;

    ~GPUPipeline();

    // Move-only
    GPUPipeline(GPUPipeline&& other) noexcept;
    GPUPipeline& operator=(GPUPipeline&& other) noexcept;
    GPUPipeline(const GPUPipeline&) = delete;
    GPUPipeline& operator=(const GPUPipeline&) = delete;

    /**
     * Create a graphics pipeline from configuration.
     * @param device GPU device
     * @param config Pipeline configuration
     * @return true on success
     */
    [[nodiscard]] bool create(SDL_GPUDevice* device, const PipelineConfig& config);

    /**
     * Release the pipeline.
     */
    void release();

    SDL_GPUGraphicsPipeline* get() const { return m_pipeline; }
    bool isValid() const { return m_pipeline != nullptr; }

    /**
     * Create a standard sprite pipeline configuration.
     * @param vertShader Vertex shader
     * @param fragShader Fragment shader
     * @param colorFormat Target color format
     * @param alpha If true, enables alpha blending; if false, opaque
     */
    static PipelineConfig createSpriteConfig(SDL_GPUShader* vertShader,
                                              SDL_GPUShader* fragShader,
                                              SDL_GPUTextureFormat colorFormat,
                                              bool alpha);

    /**
     * Create a particle pipeline configuration.
     * @param additive If true, particles add onto the scene (glow effects
     *                 like fireflies/fire/sparks); if false, standard alpha
     *                 blending.
     */
    static PipelineConfig createParticleConfig(SDL_GPUShader* vertShader,
                                                SDL_GPUShader* fragShader,
                                                SDL_GPUTextureFormat colorFormat,
                                                bool additive = false);

    /**
     * Create a primitive (colored quad) pipeline configuration.
     */
    static PipelineConfig createPrimitiveConfig(SDL_GPUShader* vertShader,
                                                 SDL_GPUShader* fragShader,
                                                 SDL_GPUTextureFormat colorFormat);

    /**
     * Create a fullscreen composite pipeline configuration.
     */
    static PipelineConfig createCompositeConfig(SDL_GPUShader* vertShader,
                                                 SDL_GPUShader* fragShader,
                                                 SDL_GPUTextureFormat colorFormat);

private:
    SDL_GPUGraphicsPipeline* m_pipeline{nullptr};
    SDL_GPUDevice* m_device{nullptr};
};

} // namespace VoidLight

#endif // GPU_PIPELINE_HPP
