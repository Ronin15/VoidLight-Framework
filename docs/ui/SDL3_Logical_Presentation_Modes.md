# SDL3 GPU Display Coordinates

## Overview

VoidLight uses SDL3_GPU and pixel-space UI layout. SDL_Renderer logical
presentation modes are not part of the active rendering path. The active display
model is:

- SDL window size for logical window metrics
- SDL pixel size and pixel density for GPU viewport/UI coordinates
- `GameEngine::getWidthInPixels()` and `getHeightInPixels()` for swapchain,
  scene texture, camera viewport, and UI layout
- `InputManager` mouse coordinate scaling from SDL window coordinates into
  pixel-space UI/gameplay coordinates

## Runtime Coordinate Contract

Use pixel-space dimensions for UI and rendering work:

```cpp
auto& engine = GameEngine::Instance();
int width = engine.getWidthInPixels();
int height = engine.getHeightInPixels();
```

Use `UIManager::getWidthInPixels()` / `getHeightInPixels()` in UI code when the
manager is already the local subsystem boundary.

## Mouse Coordinates

SDL mouse events arrive in window coordinates. `InputManager` scales them by the
window pixel density:

```cpp
const Vector2D& mouse = InputManager::Instance().getMousePosition();
```

Consumers should use `getMousePosition()` instead of directly reading raw SDL
mouse coordinates. UI hit testing and camera `screenToWorld(...)` expect the
scaled coordinate space.

## Resize and DPI Flow

Window and display events are routed by `GameEngine`, which refreshes:

- logical window size
- pixel size
- font DPI scale
- display refresh rate
- UI relayout through `UIManager::onWindowResize(...)`
- font reload through `FontManager::reloadFontsForDisplay(...)`

`InputManager` does not own resize/display-system updates.

## Related Docs

- [GameEngine](../core/GameEngine.md)
- [InputManager](../managers/InputManager.md)
- [UIManager Guide](UIManager_Guide.md)
- [DPI-Aware Font System](DPI_Aware_Font_System.md)
