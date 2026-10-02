/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

/**
 * Integration tests for GPUVertexPool triple-buffering system.
 */

#define BOOST_TEST_MODULE GPUVertexPoolTests
#include <boost/test/unit_test.hpp>

#include "GPUTestFixture.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/GPUVertexPool.hpp"
#include "gpu/GPUTypes.hpp"

using namespace VoidLight;
using namespace VoidLight::Test;

// Global fixture for SDL cleanup
BOOST_GLOBAL_FIXTURE(GPUGlobalFixture);

/**
 * Test fixture that initializes GPUDevice for vertex pool testing.
 */
struct VertexPoolTestFixture : public GPUTestFixture {
    VertexPoolTestFixture() {
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

    ~VertexPoolTestFixture() {
        if (device && device->isInitialized()) {
            device->shutdown();
        }
    }

    GPUDevice* device = nullptr;
};

// ============================================================================
// VERTEX POOL INITIALIZATION TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(VertexPoolInitTests)

BOOST_FIXTURE_TEST_CASE(DefaultConstructorNotInitialized, VertexPoolTestFixture) {
    GPUVertexPool pool;

    BOOST_CHECK(!pool.isInitialized());
    BOOST_CHECK(pool.getGPUBuffer() == nullptr);
    BOOST_CHECK_EQUAL(pool.getVertexCount(), 0u);
}

BOOST_FIXTURE_TEST_CASE(InitWithSpriteVertexSize, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    bool result = pool.init(device->get(), sizeof(SpriteVertex));

    BOOST_CHECK(result);
    BOOST_CHECK(pool.isInitialized());
    BOOST_CHECK(pool.getGPUBuffer() != nullptr);
    BOOST_CHECK_EQUAL(pool.getVertexSize(), sizeof(SpriteVertex));
    BOOST_CHECK_EQUAL(pool.getMaxVertices(), GPUVertexPool::DEFAULT_VERTEX_CAPACITY);

    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(InitWithColorVertexSize, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    bool result = pool.init(device->get(), sizeof(ColorVertex));

    BOOST_CHECK(result);
    BOOST_CHECK(pool.isInitialized());
    BOOST_CHECK_EQUAL(pool.getVertexSize(), sizeof(ColorVertex));

    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(InitWithCustomCapacity, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const size_t customCapacity = 50000;
    GPUVertexPool pool;
    bool result = pool.init(device->get(), sizeof(SpriteVertex), customCapacity);

    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(pool.getMaxVertices(), customCapacity);

    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(InitWithNullDevice, VertexPoolTestFixture) {
    GPUVertexPool pool;
    bool result = pool.init(nullptr, sizeof(SpriteVertex));

    BOOST_CHECK(!result);
    BOOST_CHECK(!pool.isInitialized());
}

BOOST_FIXTURE_TEST_CASE(ShutdownClearsState, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    pool.shutdown();

    BOOST_CHECK(!pool.isInitialized());
    BOOST_CHECK(pool.getGPUBuffer() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TRIPLE BUFFERING TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(TripleBufferingTests)

BOOST_FIXTURE_TEST_CASE(FrameCountConstant, VertexPoolTestFixture) {
    // Triple buffering requires 3 frames
    BOOST_CHECK_EQUAL(GPUVertexPool::FRAME_COUNT, 3u);
}

BOOST_FIXTURE_TEST_CASE(BeginFrameReturnsMappedPointer, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    void* ptr = pool.beginFrame();

    BOOST_CHECK(ptr != nullptr);
    BOOST_CHECK(pool.getMappedPtr() == ptr);

    pool.endFrame(0);
    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(EndFrameRecordsVertexCount, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    pool.beginFrame();
    pool.endFrame(100);

    BOOST_CHECK_EQUAL(pool.getVertexCount(), 100u);
    BOOST_CHECK(pool.getMappedPtr() == nullptr);

    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(FrameCycleAdvances, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    // Cycle through 3 frames
    for (int frame = 0; frame < 3; ++frame) {
        void* ptr = pool.beginFrame();
        BOOST_CHECK(ptr != nullptr);

        // Write some vertices
        SpriteVertex* vertices = static_cast<SpriteVertex*>(ptr);
        vertices[0] = SpriteVertex{float(frame), 0.0f, 0.0f, 0.0f, 255, 255, 255, 255};

        pool.endFrame(1);
    }

    // Frame index should wrap around (0, 1, 2, 0, ...)
    // Pool should still be functional
    void* ptr = pool.beginFrame();
    BOOST_CHECK(ptr != nullptr);
    pool.endFrame(0);

    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(GPUBufferRotatesPerFrame, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    BOOST_REQUIRE(pool.beginFrame() != nullptr);
    SDL_GPUBuffer* firstGPUBuffer = pool.getGPUBuffer();
    pool.endFrame(1);

    BOOST_REQUIRE(pool.beginFrame() != nullptr);
    SDL_GPUBuffer* secondGPUBuffer = pool.getGPUBuffer();
    pool.endFrame(1);

    BOOST_CHECK(firstGPUBuffer != nullptr);
    BOOST_CHECK(secondGPUBuffer != nullptr);
    BOOST_CHECK_NE(firstGPUBuffer, secondGPUBuffer);

    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(NoGPUStallWithTripleBuffering, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    // Simulate multiple frames - triple buffering should prevent stalls
    for (int frame = 0; frame < 10; ++frame) {
        void* ptr = pool.beginFrame();
        BOOST_CHECK(ptr != nullptr);

        // Simulate writing vertices
        SpriteVertex* vertices = static_cast<SpriteVertex*>(ptr);
        for (size_t i = 0; i < 1000; ++i) {
            vertices[i] = SpriteVertex{
                float(i), float(frame),
                0.0f, 0.0f,
                255, 255, 255, 255};
        }

        pool.endFrame(1000);
    }

    pool.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// VERTEX POOL CAPACITY TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(VertexPoolCapacityTests)

BOOST_FIXTURE_TEST_CASE(DefaultVertexCapacity, VertexPoolTestFixture) {
    // Default capacity should handle 4K resolution with zoom headroom
    BOOST_CHECK_EQUAL(GPUVertexPool::DEFAULT_VERTEX_CAPACITY, 150000u);
}

BOOST_FIXTURE_TEST_CASE(SetWrittenVertexCount, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    pool.beginFrame();

    // Manual vertex count setting (for direct writes)
    pool.setWrittenVertexCount(500);
    BOOST_CHECK_EQUAL(pool.getPendingVertexCount(), 500u);

    pool.endFrame(500);
    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(VertexPoolHandlesMaxCapacity, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    const size_t smallCapacity = 1000;
    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex), smallCapacity));
    BOOST_REQUIRE(pool.isInitialized());

    pool.beginFrame();

    // Write up to max capacity
    SpriteVertex* vertices = static_cast<SpriteVertex*>(pool.getMappedPtr());
    for (size_t i = 0; i < smallCapacity; ++i) {
        vertices[i] = SpriteVertex{float(i), 0.0f, 0.0f, 0.0f, 255, 255, 255, 255};
    }

    pool.endFrame(smallCapacity);
    BOOST_CHECK_EQUAL(pool.getVertexCount(), smallCapacity);

    pool.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// VERTEX POOL MOVE SEMANTICS TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(VertexPoolMoveTests)

BOOST_FIXTURE_TEST_CASE(MoveConstruction, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool1;
    BOOST_REQUIRE(pool1.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool1.isInitialized());

    GPUVertexPool pool2(std::move(pool1));

    // Default move semantics - pool2 takes ownership
    BOOST_CHECK(pool2.isInitialized());
    BOOST_CHECK(pool2.getGPUBuffer() != nullptr);

    pool2.shutdown();
}

BOOST_FIXTURE_TEST_CASE(MoveAssignment, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool1;
    BOOST_REQUIRE(pool1.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool1.isInitialized());

    GPUVertexPool pool2;
    pool2 = std::move(pool1);

    // Default move semantics - pool2 takes ownership
    BOOST_CHECK(pool2.isInitialized());
    BOOST_CHECK(pool2.getGPUBuffer() != nullptr);

    pool2.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// VERTEX POOL ACCESSOR TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(VertexPoolAccessorTests)

BOOST_FIXTURE_TEST_CASE(GetGPUBufferValid, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    SDL_GPUBuffer* buffer = pool.getGPUBuffer();
    BOOST_CHECK(buffer != nullptr);

    pool.shutdown();
}

BOOST_FIXTURE_TEST_CASE(GetMappedPtrNullWhenNotInFrame, VertexPoolTestFixture) {
    SKIP_IF_NO_GPU();
    BOOST_REQUIRE(device->isInitialized());

    GPUVertexPool pool;
    BOOST_REQUIRE(pool.init(device->get(), sizeof(SpriteVertex)));
    BOOST_REQUIRE(pool.isInitialized());

    // Before beginFrame, mapped pointer should be null
    BOOST_CHECK(pool.getMappedPtr() == nullptr);

    pool.beginFrame();
    BOOST_CHECK(pool.getMappedPtr() != nullptr);

    pool.endFrame(0);
    BOOST_CHECK(pool.getMappedPtr() == nullptr);

    pool.shutdown();
}

BOOST_AUTO_TEST_SUITE_END()
