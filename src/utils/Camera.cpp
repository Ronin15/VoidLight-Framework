/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "utils/Camera.hpp"
#include "entities/Entity.hpp"
#include "core/Logger.hpp"
#include "core/GameEngine.hpp"
#include "events/CameraEvent.hpp"
#include "managers/EventManager.hpp"
#include "managers/WorldManager.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <format>

#include "gpu/GPURenderer.hpp"

namespace VoidLight {

Camera::Camera() {
    // Initialize zoom from default config
    if (m_config.isValid() && !m_config.zoomLevels.empty()) {
        m_currentZoomIndex = m_config.defaultZoomLevel;
        m_zoom = m_config.zoomLevels[m_currentZoomIndex];
    }
    CAMERA_INFO("Camera created with default configuration");
}

Camera::Camera(const Config& config) : m_config(config) {
    if (!m_config.isValid()) {
        CAMERA_WARN("Invalid camera configuration provided, using defaults");
        m_config = Config{};
    }

    // Initialize zoom from config
    if (m_config.isValid() && !m_config.zoomLevels.empty()) {
        m_currentZoomIndex = m_config.defaultZoomLevel;
        m_zoom = m_config.zoomLevels[m_currentZoomIndex];
    }
    CAMERA_INFO("Camera created with custom configuration");
}

Camera::Camera(float x, float y, float viewportWidth, float viewportHeight)
    : m_viewport{.width = viewportWidth, .height = viewportHeight} {
    m_position.setX(x);
    m_position.setY(y);
    m_targetPosition.setX(x);
    m_targetPosition.setY(y);
    m_previousPosition.setX(x);
    m_previousPosition.setY(y);
    m_lastRenderedCenter.setX(x);
    m_lastRenderedCenter.setY(y);

    if (!m_viewport.isValid()) {
        CAMERA_WARN("Invalid viewport dimensions provided, using defaults");
        m_viewport = Viewport{};
    }

    // Initialize zoom from default config
    if (m_config.isValid() && !m_config.zoomLevels.empty()) {
        m_currentZoomIndex = m_config.defaultZoomLevel;
        m_zoom = m_config.zoomLevels[m_currentZoomIndex];
    }
    CAMERA_INFO(std::format("Camera created at position ({}, {})", x, y));
}

void Camera::update(float deltaTime) {
    // Store previous position BEFORE updating so render-time interpolation
    // can blend across this fixed-timestep step.
    m_previousPosition = m_position;

    // Update camera shake first (no-ops if inactive)
    if (m_shakeTimeRemaining > 0.0f) {
        m_shakeTimeRemaining -= deltaTime;
        if (m_shakeTimeRemaining <= 0.0f) {
            m_shakeTimeRemaining = 0.0f;
            m_shakeOffset = Vector2D{0.0f, 0.0f};
        } else {
            m_shakeOffset = generateShakeOffset();
        }
    }

    if (m_mode == Mode::Follow && hasTarget()) {
        if (m_autoSyncWorldBounds) {
            uint64_t const currentVersion = WorldManager::Instance().getWorldVersion();
            if (currentVersion != m_lastWorldVersion) {
                syncWorldBounds();
            }
        }

        Vector2D const targetPos = getTargetPosition();
        Vector2D const delta = targetPos - m_position;
        float const distance = delta.length();

        if (m_config.maxCatchupDistance > 0.0f && distance > m_config.maxCatchupDistance) {
            // Target ran away (teleport, dash, scene seam) — snap forward so
            // we don't trail by a screen and a half. Pull only as close as
            // the catchup radius so the lag tail still exists.
            float const overshoot = distance - m_config.maxCatchupDistance;
            m_position = m_position + delta * (overshoot / distance);
        }

        if (m_config.deadZoneRadius > 0.0f && distance <= m_config.deadZoneRadius) {
            // Inside dead zone: hold position. Suppresses sub-pixel shimmer
            // when the player is idle / pacing in place.
        } else if (m_config.followLag <= 0.0f) {
            // Lag disabled: snap to target.
            m_position = targetPos;
        } else {
            // Frame-rate-independent exponential smoothing.
            // t = 1 - e^(-dt / lag). At dt == lag, t ~= 0.632.
            float const t = 1.0f - std::exp(-deltaTime / m_config.followLag);
            m_position = m_position + delta * t;
        }
    }

    if (m_config.clampToWorldBounds) {
        clampToWorldBounds();
    }
}

void Camera::setPosition(float x, float y) {
    Vector2D const oldPosition = m_position;
    m_position.setX(x);
    m_position.setY(y);
    m_targetPosition = m_position;      // Update target position to avoid interpolation
    m_previousPosition = m_position;    // Prevents interpolation sliding (matches Entity pattern)
    m_lastRenderedCenter = m_position;  // Sync coordinate conversions with new position

    // Fire position changed event if enabled
    if (m_eventFiringEnabled) {
        firePositionChangedEvent(oldPosition, m_position);
    }
}

void Camera::setPosition(const Vector2D& position) {
    setPosition(position.getX(), position.getY());
}

void Camera::setViewport(float width, float height) {
    if (width > 0.0f && height > 0.0f) {
        m_viewport.width = width;
        m_viewport.height = height;
    } else {
        CAMERA_WARN(std::format("Invalid viewport dimensions: {}x{}", width, height));
    }
}

void Camera::setViewport(const Viewport& viewport) {
    if (viewport.isValid()) {
        m_viewport = viewport;
    } else {
        CAMERA_WARN("Invalid viewport provided");
    }
}

void Camera::setWorldBounds(float minX, float minY, float maxX, float maxY) {
    if (maxX > minX && maxY > minY) {
        m_worldBounds.minX = minX;
        m_worldBounds.minY = minY;
        m_worldBounds.maxX = maxX;
        m_worldBounds.maxY = maxY;
    } else {
        CAMERA_WARN(std::format("Invalid world bounds: ({}, {}) to ({}, {})",
                                minX, minY, maxX, maxY));
    }
}

void Camera::setWorldBounds(const Bounds& bounds) {
    if (bounds.isValid()) {
        m_worldBounds = bounds;
    } else {
        CAMERA_WARN("Invalid world bounds provided");
    }
}

void Camera::setMode(Mode mode) {
    if (m_mode != mode) {
        Mode const oldMode = m_mode;
        m_mode = mode;
        
        // When switching to follow mode, snap to target if available
        if (mode == Mode::Follow && hasTarget()) {
            Vector2D const targetPos = getTargetPosition();
            m_targetPosition = targetPos;
        }
        
        // Fire mode changed event if enabled
        if (m_eventFiringEnabled) {
            fireModeChangedEvent(oldMode, mode);
        }

        CAMERA_INFO(std::format("Camera mode changed from {} to {}",
                                static_cast<int>(oldMode), static_cast<int>(mode)));
    }
}

void Camera::setTarget(const std::weak_ptr<Entity>& target) {
    std::weak_ptr<Entity> oldTarget = m_target;
    m_target = target;
    m_positionGetter = nullptr; // Clear function-based target
    
    // Fire target changed event if enabled
    if (m_eventFiringEnabled) {
        fireTargetChangedEvent(oldTarget, target);
    }
    
    if (auto targetPtr = target.lock()) {
        CAMERA_INFO("Camera target set to entity");
        // If in follow mode, update target position immediately
        if (m_mode == Mode::Follow) {
            Vector2D targetPos = getTargetPosition();
            m_targetPosition = targetPos;
        }
    } else {
        CAMERA_WARN("Attempted to set invalid target entity");
    }
}

void Camera::setTargetPositionGetter(std::function<Vector2D()> positionGetter) {
    m_positionGetter = std::move(positionGetter);
    m_target.reset(); // Clear entity-based target

    if (m_positionGetter) {
        CAMERA_INFO("Camera target set to position getter function");
        // If in follow mode, update target position immediately
        if (m_mode == Mode::Follow) {
            Vector2D targetPos = getTargetPosition();
            m_targetPosition = targetPos;
        }
    }
}

void Camera::clearTarget() {
    m_target.reset();
    m_positionGetter = nullptr;
    CAMERA_INFO("Camera target cleared");
}

bool Camera::hasTarget() const {
    return !m_target.expired() || (m_positionGetter != nullptr);
}

bool Camera::setConfig(const Config& config) {
    if (config.isValid()) {
        m_config = config;
        CAMERA_INFO(std::format("Camera configuration updated (followLag={}s, deadZone={}px, catchup={}px)",
                                 m_config.followLag, m_config.deadZoneRadius, m_config.maxCatchupDistance));
        return true;
    } else {
        CAMERA_WARN("Invalid camera configuration provided");
        return false;
    }
}

Camera::ViewRect Camera::getViewRect() const {
    // With SDL_SetRenderScale, the visible world area shrinks as zoom increases
    // At 2x zoom, you see half as much world space (400x300 instead of 800x600)
    float const worldViewWidth = m_viewport.width / m_zoom;
    float const worldViewHeight = m_viewport.height / m_zoom;

    // Use full floating-point precision for smooth sub-pixel camera movement
    // Callers snap to pixels at render time if tile-aligned rendering is needed
    return ViewRect{
        .x      = m_position.getX() - (worldViewWidth * 0.5f),
        .y      = m_position.getY() - (worldViewHeight * 0.5f),
        .width  = worldViewWidth,
        .height = worldViewHeight
    };
}

// Removed getInterpolatedViewRect(): Use getRenderOffset() + viewport/zoom instead
// This ensures single atomic read pattern across all game states

void Camera::computeOffsetFromCenter(float centerX, float centerY,
                                     float& offsetX, float& offsetY) const {
    // Compute camera offset (top-left corner) from a given center position
    float const worldViewWidth = m_viewport.width / m_zoom;
    float const worldViewHeight = m_viewport.height / m_zoom;

    // Convert center position to top-left offset
    offsetX = centerX - (worldViewWidth * 0.5f);
    offsetY = centerY - (worldViewHeight * 0.5f);

    // Apply world bounds clamping if enabled
    if (m_config.clampToWorldBounds) {
        float const maxOffsetX = m_worldBounds.maxX - worldViewWidth;
        float const maxOffsetY = m_worldBounds.maxY - worldViewHeight;

        if (maxOffsetX > m_worldBounds.minX) {
            offsetX = std::clamp(offsetX, m_worldBounds.minX, maxOffsetX);
        } else {
            // World smaller than viewport - center it
            offsetX = (m_worldBounds.minX + m_worldBounds.maxX - worldViewWidth) * 0.5f;
        }

        if (maxOffsetY > m_worldBounds.minY) {
            offsetY = std::clamp(offsetY, m_worldBounds.minY, maxOffsetY);
        } else {
            // World smaller than viewport - center it
            offsetY = (m_worldBounds.minY + m_worldBounds.maxY - worldViewHeight) * 0.5f;
        }
    }
}

Vector2D Camera::getRenderOffset(float& offsetX, float& offsetY, float interpolationAlpha) const {
    // The camera's own m_previousPosition -> m_position blend IS the trail.
    // In Follow mode, m_position chases the target via exponential smoothing
    // in update(); rendering it directly gives the intentional lag without
    // re-introducing target/camera desync jitter (we read one position only).
    Vector2D const center{
        m_previousPosition.getX() + (m_position.getX() - m_previousPosition.getX()) * interpolationAlpha,
        m_previousPosition.getY() + (m_position.getY() - m_previousPosition.getY()) * interpolationAlpha};

    computeOffsetFromCenter(center.getX(), center.getY(), offsetX, offsetY);

    // Cache rendered center for coordinate conversion consistency
    m_lastRenderedCenter = center;

    return center;
}

bool Camera::isPointVisible(float x, float y) const {
    ViewRect const view = getViewRect();
    return x >= view.left() && x <= view.right() && 
           y >= view.top() && y <= view.bottom();
}

bool Camera::isPointVisible(const Vector2D& point) const {
    return isPointVisible(point.getX(), point.getY());
}

bool Camera::isRectVisible(float x, float y, float width, float height) const {
    ViewRect const view = getViewRect();
    
    // Check if rectangles intersect
    return !(x + width < view.left() || x > view.right() || 
             y + height < view.top() || y > view.bottom());
}

void Camera::worldToScreen(float worldX, float worldY, float& screenX, float& screenY) const {
    // Use the last rendered center in Follow mode to match what's actually displayed
    // (m_position lags behind the actual rendered viewport center due to blend factor)
    float offsetX, offsetY;
    float const centerX = (m_mode == Mode::Follow) ? m_lastRenderedCenter.getX() : m_position.getX();
    float const centerY = (m_mode == Mode::Follow) ? m_lastRenderedCenter.getY() : m_position.getY();
    computeOffsetFromCenter(centerX, centerY, offsetX, offsetY);

    // Transform world position to screen position
    screenX = worldX - offsetX;
    screenY = worldY - offsetY;
}

void Camera::screenToWorld(float screenX, float screenY, float& worldX, float& worldY) const {
    // Mouse coordinates are in PHYSICAL window space (NOT scaled by SDL_SetRenderScale)
    // Convert physical mouse coords to logical coords first
    float const logicalX = screenX / m_zoom;
    float const logicalY = screenY / m_zoom;

    // Use the last rendered center in Follow mode to match what's actually displayed
    // (m_position lags behind the actual rendered viewport center due to blend factor)
    float offsetX, offsetY;
    float const centerX = (m_mode == Mode::Follow) ? m_lastRenderedCenter.getX() : m_position.getX();
    float const centerY = (m_mode == Mode::Follow) ? m_lastRenderedCenter.getY() : m_position.getY();
    computeOffsetFromCenter(centerX, centerY, offsetX, offsetY);

    // Inverse of worldToScreen: worldPos = screenPos + cameraOffset
    worldX = logicalX + offsetX;
    worldY = logicalY + offsetY;
}

Vector2D Camera::screenToWorld(const Vector2D& screenCoords) const {
    float worldX, worldY;
    screenToWorld(screenCoords.getX(), screenCoords.getY(), worldX, worldY);
    return Vector2D(worldX, worldY);
}

Vector2D Camera::worldToScreen(const Vector2D& worldCoords) const {
    float screenX, screenY;
    worldToScreen(worldCoords.getX(), worldCoords.getY(), screenX, screenY);
    return Vector2D(screenX, screenY);
}

void Camera::snapToTarget() {
    if (hasTarget()) {
        Vector2D const targetPos = getTargetPosition();
        setPosition(targetPos);
        CAMERA_INFO("Camera snapped to target position");
    } else {
        CAMERA_WARN("Cannot snap to target: no target set");
    }
}

void Camera::shake(float duration, float intensity) {
    if (duration > 0.0f && intensity > 0.0f) {
        bool const wasShaking = m_shakeTimeRemaining > 0.0f;
        m_shakeTimeRemaining = duration;
        m_shakeIntensity = intensity;

        // Fire shake started event if enabled and wasn't already shaking
        if (m_eventFiringEnabled && !wasShaking) {
            fireShakeStartedEvent(duration, intensity);
        }

        CAMERA_INFO(std::format("Camera shake started: duration={}s, intensity={}", duration, intensity));
    }
}

void Camera::syncWorldBounds() {
    // Auto-sync world bounds with WorldManager if enabled
    // This is needed for computeOffsetFromCenter() to work correctly in ALL modes
    if (m_autoSyncWorldBounds) {
        try {
            const auto &wm = WorldManager::Instance();
            if (wm.hasActiveWorld()) {
                uint64_t const currentVersion = wm.getWorldVersion();
                if (currentVersion != m_lastWorldVersion) {
                    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
                    if (wm.getWorldBounds(minX, minY, maxX, maxY)) {
                        m_worldBounds.minX = minX;
                        m_worldBounds.minY = minY;
                        m_worldBounds.maxX = maxX;
                        m_worldBounds.maxY = maxY;
                        m_lastWorldVersion = currentVersion;
                    }
                }
            }
        } catch (...) {
            // If WorldManager is unavailable, keep current bounds
        }
    }
}

void Camera::clampToWorldBounds() {
    // Ensure world bounds are current before clamping
    syncWorldBounds();

    // Calculate effective bounds using zoom-adjusted viewport dimensions
    // At higher zoom, you see less world space, so bounds are tighter
    float const halfViewWidth = m_viewport.halfWidth() / m_zoom;
    float const halfViewHeight = m_viewport.halfHeight() / m_zoom;

    // Calculate effective bounds (world bounds minus half viewport to keep camera centered)
    float const minX = m_worldBounds.minX + halfViewWidth;
    float const maxX = m_worldBounds.maxX - halfViewWidth;
    float const minY = m_worldBounds.minY + halfViewHeight;
    float const maxY = m_worldBounds.maxY - halfViewHeight;

    // Only clamp if the world is larger than the viewport
    if (maxX > minX) {
        m_position.setX(std::clamp(m_position.getX(), minX, maxX));
    } else {
        // World is smaller than viewport, center camera
        m_position.setX((m_worldBounds.minX + m_worldBounds.maxX) * 0.5f);
    }

    if (maxY > minY) {
        m_position.setY(std::clamp(m_position.getY(), minY, maxY));
    } else {
        // World is smaller than viewport, center camera
        m_position.setY((m_worldBounds.minY + m_worldBounds.maxY) * 0.5f);
    }
}

Vector2D Camera::getTargetPosition() const {
    if (auto targetPtr = m_target.lock()) {
        return targetPtr->getPosition();
    } else if (m_positionGetter) {
        return m_positionGetter();
    }
    return m_position; // Fallback to current position
}

Vector2D Camera::generateShakeOffset() const {
    // Per CLAUDE.md: Use member vars instead of static for thread safety
    const float normalizedTime = 1.0f - (m_shakeTimeRemaining / std::max(m_shakeTimeRemaining + 0.1f, 0.1f));
    const float currentIntensity = m_shakeIntensity * (1.0f - normalizedTime); // Fade out over time

    // Use member RNG and distribution for thread safety and performance
    return Vector2D{
        m_shakeDist(m_shakeRng) * currentIntensity,
        m_shakeDist(m_shakeRng) * currentIntensity
    };
}

// Event firing helper methods
void Camera::firePositionChangedEvent(const Vector2D& oldPosition, const Vector2D& newPosition) {
    try {
        const EventManager& eventMgr = EventManager::Instance();
        eventMgr.triggerCameraMoved(newPosition, oldPosition,
                                          EventManager::DispatchMode::Deferred);
    } catch (const std::exception& ex) {
        CAMERA_ERROR(std::format("Failed to fire CameraMovedEvent: {}", ex.what()));
    }
}

void Camera::fireModeChangedEvent(Mode oldMode, Mode newMode) {
    try {
        const EventManager& eventMgr = EventManager::Instance();
        eventMgr.triggerCameraModeChanged(static_cast<int>(newMode), static_cast<int>(oldMode),
                                                EventManager::DispatchMode::Deferred);
    } catch (const std::exception& ex) {
        CAMERA_ERROR(std::format("Failed to fire CameraModeChangedEvent: {}", ex.what()));
    }
}

void Camera::fireTargetChangedEvent(const std::weak_ptr<Entity>& oldTarget, const std::weak_ptr<Entity>& newTarget) {
    try {
        const EventManager& eventMgr = EventManager::Instance();
        eventMgr.triggerCameraTargetChanged(newTarget, oldTarget,
                                                  EventManager::DispatchMode::Deferred);
    } catch (const std::exception& ex) {
        CAMERA_ERROR(std::format("Failed to fire CameraTargetChangedEvent: {}", ex.what()));
    }
}

void Camera::fireShakeStartedEvent(float duration, float intensity) {
    try {
        const EventManager& eventMgr = EventManager::Instance();
        eventMgr.triggerCameraShakeStarted(duration, intensity,
                                                 EventManager::DispatchMode::Deferred);
    } catch (const std::exception& ex) {
        CAMERA_ERROR(std::format("Failed to fire CameraShakeStartedEvent: {}", ex.what()));
    }
}

void Camera::fireShakeEndedEvent() {
    try {
        const EventManager& eventMgr = EventManager::Instance();
        eventMgr.triggerCameraShakeEnded(EventManager::DispatchMode::Deferred);
    } catch (const std::exception& ex) {
        CAMERA_ERROR(std::format("Failed to fire CameraShakeEndedEvent: {}", ex.what()));
    }
}

void Camera::fireZoomChangedEvent(float oldZoom, float newZoom) {
    try {
        const EventManager& eventMgr = EventManager::Instance();
        eventMgr.triggerCameraZoomChanged(newZoom, oldZoom, EventManager::DispatchMode::Deferred);
    } catch (const std::exception& ex) {
        CAMERA_ERROR(std::format("Failed to fire CameraZoomChangedEvent: {}", ex.what()));
    }
}

// Zoom control methods
void Camera::zoomIn() {
    const int maxZoomIndex = static_cast<int>(m_config.zoomLevels.size()) - 1;
    if (m_currentZoomIndex < maxZoomIndex) {
        float const oldZoom = m_zoom;
        m_currentZoomIndex++;
        m_zoom = m_config.zoomLevels[m_currentZoomIndex];

        // Fire zoom changed event if enabled
        if (m_eventFiringEnabled) {
            fireZoomChangedEvent(oldZoom, m_zoom);
        }

        CAMERA_INFO(std::format("Camera zoomed in to level {} (zoom: {}x)", m_currentZoomIndex, m_zoom));
    } else {
        CAMERA_DEBUG("Camera already at maximum zoom level");
    }
}

void Camera::zoomOut() {
    if (m_currentZoomIndex > 0) {
        float const oldZoom = m_zoom;
        m_currentZoomIndex--;
        m_zoom = m_config.zoomLevels[m_currentZoomIndex];

        // Fire zoom changed event if enabled
        if (m_eventFiringEnabled) {
            fireZoomChangedEvent(oldZoom, m_zoom);
        }

        CAMERA_INFO(std::format("Camera zoomed out to level {} (zoom: {}x)", m_currentZoomIndex, m_zoom));
    } else {
        CAMERA_DEBUG("Camera already at minimum zoom level");
    }
}

bool Camera::setZoomLevel(int levelIndex) {
    const int maxZoomIndex = static_cast<int>(m_config.zoomLevels.size()) - 1;
    if (levelIndex < 0 || levelIndex > maxZoomIndex) {
        CAMERA_WARN(std::format("Invalid zoom level index: {} (valid range: 0-{})",
                                levelIndex, maxZoomIndex));
        return false;
    }

    if (m_currentZoomIndex != levelIndex) {
        float const oldZoom = m_zoom;
        m_currentZoomIndex = levelIndex;
        m_zoom = m_config.zoomLevels[m_currentZoomIndex];

        // Fire zoom changed event if enabled
        if (m_eventFiringEnabled) {
            fireZoomChangedEvent(oldZoom, m_zoom);
        }

        CAMERA_INFO(std::format("Camera zoom set to level {} (zoom: {}x)", m_currentZoomIndex, m_zoom));
    }

    return true;
}

void Camera::syncViewportWithEngine() {
    const auto& gpuRenderer = VoidLight::GPURenderer::Instance();
    float const viewportWidth = static_cast<float>(gpuRenderer.getViewportWidth());
    float const viewportHeight = static_cast<float>(gpuRenderer.getViewportHeight());

    // Only update if dimensions actually changed and valid
    if (viewportWidth > 0.0f && viewportHeight > 0.0f &&
        (m_viewport.width != viewportWidth || m_viewport.height != viewportHeight)) {
        setViewport(viewportWidth, viewportHeight);
    }
}

} // namespace VoidLight
