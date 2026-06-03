# LogoState

**Code:** `include/gameStates/LogoState.hpp`, `src/gameStates/LogoState.cpp`

## Overview

`LogoState` is the startup presentation state. It pauses gameplay managers,
plays the logo sound effect, records GPU scene sprites for logo imagery, and
transitions to `GameStateId::MAIN_MENU` after its timer expires.

## Rendering

Logo imagery is scene-rendered, not UI-rendered:

- layout uses `GameEngine::getWidthInPixels()` and `getHeightInPixels()`
- sprite vertices are written into `GPURenderer`'s sprite vertex pool
- draw commands are submitted in the scene pass
- the state recalculates layout if pixel dimensions change

## Lifecycle

- `enter()` pauses managers, resets the timer, recalculates layout, and plays
  `sfx_logo`
- `update(dt)` advances the timer and changes to main menu when complete
- `exit()` has no UI cleanup because the state does not create UI components

## Related Docs

- [GPU Rendering](../gpu/GPURendering.md)
- [MainMenuState](MainMenuState.md)
