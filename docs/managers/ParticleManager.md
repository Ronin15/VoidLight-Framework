# ParticleManager

**Code:** `include/managers/ParticleManager.hpp`, `src/managers/ParticleManager.cpp`

**Singleton Access:** Use `ParticleManager::Instance()` to access the manager.

## Overview

The ParticleManager is a high-performance, production-ready particle system designed for real-time game development. It provides efficient visual effects including weather systems, fire, smoke, sparks, and custom particle effects. The system is optimized for handling thousands of particles while maintaining 60+ FPS through advanced memory management, threading, and rendering optimizations.

## Architecture Overview

### Core Design Philosophy

The ParticleManager follows a **SIMD-Optimized Structure of Arrays (SoA)** approach combined with **WorkerBudget threading** and **intelligent memory management** for maximum performance:

- **SIMD-Friendly SoA Storage**: Separate arrays for X/Y components enable vectorization optimizations
- **Lock-Free Updates**: Worker threads use lock-free batch processing with shared_mutex synchronization
- **WorkerBudget Integration**: Intelligent threading allocation with queue pressure management
- **EventManager Integration**: Seamless weather effects triggered by game events
- **Vector Optimization**: Cache-friendly sequential memory access patterns

### System Architecture

```
ParticleManager (Singleton)
├── SIMD-Optimized SoA Storage System
│   ├── ParticleSoA Structure (SIMD-friendly)
│   │   ├── Position Components (posX[], posY[])
│   │   ├── Velocity Components (velX[], velY[])
│   │   ├── Acceleration Components (accX[], accY[])
│   │   ├── Life Properties (life[], maxLife[], size[])
│   │   ├── Rotation Properties (rotation[], angularVelocity[])
│   │   ├── Color (RGBA packed uint32_t[])
│   │   ├── Flags (active, visible, weather, fade[])
│   │   └── Generation ID (uint8_t[])
│   ├── Vector-Optimized Storage
│   │   ├── Separate arrays for SIMD processing
│   │   └── Cache-aligned memory layout
│   └── Automatic Memory Management
│       ├── Free-index slot reuse
│       └── Freed indices recycled after 2 frames
├── Effect Management System
│   ├── Effect Definitions
│   │   ├── Built-in Weather Effects
│   │   ├── Built-in Visual Effects
│   │   └── Custom Effect Support
│   ├── Effect Instances
│   │   ├── Weather Effects
│   │   ├── Independent Effects
│   │   └── Grouped Effects
│   └── Emission Control
│       ├── Duration Management
│       └── Transition System
├── Threading & Performance
│   ├── WorkerBudget Integration
│   ├── Batch Processing
│   ├── Performance Monitoring
│   └── Queue Pressure Management
└── Rendering Pipeline
    ├── Background Particles (Rain, Snow, Fire)
    ├── Foreground Particles (Fog, Clouds)
    ├── Frustum Culling
    └── Alpha Blending
```

## Key Features

### 🚀 High Performance
- **SIMD-Optimized SoA Architecture**: Structure of Arrays with separate X/Y component storage enables automatic vectorization
- **Cache-Friendly Memory Layout**: Sequential access patterns optimized for modern CPU cache hierarchies
- **Vector Processing**: Position, velocity, and acceleration updates leverage SIMD instructions (SSE/AVX)
- **Lock-Free Worker Threads**: Shared_mutex with try-lock mechanisms prevent deadlocks
- **WorkerBudget Threading**: Queue pressure management with graceful degradation
- **Automatic Memory Management**: Inactive slots are recycled through a free-index pool (no compaction pass)
- **Performance Statistics**: Instantaneous rate calculation prevents metric overflow issues

### 🌦️ Weather System Integration
- **EventManager Integration**: Automatic weather effects triggered by game events
- **Realistic Weather**: Rain, snow, fog, and cloud effects with proper physics
- **Smooth Transitions**: Configurable fade-in/fade-out between weather states
- **Screen Coverage**: Full-screen weather effects with realistic particle distribution

### ✨ Visual Effects
- **Fire & Smoke**: Realistic fire with rising smoke and wind effects
- **Sparks**: Explosive spark effects with collision and gravity
- **Magic Effects**: Customizable magical particle systems
- **Blend Modes**: Alpha, Additive, Multiply, and Screen blending

### 🔧 Advanced Management
- **Independent Effects**: Effects that persist beyond weather changes
- **Effect Grouping**: Bulk operations on related effects
- **Generation System**: Batch clearing of particle generations

### 📊 Performance Monitoring
- **Real-Time Statistics**: Update/render times, throughput, particle counts
- **Performance Profiling**: Detailed timing analysis and bottleneck identification
- **Memory Tracking**: Active particle counts and memory usage monitoring

## SIMD Optimization Details

The ParticleManager has been extensively optimized for SIMD (Single Instruction, Multiple Data) processing to maximize throughput on modern CPUs:

### Structure of Arrays (SoA) Layout

#### SIMD-Friendly Component Storage
```cpp
struct ParticleSoA {
    // Separate arrays for SIMD processing (4-8 particles per instruction)
    std::vector<float> posX, posY;        // Position components
    std::vector<float> velX, velY;        // Velocity components
    std::vector<float> accX, accY;        // Acceleration components
    std::vector<float> lives, maxLives, sizes;
    std::vector<float> rotations, angularVelocities;
    std::vector<uint32_t> colors;
    std::vector<uint8_t> flags, generationIds;
    std::vector<EffectType> effectTypes;
    std::vector<uint8_t> priorities;
};
```

#### Performance Benefits
- **Vectorization**: Modern compilers can automatically vectorize loops operating on separate component arrays
- **Cache Efficiency**: Sequential memory access patterns optimize cache utilization
- **SIMD Instructions**: Position updates can process 4-8 particles simultaneously with SSE/AVX instructions
- **Memory Bandwidth**: Better utilization of memory bandwidth with predictable access patterns

### Cross-Platform SIMD Implementation

ParticleManager leverages the SIMDMath.hpp abstraction layer for cross-platform vectorization with explicit SIMD code paths:

**Supported Platforms:**
- **x86-64**: SSE2 (baseline requirement)
- **ARM64**: NEON (Apple Silicon M1/M2/M3)
- **Fallback**: Scalar implementation for unsupported platforms

**Physics Update with SIMD:**
```cpp
// Process 4 particles simultaneously using SIMDMath abstraction
// Cross-platform: SSE2/NEON/scalar fallback handled automatically
using namespace VoidLight::SIMD;

// Prepare SIMD constants
const Float4 deltaTimeVec = broadcast(deltaTime);
const Float4 atmosphericDragVec = broadcast(0.98f);

// Load 4 particles' data (aligned loads)
Float4 posXv = load4(&particles.posX[i]);
Float4 posYv = load4(&particles.posY[i]);
Float4 velXv = load4(&particles.velX[i]);
Float4 velYv = load4(&particles.velY[i]);
const Float4 accXv = load4(&particles.accX[i]);
const Float4 accYv = load4(&particles.accY[i]);

// Physics update: vel = (vel + acc * dt) * drag
velXv = mul(madd(accXv, deltaTimeVec, velXv), atmosphericDragVec);
velYv = mul(madd(accYv, deltaTimeVec, velYv), atmosphericDragVec);

// Position update: pos = pos + vel * dt
posXv = madd(velXv, deltaTimeVec, posXv);
posYv = madd(velYv, deltaTimeVec, posYv);

// Store results back to memory
store4(&particles.velX[i], velXv);
store4(&particles.velY[i], velYv);
store4(&particles.posX[i], posXv);
store4(&particles.posY[i], posYv);
```

**Active Flag Filtering with SIMD:**
```cpp
using namespace VoidLight::SIMD;  // Use SIMDMath abstraction layer

// Check 4 particles' active flags using SIMD byte operations
const Byte16 flagsv = load_byte16(&particles.flags[i]);
const Byte16 activeMask = broadcast_byte(static_cast<uint8_t>(UnifiedParticle::FLAG_ACTIVE));
const Byte16 activev = bitwise_and_byte(flagsv, activeMask);
const Byte16 gt0 = cmpgt_byte(activev, setzero_byte());
const int maskBits = movemask_byte(gt0);
bool anyActive = (maskBits & 0xF) != 0;

// Skip SIMD processing if no particles are active
if (!anyActive) continue;
```

#### Performance Improvements
- **2-4x Throughput**: SIMD processing handles 4 particles per instruction vs 1 in scalar code
- **Reduced Memory Pressure**: Better cache utilization reduces memory bandwidth requirements
- **Platform-Optimized**: Automatic selection of SSE2 (x86-64) or NEON (ARM64) path
- **Aligned Memory**: 16-byte aligned allocations enable faster loads/stores
- **FMA Utilization**: Fused multiply-add instructions (`madd`) reduce instruction count

### Memory Layout Optimization

#### Cache-Friendly Access Patterns
```cpp
// OLD: Array of Structures (AoS) - poor cache utilization
struct Particle { Vector2D pos, vel; float life; };
std::vector<Particle> particles; // Scattered memory access

// NEW: Structure of Arrays (SoA) - optimal cache utilization
std::vector<float> posX, posY, velX, velY, lives; // Sequential access
```

#### Performance Statistics Improvements
- **Instantaneous Rate Calculation**: Prevents metric overflow issues seen in cumulative averaging
- **Frame-Based Metrics**: Accurate per-frame performance measurement
- **SIMD-Aware Profiling**: Performance counters account for vectorized operations

**Implementation Details:**
- Particle physics updates process 4 particles per SIMD iteration
- Scalar pre-loop aligns to 4-particle boundaries for optimal SIMD performance
- Scalar tail loop handles remaining particles (count % 4)
- Both SSE2 (x86-64) and NEON (ARM64) paths use identical SIMDMath abstraction
- See [SIMDMath Documentation](../utils/SIMDMath.md) for complete API reference

## Core Classes and Structures

### UnifiedParticle (All Data in One Structure)
```cpp
struct UnifiedParticle {
    // All particle data unified - no synchronization issues
    Vector2D position;           // Current position
    Vector2D velocity;           // Velocity vector  
    Vector2D acceleration;       // Acceleration vector
    float life;                  // Current life
    float maxLife;               // Maximum life
    float size;                  // Particle size
    float rotation;              // Current rotation
    float angularVelocity;       // Angular velocity
    uint32_t color;              // RGBA color packed
    uint16_t textureIndex;       // Texture index
    uint8_t flags;               // Active, visible, weather, fade flags
    uint8_t generationId;        // Generation for batch clearing
    
    // Flag bit definitions
    static constexpr uint8_t FLAG_ACTIVE = 1 << 0;
    static constexpr uint8_t FLAG_VISIBLE = 1 << 1;
    static constexpr uint8_t FLAG_GRAVITY = 1 << 2;
    static constexpr uint8_t FLAG_COLLISION = 1 << 3;
    static constexpr uint8_t FLAG_WEATHER = 1 << 4;
    static constexpr uint8_t FLAG_FADE_OUT = 1 << 5;
};
```

### ParticleEffectDefinition
```cpp
struct ParticleEffectDefinition {
    std::string name;
    ParticleEffectType type;
    ParticleEmitterConfig emitterConfig;
    std::vector<std::string> textureIDs;
    float intensityMultiplier;
    bool autoTriggerOnWeather;
};
```

## Effect Types

### Built-in Weather Effects

| Effect Type | Description | Performance | Visual Characteristics |
|-------------|-------------|-------------|----------------------|
| **Rain** | Realistic rainfall with wind | 300 particles/sec | Blue droplets, downward motion with drift |
| **Heavy Rain** | Intense rainfall | 500 particles/sec | Denser, faster droplets |
| **Snow** | Gentle snowfall | 180 particles/sec | White flakes, slow descent with wind |
| **Heavy Snow** | Blizzard conditions | 350 particles/sec | Dense, varied snowflake sizes |
| **Fog** | Atmospheric fog effect | 38 particles/sec | Large, semi-transparent gray particles |
| **Cloudy** | Moving cloud wisps | 1.2 particles/sec | Large, light particles with horizontal motion |
| **Windy** | Wind streaks | 80 particles/sec | Thin horizontal streaks (API/test only; no weather maps to it) |
| **WindyDust** | Dust clouds | 150 particles/sec | Brown dust blown horizontally |
| **WindyStorm** | Storm debris | 100 particles/sec | Leaves and debris in strong wind |

### Weather Variant Selection

`ParticleManager::weatherEffectFor(WeatherType, std::string_view customName)` is
the only weather-to-variant mapping. The variant is keyed by `WeatherType`;
intensity never selects it.

| WeatherType | Variant |
|-------------|---------|
| Clear | none (`stopWeatherEffects`) |
| Cloudy | Cloudy |
| Rainy, Stormy | HeavyRain |
| Foggy | Fog |
| Snowy | HeavySnow |
| Windy | WindyStorm |
| Custom named `Rain` / `HeavyRain` / `Snow` / `HeavySnow` / `Fog` / `WindyDust` / `WindyStorm` | same-name variant |
| Custom, any other name | none; logs a warning and current weather particles stop |

- Weather intensity is stored on the effect instance but does not scale
  emission yet, so Rainy and Stormy both render HeavyRain. Emission scaling
  (or deleting the unused intensity plumbing) is a scheduled follow-up in
  `docs/framework-implementation-slices.md`.
- The light Rain, Snow, and WindyDust variants are reachable through Custom
  weather names (EventDemo cycles them). The streak `Windy` variant is only
  reachable through `triggerWeatherEffect(ParticleEffectType::Windy, ...)`.
- `getActiveWeatherEffect()` returns the running weather variant, or
  `std::nullopt` (diagnostics and tests).

### Built-in Visual Effects

| Effect Type | Description | Performance | Visual Characteristics |
|-------------|-------------|-------------|----------------------|
| **Fire** | Realistic fire with flames | 80 particles/sec | Orange-red-yellow, upward motion, additive blend |
| **Smoke** | Rising smoke effects | 25 particles/sec | Gray particles, upward drift, wind affected |
| **Sparks** | Explosive spark bursts | 150 particles/sec | Bright yellow-orange, physics with gravity |
| **Magic** | Customizable magical effects | Variable | Configurable colors and behaviors |

## API Reference

### Initialization and Lifecycle

```cpp
class ParticleManager {
public:
    // Singleton access
    static ParticleManager& Instance();
    
    // Core lifecycle
    bool init();
    void clean();
    void prepareForStateTransition();
    
    // System status
    bool isInitialized() const;
    bool isShutdown() const;
};
```

### Basic Usage Examples

#### Simple Weather Effect
```cpp
// Trigger rain effect
auto& pm = ParticleManager::Instance();
pm.triggerWeatherEffect("Rainy", 0.8f, 2.0f); // HeavyRain variant, 2s transition

// Stop all weather
pm.stopWeatherEffects(1.5f); // 1.5s fade out
```

#### Independent Effect Management
```cpp
// Play a fire effect that persists
uint32_t fireId = pm.playIndependentEffect(ParticleEffectType::Fire, Vector2D(400, 300),
                                          1.0f, -1.0f, "campfire", "fire_crackle");

// Control the effect
pm.pauseIndependentEffect(fireId, true);  // Pause
pm.stopIndependentEffect(fireId);  // Stop
```

#### Custom Effects
Effect definitions are keyed by `ParticleEffectType` (one definition per type)
and are installed by `registerBuiltInEffects()` during `init()`. To add an
effect, add a `ParticleEffectType` value and its definition there, then play it
by type with `playEffect()` or `playIndependentEffect()`.

### Effect Management

```cpp
// Basic effect control
uint32_t playEffect(ParticleEffectType effectType,
                   const Vector2D& position,
                   float intensity = 1.0f);

void stopEffect(uint32_t effectId);
bool isEffectPlaying(uint32_t effectId) const;

// Independent effects (persist beyond weather changes)
uint32_t playIndependentEffect(ParticleEffectType effectType,
                              const Vector2D& position,
                              float intensity = 1.0f,
                              float duration = -1.0f,
                              const std::string& groupTag = "",
                              const std::string& soundEffect = "");

void stopIndependentEffect(uint32_t effectId);
void stopAllIndependentEffects();
void stopIndependentEffectsByGroup(const std::string& groupTag);
void pauseIndependentEffect(uint32_t effectId, bool paused);
```

### Weather Integration

```cpp
// Production path: a state's EventTypeId::Weather handler forwards here
void handleWeatherEvent(const EventData& data);

// Weather -> variant mapping (pure, any thread) and diagnostics
static std::optional<ParticleEffectType>
weatherEffectFor(WeatherType type, std::string_view customName = {});
std::optional<ParticleEffectType> getActiveWeatherEffect() const;

// Name overload: resolves via WeatherEvent::weatherTypeFromName + weatherEffectFor
void triggerWeatherEffect(const std::string& weatherType,
                         float intensity,
                         float transitionTime = 2.0f);
// Enum overload: runs exactly this variant
void triggerWeatherEffect(ParticleEffectType effectType,
                         float intensity,
                         float transitionTime = 2.0f);

void stopWeatherEffects(float transitionTime = 2.0f);
void clearWeatherGeneration(uint8_t generationId = 0, float fadeTime = 0.5f);

// Built-in weather effect toggles
void toggleFireEffect();
void toggleSmokeEffect();
void toggleSparksEffect();
```

### Rendering Pipeline

```cpp
void renderGPU(VoidLight::GPURenderer& gpuRenderer, SDL_GPURenderPass* scenePass);
```

### Global Controls

```cpp
// System-wide controls
void setGlobalPause(bool paused);
bool isGloballyPaused() const;

void setGlobalVisibility(bool visible);
bool isGloballyVisible() const;

void setCameraViewport(float x, float y, float width, float height);
```

### Performance and Threading

```cpp
// Debug-only benchmarking toggle (compiles out in release). When disabled,
// update() forces the single-threaded path even if WorkerBudget would thread.
VOIDLIGHT_DEBUG_ONLY(void enableThreading(bool enable);)

// Performance monitoring
ParticlePerformanceStats getPerformanceStats() const;
void resetPerformanceStats();
size_t getActiveParticleCount() const;
size_t getMaxParticleCapacity() const;
```

`update()` asks `WorkerBudgetManager::shouldUseThreading(SystemType::Particle, activeCount)` once per frame and either calls the threaded batch path (`getOptimalWorkers` / `getBatchStrategy`, `ThreadSystem` batches) or the single-threaded path. Batch futures are joined before the buffer swap and deactivation scan, and `reportExecution()` runs after that join. There is no public threading threshold; WorkerBudget owns the decision.

### Memory Management

```cpp
// Capacity management
void setMaxParticles(size_t maxParticles);
size_t getMaxParticleCapacity() const;
size_t getActiveParticleCount() const;
size_t countActiveParticles() const; // scans storage
```

There is no public compaction or cleanup call. `update()` deactivates expired particles and returns their indices to a free-index pool; indices become reusable after two frames, once worker batches that could still read them have completed.

## Integration Examples

### Game Loop Integration

```cpp
// GameEngine::update() — manager slot (main thread)
// WorkerBudget threading decision happens inside update()
mp_particleManager->update(deltaTime);

// A state's renderGPUScene() draws particles into the engine-owned scene pass
void GamePlayState::renderGPUScene(VoidLight::GPURenderer& gpuRenderer,
                                   SDL_GPURenderPass* scenePass, ...) {
    auto& particleMgr = ParticleManager::Instance();
    if (particleMgr.isInitialized() && !particleMgr.isShutdown()) {
        particleMgr.renderGPU(gpuRenderer, scenePass);
    }
}
```

### EventManager Weather Integration

EventManager only dispatches weather events. The state that owns the screen
registers a transient `EventTypeId::Weather` handler in `enter()` that forwards
to ParticleManager (see `GamePlayState::registerEventHandlers()`):

```cpp
m_weatherEventToken = eventMgr.registerHandlerWithToken(
    EventTypeId::Weather,
    [this](const EventData& data) {
        ParticleManager::Instance().handleWeatherEvent(data);
        onWeatherChanged(data);
    });

// Anywhere: dispatch a weather change
EventManager::Instance().changeWeather("Rainy", 3.0f);
```

`handleWeatherEvent()` reads the event's `WeatherType` (and custom name) and
applies `weatherEffectFor()`, passing the event's intensity and transition
time. Events from `changeWeather()` and `EventFactory` carry the same type and
per-type defaults, so they select the same variant.

### State Transition Handling

`GameStateManager` does not call `ParticleManager::prepareForStateTransition()`.
The exiting state calls it during its manager cleanup (last in the AI-heavy
cleanup order):

```cpp
bool GamePlayState::exit() {
    // ... AIManager, ProjectileManager, ... WorkerBudgetManager first
    auto& particleMgr = ParticleManager::Instance();
    if (particleMgr.isInitialized() && !particleMgr.isShutdown()) {
        particleMgr.prepareForStateTransition();
    }
    // ...
}
```

## Performance Optimization

### WorkerBudget Threading

The ParticleManager integrates with the engine's WorkerBudget system for optimal performance with intelligent queue pressure management:

```cpp
void optimizeParticleProcessing() {
    // No manager-side threshold: update() asks WorkerBudget each frame.
    // The system automatically:
    // - Monitors queue pressure (90% capacity threshold)
    // - Uses optimal worker count based on WorkerBudget
    // - Gracefully degrades to single-threaded if queue is full
    // - Dynamically adjusts batch sizes based on load
}
```

### Performance Characteristics

| Particle Count | CPU Usage | Memory Usage | Threading Strategy |
|----------------|-----------|--------------|-------------------|
| 0-1,000 | <0.5% | <100KB | Single-threaded |
| 1,000-5,000 | 0.5-2% | 100-500KB | WorkerBudget threading (optimal batch size) |
| 5,000-10,000 | 2-4% | 500KB-1MB | Multi-threaded with queue pressure monitoring |
| 10,000-50,000 | 4-8% | 1-5MB | Full WorkerBudget allocation with graceful degradation |
| 50,000+ | 8-12% | 5-10MB | Automatic fallback to single-threaded if queue pressure high |

**SIMD Performance Impact:**
- **Physics Updates**: 2-4x faster with SIMD vs scalar implementation
- **Active Flag Checks**: SIMD byte operations enable efficient batch filtering
- **Overall Particle System**: 30-50% performance improvement with SIMD optimizations
- **Platform Scaling**: Near-identical performance on x86-64 (SSE2) and ARM64 (NEON)

### Memory Optimization Tips

```cpp
// Cap particle storage for known particle loads
pm.setMaxParticles(5000);

// Monitor performance
auto stats = pm.getPerformanceStats();
if (stats.particlesPerSecond < 1000) {
    // Consider reducing particle density
}
```

## Advanced Usage

### Grouped Effect Management

```cpp
// Create a campfire scene with grouped effects
uint32_t fireId = pm.playIndependentEffect(ParticleEffectType::Fire, Vector2D(400, 350),
                                          1.0f, -1.0f, "campfire");
uint32_t smokeId = pm.playIndependentEffect(ParticleEffectType::Smoke, Vector2D(400, 320),
                                           0.8f, -1.0f, "campfire");
uint32_t sparksId = pm.playIndependentEffect(ParticleEffectType::Sparks, Vector2D(400, 340),
                                            0.3f, 5.0f, "campfire");

// Control all campfire effects together
pm.pauseIndependentEffectsByGroup("campfire", true);   // Pause scene
pm.pauseIndependentEffectsByGroup("campfire", false);  // Resume scene
pm.stopIndependentEffectsByGroup("campfire");          // Stop scene
```

### Performance Monitoring and Debugging

```cpp
void monitorParticlePerformance() {
    auto stats = pm.getPerformanceStats();
    
    std::cout << "Particle Performance Report:\n";
    std::cout << "Active Particles: " << pm.getActiveParticleCount() << "\n";
    std::cout << "Max Capacity: " << pm.getMaxParticleCapacity() << "\n";
    std::cout << "Update Time: " << stats.totalUpdateTime / stats.updateCount << "ms avg\n";
    std::cout << "Render Time: " << stats.totalRenderTime / stats.renderCount << "ms avg\n";
    std::cout << "Throughput: " << stats.particlesPerSecond << " particles/sec\n";
    
    // Alert on performance issues
    if (stats.particlesPerSecond < 5000) {
        std::cout << "Warning: Low particle throughput detected\n";
    }
    
    if (pm.getActiveParticleCount() > pm.getMaxParticleCapacity() * 0.9) {
        std::cout << "Warning: Near particle capacity limit\n";
    }
}
```

## Best Practices

### ✅ Optimal Usage Patterns

```cpp
// ✅ Pick the variant by weather type or custom name
pm.triggerWeatherEffect("Rain", 1.0f);   // Light rain (Custom name)
pm.triggerWeatherEffect("Rainy", 1.0f);  // Heavy rain (Rainy -> HeavyRain)

// ✅ Group related effects
pm.playIndependentEffect(ParticleEffectType::Fire, pos, 1.0f, -1.0f, "torch_group");
pm.playIndependentEffect(ParticleEffectType::Smoke, pos, 0.6f, -1.0f, "torch_group");

// ✅ Use proper cleanup
pm.prepareForStateTransition();  // Between game states

// ✅ Monitor performance
auto stats = pm.getPerformanceStats();
if (stats.particlesPerSecond < threshold) {
    adjustParticleSettings();
}
```

### ❌ Anti-Patterns to Avoid

```cpp
// ❌ Don't create too many simultaneous weather effects
pm.triggerWeatherEffect("Rainy", 1.0f);
pm.triggerWeatherEffect("Snowy", 1.0f);  // Conflicts with rain

// ❌ Don't ignore performance monitoring
// Always check particle counts and performance stats

// ❌ Don't skip state transition cleanup
// Always call prepareForStateTransition() between states

// ❌ Don't use extremely high emission rates
config.emissionRate = 10000.0f;  // Will overwhelm system
```

### Performance Guidelines

| Particle Count | Recommendation | Expected Performance |
|----------------|----------------|---------------------|
| **0-1,000** | Single effects, light weather | 60+ FPS, <1% CPU |
| **1,000-5,000** | Multiple effects, moderate weather | 60+ FPS, 1-3% CPU |
| **5,000-10,000** | Complex scenes, heavy weather | 60+ FPS, 3-5% CPU |
| **10,000+** | Extreme effects (use sparingly) | 45+ FPS, 5-8% CPU |

## Error Handling and Debugging

### Common Issues and Solutions

| Issue | Symptoms | Solution |
|-------|----------|----------|
| **Low Performance** | Frame drops, high CPU | Reduce emission rates, enable threading |
| **Memory Growth** | Increasing RAM usage | Lower `setMaxParticles()` or emission rates; slots are reused, not compacted |
| **Visual Artifacts** | Particles not rendering | Check global visibility and camera viewport |
| **Effect Not Playing** | No particles visible | Verify effect registration and position |

### Debug Information

```cpp
// Enable debug logging
#define PARTICLE_LOG(x) std::cout << "[Particle Manager] " << x << std::endl

// Check system status
if (!pm.isInitialized()) {
    std::cout << "Error: ParticleManager not initialized\n";
}

// Monitor active effects
auto activeEffects = pm.getActiveIndependentEffects();
std::cout << "Active independent effects: " << activeEffects.size() << "\n";

// Verify an effect type has a definition
uint32_t testId = pm.playEffect(ParticleEffectType::Fire, Vector2D(0, 0));
if (testId == 0) {
    std::cout << "Error: no definition for ParticleEffectType::Fire\n";
}
```

## Production Deployment

### Performance Validation Checklist

- [ ] **Particle Limits**: Verify max particle counts don't exceed 10,000 under normal gameplay
- [ ] **Memory Usage**: Confirm total particle memory stays under 2MB
- [ ] **Frame Rate**: Maintain 60+ FPS with full weather effects active
- [ ] **Threading**: Validate WorkerBudget integration works correctly
- [ ] **State Transitions**: Test clean transitions between game states
- [ ] **Weather Integration**: Verify EventManager weather triggers work properly

### Platform-Specific Considerations

```cpp
// Mobile optimization
#ifdef MOBILE_PLATFORM
    pm.setMaxParticles(3000);  // Lower capacity
#endif

// High-end PC optimization
#ifdef HIGH_END_PC
    pm.setMaxParticles(15000);  // Higher capacity
#endif
```

## Conclusion

The ParticleManager provides a robust, high-performance foundation for visual effects in game development. With its advanced memory management, threading optimization, and comprehensive effect system, it enables rich visual experiences while maintaining excellent performance. The seamless integration with weather systems and flexible effect management makes it suitable for both simple indie games and complex AAA productions.

### Key Takeaways

- **High Performance**: Unified Particle Architecture and WorkerBudget threading deliver optimal performance
- **Rich Visual Effects**: Comprehensive built-in effects with full customization support  
- **Weather Integration**: Seamless EventManager integration for dynamic weather systems
- **Production Ready**: Thorough error handling, performance monitoring, and debugging support
- **Flexible Architecture**: Easy to extend with custom effects and behaviors

The ParticleManager transforms complex particle system management into simple, high-level API calls while delivering professional-grade performance and visual quality.

## See Also

**Related Systems:**
- [EventManager](../events/EventManager.md) - Weather event integration and particle triggering
- [ThreadSystem](../core/ThreadSystem.md) - WorkerBudget allocation and threading model
- [SIMDMath](../utils/SIMDMath.md) - Cross-platform SIMD abstraction layer used for particle physics

**Integration Examples:**
- [GameEngine](../core/GameEngine.md) - Integration with main game loop
- [Camera](../utils/Camera.md) - Camera-aware particle culling and rendering
- [WorldManager](../managers/WorldManager.md) - Environmental particles for world tiles
