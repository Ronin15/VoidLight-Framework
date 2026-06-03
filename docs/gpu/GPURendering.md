# GPU Rendering

**Code:** `include/gpu/`, `src/gpu/`

## Overview

The SDL3 GPU path is built around `GPURenderer` and a two-pass scene-plus-swapchain flow. Swapchain acquisition is explicit instead of hidden inside a monolithic frame begin.

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

```cpp
bool beginFrame();
bool acquireSwapchainTexture();
SDL_GPURenderPass* beginScenePass();
SDL_GPURenderPass* beginSwapchainPass();
void endFrame();
```

If `acquireSwapchainTexture()` fails, the engine can skip the presentable frame cleanly.

## Pass Layout

1. `beginFrame()`
   - acquire command buffer
   - map upload buffers
   - begin copy/upload work
2. `acquireSwapchainTexture()`
   - acquire the current swapchain texture
3. `beginScenePass()`
   - render world content to the intermediate scene texture
4. `beginSwapchainPass()`
   - composite the scene texture
   - render UI directly to the swapchain
5. `endFrame()`
   - close the active pass
   - submit the command buffer

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

GPU-capable states usually provide:

```cpp
recordGPUVertices(...)
renderGPUScene(...)
renderGPUUI(...)
```

GameStates record and issue scene/UI work, but `GameEngine` and `GPURenderer` still own frame lifetime, swapchain lifetime, and presentation.
