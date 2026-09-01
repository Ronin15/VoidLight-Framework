/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE BufferReuseTests
#include <boost/test/unit_test.hpp>

#include <vector>
#include <memory>
#include <chrono>

#include "utils/Vector2D.hpp"

// ============================================================================
// TEST SUITE: BufferReusePatternTests
// ============================================================================
// Tests that verify the fundamental buffer reuse patterns used throughout VoidLight
// These patterns are documented in AGENTS.md "Performance and Threading"

BOOST_AUTO_TEST_SUITE(BufferReusePatternTests)

// ----------------------------------------------------------------------------
// Test: Verify vector clear() preserves capacity
// ----------------------------------------------------------------------------
// This is the fundamental pattern used in AIManager, CollisionManager, ParticleManager
// From AGENTS.md: preserve capacity with clear(), never swap() away reusable capacity

BOOST_AUTO_TEST_CASE(TestVectorClearPreservesCapacity) {
    std::vector<int> buffer;

    // Reserve initial capacity
    const size_t initialCapacity = 1000;
    buffer.reserve(initialCapacity);
    BOOST_CHECK_EQUAL(buffer.capacity(), initialCapacity);
    BOOST_CHECK_EQUAL(buffer.size(), 0);

    // Populate buffer
    for (int i = 0; i < 500; ++i) {
        buffer.push_back(i);
    }
    BOOST_CHECK_EQUAL(buffer.size(), 500);
    BOOST_CHECK_GE(buffer.capacity(), initialCapacity); // Capacity unchanged

    // Clear buffer - should preserve capacity
    buffer.clear();
    BOOST_CHECK_EQUAL(buffer.size(), 0);               // Size reset to 0
    BOOST_CHECK_GE(buffer.capacity(), initialCapacity); // Capacity preserved

    // Second populate - no reallocation should occur
    for (int i = 0; i < 500; ++i) {
        buffer.push_back(i * 2);
    }
    BOOST_CHECK_EQUAL(buffer.size(), 500);
    BOOST_CHECK_GE(buffer.capacity(), initialCapacity); // Capacity still preserved
}

// ----------------------------------------------------------------------------
// Test: Multiple clear() cycles preserve capacity
// ----------------------------------------------------------------------------
// Simulates the pattern used in manager update loops:
// - Frame 1: populate buffer, clear() at end
// - Frame 2: populate buffer again (reuse capacity), clear() at end
// - Repeat...

BOOST_AUTO_TEST_CASE(TestMultipleClearCyclesPreserveCapacity) {
    std::vector<Vector2D> buffer;

    // Initial reserve
    const size_t capacity = 2000;
    buffer.reserve(capacity);
    size_t initialCapacity = buffer.capacity();
    BOOST_CHECK_GE(initialCapacity, capacity);

    // Simulate 100 frames of buffer reuse
    for (int frame = 0; frame < 100; ++frame) {
        // Populate buffer (simulating entity processing)
        for (int i = 0; i < 1000; ++i) {
            buffer.push_back(Vector2D(static_cast<float>(i), static_cast<float>(frame)));
        }
        BOOST_CHECK_EQUAL(buffer.size(), 1000);

        // Clear for next frame
        buffer.clear();
        BOOST_CHECK_EQUAL(buffer.size(), 0);

        // Capacity should remain stable across all frames
        BOOST_CHECK_GE(buffer.capacity(), initialCapacity);
    }

    // After 100 frames, capacity should still be preserved
    BOOST_CHECK_GE(buffer.capacity(), initialCapacity);
}

// ----------------------------------------------------------------------------
// Test: Clear vs reassignment performance
// ----------------------------------------------------------------------------
// Demonstrates why clear() is preferred over reassignment
// - clear(): O(n) but preserves capacity
// - reassignment: O(n) + deallocation + potential reallocation

BOOST_AUTO_TEST_CASE(TestClearVsReassignmentCapacity) {
    const size_t capacity = 5000;

    // Pattern 1: Using clear() (GOOD)
    std::vector<int> bufferWithClear;
    bufferWithClear.reserve(capacity);
    size_t capacityAfterReserve = bufferWithClear.capacity();

    for (int i = 0; i < 1000; ++i) {
        bufferWithClear.push_back(i);
    }
    bufferWithClear.clear(); // Preserves capacity

    BOOST_CHECK_GE(bufferWithClear.capacity(), capacityAfterReserve);

    // Pattern 2: Using reassignment (BAD)
    std::vector<int> bufferWithReassign;
    bufferWithReassign.reserve(capacity);

    for (int i = 0; i < 1000; ++i) {
        bufferWithReassign.push_back(i);
    }
    bufferWithReassign = std::vector<int>(); // Deallocates, capacity lost

    BOOST_CHECK_LT(bufferWithReassign.capacity(), capacityAfterReserve); // Capacity lost!
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: ReserveBeforePopulateTests
// ============================================================================
// Tests that verify proper use of reserve() before populating vectors
// From AGENTS.md: call reserve() when size is known; avoid per-frame allocations

BOOST_AUTO_TEST_SUITE(ReserveBeforePopulateTests)

// ----------------------------------------------------------------------------
// Test: Vector reserve prevents reallocations
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestReservePreventReallocations) {
    std::vector<int> withoutReserve;
    std::vector<int> withReserve;

    const size_t targetSize = 10000;

    // Without reserve - may trigger multiple reallocations
    int reallocationCountWithout = 0;
    size_t lastCapacityWithout = 0;
    for (size_t i = 0; i < targetSize; ++i) {
        if (withoutReserve.capacity() > lastCapacityWithout) {
            reallocationCountWithout++;
            lastCapacityWithout = withoutReserve.capacity();
        }
        withoutReserve.push_back(static_cast<int>(i));
    }

    // With reserve - should have zero reallocations
    withReserve.reserve(targetSize);
    int reallocationCountWith = 0;
    size_t lastCapacityWith = withReserve.capacity();
    for (size_t i = 0; i < targetSize; ++i) {
        if (withReserve.capacity() > lastCapacityWith) {
            reallocationCountWith++;
            lastCapacityWith = withReserve.capacity();
        }
        withReserve.push_back(static_cast<int>(i));
    }

    // Verify reserve() prevented reallocations
    BOOST_CHECK_GT(reallocationCountWithout, 0);  // Multiple reallocations without reserve
    BOOST_CHECK_EQUAL(reallocationCountWith, 0);  // Zero reallocations with reserve
}

// ----------------------------------------------------------------------------
// Test: Reserve with headroom pattern
// ----------------------------------------------------------------------------
// From AIManager.cpp: "Reserve with 10% headroom for growth"
// This prevents reallocations when entity count grows slightly

BOOST_AUTO_TEST_CASE(TestReserveWithHeadroom) {
    const size_t expectedEntityCount = 1000;
    const float headroomFactor = 1.1f; // 10% headroom

    std::vector<Vector2D> entityPositions;
    entityPositions.reserve(static_cast<size_t>(expectedEntityCount * headroomFactor));

    size_t capacityWithHeadroom = entityPositions.capacity();
    BOOST_CHECK_GE(capacityWithHeadroom, static_cast<size_t>(expectedEntityCount * headroomFactor));

    // Populate to expected count
    for (size_t i = 0; i < expectedEntityCount; ++i) {
        entityPositions.push_back(Vector2D(static_cast<float>(i), 0.0f));
    }
    BOOST_CHECK_EQUAL(entityPositions.capacity(), capacityWithHeadroom); // No reallocation

    // Add 10% more entities (within headroom)
    for (size_t i = 0; i < static_cast<size_t>(expectedEntityCount * 0.1f); ++i) {
        entityPositions.push_back(Vector2D(static_cast<float>(i + expectedEntityCount), 0.0f));
    }

    // Should still fit within reserved capacity
    BOOST_CHECK_EQUAL(entityPositions.capacity(), capacityWithHeadroom); // No reallocation
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: MemberVariableBufferTests
// ============================================================================
// Tests that verify member variable buffer reuse patterns
// From AGENTS.md: reuse member buffers on hot paths

BOOST_AUTO_TEST_SUITE(MemberVariableBufferTests)

// ----------------------------------------------------------------------------
// Test: Member variable buffer vs local variable pattern
// ----------------------------------------------------------------------------

class ManagerSimulation {
public:
    // GOOD: Member variable buffer (reused across frames)
    std::vector<float> m_reusableBuffer;

    void updateWithReuse(int entityCount) {
        m_reusableBuffer.clear(); // Preserves capacity
        for (int i = 0; i < entityCount; ++i) {
            m_reusableBuffer.push_back(static_cast<float>(i));
        }
    }

    void updateWithoutReuse(int entityCount) {
        // BAD: Local variable (allocated every call)
        std::vector<float> localBuffer;
        for (int i = 0; i < entityCount; ++i) {
            localBuffer.push_back(static_cast<float>(i));
        }
        // localBuffer deallocated here - capacity lost
    }
};

BOOST_AUTO_TEST_CASE(TestMemberVsLocalBufferPattern) {
    ManagerSimulation manager;

    const int entityCount = 1000;

    // First call with reuse - establishes capacity
    manager.updateWithReuse(entityCount);
    size_t establishedCapacity = manager.m_reusableBuffer.capacity();
    BOOST_CHECK_GE(establishedCapacity, static_cast<size_t>(entityCount));

    // Subsequent calls with reuse - capacity preserved
    for (int frame = 0; frame < 10; ++frame) {
        manager.updateWithReuse(entityCount);
        BOOST_CHECK_GE(manager.m_reusableBuffer.capacity(), establishedCapacity);
    }

    // Local buffer pattern requires reallocation every frame
    // (Can't test capacity directly since it's local, but demonstrates anti-pattern)
    for (int frame = 0; frame < 10; ++frame) {
        manager.updateWithoutReuse(entityCount);
    }
}

// ----------------------------------------------------------------------------
// Test: Pre-allocated batch buffer pattern
// ----------------------------------------------------------------------------
// From AIManager.cpp: "Pre-allocated batch buffers for distance/position calculations"

class BatchProcessorSimulation {
public:
    std::vector<std::vector<float>> m_batchBuffers;

    void init(size_t batchCount, size_t batchSize) {
        m_batchBuffers.resize(batchCount);
        for (size_t i = 0; i < batchCount; ++i) {
            m_batchBuffers[i].reserve(batchSize);
        }
    }

    void processBatch(size_t batchIndex, const std::vector<float>& data) {
        if (batchIndex >= m_batchBuffers.size()) return;

        m_batchBuffers[batchIndex].clear(); // Preserves capacity
        for (float value : data) {
            m_batchBuffers[batchIndex].push_back(value);
        }
    }
};

BOOST_AUTO_TEST_CASE(TestBatchBufferPreallocation) {
    BatchProcessorSimulation processor;

    const size_t batchCount = 8;
    const size_t batchSize = 500;

    // Initialize with pre-allocated buffers
    processor.init(batchCount, batchSize);

    // Verify all batches have reserved capacity
    for (size_t i = 0; i < batchCount; ++i) {
        BOOST_CHECK_GE(processor.m_batchBuffers[i].capacity(), batchSize);
    }

    // Process batches multiple times
    std::vector<float> testData;
    testData.reserve(batchSize);
    for (size_t i = 0; i < batchSize; ++i) {
        testData.push_back(static_cast<float>(i));
    }

    for (int frame = 0; frame < 20; ++frame) {
        for (size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
            size_t capacityBefore = processor.m_batchBuffers[batchIndex].capacity();
            processor.processBatch(batchIndex, testData);
            size_t capacityAfter = processor.m_batchBuffers[batchIndex].capacity();

            // Capacity should be preserved across all frames
            BOOST_CHECK_EQUAL(capacityAfter, capacityBefore);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
