/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef BACKGROUND_SIMULATION_MANAGER_HPP
#define BACKGROUND_SIMULATION_MANAGER_HPP

/**
 * @file BackgroundSimulationManager.hpp
 * @brief Simplified simulation for off-screen (Background tier) entities
 *
 * BackgroundSimulationManager processes entities that are outside the active
 * camera area but still need some basic simulation to maintain world consistency.
 *
 * Processing differences by tier:
 * - Active: Full AI, collision, rendering (handled by AIManager, CollisionManager)
 * - Background: Position-only updates, no collision, no rendering (this manager)
 * - Hibernated: No updates, data stored only
 *
 * Threading Model (follows AIManager pattern):
 * - Uses WorkerBudget for adaptive batch sizing
 * - Submits batches to ThreadSystem for parallel processing
 * - Called at end of GameEngine::update() for power efficiency
 * - Handles tier updates (every 60 frames) + background entity processing (10Hz)
 */

#include "utils/Vector2D.hpp"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <future>
#include <mutex>
#include <vector>

// Forward declarations
class EntityDataManager;

class BackgroundSimulationManager {
public:
    static BackgroundSimulationManager& Instance() {
        static BackgroundSimulationManager instance;
        return instance;
    }

    /**
     * @brief Initialize the background simulation manager
     * @return true if initialization successful
     */
    [[nodiscard]] bool init();

    /**
     * @brief Clean up resources
     */
    void clean();

    /**
     * @brief Prepare for state transition (clear pending work)
     */
    void prepareForStateTransition();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept {
        return m_initialized.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if manager is shut down
     */
    [[nodiscard]] bool isShutdown() const noexcept {
        return m_isShutdown.load(std::memory_order_acquire);
    }

    /**
     * @brief Set global pause state for background simulation
     * @param paused true to pause, false to resume
     */
    void setGlobalPause(bool paused) {
        m_globallyPaused.store(paused, std::memory_order_release);
    }

    /**
     * @brief Check if background simulation is globally paused
     */
    [[nodiscard]] bool isGloballyPaused() const noexcept {
        return m_globallyPaused.load(std::memory_order_acquire);
    }

    /**
     * @brief Main update - handles tier recalc AND background entity processing
     *
     * Power-efficient single entry point called at end of GameEngine::update():
     * - Phase 1: Tier updates every 60 frames (~1 sec at 60Hz)
     * - Phase 2: Background entity processing at 10Hz (only if entities exist)
     *
     * When paused: immediate return, zero CPU cycles.
     *
     * @param referencePoint Player/camera position for tier distance calculation
     * @param deltaTime Frame delta time (for accumulator)
     */
    void update(const Vector2D& referencePoint, float deltaTime);

    /**
     * @brief Wait for any async background processing to complete
     *
     * Call before state transitions or when synchronization is needed.
     */
    void waitForAsyncCompletion();

    /**
     * @brief Set the reference point for tier calculations
     *
     * Typically the player position. Entities are tiered based on
     * distance from this point.
     *
     * @param position Reference position (usually player/camera)
     */
    void setReferencePoint(const Vector2D& position);

    /**
     * @brief Get current reference point
     */
    [[nodiscard]] Vector2D getReferencePoint() const { return m_referencePoint; }

    /**
     * @brief Update entity simulation tiers based on reference point
     *
     * Should be called periodically (e.g., every 60 frames) to reassign
     * entities to Active/Background/Hibernated tiers.
     * Uses EntityDataManager::updateSimulationTiers() internally.
     */
    void updateTiers();

    /**
     * @brief Force tier update on next frame
     */
    void invalidateTiers() { m_tiersDirty.store(true, std::memory_order_release); }

    /**
     * @brief Check if manager has any work to do
     * @return true if there are background entities or tiers need checking
     */
    [[nodiscard]] bool hasWork() const noexcept {
        return m_hasNonActiveEntities.load(std::memory_order_acquire) ||
               m_tiersDirty.load(std::memory_order_acquire);
    }

    // Configuration
    void setActiveRadius(float radius) { m_activeRadius = radius; }
    void setBackgroundRadius(float radius) { m_backgroundRadius = radius; }

    /**
     * @brief Configure tier radii based on screen dimensions
     *
     * Calculates radii relative to the screen's half-diagonal (center to corner).
     * - Active: 1.5x half-diagonal (visible area + buffer)
     * - Background: 3x half-diagonal (pre-loading zone)
     *
     * @param screenWidth Logical screen width
     * @param screenHeight Logical screen height
     */
    void configureForScreenSize(int screenWidth, int screenHeight) {
        // Half-diagonal = distance from screen center to corner
        float halfWidth = static_cast<float>(screenWidth) / 2.0f;
        float halfHeight = static_cast<float>(screenHeight) / 2.0f;
        float halfDiagonal = std::sqrt(halfWidth * halfWidth + halfHeight * halfHeight);

        // Active: 1.5x visible range (entities on screen + small buffer)
        m_activeRadius = halfDiagonal * 1.5f;
        // Background: 2x visible range (pre-load zone for smooth transitions)
        m_backgroundRadius = halfDiagonal * 2.0f;

        // Mark tiers dirty to recalculate with new radii
        invalidateTiers();
    }

    [[nodiscard]] float getActiveRadius() const { return m_activeRadius; }
    [[nodiscard]] float getBackgroundRadius() const { return m_backgroundRadius; }

    /**
     * @brief Set the update rate for background simulation
     * @param hz Target updates per second (default: 30Hz)
     */
    void setUpdateRate(float hz) {
        m_updateRate = hz;
        m_updateInterval = 1.0f / hz;
    }

    [[nodiscard]] float getUpdateRate() const { return m_updateRate; }

    // Performance metrics (follows AIManager pattern)
    struct PerfStats {
        double lastUpdateMs{0.0};
        double avgUpdateMs{0.0};
        size_t lastEntitiesProcessed{0};
        size_t lastBatchCount{0};
        size_t lastTierChanges{0};
        uint64_t totalUpdates{0};
        bool lastWasThreaded{false};

        static constexpr double ALPHA = 0.05;  // EMA smoothing

        void updateAverage(double newMs) {
            if (totalUpdates == 0) {
                avgUpdateMs = newMs;
            } else {
                avgUpdateMs = ALPHA * newMs + (1.0 - ALPHA) * avgUpdateMs;
            }
            totalUpdates++;
        }
    };

    [[nodiscard]] const PerfStats& getPerfStats() const { return m_perf; }
    void resetPerfStats() { m_perf = PerfStats{}; }

private:
    BackgroundSimulationManager() = default;
    ~BackgroundSimulationManager() = default;

    BackgroundSimulationManager(const BackgroundSimulationManager&) = delete;
    BackgroundSimulationManager& operator=(const BackgroundSimulationManager&) = delete;

    // Core processing (called when accumulator triggers update)
    void processBackgroundEntities(float fixedDeltaTime);

    // Batch processing (follows AIManager pattern)
    void processSingleThreaded(float deltaTime, const std::vector<size_t>& indices);
    void processMultiThreaded(float deltaTime, const std::vector<size_t>& indices,
                              size_t batchCount, size_t batchSize);
    void processBatch(float deltaTime, const std::vector<size_t>& indices,
                      size_t startIdx, size_t endIdx);

    // Type-specific simplified simulation
    void simulateNPC(float deltaTime, size_t index);
    void simulateItem(float deltaTime, size_t index);

    // Configuration
    // Default radii based on 1920x1080 logical resolution:
    // - Half-diagonal (center to corner) ≈ 1100 pixels
    // - Active: 1.5x half-diagonal = entities visible + small buffer
    // - Background: 2x half-diagonal = pre-loading area for smooth transitions
    // - Hibernated: beyond background radius (no processing)
    float m_activeRadius{1650.0f};      // ~1.5x window half-diagonal (visible + buffer)
    float m_backgroundRadius{2200.0f};  // ~2x window half-diagonal (pre-load zone)

    // Tier update interval - every 120 main loop frames (~2 seconds at 60Hz)
    // Power optimization: entities move ~300 units/sec, radius is 1650px = safe margin
    static constexpr uint32_t TIER_UPDATE_INTERVAL{120};

    // Timing (accumulator pattern like TimestepManager)
    // 10Hz is sufficient for off-screen entities - saves CPU while maintaining world consistency
    // When entities become Active, they immediately get 60Hz updates
    float m_updateRate{10.0f};            // Target update rate in Hz
    float m_updateInterval{1.0f / 10.0f}; // Time between updates (100ms at 10Hz)
    double m_accumulator{0.0};            // Time accumulator for fixed timestep

    // State
    Vector2D m_referencePoint{0.0f, 0.0f};
    bool m_referencePointSet{false};  // First setReferencePoint call always updates
    uint32_t m_framesSinceTierUpdate{0};
    std::atomic<bool> m_tiersDirty{true};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_isShutdown{false};
    std::atomic<bool> m_globallyPaused{false};
    std::atomic<bool> m_hasNonActiveEntities{false};  // Track if work exists

    // Async task tracking (follows AIManager pattern)
    std::vector<std::future<void>> m_batchFutures;
    mutable std::mutex m_futuresMutex;

    // Reusable buffers (avoid per-frame allocation)
    std::vector<size_t> m_backgroundIndices;

    // Performance tracking
    PerfStats m_perf;

    // Minimum batch size for threading (threading threshold is adaptive via WorkerBudget)
    static constexpr size_t MIN_BATCH_SIZE = 64;
};

#endif // BACKGROUND_SIMULATION_MANAGER_HPP
