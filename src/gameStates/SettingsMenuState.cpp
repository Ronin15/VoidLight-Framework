/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "gameStates/SettingsMenuState.hpp"
#include "managers/UIManager.hpp"
#include "managers/UIConstants.hpp"
#include "managers/InputManager.hpp"
#include "managers/FontManager.hpp"
#include "managers/SettingsManager.hpp"
#include "managers/GameStateManager.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "utils/MenuNavigation.hpp"
#include "utils/ResourcePath.hpp"

#include "gpu/GPURenderer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <thread>
#include <chrono>

bool SettingsMenuState::enter() {
    GAMESTATE_INFO("Entering SETTINGS MENU State");

    VoidLight::MenuNavigation::reset();

    m_pendingRefreshCommand = InputManager::Command::COUNT;
    m_bindingSnapshot = InputManager::Instance().captureBindings();

    // Pause all game managers to reduce power draw while in settings
    GameEngine::Instance().setGlobalPause(true);

    // Get manager references at function start
    auto& ui = UIManager::Instance();
    auto& fontMgr = FontManager::Instance();

    // Wait for fonts to load
    constexpr int kMaxWaitMs = 1500;
    int waitedMs = 0;
    while (!fontMgr.areFontsLoaded() && waitedMs < kMaxWaitMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        waitedMs += 1;
    }

    // Load current settings from SettingsManager
    loadCurrentSettings();

    // Create title
    ui.createTitleAtTop("settings_title", "Settings", 60);

    // Create tab buttons
    createTabButtons();

    // Create all settings UIs (visibility controlled by current tab)
    createGraphicsUI();
    createAudioUI();
    createGameplayUI();
    createControlsUI();

    // Create action buttons (Apply, Cancel, Back)
    createActionButtons();

    // Show Graphics tab by default — switchTab() rebuilds the focus ring.
    switchTab(SettingsTab::Graphics);

    return true;
}

void SettingsMenuState::update(float deltaTime) {
    auto& ui = UIManager::Instance();
    auto& inputMgr = InputManager::Instance();

    // Skip UIManager input processing while a rebind is in flight AND on the
    // frame it completes. Otherwise the mouse-button press that finalizes the
    // rebind capture is also seen by ui.update() as a click on the binding
    // button, which re-fires its onClick and starts a fresh rebind. Using
    // m_pendingRefreshCommand as the in-flight marker avoids adding a
    // dedicated flag — it's set on rebind start and cleared below.
    const bool rebindJustFinished =
        m_pendingRefreshCommand != InputManager::Command::COUNT &&
        !inputMgr.isRebinding();
    const bool suppressUI = inputMgr.isRebinding() || rebindJustFinished;

    if (!ui.isShutdown() && !suppressUI) {
        ui.update(deltaTime);
    }

    if (rebindJustFinished) {
        refreshBindingLabels(m_pendingRefreshCommand);
        m_pendingRefreshCommand = InputManager::Command::COUNT;
    }

    VoidLight::MenuNavigation::applySelection(m_navOrder, m_selectedIndex);
}

bool SettingsMenuState::exit() {
    GAMESTATE_INFO("Exiting SETTINGS MENU State");

    // NOTE: Do NOT unpause here - gameplay states will unpause on enter
    // This keeps systems paused during menu-to-menu transitions
    auto& inputMgr = InputManager::Instance();
    inputMgr.restoreBindings(m_bindingSnapshot);
    m_pendingRefreshCommand = InputManager::Command::COUNT;

    auto& ui = UIManager::Instance();
    ui.prepareForStateTransition();

    return true;
}

void SettingsMenuState::handleInput() {
    const auto& inputManager = InputManager::Instance();

    // Don't steal menu input while the user is capturing a rebind.
    if (!inputManager.isRebinding()) {
        VoidLight::MenuNavigation::readInputs(m_navOrder, m_selectedIndex);
        handleSliderAdjust();
        if (VoidLight::MenuNavigation::cancelPressed()) {
            mp_stateManager->changeState(GameStateId::MAIN_MENU);
        }
    }
}

namespace {
// Slider IDs are the only components that consume MenuLeft/MenuRight.
constexpr std::array<std::string_view, 3> kSliderIds{
    "settings_master_volume_slider",
    "settings_music_volume_slider",
    "settings_sfx_volume_slider",
};

constexpr std::array<InputManager::Command, 17> kControlsTabCommands{
    InputManager::Command::MoveUp,
    InputManager::Command::MoveDown,
    InputManager::Command::MoveLeft,
    InputManager::Command::MoveRight,
    InputManager::Command::AttackLight,
    InputManager::Command::Interact,
    InputManager::Command::OpenInventory,
    InputManager::Command::Pause,
    InputManager::Command::WorldInteract,
    InputManager::Command::ZoomIn,
    InputManager::Command::ZoomOut,
    InputManager::Command::MenuConfirm,
    InputManager::Command::MenuCancel,
    InputManager::Command::MenuUp,
    InputManager::Command::MenuDown,
    InputManager::Command::MenuLeft,
    InputManager::Command::MenuRight,
};

bool isSliderId(std::string_view id) {
    return std::any_of(kSliderIds.begin(), kSliderIds.end(),
                       [&](std::string_view s) { return s == id; });
}
} // namespace

void SettingsMenuState::handleSliderAdjust() {
    if (m_selectedIndex >= m_navOrder.size()) return;
    const std::string_view selected = m_navOrder[m_selectedIndex];
    if (!isSliderId(selected)) return;

    const bool left  = VoidLight::MenuNavigation::leftPressed();
    const bool right = VoidLight::MenuNavigation::rightPressed();
    if (!left && !right) return;

    auto& ui = UIManager::Instance();
    const std::string sid(selected);
    constexpr float kStep = 0.05f; // 5% of the [0..1] slider range per press
    if (left) {
        ui.setValue(sid, std::max(0.0f, ui.getValue(sid) - kStep));
    }
    if (right) {
        ui.setValue(sid, std::min(1.0f, ui.getValue(sid) + kStep));
    }
}

void SettingsMenuState::rebuildNavOrder() {
    // Populate m_navBacking first (owning dynamic strings), then build
    // m_navOrder as a view list. Reserving up-front prevents reallocation
    // so the string_views remain valid for the state's lifetime.
    using DC = InputManager::DeviceCategory;

    m_navBacking.clear();
    m_navOrder.clear();

    // INVARIANT: m_navBacking capacity must never be exceeded while m_navOrder
    // holds string_views into it — any reallocation invalidates every view.
    // Reserve unconditionally so future additions can't silently grow capacity
    // mid-fill.
    constexpr size_t kBackingCapacity = kControlsTabCommands.size() * 2;
    m_navBacking.reserve(kBackingCapacity);

    if (m_currentTab == SettingsTab::Controls) {
        for (InputManager::Command cmd : kControlsTabCommands) {
            m_navBacking.push_back(bindingButtonId(cmd, DC::KeyboardMouse));
            m_navBacking.push_back(bindingButtonId(cmd, DC::Controller));
        }
        assert(m_navBacking.size() == kBackingCapacity &&
               "m_navBacking overran its reserved capacity — string_views "
               "in m_navOrder may have been invalidated");
    }

    // Tab row is always reachable.
    m_navOrder.emplace_back("settings_tab_graphics");
    m_navOrder.emplace_back("settings_tab_audio");
    m_navOrder.emplace_back("settings_tab_gameplay");
    m_navOrder.emplace_back("settings_tab_controls");

    // Body of the current tab.
    switch (m_currentTab) {
    case SettingsTab::Graphics:
        m_navOrder.emplace_back("settings_vsync_checkbox");
        m_navOrder.emplace_back("settings_fullscreen_checkbox");
        m_navOrder.emplace_back("settings_showfps_checkbox");
        break;
    case SettingsTab::Audio:
        m_navOrder.emplace_back("settings_master_volume_slider");
        m_navOrder.emplace_back("settings_music_volume_slider");
        m_navOrder.emplace_back("settings_sfx_volume_slider");
        m_navOrder.emplace_back("settings_mute_checkbox");
        break;
    case SettingsTab::Gameplay:
        m_navOrder.emplace_back("settings_autosave_checkbox");
        break;
    case SettingsTab::Controls:
        m_navOrder.insert(m_navOrder.end(), m_navBacking.begin(), m_navBacking.end());
        m_navOrder.emplace_back("settings_ctrl_reset_btn");
        break;
    }

    m_navOrder.emplace_back("settings_apply_btn");
    m_navOrder.emplace_back("settings_back_btn");
}


void SettingsMenuState::loadCurrentSettings() {
    using namespace VoidLight;
    auto& settings = SettingsManager::Instance();

    // Graphics
    m_tempSettings.resolutionWidth = settings.get<int>("graphics", "resolution_width", 1920);
    m_tempSettings.resolutionHeight = settings.get<int>("graphics", "resolution_height", 1080);
    m_tempSettings.fullscreen = settings.get<bool>("graphics", "fullscreen", false);
    m_tempSettings.vsync = settings.get<bool>("graphics", "vsync", true);
    m_tempSettings.fpsLimit = settings.get<int>("graphics", "fps_limit", 60);
    m_tempSettings.showFps = settings.get<bool>("graphics", "show_fps", false);

    // Audio
    m_tempSettings.masterVolume = settings.get<float>("audio", "master_volume", 1.0f);
    m_tempSettings.musicVolume = settings.get<float>("audio", "music_volume", 0.7f);
    m_tempSettings.sfxVolume = settings.get<float>("audio", "sfx_volume", 0.8f);
    m_tempSettings.muted = settings.get<bool>("audio", "muted", false);

    // Gameplay
    m_tempSettings.difficulty = settings.get<std::string>("gameplay", "difficulty", "normal");
    m_tempSettings.autosaveEnabled = settings.get<bool>("gameplay", "autosave_enabled", true);
    m_tempSettings.autosaveInterval = settings.get<int>("gameplay", "autosave_interval", 300);
}

void SettingsMenuState::applySettings() {
    using namespace VoidLight;
    auto& settings = SettingsManager::Instance();
    auto& gameEngine = GameEngine::Instance();

    // Graphics
    settings.set("graphics", "resolution_width", m_tempSettings.resolutionWidth);
    settings.set("graphics", "resolution_height", m_tempSettings.resolutionHeight);
    settings.set("graphics", "fullscreen", m_tempSettings.fullscreen);
    settings.set("graphics", "fps_limit", m_tempSettings.fpsLimit);
    settings.set("graphics", "show_fps", m_tempSettings.showFps);

    // Apply fullscreen setting immediately
    // SDL will automatically fire SDL_EVENT_WINDOW_RESIZED which triggers
    // InputManager::onWindowResize() → UIManager::onWindowResize()
    // This ensures clean, single-path UI repositioning
    if (gameEngine.isFullscreen() != m_tempSettings.fullscreen) {
        gameEngine.setFullscreen(m_tempSettings.fullscreen);
        GAMESTATE_INFO("Fullscreen setting applied - UI will update via SDL resize event");
    }

    // Audio
    settings.set("audio", "master_volume", m_tempSettings.masterVolume);
    settings.set("audio", "music_volume", m_tempSettings.musicVolume);
    settings.set("audio", "sfx_volume", m_tempSettings.sfxVolume);
    settings.set("audio", "muted", m_tempSettings.muted);

    // Gameplay
    settings.set("gameplay", "difficulty", m_tempSettings.difficulty);
    settings.set("gameplay", "autosave_enabled", m_tempSettings.autosaveEnabled);
    settings.set("gameplay", "autosave_interval", m_tempSettings.autosaveInterval);

    // Apply VSync (pure mode switch — writes graphics.vsync to in-memory settings).
    gameEngine.setVSyncEnabled(m_tempSettings.vsync);

    // Single explicit persist for the whole settings file.
    settings.saveToFile(VoidLight::ResourcePath::resolve("res/settings.json"));

    // Save input bindings alongside other settings; log on failure but don't abort apply
    if (!InputManager::Instance().saveBindingsToFile(
            VoidLight::ResourcePath::resolve("res/input_bindings.json"))) {
        GAMESTATE_WARN("Failed to save input bindings to disk");
    }
    m_bindingSnapshot = InputManager::Instance().captureBindings();

    GAMESTATE_INFO("Settings saved successfully");
}


void SettingsMenuState::createTabButtons() {
    auto& ui = UIManager::Instance();

    // Four tabs: Graphics, Audio, Gameplay, Controls — placed symmetrically around center.
    // With 4 tabs: offsets are -3/2, -1/2, +1/2, +3/2 of (tabWidth + tabSpacing).
    constexpr int tabWidth = 180;
    constexpr int tabHeight = 40;
    constexpr int tabSpacing = 10;
    constexpr int tabY = 80;
    const int step = tabWidth + tabSpacing;
    // Centre of 4 tabs: [-3/2 * step, -1/2 * step, +1/2 * step, +3/2 * step]
    const int offset0 = -(3 * step / 2);
    const int offset1 = -(step / 2);
    const int offset2 =  (step / 2);
    const int offset3 =  (3 * step / 2);

    ui.createButton("settings_tab_graphics",
        {ui.getWidthInPixels() / 2 + offset0, tabY, tabWidth, tabHeight}, "Graphics (1)");
    ui.setComponentPositioning("settings_tab_graphics",
        {UIPositionMode::CENTERED_H, offset0, tabY, tabWidth, tabHeight});

    ui.createButton("settings_tab_audio",
        {ui.getWidthInPixels() / 2 + offset1, tabY, tabWidth, tabHeight}, "Audio (2)");
    ui.setComponentPositioning("settings_tab_audio",
        {UIPositionMode::CENTERED_H, offset1, tabY, tabWidth, tabHeight});

    ui.createButton("settings_tab_gameplay",
        {ui.getWidthInPixels() / 2 + offset2, tabY, tabWidth, tabHeight}, "Gameplay (3)");
    ui.setComponentPositioning("settings_tab_gameplay",
        {UIPositionMode::CENTERED_H, offset2, tabY, tabWidth, tabHeight});

    ui.createButton("settings_tab_controls",
        {ui.getWidthInPixels() / 2 + offset3, tabY, tabWidth, tabHeight}, "Controls (4)");
    ui.setComponentPositioning("settings_tab_controls",
        {UIPositionMode::CENTERED_H, offset3, tabY, tabWidth, tabHeight});

    ui.setOnClick("settings_tab_graphics", [this]() {
        switchTab(SettingsTab::Graphics);
    });
    ui.setOnClick("settings_tab_audio", [this]() {
        switchTab(SettingsTab::Audio);
    });
    ui.setOnClick("settings_tab_gameplay", [this]() {
        switchTab(SettingsTab::Gameplay);
    });
    ui.setOnClick("settings_tab_controls", [this]() {
        switchTab(SettingsTab::Controls);
    });
}

void SettingsMenuState::createGraphicsUI() {
    auto& ui = UIManager::Instance();

    int leftColumnX = 200;
    int startY = UIConstants::CONTENT_START_Y_AFTER_TABS;
    int rowHeight = UIConstants::FORM_ROW_HEIGHT;
    int labelWidth = UIConstants::FORM_LABEL_WIDTH;
    int controlWidth = UIConstants::FORM_CONTROL_WIDTH;
    int controlX = leftColumnX + labelWidth + UIConstants::FORM_LABEL_CONTROL_GAP;

    // VSync checkbox
    ui.createLabel("settings_vsync_label", {leftColumnX, startY, labelWidth, 40}, "VSync:");
    ui.setComponentPositioning("settings_vsync_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY, labelWidth, 40});
    ui.createCheckbox("settings_vsync_checkbox", {controlX, startY, 30, 30}, "Enabled");
    ui.setComponentPositioning("settings_vsync_checkbox", {UIPositionMode::TOP_ALIGNED, controlX, startY, 30, 30});
    ui.setChecked("settings_vsync_checkbox", m_tempSettings.vsync);
    ui.setOnClick("settings_vsync_checkbox", [this]() {
        m_tempSettings.vsync = UIManager::Instance().getChecked("settings_vsync_checkbox");
    });

    // Fullscreen checkbox
    ui.createLabel("settings_fullscreen_label", {leftColumnX, startY + rowHeight, labelWidth, 40}, "Fullscreen:");
    ui.setComponentPositioning("settings_fullscreen_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + rowHeight, labelWidth, 40});
    ui.createCheckbox("settings_fullscreen_checkbox", {controlX, startY + rowHeight, 30, 30}, "Enabled");
    ui.setComponentPositioning("settings_fullscreen_checkbox", {UIPositionMode::TOP_ALIGNED, controlX, startY + rowHeight, 30, 30});
    ui.setChecked("settings_fullscreen_checkbox", m_tempSettings.fullscreen);
    ui.setOnClick("settings_fullscreen_checkbox", [this]() {
        m_tempSettings.fullscreen = UIManager::Instance().getChecked("settings_fullscreen_checkbox");
    });

    // Show FPS checkbox
    ui.createLabel("settings_showfps_label", {leftColumnX, startY + 2 * rowHeight, labelWidth, 40}, "Show FPS:");
    ui.setComponentPositioning("settings_showfps_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + 2 * rowHeight, labelWidth, 40});
    ui.createCheckbox("settings_showfps_checkbox", {controlX, startY + 2 * rowHeight, 30, 30}, "Enabled");
    ui.setComponentPositioning("settings_showfps_checkbox", {UIPositionMode::TOP_ALIGNED, controlX, startY + 2 * rowHeight, 30, 30});
    ui.setChecked("settings_showfps_checkbox", m_tempSettings.showFps);
    ui.setOnClick("settings_showfps_checkbox", [this]() {
        m_tempSettings.showFps = UIManager::Instance().getChecked("settings_showfps_checkbox");
    });

    // Resolution label (informational)
    ui.createLabel("settings_resolution_label", {leftColumnX, startY + 3 * rowHeight, labelWidth + controlWidth, 40},
                   std::format("Resolution: {}x{}", m_tempSettings.resolutionWidth, m_tempSettings.resolutionHeight));
    ui.setComponentPositioning("settings_resolution_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + 3 * rowHeight, labelWidth + controlWidth, 40});
}

void SettingsMenuState::createAudioUI() {
    auto& ui = UIManager::Instance();

    int leftColumnX = 200;
    int startY = UIConstants::CONTENT_START_Y_AFTER_TABS;
    int rowHeight = UIConstants::FORM_ROW_HEIGHT;
    int labelWidth = UIConstants::FORM_LABEL_WIDTH;
    int sliderWidth = UIConstants::FORM_CONTROL_WIDTH;
    int sliderX = leftColumnX + labelWidth + UIConstants::FORM_LABEL_CONTROL_GAP;

    // Master Volume slider
    ui.createLabel("settings_master_volume_label", {leftColumnX, startY, labelWidth, 40}, "Master Volume:");
    ui.setComponentPositioning("settings_master_volume_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY, labelWidth, 40});
    ui.createSlider("settings_master_volume_slider", {sliderX, startY, sliderWidth, UIConstants::DEFAULT_SLIDER_HEIGHT}, 0.0f, 1.0f);
    ui.setComponentPositioning("settings_master_volume_slider", {UIPositionMode::TOP_ALIGNED, sliderX, startY, sliderWidth, UIConstants::DEFAULT_SLIDER_HEIGHT});
    ui.setValue("settings_master_volume_slider", m_tempSettings.masterVolume);
    ui.setOnValueChanged("settings_master_volume_slider", [this](float value) {
        m_tempSettings.masterVolume = value;
        UIManager::Instance().setText("settings_master_volume_value", std::format("{}%", static_cast<int>(value * 100)));
    });
    ui.createLabel("settings_master_volume_value", {sliderX + sliderWidth + 10, startY, 80, 40},
                   std::format("{}%", static_cast<int>(m_tempSettings.masterVolume * 100)));
    ui.setComponentPositioning("settings_master_volume_value", {UIPositionMode::TOP_ALIGNED, sliderX + sliderWidth + 10, startY, 80, 40});
    // Fixed 80px width fits any "nnn%" — skip per-setText font metrics during slider drag.
    ui.enableAutoSizing("settings_master_volume_value", false);

    // Music Volume slider
    ui.createLabel("settings_music_volume_label", {leftColumnX, startY + rowHeight, labelWidth, 40}, "Music Volume:");
    ui.setComponentPositioning("settings_music_volume_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + rowHeight, labelWidth, 40});
    ui.createSlider("settings_music_volume_slider", {sliderX, startY + rowHeight, sliderWidth, UIConstants::DEFAULT_SLIDER_HEIGHT}, 0.0f, 1.0f);
    ui.setComponentPositioning("settings_music_volume_slider", {UIPositionMode::TOP_ALIGNED, sliderX, startY + rowHeight, sliderWidth, UIConstants::DEFAULT_SLIDER_HEIGHT});
    ui.setValue("settings_music_volume_slider", m_tempSettings.musicVolume);
    ui.setOnValueChanged("settings_music_volume_slider", [this](float value) {
        m_tempSettings.musicVolume = value;
        UIManager::Instance().setText("settings_music_volume_value", std::format("{}%", static_cast<int>(value * 100)));
    });
    ui.createLabel("settings_music_volume_value", {sliderX + sliderWidth + 10, startY + rowHeight, 80, 40},
                   std::format("{}%", static_cast<int>(m_tempSettings.musicVolume * 100)));
    ui.setComponentPositioning("settings_music_volume_value", {UIPositionMode::TOP_ALIGNED, sliderX + sliderWidth + 10, startY + rowHeight, 80, 40});
    ui.enableAutoSizing("settings_music_volume_value", false);

    // SFX Volume slider
    ui.createLabel("settings_sfx_volume_label", {leftColumnX, startY + 2 * rowHeight, labelWidth, 40}, "SFX Volume:");
    ui.setComponentPositioning("settings_sfx_volume_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + 2 * rowHeight, labelWidth, 40});
    ui.createSlider("settings_sfx_volume_slider", {sliderX, startY + 2 * rowHeight, sliderWidth, UIConstants::DEFAULT_SLIDER_HEIGHT}, 0.0f, 1.0f);
    ui.setComponentPositioning("settings_sfx_volume_slider", {UIPositionMode::TOP_ALIGNED, sliderX, startY + 2 * rowHeight, sliderWidth, UIConstants::DEFAULT_SLIDER_HEIGHT});
    ui.setValue("settings_sfx_volume_slider", m_tempSettings.sfxVolume);
    ui.setOnValueChanged("settings_sfx_volume_slider", [this](float value) {
        m_tempSettings.sfxVolume = value;
        UIManager::Instance().setText("settings_sfx_volume_value", std::format("{}%", static_cast<int>(value * 100)));
    });
    ui.createLabel("settings_sfx_volume_value", {sliderX + sliderWidth + 10, startY + 2 * rowHeight, 80, 40},
                   std::format("{}%", static_cast<int>(m_tempSettings.sfxVolume * 100)));
    ui.setComponentPositioning("settings_sfx_volume_value", {UIPositionMode::TOP_ALIGNED, sliderX + sliderWidth + 10, startY + 2 * rowHeight, 80, 40});
    ui.enableAutoSizing("settings_sfx_volume_value", false);

    // Mute checkbox
    ui.createLabel("settings_mute_label", {leftColumnX, startY + 3 * rowHeight, labelWidth, 40}, "Mute All:");
    ui.setComponentPositioning("settings_mute_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + 3 * rowHeight, labelWidth, 40});
    ui.createCheckbox("settings_mute_checkbox", {sliderX, startY + 3 * rowHeight, 30, 30}, "Muted");
    ui.setComponentPositioning("settings_mute_checkbox", {UIPositionMode::TOP_ALIGNED, sliderX, startY + 3 * rowHeight, 30, 30});
    ui.setChecked("settings_mute_checkbox", m_tempSettings.muted);
    ui.setOnClick("settings_mute_checkbox", [this]() {
        m_tempSettings.muted = UIManager::Instance().getChecked("settings_mute_checkbox");
    });

    // Hide audio UI by default
    ui.setComponentVisible("settings_master_volume_label", false);
    ui.setComponentVisible("settings_master_volume_slider", false);
    ui.setComponentVisible("settings_master_volume_value", false);
    ui.setComponentVisible("settings_music_volume_label", false);
    ui.setComponentVisible("settings_music_volume_slider", false);
    ui.setComponentVisible("settings_music_volume_value", false);
    ui.setComponentVisible("settings_sfx_volume_label", false);
    ui.setComponentVisible("settings_sfx_volume_slider", false);
    ui.setComponentVisible("settings_sfx_volume_value", false);
    ui.setComponentVisible("settings_mute_label", false);
    ui.setComponentVisible("settings_mute_checkbox", false);
}

void SettingsMenuState::createGameplayUI() {
    auto& ui = UIManager::Instance();

    int leftColumnX = 200;
    int startY = UIConstants::CONTENT_START_Y_AFTER_TABS;
    int rowHeight = UIConstants::FORM_ROW_HEIGHT;
    int labelWidth = UIConstants::FORM_LABEL_WIDTH;
    int controlX = leftColumnX + labelWidth + UIConstants::FORM_LABEL_CONTROL_GAP;

    // Difficulty label
    ui.createLabel("settings_difficulty_label", {leftColumnX, startY, labelWidth + 200, 40},
                   std::format("Difficulty: {}", m_tempSettings.difficulty));
    ui.setComponentPositioning("settings_difficulty_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY, labelWidth + 200, 40});

    // Autosave checkbox
    ui.createLabel("settings_autosave_label", {leftColumnX, startY + rowHeight, labelWidth, 40}, "Autosave:");
    ui.setComponentPositioning("settings_autosave_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + rowHeight, labelWidth, 40});
    ui.createCheckbox("settings_autosave_checkbox", {controlX, startY + rowHeight, 30, 30}, "Enabled");
    ui.setComponentPositioning("settings_autosave_checkbox", {UIPositionMode::TOP_ALIGNED, controlX, startY + rowHeight, 30, 30});
    ui.setChecked("settings_autosave_checkbox", m_tempSettings.autosaveEnabled);
    ui.setOnClick("settings_autosave_checkbox", [this]() {
        m_tempSettings.autosaveEnabled = UIManager::Instance().getChecked("settings_autosave_checkbox");
    });

    // Autosave interval label
    ui.createLabel("settings_autosave_interval_label", {leftColumnX, startY + 2 * rowHeight, labelWidth + 200, 40},
                   std::format("Autosave Interval: {} seconds", m_tempSettings.autosaveInterval));
    ui.setComponentPositioning("settings_autosave_interval_label", {UIPositionMode::TOP_ALIGNED, leftColumnX, startY + 2 * rowHeight, labelWidth + 200, 40});

    // Hide gameplay UI by default
    ui.setComponentVisible("settings_difficulty_label", false);
    ui.setComponentVisible("settings_autosave_label", false);
    ui.setComponentVisible("settings_autosave_checkbox", false);
    ui.setComponentVisible("settings_autosave_interval_label", false);
}

void SettingsMenuState::createActionButtons() {
    auto& ui = UIManager::Instance();

    int buttonWidth = 150;
    int buttonHeight = UIConstants::DEFAULT_BUTTON_HEIGHT;
    int buttonSpacing = 20;
    int bottomOffset = UIConstants::BOTTOM_BUTTON_MARGIN;
    int centerX = ui.getWidthInPixels() / 2;
    int bottomY = ui.getHeightInPixels() - bottomOffset;

    // Apply button (Success green) - left of center, bottom centered
    int applyX = centerX - buttonWidth - buttonSpacing/2;
    ui.createButtonSuccess("settings_apply_btn",
        {applyX, bottomY, buttonWidth, buttonHeight},
        "Apply");
    ui.setComponentPositioning("settings_apply_btn", {UIPositionMode::BOTTOM_CENTERED, -(buttonWidth/2 + buttonSpacing/2), bottomOffset, buttonWidth, buttonHeight});
    ui.setOnClick("settings_apply_btn", [this]() {
        applySettings();
    });

    // Back button (goes back without saving) - right of center, bottom centered
    int backX = centerX + buttonSpacing/2;
    ui.createButtonDanger("settings_back_btn",
        {backX, bottomY, buttonWidth, buttonHeight},
        "Back");
    ui.setComponentPositioning("settings_back_btn", {UIPositionMode::BOTTOM_CENTERED, buttonWidth/2 + buttonSpacing/2, bottomOffset, buttonWidth, buttonHeight});
    ui.setOnClick("settings_back_btn", [this]() {
        mp_stateManager->changeState(GameStateId::MAIN_MENU);
    });
}

void SettingsMenuState::switchTab(SettingsTab tab) {
    m_currentTab = tab;
    updateTabVisibility();

    // Update tab button colors to show active tab
    auto& ui = UIManager::Instance();

    // Reset all tabs to normal
    ui.applyThemeToComponent("settings_tab_graphics", UIComponentType::BUTTON);
    ui.applyThemeToComponent("settings_tab_audio", UIComponentType::BUTTON);
    ui.applyThemeToComponent("settings_tab_gameplay", UIComponentType::BUTTON);
    ui.applyThemeToComponent("settings_tab_controls", UIComponentType::BUTTON);

    // Highlight active tab
    switch (m_currentTab) {
        case SettingsTab::Graphics:
            ui.applyThemeToComponent("settings_tab_graphics", UIComponentType::BUTTON_SUCCESS);
            break;
        case SettingsTab::Audio:
            ui.applyThemeToComponent("settings_tab_audio", UIComponentType::BUTTON_SUCCESS);
            break;
        case SettingsTab::Gameplay:
            ui.applyThemeToComponent("settings_tab_gameplay", UIComponentType::BUTTON_SUCCESS);
            break;
        case SettingsTab::Controls:
            ui.applyThemeToComponent("settings_tab_controls", UIComponentType::BUTTON_SUCCESS);
            break;
    }

    // Rebuild the keyboard focus ring for the new tab and park the selection
    // on the tab button the user just activated so they can step into the body.
    rebuildNavOrder();
    m_selectedIndex = static_cast<size_t>(tab); // tabs are the first four entries
    VoidLight::MenuNavigation::applySelection(m_navOrder, m_selectedIndex);
}

void SettingsMenuState::updateTabVisibility() {
    auto& ui = UIManager::Instance();

    // Helper to set visibility on all controls-tab components
    auto setControlsTabVisible = [&](bool visible) {
        using DC = InputManager::DeviceCategory;
        for (size_t i = 0; i < kControlsTabCommands.size(); ++i) {
            const InputManager::Command cmd = kControlsTabCommands[i];
            ui.setComponentVisible(std::format("settings_ctrl_label_{}", i), visible);
            ui.setComponentVisible(bindingButtonId(cmd, DC::KeyboardMouse), visible);
            ui.setComponentVisible(bindingButtonId(cmd, DC::Controller), visible);
        }
        ui.setComponentVisible("settings_ctrl_header_kbd", visible);
        ui.setComponentVisible("settings_ctrl_header_ctrl", visible);
        ui.setComponentVisible("settings_ctrl_reset_btn", visible);
    };

    // Hide all tab content first
    // Graphics
    ui.setComponentVisible("settings_vsync_label", false);
    ui.setComponentVisible("settings_vsync_checkbox", false);
    ui.setComponentVisible("settings_fullscreen_label", false);
    ui.setComponentVisible("settings_fullscreen_checkbox", false);
    ui.setComponentVisible("settings_showfps_label", false);
    ui.setComponentVisible("settings_showfps_checkbox", false);
    ui.setComponentVisible("settings_resolution_label", false);

    // Audio
    ui.setComponentVisible("settings_master_volume_label", false);
    ui.setComponentVisible("settings_master_volume_slider", false);
    ui.setComponentVisible("settings_master_volume_value", false);
    ui.setComponentVisible("settings_music_volume_label", false);
    ui.setComponentVisible("settings_music_volume_slider", false);
    ui.setComponentVisible("settings_music_volume_value", false);
    ui.setComponentVisible("settings_sfx_volume_label", false);
    ui.setComponentVisible("settings_sfx_volume_slider", false);
    ui.setComponentVisible("settings_sfx_volume_value", false);
    ui.setComponentVisible("settings_mute_label", false);
    ui.setComponentVisible("settings_mute_checkbox", false);

    // Gameplay
    ui.setComponentVisible("settings_difficulty_label", false);
    ui.setComponentVisible("settings_autosave_label", false);
    ui.setComponentVisible("settings_autosave_checkbox", false);
    ui.setComponentVisible("settings_autosave_interval_label", false);

    // Controls
    setControlsTabVisible(false);

    // Show only active tab content
    switch (m_currentTab) {
        case SettingsTab::Graphics:
            ui.setComponentVisible("settings_vsync_label", true);
            ui.setComponentVisible("settings_vsync_checkbox", true);
            ui.setComponentVisible("settings_fullscreen_label", true);
            ui.setComponentVisible("settings_fullscreen_checkbox", true);
            ui.setComponentVisible("settings_showfps_label", true);
            ui.setComponentVisible("settings_showfps_checkbox", true);
            ui.setComponentVisible("settings_resolution_label", true);
            break;

        case SettingsTab::Audio:
            ui.setComponentVisible("settings_master_volume_label", true);
            ui.setComponentVisible("settings_master_volume_slider", true);
            ui.setComponentVisible("settings_master_volume_value", true);
            ui.setComponentVisible("settings_music_volume_label", true);
            ui.setComponentVisible("settings_music_volume_slider", true);
            ui.setComponentVisible("settings_music_volume_value", true);
            ui.setComponentVisible("settings_sfx_volume_label", true);
            ui.setComponentVisible("settings_sfx_volume_slider", true);
            ui.setComponentVisible("settings_sfx_volume_value", true);
            ui.setComponentVisible("settings_mute_label", true);
            ui.setComponentVisible("settings_mute_checkbox", true);
            break;

        case SettingsTab::Gameplay:
            ui.setComponentVisible("settings_difficulty_label", true);
            ui.setComponentVisible("settings_autosave_label", true);
            ui.setComponentVisible("settings_autosave_checkbox", true);
            ui.setComponentVisible("settings_autosave_interval_label", true);
            break;

        case SettingsTab::Controls:
            setControlsTabVisible(true);
            break;
    }
}

// static helper — stable component ID for a binding slot button
std::string SettingsMenuState::bindingButtonId(InputManager::Command c,
                                               InputManager::DeviceCategory cat)
{
    using C = InputManager::Command;
    using DC = InputManager::DeviceCategory;
    struct Entry { C cmd; const char* key; };
    static constexpr Entry kTable[] = {
        {C::MoveUp,        "move_up"},
        {C::MoveDown,      "move_down"},
        {C::MoveLeft,      "move_left"},
        {C::MoveRight,     "move_right"},
        {C::AttackLight,   "attack_light"},
        {C::Interact,      "interact"},
        {C::OpenInventory, "open_inventory"},
        {C::Pause,         "pause"},
        {C::WorldInteract, "world_interact"},
        {C::ZoomIn,        "zoom_in"},
        {C::ZoomOut,       "zoom_out"},
        {C::MenuConfirm,   "menu_confirm"},
        {C::MenuCancel,    "menu_cancel"},
        {C::MenuUp,        "menu_up"},
        {C::MenuDown,      "menu_down"},
        {C::MenuLeft,      "menu_left"},
        {C::MenuRight,     "menu_right"},
    };
    const char* catSuffix = (cat == DC::KeyboardMouse) ? "kbd" : "ctrl";
    for (const auto& e : kTable) {
        if (e.cmd == c) {
            return std::format("settings_ctrl_{}_{}", e.key, catSuffix);
        }
    }
    return std::format("settings_ctrl_unknown_{}", catSuffix);
}

void SettingsMenuState::refreshBindingLabels(InputManager::Command c)
{
    using DC = InputManager::DeviceCategory;
    auto& ui = UIManager::Instance();
    auto& inputMgr = InputManager::Instance();

    for (DC cat : {DC::KeyboardMouse, DC::Controller}) {
        const std::string btnId = bindingButtonId(c, cat);
        auto binding = inputMgr.getBindingForCategory(c, cat);
        ui.setText(btnId, binding ? inputMgr.describeBinding(*binding) : "(unbound)");
    }
}

void SettingsMenuState::createControlsUI()
{
    using DC = InputManager::DeviceCategory;
    auto& ui = UIManager::Instance();
    auto& inputMgr = InputManager::Instance();

    // Two device-typed columns: Keyboard & Mouse and Controller. Column
    // width fits the longest dual-annotated label ("Start / Options",
    // "D-Pad Right"); UIManager auto-sizing grows the button if a user
    // rebinds to something longer.
    constexpr int leftX = 100;
    constexpr int labelW = 200;
    constexpr int colW = 260;
    constexpr int colGap = 24;
    constexpr int btnH = 36;
    // rowH tuned to fit all 17 rebindable commands above the Apply/Back row.
    // 17 * 42 + header + reset button leaves ~80px before the bottom action bar.
    constexpr int rowH = 42;
    constexpr int startY = 155;
    constexpr int headerY = startY - 36;

    const int colKbdX = leftX + labelW;
    const int colCtrlX = leftX + labelW + colW + colGap;

    // Column headers
    ui.createLabel("settings_ctrl_header_kbd",
        {colKbdX, headerY, colW, btnH}, "Keyboard & Mouse");
    ui.setComponentPositioning("settings_ctrl_header_kbd",
        {UIPositionMode::TOP_ALIGNED, colKbdX, headerY, colW, btnH});

    ui.createLabel("settings_ctrl_header_ctrl",
        {colCtrlX, headerY, colW, btnH}, "Controller");
    ui.setComponentPositioning("settings_ctrl_header_ctrl",
        {UIPositionMode::TOP_ALIGNED, colCtrlX, headerY, colW, btnH});

    for (size_t i = 0; i < kControlsTabCommands.size(); ++i) {
        const InputManager::Command cmd = kControlsTabCommands[i];
        const int y = startY + static_cast<int>(i) * rowH;

        // Row label
        const std::string labelId = std::format("settings_ctrl_label_{}", i);
        ui.createLabel(labelId, {leftX, y, labelW, btnH}, inputMgr.commandDisplayName(cmd));
        ui.setComponentPositioning(labelId, {UIPositionMode::TOP_ALIGNED, leftX, y, labelW, btnH});

        // One binding button per category, in column order
        struct ColumnSpec { DC cat; int x; };
        const ColumnSpec cols[] = {
            {DC::KeyboardMouse, colKbdX},
            {DC::Controller,    colCtrlX},
        };
        for (const auto& col : cols) {
            const std::string btnId = bindingButtonId(cmd, col.cat);
            auto binding = inputMgr.getBindingForCategory(cmd, col.cat);
            const std::string label = binding ? inputMgr.describeBinding(*binding) : "(unbound)";

            ui.createButton(btnId, {col.x, y, colW, btnH}, label);
            ui.setComponentPositioning(btnId, {UIPositionMode::TOP_ALIGNED, col.x, y, colW, btnH});

            const DC cat = col.cat;
            ui.setOnClick(btnId, [this, cmd, cat, btnId]() {
                InputManager::Instance().startRebinding(cmd, cat);
                UIManager::Instance().setText(btnId, "Press any key...");
                m_pendingRefreshCommand = cmd;
            });
        }

        ui.setComponentVisible(labelId, false);
        ui.setComponentVisible(bindingButtonId(cmd, DC::KeyboardMouse), false);
        ui.setComponentVisible(bindingButtonId(cmd, DC::Controller), false);
    }

    ui.setComponentVisible("settings_ctrl_header_kbd", false);
    ui.setComponentVisible("settings_ctrl_header_ctrl", false);

    // Reset button
    const int resetY = startY + static_cast<int>(kControlsTabCommands.size()) * rowH + 10;
    ui.createButton("settings_ctrl_reset_btn",
        {leftX, resetY, 300, btnH}, "Reset Controls to Defaults");
    ui.setComponentPositioning("settings_ctrl_reset_btn",
        {UIPositionMode::TOP_ALIGNED, leftX, resetY, 300, btnH});
    ui.setOnClick("settings_ctrl_reset_btn", [this]() {
        InputManager::Instance().resetBindingsToDefaults();
        for (InputManager::Command cmd : kControlsTabCommands) {
            refreshBindingLabels(cmd);
        }
    });
    ui.setComponentVisible("settings_ctrl_reset_btn", false);
}

void SettingsMenuState::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                                           float) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.recordGPUVertices(gpuRenderer);
    }
}

void SettingsMenuState::renderGPUUI(VoidLight::GPURenderer& gpuRenderer,
                                     SDL_GPURenderPass* swapchainPass) {
    auto& ui = UIManager::Instance();
    if (!ui.isShutdown()) {
        ui.renderGPU(gpuRenderer, swapchainPass);
    }
}
