/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/pathfinding/PathfindingGrid.hpp"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/WorldManager.hpp"
#include "world/WorldData.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <numeric>
#include <queue>
#include <stdexcept>

namespace VoidLight {

// NodePool will be thread_local within findPath function

PathfindingGrid::PathfindingGrid(int width, int height, float cellSize,
                                 const Vector2D &worldOffset,
                                 bool createCoarseGrid)
    : m_w(width), m_h(height), m_cell(cellSize), m_offset(worldOffset) {

  // Validate grid dimensions to prevent 0x0 grids
  if (m_w <= 0 || m_h <= 0) {
    throw std::invalid_argument(std::format(
        "PathfindingGrid dimensions must be positive: {}x{}", width, height));
  }

  if (cellSize <= 0.0f) {
    throw std::invalid_argument(std::format(
        "PathfindingGrid cell size must be positive: {}", cellSize));
  }

  m_blocked.assign(static_cast<size_t>(m_w * m_h), 0);
  m_weight.assign(static_cast<size_t>(m_w * m_h), 1.0f);

  // Initialize coarse grid for hierarchical pathfinding (4x larger cells)
  if (createCoarseGrid) {
    initializeCoarseGrid();
  }
}

bool PathfindingGrid::inBounds(int gx, int gy) const {
  return gx >= 0 && gy >= 0 && gx < m_w && gy < m_h;
}

bool PathfindingGrid::isBlocked(int gx, int gy) const {
  if (!inBounds(gx, gy))
    return true;
  return m_blocked[static_cast<size_t>(gy * m_w + gx)] != 0;
}

std::pair<int, int> PathfindingGrid::worldToGrid(const Vector2D &w) const {
  int gx = static_cast<int>(std::floor((w.getX() - m_offset.getX()) / m_cell));
  int gy = static_cast<int>(std::floor((w.getY() - m_offset.getY()) / m_cell));
  return {gx, gy};
}

Vector2D PathfindingGrid::gridToWorld(int gx, int gy) const {
  float const wx = m_offset.getX() + gx * m_cell + m_cell * 0.5f;
  float const wy = m_offset.getY() + gy * m_cell + m_cell * 0.5f;
  return Vector2D(wx, wy);
}

bool PathfindingGrid::findNearestOpen(int gx, int gy, int maxRadius, int &outGX,
                                      int &outGY) const {
  if (inBounds(gx, gy) && !isBlocked(gx, gy)) {
    outGX = gx;
    outGY = gy;
    return true;
  }
  for (int r = 1; r <= maxRadius; ++r) {
    for (int dy = -r; dy <= r; ++dy) {
      int const y = gy + dy;
      int const x1 = gx - r;
      int const x2 = gx + r;
      if (inBounds(x1, y) && !isBlocked(x1, y)) {
        outGX = x1;
        outGY = y;
        return true;
      }
      if (inBounds(x2, y) && !isBlocked(x2, y)) {
        outGX = x2;
        outGY = y;
        return true;
      }
    }
    for (int dx = -r + 1; dx <= r - 1; ++dx) {
      int const x = gx + dx;
      int const y1 = gy - r;
      int const y2 = gy + r;
      if (inBounds(x, y1) && !isBlocked(x, y1)) {
        outGX = x;
        outGY = y1;
        return true;
      }
      if (inBounds(x, y2) && !isBlocked(x, y2)) {
        outGX = x;
        outGY = y2;
        return true;
      }
    }
  }
  return false;
}

Vector2D PathfindingGrid::snapToNearestOpenWorld(const Vector2D &pos,
                                                 float maxWorldRadius) const {
  auto [gx, gy] = worldToGrid(pos);
  int maxR = std::max(1, static_cast<int>(std::ceil(maxWorldRadius / m_cell)));
  int outGX = gx, outGY = gy;
  if (findNearestOpen(gx, gy, maxR, outGX, outGY)) {
    return gridToWorld(outGX, outGY);
  }
  return pos;
}

bool PathfindingGrid::isWorldBlocked(const Vector2D &pos) const {
  auto [gx, gy] = worldToGrid(pos);
  return isBlocked(gx, gy);
}

void PathfindingGrid::rebuildFromWorld() { rebuildFromWorld(0, m_h); }

void PathfindingGrid::initializeArrays() {
  const int cellsW = m_w;
  const int cellsH = m_h;
  if (cellsW <= 0 || cellsH <= 0) {
    PATHFIND_WARN("initializeArrays(): invalid grid dims");
    return;
  }
  m_blocked.assign(static_cast<size_t>(cellsW * cellsH), 0);
  m_weight.assign(static_cast<size_t>(cellsW * cellsH), 1.0f);
}

void PathfindingGrid::rebuildFromWorld(int rowStart, int rowEnd) {
  const WorldManager &wm = WorldManager::Instance();
  wm.withWorldDataRead([&](const VoidLight::WorldData *world) {
    if (!world) {
      PATHFIND_WARN("rebuildFromWorld(): no active world");
      return;
    }

    const int cellsW = m_w;
    const int cellsH = m_h;
    if (cellsW <= 0 || cellsH <= 0) {
      PATHFIND_WARN("rebuildFromWorld(): invalid grid dims");
      return;
    }

    rowStart = std::clamp(rowStart, 0, cellsH);
    rowEnd = std::clamp(rowEnd, 0, cellsH);
    if (rowStart >= rowEnd) {
      PATHFIND_WARN("rebuildFromWorld(): invalid row range");
      return;
    }

    const bool isFullRebuild = (rowStart == 0 && rowEnd == cellsH);
    if (isFullRebuild) {
      m_blocked.assign(static_cast<size_t>(cellsW * cellsH), 0);
      m_weight.assign(static_cast<size_t>(cellsW * cellsH), 1.0f);
    }

    const int tilesH = static_cast<int>(world->grid.size());
    const int tilesW = tilesH > 0 ? static_cast<int>(world->grid[0].size()) : 0;
    if (tilesW <= 0 || tilesH <= 0) {
      PATHFIND_WARN("rebuildFromWorld(): world has no tiles");
      return;
    }

    constexpr float tileSize = VoidLight::TILE_SIZE;
    int blockedCount = 0;
    int collisionBlockedCount = 0;

    for (int cy = rowStart; cy < rowEnd; ++cy) {
      if (!VoidLight::ThreadSystem::Exists() ||
          VoidLight::ThreadSystem::Instance().isShutdown()) {
        PATHFIND_DEBUG("Grid rebuild interrupted by ThreadSystem shutdown");
        return;
      }

      for (int cx = 0; cx < cellsW; ++cx) {
        float const x0 = m_offset.getX() + cx * m_cell;
        float const y0 = m_offset.getY() + cy * m_cell;
        float const x1 = x0 + m_cell;
        float const y1 = y0 + m_cell;

        int tx0 = static_cast<int>(std::floor(x0 / tileSize));
        int ty0 = static_cast<int>(std::floor(y0 / tileSize));
        int tx1 = static_cast<int>(std::floor((x1 - 1.0f) / tileSize));
        int ty1 = static_cast<int>(std::floor((y1 - 1.0f) / tileSize));

        tx0 = std::clamp(tx0, 0, tilesW - 1);
        ty0 = std::clamp(ty0, 0, tilesH - 1);
        tx1 = std::clamp(tx1, 0, tilesW - 1);
        ty1 = std::clamp(ty1, 0, tilesH - 1);

        int totalTiles = 0;
        int blockedTiles = 0;
        float weightSum = 0.0f;

        for (int ty = ty0; ty <= ty1; ++ty) {
          for (int tx = tx0; tx <= tx1; ++tx) {
            const auto &tile = world->grid[ty][tx];
            bool blocked = tile.obstacleType == ObstacleType::BUILDING ||
                           tile.biome == Biome::MOUNTAIN;
            if (blocked)
              blockedTiles++;

            float tw = 1.0f;
            if (tile.isWater) {
              tw = 2.0f;
            } else if (tile.obstacleType == ObstacleType::ROCK ||
                       tile.obstacleType == ObstacleType::TREE) {
              tw = 2.5f;
            }
            weightSum += tw;
            totalTiles++;
          }
        }

        bool cellBlocked =
            (totalTiles > 0) &&
            (static_cast<float>(blockedTiles) / static_cast<float>(totalTiles) >
             0.50f);
        float const cellWeight =
            (totalTiles > 0) ? (weightSum / static_cast<float>(totalTiles))
                             : 1.0f;

        if (!cellBlocked && CollisionManager::Instance().isInitialized()) {
          const float ENTITY_CLEARANCE = 28.0f;
          AABB const cellAABB(x0 + m_cell * 0.5f, y0 + m_cell * 0.5f,
                              m_cell * 0.5f + ENTITY_CLEARANCE,
                              m_cell * 0.5f + ENTITY_CLEARANCE);
          if (CollisionManager::Instance().queryAreaHasStaticOverlap(cellAABB)) {
            cellBlocked = true;
            collisionBlockedCount++;
          }
        }

        size_t cidx = static_cast<size_t>(cy * cellsW + cx);
        m_blocked[cidx] = cellBlocked ? 1 : 0;
        if (cellBlocked)
          ++blockedCount;
        m_weight[cidx] = cellWeight;
      }
    }

    if (isFullRebuild) {
      PATHFIND_INFO(std::format("Grid rebuilt (sampled): {}x{}, blocked={}/{} "
                                "({}% blocked), collision-blocked={} cells",
                                cellsW, cellsH, blockedCount, cellsW * cellsH,
                                (100.0f * blockedCount) / (cellsW * cellsH),
                                collisionBlockedCount));

      if (m_coarseGrid) {
        updateCoarseGrid();

        int coarseW = m_coarseGrid->getWidth();
        int coarseH = m_coarseGrid->getHeight();
        int coarseBlockedCount = 0;
        for (int cy = 0; cy < coarseH; ++cy) {
          for (int cx = 0; cx < coarseW; ++cx) {
            if (m_coarseGrid->isBlocked(cx, cy)) {
              coarseBlockedCount++;
            }
          }
        }
        float const coarseBlockedPercent =
            (coarseW * coarseH > 0)
                ? (100.0f * coarseBlockedCount) / (coarseW * coarseH)
                : 0.0f;

        PATHFIND_DEBUG(std::format(
            "Coarse grid updated: {}x{}, blocked={}/{} ({}% blocked)",
            coarseW, coarseH, coarseBlockedCount, coarseW * coarseH,
            coarseBlockedPercent));
      }
    }
  });
}

// Incremental Update Implementation

void PathfindingGrid::markDirtyRegion(int cellX, int cellY, int width,
                                      int height) {
  std::lock_guard<std::mutex> lock(m_dirtyRegionMutex);

  // Clamp to grid bounds
  cellX = std::clamp(cellX, 0, m_w - 1);
  cellY = std::clamp(cellY, 0, m_h - 1);
  width = std::clamp(width, 1, m_w - cellX);
  height = std::clamp(height, 1, m_h - cellY);

  // Check for overlaps with existing dirty regions and merge if possible
  for (auto &region : m_dirtyRegions) {
    // Check if new region overlaps or is adjacent to existing region
    int r1x1 = cellX, r1y1 = cellY, r1x2 = cellX + width, r1y2 = cellY + height;
    int r2x1 = region.x, r2y1 = region.y, r2x2 = region.x + region.width,
        r2y2 = region.y + region.height;

    // Expand bounds by 1 to merge adjacent regions
    if (!(r1x2 < r2x1 - 1 || r1x1 > r2x2 + 1 || r1y2 < r2y1 - 1 ||
          r1y1 > r2y2 + 1)) {
      // Merge: compute bounding box of both regions
      int mergedX1 = std::min(r1x1, r2x1);
      int mergedY1 = std::min(r1y1, r2y1);
      int mergedX2 = std::max(r1x2, r2x2);
      int mergedY2 = std::max(r1y2, r2y2);

      region.x = mergedX1;
      region.y = mergedY1;
      region.width = mergedX2 - mergedX1;
      region.height = mergedY2 - mergedY1;
      return; // Merged, no need to add new region
    }
  }

  // No overlap found, add as new dirty region
  m_dirtyRegions.push_back({cellX, cellY, width, height});

  // Limit dirty regions to prevent unbounded growth (merge if too many)
  constexpr size_t MAX_DIRTY_REGIONS = 32;
  if (m_dirtyRegions.size() > MAX_DIRTY_REGIONS) {
    // Find two closest regions and merge them
    // Simple heuristic: merge first two regions (could be improved)
    // Note: size() > 32 guarantees size() >= 2, so no additional check needed
    auto &r1 = m_dirtyRegions[0];
    auto &r2 = m_dirtyRegions[1];

    int mergedX1 = std::min(r1.x, r2.x);
    int mergedY1 = std::min(r1.y, r2.y);
    int mergedX2 = std::max(r1.x + r1.width, r2.x + r2.width);
    int mergedY2 = std::max(r1.y + r1.height, r2.y + r2.height);

    r1.x = mergedX1;
    r1.y = mergedY1;
    r1.width = mergedX2 - mergedX1;
    r1.height = mergedY2 - mergedY1;

    // Remove second region
    m_dirtyRegions.erase(m_dirtyRegions.begin() + 1);
  }
}

void PathfindingGrid::rebuildDirtyRegions() {
  std::vector<DirtyRegion> regions;
  {
    std::lock_guard<std::mutex> lock(m_dirtyRegionMutex);
    if (m_dirtyRegions.empty()) {
      return; // Nothing to rebuild
    }
    regions.swap(m_dirtyRegions); // Move regions out for processing
  }

  // Rebuild each dirty region using the row-range rebuild
  for (const auto &region : regions) {
    int rowStart = region.y;
    int rowEnd = std::min(region.y + region.height, m_h);

    // Rebuild rows in this region
    rebuildFromWorld(rowStart, rowEnd);
  }

  // Update coarse grid after all dirty regions rebuilt
  if (m_coarseGrid) {
    updateCoarseGrid();
  }

  PATHFIND_DEBUG(
      std::format("Incremental rebuild complete: {} dirty regions processed",
                  regions.size()));
}

bool PathfindingGrid::hasDirtyRegions() const {
  std::lock_guard<std::mutex> lock(m_dirtyRegionMutex);
  return !m_dirtyRegions.empty();
}

float PathfindingGrid::calculateDirtyPercent() const {
  std::lock_guard<std::mutex> lock(m_dirtyRegionMutex);
  if (m_dirtyRegions.empty() || m_w <= 0 || m_h <= 0) {
    return 0.0f;
  }

  // Calculate total dirty cells (with overlap handling)
  int totalDirtyCells =
      std::accumulate(m_dirtyRegions.begin(), m_dirtyRegions.end(), 0,
                      [](int sum, const auto &region) {
                        return sum + region.width * region.height;
                      });

  // Simple approximation (may overcount overlaps, but conservative)
  int const totalCells = m_w * m_h;
  return std::min(100.0f,
                  (static_cast<float>(totalDirtyCells) / totalCells) * 100.0f);
}

void PathfindingGrid::clearDirtyRegions() {
  std::lock_guard<std::mutex> lock(m_dirtyRegionMutex);
  m_dirtyRegions.clear();
}

void PathfindingGrid::smoothPath(std::vector<Vector2D> &path) {
  if (path.size() <= 2)
    return; // Can't smooth paths with 2 or fewer nodes

  // thread_local buffer to avoid per-path allocation (thread-safe for
  // multi-threaded pathfinding)
  thread_local std::vector<Vector2D> smoothed;
  smoothed.clear();
  if (smoothed.capacity() < path.size()) {
    smoothed.reserve(path.size());
  }
  smoothed.push_back(path[0]); // Always keep start

  size_t i = 0;
  while (i < path.size() - 1) {
    // Look ahead for line-of-sight optimization
    size_t farthest = i + 1;
    for (size_t j = i + 2; j < path.size(); j++) {
      if (hasLineOfSight(path[i], path[j])) {
        farthest = j;
      } else {
        break; // Blocked, stop looking ahead
      }
    }

    // Add the farthest reachable point
    if (farthest != i + 1) {
      smoothed.push_back(path[farthest]);
      i = farthest;
    } else {
      smoothed.push_back(path[i + 1]);
      i++;
    }
  }

  // Always keep goal - check if coordinates are different
  if (smoothed.empty() || smoothed.back().getX() != path.back().getX() ||
      smoothed.back().getY() != path.back().getY()) {
    smoothed.push_back(path.back());
  }

  path = std::move(smoothed);
}

bool PathfindingGrid::hasLineOfSight(const Vector2D &start,
                                     const Vector2D &end) const {
  auto [sx, sy] = worldToGrid(start);
  auto [ex, ey] = worldToGrid(end);

  // Simple Bresenham-like line check
  int dx = abs(ex - sx), dy = abs(ey - sy);
  int x = sx, y = sy;
  int xStep = (ex > sx) ? 1 : -1;
  int yStep = (ey > sy) ? 1 : -1;

  if (dx > dy) {
    int err = dx / 2;
    while (x != ex) {
      if (!inBounds(x, y) || isBlocked(x, y))
        return false;
      err -= dy;
      if (err < 0) {
        y += yStep;
        err += dx;
      }
      x += xStep;
    }
  } else {
    int err = dy / 2;
    while (y != ey) {
      if (!inBounds(x, y) || isBlocked(x, y))
        return false;
      err -= dx;
      if (err < 0) {
        x += xStep;
        err += dy;
      }
      y += yStep;
    }
  }

  return !isBlocked(ex, ey); // Check final position
}

PathfindingResult PathfindingGrid::findPath(const Vector2D &start,
                                            const Vector2D &goal,
                                            std::vector<Vector2D> &outPath) {
  outPath.clear();
  auto [sx_raw, sy_raw] = worldToGrid(start);
  auto [gx_raw, gy_raw] = worldToGrid(goal);

  // Validate original grid indices before any clamping
  if (!inBounds(sx_raw, sy_raw)) {
    PATHFIND_ERROR(std::format("findPath: INVALID_START - grid coords ({},{}) "
                               "out of bounds (0,0) to ({},{})",
                               sx_raw, sy_raw, m_w - 1, m_h - 1));
    VOIDLIGHT_STATS_ONLY(
      m_stats.totalRequests++;
      m_stats.invalidStarts++;
    )
    // Invalid start warnings removed - covered in PathfinderManager status
    // reporting
    return PathfindingResult::INVALID_START;
  }
  if (!inBounds(gx_raw, gy_raw)) {
    PATHFIND_ERROR(std::format("findPath: INVALID_GOAL - grid coords ({},{}) "
                               "out of bounds (0,0) to ({},{})",
                               gx_raw, gy_raw, m_w - 1, m_h - 1));
    VOIDLIGHT_STATS_ONLY(
      m_stats.totalRequests++;
      m_stats.invalidGoals++;
    )
    // Invalid goal warnings removed - covered in PathfinderManager status
    // reporting
    return PathfindingResult::INVALID_GOAL;
  }

  // Start from validated indices and then clamp to keep away from exact edges
  int sx = sx_raw;
  int sy = sy_raw;
  int gx = gx_raw;
  int gy = gy_raw;

  // Clamp grid coordinates to ensure they're away from exact boundaries
  // This prevents problematic boundary pathfinding even if world-space clamping
  // fails
  const int GRID_MARGIN =
      2; // Reduced margin; avoid over-restricting valid goals near edges
  int const xLow = std::min(GRID_MARGIN, std::max(0, m_w - 1 - GRID_MARGIN));
  int const xHigh = std::max(GRID_MARGIN, std::max(0, m_w - 1 - GRID_MARGIN));
  int const yLow = std::min(GRID_MARGIN, std::max(0, m_h - 1 - GRID_MARGIN));
  int const yHigh = std::max(GRID_MARGIN, std::max(0, m_h - 1 - GRID_MARGIN));
  gx = std::clamp(gx, xLow, xHigh);
  gy = std::clamp(gy, yLow, yHigh);
  sx = std::clamp(sx, xLow, xHigh);
  sy = std::clamp(sy, yLow, yHigh);

  // Enhanced goal validation: try a small nudge but do not early-return
  // Let the later broader nudge (radius 20) handle difficult endpoints
  if (isBlocked(gx, gy)) {
    int nearGx = gx, nearGy = gy;
    if (findNearestOpen(gx, gy, 8, nearGx, nearGy)) {
      gx = nearGx;
      gy = nearGy;
    }
    // else: keep as-is; broader nudge below will attempt again
  }

  // Quick reachability test - if goal is in a completely isolated area, reject
  // early
  int dxGoal = std::abs(sx - gx), dyGoal = std::abs(sy - gy);
  int directDistance = std::max(dxGoal, dyGoal);

  // For very long distances, do a lightweight connectivity check (less
  // restrictive) Use dynamic threshold scaled to world size (defaults to 800 if
  // manager unavailable)
  float connectivityThresholdCells = 800.0f;
  try {
    // Get dynamic threshold in pixels, convert to grid cells
    connectivityThresholdCells =
        PathfinderManager::Instance().getConnectivityThreshold() / m_cell;
  } catch (...) {
    // Fallback to static threshold if PathfinderManager not available
  }

  if (directDistance > static_cast<int>(connectivityThresholdCells)) {
    // Sample a few points along the direct line to check for major barriers
    int samples = std::min(8, directDistance / 8);
    int blockedSamples = 0;

    for (int i = 1; i < samples; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(samples);
      int const mx = sx + static_cast<int>((gx - sx) * t);
      int const my = sy + static_cast<int>((gy - sy) * t);

      // Check if this sample point has any open neighbors (basic connectivity)
      bool hasOpenNeighbor = false;
      for (int dx = -1; dx <= 1 && !hasOpenNeighbor; dx++) {
        for (int dy = -1; dy <= 1 && !hasOpenNeighbor; dy++) {
          int nx = mx + dx, ny = my + dy;
          if (inBounds(nx, ny) && !isBlocked(nx, ny)) {
            hasOpenNeighbor = true;
          }
        }
      }

      if (!hasOpenNeighbor)
        blockedSamples++;
    }

    // If more than 90% of samples lack open neighbors, it's probably
    // unreachable, but do not early-return: allow A* to attempt within
    // iteration limits.
    if (blockedSamples > (samples * 9) / 10) {
      PATHFIND_DEBUG(
          "Connectivity check: likely unreachable, proceeding conservatively");
    }
  }
  // Nudge start/goal if blocked (common after collision resolution)
  int nsx = sx, nsy = sy, ngx = gx, ngy = gy;
  // Adaptive nudge with increasing radii for large/congested worlds
  // Try progressively larger radii: 48 -> 96 -> 128 for start, 64 -> 96 -> 128
  // for goal
  bool const startOk = !isBlocked(sx, sy) ||
                       findNearestOpen(sx, sy, 48, nsx, nsy) ||
                       findNearestOpen(sx, sy, 96, nsx, nsy) ||
                       findNearestOpen(sx, sy, 128, nsx, nsy);
  bool const goalOk = !isBlocked(gx, gy) ||
                      findNearestOpen(gx, gy, 64, ngx, ngy) ||
                      findNearestOpen(gx, gy, 96, ngx, ngy) ||
                      findNearestOpen(gx, gy, 128, ngx, ngy);
  if (!startOk || !goalOk) {
    PATHFIND_DEBUG("findPath(): start or goal blocked after adaptive nudge");
    return PathfindingResult::NO_PATH_FOUND;
  }
  sx = nsx;
  sy = nsy;
  gx = ngx;
  gy = ngy;

  // Early unreachability detection using coarse grid (for large worlds)
  if (m_coarseGrid && directDistance > 512) {
    // Quick connectivity test on coarse grid to fail fast for unreachable paths
    // Coarse grid already configured with reduced iteration budget (see
    // initializeCoarseGrid)
    std::vector<Vector2D> coarsePath;
    coarsePath.reserve(directDistance / 4 +
                       10); // Reserve estimated path length
    auto coarseResult = m_coarseGrid->findPath(gridToWorld(sx, sy),
                                               gridToWorld(gx, gy), coarsePath);

    // Only fail early if coarse grid definitively finds NO_PATH_FOUND (not
    // TIMEOUT) TIMEOUT means needs more exploration, so continue to fine-grid
    // pathfinding
    if (coarseResult == PathfindingResult::NO_PATH_FOUND) {
      PATHFIND_DEBUG("Early detection: coarse grid indicates unreachable path "
                     "(disconnected regions)");
      VOIDLIGHT_STATS_ONLY(m_stats.totalRequests++;)
      return PathfindingResult::NO_PATH_FOUND;
    }
  }

  // Early success: if start equals goal after nudging
  if (sx == gx && sy == gy) {
    outPath.push_back(gridToWorld(gx, gy));
    VOIDLIGHT_STATS_ONLY(
      m_stats.totalRequests++;
      m_stats.successfulPaths++;
      m_stats.totalIterations += 1; // Minimal iteration count
      m_stats.totalPathLength += outPath.size();
      m_stats.avgPathLength =
          static_cast<uint32_t>(m_stats.totalPathLength / m_stats.successfulPaths);
    )
    return PathfindingResult::SUCCESS;
  }

  // Line-of-sight shortcut: Check direct path for simple terrain
  Vector2D worldStart = gridToWorld(sx, sy);
  Vector2D worldGoal = gridToWorld(gx, gy);
  if (hasLineOfSight(worldStart, worldGoal)) {
    // Direct path available - create simple 2-point path
    outPath.push_back(worldStart);
    outPath.push_back(worldGoal);
    VOIDLIGHT_STATS_ONLY(
      m_stats.totalRequests++;
      m_stats.successfulPaths++;
      m_stats.totalIterations += 2; // Minimal iteration count for line-of-sight
      m_stats.totalPathLength += outPath.size();
      m_stats.avgPathLength =
          static_cast<uint32_t>(m_stats.totalPathLength / m_stats.successfulPaths);
    )
    return PathfindingResult::SUCCESS;
  }

  const int W = m_w, H = m_h;
  auto idx = [&](int x, int y) { return y * W + x; };
  auto h = [&](int x, int y) {
    // OPTIMIZED HEURISTIC: More focused search with perfect admissible
    // heuristic
    int dx = std::abs(x - gx);
    int dy = std::abs(y - gy);
    int dmin = std::min(dx, dy);
    int dmax = std::max(dx, dy);
    float const baseDistance =
        m_costDiagonal * dmin + m_costStraight * (dmax - dmin);

    // LARGE-WORLD OPTIMIZATION: Slightly weight heuristic for long distances
    // Increases goal-directedness for paths >500 cells while maintaining
    // admissibility This reduces wasted exploration in open areas of large
    // worlds
    if (directDistance > 500) {
      return baseDistance *
             1.001f; // Very conservative weight to maintain optimality
    }
    return baseDistance;
  };

  // Keep baseDistance for iteration calculations
  int const baseDistance = directDistance;

  // Allow full grid access for reliable pathfinding

  // A* pathfinding using thread-local object pooling for memory optimization
  thread_local NodePool nodePool;
  int const gridSize = m_w * m_h;
  nodePool.ensureCapacity(gridSize);
  nodePool.reset();

  // Use pooled containers
  auto &open = nodePool.openQueue;
  auto &gScore = nodePool.gScoreBuffer;
  auto &fScore = nodePool.fScoreBuffer;
  auto &parent = nodePool.parentBuffer;
  auto &closed = nodePool.closedBuffer;

  size_t sIdx = static_cast<size_t>(idx(sx, sy));
  if (sIdx >= gScore.size()) {
    PATHFIND_ERROR(
        std::format("Buffer size mismatch: index {} >= buffer size {}", sIdx,
                    gScore.size()));
    return PathfindingResult::INVALID_START;
  }
  gScore[sIdx] = 0.0f;
  fScore[sIdx] = static_cast<float>(h(sx, sy));
  open.push(NodePool::Node{sx, sy, fScore[sIdx]});

  int iterations = 0;
  const int dirs = m_allowDiagonal ? 8 : 4;
  // Direction tables
  constexpr int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
  constexpr int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};

  // PERFORMANCE TUNING: Tighter iteration budget to cap worst-case CPU
  int baseIters = std::max(4000, baseDistance * 40);
  int dynamicMaxIters = std::min(m_maxIterations, baseIters + 6000);

  // Performance-focused: Reasonable queue limits to balance memory and success
  // rate

  // Memory protection for very complex pathfinding scenarios
  // Increased queue size limit for better success rate with complex paths
  const size_t maxAbsoluteQueueSize = 30000;

  while (!open.empty() && iterations++ < dynamicMaxIters) {
    // Only terminate on truly excessive memory usage, not normal pathfinding
    // complexity
    if (open.size() > maxAbsoluteQueueSize) {
      PATHFIND_DEBUG(std::format("Emergency termination: queue size {} "
                                 "exceeded emergency limit (max: {})",
                                 open.size(), maxAbsoluteQueueSize));
      break;
    }

    // EARLY SUCCESS: Check if we're very close to the goal for quick
    // termination
    if (iterations > 100 && iterations % 500 == 0) {
      NodePool::Node topNode = open.top();
      int distToGoal = std::abs(topNode.x - gx) + std::abs(topNode.y - gy);
      if (distToGoal <= 2) {
        // Very close to goal, increase iteration allowance for final push
        dynamicMaxIters = std::min(m_maxIterations, iterations + 1000);
      }
    }

    NodePool::Node cur = open.top();
    open.pop();

    // Compute and cache current node's linear index once
    const int cIndex = idx(cur.x, cur.y);
    if (closed[cIndex])
      continue;
    closed[cIndex] = 1;
    if (cur.x == gx && cur.y == gy) {
      // reconstruct
      std::vector<Vector2D> rev;
      rev.reserve(directDistance + 20); // Reserve estimated path length
      int cx = cur.x, cy = cur.y;
      while (!(cx == sx && cy == sy)) {
        rev.push_back(gridToWorld(cx, cy));
        int p = parent[static_cast<size_t>(idx(cx, cy))];
        if (p < 0)
          break; // shouldn't happen
        cy = p / W;
        cx = p % W;
      }
      rev.push_back(gridToWorld(sx, sy));
      outPath.assign(rev.rbegin(), rev.rend());

      // Apply path smoothing to reduce unnecessary waypoints
      smoothPath(outPath);

      // Update statistics instead of individual logging
      VOIDLIGHT_STATS_ONLY(
        m_stats.totalRequests++;
        m_stats.successfulPaths++;
        m_stats.totalIterations += iterations;
        m_stats.totalPathLength += outPath.size();
        m_stats.avgPathLength =
            static_cast<uint32_t>(m_stats.totalPathLength / m_stats.successfulPaths);
      )

      return PathfindingResult::SUCCESS;
    }
    // Cache current node gScore for reuse across neighbors
    const float gCur = gScore[static_cast<size_t>(cIndex)];

    for (int i = 0; i < dirs; ++i) {
      const int nx = cur.x + dx8[i];
      const int ny = cur.y + dy8[i];

      // EMERGENCY FIX: Only check bounds, no ROI restrictions
      const bool outsideBounds = (nx < 0 || nx >= W || ny < 0 || ny >= H);

      if (outsideBounds)
        continue; // Only reject actual out-of-bounds cells

      // Compute neighbor's linear index relative to current node to avoid idx()
      // calls
      const int nIndexInt = cIndex + dx8[i] + dy8[i] * W;
      const size_t nIndex = static_cast<size_t>(nIndexInt);
      if (closed[nIndex] || isBlocked(nx, ny))
        continue;

      // No-corner-cutting: if moving diagonally, both orthogonal neighbors must
      // be open
      if (m_allowDiagonal && i >= 4) {
        const int ox = cur.x + dx8[i];
        const int oy = cur.y;
        const int px = cur.x;
        const int py = cur.y + dy8[i];
        if (isBlocked(ox, oy) || isBlocked(px, py))
          continue;
      }

      const float step = (i < 4) ? m_costStraight : m_costDiagonal;
      const float weight = (m_weight[nIndex] > 0.0f) ? m_weight[nIndex] : 1.0f;
      const float tentative = gCur + step * weight;

      // Only add to queue if we found a better path
      if (tentative < gScore[nIndex]) {
        parent[nIndex] = cIndex;
        gScore[nIndex] = tentative;
        fScore[nIndex] = tentative + h(nx, ny);
        open.push(NodePool::Node{nx, ny, fScore[nIndex]});
      }
    }
  }

  // Determine termination reason: exhausted search vs. iteration cap
  bool exhaustedQueue = open.empty();
  bool const hitIterationCap =
      !exhaustedQueue; // loop ended due to iteration count

  VOIDLIGHT_STATS_ONLY(
    m_stats.totalRequests++;
    m_stats.totalIterations += iterations;
  )
  if (hitIterationCap) {
    VOIDLIGHT_STATS_ONLY(m_stats.timeouts++;)
    return PathfindingResult::TIMEOUT;
  }
  // No path found within explored region
  return PathfindingResult::NO_PATH_FOUND;
}

void PathfindingGrid::resetWeights(float defaultWeight) {
  m_weight.assign(static_cast<size_t>(m_w * m_h), defaultWeight);
}

std::shared_ptr<PathfindingGrid>
PathfindingGrid::cloneWithResetWeights(float defaultWeight) const {
  auto clone = std::make_shared<PathfindingGrid>(m_w, m_h, m_cell, m_offset,
                                                 /*createCoarseGrid=*/false);
  clone->m_blocked = m_blocked;
  clone->m_weight.assign(static_cast<size_t>(m_w * m_h), defaultWeight);
  clone->m_allowDiagonal = m_allowDiagonal;
  clone->m_maxIterations = m_maxIterations;
  clone->m_costStraight = m_costStraight;
  clone->m_costDiagonal = m_costDiagonal;

  if (m_coarseGrid) {
    // Coarse grid's own weights are untouched by resetWeights() today too --
    // preserve its blocked/config state as-is, just give it a new identity.
    clone->m_coarseGrid = std::make_unique<PathfindingGrid>(
        m_coarseGrid->m_w, m_coarseGrid->m_h, m_coarseGrid->m_cell,
        m_coarseGrid->m_offset, /*createCoarseGrid=*/false);
    clone->m_coarseGrid->m_blocked = m_coarseGrid->m_blocked;
    clone->m_coarseGrid->m_weight = m_coarseGrid->m_weight;
    clone->m_coarseGrid->m_allowDiagonal = m_coarseGrid->m_allowDiagonal;
    clone->m_coarseGrid->m_maxIterations = m_coarseGrid->m_maxIterations;
    clone->m_coarseGrid->m_costStraight = m_coarseGrid->m_costStraight;
    clone->m_coarseGrid->m_costDiagonal = m_coarseGrid->m_costDiagonal;
  }

  return clone;
}

void PathfindingGrid::addWeightCircle(const Vector2D &worldCenter,
                                      float worldRadius,
                                      float weightMultiplier) {
  if (weightMultiplier <= 1.0f)
    return; // only increase cost
  auto [cx, cy] = worldToGrid(worldCenter);
  int rad = static_cast<int>(std::ceil(worldRadius / m_cell));
  int x0 = std::max(0, cx - rad);
  int y0 = std::max(0, cy - rad);
  int x1 = std::min(m_w - 1, cx + rad);
  int y1 = std::min(m_h - 1, cy + rad);
  float const radCells = worldRadius / m_cell;
  float const r2 = radCells * radCells;
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      float dx = static_cast<float>(x - cx);
      float dy = static_cast<float>(y - cy);
      if (dx * dx + dy * dy <= r2) {
        size_t i = static_cast<size_t>(y * m_w + x);
        if (i < m_weight.size())
          m_weight[i] = std::max(m_weight[i], weightMultiplier);
      }
    }
  }
}

// ===== Hierarchical Pathfinding Implementation =====

void PathfindingGrid::initializeCoarseGrid() {
  // Skip coarse grid initialization if main grid is too small
  if (m_w < static_cast<int>(COARSE_GRID_MULTIPLIER) ||
      m_h < static_cast<int>(COARSE_GRID_MULTIPLIER)) {
    PATHFIND_INFO(std::format("Grid too small for hierarchical pathfinding "
                              "({}x{}), skipping coarse grid",
                              m_w, m_h));
    m_coarseGrid = nullptr;
    return;
  }

  // Create coarse grid with 4x larger cells for long-distance pathfinding
  float const coarseCellSize = m_cell * COARSE_GRID_MULTIPLIER;
  int coarseWidth = std::max(1, static_cast<int>(m_w / COARSE_GRID_MULTIPLIER));
  int coarseHeight =
      std::max(1, static_cast<int>(m_h / COARSE_GRID_MULTIPLIER));

  // Validate coarse grid dimensions
  if (coarseWidth <= 0 || coarseHeight <= 0) {
    PATHFIND_WARN(std::format("Invalid coarse grid dimensions: {}x{}",
                              coarseWidth, coarseHeight));
    m_coarseGrid = nullptr;
    return;
  }

  try {
    m_coarseGrid = std::make_unique<PathfindingGrid>(
        coarseWidth, coarseHeight, coarseCellSize, m_offset, false);
    // Set more aggressive settings for coarse grid (speed over precision)
    // Allow generous iteration budget on coarse grid (still far cheaper than
    // fine)
    m_coarseGrid->setMaxIterations(std::max(1000, m_maxIterations / 2));
    m_coarseGrid->setAllowDiagonal(true); // Always allow diagonal for speed

    PATHFIND_INFO(std::format(
        "Hierarchical coarse grid initialized: {}x{}, cell size: {}",
        coarseWidth, coarseHeight, coarseCellSize));
  } catch (const std::exception &e) {
    PATHFIND_WARN(
        std::format("Failed to initialize coarse grid: {}", e.what()));
    m_coarseGrid = nullptr;
  }
}

void PathfindingGrid::updateCoarseGrid() {
  if (!m_coarseGrid) {
    return;
  }

  // Downsample fine grid to coarse grid (4x4 fine cells -> 1 coarse cell)
  int coarseW = m_coarseGrid->getWidth();
  int coarseH = m_coarseGrid->getHeight();

  for (int cy = 0; cy < coarseH; ++cy) {
    for (int cx = 0; cx < coarseW; ++cx) {
      // Sample 4x4 region in fine grid with improved strategy
      int blockedCount = 0;
      int totalCount = 0;
      float avgWeight = 0.0f;
      int sampleCount = 0;

      int const fineStartX = cx * static_cast<int>(COARSE_GRID_MULTIPLIER);
      int const fineStartY = cy * static_cast<int>(COARSE_GRID_MULTIPLIER);

      // Sample the 4x4 region
      for (int fy = fineStartY;
           fy < fineStartY + static_cast<int>(COARSE_GRID_MULTIPLIER) &&
           fy < m_h;
           ++fy) {
        for (int fx = fineStartX;
             fx < fineStartX + static_cast<int>(COARSE_GRID_MULTIPLIER) &&
             fx < m_w;
             ++fx) {
          if (inBounds(fx, fy)) {
            totalCount++;
            if (isBlocked(fx, fy)) {
              blockedCount++;
            }
            size_t fineIdx = static_cast<size_t>(fy * m_w + fx);
            if (fineIdx < m_weight.size()) {
              avgWeight += m_weight[fineIdx];
              sampleCount++;
            }
          }
        }
      }

      // IMPROVED: Mark coarse cell as blocked if >50% of fine cells are blocked
      // Better reflects dense obstacles and improves refinement reliability
      bool const coarseBlocked =
          (totalCount > 0) &&
          (static_cast<float>(blockedCount) / static_cast<float>(totalCount) >
           0.50f);

      // Update coarse grid cell using safe setter methods
      m_coarseGrid->setBlocked(cx, cy, coarseBlocked);
      if (sampleCount > 0) {
        // Increase weight for areas with some obstacles (but not fully blocked)
        float const blockageRatio = (totalCount > 0)
                                        ? static_cast<float>(blockedCount) /
                                              static_cast<float>(totalCount)
                                        : 0.0f;
        float const adjustedWeight =
            (avgWeight / static_cast<float>(sampleCount)) *
            (1.0f + blockageRatio * 2.0f);
        m_coarseGrid->setWeight(cx, cy, adjustedWeight);
      } else {
        m_coarseGrid->setWeight(cx, cy, 1.0f); // Default weight
      }
    }
  }
}

bool PathfindingGrid::shouldUseHierarchicalPathfinding(
    const Vector2D &start, const Vector2D &goal) const {
  if (!m_coarseGrid) {
    return false; // No coarse grid available
  }

  // Use dynamic threshold from PathfinderManager (scaled to world size)
  // Falls back to static threshold if manager not available
  float threshold = HIERARCHICAL_DISTANCE_THRESHOLD;
  try {
    // Get dynamic threshold calculated for current world size
    threshold = PathfinderManager::Instance().getHierarchicalThreshold();
  } catch (...) {
    // Fallback to static threshold if PathfinderManager not available
  }

  float const distance = (goal - start).length();
  if (distance <= threshold) {
    return false;
  }

  // Edge-aware gating: avoid hierarchical near borders or blocked coarse cells
  // Map start/goal to coarse grid coordinates
  auto sg = m_coarseGrid->worldToGrid(start);
  auto gg = m_coarseGrid->worldToGrid(goal);

  // Coarse grid interior threshold (cells)
  const int edgeBand =
      2; // within 2 coarse cells of border, prefer direct pathfinding

  // Fetch dimensions
  const int cw = m_coarseGrid->m_w;
  const int ch = m_coarseGrid->m_h;

  auto nearBorder = [&](int gx, int gy) {
    return (gx < edgeBand || gy < edgeBand || gx >= (cw - edgeBand) ||
            gy >= (ch - edgeBand));
  };

  // If either start/goal is near border, skip hierarchical
  if (nearBorder(sg.first, sg.second) || nearBorder(gg.first, gg.second)) {
    return false;
  }

  // If either coarse cell is blocked, skip hierarchical
  if (m_coarseGrid->inBounds(sg.first, sg.second) &&
      m_coarseGrid->isBlocked(sg.first, sg.second)) {
    return false;
  }
  if (m_coarseGrid->inBounds(gg.first, gg.second) &&
      m_coarseGrid->isBlocked(gg.first, gg.second)) {
    return false;
  }

  return true;
}

PathfindingResult
PathfindingGrid::findPathHierarchical(const Vector2D &start,
                                      const Vector2D &goal,
                                      std::vector<Vector2D> &outPath) {
  if (!m_coarseGrid) {
    // Fallback to regular pathfinding
    return findPath(start, goal, outPath);
  }

  // Note: Coarse grid is updated in rebuildFromWorld() and should be current

  // Step 1: Find coarse path on low-resolution grid
  std::vector<Vector2D> coarsePath;
  auto [sx, sy] = worldToGrid(start);
  auto [gx, gy] = worldToGrid(goal);
  int estimatedDistance = std::max(std::abs(gx - sx), std::abs(gy - sy));
  coarsePath.reserve(estimatedDistance / 4 +
                     10); // Reserve estimated path length
  auto coarseResult = m_coarseGrid->findPath(start, goal, coarsePath);

  if (coarseResult != PathfindingResult::SUCCESS) {
    // Coarse pathfinding failed, try direct pathfinding as fallback
    PATHFIND_DEBUG(std::format(
        "Coarse pathfinding result: {}, attempting direct pathfinding",
        static_cast<int>(coarseResult)));
    return findPath(start, goal, outPath);
  }

  if (coarsePath.size() < 2) {
    // Coarse path too short (start == goal?), use direct pathfinding
    return findPath(start, goal, outPath);
  }

  // Step 2: Refine coarse path segments on fine grid
  const auto refineResult = refineCoarsePath(coarsePath, start, goal, outPath);

  // Step 3: Validate refined path has no disconnected segments
  // When segment refinement fails, refineCoarsePath() inserts direct waypoints
  // which can create large gaps. Detect and fallback to direct A* if needed.
  if (outPath.size() >= 2) {
    // Allow gaps up to 8x coarse cell size (accounts for diagonal + refinement
    // skips) Larger tolerance prevents unnecessary fallbacks to direct A*
    float const maxAllowedGap = m_cell * COARSE_GRID_MULTIPLIER * 8.0f;
    bool hasDisconnectedSegment = false;

    for (size_t i = 1; i < outPath.size(); ++i) {
      float gap = (outPath[i] - outPath[i - 1]).length();
      if (gap > maxAllowedGap) {
        hasDisconnectedSegment = true;
        PATHFIND_DEBUG(
            std::format("Hierarchical path has disconnected segment: "
                        "gap={:.1f} > max={:.1f}, falling back to direct A*",
                        gap, maxAllowedGap));
        break;
      }
    }

    if (hasDisconnectedSegment) {
      outPath.clear();
      return findPath(start, goal, outPath);
    }
  }

  return refineResult;
}

PathfindingResult
PathfindingGrid::refineCoarsePath(const std::vector<Vector2D> &coarsePath,
                                  const Vector2D &start, const Vector2D &goal,
                                  std::vector<Vector2D> &outPath) {
  outPath.clear();
  outPath.reserve(coarsePath.size() * 4); // Estimate refined path size

  // Add start point
  outPath.push_back(start);

  // Refine each segment of the coarse path
  Vector2D currentPoint = start;

  bool loggedFailure = false;
  for (size_t i = 1; i < coarsePath.size(); ++i) {
    Vector2D segmentGoal = coarsePath[i];

    // Check if we can directly connect to this coarse waypoint
    if (hasLineOfSight(currentPoint, segmentGoal)) {
      // Direct line of sight - no need for detailed pathfinding
      outPath.push_back(segmentGoal);
      currentPoint = segmentGoal;
    } else {
      // Need detailed pathfinding for this segment
      std::vector<Vector2D> segmentPath;
      auto result = findPath(currentPoint, segmentGoal, segmentPath);

      if (result != PathfindingResult::SUCCESS || segmentPath.empty()) {
        if (!loggedFailure) {
          PATHFIND_WARN("Segment refinement failed, using direct line");
          loggedFailure = true;
        }
        outPath.push_back(segmentGoal);
        currentPoint = segmentGoal;
      } else {
        // Add refined segment (skip first point to avoid duplicates)
        for (size_t j = 1; j < segmentPath.size(); ++j) {
          outPath.push_back(segmentPath[j]);
        }
        currentPoint = segmentPath.back();
      }
    }
  }

  // Ensure we end at the exact goal
  if (outPath.empty() || (outPath.back() - goal).length() > m_cell * 0.5f) {
    outPath.push_back(goal);
  }

  // Apply path smoothing to the final result
  smoothPath(outPath);

  return PathfindingResult::SUCCESS;
}

void PathfindingGrid::setBlocked(int gx, int gy, bool blocked) {
  if (inBounds(gx, gy)) {
    size_t idx = static_cast<size_t>(gy * m_w + gx);
    if (idx < m_blocked.size()) {
      m_blocked[idx] = blocked ? 1 : 0;
    }
  }
}

void PathfindingGrid::setWeight(int gx, int gy, float weight) {
  if (inBounds(gx, gy)) {
    size_t idx = static_cast<size_t>(gy * m_w + gx);
    if (idx < m_weight.size()) {
      m_weight[idx] = weight;
    }
  }
}

} // namespace VoidLight
