/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "events/CameraEvent.hpp"

std::string CameraModeChangedEvent::getModeString(Mode mode) const {
    switch (mode) {
        case Mode::Free: return "Free";
        case Mode::Follow: return "Follow";
        case Mode::Fixed: return "Fixed";
        default: return "Unknown";
    }
}
