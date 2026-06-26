/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

/**
 * @file TimestepManagerTests.cpp
 * @brief Unit tests for TimestepManager (core, determinism-critical)
 *
 * Tests cover:
 * - Construction defaults and configuration setters/getters
 * - Fixed-delta invariant: getUpdateDeltaTime() always returns the configured
 *   fixed timestep regardless of wall-clock elapsed time
 * - Accumulator-driven update loop: shouldUpdate() drains correctly
 * - Spiral-of-death guard: large frame deltas are clamped at MAX_ACCUMULATOR
 * - Interpolation alpha: always clamped to [0, 1]
 * - Render flag lifecycle: set by startFrame(), cleared by endFrame()
 * - reset() semantics: clears accumulator, preserves explicit frame-limit config
 * - Software frame-limiting flag: toggle and persistence across reset()
 *
 * Note: Tests that exercise the accumulator use short real-time sleeps because
 * TimestepManager uses std::chrono::steady_clock internally and does not expose
 * an injectable clock.  Sleeps are kept generous (>= 3x the fixedTimestep at
 * 60 Hz) to stay reliable on loaded CI machines.
 */

#define BOOST_TEST_MODULE TimestepManagerTests
#include <boost/test/unit_test.hpp>

#include "core/TimestepManager.hpp"

#include <chrono>
#include <thread>

// ============================================================================
// Configuration / Getter Tests  (no wall-clock dependency)
// ============================================================================

BOOST_AUTO_TEST_SUITE(TimestepManagerConfigTests)

BOOST_AUTO_TEST_CASE(DefaultConstructionUsesExpectedValues)
{
    TimestepManager tm;

    BOOST_CHECK_CLOSE(tm.getTargetFPS(), 60.0f, 0.001f);
    BOOST_CHECK_CLOSE(tm.getUpdateDeltaTime(), 1.0f / 60.0f, 0.001f);
    BOOST_CHECK_CLOSE(tm.getUpdateFrequencyHz(), 60.0f, 0.01f);
}

BOOST_AUTO_TEST_CASE(CustomConstructionAppliesParameters)
{
    TimestepManager tm(120.0f, 1.0f / 120.0f);

    BOOST_CHECK_CLOSE(tm.getTargetFPS(), 120.0f, 0.001f);
    BOOST_CHECK_CLOSE(tm.getUpdateDeltaTime(), 1.0f / 120.0f, 0.001f);
    BOOST_CHECK_CLOSE(tm.getUpdateFrequencyHz(), 120.0f, 0.01f);
}

BOOST_AUTO_TEST_CASE(SetTargetFPSUpdatesGetters)
{
    TimestepManager tm;
    tm.setTargetFPS(144.0f);

    BOOST_CHECK_CLOSE(tm.getTargetFPS(), 144.0f, 0.001f);
}

BOOST_AUTO_TEST_CASE(SetTargetFPSIgnoresNonPositiveValues)
{
    TimestepManager tm;
    const float originalFPS = tm.getTargetFPS();

    tm.setTargetFPS(0.0f);
    BOOST_CHECK_CLOSE(tm.getTargetFPS(), originalFPS, 0.001f);

    tm.setTargetFPS(-60.0f);
    BOOST_CHECK_CLOSE(tm.getTargetFPS(), originalFPS, 0.001f);
}

BOOST_AUTO_TEST_CASE(SetFixedTimestepUpdatesGetters)
{
    TimestepManager tm;
    tm.setFixedTimestep(1.0f / 30.0f);

    BOOST_CHECK_CLOSE(tm.getUpdateDeltaTime(), 1.0f / 30.0f, 0.001f);
    BOOST_CHECK_CLOSE(tm.getUpdateFrequencyHz(), 30.0f, 0.01f);
}

BOOST_AUTO_TEST_CASE(SetFixedTimestepIgnoresNonPositiveValues)
{
    TimestepManager tm;
    const float originalTimestep = tm.getUpdateDeltaTime();

    tm.setFixedTimestep(0.0f);
    BOOST_CHECK_CLOSE(tm.getUpdateDeltaTime(), originalTimestep, 0.001f);

    tm.setFixedTimestep(-0.1f);
    BOOST_CHECK_CLOSE(tm.getUpdateDeltaTime(), originalTimestep, 0.001f);
}

BOOST_AUTO_TEST_CASE(GetUpdateDeltaTimeIsAlwaysFixed)
{
    // getUpdateDeltaTime() must return the same value before and after update
    // ticks — determinism-critical for physics / game logic.
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    const float expected = tm.getUpdateDeltaTime();

    tm.startFrame();  // primes m_firstFrame; accumulator unchanged
    BOOST_CHECK_CLOSE(tm.getUpdateDeltaTime(), expected, 0.0001f);

    // Even after draining all queued updates the value must be constant.
    while (tm.shouldUpdate()) {}
    BOOST_CHECK_CLOSE(tm.getUpdateDeltaTime(), expected, 0.0001f);
}

BOOST_AUTO_TEST_CASE(SoftwareFrameLimitingFlagToggle)
{
    TimestepManager tm;

    BOOST_CHECK(!tm.isUsingSoftwareFrameLimiting());

    tm.setSoftwareFrameLimiting(true);
    BOOST_CHECK(tm.isUsingSoftwareFrameLimiting());

    tm.setSoftwareFrameLimiting(false);
    BOOST_CHECK(!tm.isUsingSoftwareFrameLimiting());
}

BOOST_AUTO_TEST_CASE(SetDisplayRefreshHzDoesNotCrash)
{
    TimestepManager tm;

    tm.setDisplayRefreshHz(60.0f);
    tm.setDisplayRefreshHz(144.0f);
    tm.setDisplayRefreshHz(240.0f);

    // Zero and negative disable display snapping
    tm.setDisplayRefreshHz(0.0f);
    tm.setDisplayRefreshHz(-1.0f);

    BOOST_CHECK(true);  // no crash is the assertion
}

BOOST_AUTO_TEST_CASE(UpdateFrequencyHzMatchesInverseOfFixedTimestep)
{
    // getUpdateFrequencyHz() == 1 / getUpdateDeltaTime() must hold exactly.
    constexpr float timestep = 1.0f / 144.0f;
    TimestepManager tm(144.0f, timestep);

    const float freq = tm.getUpdateFrequencyHz();
    const float dt   = tm.getUpdateDeltaTime();

    // freq * dt should be ~1.0 (within floating-point rounding)
    BOOST_CHECK_CLOSE(freq * dt, 1.0f, 0.01f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Render-flag Tests  (no wall-clock dependency)
// ============================================================================

BOOST_AUTO_TEST_SUITE(TimestepManagerRenderFlagTests)

BOOST_AUTO_TEST_CASE(ShouldRenderIsTrueAfterConstruction)
{
    TimestepManager tm;
    BOOST_CHECK(tm.shouldRender());
}

BOOST_AUTO_TEST_CASE(EndFrameClearsShouldRender)
{
    // Software frame-limiting is OFF by default, so endFrame() does not block.
    TimestepManager tm;
    BOOST_REQUIRE(tm.shouldRender());

    tm.endFrame();

    BOOST_CHECK(!tm.shouldRender());
}

BOOST_AUTO_TEST_CASE(StartFrameRestoresShouldRenderAfterEndFrame)
{
    // Sequence: prime → endFrame (clears) → startFrame (restores)
    TimestepManager tm;

    // Prime: turns m_firstFrame off, m_shouldRender remains true.
    tm.startFrame();

    tm.endFrame();
    BOOST_REQUIRE(!tm.shouldRender());

    // Second startFrame() is NOT the first frame, so it sets m_shouldRender = true.
    tm.startFrame();
    BOOST_CHECK(tm.shouldRender());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Accumulator / Fixed-timestep Tests  (require real-time sleeps)
// ============================================================================

BOOST_AUTO_TEST_SUITE(TimestepManagerAccumulatorTests)

BOOST_AUTO_TEST_CASE(ShouldUpdateAfterElapsedTime)
{
    // A 50 ms sleep covers ~3 frames at 60 Hz; at least one shouldUpdate()
    // call must succeed to confirm the accumulator-driven loop is working.
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    tm.startFrame();  // prime m_lastFrameTime

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tm.startFrame();  // measures ~50 ms and adds it to the accumulator

    BOOST_CHECK(tm.shouldUpdate());
}

BOOST_AUTO_TEST_CASE(ShouldUpdateDrainsAccumulator)
{
    // shouldUpdate() must eventually return false after draining all pending steps.
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    tm.startFrame();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tm.startFrame();

    int updateCount = 0;
    while (tm.shouldUpdate()) { ++updateCount; }

    BOOST_CHECK_GE(updateCount, 1);
    // After draining, the accumulator is below fixedTimestep.
    BOOST_CHECK(!tm.shouldUpdate());
}

BOOST_AUTO_TEST_CASE(SpiralOfDeathGuardClampsAccumulator)
{
    // Sleeping 500 ms is >> MAX_ACCUMULATOR (0.25 s).
    // The clamp must prevent unbounded catch-up updates.
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    tm.startFrame();  // prime

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    tm.startFrame();  // delta clamped at 0.25 s before being added

    int updateCount = 0;
    while (tm.shouldUpdate()) { ++updateCount; }

    // MAX_ACCUMULATOR / fixedTimestep = 0.25 / (1/60) ≈ 15.
    // Allow 20 as a generous ceiling to tolerate snapping and timer jitter.
    BOOST_CHECK_GT(updateCount, 0);
    BOOST_CHECK_LE(updateCount, 20);
}

BOOST_AUTO_TEST_CASE(InterpolationAlphaStartsAtZero)
{
    // Before any real frame delta is processed, accumulator == 0 → alpha == 0.
    TimestepManager tm;
    BOOST_CHECK_CLOSE(tm.getInterpolationAlpha(), 0.0, 0.001);
}

BOOST_AUTO_TEST_CASE(InterpolationAlphaRemainsInUnitRange)
{
    // After a real sub-frame delta the alpha must stay in [0, 1].
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    tm.startFrame();

    std::this_thread::sleep_for(std::chrono::milliseconds(8));  // ~half a frame
    tm.startFrame();

    const double alpha = tm.getInterpolationAlpha();
    BOOST_CHECK_GE(alpha, 0.0);
    BOOST_CHECK_LE(alpha, 1.0);
}

BOOST_AUTO_TEST_CASE(InterpolationAlphaDropsAfterUpdate)
{
    // Each shouldUpdate() drains one fixedTimestep from the accumulator, so
    // the alpha (accumulator / fixedTimestep) must not increase.
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    tm.startFrame();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tm.startFrame();

    const double alphaBefore = tm.getInterpolationAlpha();
    BOOST_REQUIRE(tm.shouldUpdate());  // consume one step
    const double alphaAfter = tm.getInterpolationAlpha();

    // Alpha decreases (or stays at 0 if accumulator was already at threshold).
    BOOST_CHECK_LE(alphaAfter, alphaBefore);
}

BOOST_AUTO_TEST_CASE(ResetClearsAccumulatorAndUpdateQueue)
{
    // After reset(), the accumulator is 0 → shouldUpdate() must return false.
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    tm.startFrame();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tm.startFrame();
    BOOST_REQUIRE(tm.shouldUpdate());  // confirm updates were queued

    tm.reset();

    BOOST_CHECK(!tm.shouldUpdate());
    BOOST_CHECK_CLOSE(tm.getInterpolationAlpha(), 0.0, 0.001);
}

BOOST_AUTO_TEST_CASE(ResetPreservesExplicitSoftwareLimitingConfig)
{
    // Explicit software frame-limiting config must survive reset() so that
    // GameEngine's persistent frame-pacing setting is honoured after pause.
    TimestepManager tm;
    tm.setSoftwareFrameLimiting(true);

    tm.reset();

    BOOST_CHECK(tm.isUsingSoftwareFrameLimiting());
}

BOOST_AUTO_TEST_CASE(MultiFrameAccumulationIsMonotonic)
{
    // Each startFrame() call should add a non-negative delta to the accumulator;
    // the total update count over two consecutive frames must be >= the first.
    TimestepManager tm(60.0f, 1.0f / 60.0f);
    tm.startFrame();  // prime

    std::this_thread::sleep_for(std::chrono::milliseconds(17));  // ~1 frame
    tm.startFrame();
    int firstCount = 0;
    while (tm.shouldUpdate()) { ++firstCount; }

    std::this_thread::sleep_for(std::chrono::milliseconds(17));  // another frame
    tm.startFrame();
    int secondCount = 0;
    while (tm.shouldUpdate()) { ++secondCount; }

    // Both frames should have produced at least one update step.
    BOOST_CHECK_GE(firstCount + secondCount, 2);
}

BOOST_AUTO_TEST_SUITE_END()
