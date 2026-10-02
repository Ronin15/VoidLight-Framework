/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef RESOURCE_HANDLE_HPP
#define RESOURCE_HANDLE_HPP

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>

namespace VoidLight {

/**
 * @brief Type-safe, lightweight handle for referencing resources
 *
 * ResourceHandle uses a 32-bit integer ID with generation counter to provide
 * fast, cache-friendly resource lookups while detecting stale references.
 * This replaces string-based resource identification for better performance.
 */
class ResourceHandle {
public:
    // Handle components
    using HandleId = uint32_t;
    using Generation = uint16_t;

    // Special values
    static constexpr HandleId INVALID_ID = 0;
    static constexpr Generation INVALID_GENERATION = 0;

    // Default constructor creates invalid handle
    constexpr ResourceHandle() noexcept
        : m_id(INVALID_ID), m_generation(INVALID_GENERATION) {}

    // Construct handle with ID and generation
    constexpr ResourceHandle(HandleId id, Generation generation) noexcept
        : m_id(id), m_generation(generation) {}

    // Accessors
    constexpr HandleId getId() const noexcept { return m_id; }
    constexpr Generation getGeneration() const noexcept { return m_generation; }

    // Validity check
    constexpr bool isValid() const noexcept {
        return m_id != INVALID_ID && m_generation != INVALID_GENERATION;
    }

    // Comparison operators
    constexpr bool operator==(const ResourceHandle& other) const noexcept {
        return m_id == other.m_id && m_generation == other.m_generation;
    }

    constexpr bool operator!=(const ResourceHandle& other) const noexcept {
        return !(*this == other);
    }

    constexpr bool operator<(const ResourceHandle& other) const noexcept {
        if (m_id != other.m_id)
            return m_id < other.m_id;
        return m_generation < other.m_generation;
    }

    // Hash support for containers
    std::size_t hash() const noexcept {
        return static_cast<std::size_t>(m_id) |
            (static_cast<std::size_t>(m_generation) << 32);
    }

    // String conversion for debugging
    std::string toString() const {
        if (!isValid())
            return "ResourceHandle::INVALID";
        return "ResourceHandle(" + std::to_string(m_id) + ":" +
            std::to_string(m_generation) + ")";
    }

private:
    HandleId m_id;
    Generation m_generation;
};

/**
 * @brief Invalid handle constant
 */
inline constexpr ResourceHandle INVALID_RESOURCE_HANDLE{};

} // namespace VoidLight

// Stream output operator for debugging and logging
inline std::ostream& operator<<(std::ostream& os,
    const VoidLight::ResourceHandle& handle) {
    return os << handle.toString();
}

// Hash function for std::unordered_map support
namespace std {
template <>
struct hash<VoidLight::ResourceHandle> {
    std::size_t
    operator()(const VoidLight::ResourceHandle& handle) const noexcept {
        return handle.hash();
    }
};
} // namespace std

#endif // RESOURCE_HANDLE_HPP
