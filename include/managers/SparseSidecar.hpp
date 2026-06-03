/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef SPARSE_SIDECAR_HPP
#define SPARSE_SIDECAR_HPP

/**
 * @file SparseSidecar.hpp
 * @brief Per-category sparse/dense/denseToEdm triple for transient per-entity state.
 *
 * Implements the industry-standard ECS component storage layout (EnTT/Flecs style).
 * Designed for N-of-M independent transient state categories (knockback, stun,
 * poison, slow, etc.) where multiple categories may coexist on the same entity.
 *
 * This is NOT a general component registry. Each transient category is declared
 * as an explicit SparseSidecar<T> member on EntityDataManager — no type erasure,
 * no polymorphism, no shared coupling between categories.
 *
 * Complexity:
 *   apply()    O(1) amortized
 *   has()      O(1) — single array load
 *   get()      O(1) — single array load + bounds check
 *   remove()   O(1) — swap-pop + one sparse patch
 *   iteration  cache-friendly contiguous dense array
 */

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

template <typename T>
class SparseSidecar
{
public:
    static constexpr uint32_t NULL_SLOT = std::numeric_limits<uint32_t>::max();

    // ========================================================================
    // GROWTH — called whenever EDM grows m_hotData
    // ========================================================================

    /**
     * @brief Grow the sparse array to cover entityCount entities.
     *        New entries are initialised to NULL_SLOT. Existing entries are untouched.
     *        Must be called from the main thread (same site as m_hotData growth).
     */
    void resizeSparse(size_t entityCount)
    {
        if (entityCount > m_sparse.size())
        {
            m_sparse.resize(entityCount, NULL_SLOT);
        }
    }

    // ========================================================================
    // WRITE ACCESS
    // ========================================================================

    /**
     * @brief Insert a default-constructed T for edmIdx, or return the existing one.
     *        Returns a mutable reference so the caller can fill the fields.
     */
    T& apply(uint32_t edmIdx)
    {
        if (edmIdx < m_sparse.size() && m_sparse[edmIdx] != NULL_SLOT)
        {
            return m_dense[m_sparse[edmIdx]];
        }

        // Grow sparse if needed (entity created before the last resizeSparse call)
        if (edmIdx >= m_sparse.size())
        {
            m_sparse.resize(static_cast<size_t>(edmIdx) + 1, NULL_SLOT);
        }

        const uint32_t denseIdx = static_cast<uint32_t>(m_dense.size());
        m_sparse[edmIdx] = denseIdx;
        m_dense.emplace_back();
        m_denseToEdm.push_back(edmIdx);
        return m_dense.back();
    }

    /**
     * @brief Swap-pop removal. Patches the displaced entity's sparse entry.
     *        No-op if edmIdx has no entry.
     */
    void remove(uint32_t edmIdx)
    {
        if (edmIdx >= m_sparse.size() || m_sparse[edmIdx] == NULL_SLOT)
        {
            return;
        }

        const uint32_t denseIdx = m_sparse[edmIdx];
        const uint32_t lastDense = static_cast<uint32_t>(m_dense.size()) - 1u;

        if (denseIdx != lastDense)
        {
            // Move last element into the vacated slot
            m_dense[denseIdx]     = std::move(m_dense[lastDense]);
            m_denseToEdm[denseIdx] = m_denseToEdm[lastDense];

            // Patch the displaced entity's sparse so it still points to denseIdx
            m_sparse[m_denseToEdm[denseIdx]] = denseIdx;
        }

        m_dense.pop_back();
        m_denseToEdm.pop_back();
        m_sparse[edmIdx] = NULL_SLOT;
    }

    /**
     * @brief Semantic alias for freeSlot cleanup — identical to remove() for
     *        single-category sidecars.  Kept distinct so grep can distinguish
     *        "entity destroyed" paths from "effect expired" paths.
     */
    void removeAllFor(uint32_t edmIdx)
    {
        remove(edmIdx);
    }

    // ========================================================================
    // READ ACCESS
    // ========================================================================

    /**
     * @brief True if edmIdx has an active entry.
     */
    [[nodiscard]] bool has(uint32_t edmIdx) const noexcept
    {
        return edmIdx < m_sparse.size() && m_sparse[edmIdx] != NULL_SLOT;
    }

    /**
     * @brief Mutable pointer, nullptr if absent.
     */
    [[nodiscard]] T* get(uint32_t edmIdx) noexcept
    {
        if (edmIdx >= m_sparse.size() || m_sparse[edmIdx] == NULL_SLOT)
        {
            return nullptr;
        }
        return &m_dense[m_sparse[edmIdx]];
    }

    /**
     * @brief Const pointer, nullptr if absent.
     */
    [[nodiscard]] const T* get(uint32_t edmIdx) const noexcept
    {
        if (edmIdx >= m_sparse.size() || m_sparse[edmIdx] == NULL_SLOT)
        {
            return nullptr;
        }
        return &m_dense[m_sparse[edmIdx]];
    }

    // ========================================================================
    // AGGREGATE ACCESS
    // ========================================================================

    [[nodiscard]] size_t activeCount() const noexcept { return m_dense.size(); }

    /** @brief Contiguous dense data for SIMD-friendly batch iteration. */
    [[nodiscard]] std::span<T>       dense() noexcept       { return m_dense; }
    [[nodiscard]] std::span<const T> dense() const noexcept { return m_dense; }

    /** @brief denseSlot → edmIdx reverse map.  Same length as dense(). */
    [[nodiscard]] std::span<const uint32_t> owners() const noexcept { return m_denseToEdm; }

private:
    // Sparse: edmIdx → denseSlot.  NULL_SLOT = entity does not have this state.
    // Sized parallel to m_hotData via resizeSparse().
    std::vector<uint32_t> m_sparse;

    // Dense: only entities with active state occupy space.  SIMD-friendly.
    std::vector<T>        m_dense;

    // Reverse lookup: denseSlot → edmIdx.  Required for swap-pop owner patching.
    std::vector<uint32_t> m_denseToEdm;
};

#endif // SPARSE_SIDECAR_HPP
