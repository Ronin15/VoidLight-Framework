/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef TRIGGER_TAG_HPP
#define TRIGGER_TAG_HPP

#include <cstdint>

namespace VoidLight {

// Trigger behavior type - determines collision processing path
enum class TriggerType : uint8_t {
    EventOnly = 0,  // Water, area triggers - skip broadphase, events only
    Physical = 1    // Bombs, pushables - full broadphase + resolution + events
};

// Enum tags for world trigger volumes. Extend as needed.
enum class TriggerTag : uint8_t {
    None = 0,
    Door,
    Checkpoint,
    Water,
    Lava,
    Portal,
    AreaEnter,
    AreaExit,
    Rock,         // Movement penalty trigger
    Tree,         // Movement penalty trigger
    Custom1,
    Custom2
};

} // namespace VoidLight

#endif // TRIGGER_TAG_HPP
