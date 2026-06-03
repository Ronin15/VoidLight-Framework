# EventFactory

## Purpose

`EventFactory` constructs event objects from definitions or helper inputs. It is
not the runtime event registry.

Use it for tests, scripted definitions, and helper construction. Runtime
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

`createMerchantSpawnEvent(...)` creates a `MerchantSpawnEvent` with
`merchantClass`, `merchantRace`, `count`, and `spawnRadius` parameters.

## Practical Guidance

- prefer `EventManager` trigger helpers for normal gameplay paths
- prefer `EventFactory` when creating richer event objects from data-driven definitions
- if you build an event object manually, wrap it in `EventData` and dispatch through current `EventManager` APIs instead of relying on removed registration/storage APIs
