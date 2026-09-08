# Interpolation System

**Code:** `include/managers/EntityDataTypes.hpp` (`TransformData`), `src/entities/Entity.cpp`, `include/utils/Camera.hpp`, `src/utils/GPUSceneRecorder.cpp`, `src/core/VoidLightMain.cpp`

## Overview

Fixed-timestep simulation with a variable render rate interpolates between the previous and current transform so motion stays smooth on high-refresh displays.

Canonical positions live in EDM `TransformData` (`position` and `previousPosition`), not on `Entity` members. The main loop is `src/core/VoidLightMain.cpp` (`update` then `render`/`present`). There is no `HammerMain.cpp` and no `Player::render(SDL_Renderer*)` path.

## Why

Without interpolation, entities snap between discrete 60 Hz sim poses when the display is faster than the update rate.

```
Update: previousPosition = last pose, position = new pose
Render:  display = lerp(previousPosition, position, interpolationAlpha)
```

`GameEngine::render()` reads `TimestepManager::getInterpolationAlpha()` and passes it into `GameStateManager::recordGPUVertices`.

## Entity / EDM

```cpp
Vector2D Entity::getInterpolatedPosition(float alpha) const {
    const auto& transform = EntityDataManager::Instance().getTransform(m_handle);
    return Vector2D(
        transform.previousPosition.getX() +
            (transform.position.getX() - transform.previousPosition.getX()) * alpha,
        transform.previousPosition.getY() +
            (transform.position.getY() - transform.previousPosition.getY()) * alpha);
}

void Entity::storePositionForInterpolation() {
    auto& transform = EntityDataManager::Instance().getTransform(m_handle);
    transform.previousPosition = transform.position;
}
```

`setPosition` writes both `position` and `previousPosition` so teleports do not slide.

GPU recording (`GPUSceneRecorder`, NPC/projectile/player record paths) uses that interpolated pose plus `Camera::getRenderOffset()`. Zoom and sub-pixel offset are applied in the composite shader, not by scaling tiles.

## Overlay / pause

When an overlay (Pause, Settings-over-GamePlay) is recording the **underneath** world's scene, `GameStateManager` passes `interpolationAlpha = 1.0f`. Timestep still advances while paused; interpolating the last two sim poses would wobble. Alpha 1 draws the current frozen pose.

## Camera

Camera interpolation is documented in [Camera](../utils/Camera.md). GPU recording uses `getRenderOffset()` with a floored camera plus composite zoom/sub-pixel. Do not apply a second zoom at tile scale.

## Related docs

- [GPURendering](../gpu/GPURendering.md)
- [GameEngine](../core/GameEngine.md)
- [TimestepManager](../core/TimestepManager.md)
- [Camera](../utils/Camera.md)
