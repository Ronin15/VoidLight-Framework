/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "utils/Vector2D.hpp"
#include <algorithm>
#include <memory>
#include <functional>
#include <cstdint>
#include <random>

// Forward declarations
class Entity;
namespace VoidLight {

/**
 * @brief Camera utility class for 2D world navigation and rendering
 *
 * This camera follows industry best practices:
 * - Non-singleton design for flexibility
 * - Smooth interpolation for player following
 * - World bounds clamping
 * - Modular and testable architecture
 * - Support for different camera modes
 */
class Camera {
public:
    /**
     * @brief Camera modes for different behaviors
     */
    enum class Mode {
        Free,       // Camera moves freely, not following anything
        Follow,     // Camera follows a target entity with smooth interpolation
        Fixed       // Camera is fixed at a specific position
    };

    /**
     * @brief Camera configuration structure
     *
     * Follow-mode tuning:
     *  - followLag is a time constant (seconds). The camera reaches ~63% of
     *    a step toward the target after followLag seconds, ~95% after 3x.
     *    Smaller = snappier, larger = more cinematic trail.
     *      0.00f  : disabled (camera snaps to target every update)
     *      0.08f  : tight, action-y
     *      0.18f  : default, subtle trail
     *      0.35f+ : floaty / cinematic
     *  - deadZoneRadius (pixels): if the target sits inside this radius
     *    around the camera, no smoothing is applied. Suppresses sub-pixel
     *    shimmer when the player is idle. 0 disables the dead zone.
     *  - maxCatchupDistance (pixels): when the target gets farther than
     *    this from the camera (e.g. teleport, fast dash), the camera snaps
     *    forward so it never falls arbitrarily far behind. 0 disables.
     */
    struct Config {
        float followLag{0.30f};            // Time constant in seconds (see above)
        float deadZoneRadius{4.0f};        // Pixels; 0 = no dead zone
        float maxCatchupDistance{600.0f};  // Pixels; 0 = no catchup snap
        bool clampToWorldBounds{true};

        // Zoom configuration
        std::vector<float> zoomLevels{1.0f, 2.0f, 3.0f};  // Integer zoom levels (pixel-perfect)
        int defaultZoomLevel{0};                           // Starting zoom level index

        bool isValid() const {
            if (followLag < 0.0f || deadZoneRadius < 0.0f || maxCatchupDistance < 0.0f) {
                return false;
            }
            if (zoomLevels.empty()) {
                return false;
            }
            if (!std::all_of(zoomLevels.begin(), zoomLevels.end(),
                             [](float zoom) { return zoom > 0.0f; })) {
                return false;
            }
            if (defaultZoomLevel < 0 || defaultZoomLevel >= static_cast<int>(zoomLevels.size())) {
                return false;
            }
            return true;
        }
    };

    /**
     * @brief Camera bounds structure for world clamping
     */
    struct Bounds {
        float minX{0.0f};
        float minY{0.0f};
        float maxX{1000.0f};
        float maxY{1000.0f};

        bool isValid() const {
            return maxX > minX && maxY > minY;
        }
    };

    /**
     * @brief Viewport structure for rendering calculations
     */
    struct Viewport {
        float width{0.0f};   // Must be set via constructor or setViewport()
        float height{0.0f};  // Must be set via constructor or setViewport()

        bool isValid() const {
            return width > 0.0f && height > 0.0f;
        }

        // Convenience methods
        float halfWidth() const { return width * 0.5f; }
        float halfHeight() const { return height * 0.5f; }
    };

public:
    /**
     * @brief Constructor with default configuration
     */
    Camera();

    /**
     * @brief Constructor with custom configuration
     * @param config Camera configuration
     */
    explicit Camera(const Config& config);

    /**
     * @brief Constructor with position and viewport
     * @param x Initial camera X position
     * @param y Initial camera Y position
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     */
    Camera(float x, float y, float viewportWidth, float viewportHeight);

    /**
     * @brief Default destructor
     */
    ~Camera() = default;

    // Non-copyable/movable (has std::atomic, RNG state, weak_ptr targets)
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&) = delete;
    Camera& operator=(Camera&&) = delete;

    /**
     * @brief Updates the camera position based on mode and target
     * @param deltaTime Time elapsed since last update in seconds
     */
    void update(float deltaTime);

    /**
     * @brief Sets the camera position directly
     * @param x X position
     * @param y Y position
     */
    void setPosition(float x, float y);

    /**
     * @brief Sets the camera position using Vector2D
     * @param position New position
     */
    void setPosition(const Vector2D& position);

    /**
     * @brief Gets the current camera position
     * @return Current position as Vector2D
     */
    const Vector2D& getPosition() const { return m_position; }

    /**
     * @brief Gets camera X position (float precision for smooth entity positioning)
     * @return X coordinate
     */
    float getX() const { return m_position.getX(); }

    /**
     * @brief Gets camera Y position (float precision for smooth entity positioning)
     * @return Y coordinate
     */
    float getY() const { return m_position.getY(); }

    /**
     * @brief Sets the viewport size
     * @param width Viewport width
     * @param height Viewport height
     */
    void setViewport(float width, float height);

    /**
     * @brief Sets the viewport using Viewport structure
     * @param viewport New viewport
     */
    void setViewport(const Viewport& viewport);

    /**
     * @brief Gets the current viewport
     * @return Current viewport
     */
    const Viewport& getViewport() const { return m_viewport; }

    /**
     * @brief Sets the world bounds for camera clamping
     * @param minX Minimum X coordinate
     * @param minY Minimum Y coordinate
     * @param maxX Maximum X coordinate
     * @param maxY Maximum Y coordinate
     */
    void setWorldBounds(float minX, float minY, float maxX, float maxY);

    /**
     * @brief Sets the world bounds using Bounds structure
     * @param bounds New world bounds
     */
    void setWorldBounds(const Bounds& bounds);

    /**
     * @brief Gets the current world bounds
     * @return Current world bounds
     */
    const Bounds& getWorldBounds() const { return m_worldBounds; }

    /**
     * @brief Sets the camera mode
     * @param mode New camera mode
     */
    void setMode(Mode mode);

    /**
     * @brief Gets the current camera mode
     * @return Current mode
     */
    Mode getMode() const { return m_mode; }

    /**
     * @brief Sets the target entity for following mode
     * @param target Weak pointer to target entity
     */
    void setTarget(const std::weak_ptr<Entity>& target);

    /**
     * @brief Sets target using a function that returns position
     * @param positionGetter Function that returns target position
     */
    void setTargetPositionGetter(std::function<Vector2D()> positionGetter);

    /**
     * @brief Clears the current target
     */
    void clearTarget();

    /**
     * @brief Gets whether camera has a valid target
     * @return True if target is valid, false otherwise
     */
    bool hasTarget() const;

    /**
     * @brief Updates camera configuration
     * @param config New configuration
     * @return True if configuration is valid and applied
     */
    bool setConfig(const Config& config);

    /**
     * @brief Gets current camera configuration
     * @return Current configuration
     */
    const Config& getConfig() const { return m_config; }

    /**
     * @brief Gets the view rectangle for rendering calculations
     * @return View rectangle with top-left corner and dimensions
     */
    struct ViewRect {
        float x, y, width, height;

        // Convenience methods
        float left() const { return x; }
        float right() const { return x + width; }
        float top() const { return y; }
        float bottom() const { return y + height; }
        float centerX() const { return x + width * 0.5f; }
        float centerY() const { return y + height * 0.5f; }
    };

    /**
     * @brief Gets the current view rectangle (uses update-thread position)
     * @return View rectangle for culling and rendering
     */
    ViewRect getViewRect() const;

    /**
     * @brief Gets render offset and returns the center position used for sync
     *
     * In Follow mode, reads target's interpolated position (ONE atomic read)
     * and derives camera offset from it. Returns the position so caller can
     * render the followed entity at EXACTLY the same spot - eliminating jitter
     * from separate atomic reads.
     *
     * In Free/Fixed modes, uses camera's own interpolation and returns the
     * camera's interpolated center position.
     *
     * NOTE: This method stores m_previousPosition for next frame's interpolation.
     * Called once per visual frame from render(), ensuring correct interpolation
     * even when update() runs multiple times per frame (fixed timestep catchup).
     *
     * @param offsetX Output: camera X offset (top-left of view)
     * @param offsetY Output: camera Y offset (top-left of view)
     * @param interpolationAlpha Blend factor for position interpolation
     * @return The center position used for offset calculation (for synced rendering)
     */
    Vector2D getRenderOffset(float& offsetX, float& offsetY, float interpolationAlpha = 1.0f) const;


    /**
     * @brief Checks if a point is visible in the camera view
     * @param x Point X coordinate
     * @param y Point Y coordinate
     * @return True if point is visible
     */
    bool isPointVisible(float x, float y) const;

    /**
     * @brief Checks if a point is visible in the camera view
     * @param point Point to check
     * @return True if point is visible
     */
    bool isPointVisible(const Vector2D& point) const;

    /**
     * @brief Checks if a rectangle intersects with the camera view
     * @param x Rectangle X coordinate
     * @param y Rectangle Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @return True if rectangle is visible
     */
    bool isRectVisible(float x, float y, float width, float height) const;

    /**
     * @brief Transforms world coordinates to screen coordinates
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param screenX Output screen X coordinate
     * @param screenY Output screen Y coordinate
     */
    void worldToScreen(float worldX, float worldY, float& screenX, float& screenY) const;

    /**
     * @brief Transforms screen coordinates to world coordinates
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param worldX Output world X coordinate
     * @param worldY Output world Y coordinate
     */
     void screenToWorld(float screenX, float screenY, float& worldX, float& worldY) const;
     Vector2D screenToWorld(const Vector2D& screenCoords) const;
     Vector2D worldToScreen(const Vector2D& worldCoords) const;

    /**
     * @brief Immediately snaps camera to target position (no interpolation)
     */
    void snapToTarget();

    /**
     * @brief Shakes the camera for a given duration and intensity
     * @param duration Duration of shake in seconds
     * @param intensity Shake intensity (pixels)
     */
    void shake(float duration, float intensity);

    /**
     * @brief Gets whether camera is currently shaking
     * @return True if shaking
     */
    bool isShaking() const { return m_shakeTimeRemaining > 0.0f; }

    /**
     * @brief Enables or disables event firing for camera state changes
     * @param enabled Whether to fire events
     */
    void setEventFiringEnabled(bool enabled) { m_eventFiringEnabled = enabled; }

    /**
     * @brief Gets whether event firing is enabled
     * @return True if events are fired on state changes
     */
    bool isEventFiringEnabled() const { return m_eventFiringEnabled; }

    /**
     * @brief Zoom in to the next zoom level (make objects larger)
     * Cycles through configured zoom levels (stops at max)
     */
    void zoomIn();

    /**
     * @brief Zoom out to the previous zoom level (make objects smaller)
     * Cycles through configured zoom levels (stops at min)
     */
    void zoomOut();

    /**
     * @brief Set zoom to a specific level index
     * @param levelIndex Index into configured zoomLevels array
     * @return True if level was valid and set
     */
    bool setZoomLevel(int levelIndex);

    /**
     * @brief Get current zoom scale factor
     * @return Current zoom level (from configured zoomLevels)
     */
    float getZoom() const { return m_zoom; }

    /**
     * @brief Get current zoom level index
     * @return Index into configured zoomLevels array
     */
    int getZoomLevel() const { return m_currentZoomIndex; }

    /**
     * @brief Get number of configured zoom levels
     * @return Number of zoom levels
     */
    int getNumZoomLevels() const { return static_cast<int>(m_config.zoomLevels.size()); }

    /**
     * @brief Synchronize viewport dimensions with GameEngine logical size
     *
     * Automatically updates the camera viewport to match the current logical
     * resolution from GameEngine. Call this in game state update() methods to
     * keep the camera viewport in sync with window resize events.
     *
     * This method is safe to call every frame as it only updates if dimensions changed.
     */
    void syncViewportWithEngine();

private:
    // Core camera state (initialized via constructor)
    Vector2D m_position{0.0f, 0.0f};        // Current camera position
    Vector2D m_targetPosition{0.0f, 0.0f};  // Target position for interpolation
    Viewport m_viewport{};                   // Camera viewport size (set via constructor)
    Bounds m_worldBounds{};                  // World boundaries (auto-synced from WorldManager)
    Config m_config{};                       // Camera configuration
    Mode m_mode{Mode::Free};                // Current camera mode

    // Target tracking
    std::weak_ptr<Entity> m_target;         // Target entity to follow
    std::function<Vector2D()> m_positionGetter; // Alternative position getter

    // Camera shake
    float m_shakeTimeRemaining{0.0f};       // Remaining shake time
    float m_shakeIntensity{0.0f};           // Current shake intensity
    Vector2D m_shakeOffset{0.0f, 0.0f};     // Current shake offset

    // Event firing
    bool m_eventFiringEnabled{false};      // Whether to fire events on state changes

    // World sync (auto-correct camera bounds when world changes)
    bool m_autoSyncWorldBounds{true};
    uint64_t m_lastWorldVersion{0};

    // Zoom state
    float m_zoom{1.0f};              // Current zoom level (1.0 = native)
    int m_currentZoomIndex{0};       // Index into ZOOM_LEVELS array

    // Previous position for render interpolation (smooth camera at any refresh rate)
    Vector2D m_previousPosition{960.0f, 540.0f};

    // Last rendered center from getRenderOffset() - used by worldToScreen/screenToWorld
    // to match coordinate conversions with the actual rendered viewport in Follow mode
    mutable Vector2D m_lastRenderedCenter{0.0f, 0.0f};

    // Shake random number generation (mutable for const generateShakeOffset)
    // Per CLAUDE.md: NEVER use static vars in threaded code - use member vars instead
    mutable std::mt19937 m_shakeRng{std::random_device{}()};
    mutable std::uniform_real_distribution<float> m_shakeDist{-1.0f, 1.0f};

    // Internal helper methods
    void syncWorldBounds();       // Sync m_worldBounds from WorldManager (called every update)
    void clampToWorldBounds();    // Clamp camera position to world bounds
    Vector2D getTargetPosition() const;
    Vector2D generateShakeOffset() const;

    // Internal: compute offset from a given center position (used by public getRenderOffset)
    void computeOffsetFromCenter(float centerX, float centerY, float& offsetX, float& offsetY) const;

    // Event firing helpers
    void firePositionChangedEvent(const Vector2D& oldPosition, const Vector2D& newPosition);
    void fireModeChangedEvent(Mode oldMode, Mode newMode);
    void fireTargetChangedEvent(const std::weak_ptr<Entity>& oldTarget, const std::weak_ptr<Entity>& newTarget);
    void fireShakeStartedEvent(float duration, float intensity);
    void fireShakeEndedEvent();
    void fireZoomChangedEvent(float oldZoom, float newZoom);
};

} // namespace VoidLight

#endif // CAMERA_HPP
