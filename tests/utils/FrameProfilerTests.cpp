#define BOOST_TEST_MODULE FrameProfilerTests

#include <boost/test/unit_test.hpp>

#include "utils/FrameProfiler.hpp"

#include <chrono>
#include <thread>

using namespace VoidLight;

#ifdef VOIDLIGHT_FRAME_PROFILER_ENABLED

namespace {

void briefPause()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

} // namespace

BOOST_AUTO_TEST_SUITE(FrameProfilerStateTests)

BOOST_AUTO_TEST_CASE(TestOverlayAndSuppressionState)
{
    auto& profiler = FrameProfiler::Instance();
    const double originalThreshold = profiler.getThresholdMs();
    const bool originalOverlayVisible = profiler.isOverlayVisible();
    const uint64_t baseFrameCount = profiler.getFrameCount();
    const uint32_t baseHitchCount = profiler.getHitchCount();

    profiler.setThresholdMs(-1.0);

    profiler.toggleOverlay();
    BOOST_CHECK_EQUAL(profiler.isOverlayVisible(), !originalOverlayVisible);
    profiler.toggleOverlay();
    BOOST_CHECK_EQUAL(profiler.isOverlayVisible(), originalOverlayVisible);

    profiler.suppressFrames(1);
    BOOST_CHECK(profiler.isSuppressed());

    profiler.beginFrame();
    profiler.endFrame();

    BOOST_CHECK_EQUAL(profiler.getFrameCount(), baseFrameCount + 1);
    BOOST_CHECK_EQUAL(profiler.getHitchCount(), baseHitchCount);
    BOOST_CHECK(!profiler.isSuppressed());

    profiler.beginFrame();
    profiler.endFrame();

    BOOST_CHECK_EQUAL(profiler.getFrameCount(), baseFrameCount + 2);
    BOOST_CHECK_EQUAL(profiler.getHitchCount(), baseHitchCount + 1);

    profiler.setThresholdMs(originalThreshold);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(FrameProfilerTimingTests)

BOOST_AUTO_TEST_CASE(TestScopedTimersRecordDurations)
{
    auto& profiler = FrameProfiler::Instance();
    const double originalThreshold = profiler.getThresholdMs();
    const uint64_t baseFrameCount = profiler.getFrameCount();
    const uint32_t baseHitchCount = profiler.getHitchCount();

    profiler.setThresholdMs(1000.0);
    profiler.beginFrame();

    {
        ScopedPhaseTimer phaseTimer(FramePhase::Update);
        briefPause();
    }

    {
        ScopedManagerTimer managerTimer(ManagerPhase::AI);
        briefPause();
    }

    {
        ScopedRenderTimerGPU renderTimer(RenderPhase::GPUScenePass);
        briefPause();
    }

    profiler.endFrame();

    BOOST_CHECK_EQUAL(profiler.getFrameCount(), baseFrameCount + 1);
    BOOST_CHECK_EQUAL(profiler.getHitchCount(), baseHitchCount);
    BOOST_CHECK_GT(profiler.getLastFrameTimeMs(), 0.0);
    BOOST_CHECK_GT(profiler.getPhaseTimeMs(FramePhase::Update), 0.0);
    BOOST_CHECK_GT(profiler.getManagerTimeMs(ManagerPhase::AI), 0.0);
    BOOST_CHECK_GT(profiler.getRenderTimeMs(RenderPhase::GPUScenePass), 0.0);

    profiler.setThresholdMs(originalThreshold);
}

BOOST_AUTO_TEST_SUITE_END()

#else

BOOST_AUTO_TEST_CASE(TestReleaseStubNoOps)
{
    auto& profiler = FrameProfiler::Instance();
    profiler.setThresholdMs(12.5);
    profiler.toggleOverlay();
    profiler.suppressFrames(2);
    profiler.beginFrame();
    profiler.endFrame();

    BOOST_CHECK_EQUAL(profiler.getThresholdMs(), 0.0);
    BOOST_CHECK(!profiler.isOverlayVisible());
    BOOST_CHECK(!profiler.isSuppressed());
    BOOST_CHECK_EQUAL(profiler.getFrameCount(), 0u);
    BOOST_CHECK_EQUAL(profiler.getHitchCount(), 0u);
    BOOST_CHECK_EQUAL(profiler.getLastFrameTimeMs(), 0.0);
    BOOST_CHECK_EQUAL(profiler.getPhaseTimeMs(FramePhase::Update), 0.0);
    BOOST_CHECK_EQUAL(profiler.getManagerTimeMs(ManagerPhase::AI), 0.0);
    BOOST_CHECK_EQUAL(profiler.getRenderTimeMs(RenderPhase::GPUScenePass), 0.0);
}

#endif
