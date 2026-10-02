/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PATH_SMOOTHER_HPP
#define PATH_SMOOTHER_HPP

#include <vector>
#include <cmath>
#include "utils/Vector2D.hpp"

namespace VoidLight {

struct PathSmoother {
    // Removes collinear points; simple post-process
    static void simplify(std::vector<Vector2D>& path) {
        if (path.size() < 3) return;
        std::vector<Vector2D> out;
        out.reserve(path.size());
        out.push_back(path.front());
        for (size_t i = 1; i + 1 < path.size(); ++i) {
            Vector2D a = out.back();
            Vector2D b = path[i];
            Vector2D c = path[i + 1];
            Vector2D ab = b - a;
            Vector2D bc = c - b;
            // Check near collinearity via cross product ~ 0
            float cross = ab.getX() * bc.getY() - ab.getY() * bc.getX();
            if (std::fabs(cross) > 1e-3f) {
                out.push_back(b);
            }
        }
        out.push_back(path.back());
        path.swap(out);
    }
};

} // namespace VoidLight

#endif // PATH_SMOOTHER_HPP
