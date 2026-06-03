# VoidLight-Framework

A modern, production-ready C++20 SDL3 game engine template for 2D games. Built for rapid prototyping and scalable game development, it features Data-Oriented Design with EntityDataManager as the central data authority, robust multi-threading, high-performance AI supporting 10K+ entities, a professional UI system, and comprehensive resource, world, and event systems. Designed for cross-platform deployment (Windows, macOS, Linux) with a focus on performance, safety, and extensibility.

## Key Features

- **Modern C++20 & SDL3 Core**

    Clean, modular codebase with strict style, memory, and type safety.

- **Rendering & Engine Core**

    Fixed timestep game loop with smooth interpolation at any display refresh rate. VSync-aware frame pacing, sprite sheet animations, particle effects with camera-aware culling, and sub-pixel smooth scrolling. Platform-native GPU shaders (Metal/macOS, DXIL/Windows, SPIR-V/Linux) with day/night ambient lighting effects.

- **Adaptive Multi-Threading System**

   Hardware-adaptive thread pool with intelligent WorkerBudget batch optimization. Automatically detects logical cores (including SMT/hyperthreading) and reserves one to reduce OS contention. Sequential manager execution gives each system ALL workers during its update window. Priority-based scheduling and adaptive batch sizing help the engine scale cleanly across hardware.

- **High-Performance AI System**

    Data-Oriented Design with EntityDataManager as single source of truth. Cache-friendly, lock-free, batch-processed AI supports 10K+ entities at 60+ FPS with simulation tiers (Active/Background/Hibernated), rich behaviors, dense per-behavior state pools, sparse transient sidecars, and scalable pathfinding and combat integration.

- **Robust Event & State Management**  
    
    Event-driven architecture centered on `EventManager` as the dispatch hub, with immediate/deferred dispatch modes, persistent and transient handler lifetimes, pooled hot-path events, merchant/NPC spawn helpers, entity/game state machines, and thread-safe manager updates.

- **Flexible UI System**

    Content-aware auto-sizing, professional theming (light/dark plus programmatic custom themes), and rich component library (buttons, labels, input fields, lists, modals, etc.). Responsive layouts with DPI-aware rendering and animation support. Centralized UI constants with resolution-aware scaling (1920×1080 baseline) and event-driven resize handling. Optimized for PC handheld devices (Steam Deck, ROG Ally, OneXPlayer) with automatic baseline resolution scaling down to 1280×720.

- **Action-Mapped Input & Menu Navigation**

    Rebindable command system for keyboard, mouse, and controller input with JSON persistence, category-specific binding capture, controller-aware menu focus, shared menu navigation helpers, and a Controls settings tab for player-facing remapping.

- **Data-Driven Resource Management**  
  
    JSON-based resource loading for items, equipment, consumables, currency, merchant goods, and gameplay content. Handle-based runtime access and EDM-backed inventories keep systems data-driven, performant, and extensible.

- **Inventory, Equipment & Trading**

    State-scoped inventory and HUD controllers support pickup, gear equip/unequip, inventory slot reordering, hotbar assignment, event-driven inventory refresh, merchant buy/sell UI, relationship-aware pricing, gifts, theft reporting, and NPC memory integration.

- **Fast, Safe Serialization**
  
    Header-only binary serialization system with smart pointer memory management. Used by SaveGameManager for robust, versioned save/load across platforms.

- **Comprehensive Testing & Analysis**

    80 core source-controlled Boost.Test executables plus 8 GPU-specific test targets covering unit, integration, and performance testing. Includes AI+Collision integration tests, GPU rendering tests, SIMD correctness validation, NPC memory coverage, and comprehensive thread safety verification with documented TSAN suppressions. Static analysis (cppcheck, clang-tidy), AddressSanitizer (ASAN), ThreadSanitizer (TSAN), and Valgrind integration support production-ready quality assurance.

- **Debug Profiling Tools**

    Built-in frame profiler (F3 toggle) with live timing overlay and automatic hitch detection. Zero overhead in Release builds.

- **Release Logging**

    In Release builds, CRITICAL and ERROR messages are written to a timestamped log file. All other levels are compiled out entirely. Log files are written under the OS user data directory (`~/Library/Application Support/HammerForgedGames/VoidLight_Template/logs/` on macOS, `%APPDATA%\HammerForgedGames\VoidLight_Template\logs\` on Windows, `~/.local/share/HammerForgedGames/VoidLight_Template/logs/` on Linux). Files are named `voidlight_YYYYMMDD_HHMMSS.log` and the 5 most recent are kept automatically. See [Logger](docs/utils/Logger.md) for details.

- **Cross-Platform Optimizations**

    Unified codebase with platform-specific enhancements: SIMD acceleration (x86-64: SSE2/AVX2, ARM64: NEON), macOS Retina support, Wayland detection, VSync-aware frame pacing, and native DPI scaling.

- **GameTime & World Simulation**

    Fantasy calendar system with day/night cycles, four seasons, dynamic weather, and temperature simulation. Chunk-based world support includes procedural generation, streaming, and resource interactions.

- **Chunk-Based World System**

    Efficient tile-based world rendering with chunk culling for off-screen optimization. Supports procedural generation, seamless streaming, and automatic seasonal tile switching.

- **Robust Combat System**

    Dedicated combat, harvesting, projectile, inventory, and social/trading controllers support hit detection, knockback, resource gathering, theft/gift flows, and gameplay-specific state transitions without bloating core engine systems.

- **Power Efficient (Race-to-Idle)**

    Optimized for battery-powered devices. Completes frame work quickly then sleeps until vsync, achieving 80%+ CPU idle residency during active gameplay. See [Power Efficiency](docs/performance/PowerEfficiency.md) for detailed benchmarks.

- **Extensive Documentation**  
    
    Full guides, API references, best practices, and troubleshooting for all major systems.

### Why Choose VoidLight-Framework?

- **Performance**: Engineered for cache efficiency, lock-free concurrency, and minimal CPU overhead—even with thousands of entities.
- **Safety**: Smart pointers, RAII, strong typing, and robust error handling throughout.
- **Extensibility**: Modular managers, clear APIs, and easy resource and UI customization.
- **Developer Experience**: Clean code, strict style, automated testing, and comprehensive docs.
- **Production-Ready Design**: Architecture and tooling designed for serious game development, with comprehensive testing infrastructure and performance validation.

---

## Quick Start

### Prerequisites

- CMake 3.28+, Ninja, C++20 compiler (GCC/Clang) - MSVC support planned
- Platforms: Linux, macOS (Apple Silicon), Windows (MinGW)
- [SDL3 dependencies](https://wiki.libsdl.org/SDL3/README-linux) (ttf, mixer)
- Boost (for tests), cppcheck & clang-tidy (static analysis), Valgrind (optional, Linux only)
- Platform shader tools (required only when regenerating checked-in shader binaries):
  - Linux: `glslangValidator` for Vulkan SPIR-V shaders
  - macOS: `glslangValidator` + `spirv-cross` for Metal shaders
  - Windows: `glslangValidator` + `spirv-cross` + `dxc` for Direct3D 12 DXIL shaders

**Platform notes:**  
See the [documentation hub](docs/README.md) and subsystem docs for current setup details and platform-specific notes.

### Build

```bash
git clone https://github.com/yourname/VoidLight-Framework.git
cd VoidLight-Framework

# Debug build (recommended for development)
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build

# Release build (optimized)
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build

# Run the engine
./bin/debug/VoidLight_Template   # Debug build
./bin/release/VoidLight_Template # Release build
```

When `ccache` is installed, CMake enables it by default for C and C++ compilation. Disable it with `-DUSE_CCACHE=OFF` if you need uncached compiler invocations.

> Platform shader tools are needed to rebuild shader binaries from source. Without them, CMake uses the checked-in shader binaries under `res/shaders/` where available.

**Sanitizer builds** (for debugging memory/thread issues):
```bash
# AddressSanitizer (memory errors, leaks)
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" -DUSE_MOLD_LINKER=OFF && ninja -C build

# ThreadSanitizer (data races, deadlocks)
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG -fsanitize=thread -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" -DUSE_MOLD_LINKER=OFF && ninja -C build

# TSAN suppressions (for known benign races)
export TSAN_OPTIONS="suppressions=$(pwd)/tests/tsan_suppressions.txt"
```

**Note:** ASAN and TSAN are mutually exclusive. To reconfigure without a full rebuild: `rm build/CMakeCache.txt` before running cmake.

---

## Testing & Static Analysis

- Run all tests: `./tests/test_scripts/run_all_tests.sh`
- See [tests/TESTING.md](tests/TESTING.md) for comprehensive test documentation and options
- Static analysis:
  - `cppcheck`: `./tests/test_scripts/run_cppcheck_focused.sh`
    See [tests/cppcheck/README.md](tests/cppcheck/README.md) for more.
  - `clang-tidy`: `./tests/clang-tidy/clang_tidy_focused.sh`
    See [tests/clang-tidy/README.md](tests/clang-tidy/README.md) for configuration, focused analysis, and full-project analysis details.
- Memory & thread safety validation: AddressSanitizer (ASAN) and ThreadSanitizer (TSAN) support
  See [docs/core/ThreadSystem.md#threadsanitizer-tsan-support](docs/core/ThreadSystem.md#threadsanitizer-tsan-support) for details

### Valgrind Diagnostics
- Targeted Valgrind diagnostics for deeper heap/lifetime checks and performance profiling
- Memcheck over high-risk ownership/lifecycle tests: `./tests/valgrind/quick_memory_check.sh`
- Cachegrind test profiling: `./tests/valgrind/cache_performance_analysis.sh`
- Callgrind function profiling: `./tests/valgrind/callgrind_profiling_analysis.sh ai`
- **Runtime diagnostics with Profile build** (Valgrind-compatible optimized):
  ```bash
  cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Profile && ninja -C build
  ./tests/valgrind/runtime_cache_analysis.sh --profile 300   # MPKI analysis
  ./tests/valgrind/runtime_memory_analysis.sh --profile 300  # Memory analysis
  ```
- ASAN/TSAN remain the primary fast memory and race gates; Valgrind is for targeted deeper diagnostics
- See [tests/valgrind/README.md](tests/valgrind/README.md) for target policy and usage

---

## Sprite Atlas Workflow

The repository includes an atlas management tool for extracting sprites, assigning texture IDs, and repacking the atlas.

### Process

```bash
# 1. Extract sprites from the current atlas
python3 tools/atlas_tool.py extract

# 2. Map extracted sprites to texture IDs
python3 tools/atlas_tool.py map

# 3. Repack the atlas and update atlas coordinates
python3 tools/atlas_tool.py pack
```

- `extract` pulls sprite regions from `res/img/atlas.png` into `res/sprites/`
- `map` starts a local browser-based mapper and applies sprite renames immediately
- `pack` rebuilds `res/img/atlas.png`, updates `res/data/atlas.json`, and cleans up temporary sprite files

Use `extract-from` to import sprites from an external image instead of the current atlas:

```bash
python3 tools/atlas_tool.py extract-from path/to/source.png
```

For the full workflow, command reference, file locations, and seasonal texture generation details, see [tools/README.md](tools/README.md).

---

## Documentation

**📚 [Documentation Hub](docs/README.md)** – Full guides, API references, and best practices.

- **Core:** [GameEngine](docs/core/GameEngine.md), [GameTimeManager](docs/managers/GameTimeManager.md), [ThreadSystem](docs/core/ThreadSystem.md), [WorkerBudget](docs/core/WorkerBudget.md), [TimestepManager](docs/core/TimestepManager.md)
- **AI System:** [Overview](docs/ai/AIManager.md), [Behavior Execution Pipeline](docs/ai/BehaviorExecutionPipeline.md), [Behaviors](docs/ai/BehaviorModes.md), [Quick Reference](docs/ai/BehaviorQuickReference.md), [NPC Memory](docs/ai/NPCMemory.md), [Pathfinding System](docs/ai/PathfindingSystem.md)
- **Collision & Physics:** [CollisionManager](docs/managers/CollisionManager.md)
- **Entity System:** [Overview](docs/entities/README.md), [EntityHandle](docs/entities/EntityHandle.md), [EntityDataManager](docs/managers/EntityDataManager.md), [BackgroundSimulationManager](docs/managers/BackgroundSimulationManager.md)
- **Event System:** [Overview](docs/events/EventManager.md), [Quick Reference](docs/events/EventManager_QuickReference.md), [Advanced](docs/events/EventManager_Advanced.md), [TimeEvents](docs/events/TimeEvents.md), [EventFactory](docs/events/EventFactory.md)
- **Controllers:** [Overview](docs/controllers/README.md), [ControllerRegistry](docs/controllers/ControllerRegistry.md), [WeatherController](docs/controllers/WeatherController.md), [DayNightController](docs/controllers/DayNightController.md), [CombatController](docs/controllers/CombatController.md), [HudController](docs/controllers/HudController.md), [InventoryController](docs/controllers/InventoryController.md), [HarvestController](docs/controllers/HarvestController.md), [SocialController](docs/controllers/SocialController.md)
- **Managers:** [BackgroundSimulationManager](docs/managers/BackgroundSimulationManager.md), [CollisionManager](docs/managers/CollisionManager.md), [EntityDataManager](docs/managers/EntityDataManager.md), [FontManager](docs/managers/FontManager.md), [InputManager](docs/managers/InputManager.md), [ParticleManager](docs/managers/ParticleManager.md), [PathfinderManager](docs/managers/PathfinderManager.md), [ProjectileManager](docs/managers/ProjectileManager.md), [ResourceFactory](docs/managers/ResourceFactory.md), [ResourceTemplateManager](docs/managers/ResourceTemplateManager.md), [SoundManager](docs/managers/SoundManager.md), [TextureManager](docs/managers/TextureManager.md), [WorldManager](docs/managers/WorldManager.md), [WorldResourceManager](docs/managers/WorldResourceManager.md)
- **UI:** [UIManager Guide](docs/ui/UIManager_Guide.md), [UIConstants Reference](docs/ui/UIConstants.md), [Auto-Sizing](docs/ui/Auto_Sizing_System.md), [DPI-Aware Fonts](docs/ui/DPI_Aware_Font_System.md), [Minimap Implementation](docs/ui/Minimap_Implementation.md)
- **GPU Rendering:** [GPU System Overview](docs/gpu/GPURendering.md)
- **GameStates:** [Overview](docs/gameStates/README.md), [LoadingState](docs/gameStates/LoadingState.md), [SettingsMenuState](docs/gameStates/SettingsMenuState.md), [GameOverState](docs/gameStates/GameOverState.md)
- **Utilities:** [FrameProfiler](docs/utils/FrameProfiler.md), [Camera](docs/utils/Camera.md), [JsonReader](docs/utils/JsonReader.md), [JSON Resource Loading](docs/utils/JSON_Resource_Loading_Guide.md), [MenuNavigation](docs/utils/MenuNavigation.md), [Serialization](docs/utils/SERIALIZATION.md), [ResourceHandle System](docs/utils/ResourceHandle_System.md)
- **Architecture:** [Interpolation System](docs/architecture/InterpolationSystem.md)
- **Performance:** [Power Efficiency](docs/performance/PowerEfficiency.md), [EntityDataManager Power Analysis](docs/performance_reports/power_profile_edm_comparison_2026-01-29.md)
- **Development:** Repo-wide agent guidance lives in [AGENTS.md](AGENTS.md).
- **Engine Issues:** [SDL3 macOS Cleanup Issue](docs/issues/SDL3_MACOS_CLEANUP_ISSUE.md)

For the full, up-to-date documentation map, see [docs/README.md](docs/README.md).

---

## Core Design Principles

- **Data-Oriented Design:** EntityDataManager as single source of truth, Structure-of-Arrays (SOA) storage.
- **Memory Safety:** Smart pointers, RAII, no raw pointers
- **Performance:** Cache-friendly, batch processing, optimized threading
- **Type Safety:** Strong typing, compile-time and runtime validation
- **Cross-Platform:** Unified codebase, platform-specific optimizations

---

## Contributing

Contributions welcome!
- Report issues via GitHub with environment details and steps to reproduce.
- Fork, branch, test, and submit PRs with clear descriptions.

---

## Notes

- Window icon support for all platforms (see `res/img/`)
- Player and NPC controls: mouse, keyboard, controller (see `InputManager`)
- Template can be adapted for 3D (see `GameEngine.cpp` and `TextureManager`)
- For advanced usage, see [docs/README.md](docs/README.md)
- This is a work in progress, and the bundled art is placeholder content credited below.

---

## Art
All art follows the original artists' licensing terms. See their pages below for details.
- World Tiles/Assets : [Pipoya](https://pipoya.itch.io/pipoya-rpg-tileset-32x32)
- Slimes [patvanmackelberg](https://opengameart.org/users/patvanmackelberg)
- Player Abigail [adythewolf](https://opengameart.org/users/adythewolf)
- World/Various Ore deposits/ore/bars/gems [Senmou](https://opengameart.org/users/senmou)

## License

[MIT License](LICENSE)
