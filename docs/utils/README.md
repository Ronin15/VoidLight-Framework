# Utility Systems

This directory contains documentation for core utility classes and helper systems used throughout the VoidLight Engine.

## Available Utilities

### Rendering & View Systems
- **[Camera](Camera.md)** - 2D camera for world navigation
  - Multiple modes: Free, Follow, Fixed
  - Smooth interpolation for target following
  - Discrete zoom levels (pixel-perfect)
  - World bounds clamping. Camera-shake offset exists on `Camera` but is not applied (`docs/review-non-issues.md`).
  - Event-driven state changes

### Debug & Profiling
- **[FrameProfiler](FrameProfiler.md)** - Debug-only frame timing system
  - Three-tier profiling: Frame → Manager → Render phases
  - Automatic hitch detection with configurable threshold
  - Live F3 debug overlay with timing breakdown
  - GPU-aware phases for accurate GPU timing
  - RAII scoped timers for easy instrumentation
  - Zero overhead in Release builds (compiles to nothing)

### Performance & Resource Systems
- **[ResourceHandle System](ResourceHandle_System.md)** - High-performance resource identification
  - 48-bit lightweight handles for optimal runtime performance
  - Two-phase architecture: name-based loading, handle-based runtime
  - Cache-friendly data access patterns
  - Automatic duplicate name detection
  - Type-safe resource operations

### Data Processing
- **[JsonReader](JsonReader.md)** - RFC 8259 compliant JSON parser
  - Custom, dependency-free implementation
  - Type-safe data access with `std::optional` support
  - Robust error handling and detailed error messages
  - File loading capabilities
  - Thread-safe for read operations (separate instances per thread)

- **[JSON Resource Loading Guide](JSON_Resource_Loading_Guide.md)** - Complete resource loading system
  - JSON-based resource definitions for items, materials, currency
  - Integration with ResourceTemplateManager and ResourceFactory
  - Extensible type system with custom resource creators
  - Comprehensive examples and error handling
  - Performance guidelines for handle-based runtime operations

### Future Utilities

- **Configuration Manager** (planned)


### Usage in Game Development
- **High-Performance Resource Access**: Use ResourceHandle system for runtime resource operations
- **JSON Resource Loading**: Define game items, materials, currency in JSON files with automatic loading
- **Configuration Loading**: JSON configuration files for game settings
- **Handle-based Runtime**: Convert resource names to handles during initialization, use handles for all runtime operations
- **Save Game Data**: JSON format for human-readable save files (alternative to binary)
- **Asset Metadata**: JSON descriptions for textures, sounds, and other assets
- **Level Data**: JSON format for level layouts and entity placement

### Performance Considerations
- All utilities are designed for minimal overhead
- **ResourceHandle system provides cache-optimized resource access** 
- Header-only implementations where possible
- Move semantics and C++20 optimization
- Memory-efficient data structures
- Single-pass parsing where applicable
- **Handle-based operations outperform string-based lookups by ~10x**

### Error Handling Standards
All utilities follow the engine's error handling conventions:
- Use engine logging macros (`GAMEENGINE_ERROR`, `GAMEENGINE_WARNING`, etc.)
- Provide detailed error messages
- Graceful fallback to default values where appropriate
- Non-throwing interfaces with explicit error checking

## Best Practices

1. **Always validate input** before processing
2. **Use type-safe accessors** to prevent runtime errors
3. **Handle missing data gracefully** with sensible defaults
4. **Log errors appropriately** using engine logging system
5. **Reuse utility instances** where possible for performance
6. **Follow engine coding standards** for consistency

## Testing

All utilities include comprehensive test suites:
- Unit tests for core functionality
- Error condition testing
- Performance benchmarks
- Real-world usage scenarios
- Cross-platform compatibility tests

Tests are located in the `tests/utils/` directory and are automatically run as part of the core test suite.

## Contributing

When adding new utilities:
1. Follow the existing documentation structure
2. Include comprehensive examples
3. Add appropriate error handling
4. Create thorough test coverage
5. Update this README with the new utility
6. Follow engine coding standards and conventions

---

For specific utility documentation, see the individual files linked above.
