/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_TYPES_HPP
#define GPU_TYPES_HPP

#include <cstdint>
#include <cstddef> // for offsetof

namespace VoidLight {

/**
 * Vertex format for textured sprites.
 */
struct SpriteVertex {
    float x, y; // Position (8 bytes)
    float u, v; // Texture coords (8 bytes)
    uint8_t r, g, b, a; // Color packed (4 bytes)
    // Total: 20 bytes per vertex
};

static_assert(sizeof(SpriteVertex) == 20, "SpriteVertex must be 20 bytes");

/**
 * Vertex format for colored primitives and particles.
 */
struct ColorVertex {
    float x, y; // Position (8 bytes)
    uint8_t r, g, b, a; // Color packed (4 bytes)
    // Total: 12 bytes per vertex
};

static_assert(sizeof(ColorVertex) == 12, "ColorVertex must be 12 bytes");

/**
 * View-projection uniform buffer data.
 */
struct ViewProjectionUBO {
    float viewProjection[16]; // 4x4 matrix
};

/**
 * Composite uniform buffer data.
 * Used for fullscreen composite pass with day/night lighting.
 */
struct CompositeUBO {
    float subPixelOffsetX;
    float subPixelOffsetY;
    float zoom;
    float _pad0;
    // Day/night ambient lighting (0-1 range)
    float ambientR;
    float ambientG;
    float ambientB;
    float ambientAlpha; // Blend strength: 0 = no tint, 1 = full tint
};

// Verify CompositeUBO layout matches std140 shader expectations
static_assert(sizeof(CompositeUBO) == 32, "CompositeUBO must be 32 bytes for std140");
static_assert(offsetof(CompositeUBO, zoom) == 8, "CompositeUBO::zoom must be at offset 8");
static_assert(offsetof(CompositeUBO, ambientR) == 16, "CompositeUBO::ambientR must be at offset 16");

} // namespace VoidLight

#endif // GPU_TYPES_HPP
