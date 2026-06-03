# Manager Index

This directory indexes manager-focused documentation. Some major systems live in sibling folders because their docs are large enough to deserve their own section.

## Core Manager Pages

- [BackgroundSimulationManager](BackgroundSimulationManager.md)
- [EntityDataManager](EntityDataManager.md)
- [EntityStateManager](EntityStateManager.md)
- [FontManager](FontManager.md)
- [GameStateManager](GameStateManager.md)
- [InputManager](InputManager.md)
- [ProjectileManager](ProjectileManager.md)
- [PathfinderManager](PathfinderManager.md)
- [ParticleManager](ParticleManager.md)
- [ResourceFactory](ResourceFactory.md)
- [ResourceTemplateManager](ResourceTemplateManager.md)
- [SaveGameManager](SaveGameManager.md)
- [SettingsManager](SettingsManager.md)
- [SoundManager](SoundManager.md)
- [TextureManager](TextureManager.md)
- [WorldManager](WorldManager.md)
- [WorldResourceManager](WorldResourceManager.md)

## Manager Docs Hosted Elsewhere

- [AIManager](../ai/AIManager.md)
- [EventManager](../events/EventManager.md)
- [UIManager](../ui/UIManager_Guide.md)
- [GameEngine](../core/GameEngine.md)
- [WorkerBudget](../core/WorkerBudget.md)
- [TimestepManager](../core/TimestepManager.md)

## Notes

- `WorldResourceManager` is a registry over EDM data, not a quantity store.
- `WorldManager` explicitly coordinates active-world setup and harvestable spawning.
- `TimestepManager` documentation lives in `docs/core/` for consistency with other timing docs.

For the broader system map, see [docs/README.md](../README.md).
