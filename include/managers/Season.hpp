/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef SEASON_HPP
#define SEASON_HPP

#include <cstdint>

/**
 * @brief Type-safe season enumeration
 */
enum class Season : uint8_t
{
    Spring = 0,
    Summer = 1,
    Fall = 2,
    Winter = 3
};

#endif // SEASON_HPP
