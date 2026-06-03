/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_RENDERER_HPP
#define GPU_RENDERER_HPP

#include "gpu/GPUTypes.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/GPUTexture.hpp"
#include "gpu/GPUSampler.hpp"
#include "gpu/GPUBuffer.hpp"
#include "gpu/GPUTransferBuffer.hpp"
#include "gpu/GPUPipeline.hpp"
#include "gpu/GPUVertexPool.hpp"
#include "gpu/SpriteBatch.hpp"
#include "gpu/UIRenderBatches.hpp"
#include <SDL3/SDL_gpu.h>
#include <memory>
#include <span>
#include <vector>

namespace VoidLight {

/**
 * Main GPU renderer singleton.
 *
 * Orchestrates the rendering pipeline:
 * - Command buffer management
 * - Copy pass for uploads
 * - Render passes for scene and swapchain
 * - Pipeline state management
 */
class GPURenderer {
public:
    static GPURenderer& Instance();

    /**
     * Initialize the renderer.
     * Must be called after GPUDevice::init().
     * @return true on success
     */
    [[nodiscard]] bool init();

    /**
     * Shutdown and release all resources.
     */
    void shutdown();

    /**
     * Begin a new frame.
     * - Acquires command buffer
     * - Maps upload buffers
     * - Begins copy pass for uploads
     * @return true if the frame can proceed
     */
    bool beginFrame();

    /**
     * Finalize uploads and acquire the swapchain texture for this frame.
     * SDL_GPU presents the acquired swapchain texture when the owning command
     * buffer is submitted, so acquisition remains part of the render command
     * buffer flow.
     * @return true if a swapchain texture is ready for this frame
     */
    bool acquireSwapchainTexture();

    /**
     * End copy pass, upload frame data, and begin scene render pass.
     * @return Active render pass for scene rendering
     */
    SDL_GPURenderPass* beginScenePass();

    /**
     * End scene pass and begin swapchain pass.
     * @return Active render pass for UI/final compositing
     */
    SDL_GPURenderPass* beginSwapchainPass();

    /**
     * End current frame.
     * - Ends active render pass
     * - Submits command buffer
     */
    void endFrame();

    // Pipeline accessors (scene rendering - to scene texture)
    SDL_GPUGraphicsPipeline* getSpriteOpaquePipeline() const;
    SDL_GPUGraphicsPipeline* getSpriteAlphaPipeline() const;
    SDL_GPUGraphicsPipeline* getParticlePipeline() const;
    SDL_GPUGraphicsPipeline* getPrimitivePipeline() const;
    SDL_GPUGraphicsPipeline* getCompositePipeline() const;

    // Sampler accessors
    SDL_GPUSampler* getNearestSampler() const { return m_nearestSampler.get(); }
    SDL_GPUSampler* getLinearSampler() const { return m_linearSampler.get(); }

    // Scene texture accessor (for compositing)
    GPUTexture* getSceneTexture() const { return m_sceneTexture.get(); }

    // Vertex pool accessors
    GPUVertexPool& getSpriteVertexPool() { return m_spriteVertexPool; }
    GPUVertexPool& getEntityVertexPool() { return m_entityVertexPool; }
    GPUVertexPool& getParticleVertexPool() { return m_particleVertexPool; }
    GPUVertexPool& getPrimitiveVertexPool() { return m_primitiveVertexPool; }
    GPUVertexPool& getUIVertexPool() { return m_uiVertexPool; }

    // Sprite batch accessors
    SpriteBatch& getSpriteBatch() { return m_spriteBatch; }
    SpriteBatch& getEntityBatch() { return m_entityBatch; }

    // Active command buffer (for advanced usage)
    SDL_GPUCommandBuffer* getCommandBuffer() const { return m_commandBuffer; }

    // Copy pass accessor (for texture uploads during frame)
    SDL_GPUCopyPass* getCopyPass() const { return m_copyPass; }

    // Screen dimensions
    uint32_t getViewportWidth() const { return m_viewportWidth; }
    uint32_t getViewportHeight() const { return m_viewportHeight; }

    /**
     * Update viewport dimensions (e.g., on window resize).
     */
    void updateViewport(uint32_t width, uint32_t height);

    /**
     * Push view-projection matrix uniform.
     * @param pass Active render pass
     * @param viewProjection 4x4 matrix data
     */
    void pushViewProjection(const SDL_GPURenderPass* pass, const float* viewProjection);

    /**
     * Submit pre-recorded UI render families during the swapchain UI pass.
     * UIManager records vertices and frame-local batches; GPURenderer owns the
     * SDL_GPU pipeline, sampler, vertex-buffer, and draw-call submission.
     */
    void renderUIBatches(SDL_GPURenderPass* pass,
                         uint32_t primitiveVertexCount,
                         std::span<const UITextureDrawBatch> imageBatches,
                         std::span<const UITextDrawBatch> textBatches);

    /**
     * Push composite uniforms.
     */
    void pushCompositeUniforms(const SDL_GPURenderPass* pass,
                                float subPixelX, float subPixelY, float zoom);

    /**
     * Set day/night ambient lighting parameters.
     * Called by DayNightController each frame.
     * @param r Red tint (0-1)
     * @param g Green tint (0-1)
     * @param b Blue tint (0-1)
     * @param alpha Blend strength (0 = no tint, 1 = full tint)
     */
    void setDayNightParams(float r, float g, float b, float alpha);

    /**
     * Set composite parameters for the current frame.
     * Call this during recordGPUVertices to configure zoom/scrolling.
     * @param zoom Zoom level (1.0 = no zoom)
     * @param subPixelX Sub-pixel X offset for smooth scrolling
     * @param subPixelY Sub-pixel Y offset for smooth scrolling
     */
    void setCompositeParams(float zoom, float subPixelX = 0.0f, float subPixelY = 0.0f);

    /**
     * Render the scene texture to the swapchain with compositing.
     * Uses composite params set via setCompositeParams().
     * @param pass Active swapchain render pass
     */
    void renderComposite(SDL_GPURenderPass* pass);

    /**
     * Create an orthographic projection matrix for 2D rendering.
     * @param left Left coordinate
     * @param right Right coordinate
     * @param bottom Bottom coordinate
     * @param top Top coordinate
     * @param out Output matrix (16 floats)
     */
    static void createOrthoMatrix(float left, float right, float bottom, float top,
                                   float* out);

private:
    GPURenderer() = default;
    ~GPURenderer() = default;

    // Non-copyable
    GPURenderer(const GPURenderer&) = delete;
    GPURenderer& operator=(const GPURenderer&) = delete;

    [[nodiscard]] bool loadShaders();
    [[nodiscard]] bool createPipelines();
    [[nodiscard]] bool createSceneTexture();
    void cleanupPartialInit();  // Clean up resources on init failure
    void resetFrameState();

    // Device reference
    SDL_GPUDevice* m_device{nullptr};
    SDL_Window* m_window{nullptr};

    // Frame state
    SDL_GPUCommandBuffer* m_commandBuffer{nullptr};
    SDL_GPUCopyPass* m_copyPass{nullptr};
    SDL_GPURenderPass* m_currentPass{nullptr};

    // Swapchain state (acquired explicitly before the swapchain render pass)
    SDL_GPUTexture* m_swapchainTexture{nullptr};
    uint32_t m_swapchainWidth{0};
    uint32_t m_swapchainHeight{0};
    bool m_frameActive{false};
    bool m_frameReadyForPresentation{false};

    // Intermediate scene texture
    std::unique_ptr<GPUTexture> m_sceneTexture;

    // Samplers
    GPUSampler m_nearestSampler;
    GPUSampler m_linearSampler;

    // Pipelines (scene rendering - to scene texture)
    GPUPipeline m_spriteOpaquePipeline;
    GPUPipeline m_spriteAlphaPipeline;
    GPUPipeline m_particlePipeline;
    GPUPipeline m_primitivePipeline;
    GPUPipeline m_compositePipeline;

    // Pipelines (UI rendering - to swapchain)
    GPUPipeline m_uiSpritePipeline;
    GPUPipeline m_uiTextAlphaPipeline;
    GPUPipeline m_uiTextSDFPipeline;
    GPUPipeline m_uiPrimitivePipeline;

    // Vertex pools
    GPUVertexPool m_spriteVertexPool;
    GPUVertexPool m_entityVertexPool;  // For entity sprites (player, NPCs) with separate textures
    GPUVertexPool m_particleVertexPool;
    GPUVertexPool m_primitiveVertexPool;
    GPUVertexPool m_uiVertexPool;  // For UI sprites (text, icons) rendered to swapchain

    // Sprite batches
    SpriteBatch m_spriteBatch;      // World tiles (atlas)
    SpriteBatch m_entityBatch;      // Entities (player, NPCs)

    // Viewport (initialized from window size in init())
    uint32_t m_viewportWidth{0};
    uint32_t m_viewportHeight{0};

    // Composite params (set per-frame by game state)
    float m_compositeZoom{1.0f};
    float m_compositeSubPixelX{0.0f};
    float m_compositeSubPixelY{0.0f};

    // Day/night ambient lighting params (0-1 range, set by DayNightController)
    float m_dayNightR{1.0f};
    float m_dayNightG{1.0f};
    float m_dayNightB{1.0f};
    float m_dayNightAlpha{0.0f};

    // Debug tracking for scene texture dimension changes (avoids static variables)
    uint32_t m_lastLoggedSceneW{0};
    uint32_t m_lastLoggedSceneH{0};

    bool m_initialized{false};
};

} // namespace VoidLight

#endif // GPU_RENDERER_HPP
