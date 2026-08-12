/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PATHFINDING_REQUEST_HPP
#define PATHFINDING_REQUEST_HPP

#include <vector>
#include <functional>
#include <cstdint>
#include "utils/Vector2D.hpp"
#include "entities/EntityHandle.hpp"  // EntityID
#include "ai/pathfinding/PathfindingGrid.hpp"

namespace VoidLight {

enum class RequestPriority : uint8_t { LOW, NORMAL, HIGH, CRITICAL };
enum class RequestStatus : uint8_t { PENDING, PROCESSING, COMPLETED, FAILED, CANCELLED };

struct PathfindingRequest {
    uint32_t requestId{0};
    EntityID entityId{0};
    Vector2D start; Vector2D goal;
    RequestPriority priority{RequestPriority::NORMAL};
    RequestStatus status{RequestStatus::PENDING};
    std::vector<Vector2D> path;
    PathfindingResult result{PathfindingResult::SUCCESS};
    std::function<void(const PathfindingRequest&)> onComplete; // optional
};

} // namespace VoidLight

#endif // PATHFINDING_REQUEST_HPP

