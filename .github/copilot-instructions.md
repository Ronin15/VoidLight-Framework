# GitHub Copilot Instructions for SDL3 VoidLight-Framework

## Project Context
This is a C++ game engine project using SDL3, built with CMake and following modern C++20 standards. Please refer to `AGENTS.md` for detailed guidelines.

## Coding Standards & Conventions

### Naming Conventions
- **Classes, Enums, Namespaces**: UpperCamelCase (e.g., `GameEngine`, `EventType`)
- **Functions, Variables**: lowerCamelCase (e.g., `updateGame()`, `playerHealth`)
- **Member Variables**: `m_` prefix (e.g., `m_isRunning`, `m_playerPosition`)
- **Constants**: ALL_CAPS with underscores (e.g., `MAX_PLAYERS`, `DEFAULT_SPEED`)

### Code Style
- **Standard**: C++20
- **Indentation**: 4 spaces, no tabs
- **Braces**: K&R (opening brace on the same line); `.clang-format` is authoritative
- **Memory Management**: Use RAII with smart pointers (`std::unique_ptr`, `std::shared_ptr`), avoid raw `new/delete`
- **STL Preference**: Use STL algorithms over manual loops when possible

### Architecture Patterns

#### Manager Pattern
- Follow existing manager patterns with singleton shutdown (`m_isShutdown` guard)
- Managers coordinate through `GameEngine`, avoid direct inter-manager dependencies
- Use `ThreadSystem` for threading, avoid raw `std::thread`

#### Game Loop & Rendering
- **Update**: Thread-safe via mutex, runs on fixed timestep
- **Render**: Main thread only, uses double buffering
- **Camera**: Centralized camera logic, avoid per-entity camera calculations
- **Threading**: No rendering from background threads

#### Memory & Performance
- Prefer stack allocation and RAII
- Use `std::vector` for dynamic arrays
- Implement move semantics for large objects
- Use `const` references for parameters when appropriate

### Build & Test Guidelines
- **Build System**: CMake with Ninja generator
- **Testing**: Boost.Test framework, tests in `tests/` directory
- **Debug**: Use AddressSanitizer for memory debugging
- **Platform**: Support Linux, macOS, and Windows with appropriate guards

### File Organization
- **Headers**: `include/` directory matching `src/` structure
- **Implementation**: `src/` directory with modular organization
- **Tests**: `tests/` directory with corresponding test files
- **Assets**: `res/` directory for resources

## Code Generation Guidelines

When generating code:
1. **Always include proper headers** and forward declarations
2. **Follow the existing project structure** and naming conventions
3. **Use the established patterns** for managers, game states, and entities
4. **Include proper error handling** and logging using provided macros
5. **Write thread-safe code** when dealing with shared resources
6. **Add appropriate comments** for complex logic
7. **Consider performance implications** and prefer efficient algorithms

## Common Patterns to Follow

### Singleton Manager Pattern
```cpp
class ExampleManager 
{
private:
    static std::unique_ptr<ExampleManager> m_instance;
    std::atomic<bool> m_isShutdown{false};
    
public:
    static ExampleManager& getInstance();
    static void shutdown();
    // ... implementation
};
```

### Error Handling
Use the provided logging macros:
- `GAMEENGINE_ERROR(message)`
- `GAMEENGINE_WARN(message)`
- `GAMEENGINE_INFO(message)`

### Thread Safety
Use the ThreadSystem for background work:
```cpp
ThreadSystem::getInstance().submitWork(WorkerBudget::LOW, [this]() {
    // Background work here
});
```

Always reference `AGENTS.md` and existing code patterns when implementing new features.
