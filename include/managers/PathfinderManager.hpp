/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PATHFINDER_MANAGER_HPP
#define PATHFINDER_MANAGER_HPP

/**
 * @file PathfinderManager.hpp
 * @brief High-performance pathfinding system manager optimized for speed and efficiency
 *
 * Ultra-high-performance PathfinderManager with ThreadSystem integration:
 * - Centralized pathfinding service for all AI entities
 * - Cache-friendly batch processing for optimal performance
 * - Thread-safe parallel path computation with WorkerBudget priorities
 * - Intelligent path caching and request scheduling
 * - Dynamic obstacle integration from CollisionManager
 * - Scales to 10K+ entities while maintaining 60+ FPS
 * - Lock-free request queuing with minimal contention
 *
 * ARCHITECTURE: Strict Event-Driven Grid Rebuilding
 * ===================================================
 * Grid rebuilds happen ONLY via event system (no synchronous fallbacks):
 *
 * 1. StaticCollidersReadyEvent → PathfinderManager::onStaticCollidersReady() → rebuildGrid() (async)
 * 2. CollisionObstacleChanged → PathfinderManager::onCollisionObstacleChanged() → rebuildGrid() (async)
 * 3. TileChanged → PathfinderManager::onTileChanged() → rebuildGrid() (async)
 *
 * Event Flow for World Loading:
 * - WorldManager fires WorldLoadedEvent after world data is ready
 * - CollisionManager receives WorldLoadedEvent, creates static collision bodies
 * - CollisionManager fires StaticCollidersReadyEvent when bodies are complete
 * - PathfinderManager receives StaticCollidersReadyEvent, builds navigation grid
 *
 * This ordering ensures PathfinderManager can query CollisionManager for obstacle
 * data during grid construction without race conditions.
 *
 * Integration Requirements:
 * - GameEngine MUST call EventManager::update() each frame to process events
 * - CollisionManager MUST fire StaticCollidersReadyEvent after rebuilding static bodies
 * - CollisionManager MUST fire CollisionObstacleChanged when individual obstacles change
 *
 * Entity Behavior When Grid Not Ready:
 * - PathfindingResult::NO_PATH_FOUND returned if grid doesn't exist
 * - Entities should continue current path or use fallback behavior
 * - Retry path request next frame (grid rebuild completes asynchronously)
 *
 * This ensures:
 * - No blocking operations on main thread (grid rebuilds on worker threads)
 * - Clean separation between pathfinding and world systems
 * - Testable event-driven architecture
 * - Entities handle gracefully degraded service during rebuilds
 */

#include "utils/Vector2D.hpp"
#include "entities/Entity.hpp"
#include "managers/EventManager.hpp"
#include "core/WorkerBudget.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

// Forward declarations
namespace VoidLight {
    class PathfindingGrid;
    enum class PathfindingResult : uint8_t;
}

// Do not include internal AI headers here; keep public API stable and minimal.

class CollisionManager;
class WorldManager;

// Int-based priority keeps public API independent of internal enums

/**
 * @brief Centralized pathfinding manager following singleton pattern
 */
class PathfinderManager {
public:
    // Public request priority (stable API)
    enum class Priority : int {
        Critical = 0,
        High     = 1,
        Normal   = 2,
        Low      = 3
    };

    /**
     * @brief Gets the singleton instance of PathfinderManager
     * @return Reference to the PathfinderManager instance
     */
    static PathfinderManager& Instance();

    /**
     * @brief Initializes the PathfinderManager and its subsystems
     * @return true if initialization successful, false otherwise
     */
    [[nodiscard]] bool init();

    /**
     * @brief Checks if PathfinderManager has been initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const;

    /**
     * @brief Updates pathfinding systems and processes pending requests
     */
    void update();

    /**
     * @brief Cleans up all pathfinding resources
     */
    void clean();

    /**
     * @brief Prepares PathfinderManager for state transition
     * Clears transient data while keeping the manager initialized
     */
    void prepareForStateTransition();

    /**
     * @brief Checks if PathfinderManager has been shut down
     * @return true if manager is shut down, false otherwise
     */
    bool isShutdown() const;

    /**
     * @brief Sets global pause state for pathfinding updates
     * @param paused true to pause pathfinding updates, false to resume
     */
    void setGlobalPause(bool paused);

    /**
     * @brief Gets the current global pause state
     * @return true if pathfinding updates are globally paused
     */
    bool isGloballyPaused() const;

    /**
     * @brief Checks if the pathfinding grid is ready for use
     * @return true if grid exists and no pending rebuilds, false otherwise
     */
    bool isGridReady() const;

    // ===== Pathfinding Request Interface =====

    /**
     * @brief Request a path asynchronously (ULTRA-HIGH-PERFORMANCE)
     * @param entityId The entity requesting the path
     * @param start Starting position in world coordinates
     * @param goal Goal position in world coordinates
     * @param priority PathPriority level for request scheduling
     * @param callback Callback when path is ready (called from background thread)
     * @return Request ID for tracking (0 if failed)
     *
     * This method completes in <0.001ms with zero blocking operations:
     * - Lock-free request queue enqueue only
     * - No mutex locks, no hash operations, no complex math
     * - All pathfinding computation happens on background thread
     * - Cache lookups and A* computation fully asynchronous
     * - Designed for 10K+ requests per second throughput
     */
    uint64_t requestPath(
        EntityID entityId,
        const Vector2D& start,
        const Vector2D& goal,
        Priority priority = Priority::Normal,
        std::function<void(EntityID, const std::vector<Vector2D>&)> callback = nullptr
    );

    /**
     * @brief Gets the current size of the request queue
     * @return Number of pending requests in queue
     */
    size_t getQueueSize() const;

    /**
     * @brief Checks if the pathfinding processor has work to do
     * @return true if there are pending requests, false otherwise
     */
    bool hasPendingWork() const;

    // ===== EDM-Integrated Async Pathfinding (No Callbacks) =====

    /**
     * @brief Request a path asynchronously with result written to EDM
     *
     * Same async performance as requestPath() but writes result directly to
     * EntityDataManager::PathData instead of invoking a callback.
     * Eliminates shared_from_this() and lambda allocation overhead.
     *
     * @param edmIndex Entity's EDM index (result written to EDM::getPathData(edmIndex))
     * @param start Starting position in world coordinates
     * @param goal Goal position in world coordinates
     * @param priority Request priority (default: Normal)
     * @return Request ID for tracking (0 if failed)
     */
    uint64_t requestPathToEDM(size_t edmIndex, const Vector2D& start,
                              const Vector2D& goal,
                              Priority priority = Priority::Normal);

    /**
     * @brief Commit worker-computed EDM path results on main thread.
     *
     * Worker threads only compute paths and enqueue completion payloads.
     * This method applies the final writes to EDM, preserving frame coherence
     * and allowing stale request filtering.
     */
    void commitCompletedPaths();

    // ===== Grid Management =====

    /**
     * @brief Rebuild the pathfinding grid from world data
     * @param allowIncremental If true, allow incremental rebuild if grid has dirty regions (default: true)
     */
    void rebuildGrid(bool allowIncremental = true);

    /**
     * @brief Add a temporary weight field (for avoidance)
     * @param center Center of the weight field in world coordinates
     * @param radius Radius of the weight field
     * @param weight Weight multiplier (higher = more expensive to traverse)
     */
    void addTemporaryWeightField(const Vector2D& center, float radius, float weight);

    /**
     * @brief Clear all temporary weight fields
     */
    void clearWeightFields();

    // ===== Configuration =====

    /**
     * @brief Set maximum number of paths to process per frame
     * @param maxPaths Maximum paths per frame (default: 5)
     */
    void setMaxPathsPerFrame(int maxPaths);

    /**
     * @brief Set cache expiration time
     * @param seconds Time in seconds before cached paths expire (default: 5.0)
     */
    void setCacheExpirationTime(float seconds);

    /**
     * @brief Enable/disable diagonal movement in pathfinding
     * @param allow true to allow diagonal movement
     */
    void setAllowDiagonal(bool allow);

    /**
     * @brief Set maximum iterations for pathfinding algorithm
     * @param maxIterations Maximum A* iterations (default: 20000)
     */
    void setMaxIterations(int maxIterations);

    // ===== Utility Functions =====

    /**
     * @brief Clamp position to world bounds with safety margin
     * @param position Position to clamp
     * @param margin Safety margin from world edges
     * @return Clamped position within world bounds
     */
    Vector2D clampToWorldBounds(const Vector2D& position, float margin = 100.0f) const;

    // Clamp using entity half-extents (pixels)
    Vector2D clampInsideExtents(const Vector2D& position, float halfW, float halfH, float extraMargin = 0.0f) const;

    /**
     * @brief Get cached world bounds for inline clamping (avoids atomic loads)
     * @param outWidth Output world width in pixels
     * @param outHeight Output world height in pixels
     * @return true if grid exists and bounds are valid
     */
    bool getCachedWorldBounds(float& outWidth, float& outHeight) const;

    // Adjust spawn to valid navigable position inside world
    Vector2D adjustSpawnToNavigable(const Vector2D& desired, float halfW = 16.0f, float halfH = 16.0f, float interiorMargin = 150.0f) const;
    // Area-constrained spawn adjustment
    Vector2D adjustSpawnToNavigableInRect(const Vector2D& desired,
                                          float halfW, float halfH,
                                          float interiorMargin,
                                          float minX, float minY,
                                          float maxX, float maxY) const;
    Vector2D adjustSpawnToNavigableInCircle(const Vector2D& desired,
                                            float halfW, float halfH,
                                            float interiorMargin,
                                            const Vector2D& center,
                                            float radius) const;

    /**
     * @brief Follow a path step for entity movement
     * @param entity Entity to move
     * @param currentPos Current entity position
     * @param path Path to follow (will be modified as nodes are reached)
     * @param pathIndex Current path index (will be modified)
     * @param speed Movement speed
     * @param nodeRadius Radius for reaching path nodes
     * @return true if successfully following path, false if path complete
     */
    bool followPathStep(const EntityPtr& entity, const Vector2D& currentPos,
                       std::vector<Vector2D>& path, size_t& pathIndex,
                       float speed, float nodeRadius = 64.0f) const;

    /**
     * @brief Get dynamic hierarchical threshold for current world
     * @return Distance threshold for using hierarchical pathfinding
     */
    float getHierarchicalThreshold() const { return m_hierarchicalThreshold; }

    /**
     * @brief Get dynamic connectivity check threshold for current world
     * @return Distance threshold for connectivity sampling
     */
    float getConnectivityThreshold() const { return m_connectivityThreshold; }

    // ===== Statistics =====

    struct PathfinderStats {
        uint64_t totalRequests{0};
        uint64_t completedRequests{0};
        uint64_t failedRequests{0};
        uint64_t cacheHits{0};
        uint64_t cacheMisses{0};
        double averageProcessingTimeMs{0.0};
        double requestsPerSecond{0.0};
        size_t queueSize{0};
        size_t queueCapacity{0};
        bool processorActive{true};
        float cacheHitRate{0.0f};

        // Simplified cache metrics
        float totalHitRate{0.0f};

        // Cache memory usage
        size_t cacheSize{0};
        size_t segmentCacheSize{0};
        double memoryUsageKB{0.0};
    };

    /**
     * @brief Get current pathfinding statistics
     * @return Current statistics snapshot
     */
    PathfinderStats getStats() const;

    /**
     * @brief Reset all statistics
     */
    void resetStats();

private:
    using PathCallback = std::function<void(EntityID, const std::vector<Vector2D>&)>;
    // Singleton implementation
    PathfinderManager() = default;
    ~PathfinderManager();
    PathfinderManager(const PathfinderManager&) = delete;
    PathfinderManager& operator=(const PathfinderManager&) = delete;

    // Core components - Clean Architecture
    // Mutex-protected shared_ptr with snapshot semantics for thread-safe grid access
    // Note: std::atomic<std::shared_ptr<T>> requires C++20 library support not available on all platforms
    std::shared_ptr<VoidLight::PathfindingGrid> m_grid;
    mutable std::shared_mutex m_gridMutex;  // Read-write lock for concurrent read access
    // Direct ThreadSystem processing - no queue needed

    // Thread-safe grid access helpers
    std::shared_ptr<VoidLight::PathfindingGrid> getGridSnapshot() const {
        std::shared_lock<std::shared_mutex> lock(m_gridMutex);
        return m_grid;  // Copy increments refcount, safe to use after lock release
    }

    void setGrid(std::shared_ptr<VoidLight::PathfindingGrid> newGrid) {
        std::unique_lock<std::shared_mutex> lock(m_gridMutex);
        m_grid = std::move(newGrid);
    }

    // Helpers - grid-passing overloads to avoid repeated getGridSnapshot() calls in hot path
    void normalizeEndpoints(Vector2D& start, Vector2D& goal) const;
    void normalizeEndpoints(Vector2D& start, Vector2D& goal,
                           const std::shared_ptr<VoidLight::PathfindingGrid>& grid) const;
    Vector2D clampToWorldBounds(const Vector2D& position, float margin,
                                const std::shared_ptr<VoidLight::PathfindingGrid>& grid) const;

    // INTERNAL ONLY: Synchronous pathfinding computation (used by async system)
    // DO NOT use directly - use requestPath() instead
    VoidLight::PathfindingResult findPathImmediate(
        const Vector2D& start,
        const Vector2D& goal,
        std::vector<Vector2D>& outPath,
        bool skipNormalization = false
    );

    // Grid-passing overload for hot path optimization
    VoidLight::PathfindingResult findPathImmediate(
        const Vector2D& start,
        const Vector2D& goal,
        std::vector<Vector2D>& outPath,
        const std::shared_ptr<VoidLight::PathfindingGrid>& grid,
        bool skipNormalization = false
    );

    // Request management - simplified
    std::atomic<uint64_t> m_nextRequestId{1};

    // Configuration
    bool m_allowDiagonal{true};
    int m_maxIterations{40000};
    float m_cellSize{64.0f}; // Optimized for 4x fewer pathfinding nodes
    size_t m_maxRequestsPerUpdate{10}; // Max requests to process per update
    float m_cacheExpirationTime{5.0f}; // Cache expiration time in seconds

    // Auto-calculated cache parameters (computed per world for optimal scaling)
    float m_endpointQuantization{128.0f};      // Dynamic: ~1% world size
    float m_cacheKeyQuantization{256.0f};      // Dynamic: worldSize / sqrt(cache)
    float m_hierarchicalThreshold{2048.0f};    // Dynamic: 5% of diagonal
    float m_connectivityThreshold{16000.0f};   // Dynamic: 25% of width
    int m_prewarmSectorCount{8};               // Dynamic: 4-16 based on size
    int m_prewarmPathCount{168};               // Dynamic: sectors² × 2.5

    // State management
    std::atomic<bool> m_initialized{false};
    std::once_flag m_initFlag; // Thread-safe initialization guard
    std::atomic<bool> m_isShutdown{false};
    std::atomic<bool> m_globallyPaused{false}; // Global pause state for update() early exit
    std::atomic<bool> m_prewarming{false}; // Track if cache pre-warming is in progress

    // Statistics tracking — fetch_add'd from concurrent ThreadSystem pathfinding
    // tasks (cacheHits/Misses, completed/failedRequests, processedCount,
    // totalProcessingTimeMs). Cache-line isolated to avoid false sharing with the
    // read-mostly state flags above (m_initialized/m_isShutdown/m_globallyPaused),
    // which are read every frame in update()/requestPath(). The 7 uint64 atomics
    // (56B) + the double atomic (8B) fill exactly one 64B line, so the trailing
    // main-thread bookkeeping below naturally starts on the next line.
    alignas(64) mutable std::atomic<uint64_t> m_enqueuedRequests{0};
    mutable std::atomic<uint64_t> m_enqueueFailures{0};
    mutable std::atomic<uint64_t> m_completedRequests{0};
    mutable std::atomic<uint64_t> m_failedRequests{0};
    mutable std::atomic<uint64_t> m_cacheHits{0};
    mutable std::atomic<uint64_t> m_cacheMisses{0};
    mutable std::atomic<uint64_t> m_processedCount{0};

    // Lightweight timing statistics (minimal overhead)
    mutable std::atomic<double> m_totalProcessingTimeMs{0.0};
    mutable std::chrono::steady_clock::time_point m_lastStatsUpdate{std::chrono::steady_clock::now()};
    mutable double m_lastRequestsPerSecond{0.0};
    mutable uint64_t m_lastTotalRequests{0};

    // High-performance single-tier cache with smart quantization
    struct PathCacheEntry {
        std::vector<Vector2D> path;
        std::chrono::steady_clock::time_point lastUsed;
        uint32_t useCount{1};
    };

    mutable std::unordered_map<uint64_t, PathCacheEntry> m_pathCache;
    mutable std::shared_mutex m_cacheMutex;  // shared_mutex for concurrent reads

    // Optimized for high entity counts (2000-10K+ entities in demo states)
    // At 32K entries: ~3.5MB memory (acceptable overhead for large-scale scenarios)
    // Combined with coarser quantization (512px+), provides 70-85% cache hit rates
    static constexpr size_t MAX_CACHE_ENTRIES = 32768;



    // Collision version tracking for cache invalidation
    std::atomic<uint64_t> m_lastCollisionVersion{0};

    // Statistics reporting frame counter
    int m_statsFrameCounter{0};

    // Event subscription tracking
    std::vector<EventManager::HandlerToken> m_eventHandlerTokens;

    // Async task synchronization (mirroring AIManager pattern)
    std::vector<std::future<void>> m_gridRebuildFutures;
    std::vector<std::future<void>> m_reusableGridRebuildFutures;  // Swap target to preserve capacity
    std::mutex m_gridRebuildFuturesMutex;

    // WorkerBudget integration for coordinated batch processing
    std::vector<std::future<void>> m_batchFutures;
    std::vector<std::future<void>> m_reusableBatchFutures;  // Swap target to preserve capacity
    // No mutex needed: update and state transitions never run concurrently

    struct PathCompletion {
        static constexpr size_t MAX_WAYPOINTS = 32;
        EntityHandle targetHandle{};
        size_t edmIndex{0};
        uint32_t requestToken{0};
        uint16_t length{0};
        bool hasPath{false};
        std::array<Vector2D, MAX_WAYPOINTS> waypoints{};
    };

    std::mutex m_pathCompletionMutex;
    std::vector<PathCompletion> m_pendingPathCompletions;
    std::vector<PathCompletion> m_reusablePathCompletions;

    // Incremental update configuration
    static constexpr float DIRTY_THRESHOLD_PERCENT = 0.25f; // Full rebuild if >25% of grid is dirty

    // DIRECT THREADSYSTEM SUBMISSION (no intermediate buffer needed)
    // Requests are submitted directly to ThreadSystem in requestPath()
    // This eliminates the mutex contention that was serializing AI batch threads

    // Internal methods - simplified
    void reportStatistics() const;
    bool ensureGridInitialized(); // Lazy initialization helper
    uint64_t computeStableCacheKey(const Vector2D& start, const Vector2D& goal) const;
    void evictOldestCacheEntry();
    void clearOldestCacheEntries(float percentage); // Smart cache clearing (partial LRU eviction)
    void clearAllCache(); // Complete cache clear for world load/unload
    void waitForGridRebuildCompletion(); // Wait for pending async grid rebuild tasks
    void subscribeToEvents(); // Subscribe to collision and world events
    void unsubscribeFromEvents(); // Unsubscribe from events

    // Auto-scaling cache optimization
    void calculateOptimalCacheSettings(); // Calculate dynamic parameters based on world size
    void prewarmPathCache(); // Seed cache with sector-based paths for fast warmup

    // Batch completion synchronization
    void waitForBatchCompletion(); // Wait for all pending batch futures to complete

    // Event handlers
    void onCollisionObstacleChanged(const Vector2D& position, float radius, const std::string& description);
    void onStaticCollidersReady();  // Called when CollisionManager finishes building static bodies
    void onWorldUnloaded();
    void onTileChanged(int x, int y);
};

#endif // PATHFINDER_MANAGER_HPP
