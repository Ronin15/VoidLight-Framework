/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUPipeline.hpp"
#include "core/Logger.hpp"
#include <format>

namespace VoidLight {

GPUPipeline::~GPUPipeline() {
    release();
}

GPUPipeline::GPUPipeline(GPUPipeline&& other) noexcept
    : m_pipeline(other.m_pipeline)
    , m_device(other.m_device)
{
    other.m_pipeline = nullptr;
    other.m_device = nullptr;
}

GPUPipeline& GPUPipeline::operator=(GPUPipeline&& other) noexcept {
    if (this != &other) {
        release();

        m_pipeline = other.m_pipeline;
        m_device = other.m_device;

        other.m_pipeline = nullptr;
        other.m_device = nullptr;
    }
    return *this;
}

bool GPUPipeline::create(SDL_GPUDevice* device, const PipelineConfig& config) {
    if (!device) {
        GAMEENGINE_ERROR("GPUPipeline::create: null device");
        return false;
    }

    if (!config.vertexShader || !config.fragmentShader) {
        GAMEENGINE_ERROR("GPUPipeline::create: missing shaders");
        return false;
    }

    release();
    m_device = device;

    // Build color target description
    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format = config.colorFormat;

    if (config.enableBlend) {
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = config.srcColorFactor;
        colorTarget.blend_state.dst_color_blendfactor = config.dstColorFactor;
        colorTarget.blend_state.color_blend_op = config.colorBlendOp;
        colorTarget.blend_state.src_alpha_blendfactor = config.srcAlphaFactor;
        colorTarget.blend_state.dst_alpha_blendfactor = config.dstAlphaFactor;
        colorTarget.blend_state.alpha_blend_op = config.alphaBlendOp;
        colorTarget.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R |
                                                    SDL_GPU_COLORCOMPONENT_G |
                                                    SDL_GPU_COLORCOMPONENT_B |
                                                    SDL_GPU_COLORCOMPONENT_A;
    } else {
        colorTarget.blend_state.enable_blend = false;
        colorTarget.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R |
                                                    SDL_GPU_COLORCOMPONENT_G |
                                                    SDL_GPU_COLORCOMPONENT_B |
                                                    SDL_GPU_COLORCOMPONENT_A;
    }

    // Build rasterizer state
    SDL_GPURasterizerState rasterizer{};
    rasterizer.fill_mode = config.fillMode;
    rasterizer.cull_mode = config.cullMode;
    rasterizer.front_face = config.frontFace;
    rasterizer.enable_depth_bias = false;
    rasterizer.enable_depth_clip = true;

    // Build depth/stencil state
    SDL_GPUDepthStencilState depthStencil{};
    depthStencil.enable_depth_test = config.enableDepthTest;
    depthStencil.enable_depth_write = config.enableDepthWrite;
    depthStencil.compare_op = config.depthCompareOp;
    depthStencil.enable_stencil_test = false;

    // Build vertex input state from embedded config arrays
    SDL_GPUVertexInputState vertexInput{};
    vertexInput.num_vertex_buffers = config.vertexBufferCount;
    vertexInput.vertex_buffer_descriptions = config.vertexBufferCount > 0
        ? config.vertexBuffers.data() : nullptr;
    vertexInput.num_vertex_attributes = config.vertexAttributeCount;
    vertexInput.vertex_attributes = config.vertexAttributeCount > 0
        ? config.vertexAttributes.data() : nullptr;

    // Build pipeline create info
    SDL_GPUGraphicsPipelineCreateInfo createInfo{};
    createInfo.vertex_shader = config.vertexShader;
    createInfo.fragment_shader = config.fragmentShader;
    createInfo.vertex_input_state = vertexInput;
    createInfo.primitive_type = config.primitiveType;
    createInfo.rasterizer_state = rasterizer;
    createInfo.depth_stencil_state = depthStencil;
    createInfo.target_info.num_color_targets = 1;
    createInfo.target_info.color_target_descriptions = &colorTarget;
    createInfo.target_info.has_depth_stencil_target = false;

    m_pipeline = SDL_CreateGPUGraphicsPipeline(device, &createInfo);

    if (!m_pipeline) {
        GAMEENGINE_ERROR(std::format("Failed to create GPU pipeline: {}", SDL_GetError()));
        return false;
    }

    return true;
}

void GPUPipeline::release() {
    if (m_pipeline && m_device) {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
    }
    m_pipeline = nullptr;
    m_device = nullptr;
}

PipelineConfig GPUPipeline::createSpriteConfig(SDL_GPUShader* vertShader,
                                                SDL_GPUShader* fragShader,
                                                SDL_GPUTextureFormat colorFormat,
                                                bool alpha) {
    PipelineConfig config{};
    config.vertexShader = vertShader;
    config.fragmentShader = fragShader;
    config.colorFormat = colorFormat;
    config.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // Vertex buffer: position(vec2) + texcoord(vec2) + color(rgba8) = 20 bytes
    config.vertexBuffers[0].slot = 0;
    config.vertexBuffers[0].pitch = sizeof(float) * 4 + sizeof(uint8_t) * 4;
    config.vertexBuffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    config.vertexBuffers[0].instance_step_rate = 0;
    config.vertexBufferCount = 1;

    // Vertex attributes: position, texcoord, color
    config.vertexAttributes[0].location = 0;
    config.vertexAttributes[0].buffer_slot = 0;
    config.vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    config.vertexAttributes[0].offset = 0;

    config.vertexAttributes[1].location = 1;
    config.vertexAttributes[1].buffer_slot = 0;
    config.vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    config.vertexAttributes[1].offset = sizeof(float) * 2;

    config.vertexAttributes[2].location = 2;
    config.vertexAttributes[2].buffer_slot = 0;
    config.vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    config.vertexAttributes[2].offset = sizeof(float) * 4;
    config.vertexAttributeCount = 3;

    if (alpha) {
        config.enableBlend = true;
        // Premultiplied alpha blending (textures have RGB * A pre-applied)
        config.srcColorFactor = SDL_GPU_BLENDFACTOR_ONE;
        config.dstColorFactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        config.colorBlendOp = SDL_GPU_BLENDOP_ADD;
        config.srcAlphaFactor = SDL_GPU_BLENDFACTOR_ONE;
        config.dstAlphaFactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        config.alphaBlendOp = SDL_GPU_BLENDOP_ADD;
    } else {
        config.enableBlend = false;
    }

    return config;
}

PipelineConfig GPUPipeline::createParticleConfig(SDL_GPUShader* vertShader,
                                                  SDL_GPUShader* fragShader,
                                                  SDL_GPUTextureFormat colorFormat,
                                                  bool additive) {
    PipelineConfig config{};
    config.vertexShader = vertShader;
    config.fragmentShader = fragShader;
    config.colorFormat = colorFormat;
    config.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // Vertex buffer: position(vec2) + color(rgba8) = 12 bytes
    config.vertexBuffers[0].slot = 0;
    config.vertexBuffers[0].pitch = sizeof(float) * 2 + sizeof(uint8_t) * 4;
    config.vertexBuffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    config.vertexBuffers[0].instance_step_rate = 0;
    config.vertexBufferCount = 1;

    // Vertex attributes: position, color
    config.vertexAttributes[0].location = 0;
    config.vertexAttributes[0].buffer_slot = 0;
    config.vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    config.vertexAttributes[0].offset = 0;

    config.vertexAttributes[1].location = 1;
    config.vertexAttributes[1].buffer_slot = 0;
    config.vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    config.vertexAttributes[1].offset = sizeof(float) * 2;
    config.vertexAttributeCount = 2;

    config.enableBlend = true;
    if (additive) {
        // Additive blending: particle color adds onto the scene rather than
        // replacing it (glow effects -- fireflies, fire, sparks).
        config.srcColorFactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        config.dstColorFactor = SDL_GPU_BLENDFACTOR_ONE;
        config.colorBlendOp = SDL_GPU_BLENDOP_ADD;
        config.srcAlphaFactor = SDL_GPU_BLENDFACTOR_ONE;
        config.dstAlphaFactor = SDL_GPU_BLENDFACTOR_ONE;
        config.alphaBlendOp = SDL_GPU_BLENDOP_ADD;
    } else {
        // Standard alpha blending for particles.
        config.srcColorFactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        config.dstColorFactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    }

    return config;
}

PipelineConfig GPUPipeline::createPrimitiveConfig(SDL_GPUShader* vertShader,
                                                   SDL_GPUShader* fragShader,
                                                   SDL_GPUTextureFormat colorFormat) {
    PipelineConfig config{};
    config.vertexShader = vertShader;
    config.fragmentShader = fragShader;
    config.colorFormat = colorFormat;
    config.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // Vertex buffer: position(vec2) + color(rgba8) = 12 bytes
    config.vertexBuffers[0].slot = 0;
    config.vertexBuffers[0].pitch = sizeof(float) * 2 + sizeof(uint8_t) * 4;
    config.vertexBuffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    config.vertexBuffers[0].instance_step_rate = 0;
    config.vertexBufferCount = 1;

    // Vertex attributes: position, color
    config.vertexAttributes[0].location = 0;
    config.vertexAttributes[0].buffer_slot = 0;
    config.vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    config.vertexAttributes[0].offset = 0;

    config.vertexAttributes[1].location = 1;
    config.vertexAttributes[1].buffer_slot = 0;
    config.vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    config.vertexAttributes[1].offset = sizeof(float) * 2;
    config.vertexAttributeCount = 2;

    // Alpha blending
    config.enableBlend = true;
    config.srcColorFactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    config.dstColorFactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    return config;
}

PipelineConfig GPUPipeline::createCompositeConfig(SDL_GPUShader* vertShader,
                                                   SDL_GPUShader* fragShader,
                                                   SDL_GPUTextureFormat colorFormat) {
    PipelineConfig config{};
    config.vertexShader = vertShader;
    config.fragmentShader = fragShader;
    config.colorFormat = colorFormat;
    config.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // No vertex input - fullscreen triangle uses gl_VertexIndex
    config.vertexBufferCount = 0;
    config.vertexAttributeCount = 0;

    // No blending for composite
    config.enableBlend = false;

    return config;
}

} // namespace VoidLight
