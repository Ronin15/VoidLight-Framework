/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "core/WorkerBudget.hpp"
#include "core/ThreadSystem.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <format>

namespace VoidLight {

// Static singleton instance
WorkerBudgetManager& WorkerBudgetManager::Instance() {
    static WorkerBudgetManager instance;
    return instance;
}

const WorkerBudget& WorkerBudgetManager::getBudget() {
    // Fast path: return cached budget if valid
    if (m_budgetValid.load(std::memory_order_acquire)) {
        return m_cachedBudget;
    }

    // Slow path: double-checked locking to prevent race on m_cachedBudget
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    if (!m_budgetValid.load(std::memory_order_relaxed)) {
        m_cachedBudget = calculateBudget();
        m_budgetValid.store(true, std::memory_order_release);
    }
    return m_cachedBudget;
}

size_t WorkerBudgetManager::getOptimalWorkers(SystemType,
                                               size_t workloadSize) {
    // No work = no workers needed
    if (workloadSize == 0) {
        return 0;
    }

    // Check queue pressure - if critically stressed, scale back
    double pressure = getQueuePressure();
    if (pressure > QUEUE_PRESSURE_CRITICAL) {
        return 1;  // Minimum threading under critical pressure
    }

    // Sequential execution model: each manager gets ALL workers during its window
    // Pre-allocated ThreadSystem means no overhead for using all threads
    return getBudget().totalWorkers;
}

std::pair<size_t, size_t> WorkerBudgetManager::getBatchStrategy(
    SystemType system,
    size_t workloadSize,
    size_t optimalWorkers) {

    if (workloadSize == 0 || optimalWorkers == 0) {
        return {1, workloadSize};  // Single batch with all items
    }

    auto& state = m_systemState[static_cast<size_t>(system)];

    // =======================================================================
    // Simple and robust: base = workers, adjusted by throughput-tuned multiplier
    // No model assumptions - multiplier is tuned by measuring actual throughput
    // =======================================================================

    // Base batch count = all available workers (max parallelism)
    float multiplier = state.multiplier.load(std::memory_order_relaxed);
    double adjustedBatches = static_cast<double>(optimalWorkers) * static_cast<double>(multiplier);

    // Convert to integer
    size_t batchCount = static_cast<size_t>(std::max(1.0, adjustedBatches));

    // Cap based on minimum items per batch (prevent trivial batches)
    size_t maxBatches = std::max(size_t{1}, workloadSize / SystemTuningState::MIN_ITEMS_PER_BATCH);
    batchCount = std::min(batchCount, maxBatches);

    // Ensure minimum batches: at least 2 when we have multiple workers
    // This guarantees actual parallelism when threading is decided
    if (optimalWorkers >= 2 && workloadSize >= SystemTuningState::MIN_ITEMS_PER_BATCH * 2) {
        batchCount = std::max(batchCount, size_t{2});
    } else {
        batchCount = std::max(batchCount, size_t{1});
    }

    // Calculate batch size (round up to ensure all items are processed)
    size_t batchSize = (workloadSize + batchCount - 1) / batchCount;

    return {batchCount, batchSize};
}

ThreadingDecision WorkerBudgetManager::shouldUseThreading(SystemType system, size_t workloadSize) {
    // ThreadSystem must exist for threading to be possible
    if (!ThreadSystem::Exists()) {
        return {.shouldThread = false, .probePhase = 0};
    }

    // Single-core hardware: no parallelism possible, always single-threaded
    if (ThreadSystem::Instance().getThreadCount() <= 1) {
        return {.shouldThread = false, .probePhase = 0};
    }

    // Always single-threaded for very small workloads
    if (workloadSize < SystemTuningState::MIN_WORKLOAD) {
        return {.shouldThread = false, .probePhase = 0};
    }

    auto& state = m_systemState[static_cast<size_t>(system)];

    // ====== Adaptive Threshold Learning State Machine ======
    size_t threshold = state.learnedThreshold.load(std::memory_order_relaxed);
    bool active = state.thresholdActive.load(std::memory_order_relaxed);

    if (threshold == 0) {
        // LEARNING: No threshold learned yet - stay single-threaded
        // reportExecution() will detect when to set threshold (time >= 1.0ms)
        return {.shouldThread = false, .probePhase = 0};
    }

    if (active) {
        // ACTIVE: Check for hysteresis deactivation
        size_t hysteresisLow = static_cast<size_t>(
            static_cast<double>(threshold) * SystemTuningState::HYSTERESIS_FACTOR);

        if (workloadSize < hysteresisLow) {
            // Dropped below hysteresis band - reset and re-learn
            state.learnedThreshold.store(0, std::memory_order_relaxed);
            state.thresholdActive.store(false, std::memory_order_relaxed);
            state.smoothedSingleTime.store(0.0, std::memory_order_relaxed);  // Reset for fresh learning
            state.singleSampleCount.store(0, std::memory_order_relaxed);     // Reset warmup for re-learning

            VOIDLIGHT_DEBUG_ONLY(
            VOIDLIGHT_DEBUG("WorkerBudget", std::format(
                "{}: Re-learning (workload {} < hysteresis {})",
                getSystemName(system), workloadSize, hysteresisLow));
            )
            return {.shouldThread = false, .probePhase = 0};  // Back to learning mode
        }

        // Still above hysteresis - continue multi-threaded
        return {.shouldThread = true, .probePhase = 0};
    }

    // Threshold learned but not active (edge case: workload dropped then rose again)
    // Activate if workload exceeds threshold
    if (workloadSize >= threshold) {
        state.thresholdActive.store(true, std::memory_order_relaxed);
        return {.shouldThread = true, .probePhase = 0};
    }

    return {.shouldThread = false, .probePhase = 0};
}

void WorkerBudgetManager::reportExecution(SystemType system, size_t workloadSize,
                                           bool wasThreaded, size_t batchCount,
                                           double totalTimeMs) {
    if (workloadSize == 0 || totalTimeMs <= 0.0) {
        return;
    }

    auto& state = m_systemState[static_cast<size_t>(system)];

    // ====== Single-threaded: threshold learning only ======
    if (!wasThreaded && workloadSize >= SystemTuningState::MIN_WORKLOAD) {
        double prevSmoothed = state.smoothedSingleTime.load(std::memory_order_relaxed);
        double newSmoothed;
        if (prevSmoothed <= 0.0) {
            // Dampen first sample to prevent cold-start spikes (burst spawn,
            // cache-cold frames) from triggering premature threshold learning.
            // Treats initial EMA state as 0.0ms with standard alpha weighting.
            newSmoothed = totalTimeMs * SystemTuningState::TIME_SMOOTHING;
        } else {
            newSmoothed = prevSmoothed * (1.0 - SystemTuningState::TIME_SMOOTHING)
                        + totalTimeMs * SystemTuningState::TIME_SMOOTHING;
        }
        state.smoothedSingleTime.store(newSmoothed, std::memory_order_relaxed);

        // Count samples so EMA can stabilize past cold-start transients
        uint32_t samples = state.singleSampleCount.fetch_add(1, std::memory_order_relaxed) + 1;

        // Learn threshold when SMOOTHED time exceeds limit after warmup
        // MIN_LEARNING_SAMPLES prevents cold-start spikes (cache-cold burst spawns)
        // from triggering premature threshold learning on the first few frames
        size_t threshold = state.learnedThreshold.load(std::memory_order_relaxed);
        if (threshold == 0
            && samples >= SystemTuningState::MIN_LEARNING_SAMPLES
            && newSmoothed >= SystemTuningState::LEARNING_TIME_THRESHOLD_MS) {
            state.learnedThreshold.store(workloadSize, std::memory_order_relaxed);
            state.thresholdActive.store(true, std::memory_order_relaxed);

            VOIDLIGHT_DEBUG_ONLY(
            VOIDLIGHT_DEBUG("WorkerBudget", std::format(
                "{}: Learned threshold={} (smoothed={:.2f}ms >= {:.1f}ms, instant={:.2f}ms)",
                getSystemName(system), workloadSize, newSmoothed,
                SystemTuningState::LEARNING_TIME_THRESHOLD_MS, totalTimeMs));
            )
        }
    }
    // ====== Multi-threaded: batch tuning only ======
    else if (wasThreaded) {
        double throughput = static_cast<double>(workloadSize) / totalTimeMs;

        // Update multi-threaded throughput (feeds batch multiplier hill-climbing)
        double prev = state.multiSmoothedThroughput.load(std::memory_order_relaxed);
        double smoothed;
        if (prev <= 0.0) {
            smoothed = throughput;  // First sample
        } else {
            smoothed = prev * (1.0 - SystemTuningState::THROUGHPUT_SMOOTHING)
                     + throughput * SystemTuningState::THROUGHPUT_SMOOTHING;
        }
        state.multiSmoothedThroughput.store(smoothed, std::memory_order_relaxed);

        // Run batch tuning hill-climb
        if (batchCount > 0) {
            updateBatchMultiplier(state, smoothed);
        }
    }
}

void WorkerBudgetManager::updateBatchMultiplier(SystemTuningState& state, double throughput) {
    // Get previous throughput for comparison
    double prevTP = state.prevMultiThroughput.load(std::memory_order_relaxed);
    state.prevMultiThroughput.store(throughput, std::memory_order_relaxed);

    // Skip first sample (need history to compare)
    if (prevTP <= 0.0) {
        return;
    }

    // Calculate relative change
    double change = (throughput - prevTP) / prevTP;

    // Hill-climb multiplier
    float multiplier = state.multiplier.load(std::memory_order_relaxed);
    int8_t direction = state.direction.load(std::memory_order_relaxed);

    if (change > SystemTuningState::THROUGHPUT_TOLERANCE) {
        // Throughput improved - continue in same direction
        multiplier += static_cast<float>(direction) * SystemTuningState::ADJUST_RATE;
    } else if (change < -SystemTuningState::THROUGHPUT_TOLERANCE) {
        // Throughput degraded - reverse direction
        direction = static_cast<int8_t>(-direction);
        state.direction.store(direction, std::memory_order_relaxed);
        multiplier += static_cast<float>(direction) * SystemTuningState::ADJUST_RATE;
    }
    // Within tolerance: at equilibrium, hold steady

    // Clamp multiplier
    multiplier = std::clamp(multiplier,
                            SystemTuningState::MIN_MULTIPLIER,
                            SystemTuningState::MAX_MULTIPLIER);

    state.multiplier.store(multiplier, std::memory_order_relaxed);
}

double WorkerBudgetManager::getExpectedThroughput(SystemType system, bool threaded) const {
    if (threaded) {
        const auto& state = m_systemState[static_cast<size_t>(system)];
        return state.multiSmoothedThroughput.load(std::memory_order_relaxed);
    }
    // Single-threaded throughput no longer tracked
    return 0.0;
}

float WorkerBudgetManager::getBatchMultiplier(SystemType system) const {
    return m_systemState[static_cast<size_t>(system)].multiplier.load(std::memory_order_relaxed);
}

size_t WorkerBudgetManager::getLearnedThreshold(SystemType system) const {
    return m_systemState[static_cast<size_t>(system)].learnedThreshold.load(std::memory_order_relaxed);
}

bool WorkerBudgetManager::isThresholdActive(SystemType system) const {
    return m_systemState[static_cast<size_t>(system)].thresholdActive.load(std::memory_order_relaxed);
}

const char* WorkerBudgetManager::getSystemName(SystemType system) {
    switch (system) {
        case SystemType::AI: return "AI";
        case SystemType::Particle: return "Particle";
        case SystemType::Pathfinding: return "Pathfinding";
        case SystemType::Event: return "Event";
        case SystemType::Collision: return "Collision";
        case SystemType::BackgroundSim: return "BackgroundSim";
        case SystemType::ProjectileSim: return "ProjectileSim";
        default: return "Unknown";
    }
}

void WorkerBudgetManager::prepareForStateTransition() {
    for (auto& state : m_systemState) {
        state.multiSmoothedThroughput.store(0.0, std::memory_order_relaxed);
        state.prevMultiThroughput.store(0.0, std::memory_order_relaxed);
        state.multiplier.store(1.0f, std::memory_order_relaxed);
        state.direction.store(1, std::memory_order_relaxed);
        state.smoothedSingleTime.store(0.0, std::memory_order_relaxed);
        state.singleSampleCount.store(0, std::memory_order_relaxed);
        state.learnedThreshold.store(0, std::memory_order_relaxed);
        state.thresholdActive.store(false, std::memory_order_relaxed);
    }
}

void WorkerBudgetManager::invalidateCache() {
    m_budgetValid.store(false, std::memory_order_release);
}

void WorkerBudgetManager::markFrameStart() {
    // Increment frame counter - this invalidates per-frame caches
    m_currentFrame.fetch_add(1, std::memory_order_relaxed);
}

double WorkerBudgetManager::getQueuePressure() const {
    // Fast path: return cached value if same frame
    uint64_t currentFrame = m_currentFrame.load(std::memory_order_relaxed);
    if (m_queuePressureFrame.load(std::memory_order_relaxed) == currentFrame) {
        return m_cachedQueuePressure.load(std::memory_order_relaxed);
    }

    // Slow path: calculate and cache
    if (!ThreadSystem::Exists()) {
        return 0.0;
    }

    const auto& threadSystem = ThreadSystem::Instance();
    size_t queueSize = threadSystem.getQueueSize();
    size_t queueCapacity = threadSystem.getQueueCapacity();

    double pressure = 0.0;
    if (queueCapacity > 0) {
        pressure = static_cast<double>(queueSize) / static_cast<double>(queueCapacity);
    }

    // Cache for this frame
    m_cachedQueuePressure.store(pressure, std::memory_order_relaxed);
    m_queuePressureFrame.store(currentFrame, std::memory_order_relaxed);

    return pressure;
}

WorkerBudget WorkerBudgetManager::calculateBudget() const {
    WorkerBudget budget;

    // Get available workers from ThreadSystem
    if (!ThreadSystem::Exists()) {
        // Fallback for tests or early initialization
        budget.totalWorkers = 4;
    } else {
        budget.totalWorkers = ThreadSystem::Instance().getThreadCount();
    }

    // Ensure minimum workers
    budget.totalWorkers = std::max(budget.totalWorkers, size_t{1});

    // Sequential execution model: all workers available to each manager during its window
    // No partitioning needed since managers don't run concurrently

    return budget;
}

} // namespace VoidLight
