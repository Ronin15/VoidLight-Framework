/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "managers/PathfinderManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "ai/pathfinding/PathfindingGrid.hpp"
#include "managers/WorldManager.hpp"
#include "managers/EventManager.hpp" // Must include for HandlerToken definition
#include "events/CollisionObstacleChangedEvent.hpp"
#include "events/WorldEvent.hpp"
#include <string_view>
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include <chrono>
#include <array>
#include <algorithm>
#include <cmath>
#include <format>

PathfinderManager& PathfinderManager::Instance() {
    static PathfinderManager instance;
    return instance;
}

PathfinderManager::~PathfinderManager() {
    if (!m_isShutdown.load(std::memory_order_acquire)) {
        clean();
    }
}

// Internal priority mapping helpers (implementation-only)
namespace {
    inline VoidLight::TaskPriority mapEnumToTaskPriority(PathfinderManager::Priority p) {
        switch (p) {
            case PathfinderManager::Priority::Critical: return VoidLight::TaskPriority::Critical;
            case PathfinderManager::Priority::High:     return VoidLight::TaskPriority::High;
            case PathfinderManager::Priority::Normal:   return VoidLight::TaskPriority::Normal;
            case PathfinderManager::Priority::Low:      return VoidLight::TaskPriority::Low;
            default:                                    return VoidLight::TaskPriority::Normal;
        }
    }
}

bool PathfinderManager::init() {
    if (m_initialized.load()) {
        return true;
    }

    try {
        PATHFIND_INFO("Initializing PathfinderManager with clean architecture");

        // Reset shutdown flag (allows re-initialization after clean())
        m_isShutdown.store(false, std::memory_order_release);

        // No queue needed - requests processed directly on ThreadSystem

        // Grid will be created lazily when first needed and world is available
        m_cellSize = 64.0f; // Optimized cell size for 4x performance improvement

        // Cache initialized in header with default expiration time

        PATHFIND_INFO("PathfinderManager initialized successfully");

        // ThreadSystem initialization is managed by the application/tests

        // Subscribe to collision obstacle change events for cache invalidation
        subscribeToEvents();

        m_initialized.store(true);
        PATHFIND_INFO("PathfinderManager initialized successfully with clean architecture");
        return true;
    }
    catch (const std::exception& e) {
        PATHFIND_ERROR(std::format("Failed to initialize PathfinderManager: {}", e.what()));
        return false;
    }
}

bool PathfinderManager::isInitialized() const {
    return m_initialized.load();
}

void PathfinderManager::update() {
    if (!m_initialized.load() || m_isShutdown.load(std::memory_order_acquire) ||
        m_globallyPaused.load(std::memory_order_acquire)) {
        return;
    }

    commitCompletedPaths();

    // Requests are submitted directly to ThreadSystem in requestPath() - no processing needed here

    VOIDLIGHT_DEBUG_ONLY(
        // Interval stats logging - zero overhead in release (entire block compiles out)
        if (++m_statsFrameCounter >= 600) {
            m_statsFrameCounter = 0;
            reportStatistics();
        }
    )
}

void PathfinderManager::setGlobalPause(bool paused) {
    m_globallyPaused.store(paused, std::memory_order_release);
}

bool PathfinderManager::isGloballyPaused() const {
    return m_globallyPaused.load(std::memory_order_acquire);
}

void PathfinderManager::clean() {
    if (m_isShutdown.load(std::memory_order_acquire)) {
        return;
    }

    PATHFIND_INFO("Cleaning up PathfinderManager");

    // Wait for grid rebuild tasks to complete before shutdown
    waitForGridRebuildCompletion();

    commitCompletedPaths();

    // Unsubscribe from events
    unsubscribeFromEvents();

    // Clear cache (shutdown - can clear all at once since frame timing doesn't matter)
    {
        std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
        m_pathCache.clear();
    }

    // No queue to clear - using direct ThreadSystem processing
    {
        std::lock_guard<std::mutex> lock(m_pathCompletionMutex);
        m_pendingPathCompletions.clear();
        m_reusablePathCompletions.clear();
    }

    // Clear grid
    setGrid(nullptr);

    m_initialized.store(false);
    m_isShutdown.store(true, std::memory_order_release);
    PATHFIND_INFO("PathfinderManager cleaned up");
}

void PathfinderManager::prepareForStateTransition() {
    PATHFIND_INFO("Preparing PathfinderManager for state transition...");

    if (!m_initialized.load() || m_isShutdown.load(std::memory_order_acquire)) {
        PATHFIND_WARN("PathfinderManager not initialized or already shutdown during state transition");
        return;
    }

    // Wait for any running grid rebuild tasks to complete BEFORE clearing data
    // This prevents async tasks from accessing deleted world data during state transitions
    waitForGridRebuildCompletion();

    commitCompletedPaths();

    // Clear path cache completely for fresh state
    {
        std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
        size_t cacheSize = m_pathCache.size();
        m_pathCache.clear();
        PATHFIND_INFO_IF(cacheSize > 0,
            std::format("Cleared {} cached paths for state transition", cacheSize));
    }

    // Reset statistics for clean slate
    m_enqueuedRequests.store(0);
    m_enqueueFailures.store(0);
    m_completedRequests.store(0);
    m_failedRequests.store(0);
    m_cacheHits.store(0);
    m_cacheMisses.store(0);
    m_processedCount.store(0);
    m_lastRequestsPerSecond = 0.0;
    m_lastTotalRequests = 0;

    // Reset statistics frame counter
    m_statsFrameCounter = 0;

    // Reset collision version tracking
    m_lastCollisionVersion.store(0);

    // Keep grid instance but invalidate any cached data within it
    // Grid will be rebuilt when needed by new state
    if (getGridSnapshot()) {
        // Clear any temporary weight fields that might be state-specific
        clearWeightFields();
    }

    // Event handlers are persistent (registered via registerPersistentHandler
    // in subscribeToEvents). No re-subscription needed across state transitions.

    PATHFIND_INFO("PathfinderManager state transition complete - cleared transient data, kept manager initialized");
}

void PathfinderManager::commitCompletedPaths() {
    m_reusablePathCompletions.clear();
    {
        std::lock_guard<std::mutex> lock(m_pathCompletionMutex);
        if (m_pendingPathCompletions.empty()) {
            return;
        }
        m_reusablePathCompletions.swap(m_pendingPathCompletions);
    }

    auto& edm = EntityDataManager::Instance();
    for (const auto& completion : m_reusablePathCompletions) {
        if (!completion.targetHandle.isValid() || !edm.hasPathData(completion.edmIndex)) {
            continue;
        }

        const size_t resolvedIndex = edm.getIndex(completion.targetHandle);
        if (resolvedIndex == SIZE_MAX || resolvedIndex != completion.edmIndex) {
            continue;
        }

        auto& pd = edm.getPathData(completion.edmIndex);
        const uint32_t latestToken = pd.latestPathRequestId.load(std::memory_order_acquire);
        if (completion.requestToken != latestToken) {
            continue; // Stale completion - a newer request exists for this slot.
        }

        if (!completion.hasPath || completion.length == 0) {
            pd.pathRequestPending.store(0, std::memory_order_release);
            continue;
        }

        auto slot = edm.getWaypointSlot(completion.edmIndex);
        for (uint16_t i = 0; i < completion.length; ++i) {
            slot[i] = completion.waypoints[i];
        }
        edm.finalizePath(completion.edmIndex, completion.length);
    }

    m_reusablePathCompletions.clear();
}


bool PathfinderManager::isShutdown() const {
    return m_isShutdown.load(std::memory_order_acquire);
}

bool PathfinderManager::isGridReady() const {
    // Grid is ready if it exists and all rebuild tasks are complete
    if (!getGridSnapshot()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_gridRebuildFuturesMutex));

    // Check if all futures are ready (completed or invalid)
    return std::all_of(m_gridRebuildFutures.begin(), m_gridRebuildFutures.end(),
        [](const auto& future) {
            return !future.valid() || future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        });
}

uint64_t PathfinderManager::requestPath(
    EntityID entityId,
    const Vector2D& start,
    const Vector2D& goal,
    Priority priority,
    std::function<void(EntityID, const std::vector<Vector2D>&)> callback
) {
    if (!m_initialized.load() || m_isShutdown.load(std::memory_order_acquire)) {
        return 0;
    }

    // Compute cache key from RAW coordinates BEFORE normalization
    const uint64_t cacheKey = computeStableCacheKey(start, goal);

    // Normalize endpoints for pathfinding accuracy
    Vector2D nStart = start;
    Vector2D nGoal = goal;
    normalizeEndpoints(nStart, nGoal);

    // Generate unique request ID (atomic, lock-free)
    const uint64_t requestId = m_nextRequestId.fetch_add(1, std::memory_order_relaxed);
    m_enqueuedRequests.fetch_add(1, std::memory_order_relaxed);

    // LOCK-FREE: Submit directly to ThreadSystem
    // Callback is captured and called when task completes
    auto& threadSystem = VoidLight::ThreadSystem::Instance();

    // Capture callback by move for async execution (avoids copy)
    auto work = [this, entityId, nStart, nGoal, cacheKey, callback = std::move(callback)]() {
        std::vector<Vector2D> path;
        bool cacheHit = false;

        // Cache lookup (shared_lock allows concurrent readers)
        {
            std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
            auto it = m_pathCache.find(cacheKey);
            if (it != m_pathCache.end()) {
                path = it->second.path;
                cacheHit = true;
                m_cacheHits.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_cacheMisses.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Bump cache-entry metadata under a separate try-lock so reads stay concurrent
        if (cacheHit) {
            std::unique_lock<std::shared_mutex> lock(m_cacheMutex, std::try_to_lock);
            if (lock.owns_lock()) {
                auto it = m_pathCache.find(cacheKey);
                if (it != m_pathCache.end()) {
                    it->second.lastUsed = std::chrono::steady_clock::now();
                    it->second.useCount++;
                }
            }
        }

        // Compute path if not cached
        if (!cacheHit) {
            findPathImmediate(nStart, nGoal, path, true);

            // Store in cache
            if (!path.empty()) {
                std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
                if (m_pathCache.size() >= MAX_CACHE_ENTRIES) {
                    evictOldestCacheEntry();
                }
                PathCacheEntry entry;
                entry.path = path;
                entry.lastUsed = std::chrono::steady_clock::now();
                entry.useCount = 1;
                m_pathCache[cacheKey] = std::move(entry);
            }
        }

        // Update stats
        if (!path.empty()) {
            m_completedRequests.fetch_add(1, std::memory_order_relaxed);
        } else {
            m_failedRequests.fetch_add(1, std::memory_order_relaxed);
        }
        m_processedCount.fetch_add(1, std::memory_order_relaxed);

        // Fire callback directly (ThreadSystem completion mechanism)
        if (callback) {
            callback(entityId, path);
        }
    };

    threadSystem.enqueueTask(work, mapEnumToTaskPriority(priority), "Pathfinding");

    return requestId;
}

uint64_t PathfinderManager::requestPathToEDM(
    size_t edmIndex,
    const Vector2D& start,
    const Vector2D& goal,
    Priority priority
) {
    if (!m_initialized.load() || m_isShutdown.load(std::memory_order_acquire)) {
        return 0;
    }

    // Get grid snapshot ONCE at start - avoid repeated atomic accesses
    auto gridSnapshot = getGridSnapshot();
    if (!gridSnapshot) {
        return 0;  // No grid available
    }

    // Compute cache key from RAW coordinates BEFORE normalization
    const uint64_t cacheKey = computeStableCacheKey(start, goal);

    // Normalize endpoints for pathfinding accuracy (pass grid to avoid re-fetching)
    Vector2D nStart = start;
    Vector2D nGoal = goal;
    normalizeEndpoints(nStart, nGoal, gridSnapshot);

    // Generate unique request ID
    const uint64_t requestId = m_nextRequestId.fetch_add(1, std::memory_order_relaxed);

    uint32_t requestToken = 0;
    EntityHandle requestHandle{};
    {
        auto& edm = EntityDataManager::Instance();
        if (!edm.hasPathData(edmIndex)) {
            return 0;
        }
        requestHandle = edm.getHandle(edmIndex);
        if (!requestHandle.isValid()) {
            return 0;
        }
        auto& pd = edm.getPathData(edmIndex);
        requestToken = pd.latestPathRequestId.fetch_add(1, std::memory_order_acq_rel) + 1;
        pd.pathRequestPending.store(1, std::memory_order_release);
    }

    // ASYNC: Enqueue to ThreadSystem and return immediately (non-blocking)
    auto& threadSystem = VoidLight::ThreadSystem::Instance();

    auto work = [this, requestHandle, edmIndex, requestToken, nStart, nGoal, cacheKey, gridSnapshot]() {
        // CRITICAL: Check shutdown before accessing any member data
        // This prevents use-after-free when PathfinderManager is destroyed while tasks pending
        if (m_isShutdown.load(std::memory_order_acquire)) {
            return;
        }

        std::array<Vector2D, FixedWaypointSlot::MAX_WAYPOINTS_PER_ENTITY> cachedWaypoints;
        uint16_t cachedLen = 0;
        bool cacheHit = false;
        thread_local std::vector<Vector2D> path;
        path.clear();

        // Cache lookup (shared_lock allows concurrent readers)
        {
            std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
            auto it = m_pathCache.find(cacheKey);
            if (it != m_pathCache.end()) {
                size_t len = std::min(it->second.path.size(),
                                      FixedWaypointSlot::MAX_WAYPOINTS_PER_ENTITY);
                for (size_t i = 0; i < len; ++i) {
                    cachedWaypoints[i] = it->second.path[i];
                }
                cachedLen = static_cast<uint16_t>(len);
                cacheHit = true;
                m_cacheHits.fetch_add(1, std::memory_order_relaxed);
            } else {
                m_cacheMisses.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Compute path if not cached (pass grid to avoid re-fetching)
        if (!cacheHit) {
            if (m_isShutdown.load(std::memory_order_acquire)) {
                return;
            }
            findPathImmediate(nStart, nGoal, path, gridSnapshot, true);

            // Store in cache (write lock to ensure cache fills on EDM requests)
            // Double-check shutdown: first check avoids lock acquisition,
            // second check ensures shutdown didn't occur while waiting for lock
            if (!path.empty()) {
                if (m_isShutdown.load(std::memory_order_acquire)) {
                    return;
                }

                std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
                if (m_isShutdown.load(std::memory_order_acquire)) {
                    return;
                }

                auto now = std::chrono::steady_clock::now();
                auto it = m_pathCache.find(cacheKey);
                if (it != m_pathCache.end()) {
                    it->second.path = path;
                    it->second.lastUsed = now;
                    it->second.useCount++;
                } else {
                    if (m_pathCache.size() >= MAX_CACHE_ENTRIES) {
                        evictOldestCacheEntry();
                    }
                    PathCacheEntry entry;
                    entry.path = path;
                    entry.lastUsed = now;
                    entry.useCount = 1;
                    m_pathCache[cacheKey] = std::move(entry);
                }
            }
        }

        if (cacheHit) {
            std::unique_lock<std::shared_mutex> lock(m_cacheMutex, std::try_to_lock);
            if (lock.owns_lock()) {
                auto it = m_pathCache.find(cacheKey);
                if (it != m_pathCache.end()) {
                    it->second.lastUsed = std::chrono::steady_clock::now();
                    it->second.useCount++;
                }
            }
        }

        if (m_isShutdown.load(std::memory_order_acquire)) {
            return;
        }

        PathCompletion completion;
        completion.targetHandle = requestHandle;
        completion.edmIndex = edmIndex;
        completion.requestToken = requestToken;

        if (cacheHit && cachedLen > 0) {
            completion.length = cachedLen;
            completion.hasPath = true;
            for (uint16_t i = 0; i < cachedLen; ++i) {
                completion.waypoints[i] = cachedWaypoints[i];
            }
        } else if (!path.empty()) {
            const uint16_t len = static_cast<uint16_t>(std::min(path.size(), PathCompletion::MAX_WAYPOINTS));
            completion.length = len;
            completion.hasPath = len > 0;
            for (uint16_t i = 0; i < len; ++i) {
                completion.waypoints[i] = path[i];
            }
        } else {
            completion.length = 0;
            completion.hasPath = false;
        }

        std::lock_guard<std::mutex> lock(m_pathCompletionMutex);
        m_pendingPathCompletions.emplace_back(completion);

    };

    threadSystem.enqueueTask(work, mapEnumToTaskPriority(priority), "PathToEDM");

    return requestId;  // Returns immediately - path computed asynchronously
}

VoidLight::PathfindingResult PathfinderManager::findPathImmediate(
    const Vector2D& start,
    const Vector2D& goal,
    std::vector<Vector2D>& outPath,
    bool skipNormalization
) {
    if (!m_initialized.load() || m_isShutdown.load(std::memory_order_acquire)) {
        return VoidLight::PathfindingResult::NO_PATH_FOUND;
    }

    // Start timing for performance statistics
    auto startTime = std::chrono::steady_clock::now();

    // Ensure grid is initialized BEFORE normalizing endpoints (needs grid for bounds)
    if (!ensureGridInitialized()) {
        // Record timing even for failed requests
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        m_totalProcessingTimeMs.fetch_add(duration.count() / 1000.0, std::memory_order_relaxed);
        return VoidLight::PathfindingResult::NO_PATH_FOUND;
    }

    // Take a snapshot of the grid to avoid races with background rebuilds
    auto gridSnapshot = getGridSnapshot();
    if (!gridSnapshot) {
        // Record timing even for failed requests
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        m_totalProcessingTimeMs.fetch_add(duration.count() / 1000.0, std::memory_order_relaxed);
        return VoidLight::PathfindingResult::NO_PATH_FOUND;
    }

    // Normalize endpoints AFTER grid exists (needs grid for bounds/snapping)
    // Skip if already normalized (prevents double normalization in async path flow)
    Vector2D nStart = start;
    Vector2D nGoal = goal;
    if (!skipNormalization) {
        normalizeEndpoints(nStart, nGoal);
    }

    // Determine which pathfinding algorithm to use based on sophisticated heuristics
    VoidLight::PathfindingResult result;
    
    if (gridSnapshot->shouldUseHierarchicalPathfinding(nStart, nGoal)) {
        // Use hierarchical pathfinding - try hierarchical first
        result = gridSnapshot->findPathHierarchical(nStart, nGoal, outPath);
        
        // Fallback to direct if hierarchical fails
        if (result != VoidLight::PathfindingResult::SUCCESS || outPath.empty()) {
            std::vector<Vector2D> directPath;
            auto directResult = gridSnapshot->findPath(nStart, nGoal, directPath);
            if (directResult == VoidLight::PathfindingResult::SUCCESS && !directPath.empty()) {
                outPath = std::move(directPath);
                result = directResult;
            }
        }
    } else {
        // Use direct pathfinding for optimal performance
        result = gridSnapshot->findPath(nStart, nGoal, outPath);
    }

    // Record timing for all completed requests
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_totalProcessingTimeMs.fetch_add(duration.count() / 1000.0, std::memory_order_relaxed);

    return result;
}

// Grid-passing overload - avoids repeated getGridSnapshot() calls in hot path
VoidLight::PathfindingResult PathfinderManager::findPathImmediate(
    const Vector2D& start,
    const Vector2D& goal,
    std::vector<Vector2D>& outPath,
    const std::shared_ptr<VoidLight::PathfindingGrid>& grid,
    bool skipNormalization
) {
    if (!m_initialized.load() || m_isShutdown.load(std::memory_order_acquire) || !grid) {
        return VoidLight::PathfindingResult::NO_PATH_FOUND;
    }

    // Normalize endpoints if needed (pass grid to avoid re-fetching)
    Vector2D nStart = start;
    Vector2D nGoal = goal;
    if (!skipNormalization) {
        normalizeEndpoints(nStart, nGoal, grid);
    }

    // Determine which pathfinding algorithm to use
    VoidLight::PathfindingResult result;

    if (grid->shouldUseHierarchicalPathfinding(nStart, nGoal)) {
        result = grid->findPathHierarchical(nStart, nGoal, outPath);

        if (result != VoidLight::PathfindingResult::SUCCESS || outPath.empty()) {
            std::vector<Vector2D> directPath;
            auto directResult = grid->findPath(nStart, nGoal, directPath);
            if (directResult == VoidLight::PathfindingResult::SUCCESS && !directPath.empty()) {
                outPath = std::move(directPath);
                result = directResult;
            }
        }
    } else {
        result = grid->findPath(nStart, nGoal, outPath);
    }

    return result;
}

size_t PathfinderManager::getQueueSize() const {
    return 0; // No queue - direct ThreadSystem processing
}

bool PathfinderManager::hasPendingWork() const {
    return false; // No queue - work submitted directly to ThreadSystem
}

void PathfinderManager::rebuildGrid(bool allowIncremental) {
    // HYBRID OPTIMIZATION: Smart decision between full parallel rebuild and incremental update
    // - Full rebuild: Use WorkerBudget parallel batching (2-4× speedup)
    // - Incremental: Rebuild only dirty regions (~10-30× speedup for small changes)

    const auto& worldManager = WorldManager::Instance();
    if (!worldManager.hasActiveWorld()) {
        PATHFIND_DEBUG("Cannot rebuild grid - no active world");
        return;
    }

    // Smart rebuild decision: check if incremental update is beneficial
    auto currentGrid = getGridSnapshot();
    if (allowIncremental && currentGrid && currentGrid->hasDirtyRegions()) {
        float dirtyPercent = currentGrid->calculateDirtyPercent();

        if (dirtyPercent <= DIRTY_THRESHOLD_PERCENT * 100.0f) {
            // Incremental update is beneficial (small change)
            PATHFIND_DEBUG(std::format("Incremental rebuild: {}% dirty (threshold: {}%)",
                          dirtyPercent, DIRTY_THRESHOLD_PERCENT * 100.0f));

            // Submit incremental rebuild to ThreadSystem (non-blocking)
            auto& threadSystem = VoidLight::ThreadSystem::Instance();
            auto rebuildFuture = threadSystem.enqueueTaskWithResult(
                [this]() {
                    if (auto grid = getGridSnapshot()) {
                        grid->rebuildDirtyRegions();
                        PATHFIND_INFO("Incremental grid rebuild complete");
                    }
                },
                VoidLight::TaskPriority::Low,
                "PathfindingGridRebuild_Incremental"
            );

            std::lock_guard<std::mutex> lock(m_gridRebuildFuturesMutex);
            m_gridRebuildFutures.push_back(std::move(rebuildFuture));
            return; // Early return - incremental rebuild submitted
        } else {
            // Too much dirty (>25%) - full rebuild is faster
            PATHFIND_DEBUG(std::format("Full rebuild: {}% dirty exceeds threshold ({}%)",
                          dirtyPercent, DIRTY_THRESHOLD_PERCENT * 100.0f));
            currentGrid->clearDirtyRegions(); // Clear dirty regions, will do full rebuild
        }
    }

    int worldWidth = 0, worldHeight = 0;
    if (!worldManager.getWorldDimensions(worldWidth, worldHeight) || worldWidth <= 0 || worldHeight <= 0) {
        PATHFIND_WARN("Invalid world dimensions during rebuild");
        return;
    }

    float const worldPixelWidth = worldWidth * VoidLight::TILE_SIZE;
    float const worldPixelHeight = worldHeight * VoidLight::TILE_SIZE;
    int const gridWidth = static_cast<int>(worldPixelWidth / m_cellSize);
    int gridHeight = static_cast<int>(worldPixelHeight / m_cellSize);
    if (gridWidth <= 0 || gridHeight <= 0) {
        PATHFIND_WARN("Calculated grid too small during rebuild");
        return;
    }

    // Capture rebuild parameters for async execution
    float const cellSize = m_cellSize;
    bool allowDiagonal = m_allowDiagonal;
    int maxIterations = m_maxIterations;

    // Get ThreadSystem reference
    auto& threadSystem = VoidLight::ThreadSystem::Instance();

    // Use centralized WorkerBudgetManager for smart worker allocation
    auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();

    // Calculate optimal worker count for grid rebuild (considers queue pressure internally)
    size_t optimalWorkerCount = budgetMgr.getOptimalWorkers(
        VoidLight::SystemType::Pathfinding,
        static_cast<size_t>(gridHeight)  // Workload = number of rows
    );

    // Get batch strategy from WorkerBudget
    auto [batchCount, batchSize] = budgetMgr.getBatchStrategy(
        VoidLight::SystemType::Pathfinding,
        static_cast<size_t>(gridHeight),
        optimalWorkerCount
    );

    // Determine if parallel batching is beneficial
    const bool useParallelBatching = (batchCount > 1);

    if (!useParallelBatching) {
        // Sequential fallback for small grids or high queue pressure
        auto rebuildFuture = threadSystem.enqueueTaskWithResult(
            [gridWidth, gridHeight, cellSize, allowDiagonal, maxIterations, this]() {
                PATHFIND_DEBUG("Sequential grid rebuild starting");
                try {
                    auto newGrid = std::make_shared<VoidLight::PathfindingGrid>(
                        gridWidth, gridHeight, cellSize, Vector2D(0, 0)
                    );
                    newGrid->setAllowDiagonal(allowDiagonal);
                    newGrid->setMaxIterations(maxIterations);
                    newGrid->rebuildFromWorld(); // Full sequential rebuild

                    setGrid(newGrid);

                    if (m_initialized.load(std::memory_order_acquire)) {
                        std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
                        clearOldestCacheEntries(0.5f);
                        calculateOptimalCacheSettings();
                        prewarmPathCache();
                        PATHFIND_INFO("Grid rebuilt successfully (sequential)");
                    }
                } catch (const std::exception& e) {
                    PATHFIND_ERROR(std::format("Sequential grid rebuild failed: {}", e.what()));
                }
            },
            VoidLight::TaskPriority::Low,
            "PathfindingGridRebuild_Sequential"
        );

        std::lock_guard<std::mutex> lock(m_gridRebuildFuturesMutex);
        m_gridRebuildFutures.push_back(std::move(rebuildFuture));
        PATHFIND_DEBUG("Sequential grid rebuild submitted");
        return;
    }

    // Parallel batching path: batchCount and batchSize already computed above
    PATHFIND_DEBUG(std::format("Parallel grid rebuild: {} rows in {} batches (size: {}), workers: {}",
                  gridHeight, batchCount, batchSize, optimalWorkerCount));

    // Submit coordinated rebuild task that manages batches
    auto rebuildFuture = threadSystem.enqueueTaskWithResult(
        [gridWidth, gridHeight, cellSize, allowDiagonal, maxIterations, batchCount, batchSize, this]() {
            PATHFIND_DEBUG("Parallel grid rebuild starting");
            try {
                // Create grid on coordinator thread
                auto newGrid = std::make_shared<VoidLight::PathfindingGrid>(
                    gridWidth, gridHeight, cellSize, Vector2D(0, 0)
                );
                newGrid->setAllowDiagonal(allowDiagonal);
                newGrid->setMaxIterations(maxIterations);

                // Initialize arrays only - don't process any cells yet
                // Batches will process all cells in parallel
                newGrid->initializeArrays();

                // Submit row batches to process cells in parallel
                // Each batch processes its row range without re-initializing arrays
                std::vector<std::future<void>> batchFutures;
                batchFutures.reserve(batchCount);

                for (size_t i = 0; i < batchCount; ++i) {
                    int rowStart = static_cast<int>(i * batchSize);
                    int rowEnd = std::min(rowStart + static_cast<int>(batchSize), gridHeight);

                    auto batchFuture = VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
                        [newGrid, rowStart, rowEnd]() {
                            newGrid->rebuildFromWorld(rowStart, rowEnd);
                        },
                        VoidLight::TaskPriority::Low,
                        std::format("PathfindingGridBatch_{}", i)
                    );
                    batchFutures.push_back(std::move(batchFuture));
                }

                // Wait for all batches to complete
                for (auto& future : batchFutures) {
                    future.wait();
                }

                // Update coarse grid after all batches complete (sequential, fast operation)
                newGrid->updateCoarseGrid();

                // Atomically swap the grid
                setGrid(newGrid);
                PATHFIND_DEBUG("Parallel grid rebuild: Grid stored atomically");

                if (m_initialized.load(std::memory_order_acquire)) {
                    std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
                    clearOldestCacheEntries(0.5f);
                    calculateOptimalCacheSettings();
                    prewarmPathCache();
                    PATHFIND_INFO(std::format("Grid rebuilt successfully (parallel, {} batches)", batchCount));
                }
            } catch (const std::exception& e) {
                PATHFIND_ERROR(std::format("Parallel grid rebuild failed: {}", e.what()));
            }
        },
        VoidLight::TaskPriority::Low,
        "PathfindingGridRebuild_Parallel"
    );

    // Store future for synchronization during state transitions
    {
        std::lock_guard<std::mutex> lock(m_gridRebuildFuturesMutex);
        m_gridRebuildFutures.push_back(std::move(rebuildFuture));
    }

    PATHFIND_DEBUG("Parallel grid rebuild submitted");
}

void PathfinderManager::addTemporaryWeightField(const Vector2D& center, float radius, float weight) {
    if (!ensureGridInitialized()) {
        return;
    }

    if (auto grid = getGridSnapshot()) {
        grid->addWeightCircle(center, radius, weight);
    }
}

void PathfinderManager::clearWeightFields() {
    if (!ensureGridInitialized()) {
        return;
    }

    // Publish a fresh grid with weights reset instead of mutating the live
    // one in place. In-flight findPath() calls on worker threads hold their
    // own gridSnapshot shared_ptr (captured at request time) and keep
    // reading the old grid safely until they finish -- there is nothing to
    // wait for, since nobody mutates the instance they're holding.
    if (auto grid = getGridSnapshot()) {
        setGrid(grid->cloneWithResetWeights(1.0f));
    }
}

void PathfinderManager::setAllowDiagonal(bool allow) {
    m_allowDiagonal = allow;
    auto grid = getGridSnapshot();
    if (grid) {
        grid->setAllowDiagonal(allow);
    }
}

void PathfinderManager::setMaxIterations(int maxIterations) {
    m_maxIterations = std::max(100, maxIterations);
    auto grid = getGridSnapshot();
    if (grid) {
        grid->setMaxIterations(m_maxIterations);
    }
}

void PathfinderManager::setMaxPathsPerFrame(int maxPaths) {
    m_maxRequestsPerUpdate = std::max(static_cast<size_t>(1), static_cast<size_t>(maxPaths));
}

void PathfinderManager::setCacheExpirationTime(float seconds) {
    m_cacheExpirationTime = std::max(1.0f, seconds);
}

PathfinderManager::PathfinderStats PathfinderManager::getStats() const {
    PathfinderStats stats{};
    
    // Manager-level statistics
    stats.totalRequests = m_enqueuedRequests.load(std::memory_order_relaxed);
    stats.queueSize = 0; // No queue - direct ThreadSystem processing
    stats.queueCapacity = 0; // No queue - direct ThreadSystem processing  
    stats.processorActive = true; // ThreadSystem based processing
    
    // Real statistics from pathfinding processing
    stats.completedRequests = m_completedRequests.load(std::memory_order_relaxed);
    stats.failedRequests = m_failedRequests.load(std::memory_order_relaxed);
    
    // Calculate average processing time and requests per second
    uint64_t const totalRequests = stats.completedRequests + stats.failedRequests;
    if (totalRequests > 0) {
        double totalTimeMs = m_totalProcessingTimeMs.load(std::memory_order_relaxed);
        stats.averageProcessingTimeMs = totalTimeMs / totalRequests;
    } else {
        stats.averageProcessingTimeMs = 0.0;
    }
    
    // Calculate requests per second using time since last stats update
    auto now = std::chrono::steady_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastStatsUpdate);
    if (timeDiff.count() > 1000) { // Update RPS every second
        double secondsSinceLastUpdate = timeDiff.count() / 1000.0;
        uint64_t currentTotalRequests = m_enqueuedRequests.load(std::memory_order_relaxed);

        if (m_lastStatsUpdate != std::chrono::steady_clock::time_point{}) {
            // Calculate RPS based on request DELTA since last update (not total)
            // Handle counter resets: if currentTotal < lastTotal, counters were reset
            int64_t deltaRequests = 0;
            if (currentTotalRequests >= m_lastTotalRequests) {
                deltaRequests = static_cast<int64_t>(currentTotalRequests - m_lastTotalRequests);
            } else {
                // Counter was reset, use current value as delta
                deltaRequests = static_cast<int64_t>(currentTotalRequests);
            }
            m_lastRequestsPerSecond = (deltaRequests > 0) ? (deltaRequests / secondsSinceLastUpdate) : 0.0;
        }

        m_lastTotalRequests = currentTotalRequests;
        m_lastStatsUpdate = now;
    }
    stats.requestsPerSecond = m_lastRequestsPerSecond;
    
    // Cache statistics
    stats.cacheHits = m_cacheHits.load(std::memory_order_relaxed);
    stats.cacheMisses = m_cacheMisses.load(std::memory_order_relaxed);

    // Cache size statistics
    {
        std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
        stats.cacheSize = m_pathCache.size();
        stats.segmentCacheSize = 0; // Segment cache removed for performance
    }

    // Calculate approximate memory usage
    size_t gridMemory = 0;
    auto currentGrid = getGridSnapshot();
    if (currentGrid) {
        // Approximate grid memory: width * height * sizeof(cell data)
        gridMemory = currentGrid->getWidth() * currentGrid->getHeight() * 8; // ~8 bytes per cell
    }
    
    // Cache memory usage (approximate)
    size_t cacheMemory = stats.cacheSize * (sizeof(PathCacheEntry) + 50) + 
                        0; // Segment cache removed for performance
    
    stats.memoryUsageKB = (gridMemory + cacheMemory) / 1024.0;
    
    // Calculate cache hit rates
    uint64_t const totalCacheChecks = stats.cacheHits + stats.cacheMisses;
    if (totalCacheChecks > 0) {
        stats.cacheHitRate = static_cast<float>(stats.cacheHits) / static_cast<float>(totalCacheChecks);
        stats.totalHitRate = stats.cacheHitRate;
    } else {
        stats.cacheHitRate = 0.0f;
        stats.totalHitRate = 0.0f;
    }
    
    return stats;
}

void PathfinderManager::resetStats() {
    m_enqueuedRequests.store(0, std::memory_order_relaxed);
    m_enqueueFailures.store(0, std::memory_order_relaxed);
    m_completedRequests.store(0, std::memory_order_relaxed);
    m_failedRequests.store(0, std::memory_order_relaxed);
    m_cacheHits.store(0, std::memory_order_relaxed);
    m_cacheMisses.store(0, std::memory_order_relaxed);
    m_processedCount.store(0, std::memory_order_relaxed);
    m_lastRequestsPerSecond = 0.0;
    m_lastTotalRequests = 0;
    
    // Fast cache clear - unordered_map::clear() is O(1) for small caches
    {
        std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
        m_pathCache.clear(); // Fast operation for cache entries
    }
    
    // No queue statistics to reset
}

Vector2D PathfinderManager::clampToWorldBounds(const Vector2D& position, float margin) const {
    // PERFORMANCE FIX: Use cached bounds instead of calling WorldManager every time
    // Cache the bounds during grid initialization and use fallback if not available

    // Use cached bounds from grid initialization if available
    auto currentGrid = getGridSnapshot();
    if (currentGrid) {
        // Get bounds from the pathfinding grid which are cached
        const float gridCellSize = 64.0f; // Match m_cellSize
        const float worldWidth = currentGrid->getWidth() * gridCellSize;
        const float worldHeight = currentGrid->getHeight() * gridCellSize;

        const float safeMarginX = std::clamp(margin, 0.0f, worldWidth * 0.5f);
        const float safeMarginY = std::clamp(margin, 0.0f, worldHeight * 0.5f);
        const float minX = safeMarginX;
        const float minY = safeMarginY;
        const float maxX = worldWidth - safeMarginX;
        const float maxY = worldHeight - safeMarginY;

        Vector2D result(
            std::clamp(position.getX(), minX, maxX),
            std::clamp(position.getY(), minY, maxY)
        );

        return result;
    }

    // No grid available - world not loaded yet, return position as-is (valid fallback)
    return position;
}

// Grid-passing overload - avoids getGridSnapshot() in hot path
Vector2D PathfinderManager::clampToWorldBounds(const Vector2D& position, float margin,
                                               const std::shared_ptr<VoidLight::PathfindingGrid>& grid) const {
    if (grid) {
        const float gridCellSize = 64.0f;
        const float worldWidth = grid->getWidth() * gridCellSize;
        const float worldHeight = grid->getHeight() * gridCellSize;
        const float safeMarginX = std::clamp(margin, 0.0f, worldWidth * 0.5f);
        const float safeMarginY = std::clamp(margin, 0.0f, worldHeight * 0.5f);
        const float minX = safeMarginX;
        const float minY = safeMarginY;
        const float maxX = worldWidth - safeMarginX;
        const float maxY = worldHeight - safeMarginY;
        return Vector2D(
            std::clamp(position.getX(), minX, maxX),
            std::clamp(position.getY(), minY, maxY)
        );
    }
    return position;
}

bool PathfinderManager::getCachedWorldBounds(float& outWidth, float& outHeight) const {
    auto grid = getGridSnapshot();
    if (grid) {
        const float gridCellSize = 64.0f;
        outWidth = grid->getWidth() * gridCellSize;
        outHeight = grid->getHeight() * gridCellSize;
        return true;
    }
    // No grid available - world not loaded yet
    return false;
}

Vector2D PathfinderManager::clampInsideExtents(const Vector2D& position, float halfW, float halfH, float extraMargin) const {
    auto grid = getGridSnapshot();
    if (grid) {
        const float gridCellSize = 64.0f;
        const float worldWidth = grid->getWidth() * gridCellSize;
        const float worldHeight = grid->getHeight() * gridCellSize;
        float minX = halfW + extraMargin;
        float minY = halfH + extraMargin;
        float maxX = worldWidth - halfW - extraMargin;
        float maxY = worldHeight - halfH - extraMargin;
        if (maxX < minX) {
            minX = worldWidth * 0.5f;
            maxX = minX;
        }
        if (maxY < minY) {
            minY = worldHeight * 0.5f;
            maxY = minY;
        }
        return Vector2D(
            std::clamp(position.getX(), minX, maxX),
            std::clamp(position.getY(), minY, maxY)
        );
    }
    // No grid available - world not loaded yet, return position as-is
    return position;
}

Vector2D PathfinderManager::adjustSpawnToNavigable(const Vector2D& desired, float halfW, float halfH, float interiorMargin) const {
    Vector2D pos = clampInsideExtents(desired, halfW, halfH, interiorMargin);
    auto grid = getGridSnapshot();
    if (grid) {
        // Snap within ~2 cells
        Vector2D snapped = grid->snapToNearestOpenWorld(pos, grid->getCellSize() * 2.0f);
        // Prefer interior if snapped remains blocked (defensive)
        if (!grid->isWorldBlocked(snapped)) return snapped;
    }
    // Fallback: pull to center a bit
    const auto &wm = WorldManager::Instance();
    float minX=0, minY=0, maxX=0, maxY=0;
    if (wm.getWorldBounds(minX, minY, maxX, maxY)) {
        Vector2D const center((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
        Vector2D dir = center - pos;
        if (dir.length() > 0.001f) {
            dir.normalize();
            return clampInsideExtents(pos + dir * 256.0f, halfW, halfH, interiorMargin);
        }
    }
    return pos;
}

Vector2D PathfinderManager::adjustSpawnToNavigableInRect(const Vector2D& desired,
                                                         float halfW, float halfH,
                                                         float interiorMargin,
                                                         float minX, float minY,
                                                         float maxX, float maxY) const {
    // Clamp area by extents + interior margin
    float aminX = minX + halfW + interiorMargin;
    float aminY = minY + halfH + interiorMargin;
    float amaxX = maxX - halfW - interiorMargin;
    float amaxY = maxY - halfH - interiorMargin;
    if (amaxX < aminX) {
        aminX = (minX + maxX) * 0.5f;
        amaxX = aminX;
    }
    if (amaxY < aminY) {
        aminY = (minY + maxY) * 0.5f;
        amaxY = aminY;
    }

    Vector2D pos = clampInsideExtents(desired, halfW, halfH, interiorMargin);
    // Clamp to area rect
    pos.setX(std::clamp(pos.getX(), aminX, amaxX));
    pos.setY(std::clamp(pos.getY(), aminY, amaxY));

    if (auto grid = getGridSnapshot()) {
        // Try snap within area (rings of ~cell size)
        float cell = grid->getCellSize();
        for (int r = 0; r <= 2; ++r) {
            float const rad = (r+1) * cell;
            for (int i = 0; i < 16; ++i) {
                float ang = static_cast<float>(i) * (static_cast<float>(M_PI) * 2.0f / 16.0f);
                Vector2D cand = Vector2D(pos.getX() + std::cos(ang) * rad,
                                         pos.getY() + std::sin(ang) * rad);
                // Keep inside area
                cand.setX(std::clamp(cand.getX(), aminX, amaxX));
                cand.setY(std::clamp(cand.getY(), aminY, amaxY));
                if (!grid->isWorldBlocked(cand)) return cand;
            }
        }
    }
    return pos;
}

Vector2D PathfinderManager::adjustSpawnToNavigableInCircle(const Vector2D& desired,
                                                           float halfW, float halfH,
                                                           float interiorMargin,
                                                           const Vector2D& center,
                                                           float radius) const {
    float effectiveR = std::max(0.0f, radius - std::max(halfW, halfH) - interiorMargin);
    Vector2D pos = clampInsideExtents(desired, halfW, halfH, interiorMargin);
    Vector2D to = pos - center;
    float const d = to.length();
    if (d > effectiveR && d > 0.001f) {
        to = to * (effectiveR / d);
        pos = center + to;
    }
    if (auto grid = getGridSnapshot()) {
        float cell = grid->getCellSize();
        for (int r = 0; r <= 2; ++r) {
            float const rad = (r+1) * cell;
            for (int i = 0; i < 16; ++i) {
                float ang = static_cast<float>(i) * (static_cast<float>(M_PI) * 2.0f / 16.0f);
                Vector2D cand = Vector2D(pos.getX() + std::cos(ang) * rad,
                                         pos.getY() + std::sin(ang) * rad);
                // Project back to circle if outside
                Vector2D tc = cand - center;
                float const cd = tc.length();
                if (cd > effectiveR && cd > 0.001f) {
                    tc = tc * (effectiveR / cd);
                    cand = center + tc;
                }
                if (!grid->isWorldBlocked(cand)) return cand;
            }
        }
    }
    return pos;
}

void PathfinderManager::normalizeEndpoints(Vector2D& start, Vector2D& goal) const {
    // Clamp to world bounds with interior margin (1.5x cell size = 96px keeps pathfinding
    // away from exact world edges where grid cells may be invalid or blocked)
    constexpr float EDGE_MARGIN = 96.0f;
    start = clampToWorldBounds(start, EDGE_MARGIN);
    goal = clampToWorldBounds(goal, EDGE_MARGIN);

    // Snap to nearest open cells if grid available
    if (auto grid = getGridSnapshot()) {
        float r = grid->getCellSize() * 2.0f;
        start = grid->snapToNearestOpenWorld(start, r);
        goal = grid->snapToNearestOpenWorld(goal, r);
    }

    // Quantize to improve cache hits - use dynamic quantization scaled to world size
    start = Vector2D(std::round(start.getX() / m_endpointQuantization) * m_endpointQuantization,
                     std::round(start.getY() / m_endpointQuantization) * m_endpointQuantization);
    goal = Vector2D(std::round(goal.getX() / m_endpointQuantization) * m_endpointQuantization,
                    std::round(goal.getY() / m_endpointQuantization) * m_endpointQuantization);

    // Re-clamp after quantization since rounding can push coordinates beyond margin
    // in edge cases where quantization grid doesn't align with world bounds.
    // Example: position at (worldWidth-64) within margin=96, quantization=128
    // rounds to either (worldWidth-128) or (worldWidth), latter exceeds bounds.
    // Cost: ~20 cycles per path request, prevents rare INVALID_GOAL errors.
    start = clampToWorldBounds(start, EDGE_MARGIN);
    goal = clampToWorldBounds(goal, EDGE_MARGIN);
}

// Grid-passing overload - avoids getGridSnapshot() calls in hot path
void PathfinderManager::normalizeEndpoints(Vector2D& start, Vector2D& goal,
                                           const std::shared_ptr<VoidLight::PathfindingGrid>& grid) const {
    constexpr float EDGE_MARGIN = 96.0f;
    start = clampToWorldBounds(start, EDGE_MARGIN, grid);
    goal = clampToWorldBounds(goal, EDGE_MARGIN, grid);

    if (grid) {
        float r = grid->getCellSize() * 2.0f;
        start = grid->snapToNearestOpenWorld(start, r);
        goal = grid->snapToNearestOpenWorld(goal, r);
    }

    start = Vector2D(std::round(start.getX() / m_endpointQuantization) * m_endpointQuantization,
                     std::round(start.getY() / m_endpointQuantization) * m_endpointQuantization);
    goal = Vector2D(std::round(goal.getX() / m_endpointQuantization) * m_endpointQuantization,
                    std::round(goal.getY() / m_endpointQuantization) * m_endpointQuantization);

    start = clampToWorldBounds(start, EDGE_MARGIN, grid);
    goal = clampToWorldBounds(goal, EDGE_MARGIN, grid);
}

bool PathfinderManager::followPathStep(const EntityPtr& entity, const Vector2D& currentPos,
                                     std::vector<Vector2D>& path, size_t& pathIndex,
                                     float speed, float nodeRadius) const {
    if (!entity || path.empty() || pathIndex >= path.size()) {
        return false;
    }
    
    Vector2D targetNode = path[pathIndex];
    Vector2D toNode = targetNode - currentPos;
    const float radius2 = nodeRadius * nodeRadius;
    float dist2 = toNode.dot(toNode);

    if (dist2 <= radius2) {
        // Reached current node, advance to next
        pathIndex++;
        if (pathIndex >= path.size()) {
            // Path complete
            path.clear();
            pathIndex = 0;
            return false;
        }
        // Continue to next node
        targetNode = path[pathIndex];
        toNode = targetNode - currentPos;
        dist2 = toNode.dot(toNode);
    }

    if (dist2 > 0.01f) { // equivalent to previous 0.1f threshold squared
        // Move toward current path node
        float invLen = 1.0f / std::sqrt(dist2);
        Vector2D const direction = toNode * invLen; // normalized
        entity->setVelocity(direction * speed);
        return true;
    }

    return false;
}

void PathfinderManager::reportStatistics() const {
    auto stats = getStats();

    PATHFIND_INFO_IF(stats.totalRequests > 0,
        std::format("PathfinderManager Status - Total Requests: {}, Completed: {}, Failed: {}, "
                    "Cache Hits: {}, Cache Misses: {}, Hit Rate: {}%, Cache Size: {}, Avg Time: {}ms, "
                    "RPS: {}, Memory: {} KB, ThreadSystem: {}",
                    stats.totalRequests, stats.completedRequests, stats.failedRequests,
                    stats.cacheHits, stats.cacheMisses, static_cast<int>(stats.cacheHitRate * 100),
                    stats.cacheSize, stats.averageProcessingTimeMs, static_cast<int>(stats.requestsPerSecond),
                    stats.memoryUsageKB, (stats.processorActive ? "Active" : "Inactive")));
    PATHFIND_INFO_IF(stats.totalRequests > 0,
        std::format("Pathfinder Summary - RPS: {:.1f}, Cache Hit: {:.0f}%, Cache Size: {}",
                    stats.requestsPerSecond, stats.cacheHitRate * 100.0f,
                    stats.cacheSize));

    // Reset per-cycle counters for next reporting window (every 600 frames / 10 seconds)
    m_enqueuedRequests.store(0, std::memory_order_relaxed);
    m_completedRequests.store(0, std::memory_order_relaxed);
    m_failedRequests.store(0, std::memory_order_relaxed);
    m_cacheHits.store(0, std::memory_order_relaxed);
    m_cacheMisses.store(0, std::memory_order_relaxed);
    m_totalProcessingTimeMs.store(0.0, std::memory_order_relaxed);
}

bool PathfinderManager::ensureGridInitialized() {
    // Check if grid exists - no fallback
    return getGridSnapshot() != nullptr;
}

uint64_t PathfinderManager::computeStableCacheKey(const Vector2D& start, const Vector2D& goal) const {
    // OPTIMIZATION: Stable cache key that does NOT depend on obstacle layout
    // This allows pre-warmed sector paths to be hit by nearby NPC requests
    //
    // Key insight: normalizeEndpoints() calls snapToNearestOpenWorld() which changes
    // coordinates based on which cells are blocked. This made pre-warmed paths
    // (from sector centers) unmatchable by runtime requests (from NPC positions).
    //
    // Solution: Quantize BEFORE any snapping, using only clamped raw coordinates.
    // Two requests within the same quantization bucket will get the same cache key,
    // even if they snap to different open cells during actual pathfinding.
    constexpr float EDGE_MARGIN = 96.0f;

    // Clamp to world bounds (same as normalizeEndpoints step 1)
    Vector2D const nStart = clampToWorldBounds(start, EDGE_MARGIN);
    Vector2D const nGoal = clampToWorldBounds(goal, EDGE_MARGIN);

    // Quantize directly - NO snapping to open cells (makes key stable)
    int const sx = static_cast<int>(nStart.getX() / m_cacheKeyQuantization);
    int const sy = static_cast<int>(nStart.getY() / m_cacheKeyQuantization);
    int const gx = static_cast<int>(nGoal.getX() / m_cacheKeyQuantization);
    int const gy = static_cast<int>(nGoal.getY() / m_cacheKeyQuantization);

    // Pack into 64-bit key: sx(16) | sy(16) | gx(16) | gy(16)
    return (static_cast<uint64_t>(sx & 0xFFFF) << 48) |
           (static_cast<uint64_t>(sy & 0xFFFF) << 32) |
           (static_cast<uint64_t>(gx & 0xFFFF) << 16) |
           static_cast<uint64_t>(gy & 0xFFFF);
}

void PathfinderManager::evictOldestCacheEntry() {
    if (m_pathCache.empty()) return;

    auto oldest = std::min_element(m_pathCache.begin(), m_pathCache.end(),
                                   [](const auto& a, const auto& b) {
                                       return a.second.lastUsed < b.second.lastUsed;
                                   });
    m_pathCache.erase(oldest);
}

void PathfinderManager::clearOldestCacheEntries(float percentage) {
    if (m_pathCache.empty() || percentage <= 0.0f) return;

    // Clamp percentage to [0, 1]
    percentage = std::min(1.0f, std::max(0.0f, percentage));

    size_t numToRemove = static_cast<size_t>(m_pathCache.size() * percentage);
    if (numToRemove == 0) return;

    // Build vector of entries sorted by lastUsed (oldest first)
    std::vector<std::pair<uint64_t, std::chrono::steady_clock::time_point>> entries;
    entries.reserve(m_pathCache.size());

    for (const auto& [key, entry] : m_pathCache) {
        entries.emplace_back(key, entry.lastUsed);
    }

    // Partial sort to find the oldest N entries (faster than full sort)
    std::partial_sort(entries.begin(), entries.begin() + numToRemove, entries.end(),
                     [](const auto& a, const auto& b) {
                         return a.second < b.second; // Oldest first
                     });

    // Remove the oldest entries
    for (size_t i = 0; i < numToRemove; ++i) {
        m_pathCache.erase(entries[i].first);
    }

    PATHFIND_DEBUG(std::format("Cleared {} oldest cache entries ({}%)",
                   numToRemove, static_cast<int>(percentage * 100)));
}

void PathfinderManager::clearAllCache() {
    std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
    size_t clearedCount = m_pathCache.size();
    m_pathCache.clear();
    PATHFIND_INFO(std::format("Cleared all cache entries: {} paths removed", clearedCount));
}

void PathfinderManager::calculateOptimalCacheSettings() {
    auto currentGrid = getGridSnapshot();
    if (!currentGrid) {
        PATHFIND_WARN("Cannot calculate cache settings - no grid available");
        return;
    }

    float worldW = currentGrid->getWidth() * m_cellSize;
    float worldH = currentGrid->getHeight() * m_cellSize;
    float diagonal = std::sqrt(worldW * worldW + worldH * worldH);

    // ADAPTIVE QUANTIZATION: Single unified quantization for correctness

    // Endpoint quantization: Conservative scaling for ACCURACY (minimize path failures)
    // 0.5% of world size with strict 256px cap to keep quantization fine-grained
    // This prevents entities from snapping to blocked cells
    m_endpointQuantization = std::clamp(worldW / 200.0f, 128.0f, 256.0f);

    // ADAPTIVE THRESHOLDS
    m_hierarchicalThreshold = diagonal * 0.05f;      // 5% of world diagonal
    m_connectivityThreshold = worldW * 0.25f;        // 25% of world width

    // ADAPTIVE PRE-WARMING (8-connected sector graph)
    // Small worlds (< 16K): 4×4 sectors = 56 paths
    // Medium worlds (32K): 8×8 sectors = 210 paths
    // Large worlds (64K+): 16×16 sectors = 930 paths
    if (worldW < 16000.0f) {
        m_prewarmSectorCount = 4;
    } else if (worldW < 48000.0f) {
        m_prewarmSectorCount = 8;
    } else {
        m_prewarmSectorCount = 16;
    }
    // Calculate actual path count for 8-connected grid: 2×N×(N-1) + 2×(N-1)²
    // For N=8: 2×8×7 + 2×7² = 112 + 98 = 210 paths
    int const N = m_prewarmSectorCount;
    m_prewarmPathCount = 2 * N * (N - 1) + 2 * (N - 1) * (N - 1);

    // Cache key quantization based on sector size for higher reuse
    // With RAW coord cache key (computed before normalization), coarser quantization is safe
    // because paths are stored/retrieved by bucket, and the actual path follows normalized coords.
    // Use full sector size to maximize cache hits for NPCs within the same sector.
    float const sectorSize = worldW / static_cast<float>(m_prewarmSectorCount);
    m_cacheKeyQuantization = sectorSize;

    // Calculate expected cache bucket count for logging
    int const bucketsX = static_cast<int>(worldW / m_cacheKeyQuantization);
    int const bucketsY = static_cast<int>(worldH / m_cacheKeyQuantization);
    int const totalBuckets = bucketsX * bucketsY;
    float cacheEfficiency = (static_cast<float>(MAX_CACHE_ENTRIES) / static_cast<float>(totalBuckets)) * 100.0f;

    PATHFIND_INFO(std::format("Auto-tuned cache settings for {}×{}px world:",
                  static_cast<int>(worldW), static_cast<int>(worldH)));
    PATHFIND_INFO(std::format("  Endpoint quantization: {}px ({}% world)",
                  static_cast<int>(m_endpointQuantization),
                  static_cast<int>((m_endpointQuantization / worldW) * 100.0f * 10.0f) / 10.0f));
    PATHFIND_INFO(std::format("  Cache key quantization: {}px",
                  static_cast<int>(m_cacheKeyQuantization)));
    PATHFIND_INFO(std::format("  Expected cache buckets: {}×{} = {} total",
                  bucketsX, bucketsY, totalBuckets));
    PATHFIND_INFO(std::format("  Cache efficiency: {}% coverage",
                  static_cast<int>(cacheEfficiency)));
    PATHFIND_INFO(std::format("  Hierarchical threshold: {}px",
                  static_cast<int>(m_hierarchicalThreshold)));
    PATHFIND_INFO(std::format("  Pre-warm sectors: {}×{} = {} paths",
                  m_prewarmSectorCount, m_prewarmSectorCount, m_prewarmPathCount));
}

void PathfinderManager::prewarmPathCache() {
    // Use compare_exchange to ensure only one thread executes pre-warming
    bool expected = false;
    auto currentGrid = getGridSnapshot();
    if (!m_prewarming.compare_exchange_strong(expected, true) || !currentGrid) {
        return;
    }

    float worldW = currentGrid->getWidth() * m_cellSize;
    float worldH = currentGrid->getHeight() * m_cellSize;
    int const sectors = m_prewarmSectorCount;
    float const sectorW = worldW / static_cast<float>(sectors);
    float const sectorH = worldH / static_cast<float>(sectors);

    PATHFIND_INFO(std::format("Pre-warming cache with {} sector-based paths (world: {}×{}px, sectors: {}×{})...",
                  m_prewarmPathCount, static_cast<int>(worldW), static_cast<int>(worldH),
                  sectors, sectors));

    // Generate paths between sector centers
    std::vector<std::pair<Vector2D, Vector2D>> seedPaths;
    seedPaths.reserve(m_prewarmPathCount);

    for (int sy = 0; sy < sectors; sy++) {
        for (int sx = 0; sx < sectors; sx++) {
            Vector2D const sectorCenter(
                (static_cast<float>(sx) + 0.5f) * sectorW,
                (static_cast<float>(sy) + 0.5f) * sectorH
            );

            // Connect to adjacent and diagonal sectors (8-connectivity)
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // Skip self

                    int const nx = sx + dx;
                    int const ny = sy + dy;

                    // Only connect to valid neighbors within bounds
                    if (nx >= 0 && nx < sectors && ny >= 0 && ny < sectors) {
                        // Only add each connection once (avoid duplicates by only connecting forward)
                        if (ny > sy || (ny == sy && nx > sx)) {
                            Vector2D const neighborCenter(
                                (static_cast<float>(nx) + 0.5f) * sectorW,
                                (static_cast<float>(ny) + 0.5f) * sectorH
                            );
                            seedPaths.emplace_back(sectorCenter, neighborCenter);
                        }
                    }
                }
            }
        }
    }

    // Submit pre-warming paths with Low priority (background processing)
    // These will be distributed automatically by ThreadSystem
    int pathCount = 0;
    for (const auto& [start, goal] : seedPaths) {
        // Use Low priority and no callback for pre-warming
        requestPath(0, start, goal, Priority::Low, nullptr);
        pathCount++;
    }

    PATHFIND_INFO(std::format("Submitted {} pre-warming paths to background ThreadSystem", pathCount));

    m_prewarming.store(false);
}

void PathfinderManager::subscribeToEvents() {
    // GameEngine::init() awaits EventManager::init() before PathfinderManager::init()
    // runs, so this gate should never bail during normal startup. It remains as
    // defense-in-depth (e.g. standalone/test init without that ordering): if it
    // ever trips, registration is skipped permanently with no retry.
    if (!EventManager::Instance().isInitialized()) {
        PATHFIND_WARN("EventManager not initialized, delaying event subscription");
        return;
    }
    
    try {
        auto& eventMgr = EventManager::Instance();
        
        // Subscribe to collision obstacle changed events (persistent — manager-level)
        auto token = eventMgr.registerPersistentHandlerWithToken(EventTypeId::CollisionObstacleChanged,
            [this](const EventData& data) {
                if (data.isActive() && data.event) {
                    auto obstacleEvent = std::dynamic_pointer_cast<CollisionObstacleChangedEvent>(data.event);
                    if (obstacleEvent) {
                        onCollisionObstacleChanged(obstacleEvent->getPosition(), 
                                                  obstacleEvent->getRadius(),
                                                  obstacleEvent->getDescription());
                    }
                }
            });
        
        m_eventHandlerTokens.push_back(token);
        PATHFIND_DEBUG("PathfinderManager subscribed to CollisionObstacleChanged events");

        // Subscribe to world events (StaticCollidersReady, WorldUnloaded, TileChanged)
        // NOTE: Grid rebuild waits for StaticCollidersReadyEvent (not WorldLoadedEvent)
        // to ensure collision data is available when pathfinding queries it
        auto worldToken = eventMgr.registerPersistentHandlerWithToken(EventTypeId::World,
            [this](const EventData& data) {
                auto baseEvent = data.event;
                if (!baseEvent) return;

                // Handle StaticCollidersReadyEvent - collision data is ready, rebuild grid
                if (auto collidersEvent = std::dynamic_pointer_cast<StaticCollidersReadyEvent>(baseEvent)) {
                    PATHFIND_INFO(std::format("Static colliders ready ({} solid, {} triggers) - rebuilding pathfinding grid",
                                  collidersEvent->getSolidBodyCount(), collidersEvent->getTriggerCount()));
                    onStaticCollidersReady();
                    return;
                }

                // Handle WorldUnloadedEvent
                if (std::dynamic_pointer_cast<WorldUnloadedEvent>(baseEvent)) {
                    onWorldUnloaded();
                    return;
                }

                // Handle TileChangedEvent
                if (auto tileEvent = std::dynamic_pointer_cast<TileChangedEvent>(baseEvent)) {
                    onTileChanged(tileEvent->getX(), tileEvent->getY());
                    return;
                }
            });

        m_eventHandlerTokens.push_back(worldToken);
        PATHFIND_DEBUG("PathfinderManager subscribed to World events (StaticCollidersReady, WorldUnloaded, TileChanged)");

    } catch (const std::exception& e) {
        PATHFIND_ERROR(std::format("Failed to subscribe to events: {}", e.what()));
    }
}

void PathfinderManager::unsubscribeFromEvents() {
    if (!EventManager::Instance().isInitialized()) {
        return;
    }
    
    try {
        auto& eventMgr = EventManager::Instance();
        for (const auto& token : m_eventHandlerTokens) {
            eventMgr.removeHandler(token);
        }
        m_eventHandlerTokens.clear();
        PATHFIND_DEBUG("PathfinderManager unsubscribed from all events");
        
    } catch (const std::exception& e) {
        PATHFIND_ERROR(std::format("Failed to unsubscribe from events: {}", e.what()));
    }
}

void PathfinderManager::onCollisionObstacleChanged(const Vector2D& position, float radius, const std::string& description) {
    // Increment collision version to trigger cache invalidation
    m_lastCollisionVersion.fetch_add(1, std::memory_order_release);
    
    // Selective cache invalidation: remove paths that pass through the affected area
    {
        std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
        
        size_t removedCount = 0;
        for (auto it = m_pathCache.begin(); it != m_pathCache.end(); ) {
            bool pathIntersectsArea = false;
            
            // Check if any point in the cached path is within the affected radius
            for (const auto& pathPoint : it->second.path) {
                float distance2 = (pathPoint - position).dot(pathPoint - position);
                if (distance2 <= (radius * radius)) {
                    pathIntersectsArea = true;
                    break;
                }
            }
            
            if (pathIntersectsArea) {
                it = m_pathCache.erase(it);
                removedCount++;
            } else {
                ++it;
            }
        }
        
        PATHFIND_DEBUG_IF(removedCount > 0,
            std::format("Invalidated {} cached paths due to obstacle change: {}",
                        removedCount, description));
    }

    // Mark dirty region on pathfinding grid for incremental update
    auto currentGrid = getGridSnapshot();
    if (currentGrid) {
        // Convert world position to grid cell coordinates
        int const gridX = static_cast<int>((position.getX()) / m_cellSize);
        int const gridY = static_cast<int>((position.getY()) / m_cellSize);
        int gridRadius = static_cast<int>(std::ceil(radius / m_cellSize)) + 1; // +1 for safety margin

        // Mark circular dirty region
        for (int dy = -gridRadius; dy <= gridRadius; ++dy) {
            for (int dx = -gridRadius; dx <= gridRadius; ++dx) {
                float const distSq = dx * dx + dy * dy;
                if (distSq <= gridRadius * gridRadius) {
                    currentGrid->markDirtyRegion(gridX + dx, gridY + dy, 1, 1);
                }
            }
        }
    }
}

void PathfinderManager::onStaticCollidersReady() {
    const auto& worldManager = WorldManager::Instance();
    int worldWidth = 0;
    int worldHeight = 0;

    worldManager.withWorldDataRead([&](const VoidLight::WorldData* worldData) {
        if (!worldData) {
            PATHFIND_WARN("StaticCollidersReady received but no world data available");
            return;
        }

        worldHeight = static_cast<int>(worldData->grid.size());
        worldWidth = worldHeight > 0 ? static_cast<int>(worldData->grid[0].size()) : 0;
    });

    if (worldWidth <= 0 || worldHeight <= 0) {
        return;
    }

    PATHFIND_INFO(std::format("Static colliders ready - rebuilding pathfinding grid (world: {}x{})",
                  worldWidth, worldHeight));

    clearAllCache();
    rebuildGrid(false); // allowIncremental=false for world loads
    PATHFIND_INFO("Pathfinding grid rebuild initiated (async)");
}

void PathfinderManager::onWorldUnloaded() {
    PATHFIND_INFO("Responding to WorldUnloadedEvent");

    // Cache and pending requests already cleared by prepareForStateTransition()
    // This event handler serves as confirmation that world cleanup completed
}

void PathfinderManager::onTileChanged(int x, int y) {
    // Convert tile coordinates to world position using global tile size constant
    Vector2D const tileWorldPos(x * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f,
                          y * VoidLight::TILE_SIZE + VoidLight::TILE_SIZE * 0.5f);

    // Invalidate paths that pass through or near the changed tile
    // Use slightly larger radius than tile size to catch paths that pass nearby
    constexpr float INVALIDATION_RADIUS = VoidLight::TILE_SIZE * 1.5f;

    std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
    size_t removedCount = 0;

    for (auto it = m_pathCache.begin(); it != m_pathCache.end(); ) {
        bool pathIntersectsTile = false;

        // Check if any point in the cached path is within the tile's influence radius
        for (const auto& pathPoint : it->second.path) {
            float distance2 = (pathPoint - tileWorldPos).dot(pathPoint - tileWorldPos);
            if (distance2 <= (INVALIDATION_RADIUS * INVALIDATION_RADIUS)) {
                pathIntersectsTile = true;
                break;
            }
        }

        if (pathIntersectsTile) {
            it = m_pathCache.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    PATHFIND_DEBUG_IF(removedCount > 0,
        std::format("Tile changed at ({}, {}), invalidated {} cached paths",
                    x, y, removedCount));

    // Mark dirty region on pathfinding grid for incremental update
    auto tileGrid = getGridSnapshot();
    if (tileGrid) {
        // Convert tile coordinates to grid cell coordinates
        // Tile coordinates are in world tiles, grid cells may be different size
        int gridX = static_cast<int>((x * VoidLight::TILE_SIZE) / m_cellSize);
        int gridY = static_cast<int>((y * VoidLight::TILE_SIZE) / m_cellSize);

        // Mark single cell dirty (tile changes typically affect one cell)
        tileGrid->markDirtyRegion(gridX, gridY, 1, 1);
        PATHFIND_DEBUG(std::format("Marked dirty region for tile change at grid ({},{})",
                      gridX, gridY));
    }
}

void PathfinderManager::waitForGridRebuildCompletion() {
    // Swap out futures into the reusable member buffer to avoid holding the lock
    // during the wait while preserving capacity across rebuilds (mirrors AIManager pattern).
    // waitForGridRebuildCompletion runs on the main thread during state transitions, so
    // m_reusableGridRebuildFutures is not accessed concurrently outside the lock.
    {
        std::lock_guard<std::mutex> lock(m_gridRebuildFuturesMutex);
        m_reusableGridRebuildFutures.clear();
        m_reusableGridRebuildFutures.swap(m_gridRebuildFutures);
        // m_gridRebuildFutures now retains the reusable buffer's capacity, so new
        // tasks can be added concurrently without reallocating.
    }

    if (!m_reusableGridRebuildFutures.empty()) {
        PATHFIND_INFO(std::format("Waiting for {} grid rebuild task(s) to complete before state transition...",
                      m_reusableGridRebuildFutures.size()));

        for (auto& future : m_reusableGridRebuildFutures) {
            if (future.valid()) {
                try {
                    future.wait(); // Block until task completes
                    PATHFIND_DEBUG("Grid rebuild task completed");
                } catch (const std::exception& e) {
                    PATHFIND_ERROR(std::format("Exception waiting for grid rebuild: {}", e.what()));
                }
            }
        }

        PATHFIND_INFO("Grid rebuild synchronization complete - safe to proceed with state transition");
    }
}

// End of file - event-driven pathfinding with collision and world integration
