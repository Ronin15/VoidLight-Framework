/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef UI_DEMO_STATE_HPP
#define UI_DEMO_STATE_HPP

#include "gameStates/GameState.hpp"
#include <string>

// Example GameState that demonstrates comprehensive UIManager usage
class UIDemoState : public GameState {
public:
    UIDemoState();
    ~UIDemoState() override = default;

    // GameState interface
    bool enter() override;
    void update(float deltaTime) override;
    void handleInput() override;
    bool exit() override;
    GameStateId getStateId() const override { return GameStateId::UI_DEMO; }

    // GPU rendering support
    void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                           float interpolationAlpha) override;
    void renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                     SDL_GPURenderPass* swapchainPass) override;
    bool supportsGPURendering() const override { return true; }

private:
    // Demo state variables
    float m_sliderValue{0.5f};
    bool m_checkboxValue{false};
    int m_selectedListItem{-1};
    std::string m_inputText{};
    
    // No state-specific constants - keep UIManager generic
    
    // Helper methods
    void handleSliderChange(float value);
    void handleCheckboxToggle();
    void handleInputChange(const std::string& text);
    void handleListSelection();
    void handleAnimation();
    void handleThemeChange();
    void updateProgressBar(float deltaTime);
    void updateSliderLabel(float value);
    void updateInputLabel(const std::string& text);
    void applyDarkTheme(bool dark);
    
    // Animation and theme state
    bool m_darkTheme{false};
    float m_progressValue{0.0f};
    bool m_progressIncreasing{true};

    // Event log demo state (sample messages)
    float m_eventLogTimer{0.0f};
    size_t m_eventLogMessageIndex{0};
    static constexpr float EVENT_LOG_INTERVAL{2.0f};
    void updateEventLogDemo(float deltaTime);
};

#endif // UI_DEMO_STATE_HPP
