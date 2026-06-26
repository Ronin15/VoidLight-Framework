/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PATHFINDING_GRID_HPP
#define PATHFINDING_GRID_HPP

#include <vector>
#include <utility>
#include <cstdint>
#include <ostream>
#include <queue>
#include <limits>
#include <algorithm>
#include <memory>
#include <mutex>
#include "utils/Vector2D.hpp"

namespace VoidLight {

enum class PathfindingResult : uint8_t { SUCCESS, NO_PATH_FOUND, INVALID_START, INVALID_GOAL, TIMEOUT };

// Stream operator for PathfindingResult to support test output
inline std::ostream& operator<<(std::ostream& os, const PathfindingResult& result) {
    switch (result) {
        case PathfindingResult::SUCCESS: return os << "SUCCESS";
        case PathfindingResult::NO_PATH_FOUND: return os << "NO_PATH_FOUND";
        case PathfindingResult::INVALID_START: return os << "INVALID_START";
        case PathfindingResult::INVALID_GOAL: return os << "INVALID_GOAL";
        case PathfindingResult::TIMEOUT: return os << "TIMEOUT";
        default: return os << "UNKNOWN";
    }
}

class PathfindingGrid {
public:
    PathfindingGrid(int width, int height, float cellSize, const Vector2D& worldOffset, bool createCoarseGrid = true);

    void rebuildFromWorld();                 // pull from WorldManager::grid (full rebuild)
    void rebuildFromWorld(int rowStart, int rowEnd); // rebuild specific row range (for parallel batching)
    void initializeArrays();                 // initialize grid arrays without processing (for parallel batching)
    void updateCoarseGrid();                 // update hierarchical coarse grid (call after parallel batch rebuild)

    // Incremental update support
    void markDirtyRegion(int cellX, int cellY, int width = 1, int height = 1); // mark region as needing rebuild
    void rebuildDirtyRegions();              // rebuild only dirty regions (incremental update)
    bool hasDirtyRegions() const;            // check if any dirty regions exist
    float calculateDirtyPercent() const;     // calculate percentage of grid that is dirty
    void clearDirtyRegions();                // clear dirty region tracking

    PathfindingResult findPath(const Vector2D& start, const Vector2D& goal,
                               std::vector<Vector2D>& outPath);
    
    // Hierarchical pathfinding for long distances (10x speedup)
    PathfindingResult findPathHierarchical(const Vector2D& start, const Vector2D& goal,
                                          std::vector<Vector2D>& outPath);
                                          
    // Decision function for choosing between direct and hierarchical pathfinding
    bool shouldUseHierarchicalPathfinding(const Vector2D& start, const Vector2D& goal) const;

    void setAllowDiagonal(bool allow) { m_allowDiagonal = allow; }
    void setMaxIterations(int maxIters) { m_maxIterations = maxIters; }
    void setCosts(float straight, float diagonal) { m_costStraight = straight; m_costDiagonal = diagonal; }

    // Dynamic weighting for avoidance fields
    void resetWeights(float defaultWeight = 1.0f);
    void addWeightCircle(const Vector2D& worldCenter, float worldRadius, float weightMultiplier);

    // Hierarchical grid access
    float getCellSize() const { return m_cell; }
    int getWidth() const { return m_w; }
    int getHeight() const { return m_h; }
    Vector2D getWorldOffset() const { return m_offset; }
    
    // Grid data access for hierarchical pathfinding
    void setBlocked(int gx, int gy, bool blocked);
    void setWeight(int gx, int gy, float weight);

    // World-space convenience helpers
    Vector2D snapToNearestOpenWorld(const Vector2D& pos, float maxWorldRadius) const;
    bool isWorldBlocked(const Vector2D& pos) const;
    
    // Statistics
    struct PathfindingStats {
        uint64_t totalRequests{0};
        uint64_t successfulPaths{0}; 
        uint64_t timeouts{0};
        uint64_t invalidStarts{0};
        uint64_t invalidGoals{0};
        uint64_t totalIterations{0};
        uint64_t totalPathLength{0};
        uint32_t avgPathLength{0};
        uint32_t framesSinceReset{0};
    };
    
    void resetStats() { m_stats = PathfindingStats{}; }
    const PathfindingStats& getStats() const { return m_stats; }

private:
    int m_w, m_h; float m_cell; Vector2D m_offset;
    std::vector<uint8_t> m_blocked; // 0 walkable, 1 blocked
    std::vector<float> m_weight;    // movement multipliers per cell

    // Incremental update support (dirty region tracking)
    struct DirtyRegion {
        int x, y;           // Grid cell coordinates
        int width, height;  // Size in grid cells
    };
    std::vector<DirtyRegion> m_dirtyRegions;
    mutable std::mutex m_dirtyRegionMutex; // Thread-safe dirty region access

    // Hierarchical pathfinding support (4x coarser grid for long distances)
    std::unique_ptr<PathfindingGrid> m_coarseGrid;
    static constexpr float COARSE_GRID_MULTIPLIER = 4.0f;
    static constexpr float HIERARCHICAL_DISTANCE_THRESHOLD = 256.0f; // Lowered for large worlds (4 tiles @ 64px)

    bool m_allowDiagonal{true};
    int m_maxIterations{12000}; // Performance-tuned: increased from 8K to 12K for better success rate
    float m_costStraight{1.0f};
    float m_costDiagonal{1.41421356f};

    PathfindingStats m_stats{};

    bool isBlocked(int gx, int gy) const;
    bool inBounds(int gx, int gy) const;
    std::pair<int,int> worldToGrid(const Vector2D& w) const;
    Vector2D gridToWorld(int gx, int gy) const;

    // Helper: find nearest unblocked cell within maxRadius (grid units)
    bool findNearestOpen(int gx, int gy, int maxRadius, int& outGX, int& outGY) const;
    
    // Path smoothing functions
    void smoothPath(std::vector<Vector2D>& path);
    bool hasLineOfSight(const Vector2D& start, const Vector2D& end) const;
    
    // Hierarchical pathfinding helpers
    void initializeCoarseGrid();
    PathfindingResult refineCoarsePath(const std::vector<Vector2D>& coarsePath,
                                     const Vector2D& start, const Vector2D& goal,
                                     std::vector<Vector2D>& outPath);

private:
    // Object pools for memory optimization
    struct NodePool {
        struct Node { int x; int y; float f; };
        struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };
        
        // Pre-allocated containers to avoid repeated allocation/deallocation
        std::priority_queue<Node, std::vector<Node>, Cmp> openQueue;
        std::vector<float> gScoreBuffer;
        std::vector<float> fScoreBuffer;
        std::vector<int> parentBuffer;
        std::vector<uint8_t> closedBuffer; // moved from local to pooled to avoid per-call allocations
        std::vector<Vector2D> pathBuffer;
        
        void ensureCapacity(int gridSize) {
            if (gScoreBuffer.size() < static_cast<size_t>(gridSize)) {
                gScoreBuffer.resize(gridSize);
                fScoreBuffer.resize(gridSize);
                parentBuffer.resize(gridSize);
                closedBuffer.resize(gridSize);
                pathBuffer.reserve(std::max(128, gridSize / 10)); // Reasonable path length estimate
            }
        }
        
        void reset() {
            // Clear but don't deallocate
            while (!openQueue.empty()) openQueue.pop();
            // Only reset if buffers are properly sized
            if (!gScoreBuffer.empty()) {
                std::fill(gScoreBuffer.begin(), gScoreBuffer.end(), std::numeric_limits<float>::max());
                std::fill(fScoreBuffer.begin(), fScoreBuffer.end(), std::numeric_limits<float>::max());
                std::fill(parentBuffer.begin(), parentBuffer.end(), -1);
                std::fill(closedBuffer.begin(), closedBuffer.end(), 0);
            }
            pathBuffer.clear();
        }
    };
    
    // NodePool will be thread_local within findPath function
};

} // namespace VoidLight

#endif // PATHFINDING_GRID_HPP
