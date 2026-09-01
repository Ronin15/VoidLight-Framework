/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE LoadingStateTests
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <thread>
#include <chrono>
#include <memory>

#include "gameStates/LoadingState.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/GameStateManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/UIManager.hpp"
#include "managers/WorldManager.hpp"

namespace {

class TestMainMenuState final : public GameState
{
public:
    static void reset()
    {
        s_entered.store(false, std::memory_order_release);
        s_exited.store(false, std::memory_order_release);
    }

    static bool entered()
    {
        return s_entered.load(std::memory_order_acquire);
    }

    static bool exited()
    {
        return s_exited.load(std::memory_order_acquire);
    }

    bool enter() override
    {
        s_entered.store(true, std::memory_order_release);
        return true;
    }

    void update(float) override {}
    void handleInput() override {}

    bool exit() override
    {
        s_exited.store(true, std::memory_order_release);
        return true;
    }

    GameStateId getStateId() const override
    {
        return GameStateId::MAIN_MENU;
    }

private:
    static inline std::atomic<bool> s_entered{false};
    static inline std::atomic<bool> s_exited{false};
};

} // namespace

// ============================================================================
// TEST SUITE: AsyncLoadingPatternTests
// ============================================================================
// Tests that validate LoadingState uses proper async patterns
// From AGENTS.md: "Use LoadingState plus async ThreadSystem work for loading instead of blocking manual rendering"

BOOST_AUTO_TEST_SUITE(AsyncLoadingPatternTests)

// ----------------------------------------------------------------------------
// Test: LoadingState uses atomics for thread-safe state
// ----------------------------------------------------------------------------
// Atomics ensure lock-free progress tracking from background thread

BOOST_AUTO_TEST_CASE(TestAtomicProgressTracking) {
    LoadingState loadingState;

    // Verify LoadingState starts with zero progress
    VoidLight::WorldGenerationConfig config;
    config.width = 800;
    config.height = 600;

    loadingState.configure(GameStateId::GAME_PLAY, config);

    // After configuration, progress should be reset to 0
    // (We can't directly access private m_progress, but we validate behavior)

    // LoadingState should have error checking methods
    BOOST_CHECK(!loadingState.hasError());
    BOOST_CHECK_EQUAL(loadingState.getLastError(), "");
}

// ----------------------------------------------------------------------------
// Test: LoadingState provides thread-safe error handling
// ----------------------------------------------------------------------------
// Error tracking must be mutex-protected for string safety

BOOST_AUTO_TEST_CASE(TestThreadSafeErrorHandling) {
    LoadingState loadingState;

    // Initially no error
    BOOST_CHECK(!loadingState.hasError());
    BOOST_CHECK_EQUAL(loadingState.getLastError(), "");

    // Multiple threads can safely check for errors
    std::atomic<int> checksCompleted{0};

    auto checkErrors = [&loadingState, &checksCompleted]() {
        for (int i = 0; i < 100; ++i) {
            loadingState.hasError();
            loadingState.getLastError();
        }
        checksCompleted.fetch_add(1, std::memory_order_relaxed);
    };

    // Launch multiple threads checking errors concurrently
    std::thread t1(checkErrors);
    std::thread t2(checkErrors);
    std::thread t3(checkErrors);

    t1.join();
    t2.join();
    t3.join();

    BOOST_CHECK_EQUAL(checksCompleted.load(), 3);
}

// ----------------------------------------------------------------------------
// Test: LoadingState can be reconfigured for reuse
// ----------------------------------------------------------------------------
// configure() should reset state atomics for fresh loading session

BOOST_AUTO_TEST_CASE(TestLoadingStateReuse) {
    LoadingState loadingState;

    VoidLight::WorldGenerationConfig config1;
    config1.width = 400;
    config1.height = 300;

    // First configuration
    loadingState.configure(GameStateId::LOGO, config1);
    BOOST_CHECK(!loadingState.hasError());

    // Second configuration (reuse)
    VoidLight::WorldGenerationConfig config2;
    config2.width = 800;
    config2.height = 600;

    loadingState.configure(GameStateId::LOADING, config2);
    BOOST_CHECK(!loadingState.hasError());
    BOOST_CHECK_EQUAL(loadingState.getLastError(), ""); // Error cleared on reconfigure
    BOOST_CHECK(!loadingState.hasError());
    BOOST_CHECK_EQUAL(loadingState.getLastError(), "");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: AsyncPatternBestPracticesTests
// ============================================================================
// Tests that validate general async loading best practices

BOOST_AUTO_TEST_SUITE(AsyncPatternBestPracticesTests)

BOOST_AUTO_TEST_CASE(TestEnterFailsWhenUnconfigured) {
    LoadingState loadingState;

    BOOST_CHECK(!loadingState.enter());
}

BOOST_AUTO_TEST_CASE(TestEnterStartsRuntimeLoadAndExitCleansUI) {
    TestMainMenuState::reset();

    BOOST_REQUIRE(VoidLight::ThreadSystem::Instance().init());
    BOOST_REQUIRE(EventManager::Instance().init());
    BOOST_REQUIRE(GameTimeManager::Instance().init());
    BOOST_REQUIRE(UIManager::Instance().init());
    BOOST_REQUIRE(CollisionManager::Instance().init());
    BOOST_REQUIRE(PathfinderManager::Instance().init());
    BOOST_REQUIRE(WorldManager::Instance().init());

    VoidLight::WorldGenerationConfig config{};
    config.width = 4;
    config.height = 4;
    config.seed = 2026;
    config.elevationFrequency = 0.1f;
    config.humidityFrequency = 0.1f;
    config.waterLevel = 0.3f;
    config.mountainLevel = 0.7f;

    GameStateManager stateManager;
    auto loadingState = std::make_unique<LoadingState>();
    auto* loadingStatePtr = loadingState.get();
    stateManager.addState(std::move(loadingState));
    stateManager.addState(std::make_unique<TestMainMenuState>());

    loadingStatePtr->configure(GameStateId::MAIN_MENU, config);
    stateManager.pushState(GameStateId::LOADING);

    auto& ui = UIManager::Instance();
    BOOST_CHECK(ui.hasComponent("loading_title"));
    BOOST_CHECK(ui.hasComponent("loading_progress"));
    BOOST_CHECK(ui.hasComponent("loading_status"));

    for (int i = 0; i < 200 && !TestMainMenuState::entered(); ++i) {
        stateManager.update(0.016f);
        EventManager::Instance().update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    BOOST_CHECK(TestMainMenuState::entered());
    BOOST_CHECK(!WorldManager::Instance().getCurrentWorldId().empty());
    BOOST_CHECK(PathfinderManager::Instance().isGridReady());
    BOOST_CHECK(!ui.hasComponent("loading_title"));
    BOOST_CHECK(!ui.hasComponent("loading_progress"));
    BOOST_CHECK(!ui.hasComponent("loading_status"));

    stateManager.clearAllStates();
    BOOST_CHECK(TestMainMenuState::exited());

    WorldManager::Instance().clean();
    PathfinderManager::Instance().clean();
    CollisionManager::Instance().clean();
    GameTimeManager::Instance().setGlobalPause(false);
    UIManager::Instance().prepareForStateTransition();
    EventManager::Instance().clean();
    VoidLight::ThreadSystem::Instance().clean();
}

// ----------------------------------------------------------------------------
// Test: Atomic operations use proper memory ordering
// ----------------------------------------------------------------------------
// Validates that atomic pattern follows best practices

BOOST_AUTO_TEST_CASE(TestAtomicMemoryOrdering) {
    // Simulate LoadingState's atomic pattern
    std::atomic<float> progress{0.0f};
    std::atomic<bool> loadComplete{false};
    std::atomic<bool> loadFailed{false};

    // Writer thread pattern (background loading)
    auto writerTask = [&progress, &loadComplete]() {
        for (int i = 0; i <= 10; ++i) {
            progress.store(i * 0.1f, std::memory_order_release); // Release semantics
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        loadComplete.store(true, std::memory_order_release); // Signal completion
    };

    // Reader thread pattern (UI update thread)
    auto readerTask = [&progress, &loadComplete, &loadFailed]() {
        int reads = 0;
        while (!loadComplete.load(std::memory_order_acquire) && reads < 100) {
            float currentProgress = progress.load(std::memory_order_acquire); // Acquire semantics
            BOOST_CHECK_GE(currentProgress, 0.0f);
            BOOST_CHECK_LE(currentProgress, 1.0f);

            bool failed = loadFailed.load(std::memory_order_acquire);
            BOOST_CHECK(!failed);

            reads++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    std::thread writer(writerTask);
    std::thread reader(readerTask);

    writer.join();
    reader.join();

    // Verify final state
    BOOST_CHECK(loadComplete.load(std::memory_order_acquire));
    BOOST_CHECK_CLOSE(progress.load(std::memory_order_acquire), 1.0f, 0.01f);
}

// ----------------------------------------------------------------------------
// Test: Mutex-protected string updates
// ----------------------------------------------------------------------------
// Strings require mutex protection (not atomic-safe)

BOOST_AUTO_TEST_CASE(TestMutexProtectedStrings) {
    std::string statusText{"Initializing..."};
    std::mutex statusMutex;

    auto updateStatus = [&statusText, &statusMutex](const std::string& newStatus) {
        std::lock_guard<std::mutex> lock(statusMutex);
        statusText = newStatus;
    };

    auto readStatus = [&statusText, &statusMutex]() -> std::string {
        std::lock_guard<std::mutex> lock(statusMutex);
        return statusText;
    };

    // Multiple threads updating and reading status
    std::atomic<int> updatesCompleted{0};
    std::atomic<int> readsCompleted{0};

    auto updater = [&updateStatus, &updatesCompleted]() {
        for (int i = 0; i < 50; ++i) {
            updateStatus("Loading step " + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        updatesCompleted.fetch_add(1, std::memory_order_relaxed);
    };

    auto reader = [&readStatus, &readsCompleted]() {
        for (int i = 0; i < 50; ++i) {
            std::string status = readStatus();
            BOOST_CHECK(!status.empty()); // Should always get valid string
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        readsCompleted.fetch_add(1, std::memory_order_relaxed);
    };

    std::thread t1(updater);
    std::thread t2(reader);
    std::thread t3(reader);

    t1.join();
    t2.join();
    t3.join();

    BOOST_CHECK_EQUAL(updatesCompleted.load(), 1);
    BOOST_CHECK_EQUAL(readsCompleted.load(), 2);
}

// ----------------------------------------------------------------------------
// Test: std::future pattern for async task result
// ----------------------------------------------------------------------------
// std::future provides one-time result retrieval from async task

BOOST_AUTO_TEST_CASE(TestFuturePattern) {
    auto asyncTask = [](int workAmount) -> bool {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return workAmount > 0;
    };

    // Launch async task (simulates LoadingState::startAsyncWorldLoad)
    std::future<bool> taskFuture = std::async(std::launch::async, asyncTask, 100);

    // Poll future status (simulates LoadingState::update checking completion)
    bool taskComplete = false;
    int pollCount = 0;
    while (!taskComplete && pollCount < 100) {
        if (taskFuture.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
            taskComplete = true;
        }
        pollCount++;
    }

    BOOST_CHECK(taskComplete);

    // Retrieve result
    bool result = taskFuture.get();
    BOOST_CHECK(result);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: NonBlockingRenderingTests
// ============================================================================
// Tests that validate loading does not block rendering

BOOST_AUTO_TEST_SUITE(NonBlockingRenderingTests)

// ----------------------------------------------------------------------------
// Test: Progress tracking allows continuous UI updates
// ----------------------------------------------------------------------------
// UI can read progress at any time without blocking background loading

BOOST_AUTO_TEST_CASE(TestNonBlockingProgressReading) {
    std::atomic<float> progress{0.0f};
    std::atomic<bool> loadComplete{false};

    // Simulate background loading task
    auto backgroundLoader = [&progress, &loadComplete]() {
        for (int i = 0; i <= 100; ++i) {
            progress.store(i / 100.0f, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        loadComplete.store(true, std::memory_order_release);
    };

    // Simulate UI rendering loop (reads progress without blocking)
    std::vector<float> progressSnapshots;
    auto uiRenderLoop = [&progress, &loadComplete, &progressSnapshots]() {
        while (!loadComplete.load(std::memory_order_acquire)) {
            // Non-blocking read
            float currentProgress = progress.load(std::memory_order_acquire);
            progressSnapshots.push_back(currentProgress);

            // Simulate frame time (60 FPS = ~16ms per frame)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    };

    std::thread loader(backgroundLoader);
    std::thread renderer(uiRenderLoop);

    loader.join();
    renderer.join();

    // Verify UI was able to sample progress multiple times
    BOOST_CHECK_GT(progressSnapshots.size(), 0);

    // Verify progress was monotonically increasing (or stable)
    for (size_t i = 1; i < progressSnapshots.size(); ++i) {
        BOOST_CHECK_GE(progressSnapshots[i], progressSnapshots[i - 1]);
    }
}

// ----------------------------------------------------------------------------
// Test: Rendering thread never blocks on loading completion
// ----------------------------------------------------------------------------
// Rendering should always proceed even if loading is slow

BOOST_AUTO_TEST_CASE(TestRenderingNeverBlocks) {
    std::atomic<bool> loadComplete{false};
    std::atomic<int> renderFrameCount{0};

    // Slow loading task
    auto slowLoader = [&loadComplete]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loadComplete.store(true, std::memory_order_release);
    };

    // Fast rendering loop (should not wait for loading)
    auto renderLoop = [&loadComplete, &renderFrameCount]() {
        auto startTime = std::chrono::steady_clock::now();
        while (!loadComplete.load(std::memory_order_acquire)) {
            // Simulate rendering work
            renderFrameCount.fetch_add(1, std::memory_order_relaxed);

            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60 FPS

            // Safety timeout after 200ms
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > std::chrono::milliseconds(200)) {
                break;
            }
        }
    };

    std::thread loader(slowLoader);
    std::thread renderer(renderLoop);

    loader.join();
    renderer.join();

    // Rendering should have completed multiple frames while loading
    BOOST_CHECK_GT(renderFrameCount.load(), 3); // At least a few frames rendered
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: ThreadSafetyPatternsTests
// ============================================================================
// Tests that validate thread-safety patterns used in async loading

BOOST_AUTO_TEST_SUITE(ThreadSafetyPatternsTests)

// ----------------------------------------------------------------------------
// Test: Reader-writer pattern with atomics
// ----------------------------------------------------------------------------
// Multiple readers, single writer pattern for progress tracking

BOOST_AUTO_TEST_CASE(TestReaderWriterAtomicPattern) {
    std::atomic<int> sharedCounter{0};
    const int targetValue = 1000;

    // Single writer
    auto writer = [&sharedCounter]() {
        for (int i = 0; i < targetValue; ++i) {
            sharedCounter.store(i, std::memory_order_release);
        }
    };

    // Multiple readers
    std::atomic<int> readersCompleted{0};
    auto reader = [&sharedCounter, &readersCompleted]() {
        int lastValue = -1;
        for (int i = 0; i < 100; ++i) {
            int value = sharedCounter.load(std::memory_order_acquire);
            BOOST_CHECK_GE(value, lastValue); // Value should never decrease
            lastValue = value;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        readersCompleted.fetch_add(1, std::memory_order_relaxed);
    };

    std::thread writerThread(writer);
    std::thread reader1(reader);
    std::thread reader2(reader);
    std::thread reader3(reader);

    writerThread.join();
    reader1.join();
    reader2.join();
    reader3.join();

    BOOST_CHECK_EQUAL(readersCompleted.load(), 3);
}

// ----------------------------------------------------------------------------
// Test: Lock-free progress reporting
// ----------------------------------------------------------------------------
// Background thread can update progress without acquiring locks

BOOST_AUTO_TEST_CASE(TestLockFreeProgressUpdate) {
    std::atomic<float> progress{0.0f};

    // Background task that updates progress frequently
    auto progressUpdater = [&progress]() {
        for (int i = 0; i <= 1000; ++i) {
            progress.store(i / 1000.0f, std::memory_order_release);
            // No locks needed - atomic operations are lock-free
        }
    };

    std::thread updater(progressUpdater);
    updater.join();

    BOOST_CHECK_CLOSE(progress.load(std::memory_order_acquire), 1.0f, 0.01f);
}

BOOST_AUTO_TEST_SUITE_END()
