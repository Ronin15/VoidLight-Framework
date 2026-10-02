/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

/**
 * Integration tests for SpriteBatch recording system.
 */

#define BOOST_TEST_MODULE SpriteBatchTests
#include <boost/test/unit_test.hpp>

#include "GPUTestFixture.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/SpriteBatch.hpp"
#include "gpu/GPUTypes.hpp"
#include <array>
#include <vector>

using namespace VoidLight;
using namespace VoidLight::Test;

// Global fixture for SDL cleanup
BOOST_GLOBAL_FIXTURE(GPUGlobalFixture);

/**
 * Test fixture that initializes GPUDevice for sprite batch testing.
 */
struct SpriteBatchTestFixture : public GPUTestFixture {
    SpriteBatchTestFixture() {
        if (!isGPUAvailable()) return;

        device = &GPUDevice::Instance();
        if (device->isInitialized()) {
            device->shutdown();
        }

        SDL_Window* window = getTestWindow();
        if (window) {
            BOOST_REQUIRE(device->init(window));
        }
    }

    ~SpriteBatchTestFixture() {
        if (device && device->isInitialized()) {
            device->shutdown();
        }
    }

    GPUDevice* device = nullptr;
};

// ============================================================================
// SPRITE BATCH CONSTANTS TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteBatchConstantsTests)

BOOST_AUTO_TEST_CASE(MaxSpritesConstant) {
    // Should support 50000 sprites for 4K rendering with zoom
    BOOST_CHECK_EQUAL(SpriteBatch::MAX_SPRITES, 50000u);
}

BOOST_AUTO_TEST_CASE(VerticesPerSpriteConstant) {
    // Each sprite is a quad = 4 vertices
    BOOST_CHECK_EQUAL(SpriteBatch::VERTICES_PER_SPRITE, 4u);
}

BOOST_AUTO_TEST_CASE(IndicesPerSpriteConstant) {
    // Each sprite needs 6 indices (2 triangles)
    BOOST_CHECK_EQUAL(SpriteBatch::INDICES_PER_SPRITE, 6u);
}

BOOST_AUTO_TEST_CASE(MaxVerticesConstant) {
    BOOST_CHECK_EQUAL(SpriteBatch::MAX_VERTICES,
        SpriteBatch::MAX_SPRITES * SpriteBatch::VERTICES_PER_SPRITE);
}

BOOST_AUTO_TEST_CASE(MaxIndicesConstant) {
    BOOST_CHECK_EQUAL(SpriteBatch::MAX_INDICES,
        SpriteBatch::MAX_SPRITES * SpriteBatch::INDICES_PER_SPRITE);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SPRITE BATCH INITIALIZATION TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteBatchInitTests)

BOOST_FIXTURE_TEST_CASE(DefaultConstructorState, SpriteBatchTestFixture) {
    SpriteBatch batch;

    BOOST_CHECK_EQUAL(batch.getSpriteCount(), 0u);
    BOOST_CHECK_EQUAL(batch.getVertexCount(), 0u);
    BOOST_CHECK(!batch.hasSprites());
    BOOST_CHECK(batch.getTexture() == nullptr);
    BOOST_CHECK(batch.getSampler() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(InitCreatesIndexBuffer, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    bool result = batch.init(device->get());

    BOOST_CHECK(result);
    BOOST_CHECK(batch.getIndexBuffer() != nullptr);

    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(InitWithNullDevice, SpriteBatchTestFixture) {
    SpriteBatch batch;
    bool result = batch.init(nullptr);

    BOOST_CHECK(!result);
}

BOOST_FIXTURE_TEST_CASE(ShutdownClearsState, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    batch.shutdown();

    BOOST_CHECK(batch.getIndexBuffer() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SPRITE BATCH RECORDING TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteBatchRecordingTests)

BOOST_FIXTURE_TEST_CASE(BeginSetsState, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    // Create a vertex buffer to write to
    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);

    // Begin recording with mock texture/sampler (nullptr for this test)
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    // State should be set but no sprites yet
    BOOST_CHECK_EQUAL(batch.getSpriteCount(), 0u);
    BOOST_CHECK(!batch.hasSprites());

    batch.end();
    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(DrawIncrementsSpriteCount, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    // Draw one sprite
    batch.draw(0, 0, 32, 32, 100, 100, 32, 32);

    BOOST_CHECK_EQUAL(batch.getSpriteCount(), 1u);
    BOOST_CHECK(batch.hasSprites());

    batch.end();
    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(DrawMultipleSprites, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    // Draw multiple sprites
    for (int i = 0; i < 100; ++i) {
        batch.draw(0, 0, 32, 32, float(i * 32), 100, 32, 32);
    }

    BOOST_CHECK_EQUAL(batch.getSpriteCount(), 100u);
    BOOST_CHECK_EQUAL(batch.getVertexCount(), 100u * SpriteBatch::VERTICES_PER_SPRITE);

    batch.end();
    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(DrawUVMethod, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    // Draw using normalized UV coordinates
    batch.drawUV(0.0f, 0.0f, 0.5f, 0.5f, 100.0f, 100.0f, 64.0f, 64.0f);

    BOOST_CHECK_EQUAL(batch.getSpriteCount(), 1u);

    batch.end();
    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(EndReturnsVertexCount, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    batch.draw(0, 0, 32, 32, 0, 0, 32, 32);
    batch.draw(32, 0, 32, 32, 32, 0, 32, 32);
    batch.draw(64, 0, 32, 32, 64, 0, 32, 32);

    size_t vertexCount = batch.end();

    BOOST_CHECK_EQUAL(vertexCount, 3u * SpriteBatch::VERTICES_PER_SPRITE);

    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(DrawWithColorTint, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    // Draw with custom color tint
    batch.draw(0, 0, 32, 32, 100, 100, 32, 32, 255, 128, 64, 200);

    BOOST_CHECK_EQUAL(batch.getSpriteCount(), 1u);

    // Verify vertex color was set (check first vertex)
    BOOST_CHECK_EQUAL(vertices[0].r, 255);
    BOOST_CHECK_EQUAL(vertices[0].g, 128);
    BOOST_CHECK_EQUAL(vertices[0].b, 64);
    BOOST_CHECK_EQUAL(vertices[0].a, 200);

    batch.end();
    batch.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SPRITE BATCH CAPACITY TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteBatchCapacityTests)

BOOST_FIXTURE_TEST_CASE(HasSpritesFlag, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    BOOST_CHECK(!batch.hasSprites());

    batch.draw(0, 0, 32, 32, 0, 0, 32, 32);

    BOOST_CHECK(batch.hasSprites());

    batch.end();
    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(LargeSpriteBatch, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 1024.0f, 1024.0f, 1024.0f);

    // Draw many sprites (but less than MAX_SPRITES)
    const size_t spriteCount = 10000;
    for (size_t i = 0; i < spriteCount; ++i) {
        batch.draw(
            float((i % 32) * 32), float((i / 32) * 32), 32, 32,
            float(i % 100) * 32, float(i / 100) * 32, 32, 32);
    }

    BOOST_CHECK_EQUAL(batch.getSpriteCount(), spriteCount);

    size_t vertexCount = batch.end();
    BOOST_CHECK_EQUAL(vertexCount, spriteCount * SpriteBatch::VERTICES_PER_SPRITE);

    batch.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SPRITE BATCH VERTEX DATA TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteBatchVertexDataTests)

BOOST_FIXTURE_TEST_CASE(VertexPositionsCorrect, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, 256.0f, 256.0f, 256.0f);

    // Draw sprite at (100, 200) with size (32, 32)
    batch.draw(0, 0, 32, 32, 100, 200, 32, 32);

    batch.end();

    // Verify quad positions in SDL_GPU Y-up space
    // Vertex 0: top-left
    BOOST_CHECK_CLOSE(vertices[0].x, 100.0f, 0.001f);
    BOOST_CHECK_CLOSE(vertices[0].y, 56.0f, 0.001f);

    // Vertex 1: top-right
    BOOST_CHECK_CLOSE(vertices[1].x, 132.0f, 0.001f);
    BOOST_CHECK_CLOSE(vertices[1].y, 56.0f, 0.001f);

    // Vertex 2: bottom-right
    BOOST_CHECK_CLOSE(vertices[2].x, 132.0f, 0.001f);
    BOOST_CHECK_CLOSE(vertices[2].y, 24.0f, 0.001f);

    // Vertex 3: bottom-left
    BOOST_CHECK_CLOSE(vertices[3].x, 100.0f, 0.001f);
    BOOST_CHECK_CLOSE(vertices[3].y, 24.0f, 0.001f);

    batch.shutdown();
}

BOOST_FIXTURE_TEST_CASE(VertexUVsNormalized, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch;
    BOOST_REQUIRE(batch.init(device->get()));

    std::vector<SpriteVertex> vertices(SpriteBatch::MAX_VERTICES);
    const float texWidth = 256.0f;
    const float texHeight = 256.0f;
    batch.begin(vertices.data(), vertices.size(), nullptr, nullptr, texWidth, texHeight, texHeight);

    // Draw sprite from texture region (64, 64, 32, 32)
    batch.draw(64, 64, 32, 32, 0, 0, 32, 32);

    batch.end();

    // Verify UVs are normalized (0-1 range)
    // Source: x=64, y=64, w=32, h=32 in 256x256 texture
    float u0 = 64.0f / texWidth; // 0.25
    float v0 = 64.0f / texHeight; // 0.25
    float u1 = 96.0f / texWidth; // 0.375
    float v1 = 96.0f / texHeight; // 0.375

    BOOST_CHECK_CLOSE(vertices[0].u, u0, 0.001f);
    BOOST_CHECK_CLOSE(vertices[0].v, v0, 0.001f);
    BOOST_CHECK_CLOSE(vertices[2].u, u1, 0.001f);
    BOOST_CHECK_CLOSE(vertices[2].v, v1, 0.001f);

    batch.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SPRITE BATCH MOVE SEMANTICS TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SpriteBatchMoveTests)

BOOST_FIXTURE_TEST_CASE(MoveConstruction, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch1;
    BOOST_REQUIRE(batch1.init(device->get()));

    SDL_GPUBuffer* indexBuffer = batch1.getIndexBuffer();

    SpriteBatch batch2(std::move(batch1));

    BOOST_CHECK(batch1.getIndexBuffer() == nullptr);
    BOOST_CHECK(batch2.getIndexBuffer() == indexBuffer);

    batch2.shutdown();
}

BOOST_FIXTURE_TEST_CASE(MoveConstructionPreservesTargetHeightDuringRecording, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch1;
    BOOST_REQUIRE(batch1.init(device->get()));

    std::array<SpriteVertex, SpriteBatch::VERTICES_PER_SPRITE> vertices{};
    batch1.begin(vertices.data(), vertices.size(), nullptr, nullptr, 64.0f, 64.0f, 200.0f);

    SpriteBatch batch2(std::move(batch1));
    batch2.drawUV(0.0f, 0.0f, 1.0f, 1.0f, 10.0f, 20.0f, 30.0f, 40.0f);

    BOOST_CHECK_CLOSE(vertices[0].y, 180.0f, 0.001f);
    BOOST_CHECK_CLOSE(vertices[2].y, 140.0f, 0.001f);

    batch2.shutdown();
}

BOOST_FIXTURE_TEST_CASE(MoveAssignment, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch1;
    BOOST_REQUIRE(batch1.init(device->get()));

    SDL_GPUBuffer* indexBuffer = batch1.getIndexBuffer();

    SpriteBatch batch2;
    batch2 = std::move(batch1);

    BOOST_CHECK(batch1.getIndexBuffer() == nullptr);
    BOOST_CHECK(batch2.getIndexBuffer() == indexBuffer);

    batch2.shutdown();
}

BOOST_FIXTURE_TEST_CASE(MoveAssignmentPreservesTargetHeightDuringRecording, SpriteBatchTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    SpriteBatch batch1;
    BOOST_REQUIRE(batch1.init(device->get()));

    std::array<SpriteVertex, SpriteBatch::VERTICES_PER_SPRITE> vertices{};
    batch1.begin(vertices.data(), vertices.size(), nullptr, nullptr, 64.0f, 64.0f, 180.0f);

    SpriteBatch batch2;
    batch2 = std::move(batch1);
    batch2.drawUV(0.0f, 0.0f, 1.0f, 1.0f, 4.0f, 8.0f, 12.0f, 16.0f);

    BOOST_CHECK_CLOSE(vertices[0].y, 172.0f, 0.001f);
    BOOST_CHECK_CLOSE(vertices[2].y, 156.0f, 0.001f);

    batch2.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()
