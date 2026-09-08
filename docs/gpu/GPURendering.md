# GPU Rendering

**Code:** `include/gpu/`, `src/gpu/`

## Overview

The SDL3 GPU path is built around `GPURenderer` and a two-pass scene-plus-swapchain flow. `GameEngine::render()` never calls `acquireSwapchainTexture()` itself; `GPURenderer::beginScenePass()` acquires on first use. See [ARCHITECTURE.md](../ARCHITECTURE.md) for the engine-owned pass order.

## Platform Shader Targets

The GPU build emits platform-native shader binaries and requests the matching SDL GPU backend at device creation time:

- Windows: Direct3D 12 with `.dxil` shaders
- macOS: Metal with `.metal` shaders
- Linux and other non-Apple Unix platforms: Vulkan with `.spv` shaders

Build-time shader tool requirements follow the same split:

- Linux: `glslangValidator`
- macOS: `glslangValidator`, `spirv-cross`
- Windows: `glslangValidator`, `spirv-cross`, `dxc`

## Frame Contract

Engine-visible sequence (`GameEngine::render` / `present`):

```text
beginFrame
GSM.recordGPUVertices()  // scene: highest hasGPUScene(); UI: stack top
beginScenePass           // acquires the swapchain texture on first use
GSM.renderGPUScene()     // highest hasGPUScene()
beginSwapchainPass
renderComposite          // scene texture -> swapchain; zoom / sub-pixel here
GSM.renderGPUUI()        // stack top
endFrame                 // GameEngine::present()
```

`acquireSwapchainTexture()` is internal to `beginScenePass()`. If acquisition fails, that frame is skipped cleanly. States never `endFrame`, submit, or present.

## Pass Layout

1. `beginFrame()` — command buffer, map upload buffers, copy/upload work
2. `beginScenePass()` — acquire swapchain, upload vertex pools, render world to the scene texture (`LOADOP_CLEAR`)
3. `beginSwapchainPass()` — composite scene texture, then UI on the swapchain
4. `endFrame()` — close the pass, submit (from `GameEngine::present()`)

## Important Branch Details

- UI text is atlas-backed through SDL3_ttf GPU draw sequences
- `GPURenderer::renderUIBatches()` owns SDL_GPU UI pipeline, sampler, vertex-buffer, and draw-call submission
- vertex pools are triple-buffered
- UI/menu text should be snapped to whole pixels before vertex emission
- `SpriteBatch::drawUVRotated(...)` draws atlas sub-rects rotated around their
  center. Angles are radians in engine screen space, where positive rotation is
  clockwise because engine/UI coordinates are Y-down; `SpriteBatch` converts
  vertices into the GPU Y-up scene coordinate system before submission.

## GameState Integration

GPU-capable states provide scene and/or UI hooks. `GameStateManager` records/renders **scene** from the highest stack state with `hasGPUScene()`, and **UI** from the top overlay:

```cpp
recordGPUSceneVertices(...)   // world / diorama
recordGPUUIVertices(...)      // UIManager (and UI-only states)
renderGPUScene(...)
renderGPUUI(...)
```

GameStates record and issue scene/UI work, but `GameEngine` and `GPURenderer` still own frame lifetime, swapchain lifetime, and presentation. Overlay pause keeps the underneath world's scene; it does not skip `LOADOP_CLEAR` or present from the state.
