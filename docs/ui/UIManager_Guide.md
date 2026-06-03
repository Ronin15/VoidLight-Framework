# UIManager Guide

**Code:** `include/managers/UIManager.hpp`, `src/managers/UIManager.cpp`

## Overview

`UIManager` is the engine's main-thread UI system. It owns component creation, layout, theming, animation, input routing, and frame-local UI render batches. SDL_GPU device objects, pipelines, samplers, command buffers, render passes, and vertex pools stay owned by the GPU architecture.

Core contracts:

- GPU text is atlas-backed through SDL3_ttf draw sequences
- raster text placement is snapped to whole pixels before vertex emission
- combat HUD helpers are part of the public API
- UI update belongs in `GameState::update()`, not in `render()`

## Basic Pattern

```cpp
void SomeState::update(float dt) {
    auto& ui = UIManager::Instance();
    ui.update(dt);
}

void SomeState::recordGPUVertices(VoidLight::GPURenderer& gpu, float) {
    UIManager::Instance().recordGPUVertices(gpu);
}

void SomeState::renderGPUUI(VoidLight::GPURenderer& gpu,
                            SDL_GPURenderPass* swapchainPass) {
    UIManager::Instance().renderGPU(gpu, swapchainPass);
}
```

## GPU Text Path

On the GPU renderer path:

- text draw data comes from `TTF_GetGPUTextDrawData()`
- `UIManager` records atlas-backed text quads into the UI vertex pool
- the renderer selects alpha or SDF pipelines based on the draw sequence type

Guidelines:

- do not generate one GPU texture per UI label
- do not apply UV flips or half-texel offsets
- snap integer-layout text positions before emitting vertices

## Combat HUD Helpers

Built-in combat HUD helpers are used by gameplay/demo states:

```cpp
createCombatHUD();
updateCombatHUD(playerHealth, playerStamina, hasTarget, targetName, targetHealth);
destroyCombatHUD();
```

Use them from states that own combat flow instead of rebuilding the same health/stamina/target frame wiring by hand.

## Input Focus and Simulated Clicks

`UIManager` tracks keyboard/controller selection separately from mouse hover. Shared menu states use `MenuNavigation::applySelection()` to set the current keyboard selection only after controller navigation is active, keeping mouse-only hover behavior clean.

`simulateClick()` queues the component callback. Tests and state code that depend on the callback having run should call `UIManager::update(...)` after simulating the click.

## Parent and Child Components

Many UI creation APIs accept an optional `parentId`. Parent/child links provide:

- cascading visibility
- cascading removal
- backdrop inheritance for labels/titles inside panels or dialogs
- child z-order elevation above the parent backdrop

Children inside a parent backdrop suppress redundant default text backgrounds,
so dialog and panel text does not draw a second opaque rectangle over the parent.
Remove the parent component to remove its linked children.

## Images and Atlas Icons

Use the atlas-backed image APIs for inventory, hotbar, and other resource icons instead of creating ad-hoc texture ownership in controllers. Controllers provide the component ID and texture/resource identity; `UIManager` owns component rendering.

`UIManager` does not own GPU textures. It resolves frame-local texture references from `TextureManager` or SDL3_ttf draw data while recording UI batches.

For transparent or steady-looking interactive components, set `hoverColor` and `pressedColor` to match the base background color. Otherwise the UI will visibly flash on hover/press even if the component appears transparent at rest.

## Positioning

After creating components, call `setComponentPositioning()` when the element must survive resize/fullscreen changes. Prefer the existing helpers when they fit:

- `createTitleAtTop()`
- `createButtonAtBottom()`
- `createCenteredButton()`
- `createCenteredDialog()`

## Rendering Notes

- `recordGPUVertices()` records UI primitives, images, and text into renderer-owned GPU vertex pools
- `renderGPU()` hands frame-local UI batches to `GPURenderer::renderUIBatches()`
- `GPURenderer` submits fixed render-family batches during the swapchain UI pass: primitives, then images, then text
- component z-order controls input priority and ordering inside each render family; it is not arbitrary cross-family visual layering
- modal overlays use explicit render occlusion to skip lower normal UI before batches are emitted
- the engine, not the state, owns clear/present

## Related Docs

- [GPU Rendering](../gpu/GPURendering.md)
- [SDL3 GPU Display Coordinates](SDL3_Logical_Presentation_Modes.md)
