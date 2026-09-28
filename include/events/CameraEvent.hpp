/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef CAMERA_EVENT_HPP
#define CAMERA_EVENT_HPP

#include "events/Event.hpp"
#include "utils/Vector2D.hpp"
#include <string>
#include <memory>

// Forward declaration
class Entity;

/**
 * @brief Event types for camera-related changes
 */
enum class CameraEventType {
    CameraMoved,        // Camera position changed
    CameraModeChanged,  // Camera mode changed (Free, Follow, Fixed)
    CameraTargetChanged,// Camera target entity changed
    CameraShakeStarted, // Camera shake effect started
    CameraShakeEnded,   // Camera shake effect ended
    ViewportChanged,    // Camera viewport size changed
    CameraZoomChanged   // Camera zoom level changed
};

/**
 * @brief Base class for all camera-related events
 */
class CameraEvent : public Event {
public:
    explicit CameraEvent(CameraEventType eventType)
        : Event(), m_eventType(eventType) {}

    ~CameraEvent() override = default;

    CameraEventType getEventType() const { return m_eventType; }

    std::string getTypeName() const override { return "CameraEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::Camera; }

    // Required Event interface implementations
    void update() override {}
    void execute() override {}
    void clean() override {}
    std::string getName() const override { return getTypeName(); }
    std::string getType() const override { return getTypeName(); }
    bool checkConditions() override { return true; }

    void reset() override {
        Event::resetCooldown();
        m_hasTriggered = false;
        // Keep event type as it's immutable
    }

protected:
    CameraEventType m_eventType;
};

/**
 * @brief Event fired when camera position changes
 */
class CameraMovedEvent : public CameraEvent {
public:
    CameraMovedEvent(const Vector2D& newPosition, const Vector2D& oldPosition)
        : CameraEvent(CameraEventType::CameraMoved), m_newPosition(newPosition), m_oldPosition(oldPosition) {}

    const Vector2D& getNewPosition() const { return m_newPosition; }
    const Vector2D& getOldPosition() const { return m_oldPosition; }

    float getNewX() const { return m_newPosition.getX(); }
    float getNewY() const { return m_newPosition.getY(); }
    float getOldX() const { return m_oldPosition.getX(); }
    float getOldY() const { return m_oldPosition.getY(); }

    std::string getTypeName() const override { return "CameraMovedEvent"; }
    std::string getName() const override { return "CameraMovedEvent"; }
    std::string getType() const override { return "CameraMovedEvent"; }

    void reset() override {
        CameraEvent::reset();
        m_newPosition = Vector2D{0, 0};
        m_oldPosition = Vector2D{0, 0};
    }

    // Setters for pool reuse
    void setNewPosition(const Vector2D& pos) { m_newPosition = pos; }
    void setOldPosition(const Vector2D& pos) { m_oldPosition = pos; }
    void configure(const Vector2D& newPos, const Vector2D& oldPos) {
        m_newPosition = newPos;
        m_oldPosition = oldPos;
    }

private:
    Vector2D m_newPosition;
    Vector2D m_oldPosition;
};

/**
 * @brief Event fired when camera mode changes
 */
class CameraModeChangedEvent : public CameraEvent {
public:
    enum class Mode {
        Free = 0,
        Follow = 1,
        Fixed = 2
    };

    CameraModeChangedEvent(Mode newMode, Mode oldMode)
        : CameraEvent(CameraEventType::CameraModeChanged), m_newMode(newMode), m_oldMode(oldMode) {}

    Mode getNewMode() const { return m_newMode; }
    Mode getOldMode() const { return m_oldMode; }

    std::string getModeString(Mode mode) const;

    std::string getTypeName() const override { return "CameraModeChangedEvent"; }
    std::string getName() const override { return "CameraModeChangedEvent"; }
    std::string getType() const override { return "CameraModeChangedEvent"; }

    void reset() override {
        CameraEvent::reset();
        m_newMode = Mode::Free;
        m_oldMode = Mode::Free;
    }

private:
    Mode m_newMode;
    Mode m_oldMode;
};

/**
 * @brief Event fired when camera target changes
 */
class CameraTargetChangedEvent : public CameraEvent {
public:
    CameraTargetChangedEvent(std::weak_ptr<Entity> newTarget,
        std::weak_ptr<Entity> oldTarget)
        : CameraEvent(CameraEventType::CameraTargetChanged), m_newTarget(newTarget), m_oldTarget(oldTarget) {}

    std::weak_ptr<Entity> getNewTarget() const { return m_newTarget; }
    std::weak_ptr<Entity> getOldTarget() const { return m_oldTarget; }

    bool hasNewTarget() const { return !m_newTarget.expired(); }
    bool hadOldTarget() const { return !m_oldTarget.expired(); }

    std::string getTypeName() const override { return "CameraTargetChangedEvent"; }
    std::string getName() const override { return "CameraTargetChangedEvent"; }
    std::string getType() const override { return "CameraTargetChangedEvent"; }

    void reset() override {
        CameraEvent::reset();
        m_newTarget.reset();
        m_oldTarget.reset();
    }

private:
    std::weak_ptr<Entity> m_newTarget;
    std::weak_ptr<Entity> m_oldTarget;
};

/**
 * @brief Event fired when camera shake starts
 */
class CameraShakeStartedEvent : public CameraEvent {
public:
    CameraShakeStartedEvent(float duration, float intensity)
        : CameraEvent(CameraEventType::CameraShakeStarted), m_duration(duration), m_intensity(intensity) {}

    float getDuration() const { return m_duration; }
    float getIntensity() const { return m_intensity; }

    std::string getTypeName() const override { return "CameraShakeStartedEvent"; }
    std::string getName() const override { return "CameraShakeStartedEvent"; }
    std::string getType() const override { return "CameraShakeStartedEvent"; }

    void reset() override {
        CameraEvent::reset();
        m_duration = 0.0f;
        m_intensity = 0.0f;
    }

    // Setters for pool reuse
    void setDuration(float d) { m_duration = d; }
    void setIntensity(float i) { m_intensity = i; }
    void configure(float duration, float intensity) {
        m_duration = duration;
        m_intensity = intensity;
    }

private:
    float m_duration{0.0f};
    float m_intensity{0.0f};
};

/**
 * @brief Event fired when camera shake ends
 */
class CameraShakeEndedEvent : public CameraEvent {
public:
    CameraShakeEndedEvent() : CameraEvent(CameraEventType::CameraShakeEnded) {}

    std::string getTypeName() const override { return "CameraShakeEndedEvent"; }
    std::string getName() const override { return "CameraShakeEndedEvent"; }
    std::string getType() const override { return "CameraShakeEndedEvent"; }
};

/**
 * @brief Event fired when camera viewport changes
 */
class ViewportChangedEvent : public CameraEvent {
public:
    ViewportChangedEvent(float newWidth, float newHeight, float oldWidth, float oldHeight)
        : CameraEvent(CameraEventType::ViewportChanged), m_newWidth(newWidth), m_newHeight(newHeight), m_oldWidth(oldWidth), m_oldHeight(oldHeight) {}

    float getNewWidth() const { return m_newWidth; }
    float getNewHeight() const { return m_newHeight; }
    float getOldWidth() const { return m_oldWidth; }
    float getOldHeight() const { return m_oldHeight; }

    std::string getTypeName() const override { return "ViewportChangedEvent"; }
    std::string getName() const override { return "ViewportChangedEvent"; }
    std::string getType() const override { return "ViewportChangedEvent"; }

    void reset() override {
        CameraEvent::reset();
        m_newWidth = 0.0f;
        m_newHeight = 0.0f;
        m_oldWidth = 0.0f;
        m_oldHeight = 0.0f;
    }

private:
    float m_newWidth{0.0f};
    float m_newHeight{0.0f};
    float m_oldWidth{0.0f};
    float m_oldHeight{0.0f};
};

/**
 * @brief Event fired when camera zoom level changes
 */
class CameraZoomChangedEvent : public CameraEvent {
public:
    CameraZoomChangedEvent(float newZoom, float oldZoom)
        : CameraEvent(CameraEventType::CameraZoomChanged), m_newZoom(newZoom), m_oldZoom(oldZoom) {}

    float getNewZoom() const { return m_newZoom; }
    float getOldZoom() const { return m_oldZoom; }

    std::string getTypeName() const override { return "CameraZoomChangedEvent"; }
    std::string getName() const override { return "CameraZoomChangedEvent"; }
    std::string getType() const override { return "CameraZoomChangedEvent"; }

    void reset() override {
        CameraEvent::reset();
        m_newZoom = 1.0f;
        m_oldZoom = 1.0f;
    }

private:
    float m_newZoom{1.0f};
    float m_oldZoom{1.0f};
};

#endif // CAMERA_EVENT_HPP
