/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

/**
 * Unit tests for GPU pipeline configuration.
 * These tests validate PipelineConfig struct creation without requiring GPU.
 */

#define BOOST_TEST_MODULE GPUPipelineConfigTests
#include <boost/test/unit_test.hpp>

#include "gpu/GPUPipeline.hpp"
#include "gpu/GPUTypes.hpp"

using namespace VoidLight;

// ============================================================================
// PIPELINE CONFIG STRUCT TESTS
// Validates PipelineConfig defaults and structure
// ============================================================================

BOOST_AUTO_TEST_SUITE(PipelineConfigStructTests)

BOOST_AUTO_TEST_CASE(PipelineConfigDefaults) {
    PipelineConfig config{};

    // Shaders should be null by default
    BOOST_CHECK(config.vertexShader == nullptr);
    BOOST_CHECK(config.fragmentShader == nullptr);

    // Default primitive type
    BOOST_CHECK(config.primitiveType == SDL_GPU_PRIMITIVETYPE_TRIANGLELIST);

    // Depth state defaults
    BOOST_CHECK_EQUAL(config.enableDepthTest, false);
    BOOST_CHECK_EQUAL(config.enableDepthWrite, false);
    BOOST_CHECK(config.depthCompareOp == SDL_GPU_COMPAREOP_LESS);

    // Blend state defaults
    BOOST_CHECK_EQUAL(config.enableBlend, true);
    BOOST_CHECK(config.srcColorFactor == SDL_GPU_BLENDFACTOR_SRC_ALPHA);
    BOOST_CHECK(config.dstColorFactor == SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA);
    BOOST_CHECK(config.colorBlendOp == SDL_GPU_BLENDOP_ADD);

    // Rasterizer defaults
    BOOST_CHECK(config.fillMode == SDL_GPU_FILLMODE_FILL);
    BOOST_CHECK(config.cullMode == SDL_GPU_CULLMODE_NONE);
    BOOST_CHECK(config.frontFace == SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE);
}

BOOST_AUTO_TEST_CASE(PipelineConfigVertexBufferArray) {
    PipelineConfig config{};

    // Vertex buffer array should have capacity for 1 buffer
    BOOST_CHECK_EQUAL(config.vertexBuffers.size(), 1u);

    // Vertex attribute array should have capacity for 4 attributes
    BOOST_CHECK_EQUAL(config.vertexAttributes.size(), 4u);

    // Counts should be 0 by default
    BOOST_CHECK_EQUAL(config.vertexBufferCount, 0u);
    BOOST_CHECK_EQUAL(config.vertexAttributeCount, 0u);
}

BOOST_AUTO_TEST_CASE(PipelineConfigModification) {
    PipelineConfig config{};

    // Modify settings
    config.enableDepthTest = true;
    config.enableDepthWrite = true;
    config.enableBlend = false;
    config.cullMode = SDL_GPU_CULLMODE_BACK;

    // Verify modifications
    BOOST_CHECK_EQUAL(config.enableDepthTest, true);
    BOOST_CHECK_EQUAL(config.enableDepthWrite, true);
    BOOST_CHECK_EQUAL(config.enableBlend, false);
    BOOST_CHECK(config.cullMode == SDL_GPU_CULLMODE_BACK);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PIPELINE TYPE ENUM TESTS
// Validates PipelineType enum values
// ============================================================================

BOOST_AUTO_TEST_SUITE(PipelineTypeTests)

BOOST_AUTO_TEST_CASE(PipelineTypeValues) {
    // Verify enum values are distinct and sequential
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(PipelineType::SpriteOpaque), 0);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(PipelineType::SpriteAlpha), 1);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(PipelineType::Particle), 2);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(PipelineType::Composite), 3);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(PipelineType::Primitive), 4);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(PipelineType::Text), 5);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(PipelineType::COUNT), 6);
}

BOOST_AUTO_TEST_CASE(PipelineTypeCount) {
    // COUNT should match the number of pipeline types
    BOOST_CHECK_EQUAL(static_cast<size_t>(PipelineType::COUNT), 6u);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SPRITE CONFIG FACTORY TESTS
// Tests for createSpriteConfig (without GPU, validates config structure)
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteConfigFactoryTests)

BOOST_AUTO_TEST_CASE(SpriteOpaqueConfigStructure) {
    // Create config without actual shaders (just testing structure)
    auto config = GPUPipeline::createSpriteConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        false // opaque
    );

    // Opaque should have no blending
    BOOST_CHECK_EQUAL(config.enableBlend, false);

    // Should have correct vertex format
    BOOST_CHECK_EQUAL(config.vertexBufferCount, 1u);
    BOOST_CHECK_EQUAL(config.vertexAttributeCount, 3u); // position, texcoord, color

    // Verify color format
    BOOST_CHECK(config.colorFormat == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
}

BOOST_AUTO_TEST_CASE(SpriteAlphaConfigStructure) {
    auto config = GPUPipeline::createSpriteConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        true // alpha blend
    );

    // Alpha should have blending enabled (premultiplied alpha)
    BOOST_CHECK_EQUAL(config.enableBlend, true);
    // Premultiplied alpha uses ONE for src (RGB already multiplied by A)
    BOOST_CHECK(config.srcColorFactor == SDL_GPU_BLENDFACTOR_ONE);
    BOOST_CHECK(config.dstColorFactor == SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA);
}

BOOST_AUTO_TEST_CASE(SpriteConfigVertexStride) {
    auto config = GPUPipeline::createSpriteConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        true);

    // Vertex stride should match SpriteVertex size (20 bytes)
    BOOST_CHECK_EQUAL(config.vertexBuffers[0].pitch, sizeof(SpriteVertex));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PARTICLE CONFIG FACTORY TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(ParticleConfigFactoryTests)

BOOST_AUTO_TEST_CASE(ParticleConfigStructure) {
    auto config = GPUPipeline::createParticleConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

    // Particles use the standard alpha blend state for the GPU pipeline
    BOOST_CHECK_EQUAL(config.enableBlend, true);
    BOOST_CHECK(config.srcColorFactor == SDL_GPU_BLENDFACTOR_SRC_ALPHA);
    BOOST_CHECK(config.dstColorFactor == SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA);

    // Should have ColorVertex format
    BOOST_CHECK_EQUAL(config.vertexBufferCount, 1u);
    BOOST_CHECK_EQUAL(config.vertexAttributeCount, 2u); // position, color
}

BOOST_AUTO_TEST_CASE(ParticleConfigVertexStride) {
    auto config = GPUPipeline::createParticleConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

    // Vertex stride should match ColorVertex size (12 bytes)
    BOOST_CHECK_EQUAL(config.vertexBuffers[0].pitch, sizeof(ColorVertex));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PRIMITIVE CONFIG FACTORY TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(PrimitiveConfigFactoryTests)

BOOST_AUTO_TEST_CASE(PrimitiveConfigStructure) {
    auto config = GPUPipeline::createPrimitiveConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

    // Primitives use standard alpha blending
    BOOST_CHECK_EQUAL(config.enableBlend, true);
    BOOST_CHECK(config.srcColorFactor == SDL_GPU_BLENDFACTOR_SRC_ALPHA);
    BOOST_CHECK(config.dstColorFactor == SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA);

    // ColorVertex format
    BOOST_CHECK_EQUAL(config.vertexBufferCount, 1u);
    BOOST_CHECK_EQUAL(config.vertexAttributeCount, 2u);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// COMPOSITE CONFIG FACTORY TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(CompositeConfigFactoryTests)

BOOST_AUTO_TEST_CASE(CompositeConfigStructure) {
    auto config = GPUPipeline::createCompositeConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

    // Composite should have no blending (fullscreen quad)
    BOOST_CHECK_EQUAL(config.enableBlend, false);

    // Composite uses no vertex input - fullscreen triangle uses gl_VertexIndex
    BOOST_CHECK_EQUAL(config.vertexBufferCount, 0u);
    BOOST_CHECK_EQUAL(config.vertexAttributeCount, 0u);
}

BOOST_AUTO_TEST_CASE(CompositeConfigNoDepth) {
    auto config = GPUPipeline::createCompositeConfig(
        nullptr, nullptr,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

    // Composite pass doesn't need depth testing
    BOOST_CHECK_EQUAL(config.enableDepthTest, false);
    BOOST_CHECK_EQUAL(config.enableDepthWrite, false);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GPU PIPELINE CLASS TESTS (without GPU)
// ============================================================================

BOOST_AUTO_TEST_SUITE(GPUPipelineClassTests)

BOOST_AUTO_TEST_CASE(DefaultConstructorInvalid) {
    GPUPipeline pipeline;

    // Default-constructed pipeline should be invalid
    BOOST_CHECK(!pipeline.isValid());
    BOOST_CHECK(pipeline.get() == nullptr);
}

BOOST_AUTO_TEST_CASE(MoveSemantics) {
    GPUPipeline pipeline1;
    GPUPipeline pipeline2;

    // Move assignment should work (even for invalid pipelines)
    pipeline2 = std::move(pipeline1);

    BOOST_CHECK(!pipeline2.isValid());
}

BOOST_AUTO_TEST_SUITE_END()
