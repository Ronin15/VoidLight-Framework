# Logger System

## Overview

The VoidLight Engine logging system provides efficient, configurable logging with zero overhead in release builds. The system is designed for high-performance game development with separate debug and release configurations.

## Table of Contents

- [Quick Start](#quick-start)
- [System Architecture](#system-architecture)
- [Log Levels](#log-levels)
- [Debug vs Release Builds](#debug-vs-release-builds)
- [Available Macros](#available-macros)
- [System-Specific Logging](#system-specific-logging)
- [Best Practices](#best-practices)
- [Performance Considerations](#performance-considerations)
- [Examples](#examples)

## Quick Start

### Basic Usage

```cpp
#include "core/Logger.hpp"

// Basic logging with system identification
VOIDLIGHT_INFO("MySystem", "Application started successfully");
VOIDLIGHT_ERROR("Network", "Failed to connect to server");
VOIDLIGHT_CRITICAL("Memory", "Out of memory condition detected");
```

### System-Specific Macros

```cpp
// Use convenient system-specific macros
GAMEENGINE_INFO("Engine initialized successfully");
TEXTURE_ERROR("Failed to load texture: player.png");
SOUND_WARN("Audio device not found, using software mixing");
```

## System Architecture

### Design Principles

1. **Zero Overhead in Release**: No logging overhead in release builds except CRITICAL messages
2. **Fast printf-based Output**: Uses printf for maximum performance in debug builds
3. **Immediate Flushing**: Real-time output for debugging with fflush()
4. **Type Safety**: String conversion handled automatically in macros
5. **System Identification**: Clear identification of which system generated each log

### Class Structure

```cpp
namespace VoidLight-Framework {
    enum class LogLevel : uint8_t {
        CRITICAL = 0,  // Always logged (even in release)
        ERROR = 1,     // Debug only
        WARNING = 2,   // Debug only
        INFO = 3,      // Debug only
        DEBUG_LEVEL = 4      // Debug only (renamed to avoid macro conflicts)
    };

    class Logger {
        static void Log(LogLevel level, const char* system, const std::string& message);
        static void Log(LogLevel level, const char* system, const char* message);
        static void SetBenchmarkMode(bool enabled);
        static bool IsBenchmarkMode();
    };
}
```

## Log Levels

### CRITICAL (Level 0)
- **Always logged** in both debug and release builds
- Reserved for fatal errors and crash conditions
- Automatically flushed to ensure output before potential crash

### ERROR (Level 1)
- **Debug builds only**
- Used for error conditions that don't crash the application
- Non-recoverable errors that affect functionality

### WARNING (Level 2)
- **Debug builds only**
- Potential issues that don't prevent operation
- Deprecated function usage, fallback behavior

### INFO (Level 3)
- **Debug builds only**
- General information about system state
- Initialization messages, status updates

### DEBUG (Level 4)
- **Debug builds only**
- Detailed debugging information
- Performance metrics, detailed state information

## Debug vs Release Builds

### Debug Build Behavior
```cpp
#ifdef DEBUG
    // Full Logger class with printf-based output
    // All log levels functional
    // Immediate flushing for real-time feedback
    #define VOIDLIGHT_INFO(system, msg) VoidLight::Logger::Log(VoidLight::LogLevel::INFO, system, std::string(msg))
#endif
```

### Release Build Behavior
```cpp
#else
    // Ultra-minimal overhead
    #define VOIDLIGHT_CRITICAL(system, msg) do { \
        printf("VoidLight Engine - [%s] CRITICAL: %s\n", system, std::string(msg).c_str()); \
    } while(0)

    // All other levels become no-ops
    #define VOIDLIGHT_ERROR(system, msg) ((void)0)
    #define VOIDLIGHT_WARN(system, msg) ((void)0)
    // ... etc
#endif
```

### Release Log File

In release builds, **CRITICAL and ERROR** messages are written to a timestamped log file. WARN, INFO, and DEBUG are compiled out entirely (zero overhead). The file is created via `SDL_GetPrefPath` under the OS user data directory:

| Platform | Path |
|----------|------|
| macOS    | `~/Library/Application Support/HammerForgedGames/VoidLight_Template/logs/` |
| Windows  | `%APPDATA%\HammerForgedGames\VoidLight_Template\logs\` |
| Linux    | `~/.local/share/HammerForgedGames/VoidLight_Template/logs/` |

**Filename format:** `voidlight_YYYYMMDD_HHMMSS.log`

The logger automatically keeps the **5 most recent** log files and deletes older ones on startup. Each file opens with a header:

```
=== VoidLight_Template Log ===
Started: YYYY-MM-DD HH:MM:SS
==========================================
```

## Available Macros

### Core Logging Macros

| Macro | Parameters | Description |
|-------|------------|-------------|
| `VOIDLIGHT_CRITICAL(system, msg)` | system: const char*, msg: any type | Critical error logging |
| `VOIDLIGHT_ERROR(system, msg)` | system: const char*, msg: any type | Error logging |
| `VOIDLIGHT_WARN(system, msg)` | system: const char*, msg: any type | Warning logging |
| `VOIDLIGHT_INFO(system, msg)` | system: const char*, msg: any type | Information logging |
| `VOIDLIGHT_DEBUG(system, msg)` | system: const char*, msg: any type | Debug logging |

### Message Parameter Types
The `msg` parameter accepts:
- `const char*` strings
- `std::string` objects
- Any type convertible to string via `std::string(msg)`

## System-Specific Logging

### Core Systems

#### GameLoop System
```cpp
GAMELOOP_CRITICAL(msg)    // GameLoop critical errors
GAMELOOP_ERROR(msg)       // GameLoop errors
GAMELOOP_WARN(msg)        // GameLoop warnings
GAMELOOP_INFO(msg)        // GameLoop information
GAMELOOP_DEBUG(msg)       // GameLoop debug info
```

#### GameEngine System
```cpp
GAMEENGINE_CRITICAL(msg)  // Engine critical errors
GAMEENGINE_ERROR(msg)     // Engine errors
GAMEENGINE_WARN(msg)      // Engine warnings
GAMEENGINE_INFO(msg)      // Engine information
GAMEENGINE_DEBUG(msg)     // Engine debug info
```

#### ThreadSystem
```cpp
THREADSYSTEM_CRITICAL(msg) // Threading critical errors
THREADSYSTEM_ERROR(msg)    // Threading errors
THREADSYSTEM_WARN(msg)     // Threading warnings
THREADSYSTEM_INFO(msg)     // Threading information
THREADSYSTEM_DEBUG(msg)    // Threading debug info
```

### Manager Systems

#### Resource Managers
```cpp
// Texture Management
TEXTURE_CRITICAL(msg)     // Texture loading failures
TEXTURE_ERROR(msg)        // Texture operation errors
TEXTURE_WARN(msg)         // Texture warnings
TEXTURE_INFO(msg)         // Texture information
TEXTURE_DEBUG(msg)        // Texture debug info

// Sound Management
SOUND_CRITICAL(msg)       // Audio system failures
SOUND_ERROR(msg)          // Sound operation errors
SOUND_WARN(msg)           // Audio warnings
SOUND_INFO(msg)           // Sound information
SOUND_DEBUG(msg)          // Sound debug info

// Font Management
FONT_CRITICAL(msg)        // Font loading failures
FONT_ERROR(msg)           // Font operation errors
FONT_WARN(msg)            // Font warnings
FONT_INFO(msg)            // Font information
FONT_DEBUG(msg)           // Font debug info
```

#### Game Systems
```cpp
// AI Management
AI_CRITICAL(msg)          // AI system failures
AI_ERROR(msg)             // AI operation errors
AI_WARN(msg)              // AI warnings
AI_INFO(msg)              // AI information
AI_DEBUG(msg)             // AI debug info

// Event Management
EVENT_CRITICAL(msg)       // Event system failures
EVENT_ERROR(msg)          // Event operation errors
EVENT_WARN(msg)           // Event warnings
EVENT_INFO(msg)           // Event information
EVENT_DEBUG(msg)          // Event debug info

// Input Management
INPUT_CRITICAL(msg)       // Input system failures
INPUT_ERROR(msg)          // Input operation errors
INPUT_WARN(msg)           // Input warnings
INPUT_INFO(msg)           // Input information
INPUT_DEBUG(msg)          // Input debug info
```

#### State Management
```cpp
// Game State Management
GAMESTATE_CRITICAL(msg)   // Game state failures
GAMESTATE_ERROR(msg)      // State operation errors
GAMESTATE_WARN(msg)       // State warnings
GAMESTATE_INFO(msg)       // State information
GAMESTATE_DEBUG(msg)      // State debug info

// Entity State Management
ENTITYSTATE_CRITICAL(msg) // Entity state failures
ENTITYSTATE_ERROR(msg)    // Entity operation errors
ENTITYSTATE_WARN(msg)     // Entity warnings
ENTITYSTATE_INFO(msg)     // Entity information
ENTITYSTATE_DEBUG(msg)    // Entity debug info
```

#### UI and Save Systems
```cpp
// UI Management
UI_CRITICAL(msg)          // UI system failures
UI_ERROR(msg)             // UI operation errors
UI_WARN(msg)              // UI warnings
UI_INFO(msg)              // UI information
UI_DEBUG(msg)             // UI debug info

// Save Game Management
SAVEGAME_CRITICAL(msg)    // Save system failures
SAVEGAME_ERROR(msg)       // Save operation errors
SAVEGAME_WARN(msg)        // Save warnings
SAVEGAME_INFO(msg)        // Save information
SAVEGAME_DEBUG(msg)       // Save debug info
```

### Entity Systems

#### Core Entity Management
```cpp
// General Entity System
ENTITY_CRITICAL(msg)      // Entity system failures
ENTITY_ERROR(msg)         // Entity operation errors
ENTITY_WARN(msg)          // Entity warnings
ENTITY_INFO(msg)          // Entity information
ENTITY_DEBUG(msg)         // Entity debug info

// Player Entity
PLAYER_CRITICAL(msg)      // Player system failures
PLAYER_ERROR(msg)         // Player operation errors
PLAYER_WARN(msg)          // Player warnings
PLAYER_INFO(msg)          // Player information
PLAYER_DEBUG(msg)         // Player debug info

// NPC Entities
NPC_CRITICAL(msg)         // NPC system failures
NPC_ERROR(msg)            // NPC operation errors
NPC_WARN(msg)             // NPC warnings
NPC_INFO(msg)             // NPC information
NPC_DEBUG(msg)            // NPC debug info
```

## Best Practices

### 1. Use Appropriate Log Levels
```cpp
// Good: Use CRITICAL for actual critical errors
GAMEENGINE_CRITICAL("Failed to initialize SDL - application cannot continue");

// Good: Use ERROR for recoverable errors
TEXTURE_ERROR("Failed to load optional texture: background.png");

// Good: Use WARN for potential issues
SOUND_WARN("Audio device busy, retrying in 100ms");

// Good: Use INFO for status updates
GAMELOOP_INFO("Game loop started at 60 FPS");

// Good: Use DEBUG for detailed information
AI_DEBUG("Pathfinding calculated route with 15 waypoints");
```

### 2. Provide Context in Messages
```cpp
// Good: Specific, actionable information
TEXTURE_ERROR("Failed to load texture 'player_sprite.png': File not found");

// Bad: Vague, unhelpful message
TEXTURE_ERROR("Load failed");
```

### 3. Use System-Specific Macros
```cpp
// Good: Use system-specific macro
SOUND_INFO("Audio subsystem initialized with OpenAL");

// Less ideal: Generic macro requires system parameter
VOIDLIGHT_INFO("SoundManager", "Audio subsystem initialized with OpenAL");
```

### 4. Format Complex Data
```cpp
// Good: Format complex data clearly
GAMELOOP_DEBUG("Frame timing - Delta: " + std::to_string(deltaTime) +
               "s, FPS: " + std::to_string(currentFPS));

// Good: Use string concatenation for readable output
AI_INFO("Entity " + std::to_string(entityId) + " switched to patrol mode");
```

## Benchmark Mode

The logging system includes a benchmark mode that disables all logging output for performance testing:

### Benchmark Mode Control
```cpp
// Enable benchmark mode (disables all logging)
VOIDLIGHT_ENABLE_BENCHMARK_MODE();
// or
VoidLight::Logger::SetBenchmarkMode(true);

// Disable benchmark mode (re-enables logging)
VOIDLIGHT_DISABLE_BENCHMARK_MODE();
// or
VoidLight::Logger::SetBenchmarkMode(false);

// Check current benchmark mode status
if (VoidLight::Logger::IsBenchmarkMode()) {
    // Logging is currently disabled
}
```

### Use Cases for Benchmark Mode
- **Performance Profiling**: Eliminate logging overhead during benchmarks
- **Release Testing**: Test release performance characteristics in debug builds
- **Automated Testing**: Reduce console output during automated test runs
- **Performance Comparison**: Compare performance with and without logging

### Example Usage
```cpp
void runPerformanceTest() {
    GAMELOOP_INFO("Starting performance test");

    // Enable benchmark mode for clean performance measurement
    VOIDLIGHT_ENABLE_BENCHMARK_MODE();

    auto start = std::chrono::high_resolution_clock::now();

    // Run performance-critical code without logging overhead
    for (int i = 0; i < 1000000; ++i) {
        processGameFrame();
        // No logging output during this loop
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Re-enable logging for results
    VOIDLIGHT_DISABLE_BENCHMARK_MODE();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    GAMELOOP_INFO("Performance test completed in " + std::to_string(duration.count()) + " microseconds");
}
```

## Performance Considerations

### Debug Build Performance
- Uses fast printf-based output
- Immediate flushing ensures real-time feedback
- String conversion handled automatically
- Minimal overhead for typical game logging
- Benchmark mode available for zero-overhead testing

### Release Build Performance
- **Zero overhead** for ERROR, WARNING, INFO, DEBUG levels
- CRITICAL and ERROR messages have minimal overhead (single printf call with mutex protection)
- No function calls or string processing for disabled levels
- Compiler optimizes away disabled macros completely
- Benchmark mode affects CRITICAL and ERROR levels in release builds

### Memory Usage
- No dynamic memory allocation
- No log buffering or storage
- Immediate output to stdout
- No memory leaks possible
- Thread-safe mutex protection with minimal memory footprint

## Examples

### Basic System Initialization
```cpp
bool GameEngine::init(std::string_view title, int width, int height, bool fullscreen) {
    GAMEENGINE_INFO("Initializing VoidLight Engine");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        GAMEENGINE_CRITICAL("Failed to initialize SDL: " + std::string(SDL_GetError()));
        return false;
    }

    GAMEENGINE_INFO("SDL initialized successfully");

    // Create window
    mp_window.reset(SDL_CreateWindow(title, width, height,
                    fullscreen ? SDL_WINDOW_FULLSCREEN : 0));

    if (!mp_window) {
        GAMEENGINE_ERROR("Failed to create window: " + std::string(SDL_GetError()));
        return false;
    }

    GAMEENGINE_DEBUG("Window created: " + std::to_string(width) + "x" +
                     std::to_string(height) + (fullscreen ? " (fullscreen)" : " (windowed)"));

    return true;
}
```

### Resource Loading with Error Handling
```cpp
bool TextureManager::load(const std::string& fileName, const std::string& textureID,
                         SDL_Renderer* renderer) {
    TEXTURE_INFO("Loading texture: " + fileName + " as ID: " + textureID);

    SDL_Surface* surface = IMG_Load(fileName.c_str());
    if (!surface) {
        TEXTURE_ERROR("Failed to load image '" + fileName + "': " + std::string(IMG_GetError()));
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        TEXTURE_ERROR("Failed to create texture from '" + fileName + "': " + std::string(SDL_GetError()));
        return false;
    }

    m_textureMap[textureID] = std::shared_ptr<SDL_Texture>(texture, SDL_DestroyTexture);
    TEXTURE_DEBUG("Texture loaded successfully - ID: " + textureID + ", File: " + fileName);

    return true;
}
```

### Game Loop with Performance Monitoring
```cpp
void GameLoop::runMainThread() {
    GAMELOOP_INFO("Starting main game loop thread");

    while (m_running.load()) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        processEvents();

        if (!m_paused.load()) {
            processUpdates();
        }

        processRender();

        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart);

        if (frameDuration.count() > 20000) { // > 20ms frame time
            GAMELOOP_WARN("Long frame detected: " + std::to_string(frameDuration.count()) + " microseconds");
        }

        GAMELOOP_DEBUG("Frame completed in " + std::to_string(frameDuration.count()) + " microseconds");
    }

    GAMELOOP_INFO("Game loop thread terminated");
}
```

### Error Recovery with Logging
```cpp
bool SoundManager::playSound(const std::string& soundID) {
    auto it = m_soundMap.find(soundID);
    if (it == m_soundMap.end()) {
        SOUND_ERROR("Attempted to play unknown sound: " + soundID);
        return false;
    }

    if (Mix_PlayChannel(-1, it->second.get(), 0) == -1) {
        SOUND_WARN("Failed to play sound '" + soundID + "': " + std::string(Mix_GetError()) +
                   " - attempting retry");

        // Retry once
        if (Mix_PlayChannel(-1, it->second.get(), 0) == -1) {
            SOUND_ERROR("Sound playback failed after retry: " + soundID);
            return false;
        }
    }

    SOUND_DEBUG("Sound played successfully: " + soundID);
    return true;
}
```

### Entity Management with Logging
```cpp
bool Player::loadDimensionsFromTexture() {
    PLAYER_DEBUG("Loading texture dimensions for player");

    auto& textureManager = TextureManager::Instance();
    auto texture = textureManager.getTexture(m_textureID);

    if (texture) {
        float width, height;
        if (SDL_GetTextureSize(texture.get(), &width, &height)) {
            PLAYER_DEBUG("Original texture dimensions: " + std::to_string(width) + "x" + std::to_string(height));

            m_width = static_cast<int>(width);
            m_height = static_cast<int>(height);

            // Calculate frame dimensions for sprite sheets
            m_frameWidth = m_width / m_numFrames;
            int frameHeight = m_height / m_spriteSheetRows;
            m_height = frameHeight;

            PLAYER_DEBUG("Frame dimensions: " + std::to_string(m_frameWidth) + "x" + std::to_string(frameHeight));
            PLAYER_DEBUG("Sprite layout: " + std::to_string(m_numFrames) + " columns x " + std::to_string(m_spriteSheetRows) + " rows");

            return true;
        } else {
            PLAYER_ERROR("Failed to query texture dimensions: " + std::string(SDL_GetError()));
            return false;
        }
    } else {
        PLAYER_ERROR("Texture '" + m_textureID + "' not found in TextureManager");
        return false;
    }
}

void NPC::setWanderArea(float x1, float y1, float x2, float y2) {
    m_wanderBounds = {x1, y1, x2, y2};
    m_hasWanderBounds = true;

    NPC_DEBUG("NPC wander area set: (" + std::to_string(x1) + ", " + std::to_string(y1) +
              ") to (" + std::to_string(x2) + ", " + std::to_string(y2) + ")");
}

void EntityStateManager::setState(const std::string& stateName) {
    if (auto current = currentState.lock()) {
        ENTITYSTATE_INFO("Exiting entity state: " + getCurrentStateName());
        current->exit();
    }

    auto it = states.find(stateName);
    if (it != states.end()) {
        currentState = it->second;
        if (auto current = currentState.lock()) {
            ENTITYSTATE_INFO("Entering entity state: " + stateName);
            current->enter();
        }
    } else {
        ENTITYSTATE_ERROR("Entity state not found: " + stateName);
        currentState.reset();
    }
}
```

## Integration with Other Systems

### Thread Safety
The logging system is thread-safe through the use of printf, which is atomic for single calls. For multi-threaded applications:

```cpp
// Safe: Single printf call per log message
THREADSYSTEM_INFO("Worker thread " + std::to_string(threadId) + " started");

// Safe: System-specific macros are thread-safe
AI_DEBUG("Processing AI update for entity " + std::to_string(entityId));
```

### Performance Monitoring Integration
```cpp
// Use with performance monitoring systems
auto start = std::chrono::high_resolution_clock::now();
processAIUpdates();
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

AI_DEBUG("AI update completed in " + std::to_string(duration.count()) + " microseconds");
```

---

The Logger system provides the foundation for debugging and monitoring the VoidLight Engine. Use it liberally in debug builds for comprehensive insight into system behavior, while maintaining zero overhead in release builds for optimal performance.
