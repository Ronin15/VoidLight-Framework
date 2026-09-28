/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

// AIManager-owned spatial grid for O(K) entity queries.
// Rebuilt once per frame on main thread, read-only during batch processing.
//
// Uses open-addressing hash table with generation-based O(1) clear and
// contiguous entry buffer (counting-sort layout) for cache-friendly queries.
#ifndef AI_INTERNAL_SPATIAL_GRID_HPP
#define AI_INTERNAL_SPATIAL_GRID_HPP

#include "utils/Vector2D.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

class AISpatialGrid {
public:
    struct Entry {
        size_t edmIndex;
        Vector2D position;
    };

    // Rebuild the grid from active entity indices.
    // getPos(edmIndex) must return the entity's current position.
    // Called once per frame on main thread before batch processing.
    template <typename PositionGetter>
    void rebuild(const std::vector<size_t>& activeIndices, PositionGetter&& getPos) {
        const size_t count = activeIndices.size();
        m_entryCount = 0;
        if (count == 0) return;

        // O(1) clear via generation increment
        m_generation++;
        if (m_generation == 0) {
            // Overflow (every ~4 billion frames): force-reset all slots
            for (auto& s : m_table) s.generation = 0;
            m_generation = 1;
        }

        // Ensure staging buffers have capacity
        if (m_tempKeys.size() < count) {
            m_tempKeys.resize(count);
            m_tempPositions.resize(count);
        }

        // Cache positions and compute cell keys (single pass over EDM data)
        for (size_t i = 0; i < count; ++i) {
            m_tempPositions[i] = getPos(activeIndices[i]);
            m_tempKeys[i] = cellKey(m_tempPositions[i]);
        }

        // Ensure hash table capacity (load factor < 0.5 for fast probing)
        size_t neededSlots = nextPowerOf2(std::max<size_t>(128, count * 2));
        if (m_table.size() < neededSlots) {
            m_table.resize(neededSlots);
        }
        m_tableMask = m_table.size() - 1;

        // Pass 1: Count entries per cell + track occupied slots
        m_usedSlots.clear();
        for (size_t i = 0; i < count; ++i) {
            bool created = false;
            auto& slot = findOrCreate(m_tempKeys[i], created);
            slot.count++;
            if (created) {
                m_usedSlots.push_back(static_cast<uint32_t>(&slot - m_table.data()));
            }
        }

        // Pass 2: Prefix sum over occupied slots only
        uint32_t offset = 0;
        for (uint32_t slotIdx : m_usedSlots) {
            auto& slot = m_table[slotIdx];
            slot.start = offset;
            offset += slot.count;
            slot.count = 0; // Reset for scatter pass
        }

        // Pass 3: Scatter entries into contiguous buffer
        m_entries.resize(count);
        for (size_t i = 0; i < count; ++i) {
            auto& slot = find(m_tempKeys[i]);
            uint32_t idx = slot.start + slot.count++;
            m_entries[idx] = {activeIndices[i], m_tempPositions[i]};
        }

        m_entryCount = count;
    }

    // Query all edmIndices within radius of center. O(K) where K = nearby entities.
    // Thread-safe for concurrent reads (no mutation after rebuild).
    void queryRadius(const Vector2D& center, float radius,
        std::vector<size_t>& outEdmIndices) const {
        outEdmIndices.clear();
        if (m_entryCount == 0) return;

        const float radiusSq = radius * radius;

        // Compute cell range covering the query AABB
        int minCellX = static_cast<int>(std::floor((center.getX() - radius) * INV_CELL_SIZE));
        int maxCellX = static_cast<int>(std::floor((center.getX() + radius) * INV_CELL_SIZE));
        int minCellY = static_cast<int>(std::floor((center.getY() - radius) * INV_CELL_SIZE));
        int maxCellY = static_cast<int>(std::floor((center.getY() + radius) * INV_CELL_SIZE));

        for (int cy = minCellY; cy <= maxCellY; ++cy) {
            for (int cx = minCellX; cx <= maxCellX; ++cx) {
                uint64_t key = packKey(cx, cy);
                const CellSlot* slot = findConst(key);
                if (!slot) continue;

                // Entries are contiguous in m_entries[start..start+count)
                const Entry* begin = m_entries.data() + slot->start;
                const Entry* end = begin + slot->count;
                for (const Entry* e = begin; e != end; ++e) {
                    float dx = e->position.getX() - center.getX();
                    float dy = e->position.getY() - center.getY();
                    if (dx * dx + dy * dy <= radiusSq) {
                        outEdmIndices.push_back(e->edmIndex);
                    }
                }
            }
        }
    }

    void clear() {
        m_generation++;
        m_entryCount = 0;
    }

    [[nodiscard]] bool empty() const { return m_entryCount == 0; }
    [[nodiscard]] size_t size() const { return m_entryCount; }

private:
    static constexpr float CELL_SIZE = 128.0f;
    static constexpr float INV_CELL_SIZE = 1.0f / CELL_SIZE;

    struct CellSlot {
        uint64_t key{0};
        uint32_t start{0};
        uint32_t count{0};
        uint32_t generation{0};
    };

    // Open-addressing with linear probing (generation-based occupancy)
    CellSlot& findOrCreate(uint64_t key, bool& wasCreated) {
        size_t idx = hashIndex(key);
        while (true) {
            auto& slot = m_table[idx];
            if (slot.generation != m_generation) {
                // Empty slot — claim it
                slot.key = key;
                slot.count = 0;
                slot.start = 0;
                slot.generation = m_generation;
                wasCreated = true;
                return slot;
            }
            if (slot.key == key) {
                wasCreated = false;
                return slot;
            }
            idx = (idx + 1) & m_tableMask;
        }
    }

    CellSlot& find(uint64_t key) {
        size_t idx = hashIndex(key);
        while (true) {
            auto& slot = m_table[idx];
            if (slot.key == key && slot.generation == m_generation) return slot;
            idx = (idx + 1) & m_tableMask;
        }
    }

    const CellSlot* findConst(uint64_t key) const {
        size_t idx = hashIndex(key);
        while (true) {
            const auto& slot = m_table[idx];
            if (slot.generation != m_generation) return nullptr;
            if (slot.key == key) return &slot;
            idx = (idx + 1) & m_tableMask;
        }
    }

    size_t hashIndex(uint64_t key) const {
        // Fibonacci hashing for good distribution
        return static_cast<size_t>((key * 0x9E3779B97F4A7C15ULL) >> 32) & m_tableMask;
    }

    static uint64_t packKey(int cx, int cy) {
        auto ux = static_cast<uint32_t>(cx);
        auto uy = static_cast<uint32_t>(cy);
        return (static_cast<uint64_t>(ux) << 32) | static_cast<uint64_t>(uy);
    }

    static uint64_t cellKey(const Vector2D& pos) {
        int cx = static_cast<int>(std::floor(pos.getX() * INV_CELL_SIZE));
        int cy = static_cast<int>(std::floor(pos.getY() * INV_CELL_SIZE));
        return packKey(cx, cy);
    }

    static size_t nextPowerOf2(size_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        return v + 1;
    }

    // Hash table (open-addressing, linear probing)
    std::vector<CellSlot> m_table;
    size_t m_tableMask{0};
    uint32_t m_generation{0};

    // Contiguous entry buffer (counting-sort layout: entries grouped by cell)
    std::vector<Entry> m_entries;
    size_t m_entryCount{0};

    // Staging buffers (reused across frames)
    std::vector<uint64_t> m_tempKeys;
    std::vector<Vector2D> m_tempPositions;
    std::vector<uint32_t> m_usedSlots; // Indices of occupied slots this frame
};

#endif // AI_INTERNAL_SPATIAL_GRID_HPP
