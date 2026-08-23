# Documentation Hub

## Overview

This hub covers the engine architecture and the major subsystem docs. Core
runtime architecture centers on `EventManager` event flow, EDM-backed AI state,
controller-owned harvesting and social flows, world-resource orchestration, and
the SDL3_GPU rendering path.

## Core Systems

- [GameEngine](core/GameEngine.md)
- [ThreadSystem](core/ThreadSystem.md)
- [WorkerBudget](core/WorkerBudget.md)
- [TimestepManager](core/TimestepManager.md)
- [ARCHITECTURE](ARCHITECTURE.md)
- [InputManager](managers/InputManager.md)

## AI and Events

- [AIManager](ai/AIManager.md)
- [Behavior Execution Pipeline](ai/BehaviorExecutionPipeline.md)
- [Behavior Modes](ai/BehaviorModes.md)
- [Behavior Quick Reference](ai/BehaviorQuickReference.md)
- [NPC Memory](ai/NPCMemory.md)
- [Pathfinding System](ai/PathfindingSystem.md)
- [EventManager](events/EventManager.md)
- [EventManager Quick Reference](events/EventManager_QuickReference.md)
- [EventManager Advanced](events/EventManager_Advanced.md)
- [EventFactory](events/EventFactory.md)
- [Time Events](events/TimeEvents.md)

## Controllers and World Systems

- [Controllers Overview](controllers/README.md)
- [CombatController](controllers/CombatController.md)
- [HudController](controllers/HudController.md)
- [InventoryController](controllers/InventoryController.md)
- [HarvestController](controllers/HarvestController.md)
- [SocialController](controllers/SocialController.md)
- [WorldManager](managers/WorldManager.md)
- [WorldResourceManager](managers/WorldResourceManager.md)
- [ProjectileManager](managers/ProjectileManager.md)

## Rendering and UI

- [GPU Rendering](gpu/GPURendering.md)
- [UIManager Guide](ui/UIManager_Guide.md)
- [UIConstants Reference](ui/UIConstants.md)
- [Auto-Sizing System](ui/Auto_Sizing_System.md)
- [DPI-Aware Font System](ui/DPI_Aware_Font_System.md)
- [Minimap Implementation](ui/Minimap_Implementation.md)

## Entities and GameStates

- [Entity System](entities/README.md)
- [GameState Documentation](gameStates/README.md)
- [LogoState](gameStates/LogoState.md)
- [MainMenuState](gameStates/MainMenuState.md)
- [GamePlayState](gameStates/GamePlayState.md)
- [LoadingState](gameStates/LoadingState.md)
- [PauseState](gameStates/PauseState.md)
- [SettingsMenuState](gameStates/SettingsMenuState.md)
- [GameOverState](gameStates/GameOverState.md)

## Utilities

- [FrameProfiler](utils/FrameProfiler.md)
- [Camera](utils/Camera.md)
- [JsonReader](utils/JsonReader.md)
- [JSON Resource Loading Guide](utils/JSON_Resource_Loading_Guide.md)
- [MenuNavigation](utils/MenuNavigation.md)
- [Serialization](utils/SERIALIZATION.md)
- [ResourceHandle System](utils/ResourceHandle_System.md)

## Architecture

- [Architecture Overview](ARCHITECTURE.md)
- [Interpolation System](architecture/InterpolationSystem.md)
- [Implementation slices (workflow and gates)](framework-implementation-slices.md)

## Performance & Development

- [Power Efficiency](performance/PowerEfficiency.md)
- [SDL3 macOS Cleanup Issue](issues/SDL3_MACOS_CLEANUP_ISSUE.md)
- [AGENTS.md](../AGENTS.md)

## Testing and Validation

- [`tests/TESTING.md`](../tests/TESTING.md)
- [`tests/valgrind/README.md`](../tests/valgrind/README.md)
- [`tests/cppcheck/README.md`](../tests/cppcheck/README.md)
- [`tests/clang-tidy/README.md`](../tests/clang-tidy/README.md)
- [`tools/README.md`](../tools/README.md)

Validation coverage includes behavior transitions, NPC memory, controller tests,
input command tests, sidecar tests, GPU frame timing benchmarks, atlas tool
tests, static-analysis helper scripts, and the retained runtime/test Valgrind
split.

## Notes

- WorkerBudget docs describe threshold learning and hysteresis. A 10-sample EMA warmup prevents cold-start spikes from triggering premature threading decisions.
- InputManager `getAxisX`/`getAxisY` return normalized `float` in [-1.0, 1.0]. Gamepad hotplugging is handled automatically.
- `TimestepManager` documentation lives under `docs/core/` and covers display-refresh-aware cadence snapping.
- Performance reports under `docs/performance_reports/` should be treated as dated measurements unless their metadata explicitly matches the checked-out commit and local environment.
