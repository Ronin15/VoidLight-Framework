# EventFactory

## Purpose

`EventFactory` constructs event objects from definitions or helper inputs. It is
not the runtime event registry.

Use it for tests and helper construction. Runtime
delivery goes through `EventManager`.

## Current Boundary

Use `EventFactory` when you need an event instance.

Use `EventManager` when you need to dispatch that event or trigger a gameplay reaction.

## Built-In Factory Types

`EventFactory` registers creators for the standard event definitions:

- `Weather`
- `SceneChange`
- `NPCSpawn`
- `MerchantSpawn`
- `ParticleEffect`
- `WorldLoaded`
- `WorldUnloaded`
- `TileChanged`
- `WorldGenerated`
- `CameraMoved`
- `CameraModeChanged`
- `CameraShake`
- `ResourceChange`

### Weather

`createWeatherEvent(name, weatherType, intensity = std::nullopt,
transitionTime = std::nullopt)` and the `Weather` definition creator build the
same event `EventManager::changeWeather(weatherType)` dispatches:

- `weatherType` goes through the `WeatherEvent` string constructor. The
  case-sensitive canonical names (`Clear`, `Cloudy`, `Rainy`, `Stormy`,
  `Foggy`, `Snowy`, `Windy`) select that `WeatherType`; any other name is
  `Custom` with the name kept. A definition without `weatherType` is `Clear`.
- Params are that type's defaults (`WeatherEvent::applyDefaultParamsForType()`).
- The only overrides are an explicit `intensity` (clamped to [0, 1]) and
  `transitionTime` (clamped to >= 0). Definitions read them from `numParams`
  only when present; nothing else is parsed. Intensity changes neither the
  particle variant (ParticleManager keys it by `WeatherType`) nor AI weather.
- There is no JSON event loader and no weather data under `res/` yet; the
  factory currently has no production caller; tests exercise it.

`createMerchantSpawnEvent(...)` creates a `MerchantSpawnEvent` with
`merchantClass`, `merchantRace`, `count`, and `spawnRadius` parameters.

## Practical Guidance

- prefer `EventManager` trigger helpers for normal gameplay paths
- prefer `EventFactory` when creating richer event objects from data-driven definitions
- if you build an event object manually, wrap it in `EventData` and dispatch through current `EventManager` APIs instead of relying on removed registration/storage APIs
