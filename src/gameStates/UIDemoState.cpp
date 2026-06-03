/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/UIDemoState.hpp"
#include "managers/UIManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/GameStateManager.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"

#include "gpu/GPURenderer.hpp"

#include <array>
#include <format>

// UIDemoState Implementation
UIDemoState::UIDemoState() {
    // Pure UIManager approach - no UIScreen
}

bool UIDemoState::enter() {
    GAMESTATE_INFO("Entering UI Example State");

    auto& ui = UIManager::Instance();

    // Calculate relative positioning for cross-resolution compatibility
    int leftColumnX = 50;
    int leftColumnWidth = 220;
    int rightColumnX = ui.getWidthInPixels() / 2 + 50;
    int rightColumnWidth = ui.getWidthInPixels() - rightColumnX - 50;

    // Create overlay background using auto-detection
    ui.createOverlay();
    // Set overlay to resize with window (full width and height)
    ui.setComponentPositioning("__overlay", {UIPositionMode::TOP_ALIGNED, 0, 0, -1, -1});

    // Title using auto-positioning
    ui.createTitleAtTop("uiexample_title_label", "UI Demo State", 40);

    // Back button using auto-positioning and instruction
    ui.createButtonAtBottom("uiexample_back_btn", "Back", 120, 40);
    // Position instruction label to the right of the button (button is at offsetX=20, width=120, so start at 20+120+10=150)
    ui.createLabel("uiexample_back_instruction", {150, ui.getHeightInPixels() - 75, 200, 30}, "Press B to go back");
    ui.setComponentPositioning("uiexample_back_instruction", {UIPositionMode::BOTTOM_ALIGNED, 150, 20, 200, 30});

    // Slider demo (left column - TOP_ALIGNED for exact positioning like AIDemoState)
    ui.createSlider("uiexample_demo_slider", {leftColumnX, 140, 200, 30}, 0.0f, 1.0f);
    ui.setValue("uiexample_demo_slider", 0.5f);
    ui.setComponentPositioning("uiexample_demo_slider", {UIPositionMode::TOP_ALIGNED, 50, 140, 200, 30});

    ui.createLabel("uiexample_slider_label", {leftColumnX + 210, 140, 200, 30}, "Slider: 0.50");
    ui.setComponentPositioning("uiexample_slider_label", {UIPositionMode::TOP_ALIGNED, 260, 140, 200, 30});

    // Checkbox demo (left column - TOP_ALIGNED for exact positioning)
    ui.createCheckbox("uiexample_demo_checkbox", {leftColumnX, 190, 250, 30}, "Toggle Option");
    ui.setComponentPositioning("uiexample_demo_checkbox", {UIPositionMode::TOP_ALIGNED, 50, 190, 250, 30});

    // Input field demo (left column - TOP_ALIGNED for exact positioning)
    ui.createInputField("uiexample_demo_input", {leftColumnX, 240, 200, 30}, "Type here...");
    ui.setComponentPositioning("uiexample_demo_input", {UIPositionMode::TOP_ALIGNED, 50, 240, 200, 30});

    ui.createLabel("uiexample_input_label", {leftColumnX + 210, 240, 300, 30}, "Input: (empty)");
    ui.setComponentPositioning("uiexample_input_label", {UIPositionMode::TOP_ALIGNED, 260, 240, 300, 30});

    // Progress bar demo (left column - TOP_ALIGNED for exact positioning)
    ui.createProgressBar("uiexample_demo_progress", {leftColumnX, 290, 200, 20}, 0.0f, 1.0f);
    ui.setComponentPositioning("uiexample_demo_progress", {UIPositionMode::TOP_ALIGNED, 50, 290, 200, 20});

    ui.createLabel("uiexample_progress_label", {leftColumnX + 210, 290, 200, 20}, "Auto Progress");
    ui.setComponentPositioning("uiexample_progress_label", {UIPositionMode::TOP_ALIGNED, 260, 290, 200, 20});

    // List demo (left column - TOP_ALIGNED for exact positioning)
    // Height: 180px = 5 items × 32px/item + 20px buffer
    ui.createList("uiexample_demo_list", {leftColumnX, 340, leftColumnWidth, 180});
    ui.setComponentPositioning("uiexample_demo_list", {UIPositionMode::TOP_ALIGNED, 50, 340, 300, 180});

    // Event Log demo - mirroring EventDemoState pattern but on right side
    // EventDemoState uses: BOTTOM_ALIGNED, offsetX=10, offsetY=20, width=730, height=180
    // We mirror this with BOTTOM_RIGHT for right-side positioning
    ui.createEventLog("uiexample_demo_event_log", {rightColumnX, ui.getHeightInPixels() - 200, rightColumnWidth, 180}, 6);
    ui.setComponentPositioning("uiexample_demo_event_log", {UIPositionMode::BOTTOM_RIGHT, 10, 20, 730, 180});

    ui.createLabel("uiexample_event_log_label", {rightColumnX, ui.getHeightInPixels() - 220, rightColumnWidth/2, 20}, "Event Log (Fixed Size):");
    ui.setComponentPositioning("uiexample_event_log_label", {UIPositionMode::BOTTOM_RIGHT, 10, 210, 730, 20});

    // Initialize event log with initial messages
    ui.addEventLogEntry("uiexample_demo_event_log", "Event log initialized");
    ui.addEventLogEntry("uiexample_demo_event_log", "Demo components created");

    // Right column components - use CENTERED_H for responsive center-based layout
    // CENTERED_H: bounds.x = (width - bounds.width) / 2 + offsetX
    // To get left edge at center+50: offsetX = 50 + bounds.width/2
    // To get left edge at center+200: offsetX = 200 + bounds.width/2

    // Animation button: left edge at center+50 → offsetX = 50 + 120/2 = 110
    ui.createButton("uiexample_animate_btn", {rightColumnX, 340, 120, 40}, "Animate");
    ui.setComponentPositioning("uiexample_animate_btn", {UIPositionMode::CENTERED_H, 110, 340, 120, 40});

    // Theme button: left edge at center+50 → offsetX = 50 + 150/2 = 125
    ui.createButton("uiexample_theme_btn", {rightColumnX, 390, 150, 40}, "Dark Theme");
    ui.setComponentPositioning("uiexample_theme_btn", {UIPositionMode::CENTERED_H, 125, 390, 150, 40});

    // Instructions: left edge at center+200 (150px right of buttons) → offsetX = 200 + 600/2 = 500
    ui.createLabel("uiexample_instructions", {rightColumnX + 150, 340, 600, 120},
                   "Controls:\n- Click buttons and UI elements\n- Type in input field\n- Select list items\n- B key to go back");
    ui.setComponentPositioning("uiexample_instructions", {UIPositionMode::CENTERED_H, 500, 340, 600, 120});

    // Populate list
    ui.addListItem("uiexample_demo_list", "Option 1: Basic Item");
    ui.addListItem("uiexample_demo_list", "Option 2: Second Item");
    ui.addListItem("uiexample_demo_list", "Option 3: Third Item");
    ui.addListItem("uiexample_demo_list", "Option 4: Fourth Item");
    ui.addListItem("uiexample_demo_list", "Option 5: Fifth Item");

    // Set up button callbacks - capture mp_stateManager for proper architecture
    ui.setOnClick("uiexample_back_btn", [this]() {
        mp_stateManager->changeState(GameStateId::MAIN_MENU);
    });

    ui.setOnClick("uiexample_animate_btn", [this]() {
        handleAnimation();
    });

    ui.setOnClick("uiexample_theme_btn", [this]() {
        handleThemeChange();
    });



    ui.setOnClick("uiexample_demo_checkbox", [this]() {
        handleCheckboxToggle();
    });

    ui.setOnClick("uiexample_demo_list", [this]() {
        handleListSelection();
    });

    ui.setOnValueChanged("uiexample_demo_slider", [this](float value) {
        handleSliderChange(value);
    });

    ui.setOnTextChanged("uiexample_demo_input", [this](const std::string& text) {
        handleInputChange(text);
    });

    return true;
}

void UIDemoState::update(float deltaTime) {
    // Store deltaTime for ui.update() in render()
    m_lastDeltaTime = deltaTime;

    // Update progress bar animation
    updateProgressBar(deltaTime);

    // Update event log demo with sample messages
    updateEventLogDemo(deltaTime);

    // Process UI input (click detection, hover states, callbacks)
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.update(deltaTime);
    }
}

bool UIDemoState::exit() {
    GAMESTATE_INFO("Exiting UI Example State");

    // Clean up UI components using simplified method
    UIManager::Instance().prepareForStateTransition();

    return true;
}

void UIDemoState::handleSliderChange(float value) {
    m_sliderValue = value;
    updateSliderLabel(value);
    GAMESTATE_DEBUG(std::format("Slider value changed: {}", value));
}

void UIDemoState::handleCheckboxToggle() {
    m_checkboxValue = !m_checkboxValue;
    GAMESTATE_DEBUG(std::format("Checkbox toggled: {}", m_checkboxValue ? "checked" : "unchecked"));
}

void UIDemoState::handleInputChange(const std::string& text) {
    m_inputText = text;
    updateInputLabel(text);
    GAMESTATE_DEBUG(std::format("Input text changed: {}", text));
}

void UIDemoState::handleListSelection() {
    m_selectedListItem = UIManager::Instance().getSelectedListItem("uiexample_demo_list");
    GAMESTATE_DEBUG(std::format("List item selected: {}", m_selectedListItem));
}

void UIDemoState::handleAnimation() {
    // Cache manager reference for better performance
    UIManager &ui = UIManager::Instance();

    // Animate the animation button
    UIRect currentBounds = ui.getBounds("uiexample_animate_btn");
    UIRect targetBounds = currentBounds;
    targetBounds.x += 50;

    ui.animateMove("uiexample_animate_btn", targetBounds, 0.5f, [currentBounds]() {
        // Animate back to original position (callback uses Instance() since it runs later)
        UIManager::Instance().animateMove("uiexample_animate_btn", currentBounds, 0.5f);
    });

    GAMESTATE_DEBUG("Animation triggered");
}

void UIDemoState::handleThemeChange() {
    m_darkTheme = !m_darkTheme;
    applyDarkTheme(m_darkTheme);
    GAMESTATE_DEBUG(std::format("Theme changed to: {}", m_darkTheme ? "dark" : "light"));
}

void UIDemoState::handleInput() {
    // Handle B key to go back
    const auto& inputManager = InputManager::Instance();
    if (inputManager.wasKeyPressed(SDL_SCANCODE_B)) {
        mp_stateManager->changeState(GameStateId::MAIN_MENU);
    }
}



void UIDemoState::updateProgressBar(float deltaTime) {
    // Animate progress bar automatically
    if (m_progressIncreasing) {
        m_progressValue += deltaTime * 0.3f; // 30% per second
        if (m_progressValue >= 1.0f) {
            m_progressValue = 1.0f;
            m_progressIncreasing = false;
        }
    } else {
        m_progressValue -= deltaTime * 0.3f;
        if (m_progressValue <= 0.0f) {
            m_progressValue = 0.0f;
            m_progressIncreasing = true;
        }
    }

    // UIManager now has built-in caching, so calling setValue() every frame is safe
    UIManager::Instance().setValue("uiexample_demo_progress", m_progressValue);
}



void UIDemoState::updateSliderLabel(float value) {
    UIManager::Instance().setText("uiexample_slider_label",
                                  std::format("Slider: {:.2f}", value));
}

void UIDemoState::updateInputLabel(const std::string& text) {
    UIManager::Instance().setText("uiexample_input_label",
                                  std::format("Input: {}", text.empty() ? "(empty)" : text));
}

void UIDemoState::applyDarkTheme(bool dark) {
    // Cache manager reference for better performance
    UIManager &ui = UIManager::Instance();

    if (dark) {
        // Use centralized dark theme
        ui.setThemeMode("dark");
        ui.setText("uiexample_theme_btn", "Light Theme");
    } else {
        // Use centralized light theme
        ui.setThemeMode("light");
        ui.setText("uiexample_theme_btn", "Dark Theme");
    }

    // Title styling is handled automatically by UIManager's TITLE component type
}

void UIDemoState::updateEventLogDemo(float deltaTime) {
    m_eventLogTimer += deltaTime;

    if (m_eventLogTimer >= EVENT_LOG_INTERVAL) {
        m_eventLogTimer = 0.0f;

        // Sample messages for demo (state-owned, not in UIManager)
        static constexpr std::array<const char*, 10> sampleMessages = {{
            "System initialized successfully", "User interface components loaded",
            "Database connection established", "Configuration files validated",
            "Network module started",          "Audio system ready",
            "Graphics renderer initialized",   "Input handlers registered",
            "Memory pools allocated",          "Security protocols activated"
        }};

        UIManager::Instance().addEventLogEntry(
            "uiexample_demo_event_log",
            sampleMessages[m_eventLogMessageIndex % sampleMessages.size()]);
        m_eventLogMessageIndex++;
    }
}

void UIDemoState::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                        float) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.recordGPUVertices(gpuRenderer);
    }
}

void UIDemoState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                                  SDL_GPURenderPass* swapchainPass) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.renderGPU(gpuRenderer, swapchainPass);
    }
}

// Pure UIManager implementation - no UIScreen needed
