/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PARTICLE_MANAGER_HPP
#define PARTICLE_MANAGER_HPP

/**
 * @file ParticleManager.hpp
 * @brief High-performance particle system manager optimized for speed and
 * efficiency
 *
 * Ultra-high-performance ParticleManager with EventManager integration:
 * - Cache-friendly Structure of Arrays (SoA) layout for optimal performance
 * - Type-indexed particle storage for fast effect dispatch
 * - Lock-free double buffering for concurrent updates
 * - Object pooling and batch processing for memory efficiency
 * - SIMD optimizations for physics calculations
 * - Automatic integration with EventManager weather effects
 * - Scales to 10k+ particles while maintaining 60+ FPS
 * - Thread-safe design with minimal lock contention
 */

#include "managers/EventManager.hpp"
#include "utils/Vector2D.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <future>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for GPU rendering
namespace VoidLight {
class GPURenderer;
}

// Lightweight aligned allocator to support SIMD-friendly vector storage
template <typename T, std::size_t Alignment> struct AlignedAllocator {
  using value_type = T;

  AlignedAllocator() noexcept {}
  template <class U>
  explicit AlignedAllocator(const AlignedAllocator<U, Alignment> &) noexcept {}

  [[nodiscard]] T *allocate(std::size_t n) {
    if (n > static_cast<std::size_t>(-1) / sizeof(T)) {
      throw std::bad_array_new_length();
    }
    void *p = ::operator new[](n * sizeof(T), std::align_val_t(Alignment));
    if (!p)
      throw std::bad_alloc();
    return static_cast<T *>(p);
  }

  void deallocate(T *p, std::size_t) noexcept {
    ::operator delete[](p, std::align_val_t(Alignment));
  }

  template <class U> struct rebind {
    using other = AlignedAllocator<U, Alignment>;
  };
};

template <class T1, std::size_t A1, class T2, std::size_t A2>
constexpr bool operator==(const AlignedAllocator<T1, A1> &,
                          const AlignedAllocator<T2, A2> &) noexcept {
  return A1 == A2;
}
template <class T1, std::size_t A1, class T2, std::size_t A2>
constexpr bool operator!=(const AlignedAllocator<T1, A1> &,
                          const AlignedAllocator<T2, A2> &) noexcept {
  return A1 != A2;
}

// Forward declarations
class TextureManager;

namespace VoidLight {
struct WorkerBudget;
}

// Use the proper logging system for thread-safe logging
// Note: This header only contains the LOG macro declaration, the actual include
// is in the .cpp file

/**
 * @brief Particle effect type enumeration for fast dispatch
 */
enum class ParticleEffectType : uint8_t {
  Rain = 0,
  HeavyRain = 1,
  Snow = 2,
  HeavySnow = 3,
  Fog = 4,
  Cloudy = 5,
  Fire = 6,
  Smoke = 7,
  Sparks = 8,
  Magic = 9,
  Custom = 10,
  Windy = 11,
  WindyDust = 12,
  WindyStorm = 13,
  AmbientDust = 14,
  AmbientFirefly = 15,
  COUNT = 16
};

/**
 * @brief Particle blend modes for rendering
 */
enum class ParticleBlendMode : uint8_t {
  Alpha = 0,    // Standard alpha blending
  Additive = 1, // Additive blending for lights/fire
  Multiply = 2, // Multiply blending for shadows/fog
  Screen = 3    // Screen blending for bright effects
};

/**
 * @brief Particle emitter configuration
 */
struct ParticleEmitterConfig {
  Vector2D position{0, 0};       // Emitter position
  Vector2D direction{0, -1};     // Primary emission direction
  float spread{45.0f};           // Spread angle in degrees
  float emissionRate{100.0f};    // Particles per second
  float minSpeed{50.0f};         // Minimum particle speed
  float maxSpeed{150.0f};        // Maximum particle speed
  float minLife{1.0f};           // Minimum particle life
  float maxLife{3.0f};           // Maximum particle life
  float minSize{1.0f};           // Minimum particle size
  float maxSize{4.0f};           // Maximum particle size
  uint32_t minColor{0xFFFFFFFF}; // Minimum color (RGBA)
  uint32_t maxColor{0xFFFFFFFF}; // Maximum color (RGBA)
  Vector2D gravity{0, 98.0f};    // Gravity acceleration
  Vector2D windForce{0, 0};      // Wind force
  bool loops{true};              // Whether emitter loops
  float duration{-1.0f};         // Emitter duration (-1 for infinite)
  // Removed textureID; particles render as SDL rects (no textures)
  ParticleBlendMode blendMode{ParticleBlendMode::Alpha}; // Blend mode

  // Advanced properties
  bool useWorldSpace{true};    // World space vs local space
  bool fullScreenSpawn{false}; // If true, spawn particles randomly across full screen height
  float burstCount{0};         // Particles per burst
  float burstInterval{1.0f};   // Time between bursts
  bool enableCollision{false}; // Enable collision detection
  float bounceDamping{0.8f};   // Collision bounce damping
};

struct ParticleEffectDefinition;

// Helper methods for enum-based classification system
ParticleEffectType weatherStringToEnum(const std::string &weatherType,
                                       float intensity);
std::string_view effectTypeToString(ParticleEffectType type);

// Built-in effect creation helpers
ParticleEffectDefinition createRainEffect();
ParticleEffectDefinition createHeavyRainEffect();
ParticleEffectDefinition createSnowEffect();
ParticleEffectDefinition createHeavySnowEffect();
ParticleEffectDefinition createFogEffect();
ParticleEffectDefinition createCloudyEffect();
ParticleEffectDefinition createFireEffect();
ParticleEffectDefinition createSmokeEffect();
ParticleEffectDefinition createSparksEffect();
ParticleEffectDefinition createMagicEffect();
ParticleEffectDefinition createWindyEffect();
ParticleEffectDefinition createWindyDustEffect();
ParticleEffectDefinition createWindyStormEffect();
ParticleEffectDefinition createAmbientDustEffect();
ParticleEffectDefinition createAmbientFireflyEffect();

struct UnifiedParticle {
  // All particle data in one structure - no synchronization issues
  Vector2D position;
  Vector2D velocity;
  Vector2D acceleration;
  float life;
  float maxLife;
  float size;
  float rotation;
  float angularVelocity;
  uint32_t color;
  uint8_t flags;
  uint8_t generationId;
  ParticleEffectType effectType; // Effect type for this particle

  enum class RenderLayer : uint8_t { Background, World, Foreground } layer;

  // Flags bit definitions
  static constexpr uint8_t FLAG_ACTIVE = 1 << 0;
  static constexpr uint8_t FLAG_VISIBLE = 1 << 1;
  static constexpr uint8_t FLAG_GRAVITY = 1 << 2;
  static constexpr uint8_t FLAG_COLLISION = 1 << 3;
  static constexpr uint8_t FLAG_WEATHER = 1 << 4;
  static constexpr uint8_t FLAG_FADE_OUT = 1 << 5;
  static constexpr uint8_t FLAG_RECENTLY_DEACTIVATED =
      1 << 6; // Marks particle for pool collection (single-thread)

  UnifiedParticle()
      : position(0, 0), velocity(0, 0), acceleration(0, 0), life(0.0f),
        maxLife(1.0f), size(2.0f), rotation(0.0f), angularVelocity(0.0f),
        color(0xFFFFFFFF), flags(0), generationId(0),
        effectType(ParticleEffectType::Custom), layer(RenderLayer::World) {}

  bool isActive() const;
  void setActive(bool active);

  bool isVisible() const;
  void setVisible(bool visible);

  bool isWeatherParticle() const;
  void setWeatherParticle(bool weather);

  bool isFadingOut() const;
  void setFadingOut(bool fading);

  float getLifeRatio() const;
};

/**
 * @brief Particle effect definition combining emitter and behavior
 */
struct ParticleEffectDefinition {
  std::string name;
  ParticleEffectType type;
  ParticleEmitterConfig emitterConfig;
  // Removed textureIDs; particles render as SDL rects
  float intensityMultiplier{1.0f};     // Effect intensity scaling
  bool autoTriggerOnWeather{false};    // Auto-trigger on weather events
  UnifiedParticle::RenderLayer layer{
      UnifiedParticle::RenderLayer::World}; // Default render layer

  ParticleEffectDefinition() : type(ParticleEffectType::Custom) {}
  ParticleEffectDefinition(const std::string &n, ParticleEffectType t)
      : name(n), type(t) {}
};

/**
 * @brief Performance statistics for monitoring
 */
struct ParticlePerformanceStats {
  double totalUpdateTime{0.0};
  double totalRenderTime{0.0};
  uint64_t updateCount{0};
  uint64_t renderCount{0};
  size_t activeParticles{0};
  size_t maxParticles{0};
  double particlesPerSecond{0.0};

  void addUpdateSample(double timeMs, size_t particleCount);
  void addRenderSample(double timeMs);
  void reset();
};

// Threading info for debug logging (passed via local vars, not stored)
struct ParticleThreadingInfo {
  size_t workerCount{0};
  size_t availableWorkers{0};
  size_t budget{0};
  size_t batchCount{1};
  bool wasThreaded{false};
  double batchTimeMs{0.0};  // Tight timing around actual work only (feeds WorkerBudget)
};

/**
 * @brief Ultra-high-performance ParticleManager
 */
class ParticleManager {
public:
  static ParticleManager &Instance() {
    static ParticleManager instance;
    return instance;
  }

  /**
   * @brief Initializes the ParticleManager and its internal systems
   * @return true if initialization successful, false otherwise
   */
  bool init();

  /**
   * @brief Checks if the Particle Manager has been initialized
   * @return true if initialized, false otherwise
   */
  bool isInitialized() const;

  /**
   * @brief Cleans up all particle resources and marks manager as shut down
   */
  void clean();

  /**
   * @brief Prepares for state transition by safely cleaning up and resetting
   * the particle system
   * @details Stops all weather effects, cleans up inactive particles, resets
   * performance stats, and leaves the system ready for immediate reuse in the
   * new state. Call this during exit() in game states for a clean transition.
   */
  void prepareForStateTransition();

  /**
   * @brief Updates all active particles using high-performance batch processing
   *
   * PERFORMANCE FEATURES:
   * - Lock-free double buffering for zero contention
   * - Cache-efficient SoA layout for 3-4x better performance
   * - SIMD optimizations for physics calculations
   * - Frustum culling for off-screen particles
   * - Batch processing with threading support
   *
   * @param deltaTime Time elapsed since last update in seconds
   */
  void update(float deltaTime);

  /**
   * @brief Records particle vertices to GPU vertex pool for GPU rendering
   * @param gpuRenderer GPU renderer instance
   * @param cameraX Camera X offset for world-space rendering
   * @param cameraY Camera Y offset for world-space rendering
   * @param interpolationAlpha Interpolation factor (0.0-1.0) for smooth rendering
   */
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer, float cameraX,
                         float cameraY, float interpolationAlpha);

  /**
   * @brief Renders particles using GPU pipeline
   * @param gpuRenderer GPU renderer instance
   * @param scenePass Active scene render pass
   */
  void renderGPU(VoidLight::GPURenderer& gpuRenderer, SDL_GPURenderPass* scenePass);

  /**
   * @brief Checks if ParticleManager has been shut down
   * @return true if manager is shut down, false otherwise
   */
  bool isShutdown() const;

  void handleParticleEffectEvent(const EventData& data);
  void handleWeatherEvent(const EventData& data);

  // Effect Management
  /**
   * @brief Registers a particle effect definition for use
   * @param effectDef Effect definition to register
   * @return true if registration successful, false otherwise
   */
  bool registerEffect(const ParticleEffectDefinition &effectDef);

  /**
   * @brief Creates and plays a particle effect at specified position
   * @param effectType Type of the effect to play
   * @param position World position to play effect
   * @param intensity Effect intensity multiplier (0.0 to 2.0)
   * @return Effect ID for controlling the effect, or 0 if failed
   */
  uint32_t playEffect(ParticleEffectType effectType, const Vector2D &position,
                      float intensity = 1.0f);

  /**
   * @brief Stops a currently playing effect
   * @param effectId Effect ID returned from playEffect
   */
  void stopEffect(uint32_t effectId);

  /**
   * @brief Sets the intensity of a playing effect
   * @param effectId Effect ID returned from playEffect
   * @param intensity New intensity value (0.0 to 2.0)
   */
  void setEffectIntensity(uint32_t effectId, float intensity);

  /**
   * @brief Checks if an effect is currently playing
   * @param effectId Effect ID to check
   * @return true if effect is playing, false otherwise
   */
  bool isEffectPlaying(uint32_t effectId) const;

  // Independent Effect Management
  /**
   * @brief Creates and plays an independent particle effect that persists until
   * manually stopped
   * @param effectType Type of the effect to play
   * @param position World position to play effect
   * @param intensity Effect intensity multiplier (0.0 to 2.0)
   * @param duration Effect duration in seconds (-1 for infinite)
   * @param groupTag Optional group tag for bulk operations
   * @param soundEffect Optional sound effect name for SoundManager
   * @return Effect ID for controlling the effect, or 0 if failed
   */
  uint32_t playIndependentEffect(ParticleEffectType effectType,
                                 const Vector2D &position,
                                 float intensity = 1.0f, float duration = -1.0f,
                                 const std::string &groupTag = "",
                                 const std::string &soundEffect = "");

  /**
   * @brief Stops an independent effect
   * @param effectId Effect ID returned from playIndependentEffect
   */
  void stopIndependentEffect(uint32_t effectId);

  /**
   * @brief Stops all independent effects
   */
  void stopAllIndependentEffects();

  /**
   * @brief Stops all independent effects with a specific group tag
   * @param groupTag Group tag to stop
   */
  void stopIndependentEffectsByGroup(const std::string &groupTag);

  /**
   * @brief Pauses/unpauses an independent effect
   * @param effectId Effect ID to pause/unpause
   * @param paused Whether to pause the effect
   */
  void pauseIndependentEffect(uint32_t effectId, bool paused);

  /**
   * @brief Pauses/unpauses all independent effects
   * @param paused Whether to pause all independent effects
   */
  void pauseAllIndependentEffects(bool paused);

  /**
   * @brief Pauses/unpauses all independent effects with a specific group tag
   * @param groupTag Group tag to pause/unpause
   * @param paused Whether to pause the effects
   */
  void pauseIndependentEffectsByGroup(const std::string &groupTag, bool paused);

  /**
   * @brief Sets global pause state for all particle updates
   * @param paused true to pause all particle updates, false to resume
   */
  void setGlobalPause(bool paused);

  /**
   * @brief Gets the current global pause state
   * @return true if particle updates are globally paused
   */
  bool isGloballyPaused() const;

  /**
   * @brief Checks if an effect is an independent effect
   * @param effectId Effect ID to check
   * @return true if effect is independent, false otherwise
   */
  bool isIndependentEffect(uint32_t effectId) const;

  /**
   * @brief Gets all active independent effect IDs
   * @return Vector of active independent effect IDs
   * @note Diagnostic/test API. Allocates a fresh vector per call — not for
   *       per-frame use. If a gameplay system ever needs to poll active
   *       effects each frame, add a `void fill(std::vector<uint32_t>& out)`
   *       overload that reuses a caller-owned buffer (same pattern as
   *       CollisionManager::queryRegion).
   */
  std::vector<uint32_t> getActiveIndependentEffects() const;

  /**
   * @brief Gets all active independent effect IDs with a specific group tag
   * @param groupTag Group tag to filter by
   * @return Vector of active independent effect IDs
   * @note Diagnostic/test API. See `getActiveIndependentEffects()` for the
   *       per-frame caveat.
   */
  std::vector<uint32_t>
  getActiveIndependentEffectsByGroup(const std::string &groupTag) const;

  // Effect Toggles for EventManager
  /**
   * @brief Toggles the Fire effect on/off for EventManager
   */
  void toggleFireEffect();

  /**
   * @brief Toggles the Smoke effect on/off for EventManager
   */
  void toggleSmokeEffect();

  /**
   * @brief Toggles the Sparks effect on/off for EventManager
   */
  void toggleSparksEffect();

  // Weather Integration (EventManager callbacks)
  /**
   * @brief Triggers weather particle effects (called by EventManager)
   * @param weatherType Weather type string ("Rainy", "Snowy", etc.)
   * @param intensity Weather intensity (0.0 to 1.0)
   * @param transitionTime Time to transition to new intensity
   */
  void triggerWeatherEffect(const std::string &weatherType, float intensity,
                            float transitionTime = 2.0f);

  /**
   * @brief Triggers weather particle effects using enum type
   * @param effectType Weather effect type
   * @param intensity Weather intensity (0.0 to 1.0)
   * @param transitionTime Time to transition to new intensity
   */
  void triggerWeatherEffect(ParticleEffectType effectType, float intensity,
                            float transitionTime = 2.0f);

  /**
   * @brief Stops all weather effects
   * @param transitionTime Time to fade out effects
   */
  void stopWeatherEffects(float transitionTime = 2.0f);

  /**
   * @brief Clears all weather particles of a specific generation
   * @param generationId Generation ID to clear (0 = all weather particles)
   * @param fadeTime Time to fade out particles before removal
   */
  void clearWeatherGeneration(uint8_t generationId = 0, float fadeTime = 0.5f);

  /**
   * @brief Sets global particle visibility
   * @param visible Whether particles should be rendered
   */
  void setGlobalVisibility(bool visible);

  /**
   * @brief Gets global visibility state
   * @return true if particles are visible, false otherwise
   */
  bool isGloballyVisible() const;

  /**
   * @brief Sets the camera viewport for frustum culling
   * @param x Camera X position
   * @param y Camera Y position
   * @param width Viewport width
   * @param height Viewport height
   */
  void setCameraViewport(float x, float y, float width, float height);

#ifndef NDEBUG
  // Threading configuration (benchmarking only - compiles out in release)
  void enableThreading(bool enable);
#endif

  /**
   * @brief Enables WorkerBudget-aware threading with intelligent resource
   * allocation
   * @param enable Whether to enable WorkerBudget integration
   *
   * WorkerBudget integration provides coordinated thread allocation across all
   * engine subsystems (AI, Particles, Events, etc.) following the engine's
   * architectural patterns. When enabled:
   *
   * - Uses VoidLight::calculateWorkerBudget() for fair thread distribution
   * - Dynamically adjusts worker count based on workload and system pressure
   * - Submits tasks via ThreadSystem::enqueueTaskWithResult() for proper
   * scheduling
   * - Monitors ThreadSystem queue pressure to prevent resource contention
   * - Automatically falls back to single-threaded mode when appropriate
   *
   * This is the recommended threading mode for production use as it ensures
   * the particle system cooperates well with other engine subsystems.
   */
  void enableWorkerBudgetThreading(bool enable);

  /**
   * @brief Updates particles using WorkerBudget-optimized batch processing
   * @param deltaTime Time elapsed since last update
   * @param traversedParticleCount Current sparse particle storage span
   * @param activeParticleCount Current number of active particles used for WorkerBudget scheduling
   * @param outThreadingInfo Output structure populated with threading decision details
   *
   * This method provides the WorkerBudget-aware update path, which:
   * - Calculates optimal thread allocation using WorkerBudget system
   * - Adjusts batch sizes based on ThreadSystem queue pressure
   * - Falls back to single-threaded mode when WorkerBudget is disabled
   * - Respects the threading threshold for efficient resource usage
   *
   * Called automatically by update() when WorkerBudget threading is enabled.
   */
  void updateWithWorkerBudget(float deltaTime, size_t traversedParticleCount,
                              size_t activeParticleCount,
                              ParticleThreadingInfo& outThreadingInfo);

  /**
   * @brief Gets current performance statistics
   * @return Performance statistics structure
   */
  ParticlePerformanceStats getPerformanceStats() const;

  /**
   * @brief Resets performance statistics
   */
  void resetPerformanceStats();

  /**
   * @brief Gets the current number of active particles
   * @return Number of active particles
   */
  size_t getActiveParticleCount() const;

  /**
   * @brief Counts the actual number of active particles in storage
   * @return Number of active particles
   */
  size_t countActiveParticles() const;

  /**
   * @brief Gets the maximum particle capacity
   * @return Maximum number of particles
   */
  size_t getMaxParticleCapacity() const;

  // Memory Management
  // Compaction removed: object pool reuse handles memory efficiently

  /**
   * @brief Sets the maximum number of particles
   * @param maxParticles Maximum particle count
   */
  void setMaxParticles(size_t maxParticles);

  // Built-in Effect Presets
  /**
   * @brief Registers all built-in weather effect presets
   */
  void registerBuiltInEffects();

private:
  ParticleManager() = default;
  ~ParticleManager() {
    if (!m_isShutdown) {
      clean();
    }
  }
  ParticleManager(const ParticleManager &) = delete;
  ParticleManager &operator=(const ParticleManager &) = delete;

  // Effect instance tracking - effects only emit particles, don't own them
  struct EffectInstance {
    uint32_t id;
    ParticleEffectType effectType;
    Vector2D position;
    float intensity;
    float currentIntensity; // For transitions
    float targetIntensity;  // Target during transitions
    float transitionSpeed;  // Transition rate
    float emissionTimer;
    float durationTimer;
    float maxDuration; // Maximum duration (-1 for infinite)
    bool active;
    bool paused; // Independent pause state
    bool isWeatherEffect;
    bool isIndependentEffect;    // Independent effects (not weather)
    std::string groupTag;        // For bulk operations
    std::string soundEffect;     // Associated sound effect
    uint8_t currentGenerationId; // Current generation for new particles

    EffectInstance()
        : id(0), effectType(ParticleEffectType::Custom), position(0, 0),
          intensity(1.0f), currentIntensity(0.0f), targetIntensity(1.0f),
          transitionSpeed(1.0f), emissionTimer(0.0f), durationTimer(0.0f),
          maxDuration(-1.0f), active(false), paused(false),
          isWeatherEffect(false), isIndependentEffect(false), groupTag(""),
          soundEffect(""), currentGenerationId(0) {}
  };

  // New particle request structure for lock-free creation
  struct NewParticleRequest {
    Vector2D position;
    Vector2D velocity;
    Vector2D acceleration;
    float life;
    float size;
    uint32_t color;
    ParticleBlendMode blendMode;
    ParticleEffectType effectType;
    uint8_t flags;
  };

  // Lock-free high-performance storage with double buffering
  struct alignas(64) LockFreeParticleStorage {
    // SoA data layout for cache-friendly updates
    struct ParticleSoA {
      using F32 = float;
      using U32 = uint32_t;
      using U8 = uint8_t;

      // SIMD-friendly SoA float lanes (authoritative storage)
      std::vector<F32, AlignedAllocator<F32, 16>> posX;
      std::vector<F32, AlignedAllocator<F32, 16>> posY;
      std::vector<F32, AlignedAllocator<F32, 16>> prevPosX;  // Previous position for interpolation
      std::vector<F32, AlignedAllocator<F32, 16>> prevPosY;  // Previous position for interpolation
      std::vector<F32, AlignedAllocator<F32, 16>> velX;
      std::vector<F32, AlignedAllocator<F32, 16>> velY;
      std::vector<F32, AlignedAllocator<F32, 16>> accX;
      std::vector<F32, AlignedAllocator<F32, 16>> accY;

      // Other particle attributes
      std::vector<F32, AlignedAllocator<F32, 16>> lives;
      std::vector<F32, AlignedAllocator<F32, 16>> maxLives;
      std::vector<F32, AlignedAllocator<F32, 16>> sizes;
      std::vector<F32, AlignedAllocator<F32, 16>> rotations;
      std::vector<F32, AlignedAllocator<F32, 16>> angularVelocities;
      std::vector<U32, AlignedAllocator<U32, 16>> colors;
      std::vector<U8, AlignedAllocator<U8, 16>> flags;
      std::vector<U8, AlignedAllocator<U8, 16>> generationIds;
      std::vector<ParticleEffectType, AlignedAllocator<ParticleEffectType, 16>>
          effectTypes;
      std::vector<UnifiedParticle::RenderLayer,
                  AlignedAllocator<UnifiedParticle::RenderLayer, 16>>
          layers;

      // CRITICAL: Unified SOA operations to prevent desynchronization
      void resize(size_t newSize);
      void reserve(size_t newCapacity);
      void push_back(const UnifiedParticle &p);
      void clear();
      size_t size() const; // authoritative size = flags.size()
      bool empty() const;

      // Safe erase operations for SOA consistency
      void eraseParticle(size_t index);

      // Validation helpers (debug-oriented)
      bool isFullyConsistent() const;
      size_t getSafeAccessCount() const;

      // Safe random access with bounds checking
      bool isValidIndex(size_t index) const;
      void swapParticles(size_t indexA, size_t indexB);
    };

    // Double-buffered particle arrays for lock-free updates
    ParticleSoA particles[2];
    std::atomic<size_t> activeBuffer{0};  // Which buffer is currently active
    std::atomic<size_t> particleCount{0}; // Current particle count
    std::atomic<size_t> writeHead{0};     // Next write position
    std::atomic<size_t> capacity{0};      // Current capacity

    // Object pool: epoch-based deferred recycling for thread safety
    // Indices are held in pending for 2 frames before becoming available,
    // ensuring background threads from previous frames have completed.
    struct ReleasedIndex {
      size_t index;
      uint64_t releaseEpoch;
    };
    std::vector<ReleasedIndex> pendingIndices;  // Recently freed, not yet safe
    std::vector<size_t> readyIndices;           // Safe to reuse (2+ frames old)
    // Upper bound of currently active indices (last index that may be active)
    size_t maxActiveIndex{0};

    // Lock-free ring buffer for new particle requests
    struct alignas(16) ParticleCreationRequest {
      Vector2D position;
      Vector2D velocity;
      Vector2D acceleration;
      uint32_t color;
      float life;
      float size;
      uint8_t flags;
      uint8_t generationId;
      ParticleEffectType effectType;
      std::atomic<bool> ready{false};
    };

    static constexpr size_t CREATION_RING_SIZE = 4096; // Must be power of 2
    std::array<ParticleCreationRequest, CREATION_RING_SIZE> creationRing;
    std::atomic<size_t> creationHead{0};
    std::atomic<size_t> creationTail{0};

    // Epoch counter for deferred index recycling
    std::atomic<uint64_t> currentEpoch{0};

    LockFreeParticleStorage();

    // Lock-free particle creation
    bool tryCreateParticle(const Vector2D &pos, const Vector2D &vel,
                           const Vector2D &acc, uint32_t color, float life,
                           float size, uint8_t flags, uint8_t genId,
                           ParticleEffectType effectType);

    // Process creation requests (called from update thread)
    void processCreationRequests();

    // Get read-only access to particles
    const ParticleSoA &getParticlesForRead() const;

    // Get writable access to particles (for updates)
    ParticleSoA &getCurrentBuffer();

    // Compaction removed

    // Submit new particle (lock-free)
    bool submitNewParticle(const NewParticleRequest &request);

    // Swap buffers for lock-free updates
    void swapBuffers();

    // Pool helpers for epoch-based deferred recycling
    inline bool hasFreeIndex() const { return !readyIndices.empty(); }

    inline size_t popFreeIndex() {
      const size_t idx = readyIndices.back();
      readyIndices.pop_back();
      return idx;
    }

    inline void pushFreeIndex(size_t idx) {
      pendingIndices.push_back({idx, currentEpoch.load(std::memory_order_relaxed)});
    }

    // Move aged indices from pending to ready (call once per frame after updates)
    inline void promoteSafeIndices() {
      const uint64_t currentEp = currentEpoch.load(std::memory_order_relaxed);
      const uint64_t safeThreshold = (currentEp >= 2) ? (currentEp - 2) : 0;

      size_t writePos = 0;
      for (size_t i = 0; i < pendingIndices.size(); ++i) {
        if (pendingIndices[i].releaseEpoch <= safeThreshold) {
          readyIndices.push_back(pendingIndices[i].index);
        } else {
          pendingIndices[writePos++] = pendingIndices[i];
        }
      }
      pendingIndices.resize(writePos);
    }
  };

  // Core storage - now lock-free
  LockFreeParticleStorage m_storage;
  std::unordered_map<ParticleEffectType, ParticleEffectDefinition>
      m_effectDefinitions;
  std::vector<EffectInstance> m_effectInstances;
  std::unordered_map<uint32_t, size_t> m_effectIdToIndex;

  // Texture management
  // Removed texture index map/IDs; particles are rects

  // Performance tracking
  ParticlePerformanceStats m_performanceStats;

  // Threading and synchronization
  std::atomic<bool> m_initialized{false};
  std::atomic<bool> m_isShutdown{false};
  std::atomic<bool> m_globallyPaused{false};
  std::atomic<bool> m_globallyVisible{true};
  std::atomic<bool> m_useThreading{true};
  std::atomic<bool> m_useWorkerBudget{true};
  // Threading threshold now managed by WorkerBudget adaptive system

  std::atomic<size_t> m_activeCount{0};

  // Camera and culling
  struct CameraViewport {
    float x{0}, y{0}, width{1920}, height{1080};
    float margin{100}; // Extra margin for smooth culling
  } m_viewport;

  // Lock-free synchronization - no mutexes needed for particles
  mutable std::shared_mutex
      m_effectsMutex;              // For effect instances and definitions
  mutable std::mutex m_statsMutex; // Only for performance stats

  // Async batch tracking for safe shutdown using futures
  std::vector<std::future<void>> m_batchFutures;
  std::vector<std::future<void>> m_reusableBatchFutures;  // Swap target to preserve capacity
  std::mutex m_batchFuturesMutex;  // Protect futures vector

  // NOTE: No update mutex - GameEngine handles update/render synchronization

  // Constants for optimization
  static constexpr size_t CACHE_LINE_SIZE = 64;
  static constexpr size_t BATCH_SIZE =
      1024; // Increased for better WorkerBudget efficiency
  static constexpr size_t DEFAULT_MAX_PARTICLES =
      100000; // Increased for modern performance
  static constexpr float MIN_VISIBLE_SIZE = 0.5f;

  // Performance optimization structures
  struct alignas(16) BatchUpdateData {
    float deltaTime;
    size_t startIndex;
    size_t endIndex;
    size_t processedCount;
  };

  // OPTIMIZATION: Pre-allocated render buffer to eliminate per-frame std::fill
  // Previously 33% of CPU time was spent on resize() value-initialization
  // This buffer is allocated once and reused every frame
  struct BatchRenderBuffers {
    static constexpr std::size_t MAX_RECTS_PER_BATCH = 2048;
    static constexpr std::size_t VERTS_PER_QUAD = 6;      // 2 triangles × 3 verts
    static constexpr std::size_t FLOATS_PER_VERT = 2;     // x, y
    static constexpr std::size_t XY_STRIDE = VERTS_PER_QUAD * FLOATS_PER_VERT;
    static constexpr std::size_t COL_STRIDE = VERTS_PER_QUAD;

    std::vector<float> xy;
    std::vector<SDL_FColor> cols;
    std::size_t vertexCount{0};

    BatchRenderBuffers() {
      // Pre-size vectors once - this is the only resize() call
      xy.resize(MAX_RECTS_PER_BATCH * XY_STRIDE);
      cols.resize(MAX_RECTS_PER_BATCH * COL_STRIDE);
    }

    // Reset for new batch (no allocation, just counter reset)
    constexpr void reset() noexcept { vertexCount = 0; }

    // Get the populated vertex count for the current batch.
    [[nodiscard]] constexpr int getVertexCount() const noexcept {
      return static_cast<int>(vertexCount);
    }

    // Safe and fast quad append - uses pre-sized buffer with bounds guarantee
    void appendQuad(float x0, float y0, float x1, float y1,
                    float x2, float y2, float x3, float y3,
                    const SDL_FColor& col) noexcept {
      const std::size_t xyBase = vertexCount * FLOATS_PER_VERT;
      const std::size_t colBase = vertexCount;

      // Triangle 1: v0, v1, v2
      xy[xyBase]      = x0; xy[xyBase + 1]  = y0;
      xy[xyBase + 2]  = x1; xy[xyBase + 3]  = y1;
      xy[xyBase + 4]  = x2; xy[xyBase + 5]  = y2;
      // Triangle 2: v2, v3, v0
      xy[xyBase + 6]  = x2; xy[xyBase + 7]  = y2;
      xy[xyBase + 8]  = x3; xy[xyBase + 9]  = y3;
      xy[xyBase + 10] = x0; xy[xyBase + 11] = y0;

      // All 6 vertices share same color
      cols[colBase]     = col; cols[colBase + 1] = col;
      cols[colBase + 2] = col; cols[colBase + 3] = col;
      cols[colBase + 4] = col; cols[colBase + 5] = col;

      vertexCount += VERTS_PER_QUAD;
    }
  };
  mutable BatchRenderBuffers m_renderBuffer;  // Mutable for use in const render methods

  // Pre-calculated lookup tables for performance
  struct ParticleOptimizationData {
    // Pre-calculated color variations
    std::array<uint32_t, 8> fireColors{{0xFF4500FF, 0xFF6500FF, 0xFFFF00FF,
                                        0xFF8C00FF, 0xFFA500FF, 0xFF0000FF,
                                        0xFFD700FF, 0xFF7F00FF}};
    std::array<uint32_t, 8> smokeColors{{0x404040FF, 0x606060FF, 0x808080FF,
                                         0x202020FF, 0x4A4A4AFF, 0x505050FF,
                                         0x707070FF, 0x303030FF}};
    std::array<uint32_t, 4> sparkColors{
        {0xFFFF00FF, 0xFF8C00FF, 0xFFD700FF, 0xFFA500FF}};

    // Fast random state for each thread
    mutable std::atomic<uint32_t> fastRandSeed{12345};

    ParticleOptimizationData() = default;
  } m_optimizationData;

  // Effect ID generation
  std::atomic<uint32_t> m_nextEffectId{1};

  // Built-in effect state tracking
  uint32_t m_fireEffectId{0};
  uint32_t m_smokeEffectId{0};
  uint32_t m_sparksEffectId{0};
  bool m_fireActive{false};
  bool m_smokeActive{false};
  bool m_sparksActive{false};

  // Helper methods
  uint32_t generateEffectId();
  size_t allocateParticle();
  void releaseParticle(size_t index);
  void updateParticleBatch(size_t start, size_t end, float deltaTime);
  void updateParticleBatchOptimized(size_t start, size_t end, float deltaTime);
  void emitParticles(EffectInstance &effect,
                     const ParticleEffectDefinition &definition,
                     float deltaTime);
  void updateEffectInstance(EffectInstance &effect, float deltaTime);
  void swapBuffers();
  void cleanupInactiveParticles();
  // precondition: caller holds m_effectsMutex (unique_lock)
  void compactInactiveEffectInstances();
  void updateEffectInstances(float deltaTime);
  void updateParticlesThreaded(float deltaTime, size_t traversedParticleCount,
                               size_t activeParticleCount,
                               ParticleThreadingInfo& outThreadingInfo);
  void updateParticlesSingleThreaded(float deltaTime,
                                     size_t traversedParticleCount);
  void updateParticleRange(LockFreeParticleStorage::ParticleSoA &particles,
                           size_t startIdx, size_t endIdx, float deltaTime,
                           float windPhase);

  // SIMD-optimized batch physics update for high-performance processing
  void
  updateParticlePhysicsSIMD(LockFreeParticleStorage::ParticleSoA &particles,
                            size_t startIdx, size_t endIdx, float deltaTime);

  // Batch color processing for alpha fading and color transitions
  void
  batchProcessParticleColors(LockFreeParticleStorage::ParticleSoA &particles,
                             size_t startIdx, size_t endIdx);
  void updateUnifiedParticle(UnifiedParticle &particle, float deltaTime);
  void createParticleForEffect(const ParticleEffectDefinition &effectDef,
                               const Vector2D &position,
                               bool isWeatherEffect = false);
  uint32_t interpolateColor(uint32_t color1, uint32_t color2, float factor);
  void recordPerformance(bool isRender, double timeMs, size_t particleCount);
  uint64_t getCurrentTimeNanos() const;
  // PERFORMANCE OPTIMIZATION: Trigonometric lookup tables for fast math
  static constexpr size_t TRIG_LUT_SIZE = 1024;
  static constexpr float TRIG_LUT_SCALE = TRIG_LUT_SIZE / (2.0f * 3.14159265f);
  std::array<float, TRIG_LUT_SIZE> m_sinLUT{};
  std::array<float, TRIG_LUT_SIZE> m_cosLUT{};
  void initTrigLookupTables();
  // Per-frame wind phase advanced once in update() and snapshot passed to workers
  float m_windPhase{0.0f};

  // Fast trigonometric functions using lookup tables
  inline float fastSin(float x) const {
    // Optimized: avoid fmodf by using integer modulo directly
    // Convert to index space and handle negative values with bitwise AND
    // (only works because TRIG_LUT_SIZE is power of 2)
    const int index = static_cast<int>(x * TRIG_LUT_SCALE);
    // Handle negative indices: add multiple of TRIG_LUT_SIZE, then mask
    const size_t wrappedIndex = (index + (TRIG_LUT_SIZE * 64)) & (TRIG_LUT_SIZE - 1);
    return m_sinLUT[wrappedIndex];
  }

  inline float fastCos(float x) const {
    // Optimized: avoid fmodf by using integer modulo directly
    // Convert to index space and handle negative values with bitwise AND
    // (only works because TRIG_LUT_SIZE is power of 2)
    const int index = static_cast<int>(x * TRIG_LUT_SCALE);
    // Handle negative indices: add multiple of TRIG_LUT_SIZE, then mask
    const size_t wrappedIndex = (index + (TRIG_LUT_SIZE * 64)) & (TRIG_LUT_SIZE - 1);
    return m_cosLUT[wrappedIndex];
  }

  // Weather type conversion helpers
  ParticleEffectType weatherStringToEnum(const std::string &weatherType,
                                         float intensity) const;
  std::string_view effectTypeToString(ParticleEffectType type) const;

  // Built-in effect creation helpers
  ParticleEffectDefinition createRainEffect();
  ParticleEffectDefinition createHeavyRainEffect();
  ParticleEffectDefinition createSnowEffect();
  ParticleEffectDefinition createHeavySnowEffect();
  ParticleEffectDefinition createFogEffect();
  ParticleEffectDefinition createCloudyEffect();
  ParticleEffectDefinition createFireEffect();
  ParticleEffectDefinition createSmokeEffect();
  ParticleEffectDefinition createSparksEffect();
  ParticleEffectDefinition createMagicEffect();
  ParticleEffectDefinition createWindyEffect();
  ParticleEffectDefinition createWindyDustEffect();
  ParticleEffectDefinition createWindyStormEffect();
  ParticleEffectDefinition createAmbientDustEffect();
  ParticleEffectDefinition createAmbientFireflyEffect();
};

#endif // PARTICLE_MANAGER_HPP
