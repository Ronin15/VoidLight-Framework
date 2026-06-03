# VoidLight-Framework Changelog Index

This folder contains **branch-scoped changelogs**. Prefer the most specific changelog for the work you're looking at.

## Branch Changelogs

- [`CHANGELOG_ITEMS_UPDATE.md`](CHANGELOG_ITEMS_UPDATE.md) — `items` (2026-04-13 → 2026-06-03): inventory/container UI path consolidation, GPU/UI cleanup, and validation/tooling fixes.
- [`CHANGELOG_PROJECTILES_UPDATE.md`](CHANGELOG_PROJECTILES_UPDATE.md) — `projectiles` (2026-04-03 → 2026-04-10): ProjectileManager + HUD ownership refactor + `[[nodiscard]]`/state enum standardization.
- [`CHANGELOG_AUDIT_REFACTOR.md`](CHANGELOG_AUDIT_REFACTOR.md) — `audit` (2026-03-30 → 2026-04-01): GPU-only audit/refactor, safety + ownership pass.
- [`CHANGELOG_ARCHITECTURE_UPDATE.md`](CHANGELOG_ARCHITECTURE_UPDATE.md) — `architecture_update` (2025-12-26): execution model + WorkerBudget + collision architecture update.
- [`testing_update.md`](testing_update.md) — `testing_update` (2025-12-02): expanded test coverage + fixes found by tests.
- [`CHANGELOG_WORLD_TIME_UPDATE.md`](CHANGELOG_WORLD_TIME_UPDATE.md) — `world_time` (2025-12-14): time/calendar + controller architecture + performance.
- [`CHANGELOG_WORLD_UPDATE_2.md`](CHANGELOG_WORLD_UPDATE_2.md) — `world_update` (2025-11-16): major engine expansion + performance overhaul.
- [`WORLD_UPDATE_CHANGELOG.md`](WORLD_UPDATE_CHANGELOG.md) — `world_update`: earlier world-update summary.
- [`Threading_fixes.md`](Threading_fixes.md) — `Threading_fixes`: threading/collision/AI refactor notes.
- [`EVENT_ALIGNMENT_CHANGELOG.md`](EVENT_ALIGNMENT_CHANGELOG.md) — event system centralization/refactor notes.
- [`COLLISIONS&PATHFINDING_UPDATE.md`](COLLISIONS&PATHFINDING_UPDATE.md) — initial collision + pathfinding stack.
- [`CHANGELOG_EDM_DATA_ORIENTED_REFACTOR.md`](CHANGELOG_EDM_DATA_ORIENTED_REFACTOR.md) — SDL3 GPU + data-driven EDM resource refactor.
- [`CHANGELOG_RESOURCE_SDL3_GPU_UPDATE.md`](CHANGELOG_RESOURCE_SDL3_GPU_UPDATE.md) — resource update + SDL3 GPU notes.
- [`CHANGELOG_RESOURCE_COMBAT_UPDATES.md`](CHANGELOG_RESOURCE_COMBAT_UPDATES.md) — resource/combat follow-ups.

---

## Legacy: `resource_update` Branch Changelog

This document originally described the `resource_update` branch directly. It is preserved below for historical context.

### Overview
This changelog documents the extensive updates made in the `resource_update` branch, representing a major overhaul of the VoidLight-Framework with focus on resource management, performance optimizations, and system stability.

## 🎯 Major Features Added

### Resource Management System
- **Complete Resource System Rewrite**: Implemented a comprehensive resource management system with type-safe handles
- **JSON Resource Loading**: Added `JsonReader` utility for loading game data from JSON files
- **Resource Templates**: Created `ResourceTemplateManager` for managing resource definitions
- **World Resources**: Implemented `WorldResourceManager` for global resource state management
- **Inventory System**: Added full inventory component system with item management
- **Resource Events**: Integrated resource change events throughout the engine

### AI System Enhancements
- **AI Behavior Optimization**: Major performance improvements to AI behavior processing
- **Thread-Safe AI Processing**: Enhanced AI manager with better threading support
- **Behavior Parameter Flexibility**: Refactored AI behavior constructors for enhanced control
- **Patrol Mode Optimization**: Improved patrol behavior efficiency

### Particle System Overhaul
- **Type Safety**: Replaced string-based particle types with enum system
- **Threading Improvements**: Fixed race conditions and vector access issues
- **Weather Effects**: Enhanced particle effects for fog, clouds, and weather
- **Performance Optimization**: Reduced CPU usage and improved rendering efficiency

## 🔧 System Improvements

### Threading System
- **ThreadSystem Rework**: Multiple iterations to improve performance and stability
- **WorkerBudget System**: Enhanced task scheduling and resource allocation
- **Cross-Platform Stability**: Fixed Windows-specific threading issues
- **Race Condition Fixes**: Resolved critical threading race conditions

### Performance Optimizations
- **Critical Bottleneck Fixes**: Optimized TaskQueue and ResourceTemplateManager
- **Callgrind Analysis**: Implemented comprehensive performance profiling
- **Memory Management**: Improved memory usage patterns and reduced allocations
- **Cache-Friendly Design**: Enhanced data structures for better cache performance

### Build System & Testing
- **Enhanced Test Suite**: Added comprehensive tests for all new systems
- **Valgrind Integration**: Improved memory analysis and profiling tools
- **Cross-Platform Support**: Fixed build issues on Windows and macOS
- **Static Analysis**: Enhanced cppcheck integration and fixed warnings

## 📁 File Changes Summary

### New Files Added (26 major additions)
- **Resource System**: `Resource.hpp/cpp`, `ResourceHandle.hpp`, `ResourceFactory.hpp/cpp`
- **Resource Components**: `InventoryComponent.hpp/cpp`, various resource type headers
- **Resource Managers**: `ResourceTemplateManager.hpp/cpp`, `WorldResourceManager.hpp/cpp`
- **JSON System**: `JsonReader.hpp/cpp` with comprehensive testing
- **Resource Events**: `ResourceChangeEvent.hpp/cpp`
- **Test Suites**: 8 new test files for resource system validation
- **Documentation**: Extensive documentation for all new systems

### Major File Modifications (30+ core files)
- **AI Behaviors**: All 8 behavior files significantly refactored
- **Managers**: `AIManager`, `ParticleManager`, `EventManager` extensively updated
- **Game States**: `EventDemoState`, `GamePlayState` enhanced with resource integration
- **Core Systems**: `ThreadSystem`, `Logger`, `GameEngine` improved
- **Entities**: `Player`, `NPC` enhanced with resource capabilities

### Documentation Overhaul
- **Comprehensive Guides**: Added detailed documentation for all new systems
- **Performance Notes**: Created performance changelog and optimization guides
- **Testing Documentation**: Enhanced testing procedures and troubleshooting guides
- **API Documentation**: Complete documentation for resource handling system

## 🐛 Bug Fixes & Stability

### Critical Fixes
- **Memory Safety**: Fixed multiple potential memory leaks and access violations
- **Thread Safety**: Resolved race conditions in particle and AI systems
- **Resource Initialization**: Fixed resource factory initialization issues
- **Vector Access**: Corrected out-of-bounds access in particle manager

### Platform-Specific Fixes
- **Windows Compatibility**: Fixed filesystem path handling and vector assertion errors
- **macOS Support**: Resolved SDL3 mixer and build configuration issues
- **Cross-Platform**: Standardized file path handling using `std::filesystem`

### Code Quality Improvements
- **Static Analysis**: Fixed 50+ cppcheck warnings and unused variables
- **Type Safety**: Improved type safety throughout the codebase
- **Error Handling**: Enhanced error handling and logging throughout systems
- **Code Standards**: Applied consistent coding standards and best practices

## 🎮 Game Features Enhanced

### Gameplay Systems
- **Player Inventory**: Full inventory system with item management UI
- **Resource Economy**: Integrated materials and currency system
- **Event System**: Enhanced event handling with resource integration
- **UI Elements**: Added inventory UI components and resource displays

### Demo Enhancements
- **Event Demo**: Significantly enhanced with resource demonstrations
- **AI Demo**: Improved AI behavior showcases
- **Particle Demo**: Enhanced particle effects and weather systems

## 📊 Performance Metrics

### Optimization Results
- **AI Processing**: ~40% improvement in behavior processing efficiency
- **Particle System**: Reduced CPU usage by ~30% with threading improvements
- **Memory Usage**: Improved memory allocation patterns and reduced leaks
- **Load Times**: Faster resource loading with JSON optimization

### Testing Coverage
- **137 files modified** with comprehensive test coverage
- **32,929 lines added**, 14,937 lines removed (net +17,992 lines)
- **8 new test suites** covering all major systems
- **Valgrind integration** for memory analysis and profiling

## 🔄 Migration Notes

### Breaking Changes
- Resource system requires migration from old resource handling
- AI behavior constructors have new parameter requirements
- Particle system uses enums instead of strings for type safety
- Threading system API changes require code updates

### Compatibility
- Maintains backward compatibility where possible
- Provides migration guides in documentation
- Incremental adoption possible for most systems

---

## Summary
The `resource_update` branch represents a major evolution of VoidLight-Framework, introducing a robust resource management system, significant performance improvements, and enhanced stability across all platforms. This update establishes a solid foundation for future game development with the engine.

**Total Impact**: 137 files modified, major systems rewritten, comprehensive testing added, and significant performance improvements achieved.
