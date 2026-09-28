/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef SCENE_CHANGE_EVENT_HPP
#define SCENE_CHANGE_EVENT_HPP

/**
 * @file SceneChangeEvent.hpp
 * @brief Event implementation for scene transitions and level changes
 *
 * SceneChangeEvent allows the game to trigger scene changes based on:
 * - Player position/proximity to trigger zones
 * - Story progression
 * - Player actions
 * - Timer-based triggers
 */

#include "Event.hpp"
#include "utils/Vector2D.hpp"
#include <string>
#include <functional>
#include <vector>
#include <iostream>
#include <unordered_map>

enum class TransitionType {
    Fade,
    Dissolve,
    Wipe,
    Slide,
    Instant,
    Custom
};

// Stream operator declared here; defined in src/events/SceneChangeEvent.cpp
std::ostream& operator<<(std::ostream& os, const TransitionType& type);

struct TransitionParams {
    float duration{1.0f}; // Duration in seconds
    std::string transitionEffect; // Effect resource ID
    bool playSound{true}; // Whether to play transition sound
    std::string soundEffect; // Sound effect ID
    float soundVolume{1.0f}; // 0.0 to 1.0

    // Color for fade transitions
    float colorR{0.0f};
    float colorG{0.0f};
    float colorB{0.0f};
    float colorA{1.0f};

    // Direction for slide/wipe transitions (in degrees, 0 = right, 90 = up, etc.)
    float direction{0.0f};

    // Custom shader parameters
    std::string shaderID;
    std::unordered_map<std::string, float> shaderParams;

    // Default constructor
    TransitionParams() = default;

    // Constructor with commonly used parameters
    explicit TransitionParams(float duration, TransitionType type = TransitionType::Fade);
};

class SceneChangeEvent : public Event {
public:
    SceneChangeEvent(const std::string& name, const std::string& targetSceneID);
    ~SceneChangeEvent() override = default;

    // Core event methods implementation
    void update() override;
    void execute() override;
    void reset() override;
    void clean() override;

    // Event identification
    std::string getName() const override { return m_name; }
    std::string getType() const override { return "SceneChange"; }
    std::string getTypeName() const override { return "SceneChangeEvent"; }
    EventTypeId getTypeId() const override { return EventTypeId::SceneChange; }

    // Scene-specific methods
    const std::string& getTargetSceneID() const { return m_targetSceneID; }
    void setTargetSceneID(const std::string& sceneID) { m_targetSceneID = sceneID; }

    // Transition parameters
    void setTransitionType(TransitionType type);
    TransitionType getTransitionType() const { return m_transitionType; }

    void setTransitionParams(const TransitionParams& params) { m_transitionParams = params; }
    const TransitionParams& getTransitionParams() const { return m_transitionParams; }

    // Condition checking
    bool checkConditions() override;

    // Trigger zone methods
    void setTriggerZone(float x, float y, float radius);
    void setTriggerZone(float x1, float y1, float x2, float y2);
    bool isPlayerInTriggerZone() const;

    // Add custom conditions
    void addCondition(std::function<bool()> condition);

    // Player input triggers
    void setRequirePlayerInput(bool required) { m_requirePlayerInput = required; }
    void setInputKey(const std::string& keyName) { m_inputKeyName = keyName; }
    bool isPlayerInputTriggered() const;

    // Timer-based triggers
    void setTimerTrigger(float seconds);
    void startTimer();
    void stopTimer();
    bool isTimerComplete() const;

    // Direct scene change (for scripting)
    static void forceSceneChange(const std::string& sceneID, TransitionType type = TransitionType::Fade, float duration = 1.0f);

private:
    std::string m_name;
    std::string m_targetSceneID;
    TransitionType m_transitionType{TransitionType::Fade};
    TransitionParams m_transitionParams;

    // Condition tracking
    std::vector<std::function<bool()>> m_conditions;

    // Trigger zone parameters
    enum class ZoneType {
        None,
        Circle,
        Rectangle
    };

    ZoneType m_zoneType{ZoneType::None};
    Vector2D m_zoneCenter{0, 0};
    float m_zoneRadius{0.0f};
    float m_zoneX1{0.0f}, m_zoneY1{0.0f}, m_zoneX2{0.0f}, m_zoneY2{0.0f};

    // Input triggers
    bool m_requirePlayerInput{false};
    std::string m_inputKeyName;

    // Timer trigger
    bool m_useTimer{false};
    bool m_timerActive{false};
    float m_timerDuration{0.0f};
    float m_timerElapsed{0.0f};

    // Internal state
    bool m_inTransition{false};
    float m_transitionProgress{0.0f};

    // Helper methods
    bool checkZoneCondition() const;
    bool checkInputCondition() const;
    bool checkTimerCondition() const;

    // Helper to get player position
    Vector2D getPlayerPosition() const;
};

#endif // SCENE_CHANGE_EVENT_HPP
