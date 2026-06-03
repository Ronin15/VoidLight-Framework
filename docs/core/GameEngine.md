# GameEngine

**Code:** `include/core/GameEngine.hpp`, `src/core/GameEngine.cpp`

## Overview

`GameEngine` coordinates the main loop, display/window state, manager initialization, GameState execution, and the render/present split.

Rendering and timing details:

- display refresh is propagated into `TimestepManager`
- VSync is controlled through swapchain present mode

## Main Loop Shape

```cpp
ts.startFrame();
engine.handleEvents();

while (ts.shouldUpdate()) {
    engine.update(ts.getUpdateDeltaTime());
}

engine.render();
engine.present();
ts.endFrame();
```

`render()` performs scene/UI rendering. `present()` completes the frame. GameStates must not call `SDL_RenderPresent()` or `SDL_RenderClear()` directly.

## Display Refresh Propagation

`GameEngine::updateDisplayRefreshRate()` reads the active display mode and calls:

```cpp
m_timestepManager->setDisplayRefreshHz(refreshHz);
```

This does not change the fixed simulation rate. It allows `TimestepManager` to quantize render deltas against the real display cadence and reduce drift on VSync-paced systems.

## Rendering

- `getWidthInPixels()` / `getHeightInPixels()` are the authoritative dimensions
  for swapchain, viewport, scene texture, and UI layout work
- present pacing happens when the swapchain texture is acquired and the frame command buffer is submitted
- `GPURenderer` owns swapchain present mode and pass sequencing
- if the swapchain texture cannot be acquired for a frame, rendering can be skipped cleanly

## Platform Notes

### GPU backend preference

The engine requests the platform-native backend that matches the compiled shader binaries:

- Windows: Direct3D 12
- macOS: Metal
- Linux and other non-Apple Unix platforms: Vulkan

On macOS this keeps the GPU path aligned with current display refresh handling, including ProMotion-style refresh reporting when available.

### Resizing / display changes

Window/display changes feed back into:

- display refresh propagation to `TimestepManager`
- cached logical window size plus pixel-space viewport/UI updates
- swapchain-authoritative GPU viewport sizing
- font DPI refresh on platforms where logical and pixel sizes differ

`setVSyncEnabled(...)` only switches the runtime swapchain present mode. It does
not save settings; callers that expose a user setting must persist the requested
value through `SettingsManager`.

## Related Docs

- [TimestepManager](TimestepManager.md)
- [GPU Rendering](../gpu/GPURendering.md)
- [WorkerBudget](WorkerBudget.md)
