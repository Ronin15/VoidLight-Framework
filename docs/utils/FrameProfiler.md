# FrameProfiler

**Code:** `include/utils/FrameProfiler.hpp`, `src/utils/FrameProfiler.cpp`

## Overview

`FrameProfiler` is a timing system (enabled by `VOIDLIGHT_FRAME_PROFILER_ENABLED`, set for Debug and ReleaseSafe builds) with three levels of breakdown:

- frame phases
- manager phases
- render phases

It powers hitch logging (frames above the 20 ms default threshold) and the F3 overlay. In Release and Profile builds it compiles to no-op stubs.

## Current Render Phases

The render-phase enum now includes both classic and GPU-specific phases:

- `BeginScene`
- `WorldTiles`
- `Entities`
- `EndScene`
- `UI`
- `GPUCmdBuffer`
- `GPUSwapchainWait`
- `GPUVertexMap`
- `GPUCopyPass`
- `GPUUpload`
- `GPUScenePass`
- `GPUSwapPass`
- `GPUSubmit`

`GPUSwapchainWait` is treated as expected pacing time rather than a normal render-cost spike when evaluating hitches.

## Key Accessors

```cpp
double getLastFrameTimeMs() const;
double getPhaseTimeMs(FramePhase phase) const;
double getManagerTimeMs(ManagerPhase mgr) const;
double getRenderTimeMs(RenderPhase phase) const;
```

`getRenderTimeMs()` is the new important accessor for tooling and debug overlays that want the granular render-phase values directly.

## Hitch Reporting

The overlay and hitch details now keep track of the worst render sub-phase, not just the top-level render bucket. That makes render/present bottlenecks easier to attribute on the GPU path.

## Usage

Use the scoped helpers or macros:

- `PROFILE_PHASE(...)`
- `PROFILE_MANAGER(...)`
- `PROFILE_RENDER(...)`
- `PROFILE_RENDER_GPU(...)` (GPU-aware render phase timer)

The RAII types behind them are `ScopedPhaseTimer`, `ScopedManagerTimer`, `ScopedRenderTimer`, and `ScopedRenderTimerGPU`.

Call `suppressFrames()` around known heavy transitions such as loading or state changes to avoid noisy hitch logs.
