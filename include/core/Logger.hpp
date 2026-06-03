/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef LOGGER_HPP
#define LOGGER_HPP

// Required includes for logging system:
// - string: Used in macro expansions for std::string() conversions
// - cstdio: Required for printf() and fflush() functions
// - cstdint: Required for uint8_t type
// - mutex: Required for thread-safe logging
// - atomic: Required for std::atomic<bool> benchmark mode flag
#include <atomic> // IWYU pragma: keep - Required for std::atomic<bool> benchmark mode flag
#include <cstdint> // IWYU pragma: keep - Required for uint8_t type
#include <cstdio> // IWYU pragma: keep - Required for printf() and fflush() functions
#include <mutex> // IWYU pragma: keep - Required for thread-safe logging
#include <string> // IWYU pragma: keep - Required for std::string() conversions in macros

namespace VoidLight {
enum class LogLevel : uint8_t {
  CRITICAL = 0,     // Always logs (even in release for crashes)
  ERROR_LEVEL = 1,  // Debug only (renamed to avoid macro conflicts)
  WARNING = 2,      // Debug only
  INFO = 3,         // Debug only
  DEBUG_LEVEL = 4   // Debug only (renamed to avoid macro conflicts)
};

#ifdef DEBUG
// Full logging system in debug builds - lockless for safety
class Logger {
private:
  static std::atomic<bool> s_benchmarkMode;
  static std::mutex s_logMutex;

public:
  static void SetBenchmarkMode(bool enabled) {
    s_benchmarkMode.store(enabled, std::memory_order_relaxed);
  }

  static bool IsBenchmarkMode() {
    return s_benchmarkMode.load(std::memory_order_relaxed);
  }

  static void Log(LogLevel level, const char *system,
                  const std::string &message) {
    if (s_benchmarkMode.load(std::memory_order_relaxed)) {
      return;
    }

    // Thread-safe logging with mutex protection
    std::lock_guard<std::mutex> lock(s_logMutex);
    printf("VoidLight Engine - [%s] %s: %s\n", system, getLevelString(level),
           message.c_str());
    if (level == LogLevel::ERROR_LEVEL || level == LogLevel::CRITICAL) {
      fflush(stdout);
    }
  }
  static void Log(LogLevel level, const char *system, const char *message) {
    if (s_benchmarkMode.load(std::memory_order_relaxed)) {
      return;
    }

    // Thread-safe logging with mutex protection
    std::lock_guard<std::mutex> lock(s_logMutex);
    printf("VoidLight Engine - [%s] %s: %s\n", system, getLevelString(level),
           message);
    if (level == LogLevel::ERROR_LEVEL || level == LogLevel::CRITICAL) {
      fflush(stdout);
    }
  }

private:
  static const char *getLevelString(LogLevel level) {
    switch (level) {
    case LogLevel::CRITICAL:
      return "CRITICAL";
    case LogLevel::ERROR_LEVEL:
      return "ERROR";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::DEBUG_LEVEL:
      return "DEBUG";
    default:
      return "UNKNOWN";
    }
  }
};

// Debug build macros - full functionality
#define VOIDLIGHT_CRITICAL(system, msg)                                           \
  VoidLight::Logger::Log(VoidLight::LogLevel::CRITICAL, system, msg)
#define VOIDLIGHT_ERROR(system, msg)                                              \
  VoidLight::Logger::Log(VoidLight::LogLevel::ERROR_LEVEL, system, msg)
#define VOIDLIGHT_WARN(system, msg)                                               \
  VoidLight::Logger::Log(VoidLight::LogLevel::WARNING, system, msg)
#define VOIDLIGHT_INFO(system, msg)                                               \
  VoidLight::Logger::Log(VoidLight::LogLevel::INFO, system, msg)
#define VOIDLIGHT_DEBUG(system, msg)                                              \
  VoidLight::Logger::Log(VoidLight::LogLevel::DEBUG_LEVEL, system, msg)

// Conditional logging macros - use when logging is the ONLY content in an if-block
// These eliminate condition evaluation overhead in release builds
#define VOIDLIGHT_WARN_IF(cond, system, msg)                                      \
  do { if (cond) VOIDLIGHT_WARN(system, msg); } while(0)
#define VOIDLIGHT_INFO_IF(cond, system, msg)                                      \
  do { if (cond) VOIDLIGHT_INFO(system, msg); } while(0)
#define VOIDLIGHT_DEBUG_IF(cond, system, msg)                                     \
  do { if (cond) VOIDLIGHT_DEBUG(system, msg); } while(0)

#else
// Release builds - file-based logging, no console dependency
// Implementations in src/core/Logger.cpp
class Logger {
private:
  static std::atomic<bool> s_benchmarkMode;
  static std::mutex s_logMutex;

public:
  static void SetBenchmarkMode(bool enabled) {
    s_benchmarkMode.store(enabled, std::memory_order_relaxed);
  }

  static bool IsBenchmarkMode() {
    return s_benchmarkMode.load(std::memory_order_relaxed);
  }

  // Declarations only - implementations in Logger.cpp write to file
  static void Log(const char *level, const char *system,
                  const std::string &message);
  static void Log(const char *level, const char *system, const char *message);
};

#define VOIDLIGHT_CRITICAL(system, msg)                                           \
  VoidLight::Logger::Log("CRITICAL", system, msg)

#define VOIDLIGHT_ERROR(system, msg)                                              \
  VoidLight::Logger::Log("ERROR", system, msg)

#define VOIDLIGHT_WARN(system, msg) ((void)0)  // Zero overhead
#define VOIDLIGHT_INFO(system, msg) ((void)0)  // Zero overhead
#define VOIDLIGHT_DEBUG(system, msg) ((void)0) // Zero overhead

// Conditional logging macros - compiled out entirely in release
#define VOIDLIGHT_WARN_IF(cond, system, msg) ((void)0)
#define VOIDLIGHT_INFO_IF(cond, system, msg) ((void)0)
#define VOIDLIGHT_DEBUG_IF(cond, system, msg) ((void)0)
#endif

// Debug-only code block — compiles out entirely in release
#ifdef DEBUG
#define VOIDLIGHT_DEBUG_ONLY(...) __VA_ARGS__
#else
#define VOIDLIGHT_DEBUG_ONLY(...)
#endif

// Convenience macros for each manager and core system

// Core Systems
#define GAMELOOP_CRITICAL(msg) VOIDLIGHT_CRITICAL("GameLoop", msg)
#define GAMELOOP_ERROR(msg) VOIDLIGHT_ERROR("GameLoop", msg)
#define GAMELOOP_WARN(msg) VOIDLIGHT_WARN("GameLoop", msg)
#define GAMELOOP_INFO(msg) VOIDLIGHT_INFO("GameLoop", msg)
#define GAMELOOP_DEBUG(msg) VOIDLIGHT_DEBUG("GameLoop", msg)

#define GAMEENGINE_CRITICAL(msg) VOIDLIGHT_CRITICAL("GameEngine", msg)
#define GAMEENGINE_ERROR(msg) VOIDLIGHT_ERROR("GameEngine", msg)
#define GAMEENGINE_WARN(msg) VOIDLIGHT_WARN("GameEngine", msg)
#define GAMEENGINE_INFO(msg) VOIDLIGHT_INFO("GameEngine", msg)
#define GAMEENGINE_DEBUG(msg) VOIDLIGHT_DEBUG("GameEngine", msg)

#define THREADSYSTEM_CRITICAL(msg) VOIDLIGHT_CRITICAL("ThreadSystem", msg)
#define THREADSYSTEM_ERROR(msg) VOIDLIGHT_ERROR("ThreadSystem", msg)
#define THREADSYSTEM_WARN(msg) VOIDLIGHT_WARN("ThreadSystem", msg)
#define THREADSYSTEM_INFO(msg) VOIDLIGHT_INFO("ThreadSystem", msg)
#define THREADSYSTEM_DEBUG(msg) VOIDLIGHT_DEBUG("ThreadSystem", msg)

#define TIMESTEP_CRITICAL(msg) VOIDLIGHT_CRITICAL("TimestepManager", msg)
#define TIMESTEP_ERROR(msg) VOIDLIGHT_ERROR("TimestepManager", msg)
#define TIMESTEP_WARN(msg) VOIDLIGHT_WARN("TimestepManager", msg)
#define TIMESTEP_INFO(msg) VOIDLIGHT_INFO("TimestepManager", msg)
#define TIMESTEP_DEBUG(msg) VOIDLIGHT_DEBUG("TimestepManager", msg)

#define RESOURCEPATH_CRITICAL(msg) VOIDLIGHT_CRITICAL("ResourcePath", msg)
#define RESOURCEPATH_ERROR(msg) VOIDLIGHT_ERROR("ResourcePath", msg)
#define RESOURCEPATH_WARN(msg) VOIDLIGHT_WARN("ResourcePath", msg)
#define RESOURCEPATH_INFO(msg) VOIDLIGHT_INFO("ResourcePath", msg)
#define RESOURCEPATH_DEBUG(msg) VOIDLIGHT_DEBUG("ResourcePath", msg)

// Manager Systems
#define BGSIM_CRITICAL(msg) VOIDLIGHT_CRITICAL("BackgroundSim", msg)
#define BGSIM_ERROR(msg) VOIDLIGHT_ERROR("BackgroundSim", msg)
#define BGSIM_WARN(msg) VOIDLIGHT_WARN("BackgroundSim", msg)
#define BGSIM_INFO(msg) VOIDLIGHT_INFO("BackgroundSim", msg)
#define BGSIM_DEBUG(msg) VOIDLIGHT_DEBUG("BackgroundSim", msg)
#define PROJ_CRITICAL(msg) VOIDLIGHT_CRITICAL("Projectile", msg)
#define PROJ_ERROR(msg) VOIDLIGHT_ERROR("Projectile", msg)
#define PROJ_WARN(msg) VOIDLIGHT_WARN("Projectile", msg)
#define PROJ_INFO(msg) VOIDLIGHT_INFO("Projectile", msg)
#define PROJ_DEBUG(msg) VOIDLIGHT_DEBUG("Projectile", msg)
#define TEXTURE_CRITICAL(msg) VOIDLIGHT_CRITICAL("TextureManager", msg)
#define TEXTURE_ERROR(msg) VOIDLIGHT_ERROR("TextureManager", msg)
#define TEXTURE_WARN(msg) VOIDLIGHT_WARN("TextureManager", msg)
#define TEXTURE_INFO(msg) VOIDLIGHT_INFO("TextureManager", msg)
#define TEXTURE_DEBUG(msg) VOIDLIGHT_DEBUG("TextureManager", msg)

#define SOUND_CRITICAL(msg) VOIDLIGHT_CRITICAL("SoundManager", msg)
#define SOUND_ERROR(msg) VOIDLIGHT_ERROR("SoundManager", msg)
#define SOUND_WARN(msg) VOIDLIGHT_WARN("SoundManager", msg)
#define SOUND_INFO(msg) VOIDLIGHT_INFO("SoundManager", msg)
#define SOUND_DEBUG(msg) VOIDLIGHT_DEBUG("SoundManager", msg)

#define FONT_CRITICAL(msg) VOIDLIGHT_CRITICAL("FontManager", msg)
#define FONT_ERROR(msg) VOIDLIGHT_ERROR("FontManager", msg)
#define FONT_WARN(msg) VOIDLIGHT_WARN("FontManager", msg)
#define FONT_INFO(msg) VOIDLIGHT_INFO("FontManager", msg)
#define FONT_DEBUG(msg) VOIDLIGHT_DEBUG("FontManager", msg)

#define PARTICLE_CRITICAL(msg) VOIDLIGHT_CRITICAL("ParticleManager", msg)
#define PARTICLE_ERROR(msg) VOIDLIGHT_ERROR("ParticleManager", msg)
#define PARTICLE_WARN(msg) VOIDLIGHT_WARN("ParticleManager", msg)
#define PARTICLE_INFO(msg) VOIDLIGHT_INFO("ParticleManager", msg)
#define PARTICLE_DEBUG(msg) VOIDLIGHT_DEBUG("ParticleManager", msg)

#define AI_CRITICAL(msg) VOIDLIGHT_CRITICAL("AIManager", msg)
#define AI_ERROR(msg) VOIDLIGHT_ERROR("AIManager", msg)
#define AI_WARN(msg) VOIDLIGHT_WARN("AIManager", msg)
#define AI_INFO(msg) VOIDLIGHT_INFO("AIManager", msg)
#define AI_DEBUG(msg) VOIDLIGHT_DEBUG("AIManager", msg)

#define EVENT_CRITICAL(msg) VOIDLIGHT_CRITICAL("EventManager", msg)
#define EVENT_ERROR(msg) VOIDLIGHT_ERROR("EventManager", msg)
#define EVENT_WARN(msg) VOIDLIGHT_WARN("EventManager", msg)
#define EVENT_INFO(msg) VOIDLIGHT_INFO("EventManager", msg)
#define EVENT_DEBUG(msg) VOIDLIGHT_DEBUG("EventManager", msg)

#define INPUT_CRITICAL(msg) VOIDLIGHT_CRITICAL("InputManager", msg)
#define INPUT_ERROR(msg) VOIDLIGHT_ERROR("InputManager", msg)
#define INPUT_WARN(msg) VOIDLIGHT_WARN("InputManager", msg)
#define INPUT_INFO(msg) VOIDLIGHT_INFO("InputManager", msg)
#define INPUT_DEBUG(msg) VOIDLIGHT_DEBUG("InputManager", msg)
#define INPUT_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "InputManager", msg)
#define INPUT_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "InputManager", msg)
#define INPUT_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "InputManager", msg)

#define UI_CRITICAL(msg) VOIDLIGHT_CRITICAL("UIManager", msg)
#define UI_ERROR(msg) VOIDLIGHT_ERROR("UIManager", msg)
#define UI_WARN(msg) VOIDLIGHT_WARN("UIManager", msg)
#define UI_INFO(msg) VOIDLIGHT_INFO("UIManager", msg)
#define UI_DEBUG(msg) VOIDLIGHT_DEBUG("UIManager", msg)

#define CAMERA_CRITICAL(msg) VOIDLIGHT_CRITICAL("Camera", msg)
#define CAMERA_ERROR(msg) VOIDLIGHT_ERROR("Camera", msg)
#define CAMERA_WARN(msg) VOIDLIGHT_WARN("Camera", msg)
#define CAMERA_INFO(msg) VOIDLIGHT_INFO("Camera", msg)
#define CAMERA_DEBUG(msg) VOIDLIGHT_DEBUG("Camera", msg)

#define GPU_SCENE_RECORDER_CRITICAL(msg) VOIDLIGHT_CRITICAL("GPUSceneRecorder", msg)
#define GPU_SCENE_RECORDER_ERROR(msg) VOIDLIGHT_ERROR("GPUSceneRecorder", msg)
#define GPU_SCENE_RECORDER_WARN(msg) VOIDLIGHT_WARN("GPUSceneRecorder", msg)
#define GPU_SCENE_RECORDER_INFO(msg) VOIDLIGHT_INFO("GPUSceneRecorder", msg)
#define GPU_SCENE_RECORDER_DEBUG(msg) VOIDLIGHT_DEBUG("GPUSceneRecorder", msg)

#define SAVEGAME_CRITICAL(msg) VOIDLIGHT_CRITICAL("SaveGameManager", msg)
#define SAVEGAME_ERROR(msg) VOIDLIGHT_ERROR("SaveGameManager", msg)
#define SAVEGAME_WARN(msg) VOIDLIGHT_WARN("SaveGameManager", msg)
#define SAVEGAME_INFO(msg) VOIDLIGHT_INFO("SaveGameManager", msg)
#define SAVEGAME_DEBUG(msg) VOIDLIGHT_DEBUG("SaveGameManager", msg)

#define RESOURCE_CRITICAL(msg) VOIDLIGHT_CRITICAL("ResourceTemplateManager", msg)
#define RESOURCE_ERROR(msg) VOIDLIGHT_ERROR("ResourceTemplateManager", msg)
#define RESOURCE_WARN(msg) VOIDLIGHT_WARN("ResourceTemplateManager", msg)
#define RESOURCE_INFO(msg) VOIDLIGHT_INFO("ResourceTemplateManager", msg)
#define RESOURCE_DEBUG(msg) VOIDLIGHT_DEBUG("ResourceTemplateManager", msg)
#define RESOURCE_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "ResourceTemplateManager", msg)
#define RESOURCE_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "ResourceTemplateManager", msg)
#define RESOURCE_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "ResourceTemplateManager", msg)

#define INVENTORY_CRITICAL(msg) VOIDLIGHT_CRITICAL("InventoryComponent", msg)
#define INVENTORY_ERROR(msg) VOIDLIGHT_ERROR("InventoryComponent", msg)
#define INVENTORY_WARN(msg) VOIDLIGHT_WARN("InventoryComponent", msg)
#define INVENTORY_INFO(msg) VOIDLIGHT_INFO("InventoryComponent", msg)
#define INVENTORY_DEBUG(msg) VOIDLIGHT_DEBUG("InventoryComponent", msg)
#define INVENTORY_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "InventoryComponent", msg)
#define INVENTORY_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "InventoryComponent", msg)
#define INVENTORY_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "InventoryComponent", msg)

#define WORLD_RESOURCE_CRITICAL(msg)                                           \
  VOIDLIGHT_CRITICAL("WorldResourceManager", msg)
#define WORLD_RESOURCE_ERROR(msg) VOIDLIGHT_ERROR("WorldResourceManager", msg)
#define WORLD_RESOURCE_WARN(msg) VOIDLIGHT_WARN("WorldResourceManager", msg)
#define WORLD_RESOURCE_INFO(msg) VOIDLIGHT_INFO("WorldResourceManager", msg)
#define WORLD_RESOURCE_DEBUG(msg) VOIDLIGHT_DEBUG("WorldResourceManager", msg)
#define WORLD_RESOURCE_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "WorldResourceManager", msg)
#define WORLD_RESOURCE_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "WorldResourceManager", msg)
#define WORLD_RESOURCE_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "WorldResourceManager", msg)

#define WORLD_MANAGER_CRITICAL(msg) VOIDLIGHT_CRITICAL("WorldManager", msg)
#define WORLD_MANAGER_ERROR(msg) VOIDLIGHT_ERROR("WorldManager", msg)
#define WORLD_MANAGER_WARN(msg) VOIDLIGHT_WARN("WorldManager", msg)
#define WORLD_MANAGER_INFO(msg) VOIDLIGHT_INFO("WorldManager", msg)
#define WORLD_MANAGER_DEBUG(msg) VOIDLIGHT_DEBUG("WorldManager", msg)

// Entity and State Systems
#define GAMESTATE_CRITICAL(msg) VOIDLIGHT_CRITICAL("GameStateManager", msg)
#define GAMESTATE_ERROR(msg) VOIDLIGHT_ERROR("GameStateManager", msg)
#define GAMESTATE_WARN(msg) VOIDLIGHT_WARN("GameStateManager", msg)
#define GAMESTATE_INFO(msg) VOIDLIGHT_INFO("GameStateManager", msg)
#define GAMESTATE_DEBUG(msg) VOIDLIGHT_DEBUG("GameStateManager", msg)
#define GAMESTATE_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "GameStateManager", msg)
#define GAMESTATE_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "GameStateManager", msg)
#define GAMESTATE_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "GameStateManager", msg)

#define GAMEPLAY_CRITICAL(msg) VOIDLIGHT_CRITICAL("GamePlayState", msg)
#define GAMEPLAY_ERROR(msg) VOIDLIGHT_ERROR("GamePlayState", msg)
#define GAMEPLAY_WARN(msg) VOIDLIGHT_WARN("GamePlayState", msg)
#define GAMEPLAY_INFO(msg) VOIDLIGHT_INFO("GamePlayState", msg)
#define GAMEPLAY_DEBUG(msg) VOIDLIGHT_DEBUG("GamePlayState", msg)
#define GAMEPLAY_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "GamePlayState", msg)
#define GAMEPLAY_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "GamePlayState", msg)
#define GAMEPLAY_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "GamePlayState", msg)

#define ENTITYSTATE_CRITICAL(msg) VOIDLIGHT_CRITICAL("EntityStateManager", msg)
#define ENTITYSTATE_ERROR(msg) VOIDLIGHT_ERROR("EntityStateManager", msg)
#define ENTITYSTATE_WARN(msg) VOIDLIGHT_WARN("EntityStateManager", msg)
#define ENTITYSTATE_INFO(msg) VOIDLIGHT_INFO("EntityStateManager", msg)
#define ENTITYSTATE_DEBUG(msg) VOIDLIGHT_DEBUG("EntityStateManager", msg)

// Entity Systems
#define ENTITY_CRITICAL(msg) VOIDLIGHT_CRITICAL("Entity", msg)
#define ENTITY_ERROR(msg) VOIDLIGHT_ERROR("Entity", msg)
#define ENTITY_WARN(msg) VOIDLIGHT_WARN("Entity", msg)
#define ENTITY_INFO(msg) VOIDLIGHT_INFO("Entity", msg)
#define ENTITY_DEBUG(msg) VOIDLIGHT_DEBUG("Entity", msg)

#define PLAYER_CRITICAL(msg) VOIDLIGHT_CRITICAL("Player", msg)
#define PLAYER_ERROR(msg) VOIDLIGHT_ERROR("Player", msg)
#define PLAYER_WARN(msg) VOIDLIGHT_WARN("Player", msg)
#define PLAYER_INFO(msg) VOIDLIGHT_INFO("Player", msg)
#define PLAYER_DEBUG(msg) VOIDLIGHT_DEBUG("Player", msg)
#define PLAYER_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "Player", msg)
#define PLAYER_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "Player", msg)
#define PLAYER_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "Player", msg)

#define NPC_CRITICAL(msg) VOIDLIGHT_CRITICAL("NPC", msg)
#define NPC_ERROR(msg) VOIDLIGHT_ERROR("NPC", msg)
#define NPC_WARN(msg) VOIDLIGHT_WARN("NPC", msg)
#define NPC_INFO(msg) VOIDLIGHT_INFO("NPC", msg)
#define NPC_DEBUG(msg) VOIDLIGHT_DEBUG("NPC", msg)

// Collision and Pathfinding Systems
#define COLLISION_CRITICAL(msg) VOIDLIGHT_CRITICAL("CollisionManager", msg)
#define COLLISION_ERROR(msg) VOIDLIGHT_ERROR("CollisionManager", msg)
#define COLLISION_WARN(msg) VOIDLIGHT_WARN("CollisionManager", msg)
#define COLLISION_INFO(msg) VOIDLIGHT_INFO("CollisionManager", msg)
#define COLLISION_DEBUG(msg) VOIDLIGHT_DEBUG("CollisionManager", msg)

#define PATHFIND_CRITICAL(msg) VOIDLIGHT_CRITICAL("Pathfinding", msg)
#define PATHFIND_ERROR(msg) VOIDLIGHT_ERROR("Pathfinding", msg)
#define PATHFIND_WARN(msg) VOIDLIGHT_WARN("Pathfinding", msg)
#define PATHFIND_INFO(msg) VOIDLIGHT_INFO("Pathfinding", msg)
#define PATHFIND_DEBUG(msg) VOIDLIGHT_DEBUG("Pathfinding", msg)

#define SETTINGS_CRITICAL(msg) VOIDLIGHT_CRITICAL("SettingsManager", msg)
#define SETTINGS_ERROR(msg) VOIDLIGHT_ERROR("SettingsManager", msg)
#define SETTINGS_WARNING(msg) VOIDLIGHT_WARN("SettingsManager", msg)
#define SETTINGS_WARN(msg) SETTINGS_WARNING(msg)
#define SETTINGS_INFO(msg) VOIDLIGHT_INFO("SettingsManager", msg)
#define SETTINGS_DEBUG(msg) VOIDLIGHT_DEBUG("SettingsManager", msg)

// WeatherController logging
#define WEATHER_CRITICAL(msg) VOIDLIGHT_CRITICAL("WeatherController", msg)
#define WEATHER_ERROR(msg) VOIDLIGHT_ERROR("WeatherController", msg)
#define WEATHER_WARNING(msg) VOIDLIGHT_WARN("WeatherController", msg)
#define WEATHER_WARN(msg) WEATHER_WARNING(msg)
#define WEATHER_INFO(msg) VOIDLIGHT_INFO("WeatherController", msg)
#define WEATHER_DEBUG(msg) VOIDLIGHT_DEBUG("WeatherController", msg)

// DayNightController logging
#define DAYNIGHT_CRITICAL(msg) VOIDLIGHT_CRITICAL("DayNightController", msg)
#define DAYNIGHT_ERROR(msg) VOIDLIGHT_ERROR("DayNightController", msg)
#define DAYNIGHT_WARN(msg) VOIDLIGHT_WARN("DayNightController", msg)
#define DAYNIGHT_INFO(msg) VOIDLIGHT_INFO("DayNightController", msg)
#define DAYNIGHT_DEBUG(msg) VOIDLIGHT_DEBUG("DayNightController", msg)

// TimeController logging
#define TIME_CRITICAL(msg) VOIDLIGHT_CRITICAL("TimeController", msg)
#define TIME_ERROR(msg) VOIDLIGHT_ERROR("TimeController", msg)
#define TIME_WARN(msg) VOIDLIGHT_WARN("TimeController", msg)
#define TIME_INFO(msg) VOIDLIGHT_INFO("TimeController", msg)
#define TIME_DEBUG(msg) VOIDLIGHT_DEBUG("TimeController", msg)

// CombatController logging
#define COMBAT_CRITICAL(msg) VOIDLIGHT_CRITICAL("CombatController", msg)
#define COMBAT_ERROR(msg) VOIDLIGHT_ERROR("CombatController", msg)
#define COMBAT_WARN(msg) VOIDLIGHT_WARN("CombatController", msg)
#define COMBAT_INFO(msg) VOIDLIGHT_INFO("CombatController", msg)
#define COMBAT_DEBUG(msg) VOIDLIGHT_DEBUG("CombatController", msg)

// InventoryController logging
#define INVENTORY_CONTROLLER_CRITICAL(msg) VOIDLIGHT_CRITICAL("InventoryController", msg)
#define INVENTORY_CONTROLLER_ERROR(msg) VOIDLIGHT_ERROR("InventoryController", msg)
#define INVENTORY_CONTROLLER_WARN(msg) VOIDLIGHT_WARN("InventoryController", msg)
#define INVENTORY_CONTROLLER_INFO(msg) VOIDLIGHT_INFO("InventoryController", msg)
#define INVENTORY_CONTROLLER_DEBUG(msg) VOIDLIGHT_DEBUG("InventoryController", msg)

// SocialController logging
#define SOCIAL_CRITICAL(msg) VOIDLIGHT_CRITICAL("SocialController", msg)
#define SOCIAL_ERROR(msg) VOIDLIGHT_ERROR("SocialController", msg)
#define SOCIAL_WARN(msg) VOIDLIGHT_WARN("SocialController", msg)
#define SOCIAL_INFO(msg) VOIDLIGHT_INFO("SocialController", msg)
#define SOCIAL_DEBUG(msg) VOIDLIGHT_DEBUG("SocialController", msg)

// HarvestController logging
#define HARVEST_CRITICAL(msg) VOIDLIGHT_CRITICAL("HarvestController", msg)
#define HARVEST_ERROR(msg) VOIDLIGHT_ERROR("HarvestController", msg)
#define HARVEST_WARN(msg) VOIDLIGHT_WARN("HarvestController", msg)
#define HARVEST_INFO(msg) VOIDLIGHT_INFO("HarvestController", msg)
#define HARVEST_DEBUG(msg) VOIDLIGHT_DEBUG("HarvestController", msg)

#define RESOURCE_RENDER_CRITICAL(msg) VOIDLIGHT_CRITICAL("ResourceRenderController", msg)
#define RESOURCE_RENDER_ERROR(msg) VOIDLIGHT_ERROR("ResourceRenderController", msg)
#define RESOURCE_RENDER_WARN(msg) VOIDLIGHT_WARN("ResourceRenderController", msg)
#define RESOURCE_RENDER_INFO(msg) VOIDLIGHT_INFO("ResourceRenderController", msg)
#define RESOURCE_RENDER_DEBUG(msg) VOIDLIGHT_DEBUG("ResourceRenderController", msg)
#define RESOURCE_RENDER_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "ResourceRenderController", msg)

// Conditional logging macros for common systems
// Use these when an if-block contains ONLY logging (eliminates condition overhead in release)
#define GAMEENGINE_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "GameEngine", msg)
#define GAMEENGINE_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "GameEngine", msg)
#define GAMEENGINE_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "GameEngine", msg)

#define AI_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "AIManager", msg)
#define AI_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "AIManager", msg)
#define AI_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "AIManager", msg)

#define COLLISION_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "CollisionManager", msg)
#define COLLISION_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "CollisionManager", msg)
#define COLLISION_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "CollisionManager", msg)

#define PATHFIND_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "Pathfinding", msg)
#define PATHFIND_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "Pathfinding", msg)
#define PATHFIND_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "Pathfinding", msg)

#define EVENT_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "EventManager", msg)
#define EVENT_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "EventManager", msg)
#define EVENT_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "EventManager", msg)

#define WORLD_MANAGER_WARN_IF(cond, msg) VOIDLIGHT_WARN_IF(cond, "WorldManager", msg)
#define WORLD_MANAGER_INFO_IF(cond, msg) VOIDLIGHT_INFO_IF(cond, "WorldManager", msg)
#define WORLD_MANAGER_DEBUG_IF(cond, msg) VOIDLIGHT_DEBUG_IF(cond, "WorldManager", msg)

// Benchmark mode convenience macros
#define VOIDLIGHT_ENABLE_BENCHMARK_MODE()                                         \
  VoidLight::Logger::SetBenchmarkMode(true)
#define VOIDLIGHT_DISABLE_BENCHMARK_MODE()                                        \
  VoidLight::Logger::SetBenchmarkMode(false)

} // namespace VoidLight

#endif // LOGGER_HPP
