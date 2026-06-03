# Camera

**Code:** `include/utils/Camera.hpp`, `src/utils/Camera.cpp`

## Overview

`Camera` is a non-singleton 2D camera with free, follow, and fixed modes, zoom levels, world clamping, and coordinate conversion helpers.

In follow mode, coordinate conversions use the last rendered center rather than the raw live position.

## Follow Mode

Follow mode combines:

- subtle damping / smoothing toward the target
- `m_lastRenderedCenter` caching
- conversion helpers tied to what was actually rendered last frame
- configurable `followLag`, `deadZoneRadius`, and `maxCatchupDistance`

Why this matters:

- screen-to-world and world-to-screen conversions stay aligned with the viewport the player saw
- click/interaction coordinates stay aligned while the camera is mid-smoothing

## Coordinate Conversion

In follow mode:

- `worldToScreen(...)` uses `m_lastRenderedCenter`
- `screenToWorld(...)` uses `m_lastRenderedCenter`

In free/fixed mode:

- conversions use the current camera position directly

The rendered camera center is the authoritative conversion source for smoothed follow cameras.

## Follow Tuning

`Camera::Config` owns the follow-mode tuning contract:

- `followLag`: time constant in seconds. `0.0f` snaps to the target, smaller
  values are tighter, and larger values create a more visible trail.
- `deadZoneRadius`: pixel radius around the current camera center where target
  motion is ignored. This suppresses idle shimmer.
- `maxCatchupDistance`: maximum pixel distance the target can get ahead before
  the camera snaps closer. This prevents teleports or fast dashes from leaving
  the camera far behind.

Config validation rejects negative tuning values, empty zoom lists, non-positive
zoom levels, and invalid default zoom indices.

## Practical Guidance

- call `update(deltaTime)` every frame before rendering
- keep viewport dimensions synchronized with the current pixel-space viewport
- if you snap or directly reposition the camera, the rendered-center cache is synchronized accordingly

## Related Docs

- [GameEngine](../core/GameEngine.md)
- [GPU Rendering](../gpu/GPURendering.md)
