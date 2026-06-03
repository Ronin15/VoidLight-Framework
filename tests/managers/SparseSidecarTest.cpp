/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE SparseSidecarTests
#include <boost/test/unit_test.hpp>

#include "managers/SparseSidecar.hpp"
#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

// ============================================================================
// Helper types
// ============================================================================

struct SimpleData
{
    int value{0};
    float weight{1.0f};
};

// ============================================================================
// BASIC OPERATIONS
// ============================================================================

BOOST_AUTO_TEST_SUITE(BasicOperations)

BOOST_AUTO_TEST_CASE(InsertAndHas)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(10);

    BOOST_CHECK(!sidecar.has(0));
    BOOST_CHECK(!sidecar.has(5));

    sidecar.apply(5).value = 42;

    BOOST_CHECK(!sidecar.has(0));
    BOOST_CHECK(sidecar.has(5));
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 1u);
}

BOOST_AUTO_TEST_CASE(GetReturnsNullptrOnAbsent)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(10);

    BOOST_CHECK(sidecar.get(3) == nullptr);
    BOOST_CHECK(static_cast<const SparseSidecar<SimpleData>&>(sidecar).get(3) == nullptr);
}

BOOST_AUTO_TEST_CASE(GetReturnsMutablePointerOnPresent)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(10);

    sidecar.apply(7).value = 99;

    SimpleData* p = sidecar.get(7);
    BOOST_REQUIRE(p != nullptr);
    BOOST_CHECK_EQUAL(p->value, 99);

    // Mutation via pointer
    p->value = 100;
    BOOST_CHECK_EQUAL(sidecar.get(7)->value, 100);
}

BOOST_AUTO_TEST_CASE(ApplyOnExistingReturnsExisting)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(10);

    sidecar.apply(4).value = 7;
    sidecar.apply(4).weight = 3.14f;

    // Calling apply() again must not create a new entry
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 1u);
    BOOST_CHECK_EQUAL(sidecar.get(4)->value, 7);
    BOOST_CHECK_CLOSE(sidecar.get(4)->weight, 3.14f, 0.001f);
}

BOOST_AUTO_TEST_CASE(RemoveSingle)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(10);

    sidecar.apply(2).value = 1;
    sidecar.apply(6).value = 2;
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 2u);

    sidecar.remove(2);
    BOOST_CHECK(!sidecar.has(2));
    BOOST_CHECK(sidecar.has(6));
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 1u);
    BOOST_CHECK(sidecar.get(2) == nullptr);
}

BOOST_AUTO_TEST_CASE(RemoveAbsentIsNoOp)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(10);

    sidecar.apply(3).value = 5;
    // Remove entity that was never inserted
    sidecar.remove(8);

    BOOST_CHECK_EQUAL(sidecar.activeCount(), 1u);
    BOOST_CHECK(sidecar.has(3));
}

BOOST_AUTO_TEST_CASE(RemoveAllFor)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(10);

    sidecar.apply(1).value = 10;
    sidecar.removeAllFor(1);

    BOOST_CHECK(!sidecar.has(1));
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 0u);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SWAP-POP CORRECTNESS
// ============================================================================

BOOST_AUTO_TEST_SUITE(SwapPopCorrectness)

/**
 * Three-entity scenario: insert A, B, C then remove B.
 * C must be displaced to B's dense slot.
 * Verify C's sparse entry now points to the correct (displaced) dense slot.
 */
BOOST_AUTO_TEST_CASE(RemoveMidElementPatchesDisplacedSparse)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(20);

    const uint32_t A = 2, B = 7, C = 15;
    sidecar.apply(A).value = 100;
    sidecar.apply(B).value = 200;
    sidecar.apply(C).value = 300;

    BOOST_REQUIRE_EQUAL(sidecar.activeCount(), 3u);

    // Remove the middle element
    sidecar.remove(B);

    // B is gone
    BOOST_CHECK(!sidecar.has(B));
    BOOST_CHECK(sidecar.get(B) == nullptr);

    // A and C still present with correct values
    BOOST_REQUIRE(sidecar.has(A));
    BOOST_REQUIRE(sidecar.has(C));
    BOOST_CHECK_EQUAL(sidecar.get(A)->value, 100);
    BOOST_CHECK_EQUAL(sidecar.get(C)->value, 300);

    // Dense array integrity: owners() must be consistent
    auto owners = sidecar.owners();
    auto dense  = sidecar.dense();
    BOOST_REQUIRE_EQUAL(owners.size(), 2u);
    BOOST_REQUIRE_EQUAL(dense.size(), 2u);

    for (size_t i = 0; i < owners.size(); ++i)
    {
        uint32_t edmIdx = owners[i];
        BOOST_CHECK(sidecar.has(edmIdx));
        BOOST_CHECK(sidecar.get(edmIdx) == &dense[i]);
    }
}

BOOST_AUTO_TEST_CASE(RemoveFirstElementOfThree)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(20);

    const uint32_t A = 0, B = 5, C = 10;
    sidecar.apply(A).value = 1;
    sidecar.apply(B).value = 2;
    sidecar.apply(C).value = 3;

    sidecar.remove(A);

    BOOST_CHECK(!sidecar.has(A));
    BOOST_CHECK(sidecar.has(B));
    BOOST_CHECK(sidecar.has(C));
    BOOST_CHECK_EQUAL(sidecar.get(B)->value, 2);
    BOOST_CHECK_EQUAL(sidecar.get(C)->value, 3);
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 2u);

    // Cross-check owners table
    auto owners = sidecar.owners();
    auto dense  = sidecar.dense();
    for (size_t i = 0; i < owners.size(); ++i)
    {
        BOOST_CHECK(sidecar.get(owners[i]) == &dense[i]);
    }
}

BOOST_AUTO_TEST_CASE(RemoveLastElementOfThree)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(20);

    const uint32_t A = 1, B = 9, C = 18;
    sidecar.apply(A).value = 10;
    sidecar.apply(B).value = 20;
    sidecar.apply(C).value = 30;

    sidecar.remove(C);

    BOOST_CHECK(!sidecar.has(C));
    BOOST_CHECK_EQUAL(sidecar.get(A)->value, 10);
    BOOST_CHECK_EQUAL(sidecar.get(B)->value, 20);
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 2u);
}

BOOST_AUTO_TEST_CASE(InsertAfterRemoveReusesSlot)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(20);

    sidecar.apply(3).value = 7;
    sidecar.remove(3);

    // Re-insert same entity
    sidecar.apply(3).value = 42;

    BOOST_CHECK(sidecar.has(3));
    BOOST_CHECK_EQUAL(sidecar.get(3)->value, 42);
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 1u);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// RESIZE STABILITY
// ============================================================================

BOOST_AUTO_TEST_SUITE(ResizeStability)

BOOST_AUTO_TEST_CASE(ExistingEntriesSurviveGrow)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(5);

    sidecar.apply(2).value = 99;
    sidecar.apply(4).value = 77;

    // Grow to a larger size
    sidecar.resizeSparse(100);

    BOOST_CHECK(sidecar.has(2));
    BOOST_CHECK(sidecar.has(4));
    BOOST_CHECK_EQUAL(sidecar.get(2)->value, 99);
    BOOST_CHECK_EQUAL(sidecar.get(4)->value, 77);
    BOOST_CHECK_EQUAL(sidecar.activeCount(), 2u);
}

BOOST_AUTO_TEST_CASE(NewIndicesAfterGrowAreAbsent)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(5);
    sidecar.apply(1).value = 1;

    sidecar.resizeSparse(50);

    for (uint32_t i = 2; i < 50; ++i)
    {
        BOOST_CHECK(!sidecar.has(i));
        BOOST_CHECK(sidecar.get(i) == nullptr);
    }
}

BOOST_AUTO_TEST_CASE(IdempotentResize)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(20);
    sidecar.apply(10).value = 5;

    // Resize to same or smaller must be a no-op
    sidecar.resizeSparse(20);
    sidecar.resizeSparse(5);   // Smaller — must not truncate

    BOOST_CHECK(sidecar.has(10));
    BOOST_CHECK_EQUAL(sidecar.get(10)->value, 5);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ITERATION SURFACE
// ============================================================================

BOOST_AUTO_TEST_SUITE(IterationSurface)

BOOST_AUTO_TEST_CASE(DenseSpanCoversAllActiveEntries)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(50);

    const std::vector<uint32_t> indices{3, 7, 12, 25, 48};
    for (uint32_t i = 0; i < static_cast<uint32_t>(indices.size()); ++i)
    {
        sidecar.apply(indices[i]).value = static_cast<int>(i * 10);
    }

    auto dense = sidecar.dense();
    BOOST_CHECK_EQUAL(dense.size(), indices.size());

    // Sum via span
    int sum = 0;
    for (const auto& d : dense) { sum += d.value; }
    BOOST_CHECK_EQUAL(sum, 0 + 10 + 20 + 30 + 40);
}

BOOST_AUTO_TEST_CASE(OwnersAndDenseAreConsistent)
{
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(30);

    sidecar.apply(0).value  = 1;
    sidecar.apply(15).value = 2;
    sidecar.apply(29).value = 3;

    auto owners = sidecar.owners();
    auto dense  = sidecar.dense();

    BOOST_REQUIRE_EQUAL(owners.size(), dense.size());

    for (size_t i = 0; i < owners.size(); ++i)
    {
        uint32_t edmIdx = owners[i];
        BOOST_CHECK(sidecar.has(edmIdx));
        BOOST_CHECK_EQUAL(sidecar.get(edmIdx), &dense[i]);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// STRESS TEST — 10K random inserts and removes
// ============================================================================

BOOST_AUTO_TEST_SUITE(StressTest)

BOOST_AUTO_TEST_CASE(TenThousandRandomInsertsAndRemoves)
{
    constexpr size_t ENTITY_COUNT = 10'000;
    SparseSidecar<SimpleData> sidecar;
    sidecar.resizeSparse(ENTITY_COUNT);

    std::mt19937 rng{42};
    std::uniform_int_distribution<uint32_t> idxDist{0, static_cast<uint32_t>(ENTITY_COUNT - 1)};

    // Ground-truth set of active indices
    std::unordered_set<uint32_t> groundTruth;

    // Phase 1: insert half the entities
    for (size_t i = 0; i < ENTITY_COUNT / 2; ++i)
    {
        uint32_t idx = idxDist(rng);
        sidecar.apply(idx).value = static_cast<int>(idx);
        groundTruth.insert(idx);
    }

    BOOST_CHECK_EQUAL(sidecar.activeCount(), groundTruth.size());

    // Phase 2: random removes and inserts
    for (size_t i = 0; i < ENTITY_COUNT; ++i)
    {
        uint32_t idx = idxDist(rng);
        if (groundTruth.count(idx))
        {
            sidecar.remove(idx);
            groundTruth.erase(idx);
        }
        else
        {
            sidecar.apply(idx).value = static_cast<int>(idx);
            groundTruth.insert(idx);
        }
    }

    // Verify active count
    BOOST_CHECK_EQUAL(sidecar.activeCount(), groundTruth.size());

    // Verify presence/absence for every entity
    for (uint32_t i = 0; i < ENTITY_COUNT; ++i)
    {
        bool expected = (groundTruth.count(i) > 0);
        BOOST_CHECK_EQUAL(sidecar.has(i), expected);
        if (expected)
        {
            BOOST_CHECK(sidecar.get(i) != nullptr);
            BOOST_CHECK_EQUAL(sidecar.get(i)->value, static_cast<int>(i));
        }
        else
        {
            BOOST_CHECK(sidecar.get(i) == nullptr);
        }
    }

    // owners/dense consistency check
    auto owners = sidecar.owners();
    auto dense  = sidecar.dense();
    BOOST_REQUIRE_EQUAL(owners.size(), dense.size());
    for (size_t i = 0; i < owners.size(); ++i)
    {
        BOOST_CHECK(sidecar.get(owners[i]) == &dense[i]);
    }
}

BOOST_AUTO_TEST_SUITE_END()
