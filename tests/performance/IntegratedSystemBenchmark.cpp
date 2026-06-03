/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE IntegratedSystemBenchmark
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <cmath>
#include <random>

#include "core/ThreadSystem.hpp"
#include "core/Logger.hpp"  // For benchmark mode
#include "managers/EventManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/AIManager.hpp"
#include "managers/ParticleManager.hpp"
#include "managers/EntityDataManager.hpp" // For TransformData definition
#include "managers/BackgroundSimulationManager.hpp"
#include "utils/Vector2D.hpp"
#include "events/Event.hpp"
#include "events/WorldEvent.hpp"

// Test helper for data-driven NPCs (NPCs are purely data, no Entity class)
class BenchmarkNPC {
public:
    explicit BenchmarkNPC(int id, const Vector2D& pos) : m_id(id) {
        auto& edm = EntityDataManager::Instance();
        m_handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
    }

    static std::shared_ptr<BenchmarkNPC> create(int id, const Vector2D& pos) {
        return std::make_shared<BenchmarkNPC>(id, pos);
    }

    [[nodiscard]] EntityHandle getHandle() const { return m_handle; }
    int getId() const { return m_id; }

private:
    EntityHandle m_handle;
    int m_id;
};

struct ThreadSystemFixture {
    ThreadSystemFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }
    ~ThreadSystemFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

BOOST_AUTO_TEST_SUITE(IntegratedSystemBenchmarkSuite)

namespace {
    // Test configuration constants.
    // Note: this bench measures *managers only* (no GPU, audio, input, game logic).
    // Production frames are typically 2–4x more expensive than what we measure here, so
    // MANAGER_BUDGET_MS is a tighter target than the 16.67 ms full-frame budget — managers
    // must finish in well under a frame to leave room for everything else.
    constexpr float TARGET_FRAME_TIME_MS = 16.67f;       // 60 FPS full-frame budget
    constexpr float MANAGER_BUDGET_MS    = 10.0f;        // Realistic manager-only budget
    constexpr float P95_TARGET_MS = 20.0f;
    constexpr float P99_TARGET_MS = 25.0f;
    constexpr float MAX_FRAME_DROP_PERCENT = 5.0f;
    // Scaling-test setup distributes entities across tiers (60/30/10).
    constexpr float TIER_ACTIVE_FRACTION = 0.60f;

    // Benchmark helper class
    class IntegratedSystemBenchmark {
    public:
        struct FrameStats {
            double averageMs;
            double medianMs;
            double p95Ms;
            double p99Ms;
            double maxMs;
            double minMs;
            size_t frameDrops;
            size_t totalFrames;
            double frameDropPercent;
            std::vector<double> frameTimes;
        };

        IntegratedSystemBenchmark() : m_rng(12345) {
            initializeAllManagers();
        }

        ~IntegratedSystemBenchmark() {
            cleanupAllManagers();
        }

        // Test 1: Realistic game simulation at 60 FPS target
        void testRealisticGameSimulation60FPS() {
            std::cout << "\n=== Integrated System Load Benchmark ===" << std::endl;
            std::cout << "Configuration:" << std::endl;
            std::cout << "  AI Entities: 2,000" << std::endl;
            std::cout << "  Particles: 1,000" << std::endl;
            std::cout << "  Duration: 120 frames (2 seconds @ 60 FPS)" << std::endl;
            std::cout << std::endl;

            // Setup realistic game scenario (reduced for faster ctest)
            setupRealisticScenario(2000, 1000);

            // Run benchmark
            constexpr size_t frameCount = 120;
            constexpr float deltaTime = 1.0f / 60.0f;

            auto stats = runFrameBenchmark(frameCount, deltaTime);

            // Print results
            printFrameStatistics(stats);
            printTestResult(stats);
        }

        // Test 2: Scaling under increasing load
        void testScalingUnderLoad() {
            std::cout << "\n=== Scaling Under Load Benchmark ===" << std::endl;
            std::cout << "Testing frame time degradation with increasing entity counts" << std::endl;
            std::cout << std::endl;

            // Entity counts aligned with engine's 10K+ AI target (see CLAUDE.md)
            std::vector<size_t> entityCounts = {500, 1000, 2500, 5000, 10000};
            std::vector<FrameStats> scalingResults;

            for (size_t entityCount : entityCounts) {
                const size_t activeCount = static_cast<size_t>(entityCount * TIER_ACTIVE_FRACTION);
                std::cout << "Testing with " << entityCount << " entities ("
                          << activeCount << " Active)..." << std::endl;

                cleanupScenario();
                setupRealisticScenario(entityCount, entityCount / 2);

                // 60 frames (1 second at 60 FPS) - sufficient for stable measurements
                constexpr size_t frameCount = 60;
                constexpr float deltaTime = 1.0f / 60.0f;
                auto stats = runFrameBenchmark(frameCount, deltaTime);

                scalingResults.push_back(stats);

                std::cout << "  Average: " << std::fixed << std::setprecision(2)
                         << stats.averageMs << "ms, P95: " << stats.p95Ms
                         << "ms, Frame drops: " << stats.frameDropPercent << "%" << std::endl;
            }

            printScalingSummary(entityCounts, scalingResults);
        }

        // Test 3: Manager coordination overhead — measured at multiple scales because
        // overhead at 1K may not generalize to 10K (contention grows non-linearly).
        void testManagerCoordinationOverhead() {
            std::cout << "\n=== Manager Coordination Overhead Benchmark ===" << std::endl;
            std::cout << "Measuring overhead from cross-manager communication" << std::endl;
            std::cout << std::endl;

            constexpr size_t frameCount = 60;
            constexpr float deltaTime = 1.0f / 60.0f;
            const std::vector<size_t> entityCounts = {1000, 10000};
            bool allPass = true;

            for (size_t entityCount : entityCounts) {
                std::cout << "--- Coordination at " << entityCount << " entities ---" << std::endl;

                std::cout << "Baseline (managers idle)..." << std::endl;
                cleanupScenario();
                auto baselineStats = runFrameBenchmark(frameCount, deltaTime);

                std::cout << "AI Manager only..." << std::endl;
                cleanupScenario();
                setupAIOnly(entityCount);
                auto aiStats = runFrameBenchmark(frameCount, deltaTime);

                std::cout << "Particle Manager only..." << std::endl;
                cleanupScenario();
                setupParticlesOnly(entityCount / 2);
                auto particleStats = runFrameBenchmark(frameCount, deltaTime);

                std::cout << "All managers active..." << std::endl;
                cleanupScenario();
                setupRealisticScenario(entityCount, entityCount / 2);
                auto allStats = runFrameBenchmark(frameCount, deltaTime);

                double individualSum = aiStats.averageMs + particleStats.averageMs;
                double overhead = allStats.averageMs - individualSum;
                double overheadPct = (allStats.averageMs > 0)
                    ? (overhead / allStats.averageMs * 100.0) : 0.0;

                std::cout << std::fixed << std::setprecision(2);
                std::cout << "  Baseline (idle): " << baselineStats.averageMs << "ms\n";
                std::cout << "  AI only: " << aiStats.averageMs << "ms\n";
                std::cout << "  Particles only: " << particleStats.averageMs << "ms\n";
                std::cout << "  Sum: " << individualSum << "ms\n";
                std::cout << "  All active: " << allStats.averageMs << "ms\n";
                std::cout << "  Coordination overhead: " << overhead << "ms ("
                         << overheadPct << "%)\n";

                // Threshold scales with entity count — more work means more synchronization.
                const double thresholdMs = (entityCount <= 1000) ? 2.0 : 5.0;
                if (overhead < thresholdMs) {
                    std::cout << "  ✓ PASS: overhead < " << thresholdMs << "ms\n\n";
                } else {
                    std::cout << "  ✗ FAIL: overhead >= " << thresholdMs << "ms\n\n";
                    allPass = false;
                }
            }

            if (allPass) {
                std::cout << "Overall: ✓ PASS — coordination overhead within budget at all scales\n";
            } else {
                std::cout << "Overall: ✗ FAIL — coordination overhead exceeds budget\n";
            }
        }

        // Test 4: Sustained performance over time
        void testSustainedPerformance() {
            std::cout << "\n=== Sustained Performance Benchmark ===" << std::endl;
            std::cout << "Testing for performance degradation over 10 seconds" << std::endl;
            std::cout << std::endl;

            setupRealisticScenario(2000, 1000);

            constexpr size_t totalFrames = 600;   // 10 seconds at 60 FPS
            constexpr size_t sampleInterval = 60; // Sample every second
            constexpr float deltaTime = 1.0f / 60.0f;

            // Warmup frames (REQUIRED - longer than runFrameBenchmark's 16 frames)
            // Without warmup, first segment appears artificially fast due to:
            // - WorkerBudget learning throughput (~1s to converge)
            // - Thread pool warming up
            // - Particle system ramping up
            // 120 frames = 2 seconds ensures full WorkerBudget convergence
            for (size_t i = 0; i < 120; ++i) {
                updateAllManagers(deltaTime);
            }

            std::vector<double> segmentAverages;

            for (size_t segment = 0; segment < totalFrames / sampleInterval; ++segment) {
                std::vector<double> segmentTimes;
                segmentTimes.reserve(sampleInterval);

                for (size_t frame = 0; frame < sampleInterval; ++frame) {
                    auto startTime = std::chrono::high_resolution_clock::now();

                    updateAllManagers(deltaTime);

                    auto endTime = std::chrono::high_resolution_clock::now();
                    double frameTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

                    segmentTimes.push_back(frameTime);
                }

                double segmentAverage = std::accumulate(segmentTimes.begin(), segmentTimes.end(), 0.0) / segmentTimes.size();
                segmentAverages.push_back(segmentAverage);

                std::cout << "Segment " << (segment + 1) << " (t=" << (segment + 1)
                         << "s): " << std::fixed << std::setprecision(2)
                         << segmentAverage << "ms average" << std::endl;
            }

            // Analyze degradation
            double firstSegment = segmentAverages.front();
            double lastSegment = segmentAverages.back();
            double degradation = lastSegment - firstSegment;
            double degradationPercent = (degradation / firstSegment) * 100.0;

            std::cout << "\nDegradation Analysis:" << std::endl;
            std::cout << "  First segment average: " << firstSegment << "ms" << std::endl;
            std::cout << "  Last segment average: " << lastSegment << "ms" << std::endl;
            std::cout << "  Degradation: " << degradation << "ms ("
                     << degradationPercent << "%)" << std::endl;

            if (std::abs(degradationPercent) < 10.0) {
                std::cout << "\n✓ PASS: Performance degradation < 10%" << std::endl;
            } else {
                std::cout << "\n⚠ WARNING: Performance degradation >= 10% (check for memory leaks)" << std::endl;
            }
        }

    private:
        std::mt19937 m_rng;
        std::vector<std::shared_ptr<BenchmarkNPC>> m_testEntities;

        void initializeAllManagers() {
            // Enable benchmark mode to suppress verbose logging during benchmarks
            VOIDLIGHT_ENABLE_BENCHMARK_MODE();

            // Initialize in dependency order (matching GameEngine::init pattern)
            // ThreadSystem handled by global fixture
            BOOST_REQUIRE(EntityDataManager::Instance().init());

            EventManager::Instance().init();
            PathfinderManager::Instance().init();
            PathfinderManager::Instance().rebuildGrid();
            CollisionManager::Instance().init();
            AIManager::Instance().init();
            #ifndef NDEBUG
            AIManager::Instance().enableThreading(true);
            #endif
            ParticleManager::Instance().init();  // Initialize without texture manager
            ParticleManager::Instance().registerBuiltInEffects();

            // Initialize tier system for culling
            BackgroundSimulationManager::Instance().init();
            // Headless test: simulate 1920x1080 radii (half-diagonal ~1100px)
            // Active: 1.5x = 1650, Background: 2.0x = 2200
            BackgroundSimulationManager::Instance().setActiveRadius(1650.0f);
            BackgroundSimulationManager::Instance().setBackgroundRadius(2200.0f);
        }

        void cleanupAllManagers() {
            // Cleanup in reverse order
            cleanupScenario();

            BackgroundSimulationManager::Instance().clean();
            ParticleManager::Instance().clean();
            AIManager::Instance().clean();
            CollisionManager::Instance().clean();
            PathfinderManager::Instance().clean();
            EventManager::Instance().clean();
            EntityDataManager::Instance().clean();
        }

        void cleanupScenario() {
            // Remove all AI entities
            for (auto& entity : m_testEntities) {
                if (entity) {
                    AIManager::Instance().unregisterEntity(entity->getHandle());
                    AIManager::Instance().unassignBehavior(entity->getHandle());
                }
            }
            m_testEntities.clear();

            // Particles will be cleaned automatically during manager cleanup
        }

        void setupRealisticScenario(size_t aiEntityCount, size_t particleCount) {
            auto& aiMgr = AIManager::Instance();
            auto& particleMgr = ParticleManager::Instance();

            m_testEntities.reserve(aiEntityCount);

            // Use standard data-oriented behavior names (no registration needed)
            const std::vector<std::string> behaviorNames = {"Wander", "Guard", "Idle"};

            // Create AI entities distributed across tier zones for realistic testing
            // Active tier: within 1650px of center (first 60%)
            // Background tier: 1650-2200px from center (next 30%)
            // Hibernated tier: beyond 2200px (last 10%)
            std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
            std::uniform_real_distribution<float> distActive(0.0f, 1650.0f);
            std::uniform_real_distribution<float> distBackground(1650.0f, 2200.0f);
            std::uniform_real_distribution<float> distHibernated(2200.0f, 4000.0f);

            for (size_t i = 0; i < aiEntityCount; ++i) {
                float angle = angleDist(m_rng);
                float distance;

                if (i < aiEntityCount * 6 / 10) {
                    // 60% in Active tier
                    distance = distActive(m_rng);
                } else if (i < aiEntityCount * 9 / 10) {
                    // 30% in Background tier
                    distance = distBackground(m_rng);
                } else {
                    // 10% in Hibernated tier
                    distance = distHibernated(m_rng);
                }

                Vector2D pos(2500.0f + distance * std::cos(angle),
                             2500.0f + distance * std::sin(angle));
                auto entity = BenchmarkNPC::create(static_cast<int>(i), pos);
                m_testEntities.push_back(entity);

                // Assign behavior - distribute across types using data-oriented API
                std::string behaviorName = behaviorNames[i % behaviorNames.size()];
                aiMgr.registerEntity(entity->getHandle(), behaviorName);
            }

            // Set player for distance optimization
            if (!m_testEntities.empty()) {
                aiMgr.setPlayerHandle(m_testEntities[0]->getHandle());
            }

            // Create particle effects
            Vector2D spawnCenter(2500.0f, 2500.0f);
            size_t effectsNeeded = particleCount / 100;  // Approximate particles per effect

            for (size_t i = 0; i < effectsNeeded; ++i) {
                particleMgr.playEffect(ParticleEffectType::Rain, spawnCenter, 1.0f);
            }
        }

        void setupAIOnly(size_t entityCount) {
            auto& aiMgr = AIManager::Instance();
            m_testEntities.reserve(entityCount);

            // Distribute entities across tier zones (same as setupRealisticScenario)
            std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
            std::uniform_real_distribution<float> distActive(0.0f, 1650.0f);
            std::uniform_real_distribution<float> distBackground(1650.0f, 2200.0f);
            std::uniform_real_distribution<float> distHibernated(2200.0f, 4000.0f);

            for (size_t i = 0; i < entityCount; ++i) {
                float angle = angleDist(m_rng);
                float distance;

                if (i < entityCount * 6 / 10) {
                    distance = distActive(m_rng);
                } else if (i < entityCount * 9 / 10) {
                    distance = distBackground(m_rng);
                } else {
                    distance = distHibernated(m_rng);
                }

                Vector2D pos(2500.0f + distance * std::cos(angle),
                             2500.0f + distance * std::sin(angle));
                auto entity = BenchmarkNPC::create(static_cast<int>(i), pos);
                m_testEntities.push_back(entity);

                // Use data-oriented behavior API with standard behavior name
                aiMgr.registerEntity(entity->getHandle(), "Wander");
            }

            if (!m_testEntities.empty()) {
                aiMgr.setPlayerHandle(m_testEntities[0]->getHandle());
            }
        }

        void setupParticlesOnly(size_t particleCount) {
            auto& particleMgr = ParticleManager::Instance();
            Vector2D spawnCenter(2500.0f, 2500.0f);

            size_t effectsNeeded = particleCount / 100;
            for (size_t i = 0; i < effectsNeeded; ++i) {
                particleMgr.playEffect(ParticleEffectType::Rain, spawnCenter, 1.0f);
            }
        }

        void updateAllManagers(float deltaTime) {
            // Simulate realistic frame update order (matching GameEngine::update pattern)
            EventManager::Instance().update();
            AIManager::Instance().update(deltaTime);
            CollisionManager::Instance().update(deltaTime);
            ParticleManager::Instance().update(deltaTime);

            // Tier culling update (reference point = center of spawn area)
            Vector2D referencePoint(2500.0f, 2500.0f);
            BackgroundSimulationManager::Instance().update(referencePoint, deltaTime);
        }

        FrameStats runFrameBenchmark(size_t frameCount, float deltaTime) {
            // Extended warmup: WorkerBudget hill-climb needs ~100 frames to converge.
            // 16 frames was too short and made scaling-test numbers reflect pre-converged state.
            constexpr size_t WARMUP_FRAMES = 100;
            for (size_t i = 0; i < WARMUP_FRAMES; ++i) {
                updateAllManagers(deltaTime);
            }

            // Work-done sanity check via AIManager's exposed counter. Catches early-returns
            // or any path that times-fast-but-does-nothing. Skipped if there's no AI work
            // (baseline / particles-only tests) since the counter wouldn't advance anyway.
            auto& aim = AIManager::Instance();
            const size_t executionsBefore = aim.getBehaviorUpdateCount();

            std::vector<double> frameTimes;
            frameTimes.reserve(frameCount);
            for (size_t i = 0; i < frameCount; ++i) {
                auto startTime = std::chrono::high_resolution_clock::now();

                updateAllManagers(deltaTime);

                auto endTime = std::chrono::high_resolution_clock::now();
                double frameTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
                frameTimes.push_back(frameTime);
            }

            // Sanity check: if the test had AI entities, behaviors should have executed.
            const size_t executionsAfter = aim.getBehaviorUpdateCount();
            const size_t actualExecutions = executionsAfter - executionsBefore;
            if (!m_testEntities.empty() && actualExecutions == 0) {
                std::cout << "  WARNING: 0 behavior executions across " << frameCount
                          << " frames — measurement may be invalid\n";
            }

            return calculateFrameStats(frameTimes);
        }

        FrameStats calculateFrameStats(const std::vector<double>& frameTimes) {
            FrameStats stats;
            stats.frameTimes = frameTimes;
            stats.totalFrames = frameTimes.size();

            // Sort for percentile calculations
            std::vector<double> sortedTimes = frameTimes;
            std::sort(sortedTimes.begin(), sortedTimes.end());

            stats.averageMs = std::accumulate(sortedTimes.begin(), sortedTimes.end(), 0.0) / sortedTimes.size();
            stats.medianMs = sortedTimes[sortedTimes.size() / 2];
            stats.p95Ms = sortedTimes[static_cast<size_t>(sortedTimes.size() * 0.95)];
            stats.p99Ms = sortedTimes[static_cast<size_t>(sortedTimes.size() * 0.99)];
            stats.maxMs = sortedTimes.back();
            stats.minMs = sortedTimes.front();

            stats.frameDrops = std::count_if(sortedTimes.begin(), sortedTimes.end(),
                [](double t) { return t > TARGET_FRAME_TIME_MS; });
            stats.frameDropPercent = (static_cast<double>(stats.frameDrops) / stats.totalFrames) * 100.0;

            return stats;
        }

        void printFrameStatistics(const FrameStats& stats) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Frame Time Statistics:" << std::endl;

            std::cout << "  Average: " << stats.averageMs << "ms ";
            if (stats.averageMs < TARGET_FRAME_TIME_MS) {
                std::cout << "✓ (target < " << TARGET_FRAME_TIME_MS << "ms)" << std::endl;
            } else {
                std::cout << "✗ (target < " << TARGET_FRAME_TIME_MS << "ms)" << std::endl;
            }

            std::cout << "  Median: " << stats.medianMs << "ms" << std::endl;

            std::cout << "  P95: " << stats.p95Ms << "ms ";
            if (stats.p95Ms < P95_TARGET_MS) {
                std::cout << "✓ (target < " << P95_TARGET_MS << "ms)" << std::endl;
            } else {
                std::cout << "⚠ (target < " << P95_TARGET_MS << "ms)" << std::endl;
            }

            std::cout << "  P99: " << stats.p99Ms << "ms ";
            if (stats.p99Ms < P99_TARGET_MS) {
                std::cout << "✓ (target < " << P99_TARGET_MS << "ms)" << std::endl;
            } else {
                std::cout << "⚠ (target < " << P99_TARGET_MS << "ms)" << std::endl;
            }

            std::cout << "  Max: " << stats.maxMs << "ms" << std::endl;
            std::cout << "  Min: " << stats.minMs << "ms" << std::endl;

            std::cout << "  Frame drops (>" << TARGET_FRAME_TIME_MS << "ms): "
                     << stats.frameDrops << "/" << stats.totalFrames
                     << " (" << std::setprecision(1) << stats.frameDropPercent << "%) ";
            if (stats.frameDropPercent < MAX_FRAME_DROP_PERCENT) {
                std::cout << "✓" << std::endl;
            } else {
                std::cout << "⚠" << std::endl;
            }
            std::cout << std::endl;
        }

        void printTestResult(const FrameStats& stats) {
            bool avgPass = stats.averageMs < TARGET_FRAME_TIME_MS;
            bool p95Pass = stats.p95Ms < P95_TARGET_MS;
            bool frameDropPass = stats.frameDropPercent < MAX_FRAME_DROP_PERCENT;

            std::cout << "Result: ";
            if (avgPass && p95Pass && frameDropPass) {
                std::cout << "PASS ✓" << std::endl;
            } else if (avgPass && frameDropPass) {
                std::cout << "PASS with warnings (P95 acceptable)" << std::endl;
            } else {
                std::cout << "FAIL ✗ (needs optimization)" << std::endl;
            }
        }

        void printScalingSummary(const std::vector<size_t>& entityCounts,
                                const std::vector<FrameStats>& results) {
            std::cout << "\n=== Scaling Summary ===" << std::endl;
            std::cout << "Threshold: " << MANAGER_BUDGET_MS
                      << " ms manager-only budget (leaves headroom for GPU + audio + game logic)\n";
            std::cout << std::left << std::setw(10) << "Total"
                     << std::setw(10) << "Active"
                     << std::setw(12) << "Avg (ms)"
                     << std::setw(12) << "P95 (ms)"
                     << std::setw(15) << "Drops (%)"
                     << "Status" << std::endl;
            std::cout << std::string(70, '-') << std::endl;

            for (size_t i = 0; i < entityCounts.size(); ++i) {
                const auto& stats = results[i];
                const size_t activeCount = static_cast<size_t>(entityCounts[i] * TIER_ACTIVE_FRACTION);
                std::cout << std::fixed << std::setprecision(2);
                std::cout << std::left << std::setw(10) << entityCounts[i]
                         << std::setw(10) << activeCount
                         << std::setw(12) << stats.averageMs
                         << std::setw(12) << stats.p95Ms
                         << std::setw(15) << stats.frameDropPercent;

                if (stats.averageMs < MANAGER_BUDGET_MS && stats.frameDropPercent < MAX_FRAME_DROP_PERCENT) {
                    std::cout << "✓ within manager budget" << std::endl;
                } else if (stats.averageMs < TARGET_FRAME_TIME_MS) {
                    std::cout << "~ over manager budget, under full-frame" << std::endl;
                } else {
                    std::cout << "✗ over full-frame budget" << std::endl;
                }
            }

            // Find max sustainable Active entity count under manager-only budget.
            // Reporting Active count rather than total — that's what's actually working per frame.
            size_t maxSustainableActive = 0;
            for (size_t i = 0; i < results.size(); ++i) {
                if (results[i].averageMs < MANAGER_BUDGET_MS &&
                    results[i].frameDropPercent < MAX_FRAME_DROP_PERCENT) {
                    maxSustainableActive = static_cast<size_t>(entityCounts[i] * TIER_ACTIVE_FRACTION);
                }
            }

            std::cout << "\nMax Active NPCs sustainable within " << MANAGER_BUDGET_MS
                      << " ms manager budget: " << maxSustainableActive << std::endl;
            std::cout << "(Production frames will add GPU + audio + game logic on top.)\n";
        }
    };
}

// Boost test cases
BOOST_AUTO_TEST_CASE(TestRealisticGameSimulation60FPS) {
    IntegratedSystemBenchmark benchmark;
    benchmark.testRealisticGameSimulation60FPS();
}

BOOST_AUTO_TEST_CASE(TestScalingUnderLoad) {
    IntegratedSystemBenchmark benchmark;
    benchmark.testScalingUnderLoad();
}

BOOST_AUTO_TEST_CASE(TestManagerCoordinationOverhead) {
    IntegratedSystemBenchmark benchmark;
    benchmark.testManagerCoordinationOverhead();
}

BOOST_AUTO_TEST_CASE(TestSustainedPerformance) {
    IntegratedSystemBenchmark benchmark;
    benchmark.testSustainedPerformance();
}

BOOST_AUTO_TEST_SUITE_END()
