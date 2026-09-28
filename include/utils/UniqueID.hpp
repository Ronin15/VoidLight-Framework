/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef UNIQUE_ID_HPP
#define UNIQUE_ID_HPP

#include <atomic>
#include <cstdint>

namespace VoidLight {
    /**
     * @brief A thread-safe generator for unique 64-bit identifiers.
     *
     * This class provides a simple way to get unique IDs throughout the
     * application's lifetime. It uses a static atomic counter to ensure
     * uniqueness even in multi-threaded environments.
     */
class UniqueID {
public:
    using IDType = uint64_t;

        /**
         * @brief Generates a new unique ID.
         * @return A new, unique 64-bit integer.
         */
    static IDType generate() {
            // Atomically increment the counter and return the new value.
            // The first ID generated will be 1.
        return m_nextID++;
    }

        /**
         * @brief A constant representing an invalid or uninitialized ID.
         */
    static constexpr IDType INVALID_ID = 0;

private:
        // Static atomic counter to ensure thread-safe ID generation.
        // Starts at 1, so that INVALID_ID (0) is never generated.
    static inline std::atomic<IDType> m_nextID{1};
};

} // namespace VoidLight

#endif // UNIQUE_ID_HPP
