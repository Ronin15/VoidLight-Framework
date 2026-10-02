# DayNightController

**Code:** `include/controllers/world/DayNightController.hpp`, `src/controllers/world/DayNightController.cpp`, `tests/controllers/DayNightControllerTests.cpp`

**Ownership:** The owning GameState adds it with `m_controllers.add<DayNightController>()`. It is not a singleton and not a `GamePlayState` member.

## Overview

Tracks Morning / Day / Evening / Night from `HourChangedEvent`, dispatches `TimePeriodChangedEvent` on period change, and interpolates GPU ambient lighting each frame. Period bounds come from free `hourToTimePeriod()` in `include/events/TimeEvent.hpp` (not a controller-local copy).

`update(dt)` is required every frame. DayNight is **not** `IUpdatable`; `updateAll()` will not tick it. GamePlay calls `m_controllers.get<DayNightController>()->update(deltaTime)` explicitly.

## Event flow

```
GameTimeManager::dispatchTimeEvents()
  → HourChangedEvent (Deferred)
    → DayNightController detects period change
      → sets target lighting
      → dispatches TimePeriodChangedEvent
    → update(dt) interpolates current toward target
      → GPURenderer::setDayNightParams(...)
        → composite shader tints the scene
```

UI composites after the scene, so buttons/text stay legible.

## Update pattern

```cpp
// GamePlayState::enter()
m_controllers.add<DayNightController>();
m_controllers.subscribeAll();   // DayNightController::subscribe()

// GamePlayState::update()
if (auto* dayNight = m_controllers.get<DayNightController>()) {
    dayNight->update(deltaTime);
}
```

`subscribe()` writes the initial GPU values. `transitionToPeriod()` only sets targets; interpolation and `setDayNightParams` run in `update(dt)`.

Transition duration is 30 seconds for a full period change.

## GPU path

There is no `USE_SDL3_GPU` branch and no SDL_Renderer overlay fill. Production is GPU-only:

```cpp
GPURenderer::Instance().setDayNightParams(r, g, b, a);
```

Composite shader:

```glsl
vec3 tinted = mix(scene.rgb, scene.rgb * ambientColor.rgb, ambientColor.a);
```

MainMenu may set composite lighting directly for a menu grade and must reset it on `exit()`. EventDemo (and any other state that registers DayNight) must call `update(dt)` or drop the controller.

## Related docs

- [GPURendering](../gpu/GPURendering.md)
- [GamePlayState](../gameStates/GamePlayState.md)
- [GameTimeManager](../managers/GameTimeManager.md)
