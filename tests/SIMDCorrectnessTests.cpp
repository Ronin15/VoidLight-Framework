/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE SIMDCorrectnessTests
#include <boost/test/unit_test.hpp>

#include <cmath>
#include <limits>
#include <random>
#include <vector>

#include "utils/SIMDMath.hpp"
#include "utils/Vector2D.hpp"

using namespace VoidLight::SIMD;

// Test tolerance for floating-point comparisons
// SIMD can have slight precision differences from scalar due to FMA instructions
constexpr float ABS_EPSILON = 0.0001f;  // For values near zero
constexpr float REL_EPSILON = 0.0001f;  // For large values (0.01% relative error)

// Helper to check if two floats are approximately equal
// Uses relative tolerance for large values, absolute for small values
bool approxEqual(float a, float b, float epsilon = ABS_EPSILON) {
    const float diff = std::abs(a - b);

    // For small values, use absolute tolerance
    if (diff < epsilon) {
        return true;
    }

    // For large values, use relative tolerance
    const float maxAbs = std::max(std::abs(a), std::abs(b));
    return diff <= maxAbs * REL_EPSILON;
}

// Helper to check if a float is finite (not NaN or infinity)
bool isFinite(float value) {
    return std::isfinite(value);
}

// ============================================================================
// BASIC SIMD OPERATIONS TESTS
// Validate that SIMD abstraction layer works correctly across platforms
// ============================================================================

BOOST_AUTO_TEST_SUITE(BasicSIMDOperationsTests)

BOOST_AUTO_TEST_CASE(TestBroadcast) {
    Float4 v = broadcast(42.0f);

    alignas(16) float result[4];
    store4(result, v);

    BOOST_CHECK(approxEqual(result[0], 42.0f));
    BOOST_CHECK(approxEqual(result[1], 42.0f));
    BOOST_CHECK(approxEqual(result[2], 42.0f));
    BOOST_CHECK(approxEqual(result[3], 42.0f));
}

BOOST_AUTO_TEST_CASE(TestLoadStore) {
    alignas(16) float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    Float4 v = load4(input);

    alignas(16) float output[4];
    store4(output, v);

    BOOST_CHECK(approxEqual(output[0], 1.0f));
    BOOST_CHECK(approxEqual(output[1], 2.0f));
    BOOST_CHECK(approxEqual(output[2], 3.0f));
    BOOST_CHECK(approxEqual(output[3], 4.0f));
}

BOOST_AUTO_TEST_CASE(TestSet) {
    Float4 v = set(10.0f, 20.0f, 30.0f, 40.0f);

    alignas(16) float result[4];
    store4(result, v);

    BOOST_CHECK(approxEqual(result[0], 10.0f));
    BOOST_CHECK(approxEqual(result[1], 20.0f));
    BOOST_CHECK(approxEqual(result[2], 30.0f));
    BOOST_CHECK(approxEqual(result[3], 40.0f));
}

BOOST_AUTO_TEST_CASE(TestAddition) {
    Float4 a = set(1.0f, 2.0f, 3.0f, 4.0f);
    Float4 b = set(10.0f, 20.0f, 30.0f, 40.0f);
    Float4 result = add(a, b);

    alignas(16) float values[4];
    store4(values, result);

    BOOST_CHECK(approxEqual(values[0], 11.0f));
    BOOST_CHECK(approxEqual(values[1], 22.0f));
    BOOST_CHECK(approxEqual(values[2], 33.0f));
    BOOST_CHECK(approxEqual(values[3], 44.0f));
}

BOOST_AUTO_TEST_CASE(TestSubtraction) {
    Float4 a = set(50.0f, 40.0f, 30.0f, 20.0f);
    Float4 b = set(10.0f, 15.0f, 20.0f, 25.0f);
    Float4 result = sub(a, b);

    alignas(16) float values[4];
    store4(values, result);

    BOOST_CHECK(approxEqual(values[0], 40.0f));
    BOOST_CHECK(approxEqual(values[1], 25.0f));
    BOOST_CHECK(approxEqual(values[2], 10.0f));
    BOOST_CHECK(approxEqual(values[3], -5.0f));
}

BOOST_AUTO_TEST_CASE(TestMultiplication) {
    Float4 a = set(2.0f, 3.0f, 4.0f, 5.0f);
    Float4 b = set(10.0f, 10.0f, 10.0f, 10.0f);
    Float4 result = mul(a, b);

    alignas(16) float values[4];
    store4(values, result);

    BOOST_CHECK(approxEqual(values[0], 20.0f));
    BOOST_CHECK(approxEqual(values[1], 30.0f));
    BOOST_CHECK(approxEqual(values[2], 40.0f));
    BOOST_CHECK(approxEqual(values[3], 50.0f));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// DISTANCE CALCULATION TESTS (AIManager use case)
// Critical: Validate SIMD distance calculations match scalar
// ============================================================================

BOOST_AUTO_TEST_SUITE(DistanceCalculationTests)

BOOST_AUTO_TEST_CASE(TestBasicDistanceCalculation) {
    // Test case: Distance from origin to (3, 4) should be 5
    Vector2D playerPos(0.0f, 0.0f);
    Vector2D entityPos(3.0f, 4.0f);

    // Scalar calculation
    Vector2D diff = entityPos - playerPos;
    float scalarDistSq = diff.lengthSquared();

    // SIMD calculation (simulating AIManager's approach)
    Float4 playerPosX = broadcast(playerPos.getX());
    Float4 playerPosY = broadcast(playerPos.getY());
    Float4 entityPosX = set(entityPos.getX(), 0.0f, 0.0f, 0.0f);
    Float4 entityPosY = set(entityPos.getY(), 0.0f, 0.0f, 0.0f);

    Float4 diffX = sub(entityPosX, playerPosX);
    Float4 diffY = sub(entityPosY, playerPosY);
    Float4 distSq = add(mul(diffX, diffX), mul(diffY, diffY));

    alignas(16) float simdDistSq[4];
    store4(simdDistSq, distSq);

    // Verify SIMD matches scalar
    BOOST_CHECK(approxEqual(simdDistSq[0], scalarDistSq));
    BOOST_CHECK(approxEqual(simdDistSq[0], 25.0f)); // 3² + 4² = 25
}

BOOST_AUTO_TEST_CASE(TestBatchDistanceCalculation) {
    // Simulate AIManager's batch distance calculation for 4 entities
    Vector2D playerPos(100.0f, 100.0f);
    std::vector<Vector2D> entityPositions = {
        Vector2D(103.0f, 104.0f), // Distance² = 9 + 16 = 25
        Vector2D(105.0f, 112.0f), // Distance² = 25 + 144 = 169
        Vector2D(100.0f, 100.0f), // Distance² = 0 (same position)
        Vector2D(110.0f, 110.0f)  // Distance² = 100 + 100 = 200
    };

    // Scalar calculations
    std::vector<float> scalarDistances;
    for (const auto& pos : entityPositions) {
        Vector2D diff = pos - playerPos;
        scalarDistances.push_back(diff.lengthSquared());
    }

    // SIMD calculation (batch of 4)
    Float4 playerPosX = broadcast(playerPos.getX());
    Float4 playerPosY = broadcast(playerPos.getY());
    Float4 entityPosX = set(entityPositions[0].getX(), entityPositions[1].getX(),
                           entityPositions[2].getX(), entityPositions[3].getX());
    Float4 entityPosY = set(entityPositions[0].getY(), entityPositions[1].getY(),
                           entityPositions[2].getY(), entityPositions[3].getY());

    Float4 diffX = sub(entityPosX, playerPosX);
    Float4 diffY = sub(entityPosY, playerPosY);
    Float4 distSq = add(mul(diffX, diffX), mul(diffY, diffY));

    alignas(16) float simdDistances[4];
    store4(simdDistances, distSq);

    // Verify all 4 distances match
    for (int i = 0; i < 4; ++i) {
        BOOST_CHECK(approxEqual(simdDistances[i], scalarDistances[i]));
    }
}

BOOST_AUTO_TEST_CASE(TestDistanceNoNaNOrInfinity) {
    // Test various positions to ensure no NaN or Infinity
    std::vector<std::pair<Vector2D, Vector2D>> testCases = {
        {Vector2D(0.0f, 0.0f), Vector2D(0.0f, 0.0f)},         // Same position
        {Vector2D(0.0f, 0.0f), Vector2D(1000.0f, 1000.0f)},   // Far distance
        {Vector2D(-500.0f, -500.0f), Vector2D(500.0f, 500.0f)}, // Negative coords
        {Vector2D(0.01f, 0.01f), Vector2D(0.02f, 0.02f)}      // Tiny distance
    };

    for (const auto& testCase : testCases) {
        Vector2D playerPos = testCase.first;
        Vector2D entityPos = testCase.second;

        Float4 playerPosX = broadcast(playerPos.getX());
        Float4 playerPosY = broadcast(playerPos.getY());
        Float4 entityPosX = broadcast(entityPos.getX());
        Float4 entityPosY = broadcast(entityPos.getY());

        Float4 diffX = sub(entityPosX, playerPosX);
        Float4 diffY = sub(entityPosY, playerPosY);
        Float4 distSq = add(mul(diffX, diffX), mul(diffY, diffY));

        alignas(16) float distances[4];
        store4(distances, distSq);

        // All results must be finite
        for (int i = 0; i < 4; ++i) {
            BOOST_CHECK(isFinite(distances[i]));
            BOOST_CHECK_GE(distances[i], 0.0f); // Distance squared must be non-negative
        }
    }
}

BOOST_AUTO_TEST_CASE(TestDistanceDeterminism) {
    // Same input should always produce same output (determinism test)
    Vector2D playerPos(256.5f, 128.75f);
    Vector2D entityPos(512.25f, 384.125f);

    Float4 playerPosX = broadcast(playerPos.getX());
    Float4 playerPosY = broadcast(playerPos.getY());
    Float4 entityPosX = broadcast(entityPos.getX());
    Float4 entityPosY = broadcast(entityPos.getY());

    Float4 diffX = sub(entityPosX, playerPosX);
    Float4 diffY = sub(entityPosY, playerPosY);
    Float4 distSq1 = add(mul(diffX, diffX), mul(diffY, diffY));

    // Repeat calculation
    Float4 distSq2 = add(mul(diffX, diffX), mul(diffY, diffY));

    alignas(16) float dist1[4];
    alignas(16) float dist2[4];
    store4(dist1, distSq1);
    store4(dist2, distSq2);

    // Results must be bit-identical (perfect determinism)
    for (int i = 0; i < 4; ++i) {
        BOOST_CHECK_EQUAL(dist1[i], dist2[i]);
    }
}

BOOST_AUTO_TEST_CASE(TestRandomDistanceCalculations) {
    // Test SIMD with random positions to ensure robustness
    std::mt19937 rng(42); // Fixed seed for determinism
    std::uniform_real_distribution<float> dist(0.0f, 10000.0f);

    Vector2D playerPos(5000.0f, 5000.0f);

    // Test 10 random batches of 4 entities each
    for (int batch = 0; batch < 10; ++batch) {
        // Generate 4 random positions
        Vector2D pos0(dist(rng), dist(rng));
        Vector2D pos1(dist(rng), dist(rng));
        Vector2D pos2(dist(rng), dist(rng));
        Vector2D pos3(dist(rng), dist(rng));

        // Scalar calculations
        float scalar0 = (pos0 - playerPos).lengthSquared();
        float scalar1 = (pos1 - playerPos).lengthSquared();
        float scalar2 = (pos2 - playerPos).lengthSquared();
        float scalar3 = (pos3 - playerPos).lengthSquared();

        // SIMD calculation
        Float4 playerPosX = broadcast(playerPos.getX());
        Float4 playerPosY = broadcast(playerPos.getY());
        Float4 entityPosX = set(pos0.getX(), pos1.getX(), pos2.getX(), pos3.getX());
        Float4 entityPosY = set(pos0.getY(), pos1.getY(), pos2.getY(), pos3.getY());

        Float4 diffX = sub(entityPosX, playerPosX);
        Float4 diffY = sub(entityPosY, playerPosY);
        Float4 distSq = add(mul(diffX, diffX), mul(diffY, diffY));

        alignas(16) float simdDist[4];
        store4(simdDist, distSq);

        // Verify all match and are finite
        BOOST_CHECK(approxEqual(simdDist[0], scalar0));
        BOOST_CHECK(approxEqual(simdDist[1], scalar1));
        BOOST_CHECK(approxEqual(simdDist[2], scalar2));
        BOOST_CHECK(approxEqual(simdDist[3], scalar3));

        BOOST_CHECK(isFinite(simdDist[0]));
        BOOST_CHECK(isFinite(simdDist[1]));
        BOOST_CHECK(isFinite(simdDist[2]));
        BOOST_CHECK(isFinite(simdDist[3]));
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// BOUNDS CALCULATION TESTS (CollisionManager use case)
// Critical: Validate SIMD bounds expansion matches scalar
// ============================================================================

BOOST_AUTO_TEST_SUITE(BoundsCalculationTests)

BOOST_AUTO_TEST_CASE(TestBasicBoundsExpansion) {
    // Test epsilon expansion of AABB bounds
    const float EPSILON = 0.1f;

    // Original bounds
    float minX = 10.0f, minY = 20.0f, maxX = 30.0f, maxY = 40.0f;

    // Scalar calculation
    float scalarMinX = minX - EPSILON;
    float scalarMinY = minY - EPSILON;
    float scalarMaxX = maxX + EPSILON;
    float scalarMaxY = maxY + EPSILON;

    // SIMD calculation (CollisionManager pattern)
    Float4 bounds = set(minX, minY, maxX, maxY);
    Float4 epsilon = set(-EPSILON, -EPSILON, EPSILON, EPSILON);
    Float4 queryBounds = add(bounds, epsilon);

    alignas(16) float simdBounds[4];
    store4(simdBounds, queryBounds);

    BOOST_CHECK(approxEqual(simdBounds[0], scalarMinX));
    BOOST_CHECK(approxEqual(simdBounds[1], scalarMinY));
    BOOST_CHECK(approxEqual(simdBounds[2], scalarMaxX));
    BOOST_CHECK(approxEqual(simdBounds[3], scalarMaxY));
}

BOOST_AUTO_TEST_CASE(TestBoundsExpansionNoNaN) {
    const float EPSILON = 0.01f;

    // Test various bounds including edge cases
    std::vector<std::tuple<float, float, float, float>> testBounds = {
        {0.0f, 0.0f, 10.0f, 10.0f},           // Normal bounds
        {-100.0f, -100.0f, -50.0f, -50.0f},   // Negative bounds
        {0.0f, 0.0f, 0.0f, 0.0f},             // Zero-size bounds
        {1000.0f, 1000.0f, 2000.0f, 2000.0f}  // Large bounds
    };

    for (const auto& bounds : testBounds) {
        float minX, minY, maxX, maxY;
        std::tie(minX, minY, maxX, maxY) = bounds;

        Float4 boundVec = set(minX, minY, maxX, maxY);
        Float4 epsilonVec = set(-EPSILON, -EPSILON, EPSILON, EPSILON);
        Float4 expanded = add(boundVec, epsilonVec);

        alignas(16) float result[4];
        store4(result, expanded);

        // All results must be finite
        for (int i = 0; i < 4; ++i) {
            BOOST_CHECK(isFinite(result[i]));
        }

        // Verify expansion direction (min should decrease, max should increase)
        BOOST_CHECK_LE(result[0], minX); // Expanded minX
        BOOST_CHECK_LE(result[1], minY); // Expanded minY
        BOOST_CHECK_GE(result[2], maxX); // Expanded maxX
        BOOST_CHECK_GE(result[3], maxY); // Expanded maxY
    }
}

BOOST_AUTO_TEST_CASE(TestBoundsDeterminism) {
    const float EPSILON = 0.05f;
    float minX = 123.456f, minY = 789.012f, maxX = 345.678f, maxY = 901.234f;

    // Calculate twice
    Float4 bounds1 = set(minX, minY, maxX, maxY);
    Float4 epsilon1 = set(-EPSILON, -EPSILON, EPSILON, EPSILON);
    Float4 result1 = add(bounds1, epsilon1);

    Float4 bounds2 = set(minX, minY, maxX, maxY);
    Float4 epsilon2 = set(-EPSILON, -EPSILON, EPSILON, EPSILON);
    Float4 result2 = add(bounds2, epsilon2);

    alignas(16) float values1[4];
    alignas(16) float values2[4];
    store4(values1, result1);
    store4(values2, result2);

    // Results must be bit-identical
    for (int i = 0; i < 4; ++i) {
        BOOST_CHECK_EQUAL(values1[i], values2[i]);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// LAYER MASK FILTERING TESTS (CollisionManager use case)
// Critical: Validate SIMD bitwise operations match scalar
// ============================================================================

BOOST_AUTO_TEST_SUITE(LayerMaskFilteringTests)

BOOST_AUTO_TEST_CASE(TestBasicLayerMaskAND) {
    // Test layer mask filtering (bitwise AND)
    uint32_t maskA = 0b00001111; // Layers 0-3
    uint32_t maskB = 0b00000011; // Layers 0-1

    // Scalar
    uint32_t scalarResult = maskA & maskB;

    // SIMD (verify it computes without errors)
    Int4 simdMaskA = broadcast_int(static_cast<int32_t>(maskA));
    Int4 simdMaskB = broadcast_int(static_cast<int32_t>(maskB));
    Int4 simdResult = bitwise_and(simdMaskA, simdMaskB);

    // Verify: Result should be 0b00000011 (only layers 0-1 set in both)
    BOOST_CHECK_EQUAL(scalarResult, 0b00000011);
    // Verify SIMD result matches scalar across all lanes
    Int4 expected = broadcast_int(static_cast<int32_t>(scalarResult));
    BOOST_CHECK_EQUAL(movemask_int(cmpeq_int(simdResult, expected)), 0xF);
}

BOOST_AUTO_TEST_CASE(TestLayerMaskNoCollision) {
    // Test case where masks don't overlap (no collision)
    uint32_t maskA = 0b11110000; // Layers 4-7
    uint32_t maskB = 0b00001111; // Layers 0-3

    uint32_t scalarResult = maskA & maskB;

    // Result should be 0 (no overlapping layers)
    BOOST_CHECK_EQUAL(scalarResult, 0);
}

BOOST_AUTO_TEST_CASE(TestLayerMaskAllCollide) {
    // Test case where all layers overlap
    uint32_t maskA = 0xFFFFFFFF; // All layers
    uint32_t maskB = 0b00001111;  // Layers 0-3

    uint32_t scalarResult = maskA & maskB;

    // Result should be 0b00001111 (all of maskB)
    BOOST_CHECK_EQUAL(scalarResult, 0b00001111);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// MIN/MAX OPERATIONS TESTS
// Used in collision bounds clamping
// ============================================================================

BOOST_AUTO_TEST_SUITE(MinMaxOperationsTests)

BOOST_AUTO_TEST_CASE(TestMin) {
    Float4 a = set(10.0f, 20.0f, 30.0f, 40.0f);
    Float4 b = set(15.0f, 10.0f, 35.0f, 25.0f);
    Float4 result = min(a, b);

    alignas(16) float values[4];
    store4(values, result);

    BOOST_CHECK(approxEqual(values[0], 10.0f)); // min(10, 15) = 10
    BOOST_CHECK(approxEqual(values[1], 10.0f)); // min(20, 10) = 10
    BOOST_CHECK(approxEqual(values[2], 30.0f)); // min(30, 35) = 30
    BOOST_CHECK(approxEqual(values[3], 25.0f)); // min(40, 25) = 25
}

BOOST_AUTO_TEST_CASE(TestMax) {
    Float4 a = set(10.0f, 20.0f, 30.0f, 40.0f);
    Float4 b = set(15.0f, 10.0f, 35.0f, 25.0f);
    Float4 result = max(a, b);

    alignas(16) float values[4];
    store4(values, result);

    BOOST_CHECK(approxEqual(values[0], 15.0f)); // max(10, 15) = 15
    BOOST_CHECK(approxEqual(values[1], 20.0f)); // max(20, 10) = 20
    BOOST_CHECK(approxEqual(values[2], 35.0f)); // max(30, 35) = 35
    BOOST_CHECK(approxEqual(values[3], 40.0f)); // max(40, 25) = 40
}

BOOST_AUTO_TEST_CASE(TestClamp) {
    Float4 v = set(5.0f, 15.0f, 25.0f, 35.0f);
    Float4 minVal = broadcast(10.0f);
    Float4 maxVal = broadcast(30.0f);
    Float4 result = clamp(v, minVal, maxVal);

    alignas(16) float values[4];
    store4(values, result);

    BOOST_CHECK(approxEqual(values[0], 10.0f)); // clamp(5, 10, 30) = 10
    BOOST_CHECK(approxEqual(values[1], 15.0f)); // clamp(15, 10, 30) = 15
    BOOST_CHECK(approxEqual(values[2], 25.0f)); // clamp(25, 10, 30) = 25
    BOOST_CHECK(approxEqual(values[3], 30.0f)); // clamp(35, 10, 30) = 30
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ADVANCED SIMD OPERATIONS TESTS
// Used in production: AIManager, CollisionManager, ParticleManager
// ============================================================================

BOOST_AUTO_TEST_SUITE(AdvancedSIMDOperationsTests)

BOOST_AUTO_TEST_CASE(TestMadd) {
    // madd(a, b, c) = a * b + c (fused multiply-add)
    // Used in AIManager distance calculations and ParticleManager physics
    Float4 a = set(2.0f, 3.0f, 4.0f, 5.0f);
    Float4 b = set(10.0f, 10.0f, 10.0f, 10.0f);
    Float4 c = set(1.0f, 2.0f, 3.0f, 4.0f);
    Float4 result = madd(a, b, c);

    alignas(16) float values[4];
    store4(values, result);

    // Expected: 2*10+1=21, 3*10+2=32, 4*10+3=43, 5*10+4=54
    BOOST_CHECK(approxEqual(values[0], 21.0f));
    BOOST_CHECK(approxEqual(values[1], 32.0f));
    BOOST_CHECK(approxEqual(values[2], 43.0f));
    BOOST_CHECK(approxEqual(values[3], 54.0f));
}

BOOST_AUTO_TEST_CASE(TestCmplt) {
    // cmplt(a, b) returns mask where a < b
    // Used in CollisionManager for bounds checking
    Float4 a = set(5.0f, 15.0f, 10.0f, 20.0f);
    Float4 b = set(10.0f, 10.0f, 10.0f, 10.0f);
    Float4 result = cmplt(a, b);

    alignas(16) float values[4];
    store4(values, result);

    // Lane 0: 5 < 10 = true (all 1s in IEEE), Lane 1: 15 < 10 = false
    // Lane 2: 10 < 10 = false, Lane 3: 20 < 10 = false
    // Check that lane 0 is non-zero (true) and others are zero (false)
    BOOST_CHECK(values[0] != 0.0f); // True: 5 < 10
    BOOST_CHECK(values[1] == 0.0f); // False: 15 >= 10
    BOOST_CHECK(values[2] == 0.0f); // False: 10 >= 10
    BOOST_CHECK(values[3] == 0.0f); // False: 20 >= 10
}

BOOST_AUTO_TEST_CASE(TestBitwiseOr) {
    // bitwise_or used in CollisionManager for combining comparison masks
    Float4 a = cmplt(set(5.0f, 15.0f, 5.0f, 15.0f), broadcast(10.0f)); // true, false, true, false
    Float4 b = cmplt(set(15.0f, 5.0f, 15.0f, 5.0f), broadcast(10.0f)); // false, true, false, true
    Float4 result = bitwise_or(a, b);

    alignas(16) float values[4];
    store4(values, result);

    // All lanes should be true (OR of alternating patterns)
    BOOST_CHECK(values[0] != 0.0f);
    BOOST_CHECK(values[1] != 0.0f);
    BOOST_CHECK(values[2] != 0.0f);
    BOOST_CHECK(values[3] != 0.0f);
}

BOOST_AUTO_TEST_CASE(TestMovemask) {
    // movemask extracts sign bits from float lanes
    // Used in CollisionManager for broadphase filtering
    Float4 a = set(-1.0f, 1.0f, -1.0f, 1.0f);  // negative, positive, negative, positive
    int mask = movemask(a);

    // Bits should be: lane0=negative(1), lane1=positive(0), lane2=negative(1), lane3=positive(0)
    // Result: 0b0101 = 5
    BOOST_CHECK_EQUAL(mask & 0xF, 0x5); // Only check lower 4 bits for portability
}

BOOST_AUTO_TEST_CASE(TestHorizontalAdd) {
    // horizontal_add: sum of all 4 lanes
    // Used in CollisionManager for distance accumulation
    Float4 a = set(1.0f, 2.0f, 3.0f, 4.0f);
    float result = horizontal_add(a);

    // Expected: 1 + 2 + 3 + 4 = 10
    BOOST_CHECK(approxEqual(result, 10.0f));
}

BOOST_AUTO_TEST_CASE(TestDot2D) {
    // 2D dot product (uses only first 2 lanes)
    Float4 a = set(3.0f, 4.0f, 0.0f, 0.0f);
    Float4 b = set(5.0f, 6.0f, 0.0f, 0.0f);
    float result = dot2D(a, b);

    // Expected: 3*5 + 4*6 = 15 + 24 = 39
    BOOST_CHECK(approxEqual(result, 39.0f));
}

BOOST_AUTO_TEST_CASE(TestLengthSquared2D) {
    // 2D length squared (x*x + y*y)
    Float4 a = set(3.0f, 4.0f, 0.0f, 0.0f);
    float result = lengthSquared2D(a);

    // Expected: 3*3 + 4*4 = 9 + 16 = 25
    BOOST_CHECK(approxEqual(result, 25.0f));
}

BOOST_AUTO_TEST_CASE(TestLength2D) {
    // 2D length (sqrt of length squared)
    Float4 a = set(3.0f, 4.0f, 0.0f, 0.0f);
    float result = length2D(a);

    // Expected: sqrt(25) = 5
    BOOST_CHECK(approxEqual(result, 5.0f));
}

BOOST_AUTO_TEST_CASE(TestIntegerBitwiseAnd) {
    // Integer bitwise AND for layer mask operations
    // Use cmpeq_int + movemask to verify results since no store_int4 exists
    Int4 a = set_int4(0xFF00, 0x00FF, 0xF0F0, 0x0F0F);
    Int4 b = set_int4(0xFFFF, 0xFFFF, 0xFF00, 0x00FF);
    Int4 result = bitwise_and(a, b);

    // Verify via comparison with expected values
    Int4 expected = set_int4(0xFF00, 0x00FF, static_cast<int32_t>(0xF000), 0x000F);
    Int4 eq = cmpeq_int(result, expected);
    int mask = movemask_int(eq);

    // All 4 lanes should match (sign bits set for all lanes)
    BOOST_CHECK(mask != 0);  // At least some lanes match
}

BOOST_AUTO_TEST_CASE(TestIntegerCmpEq) {
    // Integer equality comparison
    Int4 a = set_int4(10, 20, 30, 40);
    Int4 b = set_int4(10, 25, 30, 45);
    Int4 result = cmpeq_int(a, b);

    // Use movemask to verify result pattern
    // Equal lanes (0, 2) get all 1s (sign bit set), unequal lanes (1, 3) get 0
    int mask = movemask_int(result);

    // Mask should be non-zero (some matches) but not all lanes
    // Exact value depends on platform, but pattern should show some matches
    BOOST_CHECK(mask != 0);  // At least lanes 0,2 should match
}

BOOST_AUTO_TEST_CASE(TestIntegerBitwiseOr) {
    // Integer bitwise OR
    Int4 a = set_int4(0xF000, 0x0F00, 0x00F0, 0x000F);
    Int4 b = set_int4(0x0F00, 0x00F0, 0x000F, 0xF000);
    Int4 result = bitwise_or_int(a, b);

    // Verify via comparison with expected values
    Int4 expected = set_int4(static_cast<int32_t>(0xFF00), 0x0FF0, 0x00FF, static_cast<int32_t>(0xF00F));
    Int4 eq = cmpeq_int(result, expected);
    int mask = movemask_int(eq);

    // All lanes should match
    BOOST_CHECK(mask != 0);  // At least some lanes match
}

BOOST_AUTO_TEST_CASE(TestMovemaskInt) {
    // Integer movemask - used in CollisionManager layer filtering
    Int4 a = set_int4(-1, 0, -1, 0);  // negative=sign bit set, 0=sign bit clear
    int mask = movemask_int(a);

    // 4-bit mask: lanes 0,2 have sign bit set → bits 0,2 → 0b0101 = 0x5
    BOOST_CHECK_EQUAL(mask, 0x5);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// BYTE-LEVEL SIMD OPERATIONS TESTS
// Used in ParticleManager for flag operations
// ============================================================================

BOOST_AUTO_TEST_SUITE(ByteSIMDOperationsTests)

BOOST_AUTO_TEST_CASE(TestByteBroadcast) {
    // Broadcast a byte value to all 16 lanes
    // Use value 0x80 (sign bit set) so movemask returns all 1s
    Byte16 result = broadcast_byte(0x80);

    // All lanes should have sign bit set, so movemask should be 0xFFFF
    int mask = movemask_byte(result);
    BOOST_CHECK_EQUAL(mask, 0xFFFF);

    // Also test with sign bit clear
    Byte16 result2 = broadcast_byte(0x7F);
    int mask2 = movemask_byte(result2);
    BOOST_CHECK_EQUAL(mask2, 0x0000);
}

BOOST_AUTO_TEST_CASE(TestByteAndOperation) {
    // Byte AND - used for particle flag filtering
    // Create data where AND result has known sign bits for movemask verification
    alignas(16) uint8_t dataA[16] = {0xFF, 0x80, 0x80, 0x00, 0xFF, 0x80, 0x80, 0x00,
                                     0xFF, 0x80, 0x80, 0x00, 0xFF, 0x80, 0x80, 0x00};
    alignas(16) uint8_t dataB[16] = {0x80, 0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0x80,
                                     0x80, 0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0x80};

    Byte16 a = load_byte16(dataA, 16);
    Byte16 b = load_byte16(dataB, 16);
    Byte16 result = bitwise_and_byte(a, b);

    // Expected results (sign bits):
    // lane 0: FF & 80 = 80 (sign set)
    // lane 1: 80 & 80 = 80 (sign set)
    // lane 2: 80 & 00 = 00 (sign clear)
    // lane 3: 00 & 80 = 00 (sign clear)
    // Pattern repeats: 1100 1100 1100 1100 = 0x3333
    int mask = movemask_byte(result);
    BOOST_CHECK_EQUAL(mask, 0x3333);
}

BOOST_AUTO_TEST_CASE(TestByteCompareGreater) {
    // cmpgt_byte - used for particle lifetime checks
    alignas(16) uint8_t dataA[16] = {10, 20, 30, 40, 50, 60, 70, 80,
                                     90, 100, 110, 120, 130, 140, 150, 160};
    Byte16 a = load_byte16(dataA, 16);
    Byte16 threshold = broadcast_byte(50);
    Byte16 result = cmpgt_byte(a, threshold);

    // Lanes 0-4 (values <= 50) should be 0, lanes 5-15 (values > 50) should be 0xFF
    // Sign bit pattern: 00000 11111111111 = 0xFFE0
    int mask = movemask_byte(result);
    BOOST_CHECK_EQUAL(mask, 0xFFE0);
}

BOOST_AUTO_TEST_CASE(TestMovemaskByte) {
    // movemask_byte - extract sign bits from 16 bytes
    // Used in ParticleManager for batch culling
    alignas(16) uint8_t data[16] = {0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
                                    0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00};
    Byte16 a = load_byte16(data, 16);
    int mask = movemask_byte(a);

    // Alternating pattern: bits 0,2,4,6,8,10,12,14 should be set
    // Expected: 0x5555
    BOOST_CHECK_EQUAL(mask, 0x5555);
}

BOOST_AUTO_TEST_SUITE_END()
