/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef SEX_HPP
#define SEX_HPP

#include <cstdint>

/**
 * @brief Biological sex for creatures
 */
enum class Sex : uint8_t {
    Male = 0,
    Female = 1,
    Unknown = 2 // For creatures where sex is undefined/irrelevant
};

#endif
