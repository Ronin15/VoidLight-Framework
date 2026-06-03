/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "core/GameEngine.hpp"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"
#include "core/Logger.hpp"
#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"

#include "gpu/GPUDevice.hpp"
#include "gpu/GPURenderer.hpp"
#include "utils/FrameProfiler.hpp"
#include "gameStates/AIDemoState.hpp"
#include "gameStates/AdvancedAIDemoState.hpp"
#include "gameStates/EventDemoState.hpp"
#include "gameStates/GamePlayState.hpp"
#include "gameStates/GameOverState.hpp"
#include "gameStates/LoadingState.hpp"
#include "gameStates/LogoState.hpp"
#include "gameStates/MainMenuState.hpp"
#include "gameStates/OverlayDemoState.hpp"
#include "gameStates/SettingsMenuState.hpp"
#include "gameStates/UIDemoState.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/FontManager.hpp"  // For FrameProfiler overlay
#include "managers/GameStateManager.hpp"
#include "managers/GameTimeManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/ParticleManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/ProjectileManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/SaveGameManager.hpp"
#include "managers/SettingsManager.hpp"
#include "managers/SoundManager.hpp"
#include "managers/TextureManager.hpp"
#include "managers/UIManager.hpp"
#include "managers/WorldManager.hpp"
#include "managers/WorldResourceManager.hpp"
#include "utils/ResourcePath.hpp"
#include <cstdlib>
#include <format>
#include <future>
#include <string>
#include <string_view>
#include <vector>

#define VOIDLIGHT_GRAY 31, 32, 34, 255

float GameEngine::calculateFontDPIScale(
#ifdef __APPLE__
    int logicalWidth, int logicalHeight, int pixelWidth, int pixelHeight
#else
    int, int, int, int
#endif
) const {
#ifdef __APPLE__
  SDL_Window *window = mp_window.get();
  if (window) {
    const float pixelDensity = SDL_GetWindowPixelDensity(window);
    if (pixelDensity > 0.0f) {
      return pixelDensity;
    }
  }

  if (logicalWidth > 0 && logicalHeight > 0 && pixelWidth > 0 &&
      pixelHeight > 0) {
    const float scaleX = static_cast<float>(pixelWidth) /
                         static_cast<float>(logicalWidth);
    const float scaleY = static_cast<float>(pixelHeight) /
                         static_cast<float>(logicalHeight);
    if (scaleX > 0.0f && scaleY > 0.0f) {
      return (scaleX > scaleY) ? scaleX : scaleY;
    }
  }

  if (window) {
    const float displayScale = SDL_GetWindowDisplayScale(window);
    if (displayScale > 0.0f) {
      return displayScale;
    }
  }

  return 1.0f;
#else
    return 1.0f;
#endif
}

bool GameEngine::init(std::string_view title) {
  GAMEENGINE_INFO("Initializing SDL Video and Gamepad");

  // Initialize video and gamepad together to ensure proper IOKit setup on macOS
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    GAMEENGINE_CRITICAL(
        std::format("SDL initialization failed: {}", SDL_GetError()));
    return false;
  }

  GAMEENGINE_INFO("SDL Video online");

  // Initialize resource path resolver (detects bundle vs direct execution)
  // Must be after SDL_Init for SDL_GetBasePath() to work
  VoidLight::ResourcePath::init();

  // Load settings from disk - window dimensions and fullscreen state
  constexpr int DEFAULT_WIDTH = 1280;
  constexpr int DEFAULT_HEIGHT = 720;
  const std::string settingsPath =
      VoidLight::ResourcePath::resolve("res/settings.json");
  auto &settingsManager = VoidLight::SettingsManager::Instance();
  if (!settingsManager.loadFromFile(settingsPath)) {
    GAMEENGINE_WARN("Failed to load settings.json - using defaults");
  } else {
    GAMEENGINE_INFO(std::format("Settings loaded from {}", settingsPath));
  }

  // Get window configuration from settings
  const int width =
      settingsManager.get<int>("graphics", "resolution_width", DEFAULT_WIDTH);
  const int height =
      settingsManager.get<int>("graphics", "resolution_height", DEFAULT_HEIGHT);
  bool fullscreen = settingsManager.get<bool>("graphics", "fullscreen", false);

  // Set SDL hints for better rendering quality
  SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD,
              "3"); // Use geometry for smoother lines
  SDL_SetHint("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR",
              "0");                           // Don't bypass compositor
  SDL_SetHint("SDL_MOUSE_AUTO_CAPTURE", "0"); // Prevent mouse capture issues

  // Performance hints for rendering
  SDL_SetHint("SDL_RENDER_SCALE_QUALITY",
              "0"); // Use nearest pixel sampling for crisp tiles
  SDL_SetHint("SDL_RENDER_BATCHING",
              "1"); // Enable render batching for performance

// Prefer Vulkan backend for lower per-draw-call overhead on Linux/AMD
// SDL3 auto-falls back to OpenGL if Vulkan unavailable
#ifdef __linux__
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
#endif

  // VSync and buffering hints
  // VSync hint - preference before renderer creation (runtime API
  // SDL_SetRenderVSync() also used)
  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
  // Double buffer hint - only supported on Raspberry Pi and Wayland (no-op on
  // Windows/macOS)
  SDL_SetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1");
  // Framebuffer acceleration - cross-platform
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");

// macOS-specific hints for fullscreen and DPI handling
#ifdef __APPLE__
  // Use Spaces fullscreen (default "1") to preserve ProMotion adaptive refresh rate
  // Game Mode is triggered by Info.plist LSApplicationCategoryType + LSSupportsGameMode,
  // NOT by exclusive vs Spaces fullscreen. Exclusive fullscreen ("0") forces a display
  // mode change that can lock refresh rate to 60Hz on ProMotion displays.
  // See: https://github.com/libsdl-org/SDL/issues/8452
  SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "1");
  // Use Metal for best performance and ProMotion support on macOS
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
#endif

  GAMEENGINE_DEBUG("SDL rendering hints configured for optimal quality");

  // Use reliable window sizing approach instead of potentially corrupted
  // display bounds
  if (width <= 0 || height <= 0) {
    // Default to reasonable size if not specified
    m_windowWidth = 1280;
    m_windowHeight = 720;
    GAMEENGINE_INFO(std::format("Using default window size: {}x{}",
                                m_windowWidth, m_windowHeight));
  } else {
    // Use the provided dimensions
    m_windowWidth = width;
    m_windowHeight = height;
    GAMEENGINE_INFO(std::format("Using requested window size: {}x{}",
                                m_windowWidth, m_windowHeight));
  }

  // Save windowed dimensions before fullscreen might override them
  m_windowedWidth = m_windowWidth;
  m_windowedHeight = m_windowHeight;

  // Query display capabilities for intelligent window sizing (all platforms)
  int displayCount = 0;
  std::unique_ptr<SDL_DisplayID[], decltype(&SDL_free)> displays(
      SDL_GetDisplays(&displayCount), SDL_free);

  const SDL_DisplayMode *displayMode = nullptr;
  if (displays && displayCount > 0) {
    displayMode = SDL_GetDesktopDisplayMode(displays[0]);
    if (displayMode) {
      GAMEENGINE_INFO(std::format("Primary display: {}x{} @ {}Hz",
                                  displayMode->w, displayMode->h,
                                  displayMode->refresh_rate));

      // If requested window size is larger than 90% of display, use fullscreen
      // This prevents awkward oversized windows on smaller displays (handhelds,
      // laptops)
      const float displayUsageThreshold = 0.9f;
      if (!fullscreen &&
          (m_windowWidth >
               static_cast<int>(displayMode->w * displayUsageThreshold) ||
           m_windowHeight >
               static_cast<int>(displayMode->h * displayUsageThreshold))) {
        GAMEENGINE_INFO(
            std::format("Requested window size ({}x{}) is larger than {}% of "
                        "display - enabling fullscreen for better UX",
                        m_windowWidth, m_windowHeight,
                        static_cast<int>(displayUsageThreshold * 100)));
        fullscreen = true;
      }
    }
  } else {
    GAMEENGINE_WARN("Could not query display capabilities - proceeding with "
                    "requested dimensions");
  }
  // Window handling with platform-specific optimizations
  SDL_WindowFlags flags = 0;
#ifdef __APPLE__
  // Always use high pixel density on macOS for crisp Retina rendering in both
  // windowed and fullscreen
  flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif
  if (fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
#ifdef __APPLE__
    // On macOS, keep logical dimensions for proper scaling
    // Don't override m_windowWidth and m_windowHeight for macOS
    GAMEENGINE_INFO("Window set to Fullscreen mode for macOS compatibility");
#else
    // On non-macOS platforms, use native display resolution for fullscreen
    if (displayMode) {
      m_windowWidth = displayMode->w;
      m_windowHeight = displayMode->h;
      GAMEENGINE_INFO(std::format("Window size set to Full Screen: {}x{}",
                                  m_windowWidth, m_windowHeight));
    }
#endif
  }

  // Track initial fullscreen state
  m_isFullscreen = fullscreen;

  mp_window.reset(
      SDL_CreateWindow(title.data(), m_windowWidth, m_windowHeight, flags));

  if (!mp_window) {
    GAMEENGINE_ERROR(
        std::format("Failed to create window: {}", SDL_GetError()));
    return false;
  }

  SDL_SetWindowMinimumSize(mp_window.get(), 1280, 720);

  GAMEENGINE_DEBUG("Window creation system online");

  // macOS Game Mode is triggered by Info.plist (LSApplicationCategoryType=games + LSSupportsGameMode)
  // Spaces fullscreen (SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES=1) preserves ProMotion adaptive refresh

  GAMEENGINE_INFO("Initializing SDL3_GPU rendering backend");
  auto &gpuDevice = VoidLight::GPUDevice::Instance();
  if (!gpuDevice.init(mp_window.get())) {
    GAMEENGINE_ERROR("GPUDevice init failed");
    return false;
  }

  auto &gpuRenderer = VoidLight::GPURenderer::Instance();
  if (!gpuRenderer.init()) {
    GAMEENGINE_ERROR("GPURenderer init failed");
    gpuDevice.shutdown();
    return false;
  }

  GAMEENGINE_INFO("SDL3_GPU rendering initialized successfully");

  // Set window icon
  GAMEENGINE_INFO("Setting window icon");

  // Use native SDL3 PNG loading for the icon
  const std::string iconPath = VoidLight::ResourcePath::resolve("res/img/icon.png");

  // Use a separate thread to load the icon
  auto iconFuture =
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          [iconPath]()
              -> std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> {
            return std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>(
                SDL_LoadPNG(iconPath.c_str()), SDL_DestroySurface);
          });

  // Continue with initialization while icon loads
  // Cache window sizes once for subsequent initialization steps
  int pixelWidth = m_windowWidth;
  int pixelHeight = m_windowHeight;
  int logicalWidth = m_windowWidth;
  int logicalHeight = m_windowHeight;

  if (!SDL_GetWindowSizeInPixels(mp_window.get(), &pixelWidth, &pixelHeight)) {
    GAMEENGINE_ERROR(
        std::format("Failed to get window pixel size: {}", SDL_GetError()));
  }
  if (!SDL_GetWindowSize(mp_window.get(), &logicalWidth, &logicalHeight)) {
    GAMEENGINE_ERROR(
        std::format("Failed to get window logical size: {}", SDL_GetError()));
  }

  GAMEENGINE_DEBUG("GPU rendering system online");

  // Unified VSync initialization with automatic fallback
  // Detect platform for logging purposes only (not used to disable VSync)
  const std::string videoDriverRaw =
      SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "";
  std::string_view videoDriver = videoDriverRaw;
  m_isWayland = (videoDriver == "wayland");

  // Fallback to environment detection if driver info unavailable
  if (!m_isWayland) {
    const std::string sessionTypeRaw =
        std::getenv("XDG_SESSION_TYPE") ? std::getenv("XDG_SESSION_TYPE") : "";
    const std::string waylandDisplayRaw =
        std::getenv("WAYLAND_DISPLAY") ? std::getenv("WAYLAND_DISPLAY") : "";

    std::string_view sessionType = sessionTypeRaw;
    bool hasWaylandDisplay = !waylandDisplayRaw.empty();

    m_isWayland = (sessionType == "wayland") || hasWaylandDisplay;
  }

  VOIDLIGHT_DEBUG_ONLY(
  if (m_isWayland) {
    GAMEENGINE_INFO("Platform detected: Wayland");
  } else {
    GAMEENGINE_INFO(std::format(
        "Platform detected: {}",
        videoDriver.empty() ? "Unknown" : std::string(videoDriver)));
  }
  )

  // Load VSync preference from SettingsManager (defaults to enabled)
  auto &settings = VoidLight::SettingsManager::Instance();
  bool vsyncRequested = settings.get<bool>("graphics", "vsync", true);
  GAMEENGINE_INFO(std::format("VSync setting from SettingsManager: {}",
                              vsyncRequested ? "enabled" : "disabled"));

  // Create TimestepManager (uses default 60 FPS target and 1/60s fixed
  // timestep)
  m_timestepManager = std::make_unique<TimestepManager>();
  updateDisplayRefreshRate();

  auto& gpuDev = VoidLight::GPUDevice::Instance();
  const SDL_GPUPresentMode requestedMode = vsyncRequested
      ? SDL_GPU_PRESENTMODE_VSYNC
      : SDL_GPU_PRESENTMODE_MAILBOX;

  bool committedVSync = false;
  bool usingSoftwareFallback = false;

  if (SDL_SetGPUSwapchainParameters(
          gpuDev.get(), gpuDev.getWindow(),
          SDL_GPU_SWAPCHAINCOMPOSITION_SDR, requestedMode)) {
    committedVSync = vsyncRequested;
    GAMEENGINE_INFO(std::format("GPU rendering: present mode {}",
                                committedVSync ? "VSYNC" : "MAILBOX"));
  } else if (!vsyncRequested) {
    const std::string mailboxError = SDL_GetError();
    if (SDL_SetGPUSwapchainParameters(gpuDev.get(), gpuDev.getWindow(),
                                      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                      SDL_GPU_PRESENTMODE_VSYNC)) {
      committedVSync = true;
      GAMEENGINE_WARN(std::format(
          "Failed to set GPU present mode to MAILBOX: {}. Falling back to VSYNC.",
          mailboxError));
    } else {
      usingSoftwareFallback = true;
      GAMEENGINE_WARN(std::format(
          "Failed to configure GPU present mode: {}. Falling back to software frame limiting.",
          SDL_GetError()));
    }
  } else {
    usingSoftwareFallback = true;
    GAMEENGINE_WARN(std::format(
        "Failed to configure GPU present mode: {}. Falling back to software frame limiting.",
        SDL_GetError()));
  }

  m_vsyncRequested = committedVSync;
  m_timestepManager->setSoftwareFrameLimiting(usingSoftwareFallback || !committedVSync);

  if (m_timestepManager->isUsingSoftwareFrameLimiting()) {
    TIMESTEP_INFO(std::format("Created: {:.0f} Hz updates, {:.0f} FPS target, "
                              "software frame limiting",
                              m_timestepManager->getUpdateFrequencyHz(),
                              m_timestepManager->getTargetFPS()));
  } else {
    TIMESTEP_INFO(std::format("Created: {:.0f} Hz updates, VSync enabled",
                              m_timestepManager->getUpdateFrequencyHz()));
  }

  // Store actual dimensions for UI positioning
  int const actualWidth = pixelWidth;
  int const actualHeight = pixelHeight;
  m_widthInPixels = actualWidth;
  m_heightInPixels = actualHeight;

  GAMEENGINE_INFO(
      std::format("GPU rendering at native resolution: {}x{}",
                  actualWidth, actualHeight));

  // Now check if the icon was loaded successfully
  try {
    auto iconSurfacePtr = iconFuture.get();
    if (iconSurfacePtr) {
      SDL_SetWindowIcon(mp_window.get(), iconSurfacePtr.get());
      // No need to manually destroy the surface, smart pointer will handle it
      GAMEENGINE_INFO("Window icon set successfully");
    } else {
      GAMEENGINE_WARN("Failed to load window icon");
    }
  } catch (const std::exception &e) {
    GAMEENGINE_WARN(std::format("Error loading window icon: {}", e.what()));
  }

  // INITIALIZING GAME RESOURCE LOADING AND
  // MANAGEMENT_________________________BEGIN

  // Calculate DPI-aware font sizes before threading.
  const float dpiScale =
      calculateFontDPIScale(logicalWidth, logicalHeight, pixelWidth,
                            pixelHeight);
#ifdef __APPLE__
  GAMEENGINE_INFO(std::format(
      "macOS font DPI scale: {:.2f} (logical: {}x{}, pixels: {}x{})",
      dpiScale, logicalWidth, logicalHeight, pixelWidth, pixelHeight));
#else
  GAMEENGINE_INFO("Non-macOS: Using DPI scale 1.0 (SDL3 logical presentation "
                  "handles scaling)");
#endif

  // Store DPI scale for use by other managers
  m_dpiScale = dpiScale;

  GAMEENGINE_INFO("Using display-aware font sizing - SDL3 handles DPI scaling "
                  "automatically");

  GAMEENGINE_INFO(std::format("DPI scale: {}, window: {}x{}", dpiScale,
                              m_windowWidth, m_windowHeight));

  // Use multiple threads for initialization
  std::vector<std::future<bool>> initTasks; // Initialization tasks vector
  initTasks.reserve(12); // Reserve capacity for typical number of init tasks

  // CRITICAL: Initialize Event Manager FIRST - #1
  // All other managers that register event handlers depend on this
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO("Creating Event Manager");
            EventManager &eventMgr = EventManager::Instance();
            if (!eventMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Event Manager");
              return false;
            }
            GAMEENGINE_INFO("Event Manager initialized successfully");
            return true;
          }));

  // Initialize EntityDataManager - #1.5
  // Central data authority for all entities (Phase 1 of Entity System Overhaul)
  // Must be initialized before any entities are created
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO("Creating Entity Data Manager");
            EntityDataManager &edm = EntityDataManager::Instance();
            if (!edm.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Entity Data Manager");
              return false;
            }
            GAMEENGINE_INFO("Entity Data Manager initialized successfully");
            return true;
          }));

  // Initialize input manager in a background thread - #2
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO(
                "Detecting and initializing gamepads and input handling");
            InputManager &inputMgr = InputManager::Instance();
            if (!inputMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Input Manager");
              return false;
            }
            inputMgr.initializeGamePad();
            // Load user-customized bindings; fall back to defaults on failure
            if (!inputMgr.loadBindingsFromFile(
                    VoidLight::ResourcePath::resolve("res/input_bindings.json"))) {
              GAMEENGINE_WARN("Input bindings file not found or invalid — using defaults");
            }
            return true;
          }));

  // Create and initialize texture manager - MAIN THREAD
  GAMEENGINE_INFO("Creating Texture Manager");
  TextureManager &texMgr = TextureManager::Instance();

  // Load textures in main thread
  GAMEENGINE_INFO("Creating and loading textures");
  const std::string textureResPath = VoidLight::ResourcePath::resolve("res/img");
  constexpr std::string_view texturePrefix = "";

  texMgr.loadGPU(textureResPath, std::string(texturePrefix));

  // Initialize sound manager in a separate thread - #3
  // Resolve paths before lambda capture
  const std::string sfxPath = VoidLight::ResourcePath::resolve("res/sfx");
  const std::string musicPath = VoidLight::ResourcePath::resolve("res/music");
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          [sfxPath, musicPath]() -> bool {
            GAMEENGINE_INFO("Creating Sound Manager");
            SoundManager &soundMgr = SoundManager::Instance();
            if (!soundMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Sound Manager");
              return false;
            }

            GAMEENGINE_INFO("Loading sounds and music");
            constexpr std::string_view sfxPrefix = "sfx";
            constexpr std::string_view musicPrefix = "music";
            soundMgr.loadSFX(sfxPath, std::string(sfxPrefix));
            soundMgr.loadMusic(musicPath, std::string(musicPrefix));
            return true;
          }));

  // Initialize font manager in a separate thread - #4
  const std::string fontsPath = VoidLight::ResourcePath::resolve("res/fonts");
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          [this, fontsPath]() -> bool {
            GAMEENGINE_INFO("Creating Font Manager");
            FontManager &fontMgr = FontManager::Instance();
            if (!fontMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Font Manager");
              return false;
            }

            GAMEENGINE_INFO("Loading fonts with display-aware sizing");

            // Use logical dimensions with DPI scale for proper sizing on
            // high-DPI displays
            if (!fontMgr.loadFontsForDisplay(fontsPath,
                                             m_windowWidth, m_windowHeight,
                                             m_dpiScale)) {
              GAMEENGINE_CRITICAL("Failed to load fonts for display");
              return false;
            }
            return true;
          }));

  // Initialize save game manager in a separate thread - #5
  // Use SDL_GetPrefPath for a writable save location (works with bundles)
  const char* prefPath = SDL_GetPrefPath("HammerForgedGames", VOIDLIGHT_APP_NAME);
  std::string saveDir;
  if (prefPath) {
    saveDir = prefPath;
    GAMEENGINE_INFO(std::format("Using SDL pref path for saves: {}", saveDir));
  } else {
    saveDir = VoidLight::ResourcePath::resolve("res");
    GAMEENGINE_WARN(std::format("SDL_GetPrefPath failed, using: {}", saveDir));
  }
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          [saveDir]() -> bool {
            GAMEENGINE_INFO("Creating Save Game Manager");
            SaveGameManager &saveMgr = SaveGameManager::Instance();

            // Set save directory BEFORE init() so it creates the right path
            saveMgr.setSaveDirectory(saveDir);

            if (!saveMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Save Game Manager");
              return false;
            }
            return true;
          }));

  // Initialize Pathfinder Manager - #6
  // CRITICAL: Must complete BEFORE AIManager (explicit dependency)
  GAMEENGINE_INFO("Creating Pathfinder Manager (AIManager dependency)");
  auto pathfinderFuture =
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO("Initializing PathfinderManager");
            PathfinderManager &pathfinderMgr = PathfinderManager::Instance();
            if (!pathfinderMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Pathfinder Manager");
              return false;
            }
            GAMEENGINE_INFO("Pathfinder Manager initialized successfully");
            return true;
          });

  // Initialize Collision Manager - #7
  // CRITICAL: Must complete BEFORE AIManager (explicit dependency)
  GAMEENGINE_INFO("Creating Collision Manager (AIManager dependency)");
  auto collisionFuture =
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO("Initializing CollisionManager");
            CollisionManager &collisionMgr = CollisionManager::Instance();
            if (!collisionMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize Collision Manager");
              return false;
            }
            GAMEENGINE_INFO("Collision Manager initialized successfully");
            return true;
          });

  // Wait for AIManager dependencies to complete before proceeding
  // This enforces the initialization dependency graph explicitly
  GAMEENGINE_INFO("Waiting for AIManager dependencies (PathfinderManager, "
                  "CollisionManager)");
  try {
    if (!pathfinderFuture.get()) {
      GAMEENGINE_CRITICAL("PathfinderManager initialization failed");
      return false;
    }
    if (!collisionFuture.get()) {
      GAMEENGINE_CRITICAL("CollisionManager initialization failed");
      return false;
    }
  } catch (const std::exception &e) {
    GAMEENGINE_CRITICAL(std::format(
        "AIManager dependency initialization threw exception: {}", e.what()));
    return false;
  }
  GAMEENGINE_INFO("AIManager dependencies initialized successfully");

  // Initialize AI Manager - #8
  // Dependencies satisfied: PathfinderManager and CollisionManager are now
  // fully initialized
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO("Creating AI Manager");
            AIManager &aiMgr = AIManager::Instance();
            if (!aiMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize AI Manager");
              return false;
            }
            GAMEENGINE_INFO("AI Manager initialized successfully");
            return true;
          }));

  // Initialize Particle Manager in a separate thread - #9
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult([]()
                                                                       -> bool {
        GAMEENGINE_INFO("Creating Particle Manager");
        ParticleManager &particleMgr = ParticleManager::Instance();
        if (!particleMgr.init()) {
          GAMEENGINE_CRITICAL("Failed to initialize Particle Manager");
          return false;
        }

        GAMEENGINE_INFO("Particle Manager initialized successfully");
        return true;
      }));

  // Initialize Resource Template Manager in a separate thread - #10
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult([]()
                                                                       -> bool {
        GAMEENGINE_INFO("Creating Resource Template Manager");
        ResourceTemplateManager &resourceMgr =
            ResourceTemplateManager::Instance();
        if (!resourceMgr.init()) {
          GAMEENGINE_CRITICAL("Failed to initialize Resource Template Manager");
          return false;
        }
        GAMEENGINE_INFO("Resource Template Manager initialized successfully");
        return true;
      }));

  // Initialize World Resource Manager for global resource tracking - #11
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO("Creating World Resource Manager");
            WorldResourceManager &worldResourceMgr =
                WorldResourceManager::Instance();
            if (!worldResourceMgr.init()) {
              GAMEENGINE_CRITICAL(
                  "Failed to initialize World Resource Manager");
              return false;
            }
            GAMEENGINE_INFO("World Resource Manager initialized successfully");
            return true;
          }));

  // Initialize World Manager for world generation and management - #12
  initTasks.push_back(
      VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
          []() -> bool {
            GAMEENGINE_INFO("Creating World Manager");
            WorldManager &worldMgr = WorldManager::Instance();
            if (!worldMgr.init()) {
              GAMEENGINE_CRITICAL("Failed to initialize World Manager");
              return false;
            }
            GAMEENGINE_INFO("World Manager initialized successfully");
            return true;
          }));

  // Initialize game state manager (on main thread because it directly calls
  // rendering) - MAIN THREAD
  GAMEENGINE_INFO(
      "Creating Game State Manager and setting up initial Game States");
  mp_gameStateManager = std::make_unique<GameStateManager>();

  // Initialize UI Manager (on main thread because it uses font/text rendering)
  // - MAIN THREAD
  GAMEENGINE_INFO("Creating UI Manager");
  UIManager &uiMgr = UIManager::Instance();
  if (!uiMgr.init()) {
    GAMEENGINE_CRITICAL("Failed to initialize UI Manager");
    return false;
  }

  GAMEENGINE_DEBUG("UI Manager initialized successfully");

  // Setting Up initial game states
  mp_gameStateManager->addState(std::make_unique<LogoState>());
  mp_gameStateManager->addState(
      std::make_unique<LoadingState>()); // Shared loading screen state
  mp_gameStateManager->addState(std::make_unique<MainMenuState>());
  mp_gameStateManager->addState(std::make_unique<SettingsMenuState>());
  mp_gameStateManager->addState(std::make_unique<GamePlayState>());
  mp_gameStateManager->addState(std::make_unique<GameOverState>());
  mp_gameStateManager->addState(std::make_unique<AIDemoState>());
  mp_gameStateManager->addState(std::make_unique<AdvancedAIDemoState>());
  mp_gameStateManager->addState(std::make_unique<EventDemoState>());
  mp_gameStateManager->addState(std::make_unique<UIDemoState>());
  mp_gameStateManager->addState(std::make_unique<OverlayDemoState>());

  // Wait for all initialization tasks to complete
  bool allTasksSucceeded = true;
  for (auto &task : initTasks) {
    try {
      allTasksSucceeded &= task.get();
    } catch (const std::exception &e) {
      GAMEENGINE_ERROR(std::format("Initialization task failed: {}", e.what()));
      allTasksSucceeded = false;
    }
  }

  if (!allTasksSucceeded) {
    GAMEENGINE_ERROR("One or more initialization tasks failed");
    return false;
  }

  // Initialize GameTimeManager (fast, no threading needed)
  // Time scale: 60.0 = 1 real second equals 1 game minute
  GAMEENGINE_INFO("Initializing GameTimeManager system");
  if (!GameTimeManager::Instance().init(12.0f, 60.0f)) {
    GAMEENGINE_ERROR("Failed to initialize GameTimeManager");
    return false;
  }
  GAMEENGINE_INFO("GameTimeManager initialized (starting at noon, 60x speed)");

  // Initialize BackgroundSimulationManager (depends on EntityDataManager)
  // Handles simplified simulation for off-screen entities (Phase 5 of Entity
  // System Overhaul)
  GAMEENGINE_INFO("Initializing Background Simulation Manager");
  if (!BackgroundSimulationManager::Instance().init()) {
    GAMEENGINE_CRITICAL("Failed to initialize Background Simulation Manager");
    return false;
  }

  // Initialize Projectile Manager (after CollisionManager and EventManager)
  GAMEENGINE_INFO("Initializing Projectile Manager");
  if (!ProjectileManager::Instance().init()) {
    GAMEENGINE_CRITICAL("Failed to initialize Projectile Manager");
    return false;
  }
  // Configure tier radii based on logical screen size (dynamic for different
  // devices)
  BackgroundSimulationManager::Instance().configureForScreenSize(
      m_widthInPixels, m_heightInPixels);
  GAMEENGINE_INFO(std::format(
      "Background Simulation Manager initialized (activeRadius: {:.0f}, "
      "backgroundRadius: {:.0f})",
      BackgroundSimulationManager::Instance().getActiveRadius(),
      BackgroundSimulationManager::Instance().getBackgroundRadius()));

  // Step 2: Cache manager references for performance (after all background init
  // complete)
  GAMEENGINE_INFO("Caching and validating manager references");
  try {
    // Validate AI Manager before caching
    AIManager &aiMgrTest = AIManager::Instance();
    if (!aiMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL("AIManager not properly initialized before caching!");
      return false;
    }
    mp_aiManager = &aiMgrTest;

    // Validate Event Manager before caching
    EventManager &eventMgrTest = EventManager::Instance();
    if (!eventMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL(
          "EventManager not properly initialized before caching!");
      return false;
    }
    mp_eventManager = &eventMgrTest;

    // Validate Particle Manager before caching
    ParticleManager &particleMgrTest = ParticleManager::Instance();
    if (!particleMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL(
          "ParticleManager not properly initialized before caching!");
      return false;
    }
    mp_particleManager = &particleMgrTest;

    // Validate Pathfinder Manager before caching (initialized by AIManager)
    PathfinderManager &pathfinderMgrTest = PathfinderManager::Instance();
    if (!pathfinderMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL(
          "PathfinderManager not properly initialized before caching!");
      return false;
    }
    mp_pathfinderManager = &pathfinderMgrTest;

    // Validate Collision Manager before caching (initialized by AIManager)
    CollisionManager &collisionMgrTest = CollisionManager::Instance();
    if (!collisionMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL(
          "CollisionManager not properly initialized before caching!");
      return false;
    }
    mp_collisionManager = &collisionMgrTest;

    // Validate Background Simulation Manager before caching
    BackgroundSimulationManager &bgSimMgrTest =
        BackgroundSimulationManager::Instance();
    if (!bgSimMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL("BackgroundSimulationManager not properly "
                          "initialized before caching!");
      return false;
    }
    mp_backgroundSimManager = &bgSimMgrTest;

    // Validate Projectile Manager before caching
    ProjectileManager &projectileMgrTest = ProjectileManager::Instance();
    if (!projectileMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL("ProjectileManager not properly initialized before caching!");
      return false;
    }
    mp_projectileManager = &projectileMgrTest;

    // Validate Resource Manager before caching
    ResourceTemplateManager &resourceMgrTest =
        ResourceTemplateManager::Instance();
    if (!resourceMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL(
          "ResourceTemplateManager not properly initialized before caching!");
      return false;
    }
    mp_resourceTemplateManager = &resourceMgrTest;

    // Validate World Resource Manager before caching
    WorldResourceManager &worldResourceMgrTest =
        WorldResourceManager::Instance();
    if (!worldResourceMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL(
          "WorldResourceManager not properly initialized before caching!");
      return false;
    }
    mp_worldResourceManager = &worldResourceMgrTest;

    // Validate World Manager before caching
    WorldManager &worldMgrTest = WorldManager::Instance();
    if (!worldMgrTest.isInitialized()) {
      GAMEENGINE_CRITICAL(
          "WorldManager not properly initialized before caching!");
      return false;
    }
    mp_worldManager = &worldMgrTest;

    // InputManager not cached - handled in handleEvents() for proper SDL
    // architecture

    // Manager references are valid (assigned above)

    // Manager references cached - managers handle their own logging internally
    GAMEENGINE_INFO("Manager references cached successfully");
  } catch (const std::exception &e) {
    GAMEENGINE_ERROR(
        std::format("Error caching manager references: {}", e.what()));
    return false;
  }
  //_______________________________________________________________________________________________________________END

  GAMEENGINE_INFO(std::format("Game {} initialized successfully!", title));
  GAMEENGINE_INFO(std::format("Running {}", title));

  m_running = true;
  return true;
}

void GameEngine::handleEvents() {
  // SDL event polling - GameEngine owns the event loop as it owns the
  // window/renderer InputManager receives input events and maintains input
  // state
  InputManager &inputMgr = InputManager::Instance();

  // Clear previous frame's pressed keys before processing new events
  inputMgr.clearFrameInput();

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      GAMEENGINE_INFO("Shutting down!");
      setRunning(false);
      break;

    // Input events -> route to InputManager
    case SDL_EVENT_KEY_DOWN:
      inputMgr.onKeyDown(event);
      break;
    case SDL_EVENT_KEY_UP:
      inputMgr.onKeyUp(event);
      break;
    case SDL_EVENT_MOUSE_MOTION:
      inputMgr.onMouseMove(event);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      inputMgr.onMouseButtonDown(event);
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      inputMgr.onMouseButtonUp(event);
      break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      inputMgr.onGamepadAxisMove(event);
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      inputMgr.onGamepadButtonDown(event);
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      inputMgr.onGamepadButtonUp(event);
      break;
    case SDL_EVENT_GAMEPAD_ADDED:
      inputMgr.onGamepadAdded(event);
      break;
    case SDL_EVENT_GAMEPAD_REMOVED:
      inputMgr.onGamepadRemoved(event);
      break;
    case SDL_EVENT_GAMEPAD_REMAPPED:
      inputMgr.onGamepadRemapped(event);
      break;

    // Window/Display events -> handle directly in GameEngine
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      onWindowResize(event);
      break;
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_OCCLUDED:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      onWindowEvent(event);
      break;
    case SDL_EVENT_DISPLAY_ORIENTATION:
    case SDL_EVENT_DISPLAY_ADDED:
    case SDL_EVENT_DISPLAY_REMOVED:
    case SDL_EVENT_DISPLAY_MOVED:
    case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
      onDisplayChange(event);
      break;

    default:
      break;
    }
  }

  // Resolve semantic command state from raw SDL state now that all events
  // for this frame are in. Must run before game states read commands.
  inputMgr.refreshCommandState();

  // Global fullscreen toggle (F1 key) - processed before state input
  if (inputMgr.wasKeyPressed(SDL_SCANCODE_F1)) {
    toggleFullscreen();
  }

  // Debug profiler overlay toggle (F3 key) - Debug builds only
  if (inputMgr.wasKeyPressed(SDL_SCANCODE_F3)) {
    VoidLight::FrameProfiler::Instance().toggleOverlay();
  }

  // Handle game state input on main thread where SDL events are processed
  mp_gameStateManager->handleInput();
}

void GameEngine::setRunning(bool running) { m_running = running; }

bool GameEngine::getRunning() const { return m_running; }

float GameEngine::getCurrentFPS() const {
  if (m_timestepManager) {
    return m_timestepManager->getCurrentFPS();
  }
  return 0.0f;
}

void GameEngine::update(float deltaTime) {
  if (!m_running) {
    return;
  }

  // OPTIMAL MANAGER UPDATE ARCHITECTURE - CLEAN DESIGN
  // ===================================================
  // Update order optimized for correct NPC movement AND animation sync.
  // Key design: AIManager handles batch synchronization internally.
  //
  // UPDATE STRATEGY:
  // - Events FIRST (can trigger state changes)
  // - GameStates SECOND (player input/movement - AI needs current player position)
  // - AI THIRD (NPC behaviors react to current player, sets velocities + positions)
  // - Collision gets guaranteed-complete updates (no timing issues)
  //
  // CRITICAL ORDER RATIONALE:
  // - GameStateManager.update() handles player input/movement FIRST
  // - AIManager.update() then reacts to current player position (not stale)
  // - NPCRenderController reads velocity from PREVIOUS frame (1-frame lag OK for animation)
  // - GameStateManager GPU render hooks handle state scene/UI rendering
  //
  // GLOBAL SYSTEMS (Updated by GameEngine):
  // - EventManager: Global game events (weather, scene changes), batch processing
  // - AIManager: Parallel batch processing with internal sync (self-contained)
  // - GameStateManager: Player movement and state-specific logic (reads AI velocities)
  // - ParticleManager: Global particle system with weather integration
  // - PathfinderManager: Periodic pathfinding grid updates (every 300/600 frames)
  // - CollisionManager: Collision detection and resolution for all entities
  // - InputManager: Handled in handleEvents() for proper SDL event polling architecture
  //
  // STATE-MANAGED SYSTEMS (Updated by individual states):
  // - UIManager: Optional, state-specific, only updated when UI is actually used
  //   See UIDemoState::update() for proper state-managed pattern

  // Mark frame start for WorkerBudget per-frame caching
  VoidLight::WorkerBudgetManager::Instance().markFrameStart();

  // 1. Event system - FIRST: process global events, state changes, weather triggers
  { PROFILE_MANAGER(VoidLight::ManagerPhase::Event);
    mp_eventManager->update(); }
  if (!m_running) {
    return;
  }

  // 2. Game states - player movement and state logic
  //    MUST update BEFORE AIManager so NPCs react to current player position.
  //    Push FPS to GameStateManager so states don't need to call GameEngine::Instance()
  { PROFILE_MANAGER(VoidLight::ManagerPhase::GameState);
    mp_gameStateManager->setCurrentFPS(m_timestepManager->getCurrentFPS());
    mp_gameStateManager->update(deltaTime); }
  if (!m_running) {
    return;
  }

  // 3. AI system - processes NPC behaviors with internal parallelization
  //    Sets NPC velocities and applies position updates.
  //    Batches run in parallel, waits for completion internally before returning.
  { PROFILE_MANAGER(VoidLight::ManagerPhase::AI);
    mp_aiManager->update(deltaTime); }

  // 3.5 Projectile system - position integration + lifetime management
  //     Uses WorkerBudget threading with SIMD 4-wide movement.
  //     Collision damage handled via CollisionManager::setProjectileHitSink().
  { PROFILE_MANAGER(VoidLight::ManagerPhase::Projectile);
    mp_projectileManager->update(deltaTime); }

  // 4. Particle system - global weather and effect particles
  { PROFILE_MANAGER(VoidLight::ManagerPhase::Particle);
    mp_particleManager->update(deltaTime); }

  // 5. Pathfinding system - periodic grid updates (every 300/600 frames)
  // PathfinderManager initialized by AIManager, cached by GameEngine for
  // performance
  { PROFILE_MANAGER(VoidLight::ManagerPhase::Pathfinder);
    mp_pathfinderManager->update(); }

  // 6. Collision system - processes complete NPC updates from AIManager
  //    AIManager guarantees all batches complete before returning, so collision
  //    always receives complete, consistent updates (no partial/stale data).
  { PROFILE_MANAGER(VoidLight::ManagerPhase::Collision);
    mp_collisionManager->update(deltaTime); }

  // 7. Background simulation (tier updates + entity processing)
  // Single call handles everything: tier recalc every 60 frames,
  // background entity processing at 10Hz when entities exist.
  // Power-efficient: immediate return when paused or no work needed.
  { PROFILE_MANAGER(VoidLight::ManagerPhase::BackgroundSim);
    mp_backgroundSimManager->update(mp_aiManager->getPlayerPosition(), deltaTime); }
}

void GameEngine::render() {
  // Calculate interpolation alpha for smooth rendering between fixed updates
  float interpolationAlpha =
      static_cast<float>(m_timestepManager->getInterpolationAlpha());

  auto& gpuRenderer = VoidLight::GPURenderer::Instance();
  auto& profiler = VoidLight::FrameProfiler::Instance();

  if (!gpuRenderer.beginFrame()) {
    return;
  }

  profiler.renderOverlay();

  profiler.beginRender(VoidLight::RenderPhase::WorldTiles);
  mp_gameStateManager->recordGPUVertices(gpuRenderer, interpolationAlpha);
  profiler.endRender(VoidLight::RenderPhase::WorldTiles);

  SDL_GPURenderPass* scenePass = gpuRenderer.beginScenePass();

  profiler.beginRender(VoidLight::RenderPhase::Entities);
  if (scenePass) {
    mp_gameStateManager->renderGPUScene(gpuRenderer, scenePass, interpolationAlpha);
  }
  profiler.endRender(VoidLight::RenderPhase::Entities);

  profiler.beginRender(VoidLight::RenderPhase::EndScene);
  SDL_GPURenderPass* swapchainPass = gpuRenderer.beginSwapchainPass();
  if (swapchainPass) {
    gpuRenderer.renderComposite(swapchainPass);
  }
  profiler.endRender(VoidLight::RenderPhase::EndScene);

  profiler.beginRender(VoidLight::RenderPhase::UI);
  if (swapchainPass) {
    mp_gameStateManager->renderGPUUI(gpuRenderer, swapchainPass);
  }
  profiler.endRender(VoidLight::RenderPhase::UI);
}

void GameEngine::present() {
  // Present is separate from render for accurate profiling.
  // For the GPU path, frame pacing now happens when the swapchain pass acquires
  // the swapchain texture rather than at frame start.
  VoidLight::GPURenderer::Instance().endFrame();
}

void GameEngine::processBackgroundTasks() {
  // End-of-frame cleanup hook — runs once per frame after render/present,
  // using otherwise-idle CPU time while GPU finishes the frame.
  //
  // All managers and rendering are complete at this point, so structural
  // changes to entity storage (freeing slots, updating indices) are safe.

  // Drain deferred entity destructions — returns slots to m_freeSlots,
  // keeping the free list healthy for next frame's allocations.
  // This is the ONLY place processDestructionQueue runs during gameplay.
  // (prepareForStateTransition also calls it during state changes.)
  EntityDataManager::Instance().processDestructionQueue();
}

bool GameEngine::isVSyncEnabled() const noexcept {
  return m_vsyncRequested;
}

void GameEngine::clean() {
  GAMEENGINE_INFO("Starting shutdown sequence...");

  // Clean up engine managers (non-singletons)
  GAMEENGINE_INFO("Cleaning up GameState manager...");
  if (mp_gameStateManager) {
    mp_gameStateManager->clearAllStates();
    mp_gameStateManager.reset();
  }

  // Active state exit paths may need workers alive to drain pathfinding,
  // background simulation, or other queued jobs. Once states are gone, shut the
  // worker pool down before singleton manager cleanup so no late tasks can race
  // with SDL-backed resource destruction.
  GAMEENGINE_INFO("Cleaning up Thread System...");
  if (VoidLight::ThreadSystem::Exists()) {
    VoidLight::ThreadSystem &threadSystem =
        VoidLight::ThreadSystem::Instance();
    if (!threadSystem.isShutdown()) {
      threadSystem.clean();
    }
  }

  // Save copies of the smart pointers to resources we'll clean up at the very
  // end
  auto window_to_destroy = std::move(mp_window);

  // Clean up Managers in the correct order, respecting dependencies.
  // Systems that are used by other systems must be cleaned up last.
  GAMEENGINE_INFO("Cleaning up Particle Manager...");
  ParticleManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Font Manager...");
  FontManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Sound Manager...");
  SoundManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up UI Manager...");
  UIManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up AI Manager...");
  AIManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Projectile Manager...");
  ProjectileManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Background Simulation Manager...");
  BackgroundSimulationManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Event Manager...");
  EventManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Pathfinder Manager...");
  PathfinderManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Collision Manager...");
  CollisionManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Entity Data Manager...");
  EntityDataManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Save Game Manager...");
  SaveGameManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Input Manager...");
  InputManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up World Manager...");
  WorldManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up World Resource Manager...");
  WorldResourceManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Texture Manager...");
  TextureManager::Instance().clean();

  GAMEENGINE_INFO("Cleaning up Resource Template Manager...");
  ResourceTemplateManager::Instance().clean();

  // Clear manager cache references
  GAMEENGINE_INFO("Clearing manager caches...");
  mp_aiManager = nullptr;
  mp_eventManager = nullptr;
  mp_particleManager = nullptr;
  mp_pathfinderManager = nullptr;
  mp_collisionManager = nullptr;
  mp_projectileManager = nullptr;
  mp_resourceTemplateManager = nullptr;
  mp_worldResourceManager = nullptr;
  mp_worldManager = nullptr;

  // InputManager not cached
  GAMEENGINE_INFO("Manager caches cleared");

  // Finally clean up SDL resources
  GAMEENGINE_INFO("Cleaning up SDL resources...");

  // Explicitly reset smart pointers at the end, after all subsystems
  // are done using them - this will trigger their custom deleters
  GAMEENGINE_INFO("Shutting down GPU renderer...");
  VoidLight::GPURenderer::Instance().shutdown();
  GAMEENGINE_INFO("Shutting down GPU device...");
  VoidLight::GPUDevice::Instance().shutdown();

  GAMEENGINE_INFO("Destroying window...");
  window_to_destroy.reset();
  GAMEENGINE_INFO("Window destroyed successfully");

  GAMEENGINE_INFO("Calling SDL_Quit...");
  SDL_Quit();

  GAMEENGINE_INFO("SDL resources cleaned!");
  GAMEENGINE_INFO("Shutdown complete!");
}

bool GameEngine::setVSyncEnabled(bool enable) {
  GAMEENGINE_INFO(
      std::format("{} VSync...", enable ? "Enabling" : "Disabling"));

  auto& gpuDevice = VoidLight::GPUDevice::Instance();
  if (!gpuDevice.isInitialized()) {
    GAMEENGINE_ERROR("Cannot set VSync - GPU device not initialized");
    return false;
  }

  const SDL_GPUPresentMode requestedMode = enable
      ? SDL_GPU_PRESENTMODE_VSYNC
      : SDL_GPU_PRESENTMODE_MAILBOX;

  bool committedVSync = false;
  bool usingSoftwareFallback = false;

  if (SDL_SetGPUSwapchainParameters(
          gpuDevice.get(), gpuDevice.getWindow(),
          SDL_GPU_SWAPCHAINCOMPOSITION_SDR, requestedMode)) {
    committedVSync = enable;
  } else if (!enable) {
    const std::string mailboxError = SDL_GetError();
    if (SDL_SetGPUSwapchainParameters(gpuDevice.get(), gpuDevice.getWindow(),
                                      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                      SDL_GPU_PRESENTMODE_VSYNC)) {
      committedVSync = true;
      GAMEENGINE_WARN(std::format(
          "Failed to disable VSync via MAILBOX: {}. Falling back to VSYNC.",
          mailboxError));
    } else {
      GAMEENGINE_ERROR(std::format(
          "Failed to set GPU present mode: {}", SDL_GetError()));
      usingSoftwareFallback = true;
    }
  } else {
    GAMEENGINE_ERROR(std::format(
        "Failed to enable VSync on GPU swapchain: {}", SDL_GetError()));
    usingSoftwareFallback = true;
  }

  m_vsyncRequested = committedVSync;
  if (m_timestepManager) {
    m_timestepManager->setSoftwareFrameLimiting(usingSoftwareFallback || !committedVSync);
  }

  const bool success = !usingSoftwareFallback && (committedVSync == enable);

  if (usingSoftwareFallback) {
    GAMEENGINE_WARN("Falling back to software frame limiting");
  } else if (!success) {
    GAMEENGINE_WARN(std::format(
        "Requested {} but committed to {} — driver does not support MAILBOX",
        enable ? "VSYNC" : "MAILBOX",
        committedVSync ? "VSYNC" : "MAILBOX"));
  } else {
    GAMEENGINE_INFO(std::format("GPU frame pacing committed to {}",
                                committedVSync ? "VSYNC" : "MAILBOX"));
  }

  // Pure mode switch: update in-memory VSync and frame-pacing state only.
  // Persistence is the caller's responsibility (SettingsMenuState::applySettings
  // issues a single explicit saveToFile for the whole settings file).
  VoidLight::SettingsManager::Instance().set("graphics", "vsync", committedVSync);

  return success;
}

void GameEngine::toggleFullscreen() {
  if (!mp_window) {
    GAMEENGINE_ERROR("Cannot toggle fullscreen - window not initialized");
    return;
  }

  // Toggle fullscreen state
  m_isFullscreen = !m_isFullscreen;

  GAMEENGINE_INFO(std::format(
      "Toggling fullscreen mode: {} (windowed size: {}x{})",
      m_isFullscreen ? "ON" : "OFF", m_windowedWidth, m_windowedHeight));

  // Spaces fullscreen preserves ProMotion adaptive refresh
  // Game Mode is triggered by Info.plist settings, not fullscreen type
  if (!SDL_SetWindowFullscreen(mp_window.get(), m_isFullscreen)) {
    GAMEENGINE_ERROR(
        std::format("Failed to toggle fullscreen: {}", SDL_GetError()));
    m_isFullscreen = !m_isFullscreen; // Revert state on failure
    return;
  }

  // Restore windowed size when exiting fullscreen
  if (!m_isFullscreen) {
    if (!SDL_SetWindowSize(mp_window.get(), m_windowedWidth,
                           m_windowedHeight)) {
      GAMEENGINE_ERROR(
          std::format("Failed to restore window size: {}", SDL_GetError()));
    } else {
      GAMEENGINE_INFO(std::format("Restored window size to {}x{}",
                                  m_windowedWidth, m_windowedHeight));
    }
  }

  GAMEENGINE_INFO(std::format("Fullscreen mode {}",
                              m_isFullscreen ? "enabled" : "disabled"));

  // Note: SDL will automatically trigger SDL_EVENT_WINDOW_RESIZED
  // which will be handled by InputManager::onWindowResize()
  // This ensures fonts and UI are properly updated for the new display mode
}

void GameEngine::setFullscreen(bool enabled) {
  if (!mp_window) {
    GAMEENGINE_ERROR("Cannot set fullscreen - window not initialized");
    return;
  }

  // Only change if the state is different
  if (m_isFullscreen == enabled) {
    return;
  }

  // Use the existing toggle function since the state needs to change
  toggleFullscreen();
}

void GameEngine::updateDisplayRefreshRate() {
  if (!m_timestepManager || !mp_window) {
    return;
  }

  SDL_DisplayID displayID = SDL_GetDisplayForWindow(mp_window.get());
  if (displayID == 0) {
    m_timestepManager->setDisplayRefreshHz(0.0f);
    GAMEENGINE_WARN("Unable to determine active display for timestep refresh tracking");
    return;
  }

  const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(displayID);
  if (!displayMode || displayMode->refresh_rate <= 0.0f) {
    displayMode = SDL_GetDesktopDisplayMode(displayID);
  }

  if (!displayMode || displayMode->refresh_rate <= 0.0f) {
    m_timestepManager->setDisplayRefreshHz(0.0f);
    GAMEENGINE_WARN("Unable to query active display refresh rate for timestep tracking");
    return;
  }

  m_timestepManager->setDisplayRefreshHz(displayMode->refresh_rate);
  GAMEENGINE_INFO(std::format("TimestepManager display refresh set to {:.2f}Hz",
                              displayMode->refresh_rate));
}

void GameEngine::setGlobalPause(bool paused) {
  m_globallyPaused = paused;

  // Pause all managers (cached pointers guaranteed valid after init)
  mp_aiManager->setGlobalPause(paused);
  mp_particleManager->setGlobalPause(paused);
  mp_collisionManager->setGlobalPause(paused);
  mp_pathfinderManager->setGlobalPause(paused);
  mp_backgroundSimManager->setGlobalPause(paused);
  mp_projectileManager->setGlobalPause(paused);
  GameTimeManager::Instance().setGlobalPause(paused);
  EventManager::Instance().setGlobalPause(paused);

  VOIDLIGHT_DEBUG_ONLY(
  if (paused) {
    GAMEENGINE_INFO("Game globally paused - all managers idle");
  } else {
    GAMEENGINE_INFO("Game globally resumed");
  }
  )
}

bool GameEngine::isGloballyPaused() const { return m_globallyPaused; }

int GameEngine::getOptimalDisplayIndex() const {
#ifdef __APPLE__
  // On macOS, prioritize the primary display (0) for MacBook built-in screens
  return 0;
#else
  // On other platforms, try secondary display first if available
  int displayCount = 0;
  std::unique_ptr<SDL_DisplayID[], decltype(&SDL_free)> displays(
      SDL_GetDisplays(&displayCount), SDL_free);
  return (displayCount > 1) ? 1 : 0;
#endif
}

void GameEngine::onWindowResize(const SDL_Event &event) {
  const char *eventName = "Window metrics changed";
  switch (event.type) {
  case SDL_EVENT_WINDOW_RESIZED:
    eventName = "Window resized";
    break;
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    eventName = "Window pixel size changed";
    break;
  case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    eventName = "Window display scale changed";
    break;
  default:
    break;
  }

  refreshWindowMetrics(eventName);
}

void GameEngine::onWindowEvent(const SDL_Event &event) {
  InputManager &inputMgr = InputManager::Instance();

  switch (event.type) {
  case SDL_EVENT_WINDOW_MINIMIZED:
  case SDL_EVENT_WINDOW_OCCLUDED:
  case SDL_EVENT_WINDOW_HIDDEN:
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    if (!m_windowOccluded) {
      m_windowOccluded = true;
      inputMgr.onFocusLost();
      if (m_timestepManager) {
        m_timestepManager->setSoftwareFrameLimiting(true);
      }
      GAMEENGINE_DEBUG("Window occluded - enabling software frame limiting");
    }
    break;
  case SDL_EVENT_WINDOW_RESTORED:
  case SDL_EVENT_WINDOW_SHOWN:
  case SDL_EVENT_WINDOW_EXPOSED:
  case SDL_EVENT_WINDOW_FOCUS_GAINED:
    if (m_windowOccluded) {
      m_windowOccluded = false;
      if (m_timestepManager && m_vsyncRequested) {
        m_timestepManager->setSoftwareFrameLimiting(false);
      }
      GAMEENGINE_DEBUG("Window visible - GPU swapchain handles VSync");
    }
    break;
  default:
    break;
  }
}

void GameEngine::onDisplayChange(const SDL_Event &event) {
  const char *eventName = "Unknown";
  switch (event.type) {
  case SDL_EVENT_DISPLAY_ORIENTATION:
    eventName = "Orientation Change";
    break;
  case SDL_EVENT_DISPLAY_ADDED:
    eventName = "Display Added";
    break;
  case SDL_EVENT_DISPLAY_REMOVED:
    eventName = "Display Removed";
    break;
  case SDL_EVENT_DISPLAY_MOVED:
    eventName = "Display Moved";
    break;
  case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    eventName = "Content Scale Changed";
    break;
  default:
    break;
  }

  GAMEENGINE_INFO(std::format("Display event detected: {}", eventName));
  refreshWindowMetrics(eventName);
}

void GameEngine::refreshWindowMetrics(std::string_view reason) {
  if (!mp_window) {
    return;
  }

  int logicalWidth = m_windowWidth;
  int logicalHeight = m_windowHeight;
  if (!SDL_GetWindowSize(mp_window.get(), &logicalWidth, &logicalHeight)) {
    GAMEENGINE_ERROR(std::format("Failed to get logical window size: {}",
                                 SDL_GetError()));
  }

  int pixelWidth = logicalWidth;
  int pixelHeight = logicalHeight;
  if (!SDL_GetWindowSizeInPixels(mp_window.get(), &pixelWidth, &pixelHeight)) {
    GAMEENGINE_ERROR(std::format("Failed to get actual window pixel size: {}",
                                 SDL_GetError()));
  }

  float fontDPIScale = 1.0f;
#ifdef __APPLE__
  fontDPIScale =
      calculateFontDPIScale(logicalWidth, logicalHeight, pixelWidth,
                            pixelHeight);
#endif

  GAMEENGINE_INFO(std::format(
      "{} -> logical: {}x{}, pixels: {}x{}, font DPI scale: {:.2f}", reason,
      logicalWidth, logicalHeight, pixelWidth, pixelHeight, fontDPIScale));

  setWindowSize(logicalWidth, logicalHeight);
  setSizeInPixels(pixelWidth, pixelHeight);
  setDPIScale(fontDPIScale);
  updateDisplayRefreshRate();

  if (mp_backgroundSimManager) {
    mp_backgroundSimManager->configureForScreenSize(pixelWidth, pixelHeight);
    GAMEENGINE_INFO(std::format(
        "Tier radii reconfigured (active: {:.0f}, background: {:.0f})",
        mp_backgroundSimManager->getActiveRadius(),
        mp_backgroundSimManager->getBackgroundRadius()));
  }

  try {
    UIManager &uiManager = UIManager::Instance();
    uiManager.setGlobalScale(1.0f);

    FontManager &fontManager = FontManager::Instance();
    const std::string fontsPath = VoidLight::ResourcePath::resolve("res/fonts");
    if (!fontManager.reloadFontsForDisplay(fontsPath, m_windowWidth,
                                           m_windowHeight, m_dpiScale)) {
      GAMEENGINE_WARN("Failed to reload fonts for updated window/display metrics");
    } else {
      GAMEENGINE_INFO("Font system reinitialized successfully after window/display update");
    }

    uiManager.onWindowResize(getWidthInPixels(), getHeightInPixels());
    GAMEENGINE_INFO("UIManager notified for UI component repositioning");
  } catch (const std::exception &e) {
    GAMEENGINE_ERROR(std::format(
        "Error updating window/display dependent systems: {}", e.what()));
  }
}
