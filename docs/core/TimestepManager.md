# TimestepManager

**Code:** `include/core/TimestepManager.hpp`, `src/core/TimestepManager.cpp`

## Overview

`TimestepManager` owns fixed-update timing plus render cadence handling. Display refresh is propagated explicitly through:

```cpp
setDisplayRefreshHz(float hz);
```

The fixed simulation rate remains unchanged; refresh-rate input is used to better quantize VSync-paced render deltas.

## Core Loop

```cpp
startFrame();
while (shouldUpdate()) {
    update(getUpdateDeltaTime());
}
if (shouldRender()) {
    render(getInterpolationAlpha());
}
endFrame();
```

## Key APIs

```cpp
setTargetFPS(float fps);
setFixedTimestep(float timestep);
setDisplayRefreshHz(float hz);
setSoftwareFrameLimiting(bool enabled);
reset();
```

Useful accessors:

```cpp
getCurrentFPS()
getFrameTimeMs()
getInterpolationAlpha()
isFrameTimeExcessive()
```

## Why Refresh Propagation Matters

On high-refresh or variable-refresh displays, the real display cadence may differ from the nominal target FPS. Feeding the active refresh rate into `TimestepManager` reduces drift and micro-jitter without changing gameplay update frequency.
