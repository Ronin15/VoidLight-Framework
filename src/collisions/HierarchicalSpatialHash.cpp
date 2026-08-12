/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "collisions/HierarchicalSpatialHash.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cassert>
#include <format>

namespace VoidLight {

// ========== HierarchicalSpatialHash Implementation ==========

HierarchicalSpatialHash::HierarchicalSpatialHash() {
    // Reserve reasonable initial capacity
    m_regions.reserve(64);
    m_bodyLocations.reserve(1024);
}

void HierarchicalSpatialHash::insert(size_t bodyIndex, const AABB& aabb) {
    // SINGLE-THREADED: No locks needed - collision runs on main thread only

    // Get the coarse regions this body overlaps (reuse scratch, retain capacity)
    auto& regions = m_tempModifyRegions;
    getCoarseCoordsForAABB(aabb, regions);

    if (regions.empty()) {
        return;
    }

    // Track the body's location BEFORE the region-insertion loop. If this
    // insertion crosses REGION_ACTIVE_THRESHOLD, insertIntoRegion() calls
    // subdivideRegion(), which re-files every body in the coarse region by
    // looking up its lastAABB in m_bodyLocations. If this body's entry did not
    // exist yet (written after the loop), the current body would be skipped and
    // then discarded by bodyIndices.clear() — dropping it from all fine cells.
    // unordered_map references are only invalidated by erasing that element, so
    // this reference stays valid across the loop below.
    BodyLocation& location = m_bodyLocations[bodyIndex];
    location.region = regions[0]; // Use first region as primary
    location.fineCell = 0; // Refined below once fine subdivision is known
    location.lastAABB = aabb;

    // Insert into all overlapping regions
    for (const auto& regionCoord : regions) {
        Region& region = m_regions[regionCoord];
        region.coord = regionCoord;
        insertIntoRegion(region, bodyIndex, aabb);
    }

    // If primary region has fine subdivision, compute fine cell
    auto regionIt = m_regions.find(regions[0]);
    if (regionIt != m_regions.end() && regionIt->second.hasFineSplit) {
        FineCoord fineCoord = getFineCoord(aabb, regions[0]);
        location.fineCell = computeGridKey(fineCoord);
    }
}

void HierarchicalSpatialHash::remove(size_t bodyIndex) {
    // SINGLE-THREADED: No locks needed - collision runs on main thread only

    auto locationIt = m_bodyLocations.find(bodyIndex);
    if (locationIt == m_bodyLocations.end()) {
        return; // Body not found
    }

    const BodyLocation& location = locationIt->second;
    const AABB& aabb = location.lastAABB;

    // Get all regions this body was in (reuse scratch, retain capacity)
    auto& regions = m_tempModifyRegions;
    getCoarseCoordsForAABB(aabb, regions);

    // Remove from all regions
    for (const auto& regionCoord : regions) {
        auto regionIt = m_regions.find(regionCoord);
        if (regionIt != m_regions.end()) {
            removeFromRegion(regionIt->second, bodyIndex, aabb);

            // Clean up empty regions
            if (regionIt->second.bodyCount == 0) {
                m_regions.erase(regionIt);
            }
        }
    }

    m_bodyLocations.erase(locationIt);
}

void HierarchicalSpatialHash::update(size_t bodyIndex, const AABB& oldAABB, const AABB& newAABB) {

    // Check movement threshold to avoid unnecessary work
    if (!hasMovedSignificantly(oldAABB, newAABB)) {
        // Update stored AABB but don't rehash
        auto locationIt = m_bodyLocations.find(bodyIndex);
        if (locationIt != m_bodyLocations.end()) {
            locationIt->second.lastAABB = newAABB;
        }
        return;
    }

    // Check if regions changed (reuse scratch, retain capacity).
    // Distinct from m_tempModifyRegions so the remove()/insert() fallback below
    // cannot clobber these while they are still in use.
    auto& oldRegions = m_tempUpdateOldRegions;
    auto& newRegions = m_tempUpdateNewRegions;
    getCoarseCoordsForAABB(oldAABB, oldRegions);
    getCoarseCoordsForAABB(newAABB, newRegions);

    // If regions are the same, just update fine cells if needed
    if (oldRegions == newRegions && oldRegions.size() == 1) {
        CoarseCoord regionCoord = oldRegions[0];
        auto regionIt = m_regions.find(regionCoord);
        if (regionIt != m_regions.end()) {
            if (regionIt->second.hasFineSplit) {
                // Handle fine-subdivided region
                FineCoord const oldFine = getFineCoord(oldAABB, regionCoord);
                FineCoord const newFine = getFineCoord(newAABB, regionCoord);

                if (oldFine.x != newFine.x || oldFine.y != newFine.y) {
                    // Move between fine cells within same region
                    GridKey oldKey = computeGridKey(oldFine);
                    GridKey newKey = computeGridKey(newFine);

                    Region& region = regionIt->second;

                    // Remove from old fine cell
                    auto oldCellIt = region.fineCells.find(oldKey);
                    if (oldCellIt != region.fineCells.end()) {
                        auto& bodyVec = oldCellIt->second;
                        bodyVec.erase(std::remove(bodyVec.begin(), bodyVec.end(), bodyIndex), bodyVec.end());
                        if (bodyVec.empty()) {
                            region.fineCells.erase(oldCellIt);
                        }
                    }

                    // Add to new fine cell
                    region.fineCells[newKey].push_back(bodyIndex);

                    // Update location tracking
                    auto locationIt = m_bodyLocations.find(bodyIndex);
                    if (locationIt != m_bodyLocations.end()) {
                        locationIt->second.fineCell = newKey;
                        locationIt->second.lastAABB = newAABB;
                    }
                } else {
                    // Same fine cell, just update AABB
                    auto locationIt = m_bodyLocations.find(bodyIndex);
                    if (locationIt != m_bodyLocations.end()) {
                        locationIt->second.lastAABB = newAABB;
                    }
                }
            } else {
                // Handle non-subdivided region - just update AABB in location tracking
                // The body remains in the same coarse cell (region.bodyIndices)
                auto locationIt = m_bodyLocations.find(bodyIndex);
                if (locationIt != m_bodyLocations.end()) {
                    locationIt->second.lastAABB = newAABB;
                }
            }
        }
        return;
    }

    // Full remove/insert if regions changed
    remove(bodyIndex);
    insert(bodyIndex, newAABB);
}

void HierarchicalSpatialHash::clear() {
    // SINGLE-THREADED: No locks needed - collision runs on main thread only
    m_regions.clear();
    m_bodyLocations.clear();
}

void HierarchicalSpatialHash::reserve(size_t expectedBodyCount) {
    // OPTIMIZATION: Pre-allocate capacity to prevent hash table rebalancing during insertions
    // Prevents 10-20% performance regression from hash growth and rehashing
    // Expected ratio: ~5-10 coarse regions per 1000 bodies (depends on world distribution)
    // Use 2x expectedBodyCount to be safe (load factor < 0.5 prevents rebalancing)

    // Reserve space for expected number of coarse regions
    // Empirically: ~200 bodies → ~20 regions, ~5000 bodies → ~250 regions
    size_t expectedRegions = std::max(size_t(64), expectedBodyCount / 20);
    reserveRegions(expectedRegions * 2);  // 2x safety margin
}

void HierarchicalSpatialHash::reserveRegions(size_t expectedRegionCount) {
    // OPTIMIZATION: Pre-allocate hash table buckets to prevent rebalancing
    // std::unordered_map rehashes when load factor exceeds ~0.75
    // Pre-allocating prevents mid-insertion rehashing which causes:
    // - All elements rehashed and repositioned
    // - Memory reallocation with cache misses
    // - Performance regression of 10-15%

    // Reserve buckets (2x expected count prevents rebalancing)
    m_regions.reserve(expectedRegionCount);

    // Also pre-allocate body locations tracking
    // Most bodies will be tracked (10-20x more than regions)
    m_bodyLocations.reserve(expectedRegionCount * 10);
}

void HierarchicalSpatialHash::queryRegion(const AABB& area, std::vector<size_t>& outBodyIndices) const {
    outBodyIndices.clear();
    outBodyIndices.reserve(64); // Reserve for typical query result size

    // PERFORMANCE: Use persistent buffers to eliminate per-query allocations (single-threaded safe)
    m_tempSeenBodies.clear();
    m_tempSeenBodies.reserve(64);

    // Get all coarse regions this query overlaps (reuse persistent buffer - NO allocation!)
    getCoarseCoordsForAABB(area, m_tempQueryRegions);

    // PERFORMANCE: No mutex needed - single-threaded collision system
    for (const auto& regionCoord : m_tempQueryRegions) {
        auto regionIt = m_regions.find(regionCoord);
        if (regionIt == m_regions.end()) {
            continue;
        }

        const Region& region = regionIt->second;

        if (region.hasFineSplit) {
            // Query only fine cells that overlap the query area (reuse persistent buffer - NO allocation!)
            getFineCoordList(area, regionCoord, m_tempQueryFineCells);

            for (const auto& fineCoord : m_tempQueryFineCells) {
                GridKey key = computeGridKey(fineCoord);
                auto fineCellIt = region.fineCells.find(key);
                if (fineCellIt != region.fineCells.end()) {
                    for (size_t bodyIndex : fineCellIt->second) {
                        // PERFORMANCE: O(1) hash set insertion for deduplication
                        if (m_tempSeenBodies.insert(bodyIndex).second) {
                            outBodyIndices.push_back(bodyIndex);
                        }
                    }
                }
            }
        } else {
            // Query coarse cell directly
            for (size_t bodyIndex : region.bodyIndices) {
                // PERFORMANCE: O(1) hash set insertion for deduplication
                if (m_tempSeenBodies.insert(bodyIndex).second) {
                    outBodyIndices.push_back(bodyIndex);
                }
            }
        }
    }
}

void HierarchicalSpatialHash::queryRegionBounds(float minX, float minY, float maxX, float maxY, std::vector<size_t>& outBodyIndices) const {
    outBodyIndices.clear();
    outBodyIndices.reserve(64); // Reserve for typical query result size

    // PERFORMANCE: Use persistent buffers to eliminate per-query allocations (single-threaded safe)
    m_tempSeenBodies.clear();
    m_tempSeenBodies.reserve(64);

    // Get all coarse regions this query overlaps using bounds directly (NO AABB construction!)
    getCoarseCoordsForBounds(minX, minY, maxX, maxY, m_tempQueryRegions);

    // Construct AABB only if needed for fine cell queries (rare case)
    AABB const queryArea(
        (minX + maxX) * 0.5f, // centerX
        (minY + maxY) * 0.5f, // centerY
        (maxX - minX) * 0.5f, // halfWidth
        (maxY - minY) * 0.5f  // halfHeight
    );

    // PERFORMANCE: No mutex needed - single-threaded collision system
    for (const auto& regionCoord : m_tempQueryRegions) {
        auto regionIt = m_regions.find(regionCoord);
        if (regionIt == m_regions.end()) {
            continue;
        }

        const Region& region = regionIt->second;

        if (region.hasFineSplit) {
            // Query only fine cells that overlap the query area (reuse persistent buffer - NO allocation!)
            getFineCoordList(queryArea, regionCoord, m_tempQueryFineCells);

            for (const auto& fineCoord : m_tempQueryFineCells) {
                GridKey key = computeGridKey(fineCoord);
                auto fineCellIt = region.fineCells.find(key);
                if (fineCellIt != region.fineCells.end()) {
                    for (size_t bodyIndex : fineCellIt->second) {
                        // PERFORMANCE: O(1) hash set insertion for deduplication
                        if (m_tempSeenBodies.insert(bodyIndex).second) {
                            outBodyIndices.push_back(bodyIndex);
                        }
                    }
                }
            }
        } else {
            // Query coarse cell directly
            for (size_t bodyIndex : region.bodyIndices) {
                // PERFORMANCE: O(1) hash set insertion for deduplication
                if (m_tempSeenBodies.insert(bodyIndex).second) {
                    outBodyIndices.push_back(bodyIndex);
                }
            }
        }
    }
}

void HierarchicalSpatialHash::queryRegionBoundsThreadSafe(
    float minX, float minY, float maxX, float maxY,
    std::vector<size_t>& outBodyIndices,
    QueryBuffers& buffers) const {

    outBodyIndices.clear();
    outBodyIndices.reserve(64);

    // Use thread-local buffers instead of mutable members (thread-safe)
    buffers.seenBodies.clear();
    buffers.seenBodies.reserve(64);

    // Get all coarse regions this query overlaps using bounds directly
    getCoarseCoordsForBounds(minX, minY, maxX, maxY, buffers.queryRegions);

    // Construct AABB only if needed for fine cell queries
    AABB const queryArea(
        (minX + maxX) * 0.5f,
        (minY + maxY) * 0.5f,
        (maxX - minX) * 0.5f,
        (maxY - minY) * 0.5f
    );

    // Thread-safe read-only access to m_regions (no modifications during query)
    for (const auto& regionCoord : buffers.queryRegions) {
        auto regionIt = m_regions.find(regionCoord);
        if (regionIt == m_regions.end()) {
            continue;
        }

        const Region& region = regionIt->second;

        if (region.hasFineSplit) {
            // Query only fine cells that overlap the query area
            getFineCoordList(queryArea, regionCoord, buffers.queryFineCells);

            for (const auto& fineCoord : buffers.queryFineCells) {
                GridKey key = computeGridKey(fineCoord);
                auto fineCellIt = region.fineCells.find(key);
                if (fineCellIt != region.fineCells.end()) {
                    for (size_t bodyIndex : fineCellIt->second) {
                        if (buffers.seenBodies.insert(bodyIndex).second) {
                            outBodyIndices.push_back(bodyIndex);
                        }
                    }
                }
            }
        } else {
            // Query coarse cell directly
            for (size_t bodyIndex : region.bodyIndices) {
                if (buffers.seenBodies.insert(bodyIndex).second) {
                    outBodyIndices.push_back(bodyIndex);
                }
            }
        }
    }
}

// ========== Batch Operations ==========

void HierarchicalSpatialHash::insertBatch(const std::vector<std::pair<size_t, AABB>>& bodies) {

    // Pre-reserve capacity
    m_bodyLocations.reserve(m_bodyLocations.size() + bodies.size());

    for (const auto& body : bodies) {
        insert(body.first, body.second);
    }
}

void HierarchicalSpatialHash::updateBatch(const std::vector<std::tuple<size_t, AABB, AABB>>& updates) {

    for (const auto& updateInfo : updates) {
        update(std::get<0>(updateInfo), std::get<1>(updateInfo), std::get<2>(updateInfo));
    }
}

// ========== Statistics and Debugging ==========

size_t HierarchicalSpatialHash::getActiveRegionCount() const {
    return std::count_if(m_regions.begin(), m_regions.end(),
        [](const auto& regionPair) { return regionPair.second.bodyCount > 0; });
}

size_t HierarchicalSpatialHash::getTotalFineCells() const {
    return std::accumulate(m_regions.begin(), m_regions.end(), size_t{0},
        [](size_t sum, const auto& regionPair) {
            return sum + regionPair.second.fineCells.size();
        });
}

void HierarchicalSpatialHash::logStatistics() const {
    COLLISION_INFO("HierarchicalSpatialHash Statistics:");
    COLLISION_INFO(std::format("  Total Bodies: {}", m_bodyLocations.size()));
    COLLISION_INFO(std::format("  Total Regions: {}", m_regions.size()));
    COLLISION_INFO(std::format("  Active Regions: {}", getActiveRegionCount()));
    COLLISION_INFO(std::format("  Total Fine Cells: {}", getTotalFineCells()));
}

// ========== Private Helper Methods ==========

HierarchicalSpatialHash::CoarseCoord HierarchicalSpatialHash::getCoarseCoord(const AABB& aabb) const {
    return {
        static_cast<int32_t>(std::floor(aabb.center.getX() / COARSE_CELL_SIZE)),
        static_cast<int32_t>(std::floor(aabb.center.getY() / COARSE_CELL_SIZE))
    };
}

void HierarchicalSpatialHash::getCoarseCoordsForAABB(const AABB& aabb, std::vector<CoarseCoord>& out) const {
    getCoarseCoordsForBounds(aabb.left(), aabb.top(), aabb.right(), aabb.bottom(), out);
}

void HierarchicalSpatialHash::getCoarseCoordsForBounds(float minX, float minY, float maxX, float maxY, std::vector<CoarseCoord>& out) const {
    int32_t gridMinX = static_cast<int32_t>(std::floor(minX / COARSE_CELL_SIZE));
    int32_t gridMaxX = static_cast<int32_t>(std::floor(maxX / COARSE_CELL_SIZE));
    int32_t gridMinY = static_cast<int32_t>(std::floor(minY / COARSE_CELL_SIZE));
    int32_t gridMaxY = static_cast<int32_t>(std::floor(maxY / COARSE_CELL_SIZE));

    out.clear();
    for (int32_t y = gridMinY; y <= gridMaxY; ++y) {
        for (int32_t x = gridMinX; x <= gridMaxX; ++x) {
            out.push_back({x, y});
        }
    }
}

HierarchicalSpatialHash::FineCoord HierarchicalSpatialHash::getFineCoord(const AABB& aabb,
                                                                         const CoarseCoord& region) const {
    // Compute fine coordinates relative to the region's origin
    float const regionOriginX = region.x * COARSE_CELL_SIZE;
    float const regionOriginY = region.y * COARSE_CELL_SIZE;
    float const relativeX = aabb.center.getX() - regionOriginX;
    float const relativeY = aabb.center.getY() - regionOriginY;

    int32_t fineX = static_cast<int32_t>(std::floor(relativeX / FINE_CELL_SIZE));
    int32_t fineY = static_cast<int32_t>(std::floor(relativeY / FINE_CELL_SIZE));

    // Clamp to region bounds so a boundary-straddling body whose center lies
    // outside this region is filed into the same boundary fine cell the query
    // side inspects. getFineCoordList clamps query bounds identically; insert
    // and query MUST agree or a straddling body becomes unreachable.
    // (fine cells per coarse cell = COARSE_CELL_SIZE / FINE_CELL_SIZE = 4)
    int32_t const maxCellIndex = static_cast<int32_t>(COARSE_CELL_SIZE / FINE_CELL_SIZE) - 1;
    fineX = std::max(0, std::min(fineX, maxCellIndex));
    fineY = std::max(0, std::min(fineY, maxCellIndex));

    return {fineX, fineY};
}

void HierarchicalSpatialHash::getFineCoordList(const AABB& aabb, const CoarseCoord& region, std::vector<FineCoord>& out) const {
    // Compute region's origin in world coordinates
    float const regionOriginX = region.x * COARSE_CELL_SIZE;
    float const regionOriginY = region.y * COARSE_CELL_SIZE;

    // Convert AABB bounds to region-relative coordinates
    float const relativeLeft = aabb.left() - regionOriginX;
    float const relativeRight = aabb.right() - regionOriginX;
    float const relativeTop = aabb.top() - regionOriginY;
    float const relativeBottom = aabb.bottom() - regionOriginY;

    // Find fine cell bounds
    int32_t minX = static_cast<int32_t>(std::floor(relativeLeft / FINE_CELL_SIZE));
    int32_t maxX = static_cast<int32_t>(std::floor(relativeRight / FINE_CELL_SIZE));
    int32_t minY = static_cast<int32_t>(std::floor(relativeTop / FINE_CELL_SIZE));
    int32_t maxY = static_cast<int32_t>(std::floor(relativeBottom / FINE_CELL_SIZE));

    // Clamp to region bounds (fine cells per coarse cell = COARSE_CELL_SIZE / FINE_CELL_SIZE = 4)
    int32_t const maxCellIndex = static_cast<int32_t>(COARSE_CELL_SIZE / FINE_CELL_SIZE) - 1;
    minX = std::max(0, std::min(minX, maxCellIndex));
    maxX = std::max(0, std::min(maxX, maxCellIndex));
    minY = std::max(0, std::min(minY, maxCellIndex));
    maxY = std::max(0, std::min(maxY, maxCellIndex));

    out.clear();
    for (int32_t y = minY; y <= maxY; ++y) {
        for (int32_t x = minX; x <= maxX; ++x) {
            out.push_back({x, y});
        }
    }
}

HierarchicalSpatialHash::GridKey HierarchicalSpatialHash::computeGridKey(const FineCoord& coord) const {
    // Simple 2D grid hash - much faster than Morton encoding
    // Pack coordinates into 64-bit key: (x << 32) | y
    uint32_t const x = static_cast<uint32_t>(coord.x + 32768); // Offset for negative coords
    uint32_t const y = static_cast<uint32_t>(coord.y + 32768);
    return (static_cast<uint64_t>(x) << 32) | y;
}

bool HierarchicalSpatialHash::hasMovedSignificantly(const AABB& oldAABB, const AABB& newAABB) const {
    Vector2D const centerDelta = newAABB.center - oldAABB.center;
    float const distanceSquared = centerDelta.lengthSquared();
    return distanceSquared > (MOVEMENT_THRESHOLD * MOVEMENT_THRESHOLD);
}

void HierarchicalSpatialHash::insertIntoRegion(Region& region, size_t bodyIndex, const AABB& aabb) {
    region.bodyCount++;

    if (region.hasFineSplit) {
        // Insert into fine cell
        FineCoord const fineCoord = getFineCoord(aabb, region.coord);
        GridKey key = computeGridKey(fineCoord);
        region.fineCells[key].push_back(bodyIndex);
    } else {
        // Insert into coarse cell
        region.bodyIndices.push_back(bodyIndex);

        // Check if we should subdivide
        if (region.bodyCount > REGION_ACTIVE_THRESHOLD) {
            subdivideRegion(region);
        }
    }
}

void HierarchicalSpatialHash::removeFromRegion(Region& region, size_t bodyIndex, const AABB& aabb) {
    region.bodyCount--;

    if (region.hasFineSplit) {
        // Remove from fine cell
        FineCoord const fineCoord = getFineCoord(aabb, region.coord);
        GridKey key = computeGridKey(fineCoord);

        auto fineCellIt = region.fineCells.find(key);
        if (fineCellIt != region.fineCells.end()) {
            auto& bodyVec = fineCellIt->second;
            bodyVec.erase(std::remove(bodyVec.begin(), bodyVec.end(), bodyIndex), bodyVec.end());
            if (bodyVec.empty()) {
                region.fineCells.erase(fineCellIt);
            }
        }

        // Check if we should unsubdivide
        if (region.bodyCount <= REGION_ACTIVE_THRESHOLD) {
            unsubdivideRegion(region);
        }
    } else {
        // Remove from coarse cell
        auto& bodyVec = region.bodyIndices;
        bodyVec.erase(std::remove(bodyVec.begin(), bodyVec.end(), bodyIndex), bodyVec.end());
    }
}

void HierarchicalSpatialHash::subdivideRegion(Region& region) {
    if (region.hasFineSplit) return; // Already subdivided

    // Move all bodies from coarse list to fine cells
    for (size_t bodyIndex : region.bodyIndices) {
        auto locationIt = m_bodyLocations.find(bodyIndex);
        if (locationIt != m_bodyLocations.end()) {
            const AABB& aabb = locationIt->second.lastAABB;
            FineCoord fineCoord = getFineCoord(aabb, region.coord);
            GridKey key = computeGridKey(fineCoord);
            region.fineCells[key].push_back(bodyIndex);
        }
    }

    region.bodyIndices.clear();
    region.hasFineSplit = true;
}

void HierarchicalSpatialHash::unsubdivideRegion(Region& region) {
    if (!region.hasFineSplit) return; // Not subdivided

    // Move all bodies from fine cells back to coarse list
    for (const auto& fineCellPair : region.fineCells) {
        for (size_t bodyIndex : fineCellPair.second) {
            region.bodyIndices.push_back(bodyIndex);
        }
    }

    region.fineCells.clear();
    region.hasFineSplit = false;
}

} // namespace VoidLight