/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef WORKER_BUDGET_HPP
#define WORKER_BUDGET_HPP

#include <cstddef>
#include <array>
#include <atomic>
#include <mutex>
#include <utility>

namespace VoidLight {

// Forward declaration
class ThreadSystem;

/**
 * @brief System types for WorkerBudgetManager
 */
enum class SystemType : uint8_t {
    AI = 0,
    Particle = 1,
    Pathfinding = 2,
    Event = 3,
    Collision = 4,
    BackgroundSim = 5,
    ProjectileSim = 6,
    COUNT = 7
};

/**
 * @brief Threading decision result from WorkerBudgetManager
 *
 * WorkerBudget is the authoritative source for threading decisions.
 * Managers should use shouldThread directly without additional overrides.
 */
struct ThreadingDecision {
    bool shouldThread;  // true = use multi-threading, false = single-threaded
    int probePhase;     // 0=normal, non-zero=exploration (for debugging)
};

/**
 * @brief Worker budget allocation for game engine subsystems
 *
 * Simplified for sequential execution model: each manager gets ALL workers
 * during its execution window since managers don't run concurrently.
 */
struct WorkerBudget {
    size_t totalWorkers{0};  // Total available worker threads (all usable per manager)
};

/**
 * @brief Queue pressure thresholds
 * Used to prevent queue overflow when ThreadSystem is under heavy load
 */
static constexpr float QUEUE_PRESSURE_CRITICAL = 0.90f;

/**
 * @brief Centralized worker budget manager with unified adaptive tuning
 *
 * Optimized for sequential execution model: since managers execute one at a time
 * in the main loop, each manager gets ALL available workers during its window.
 * Pre-allocated ThreadSystem eliminates threading overhead.
 *
 * Unified Tuning Design:
 * - Both single-threaded and multi-threaded modes report throughput
 * - Threading decision based on throughput comparison (no forced probing)
 * - Batch tuning hill-climbs to find optimal granularity
 * - Exploration triggered by signals (multiplier trend, stale data), not timer
 *
 * Provides:
 * 1. Full worker allocation per manager (sequential execution = no contention)
 * 2. Adaptive batch sizing via timing feedback (auto-converges to optimal)
 * 3. Unified throughput tracking for informed threading decisions
 * 4. Queue pressure monitoring (prevents ThreadSystem overload)
 *
 * Thread Safety:
 * - All mutable state uses atomics
 * - Managers don't hold state - they call into this singleton
 */
class WorkerBudgetManager {
public:
    /**
     * @brief Get singleton instance
     */
    static WorkerBudgetManager& Instance();

    /**
     * @brief Get cached worker budget
     * Calculates once and caches the result until invalidated
     */
    const WorkerBudget& getBudget();

    /**
     * @brief Get optimal worker count for a system
     *
     * Sequential execution model: returns ALL workers for any active workload.
     * Only scales back under critical queue pressure (>90%).
     *
     * @param system The system type (AI, Particle, Pathfinding, Event)
     * @param workloadSize Current workload (entities, particles, etc.)
     * @return All available workers (or 0 if workloadSize is 0)
     */
    size_t getOptimalWorkers(SystemType system, size_t workloadSize);

    /**
     * @brief Get adaptive batch strategy
     *
     * Uses hill-climbing tuned multiplier to find optimal batch count.
     * Balances parallelism benefit against scheduling overhead.
     *
     * @param system The system type
     * @param workloadSize Total items to process
     * @param optimalWorkers Worker count from getOptimalWorkers()
     * @return Pair of {batchCount, batchSize}
     */
    std::pair<size_t, size_t> getBatchStrategy(SystemType system,
                                                size_t workloadSize,
                                                size_t optimalWorkers);

    /**
     * @brief Determine if threading should be used for current workload
     *
     * WorkerBudget is the AUTHORITATIVE source for threading decisions.
     * Managers should use decision.shouldThread directly without overrides.
     *
     * Decision based on a learned single-thread timing threshold:
     * - Stays single-threaded until the manager reports a workload whose
     *   single-thread batch time crosses the learning threshold
     * - Once learned, uses hysteresis to stay threaded until workload drops
     *   below the low band
     * - Multi-threaded samples do not affect the switch-over threshold
     *
     * @param system The system type
     * @param workloadSize Number of items to process
     * @return ThreadingDecision with shouldThread and probePhase
     */
    ThreadingDecision shouldUseThreading(SystemType system, size_t workloadSize);

    /**
     * @brief Report execution result for unified tuning
     *
     * Call after processing completes using the same workload unit that was
     * passed to shouldUseThreading().
     *
     * Reporting semantics:
     * - Single-threaded samples update the learned switch-over threshold
     * - Multi-threaded samples update only batch hill-climb tuning
     * - batchCount is ignored for single-threaded reports
     *
     * @param system The system type
     * @param workloadSize Number of items that were processed
     * @param wasThreaded true if multi-threading was used
     * @param batchCount Number of batches used for multi-threaded execution
     * @param totalTimeMs Total time for processing to complete
     */
    void reportExecution(SystemType system, size_t workloadSize,
                         bool wasThreaded, size_t batchCount, double totalTimeMs);

    /**
     * @brief Get expected throughput for multi-threaded mode (for debugging/logging)
     * @param system The system type
     * @param threaded true for multi-threaded (false returns 0.0, single-threaded not tracked)
     * @return Smoothed throughput value (items per ms) or 0.0 for single-threaded
     */
    double getExpectedThroughput(SystemType system, bool threaded) const;

    /**
     * @brief Get current batch multiplier for a system (for debugging/logging)
     * @param system The system type
     * @return Current multiplier value
     */
    float getBatchMultiplier(SystemType system) const;

    /**
     * @brief Get learned threading threshold for a system
     * @param system The system type
     * @return Learned threshold (0 if not learned yet)
     */
    size_t getLearnedThreshold(SystemType system) const;

    /**
     * @brief Check if system's learned threshold is currently active
     * @param system The system type
     * @return true if threshold learned and workload is above hysteresis band
     */
    bool isThresholdActive(SystemType system) const;

    /**
     * @brief Reset all tuning state for state transitions
     *
     * Resets learned thresholds, EMA values, and batch multipliers for all systems.
     * Call during game state exit alongside other manager prepareForStateTransition() calls.
     */
    void prepareForStateTransition();

    /**
     * @brief Invalidate cached budget (call when ThreadSystem changes)
     */
    void invalidateCache();

    /**
     * @brief Mark start of a new frame for cache invalidation
     *
     * Call once per frame (typically at start of GameEngine::update())
     * to enable per-frame caching of expensive operations like queue pressure.
     */
    void markFrameStart();

private:
    WorkerBudgetManager() = default;
    ~WorkerBudgetManager() = default;

    // Non-copyable
    WorkerBudgetManager(const WorkerBudgetManager&) = delete;
    WorkerBudgetManager& operator=(const WorkerBudgetManager&) = delete;

    /**
     * @brief Unified per-system tuning state
     *
     * Two measurement purposes only:
     * - Single-threaded: smoothedSingleTime learns the switch-over workload
     * - Multi-threaded: multiSmoothedThroughput tunes batch hill-climbing
     *
     * Threading decision via adaptive threshold learning:
     * - Single-threaded until smoothed single-thread time crosses 0.9ms
     * - Once threshold learned, use multi-threading with hysteresis
     * - Re-learn when workload drops below threshold - 5%
     *
     * Cache line padding: Atomics are grouped by update pattern and padded
     * to 64-byte boundaries to prevent false sharing when different systems
     * update their state from different cores.
     */
    struct alignas(64) SystemTuningState {
        // ===== Cache line 1: Multi-threaded path (frequently written together) =====
        std::atomic<double> multiSmoothedThroughput{0.0};   // Items per ms when multi-threaded (8 bytes)
        std::atomic<double> prevMultiThroughput{0.0};       // Previous for hill-climb (8 bytes)
        std::atomic<float> multiplier{1.0f};                // Batch multiplier (4 bytes)
        std::atomic<int8_t> direction{1};                   // Hill-climb direction (1 byte)
        char _pad1[64 - 21]{};  // Pad to 64-byte boundary (43 bytes)

        // ===== Cache line 2: Single-threaded path (threshold learning only) =====
        std::atomic<double> smoothedSingleTime{0.0};        // EMA of single-threaded ms (8 bytes)
        std::atomic<uint32_t> singleSampleCount{0};         // Warmup counter for EMA stabilization (4 bytes)
        char _pad2[64 - 12]{};  // Pad to 64-byte boundary (52 bytes)

        // ===== Cache line 3: Mode state (written occasionally) =====
        std::atomic<size_t> learnedThreshold{0};            // Entity count threshold (8 bytes)
        std::atomic<bool> thresholdActive{false};           // Above threshold flag (1 byte)
        char _pad3[64 - 9]{};  // Pad to 64-byte boundary (55 bytes)

        // ===== Constants (read-only, no padding needed) =====
        static constexpr float MIN_MULTIPLIER = 0.4f;
        static constexpr float MAX_MULTIPLIER = 2.0f;
        static constexpr float ADJUST_RATE = 0.01f;

        static constexpr double THROUGHPUT_TOLERANCE = 0.10;
        static constexpr double THROUGHPUT_SMOOTHING = 0.15;

        static constexpr size_t MIN_ITEMS_PER_BATCH = 8;

        static constexpr size_t MIN_WORKLOAD = 100;

        static constexpr double LEARNING_TIME_THRESHOLD_MS = 0.9;
        static constexpr double HYSTERESIS_FACTOR = 0.95;
        static constexpr double TIME_SMOOTHING = 0.25;

        static constexpr uint32_t MIN_LEARNING_SAMPLES = 10;
    };

    // Cached budget (protected by double-checked locking)
    WorkerBudget m_cachedBudget{};
    std::atomic<bool> m_budgetValid{false};
    mutable std::mutex m_cacheMutex;

    // Per-frame queue pressure cache (avoids redundant atomic reads)
    mutable std::atomic<double> m_cachedQueuePressure{0.0};
    std::atomic<uint64_t> m_currentFrame{0};
    mutable std::atomic<uint64_t> m_queuePressureFrame{0};

    // Per-system unified tuning state
    std::array<SystemTuningState, static_cast<size_t>(SystemType::COUNT)> m_systemState{};

    /**
     * @brief Get system name for debug logging
     * @param system The system type
     * @return Human-readable system name
     */
    static const char* getSystemName(SystemType system);

    /**
     * @brief Update batch multiplier via hill-climbing
     * @param state The system's tuning state
     * @param throughput Current measured throughput
     */
    void updateBatchMultiplier(SystemTuningState& state, double throughput);

    /**
     * @brief Get current queue pressure from ThreadSystem (0.0 to 1.0)
     */
    double getQueuePressure() const;

    /**
     * @brief Calculate worker budget (internal)
     */
    WorkerBudget calculateBudget() const;
};

} // namespace VoidLight

#endif // WORKER_BUDGET_HPP
