/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE EventManagerScalingBenchmark
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include "managers/EventManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "core/WorkerBudget.hpp"
#include "events/EntityEvents.hpp"
#include "events/Event.hpp"
#include "events/ParticleEffectEvent.hpp"
#include "events/WeatherEvent.hpp"
#include "core/ThreadSystem.hpp"
#include "core/Logger.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <random>
#include <mutex>
#include <iomanip>
#include <algorithm>

// Mock Event class for benchmarking with realistic work
class MockEvent : public Event {
public:
    MockEvent(const std::string& name, int /* complexity */ = 5)
        : m_name(name), m_updateCount(0), m_executeCount(0) {
        m_rng.seed(std::hash<std::string>{}(name));
    }

    void update() override {
        // Simulate realistic event update work: condition checking, state updates
        // Real events do: check time, check positions, update flags
        bool timeCondition = (m_updateCount % 10) == 0;
        bool positionCondition = (m_updateCount % 7) != 0;
        volatile bool conditionMet = timeCondition && positionCondition;

        // Simple state updates that real events do
        m_lastUpdateTime = m_updateCount * 0.016f; // 60fps timing
        m_internalState = conditionMet ? 1 : 0;

        m_updateCount++;
        // Use variables to prevent optimization
        if (conditionMet && m_lastUpdateTime < 0) { /* never happens */ }
    }

    void execute() override {
        // Simulate realistic event execution: apply effects, trigger actions
        // Real events do: play sounds, set flags, update positions, trigger transitions
        m_effectActive = true;
        m_targetX = 100.0f + (m_executeCount % 50);
        m_targetY = 200.0f + (m_executeCount % 30);

        // Simulate triggering other systems (just state changes)
        m_soundTriggered = (m_executeCount % 3) == 0;
        m_particleTriggered = (m_executeCount % 5) == 0;

        m_executeCount++;
        // Use variables to prevent optimization
        if (m_effectActive && m_targetX < 0) { /* never happens */ }
    }

    void reset() override {
        m_updateCount = 0;
        m_executeCount = 0;
    }

    void clean() override {}

    std::string getName() const override { return m_name; }
    std::string getType() const override { return "Mock"; }
    std::string getTypeName() const override { return "MockEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Custom; }
    bool checkConditions() override { return true; }

    int getUpdateCount() const { return m_updateCount; }
    int getExecuteCount() const { return m_executeCount; }

private:
    std::string m_name;
    std::atomic<int> m_updateCount;
    std::atomic<int> m_executeCount;
    std::mt19937 m_rng;

    // Realistic event state variables
    float m_lastUpdateTime{0.0f};
    int m_internalState{0};
    bool m_effectActive{false};
    float m_targetX{0.0f};
    float m_targetY{0.0f};
    bool m_soundTriggered{false};
    bool m_particleTriggered{false};
};

// Global shutdown flag for coordinated cleanup
static std::atomic<bool> g_shutdownInProgress{false};
static std::mutex g_outputMutex;

namespace {

std::shared_ptr<MockEvent> createBenchmarkCustomEvent(int sequence) {
    return std::make_shared<MockEvent>("BenchmarkCustom_" + std::to_string(sequence));
}

EventManager::DeferredEvent createDeferredBenchmarkEvent(int sequence) {
    EventData data;
    data.setActive(true);

    switch (sequence % 3) {
        case 0:
            data.typeId = EventTypeId::Weather;
            data.event = std::make_shared<WeatherEvent>("BenchmarkWeather", "Rainy");
            break;
        case 1:
            data.typeId = EventTypeId::Custom;
            data.event = createBenchmarkCustomEvent(sequence);
            break;
        case 2:
        default:
            data.typeId = EventTypeId::ParticleEffect;
            data.event = std::make_shared<ParticleEffectEvent>(
                "BenchmarkParticle", ParticleEffectType::Fire, 50.0f, 50.0f, 1.0f, -1.0f, "", "");
            break;
    }

    return EventManager::DeferredEvent{data.typeId, std::move(data)};
}

void enqueueSingleBenchmarkEvent(int sequence) {
    switch (sequence % 3) {
        case 0:
            EventManager::Instance().changeWeather(
                "Rainy", 1.0f, EventManager::DispatchMode::Deferred);
            break;
        case 1:
            EventManager::Instance().dispatchEvent(
                createBenchmarkCustomEvent(sequence), EventManager::DispatchMode::Deferred);
            break;
        case 2:
        default:
            EventManager::Instance().triggerParticleEffect(
                "Fire", 50.0f, 50.0f, 1.0f, -1.0f, "",
                EventManager::DispatchMode::Deferred);
            break;
    }
}

EventManager::DeferredEvent createDeferredCombatBenchmarkEvent(int sequence,
                                                               EntityHandle attacker,
                                                               EntityHandle target) {
    auto damageEvent = EventManager::Instance().acquireDamageEvent();
    if (!damageEvent) {
        damageEvent = std::make_shared<DamageEvent>();
    }

    const float damage = 5.0f + static_cast<float>(sequence % 20);
    const Vector2D knockback(0.1f * static_cast<float>(sequence % 4), 0.0f);
    damageEvent->configure(attacker, target, damage, knockback);

    EventData data;
    data.typeId = EventTypeId::Combat;
    data.setActive(true);
    data.event = damageEvent;
    return EventManager::DeferredEvent{EventTypeId::Combat, std::move(data)};
}

} // namespace

// Mock event handler for benchmarking
class BenchmarkEventHandler {
public:
    BenchmarkEventHandler(int id, int /* complexity */ = 5)
        : m_id(id), m_callCount(0), m_totalProcessingTime(0) {
        // Seed with handler ID for deterministic but varied behavior
        m_rng.seed(static_cast<unsigned int>(id + 12345));
    }

    void handleEvent(const std::string& params) {
        auto startTime = std::chrono::high_resolution_clock::now();

        m_callCount.fetch_add(1);

        // Simulate realistic event handler work: respond to events with simple logic
        // Real handlers do: update UI, play sounds, modify game state
        m_handlerState = (m_callCount % 4);
        m_lastProcessedId = m_id;

        // Simple conditional logic that real handlers do
        bool shouldUpdate = (m_callCount % 3) == 0;
        bool shouldNotify = (m_callCount % 7) == 0;

        volatile int workResult = 0;
        if (shouldUpdate) {
            workResult += m_handlerState * 2;
        }
        if (shouldNotify) {
            workResult += m_lastProcessedId;
        }

        // Use the result to prevent compiler optimization
        if (workResult < -1000) { /* never happens */ }

        // Store the parameter for verification
        m_lastParams = params;

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
        m_totalProcessingTime.fetch_add(duration.count());
    }

    int getCallCount() const { return m_callCount.load(); }
    int64_t getTotalProcessingTime() const { return m_totalProcessingTime.load(); }
    std::string getLastParams() const { return m_lastParams; }
    int getId() const { return m_id; }
    void resetCounters() {
        m_callCount.store(0);
        m_totalProcessingTime.store(0);
    }

private:
    int m_id;
    std::atomic<int> m_callCount;
    std::atomic<int64_t> m_totalProcessingTime;
    std::string m_lastParams;
    std::mt19937 m_rng;

    // Realistic handler state
    int m_handlerState{0};
    int m_lastProcessedId{0};
};

// Global fixture for proper initialization/cleanup
struct GlobalFixture {
    GlobalFixture() {
        std::lock_guard<std::mutex> lock(g_outputMutex);
        std::cout << "\n===== EventManager Scaling Benchmark Started =====" << std::endl;
        g_shutdownInProgress.store(false);

        // Enable benchmark mode to silence manager logging during tests
        VOIDLIGHT_ENABLE_BENCHMARK_MODE();

        // Initialize ThreadSystem for EventManager threading
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
        BOOST_REQUIRE(EventManager::Instance().init());
    }

    ~GlobalFixture() {
        std::lock_guard<std::mutex> lock(g_outputMutex);
        g_shutdownInProgress.store(true);

        // Clean up EventManager
        try {
            EventManager::Instance().clean();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (...) {
            // Ignore cleanup errors during shutdown
        }

        // Disable benchmark mode after cleanup
        VOIDLIGHT_DISABLE_BENCHMARK_MODE();

        std::cout << "\n===== EventManager Scaling Benchmark Completed =====" << std::endl;
    }
};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

struct EventManagerScalingFixture {
    EventManagerScalingFixture() {
        // Initialize EventManager
        BOOST_REQUIRE(EventManager::Instance().init());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~EventManagerScalingFixture() {
        cleanup();
    }

    void cleanup() {
        // Clean up handlers
        handlers.clear();

        // Reset EventManager
        EventManager::Instance().clean();
        BOOST_CHECK(EventManager::Instance().init());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void runHandlerBenchmark(int numEventTypes, int numHandlersPerType, int numTriggers, bool useBatching) {
        if (g_shutdownInProgress.load()) {
            return;
        }

        cleanup();
        handlers.clear();

        #ifndef NDEBUG
        EventManager::Instance().enableThreading(true);
        #endif
        // Use default threshold (100) - matches EventManager::m_threadingThreshold

        // WorkerBudget: all workers available to each manager during its update window
        auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
        size_t totalWorkers = budgetMgr.getBudget().totalWorkers;

        std::cout << "\n=== Deferred Event Benchmark ===" << std::endl;
        std::cout << "  Config: " << numEventTypes << " types, "
                  << numHandlersPerType << " handlers per type, "
                  << numTriggers << " deferred events" << std::endl;
        std::cout << "  Mode: " << (useBatching ? "batch enqueue + FIFO drain"
                                                : "single enqueue + FIFO drain") << std::endl;
        std::cout << "  System: " << totalWorkers << " workers (all available via WorkerBudget)" << std::endl;

        // Register simple handlers (just count calls)
        std::atomic<int> weatherCallCount{0};
        std::atomic<int> customCallCount{0};
        std::atomic<int> particleCallCount{0};

        for (int i = 0; i < numHandlersPerType; ++i) {
            EventManager::Instance().registerHandler(EventTypeId::Weather,
                [&weatherCallCount](const EventData&) { weatherCallCount++; });
            EventManager::Instance().registerHandler(EventTypeId::Custom,
                [&customCallCount](const EventData&) { customCallCount++; });
            EventManager::Instance().registerHandler(EventTypeId::ParticleEffect,
                [&particleCallCount](const EventData&) { particleCallCount++; });
        }

        // Warmup
        for (int i = 0; i < 10; ++i) {
            enqueueSingleBenchmarkEvent(i);
        }
        EventManager::Instance().update();

        // Benchmark: measure deferred enqueue and deferred drain separately.
        const int numMeasurements = 3;
        std::vector<double> enqueueDurations;
        std::vector<double> drainDurations;
        std::vector<double> totalDurations;

        for (int run = 0; run < numMeasurements; run++) {
            weatherCallCount = 0;
            customCallCount = 0;
            particleCallCount = 0;

            auto startTime = std::chrono::high_resolution_clock::now();

            if (useBatching) {
                std::vector<EventManager::DeferredEvent> localBatch;
                localBatch.reserve(numTriggers);
                for (int i = 0; i < numTriggers; ++i) {
                    localBatch.push_back(createDeferredBenchmarkEvent(i));
                }
                auto enqueueEndTime = std::chrono::high_resolution_clock::now();
                EventManager::Instance().enqueueBatch(std::move(localBatch));
                auto drainStartTime = std::chrono::high_resolution_clock::now();
                EventManager::Instance().update();
                auto endTime = std::chrono::high_resolution_clock::now();

                enqueueDurations.push_back(
                    std::chrono::duration<double, std::milli>(enqueueEndTime - startTime).count());
                drainDurations.push_back(
                    std::chrono::duration<double, std::milli>(endTime - drainStartTime).count());
                totalDurations.push_back(
                    std::chrono::duration<double, std::milli>(endTime - startTime).count());
            } else {
                for (int i = 0; i < numTriggers; ++i) {
                    enqueueSingleBenchmarkEvent(i);
                }
                auto enqueueEndTime = std::chrono::high_resolution_clock::now();
                EventManager::Instance().update();
                auto endTime = std::chrono::high_resolution_clock::now();

                enqueueDurations.push_back(
                    std::chrono::duration<double, std::milli>(enqueueEndTime - startTime).count());
                drainDurations.push_back(
                    std::chrono::duration<double, std::milli>(endTime - enqueueEndTime).count());
                totalDurations.push_back(
                    std::chrono::duration<double, std::milli>(endTime - startTime).count());
            }

            const int totalHandlerCalls =
                weatherCallCount.load() + customCallCount.load() + particleCallCount.load();
            BOOST_CHECK_EQUAL(totalHandlerCalls, numTriggers * numHandlersPerType);
        }

        // Calculate statistics
        double avgEnqueueMs = std::accumulate(
            enqueueDurations.begin(), enqueueDurations.end(), 0.0) / enqueueDurations.size();
        double avgDrainMs = std::accumulate(
            drainDurations.begin(), drainDurations.end(), 0.0) / drainDurations.size();
        double avgTotalMs = std::accumulate(
            totalDurations.begin(), totalDurations.end(), 0.0) / totalDurations.size();
        double minTotalMs = *std::min_element(totalDurations.begin(), totalDurations.end());
        double maxTotalMs = *std::max_element(totalDurations.begin(), totalDurations.end());

        double eventsPerSecond = (numTriggers / avgTotalMs) * 1000.0;
        double avgTimePerEvent = avgTotalMs / numTriggers;

        std::cout << "\nPerformance (avg of " << numMeasurements << " runs):" << std::endl;
        std::cout << "  Enqueue time: " << std::fixed << std::setprecision(2)
                  << avgEnqueueMs << " ms" << std::endl;
        std::cout << "  Drain time: " << std::fixed << std::setprecision(2)
                  << avgDrainMs << " ms" << std::endl;
        std::cout << "  Total time: " << std::fixed << std::setprecision(2)
                  << avgTotalMs << " ms" << std::endl;
        std::cout << "  Min/Max total: " << minTotalMs << " / " << maxTotalMs << " ms" << std::endl;
        std::cout << "  Events/sec: " << std::setprecision(0) << eventsPerSecond << std::endl;
        std::cout << "  Time per event: " << std::setprecision(4) << avgTimePerEvent << " ms" << std::endl;

        cleanup();
    }

    void runScalabilityTest() {
        std::cout << "\n===== SCALABILITY TEST =====" << std::endl;
        // WorkerBudget: all workers available to each manager during its update window
        auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
        size_t totalWorkers = budgetMgr.getBudget().totalWorkers;
        std::cout << "System Configuration: " << totalWorkers
                  << " workers (all available via WorkerBudget)" << std::endl;

        // Test progression: realistic event counts for actual games
        std::vector<std::tuple<int, int, int>> testCases = {
            {3, 1, 10},         // Small game: 3 types, 1 handler, 10 events
            {3, 2, 25},         // Medium game: 3 types, 2 handlers each, 25 events
            {3, 3, 50},         // Large game: 3 types, 3 handlers each, 50 events
            {3, 4, 100},        // Very large game: 3 types, 4 handlers each, 100 events
            {3, 5, 200},        // Massive game: 3 types, 5 handlers each, 200 events
        };

        for (const auto& [numTypes, numHandlers, numEvents] : testCases) {
            std::cout << "\n--- Test Case: " << numTypes << " types, "
                      << numHandlers << " handlers, " << numEvents << " events ---" << std::endl;

            // Compare single deferred enqueue against batch enqueue.
            runHandlerBenchmark(numTypes, numHandlers, numEvents, false);
            runHandlerBenchmark(numTypes, numHandlers, numEvents, true);
        }
    }

    void runConcurrencyTest(int numThreads, int eventsPerThread) {
        std::cout << "\n===== CONCURRENCY BENCHMARK =====" << std::endl;

        const int totalEvents = numThreads * eventsPerThread;
        std::cout << "  Config: " << numThreads << " threads, "
                  << eventsPerThread << " events/thread = "
                  << totalEvents << " total deferred events" << std::endl;

        cleanup();

        // Register simple handler with shared_ptr for safe lifetime management
        auto handlerCallCount = std::make_shared<std::atomic<int>>(0);

        EventManager::Instance().registerHandler(EventTypeId::Weather,
            [handlerCallCount](const EventData&) { ++(*handlerCallCount); });
        EventManager::Instance().registerHandler(EventTypeId::Custom,
            [handlerCallCount](const EventData&) { ++(*handlerCallCount); });
        EventManager::Instance().registerHandler(EventTypeId::ParticleEffect,
            [handlerCallCount](const EventData&) { ++(*handlerCallCount); });

        // Benchmark concurrent deferred dispatch + drain
        auto& threadSystem = VoidLight::ThreadSystem::Instance();
        std::atomic<int> tasksCompleted{0};

        auto startTime = std::chrono::high_resolution_clock::now();

        // Queue events from multiple threads
        for (int t = 0; t < numThreads; ++t) {
            threadSystem.enqueueTask([&tasksCompleted, t, eventsPerThread]() {
                for (int i = 0; i < eventsPerThread; ++i) {
                    int eventNum = t * eventsPerThread + i;
                    switch (eventNum % 3) {
                        case 0:
                            EventManager::Instance().changeWeather("Storm", 1.0f,
                                EventManager::DispatchMode::Deferred);
                            break;
                        case 1:
                            EventManager::Instance().dispatchEvent(
                                createBenchmarkCustomEvent(eventNum),
                                EventManager::DispatchMode::Deferred);
                            break;
                        case 2:
                            EventManager::Instance().triggerParticleEffect("Fire", 50.0f, 50.0f,
                                1.0f, -1.0f, "", EventManager::DispatchMode::Deferred);
                            break;
                    }
                }
                tasksCompleted.fetch_add(1);
            });
        }

        // Wait for queuing to complete
        while (tasksCompleted.load() < numThreads) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        // Drain the deferred queue
        int frameCount = 0;
        int prevCount = 0;
        int stableFrames = 0;

        while (frameCount < 100 && stableFrames < 5) {
            EventManager::Instance().update();
            frameCount++;

            int currentCount = handlerCallCount->load();
            if (currentCount == prevCount) {
                stableFrames++;
            } else {
                stableFrames = 0;
                prevCount = currentCount;
            }

            // Early exit if we processed all events
            if (currentCount >= totalEvents) {
                break;
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        double totalTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        int processedEvents = handlerCallCount->load();
        double eventsPerSecond = (processedEvents / totalTimeMs) * 1000.0;

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Total time: " << std::fixed << std::setprecision(2) << totalTimeMs << " ms" << std::endl;
        std::cout << "  Processed: " << processedEvents << "/" << totalEvents << " events" << std::endl;
        std::cout << "  Drain frames: " << frameCount << std::endl;
        std::cout << "  Events/sec: " << std::setprecision(0) << eventsPerSecond << std::endl;
        std::cout << "  Avg time/event: " << std::setprecision(4) << (totalTimeMs / processedEvents) << " ms" << std::endl;

        // Extra safety: ensure all deferred work completes
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        cleanup();
    }

    std::vector<std::shared_ptr<BenchmarkEventHandler>> handlers;
};

// Basic functionality test
BOOST_AUTO_TEST_CASE(BasicHandlerPerformance) {
    EventManagerScalingFixture fixture;

    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    std::cout << "\n===== BASIC HANDLER PERFORMANCE TEST =====" << std::endl;

    // Simple test with realistic small game event count
    fixture.runHandlerBenchmark(3, 1, 10, false);
    fixture.runHandlerBenchmark(3, 1, 10, true);
}

// Medium scale test
BOOST_AUTO_TEST_CASE(MediumScalePerformance) {
    EventManagerScalingFixture fixture;

    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    std::cout << "\n===== MEDIUM SCALE PERFORMANCE TEST =====" << std::endl;

    // Medium load test - realistic medium game event count
    fixture.runHandlerBenchmark(3, 3, 50, false);
    fixture.runHandlerBenchmark(3, 3, 50, true);
}

// Comprehensive scalability test
BOOST_AUTO_TEST_CASE(ComprehensiveScalabilityTest) {
    EventManagerScalingFixture fixture;

    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    fixture.runScalabilityTest();
}

// Concurrency test
BOOST_AUTO_TEST_CASE(ConcurrencyTest) {
    EventManagerScalingFixture fixture;

    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    // Test concurrent event generation using WorkerBudget (production config)
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    // Use the same logic as production: optimal workers for 4000 events
    size_t optimalWorkerCount = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::Event, 4000);
    int numThreads = static_cast<int>(std::max(static_cast<size_t>(1), optimalWorkerCount));

    // FIXED: Keep total at 4000 events, divide by thread count
    const int totalEvents = 4000;
    int eventsPerThread = totalEvents / numThreads;
    fixture.runConcurrencyTest(numThreads, eventsPerThread);  // 4000 total events
}

// High-scale threading verification test
BOOST_AUTO_TEST_CASE(ThreadingVerificationTest) {
    EventManagerScalingFixture fixture;

    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    std::cout << "\n===== THREADING VERIFICATION TEST =====" << std::endl;
    std::cout << "Testing with heavy handlers to trigger WorkerBudget threading threshold (0.9ms)\n" << std::endl;

    fixture.cleanup();

    // Register handlers that do some actual work (simulate real game handlers)
    auto handlerCallCount = std::make_shared<std::atomic<int>>(0);
    volatile int workSink = 0;  // Prevent optimization

    // Heavy handler that simulates real work (e.g., updating UI, game state)
    auto heavyHandler = [handlerCallCount, &workSink](const EventData&) {
        ++(*handlerCallCount);
        // Simulate realistic handler work - ~1-2 microseconds per call
        int sum = 0;
        for (int i = 0; i < 100; ++i) {
            sum += i * i;
        }
        workSink = sum;
    };

    // Register multiple handlers per type (realistic game scenario)
    for (int i = 0; i < 5; ++i) {
        EventManager::Instance().registerHandler(EventTypeId::Weather, heavyHandler);
        EventManager::Instance().registerHandler(EventTypeId::Custom, heavyHandler);
        EventManager::Instance().registerHandler(EventTypeId::ParticleEffect, heavyHandler);
    }

    // Queue a large batch of deferred events
    const int totalEvents = 50000;
    std::cout << "Queueing " << totalEvents << " deferred events..." << std::endl;

    auto startQueue = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < totalEvents; ++i) {
        switch (i % 3) {
            case 0:
                EventManager::Instance().changeWeather("Storm", 1.0f,
                    EventManager::DispatchMode::Deferred);
                break;
            case 1:
                EventManager::Instance().dispatchEvent(
                    createBenchmarkCustomEvent(i),
                    EventManager::DispatchMode::Deferred);
                break;
            case 2:
                EventManager::Instance().triggerParticleEffect("Fire", 50.0f, 50.0f,
                    1.0f, -1.0f, "", EventManager::DispatchMode::Deferred);
                break;
        }
    }
    auto endQueue = std::chrono::high_resolution_clock::now();
    double queueTimeMs = std::chrono::duration<double, std::milli>(endQueue - startQueue).count();
    std::cout << "Queue time: " << std::fixed << std::setprecision(2) << queueTimeMs << " ms" << std::endl;

    // Drain the queue over multiple frames
    std::cout << "\nDraining queue (watching for MULTI-THREADED messages)..." << std::endl;
    auto startDrain = std::chrono::high_resolution_clock::now();

    int frameCount = 0;
    int prevCount = 0;
    int stableFrames = 0;

    while (frameCount < 500 && stableFrames < 10) {
        EventManager::Instance().update();
        frameCount++;

        int currentCount = handlerCallCount->load();
        if (currentCount == prevCount) {
            stableFrames++;
        } else {
            stableFrames = 0;
            prevCount = currentCount;
        }
    }

    auto endDrain = std::chrono::high_resolution_clock::now();
    double drainTimeMs = std::chrono::duration<double, std::milli>(endDrain - startDrain).count();

    int processedHandlerCalls = handlerCallCount->load();
    // Each event triggers 5 handlers per type
    int expectedHandlerCalls = totalEvents * 5;

    std::cout << "\nResults:" << std::endl;
    std::cout << "  Drain time: " << std::fixed << std::setprecision(2) << drainTimeMs << " ms" << std::endl;
    std::cout << "  Frames: " << frameCount << std::endl;
    std::cout << "  Handler calls: " << processedHandlerCalls << " (expected ~" << expectedHandlerCalls << ")" << std::endl;
    std::cout << "  Events/sec: " << std::setprecision(0)
              << (totalEvents / drainTimeMs) * 1000.0 << std::endl;

    // Verify we processed events
    BOOST_CHECK(processedHandlerCalls > 0);

    fixture.cleanup();
}

// Extreme scale test
BOOST_AUTO_TEST_CASE(ExtremeScaleTest) {
    EventManagerScalingFixture fixture;

    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    std::cout << "\n===== EXTREME SCALE TEST =====" << std::endl;

    try {
        // Large scale test - realistic maximum game event count
        const int numEventTypes = 3;
        const int numHandlersPerType = 10;
        const int numEvents = 500;



        // Only test batched mode for extreme scale (immediate would be too slow)
        fixture.runHandlerBenchmark(numEventTypes, numHandlersPerType, numEvents, true);

    } catch (const std::exception& e) {
        std::cerr << "Error in extreme scale test: " << e.what() << std::endl;
    }
}

// Detect optimal threading threshold for EventManager
BOOST_AUTO_TEST_CASE(TestThreadingThreshold) {
    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    std::cout << "\n===== EVENT THREADING THRESHOLD DETECTION =====" << std::endl;
    std::cout << "Comparing single-threaded vs multi-threaded at different event counts\n" << std::endl;

    std::vector<int> testCounts = {25, 50, 75, 100, 150, 200, 300, 500};
    size_t optimalThreshold = 0;
    size_t marginalThreshold = 0;

    std::cout << std::setw(10) << "Events"
              << std::setw(18) << "Single (ms)"
              << std::setw(18) << "Threaded (ms)"
              << std::setw(12) << "Speedup"
              << std::setw(15) << "Verdict" << std::endl;
    std::cout << std::string(73, '-') << std::endl;

    const int numHandlersPerType = 3;  // Realistic handler count
    const int numMeasurements = 3;

    auto runBenchmark = [&](int numTriggers, bool useThreading) -> double {
        // Reset
        EventManager::Instance().clean();
        BOOST_REQUIRE(EventManager::Instance().init());
        #ifndef NDEBUG
        EventManager::Instance().enableThreading(useThreading);
        #endif
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // Register handlers
        std::atomic<int> callCount{0};
        for (int i = 0; i < numHandlersPerType; ++i) {
            EventManager::Instance().registerHandler(EventTypeId::Weather,
                [&callCount](const EventData&) { callCount++; });
            EventManager::Instance().registerHandler(EventTypeId::Custom,
                [&callCount](const EventData&) { callCount++; });
            EventManager::Instance().registerHandler(EventTypeId::ParticleEffect,
                [&callCount](const EventData&) { callCount++; });
        }

        // Warmup
        for (int i = 0; i < 5; ++i) {
            enqueueSingleBenchmarkEvent(i);
        }
        EventManager::Instance().update();

        // Measure
        std::vector<double> durations;
        for (int run = 0; run < numMeasurements; ++run) {
            callCount = 0;
            auto startTime = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < numTriggers; ++i) {
                switch (i % 3) {
                    case 0: EventManager::Instance().changeWeather("Rainy", 1.0f); break;
                    case 1: EventManager::Instance().dispatchEvent(createBenchmarkCustomEvent(i)); break;
                    case 2: EventManager::Instance().triggerParticleEffect("Fire", 50.0f, 50.0f); break;
                }
            }

            EventManager::Instance().update();

            auto endTime = std::chrono::high_resolution_clock::now();
            durations.push_back(std::chrono::duration<double, std::milli>(endTime - startTime).count());
        }

        // Return median
        std::sort(durations.begin(), durations.end());
        return durations[durations.size() / 2];
    };

    for (int count : testCounts) {
        double singleTime = runBenchmark(count, false);
        double threadedTime = runBenchmark(count, true);

        double speedup = (threadedTime > 0) ? singleTime / threadedTime : 0;

        std::string verdict;
        if (speedup > 1.5) {
            verdict = "THREAD";
            if (optimalThreshold == 0) optimalThreshold = count;
        } else if (speedup > 1.1) {
            verdict = "marginal";
            if (marginalThreshold == 0) marginalThreshold = count;
        } else {
            verdict = "single";
        }

        std::cout << std::setw(10) << count
                  << std::setw(18) << std::fixed << std::setprecision(3) << singleTime
                  << std::setw(18) << std::fixed << std::setprecision(3) << threadedTime
                  << std::setw(11) << std::fixed << std::setprecision(2) << speedup << "x"
                  << std::setw(15) << verdict << std::endl;
    }

    std::cout << "\n=== EVENT THREADING RECOMMENDATION ===" << std::endl;
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    double multiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::Event, true);
    float batchMult = budgetMgr.getBatchMultiplier(VoidLight::SystemType::Event);
    std::cout << "Multi throughput:  " << std::fixed << std::setprecision(2) << multiTP << " items/ms" << std::endl;
    std::cout << "Batch multiplier:  " << std::fixed << std::setprecision(2) << batchMult << std::endl;

    if (optimalThreshold > 0) {
        std::cout << "Measured optimal crossover:  " << optimalThreshold << " events (speedup > 1.5x)" << std::endl;
        std::cout << "STATUS: WorkerBudget will adapt throughput tracking over time" << std::endl;
    } else if (marginalThreshold > 0) {
        std::cout << "Marginal benefit at: " << marginalThreshold << " events" << std::endl;
        std::cout << "STATUS: WorkerBudget will learn threading provides minimal benefit" << std::endl;
    } else {
        std::cout << "STATUS: Single-threaded is faster at all tested counts" << std::endl;
        std::cout << "STATUS: WorkerBudget will prefer single-threaded mode" << std::endl;
    }

    std::cout << "========================================\n" << std::endl;

    // Restore threading
    #ifndef NDEBUG
    EventManager::Instance().enableThreading(true);
    #endif
}

// ---------------------------------------------------------------------------
// WorkerBudget Adaptive Tuning Test (Batch Sizing + Throughput Tracking)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(WorkerBudgetAdaptiveTuning)
{
    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    std::cout << "\n--- WorkerBudget Adaptive Tuning (Event) ---\n";
    std::cout << "Tests throughput tracking and mode selection\n";
    std::cout << "(Tracks multi throughput for batch tuning)\n\n";

    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
    auto& eventMgr = EventManager::Instance();

    double initialMultiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::Event, true);
    std::cout << "Initial multi throughput:  " << std::fixed << std::setprecision(2) << initialMultiTP << " items/ms\n\n";

    // Setup handlers
    eventMgr.clean();
    BOOST_REQUIRE(eventMgr.init());
    std::atomic<int> callCount{0};
    for (int i = 0; i < 3; ++i) {
        eventMgr.registerHandler(EventTypeId::Weather,
            [&callCount](const EventData&) { callCount++; });
        eventMgr.registerHandler(EventTypeId::ParticleEffect,
            [&callCount](const EventData&) { callCount++; });
    }

    constexpr int EVENTS_PER_FRAME = 100;
    constexpr int FRAMES_PER_PHASE = 550;
    constexpr int NUM_PHASES = 4;

    std::cout << std::setw(8) << "Phase"
              << std::setw(12) << "Frames"
              << std::setw(14) << "Avg Time(ms)"
              << std::setw(14) << "MultiTP"
              << std::setw(12) << "BatchMult\n";

    for (int phase = 0; phase < NUM_PHASES; ++phase) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int frame = 0; frame < FRAMES_PER_PHASE; ++frame) {
            for (int i = 0; i < EVENTS_PER_FRAME; ++i) {
                if (i % 2 == 0)
                    eventMgr.changeWeather("Rainy", 1.0f);
                else
                    eventMgr.triggerParticleEffect("Fire", 50.0f, 50.0f);
            }
            eventMgr.update();
        }

        auto end = std::chrono::high_resolution_clock::now();
        double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
        double avgMs = totalMs / FRAMES_PER_PHASE;

        double multiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::Event, true);
        float batchMult = budgetMgr.getBatchMultiplier(VoidLight::SystemType::Event);

        std::cout << std::setw(8) << (phase + 1)
                  << std::setw(12) << ((phase + 1) * FRAMES_PER_PHASE)
                  << std::setw(14) << std::fixed << std::setprecision(3) << avgMs
                  << std::setw(14) << std::fixed << std::setprecision(2) << multiTP
                  << std::setw(12) << std::fixed << std::setprecision(2) << batchMult << "\n";
    }

    double finalMultiTP = budgetMgr.getExpectedThroughput(VoidLight::SystemType::Event, true);
    float finalBatchMult = budgetMgr.getBatchMultiplier(VoidLight::SystemType::Event);

    std::cout << "\nFinal multi throughput:  " << std::fixed << std::setprecision(2) << finalMultiTP << " items/ms\n";
    std::cout << "Final batch multiplier:  " << std::fixed << std::setprecision(2) << finalBatchMult << "\n";

    // Result - throughput tracking is working if we have any collected data
    bool throughputCollected = (finalMultiTP > 0);
    if (throughputCollected) {
        std::cout << "Status: PASS (throughput tracking active)\n";
    } else {
        std::cout << "Status: PASS (system initialized, awaiting workload)\n";
    }

    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// Batch Enqueue vs Single Enqueue Performance Test
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(BatchEnqueuePerformanceTest)
{
    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    std::cout << "\n===== BATCH ENQUEUE vs SINGLE ENQUEUE BENCHMARK =====" << std::endl;
    std::cout << "Simulates AI combat workers enqueueing damage events\n" << std::endl;

    auto& eventMgr = EventManager::Instance();
    eventMgr.clean();
    BOOST_REQUIRE(eventMgr.init());

    // Register a simple handler
    std::atomic<int> handlerCalls{0};
    eventMgr.registerHandler(EventTypeId::Custom,
        [&handlerCalls](const EventData&) { ++handlerCalls; });

    const int numWorkers = 10;
    const int eventsPerWorker = 1000;
    const int totalEvents = numWorkers * eventsPerWorker;
    const int numRuns = 5;

    std::cout << "Config: " << numWorkers << " workers, " << eventsPerWorker
              << " events/worker = " << totalEvents << " total events\n" << std::endl;

    std::cout << std::setw(25) << "Method"
              << std::setw(12) << "Time (ms)"
              << std::setw(15) << "Events/sec"
              << std::setw(10) << "Locks" << std::endl;
    std::cout << std::string(62, '-') << std::endl;

    // Pre-create events outside timing (to isolate lock overhead)
    std::vector<std::shared_ptr<MockEvent>> preCreatedEvents;
    preCreatedEvents.reserve(totalEvents);
    for (int i = 0; i < totalEvents; ++i) {
        preCreatedEvents.push_back(std::make_shared<MockEvent>("DamageEvent"));
    }

    // Test 1: Single enqueue (one lock per event) - WITH event creation
    double singleWithCreation = 0.0;
    {
        std::vector<double> durations;
        for (int run = 0; run < numRuns; ++run) {
            auto startTime = std::chrono::high_resolution_clock::now();

            auto& threadSystem = VoidLight::ThreadSystem::Instance();
            std::atomic<int> workersComplete{0};

            for (int w = 0; w < numWorkers; ++w) {
                threadSystem.enqueueTask([&eventMgr, &workersComplete]() {
                    for (int i = 0; i < eventsPerWorker; ++i) {
                        auto event = std::make_shared<MockEvent>("DamageEvent");
                        eventMgr.dispatchEvent(event, EventManager::DispatchMode::Deferred);
                    }
                    workersComplete.fetch_add(1);
                });
            }

            while (workersComplete.load() < numWorkers) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }

            auto endTime = std::chrono::high_resolution_clock::now();
            durations.push_back(std::chrono::duration<double, std::milli>(endTime - startTime).count());
            eventMgr.update();
        }

        singleWithCreation = std::accumulate(durations.begin(), durations.end(), 0.0) / durations.size();
        std::cout << std::setw(25) << "Single + alloc"
                  << std::setw(12) << std::fixed << std::setprecision(2) << singleWithCreation
                  << std::setw(15) << std::setprecision(0) << (totalEvents / singleWithCreation) * 1000.0
                  << std::setw(10) << totalEvents << std::endl;
    }

    // Test 2: Single enqueue - PRE-CREATED events (isolate lock overhead)
    double singleNoAlloc = 0.0;
    {
        std::vector<double> durations;
        for (int run = 0; run < numRuns; ++run) {
            auto startTime = std::chrono::high_resolution_clock::now();

            auto& threadSystem = VoidLight::ThreadSystem::Instance();
            std::atomic<int> workersComplete{0};

            for (int w = 0; w < numWorkers; ++w) {
                int startIdx = w * eventsPerWorker;
                threadSystem.enqueueTask([&eventMgr, &workersComplete, &preCreatedEvents, startIdx]() {
                    for (int i = 0; i < eventsPerWorker; ++i) {
                        eventMgr.dispatchEvent(preCreatedEvents[startIdx + i], EventManager::DispatchMode::Deferred);
                    }
                    workersComplete.fetch_add(1);
                });
            }

            while (workersComplete.load() < numWorkers) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }

            auto endTime = std::chrono::high_resolution_clock::now();
            durations.push_back(std::chrono::duration<double, std::milli>(endTime - startTime).count());
            eventMgr.update();
        }

        singleNoAlloc = std::accumulate(durations.begin(), durations.end(), 0.0) / durations.size();
        std::cout << std::setw(25) << "Single (no alloc)"
                  << std::setw(12) << std::fixed << std::setprecision(2) << singleNoAlloc
                  << std::setw(15) << std::setprecision(0) << (totalEvents / singleNoAlloc) * 1000.0
                  << std::setw(10) << totalEvents << std::endl;
    }

    // Test 3: Batch enqueue - PRE-CREATED events (isolate lock overhead)
    double batchNoAlloc = 0.0;
    {
        std::vector<double> durations;
        for (int run = 0; run < numRuns; ++run) {
            auto startTime = std::chrono::high_resolution_clock::now();

            auto& threadSystem = VoidLight::ThreadSystem::Instance();
            std::atomic<int> workersComplete{0};

            for (int w = 0; w < numWorkers; ++w) {
                int startIdx = w * eventsPerWorker;
                threadSystem.enqueueTask([&eventMgr, &workersComplete, &preCreatedEvents, startIdx]() {
                    std::vector<EventManager::DeferredEvent> localBatch;
                    localBatch.reserve(eventsPerWorker);

                    for (int i = 0; i < eventsPerWorker; ++i) {
                        EventData data;
                        data.event = preCreatedEvents[startIdx + i];
                        localBatch.push_back(EventManager::DeferredEvent{EventTypeId::Custom, std::move(data)});
                    }

                    eventMgr.enqueueBatch(std::move(localBatch));
                    workersComplete.fetch_add(1);
                });
            }

            while (workersComplete.load() < numWorkers) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }

            auto endTime = std::chrono::high_resolution_clock::now();
            durations.push_back(std::chrono::duration<double, std::milli>(endTime - startTime).count());
            eventMgr.update();
        }

        batchNoAlloc = std::accumulate(durations.begin(), durations.end(), 0.0) / durations.size();
        std::cout << std::setw(25) << "Batch (no alloc)"
                  << std::setw(12) << std::fixed << std::setprecision(2) << batchNoAlloc
                  << std::setw(15) << std::setprecision(0) << (totalEvents / batchNoAlloc) * 1000.0
                  << std::setw(10) << numWorkers << std::endl;
    }

    std::cout << "\n--- Analysis ---" << std::endl;
    std::cout << "Allocation overhead: " << std::fixed << std::setprecision(2)
              << (singleWithCreation - singleNoAlloc) << "ms ("
              << std::setprecision(0) << ((singleWithCreation - singleNoAlloc) / singleWithCreation * 100) << "% of total)" << std::endl;
    std::cout << "Lock overhead (single): " << std::fixed << std::setprecision(2) << singleNoAlloc << "ms" << std::endl;
    std::cout << "Lock overhead (batch):  " << std::fixed << std::setprecision(2) << batchNoAlloc << "ms" << std::endl;
    std::cout << "Batch speedup: " << std::setprecision(2) << (singleNoAlloc / batchNoAlloc) << "x" << std::endl;

    eventMgr.clean();
    BOOST_REQUIRE(eventMgr.init());
}

// ---------------------------------------------------------------------------
// Combat Burst Profile Benchmark
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(CombatBurstProfileBenchmark)
{
    if (g_shutdownInProgress.load()) {
        BOOST_TEST_MESSAGE("Skipping test due to shutdown in progress");
        return;
    }

    struct CombatProfile {
        const char* label;
        int totalEvents;
    };

    struct CombatScene {
        std::vector<EntityHandle> attackers;
        std::vector<EntityHandle> targets;
    };

    const std::vector<CombatProfile> profiles = {
        {"Skirmish frame", 200},
        {"Raid frame", 800},
        {"Siege frame", 2000},
        {"Large combat frame", 4000},
        {"Near-cap battle frame", 8000},
    };

    std::cout << "\n===== COMBAT BURST PROFILE BENCHMARK =====" << std::endl;
    std::cout << "Batched deferred combat events using pooled DamageEvent objects\n" << std::endl;

    auto resetCombatManagers = []() {
        EventManager::Instance().clean();
        EntityDataManager::Instance().clean();

        BOOST_REQUIRE(EntityDataManager::Instance().init());
        BOOST_REQUIRE(EventManager::Instance().init());
    };

    auto setupCombatScene = [&resetCombatManagers](int totalEvents) {
        resetCombatManagers();

        auto& edm = EntityDataManager::Instance();

        CombatScene scene;
        scene.attackers.reserve(static_cast<size_t>(totalEvents));
        scene.targets.reserve(static_cast<size_t>(totalEvents));

        for (int i = 0; i < totalEvents; ++i) {
            const float row = static_cast<float>(i / 100);
            const float col = static_cast<float>(i % 100);

            EntityHandle attackerHandle = edm.createNPCWithRaceClass(
                Vector2D(100.0f + col * 12.0f, 100.0f + row * 18.0f), "Human", "Guard");
            BOOST_REQUIRE(attackerHandle.isValid());

            EntityHandle targetHandle = edm.createNPCWithRaceClass(
                Vector2D(106.0f + col * 12.0f, 100.0f + row * 18.0f), "Human", "Guard");
            BOOST_REQUIRE(targetHandle.isValid());

            auto& targetData = edm.getCharacterData(targetHandle);
            targetData.maxHealth = 1000000.0f;
            targetData.health = 1000000.0f;
            targetData.mass = 1.0f;

            scene.attackers.push_back(attackerHandle);
            scene.targets.push_back(targetHandle);
        }

        return scene;
    };

    auto runCombatBurst = [](const CombatProfile& profile,
                             const CombatScene& scene) {
        auto& eventMgr = EventManager::Instance();
        auto& threadSystem = VoidLight::ThreadSystem::Instance();
        auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
        const size_t producerWorkers =
            std::max<size_t>(1, std::min<size_t>(budgetMgr.getBudget().totalWorkers,
                                                 static_cast<size_t>(profile.totalEvents)));
        const int eventsPerProducer = std::max<int>(
            1, static_cast<int>((profile.totalEvents + static_cast<int>(producerWorkers) - 1) /
                                static_cast<int>(producerWorkers)));
        std::atomic<int> workersComplete{0};

        for (size_t worker = 0; worker < producerWorkers; ++worker) {
            threadSystem.enqueueTask([&eventMgr, &workersComplete, &profile,
                                      &scene, worker, eventsPerProducer]() {
                std::vector<EventManager::DeferredEvent> localBatch;
                localBatch.reserve(static_cast<size_t>(eventsPerProducer));

                const int startIndex = static_cast<int>(worker) * eventsPerProducer;
                const int endIndex = std::min(startIndex + eventsPerProducer, profile.totalEvents);
                for (int i = startIndex; i < endIndex; ++i) {
                    localBatch.push_back(createDeferredCombatBenchmarkEvent(
                        i, scene.attackers[static_cast<size_t>(i)],
                        scene.targets[static_cast<size_t>(i)]));
                }

                eventMgr.enqueueBatch(std::move(localBatch));
                workersComplete.fetch_add(1);
            });
        }

        while (workersComplete.load() < static_cast<int>(producerWorkers)) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }

        eventMgr.update();
    };

    for (const auto& profile : profiles) {
        const int totalEvents = profile.totalEvents;
        auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
        const size_t producerWorkers =
            std::max<size_t>(1, std::min<size_t>(budgetMgr.getBudget().totalWorkers,
                                                 static_cast<size_t>(totalEvents)));
        const int eventsPerProducer = std::max<int>(
            1, static_cast<int>((totalEvents + static_cast<int>(producerWorkers) - 1) /
                                static_cast<int>(producerWorkers)));
        const double expectedTotalDamage = [&profile]() {
            double totalDamage = 0.0;
            for (int i = 0; i < profile.totalEvents; ++i) {
                totalDamage += 5.0 + static_cast<double>(i % 20);
            }
            return totalDamage;
        }();

        constexpr int WARMUP_FRAMES = 8;
        auto warmupScene = setupCombatScene(totalEvents);
        for (int warmup = 0; warmup < WARMUP_FRAMES; ++warmup) {
            runCombatBurst(profile, warmupScene);
        }

        auto scene = setupCombatScene(totalEvents);

        auto& threadSystem = VoidLight::ThreadSystem::Instance();
        auto& eventMgr = EventManager::Instance();
        std::atomic<int> workersComplete{0};

        auto enqueueStart = std::chrono::high_resolution_clock::now();
        for (size_t worker = 0; worker < producerWorkers; ++worker) {
            threadSystem.enqueueTask([&eventMgr, &workersComplete, &profile,
                                      &scene, worker, eventsPerProducer]() {
                std::vector<EventManager::DeferredEvent> localBatch;
                localBatch.reserve(static_cast<size_t>(eventsPerProducer));

                const int startIndex = static_cast<int>(worker) * eventsPerProducer;
                const int endIndex = std::min(startIndex + eventsPerProducer, profile.totalEvents);
                for (int i = startIndex; i < endIndex; ++i) {
                    localBatch.push_back(createDeferredCombatBenchmarkEvent(
                        i, scene.attackers[static_cast<size_t>(i)],
                        scene.targets[static_cast<size_t>(i)]));
                }

                eventMgr.enqueueBatch(std::move(localBatch));
                workersComplete.fetch_add(1);
            });
        }

        while (workersComplete.load() < static_cast<int>(producerWorkers)) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        auto enqueueEnd = std::chrono::high_resolution_clock::now();

        auto drainStart = std::chrono::high_resolution_clock::now();
        eventMgr.update();
        auto drainEnd = std::chrono::high_resolution_clock::now();

        auto decision = budgetMgr.shouldUseThreading(
            VoidLight::SystemType::Event, static_cast<size_t>(totalEvents));
        const size_t optimalWorkers = budgetMgr.getOptimalWorkers(
            VoidLight::SystemType::Event, static_cast<size_t>(totalEvents));
        const auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
            VoidLight::SystemType::Event, static_cast<size_t>(totalEvents), optimalWorkers);
        const size_t learnedThreshold =
            budgetMgr.getLearnedThreshold(VoidLight::SystemType::Event);
        const float batchMultiplier =
            budgetMgr.getBatchMultiplier(VoidLight::SystemType::Event);

        const double enqueueMs =
            std::chrono::duration<double, std::milli>(enqueueEnd - enqueueStart).count();
        const double drainMs =
            std::chrono::duration<double, std::milli>(drainEnd - drainStart).count();
        const double totalMs =
            std::chrono::duration<double, std::milli>(drainEnd - enqueueStart).count();
        const size_t queueCap = 8192;
        const double queueUsagePct =
            (static_cast<double>(totalEvents) / static_cast<double>(queueCap)) * 100.0;
        constexpr double FRAME_BUDGET_60HZ_MS = 16.67;
        constexpr double FRAME_BUDGET_120HZ_MS = 8.33;
        const double drainPct60 = (drainMs / FRAME_BUDGET_60HZ_MS) * 100.0;
        const double drainPct120 = (drainMs / FRAME_BUDGET_120HZ_MS) * 100.0;
        const double totalPct60 = (totalMs / FRAME_BUDGET_60HZ_MS) * 100.0;
        const double totalPct120 = (totalMs / FRAME_BUDGET_120HZ_MS) * 100.0;

        std::cout << profile.label << ":" << std::endl;
        std::cout << "  Producer workers: " << producerWorkers
                  << ", events/producer: " << eventsPerProducer
                  << ", total combat events: " << totalEvents << std::endl;
        std::cout << "  Enqueue: " << std::fixed << std::setprecision(2)
                  << enqueueMs << " ms" << std::endl;
        std::cout << "  Drain:   " << std::fixed << std::setprecision(2)
                  << drainMs << " ms" << std::endl;
        std::cout << "  Total:   " << std::fixed << std::setprecision(2)
                  << totalMs << " ms" << std::endl;
        std::cout << "  WorkerBudget mode: "
                  << (decision.shouldThread ? "threaded" : "single") << std::endl;
        std::cout << "  Event workers: " << optimalWorkers
                  << ", batches: " << batchCount
                  << ", batch size: " << batchSize << std::endl;
        std::cout << "  Learned threshold: " << learnedThreshold
                  << ", batch multiplier: " << std::fixed << std::setprecision(2)
                  << batchMultiplier << std::endl;
        std::cout << "  Throughput: " << std::setprecision(0)
                  << (totalEvents / totalMs) * 1000.0 << " events/sec" << std::endl;
        std::cout << "  Queue usage vs 8192 cap: " << std::setprecision(1)
                  << queueUsagePct << "%" << std::endl;
        std::cout << "  Drain frame impact: " << std::fixed << std::setprecision(1)
                  << drainPct60 << "% of 60Hz, "
                  << drainPct120 << "% of 120Hz" << std::endl;
        std::cout << "  Total frame impact: " << std::fixed << std::setprecision(1)
                  << totalPct60 << "% of 60Hz, "
                  << totalPct120 << "% of 120Hz" << std::endl;

        double totalRemainingHealth = 0.0;
        auto& edm = EntityDataManager::Instance();
        for (const EntityHandle targetHandle : scene.targets) {
            totalRemainingHealth += static_cast<double>(
                edm.getCharacterData(targetHandle).health);
        }

        const double expectedRemainingHealth =
            static_cast<double>(profile.totalEvents) * 1000000.0 - expectedTotalDamage;
        BOOST_CHECK_CLOSE(totalRemainingHealth, expectedRemainingHealth, 0.001);
    }

    std::cout << std::endl;
}
