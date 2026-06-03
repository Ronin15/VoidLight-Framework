/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "managers/InputManager.hpp"
#include "core/Logger.hpp"
#include "core/GameEngine.hpp"
#include "utils/JsonReader.hpp"
#include "SDL3/SDL_gamepad.h"
#include "SDL3/SDL_joystick.h"
#include "utils/Vector2D.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <span>

// Removed global pointer - now managed as member variable

InputManager::InputManager()
    : m_keystates(nullptr) {
  // Reserve capacity for performance optimization
  m_pressedThisFrame.reserve(16);  // Typical max keys pressed per frame
  m_gamepads.reserve(4);           // Max 4 gamepads typically
  m_mouseButtonStates.reserve(3);  // 3 mouse buttons

  // Create button states for the mouse
  for (int i = 0; i < 3; i++) {
    m_mouseButtonStates.push_back(false);
  }
}

bool InputManager::init() {
  if (m_isInitialized && !m_isShutdown) {
    INPUT_WARN("InputManager already initialized");
    return true;
  }

  INPUT_INFO("Initializing InputManager");

  // Get initial keyboard state from SDL (may be null on devices without keyboard)
  m_keystates = SDL_GetKeyboardState(nullptr);
  if (!m_keystates) {
    INPUT_WARN("No keyboard state available - device may not have keyboard input");
  }

  if (m_mouseButtonStates.empty()) {
    m_mouseButtonStates.assign(3, false);
  } else {
    reset();
  }

  m_pressedThisFrame.clear();
  m_currentDown.fill(false);
  m_previousDown.fill(false);
  m_prevMouseButtonStates.fill(false);
  m_rebindCommand = Command::COUNT;

  loadDefaultBindings();

  m_isInitialized = true;
  m_isShutdown = false;
  INPUT_INFO("InputManager initialized successfully");
  return true;
}

void InputManager::initializeGamePad() {
  // Check if gamepad subsystem is already initialized
  if (m_gamePadInitialized) {
    return;
  }

  // Gamepad subsystem is initialized by GameEngine::init() with SDL_INIT_GAMEPAD
  // Just detect and open available gamepads here

  // Get all available gamepads with RAII management
  int numGamepads = 0;
  auto gamepadIDs = std::unique_ptr<SDL_JoystickID[], decltype(&SDL_free)>(
      SDL_GetGamepads(&numGamepads), SDL_free);

  if (!gamepadIDs) {
    INPUT_ERROR(std::format("Failed to get gamepad IDs: {}", SDL_GetError()));
    return;
  }

  if (numGamepads > 0) {
    INPUT_INFO(std::format("Number of Game Pads detected: {}", numGamepads));
    bool openedAnyGamepad = false;
    for (int i = 0; i < numGamepads; i++) {
      if (openGamepad(gamepadIDs[i])) {
        openedAnyGamepad = true;
      }
    }

    m_gamePadInitialized = openedAnyGamepad;
  } else {
    INPUT_INFO("No gamepads found");
    // Subsystem stays initialized - SDL_Quit() will clean up all subsystems
    return;
  }

}

void InputManager::reset() {
  m_mouseButtonStates[LEFT] = false;
  m_mouseButtonStates[RIGHT] = false;
  m_mouseButtonStates[MIDDLE] = false;
}

bool InputManager::isKeyDown(SDL_Scancode key) const {
  if (m_keystates != nullptr) {
    return m_keystates[key] == 1;
  }
  return false;
}

float InputManager::getAxisX(int joy, int stick) const {
  if (joy < 0 || joy >= static_cast<int>(m_gamepads.size())) {
    return 0;
  }

  if (stick == 1) {
    return m_gamepads[joy].leftStick.getX();
  } else if (stick == 2) {
    return m_gamepads[joy].rightStick.getX();
  }

  return 0;
}

float InputManager::getAxisY(int joy, int stick) const {
  if (joy < 0 || joy >= static_cast<int>(m_gamepads.size())) {
    return 0;
  }

  if (stick == 1) {
    return m_gamepads[joy].leftStick.getY();
  } else if (stick == 2) {
    return m_gamepads[joy].rightStick.getY();
  }

  return 0;
}

float InputManager::getGamepadAxisValue(int joy, SDL_GamepadAxis axis) const {
  if (joy < 0 || joy >= static_cast<int>(m_gamepads.size())) {
    return 0.0f;
  }

  const GamepadState& gamepad = m_gamepads[static_cast<size_t>(joy)];
  switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
      return gamepad.leftStick.getX();
    case SDL_GAMEPAD_AXIS_LEFTY:
      return gamepad.leftStick.getY();
    case SDL_GAMEPAD_AXIS_RIGHTX:
      return gamepad.rightStick.getX();
    case SDL_GAMEPAD_AXIS_RIGHTY:
      return gamepad.rightStick.getY();
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
      return gamepad.leftTrigger;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
      return gamepad.rightTrigger;
    default:
      return 0.0f;
  }
}

bool InputManager::getButtonState(int joy, int buttonNumber) const {
  if (joy < 0 || joy >= static_cast<int>(m_gamepads.size()) ||
      buttonNumber < 0 ||
      buttonNumber >= static_cast<int>(m_gamepads[joy].buttonStates.size())) {
    return false;
  }

  return m_gamepads[joy].buttonStates[buttonNumber];
}

bool InputManager::getMouseButtonState(int buttonNumber) const {
  if (buttonNumber < 0 ||
      buttonNumber >= static_cast<int>(m_mouseButtonStates.size())) {
    return false;
  }

  return m_mouseButtonStates[buttonNumber];
}

const Vector2D& InputManager::getMousePosition() const {
  return m_mousePosition;
}

bool InputManager::wasKeyPressed(SDL_Scancode key) const {
  // Check if this key was pressed this frame using std::any_of
  return std::any_of(m_pressedThisFrame.begin(), m_pressedThisFrame.end(),
                     [key](SDL_Scancode pressedKey) { return pressedKey == key; });
}

void InputManager::clearFrameInput() {
  // This is now handled automatically in update()
  // but keeping for backward compatibility
  m_pressedThisFrame.clear();
}



// =============================================================================
// Command-layer — sampling
// =============================================================================

bool InputManager::sampleBinding(const InputBinding& b) const
{
    switch (b.source) {
        case InputSource::Keyboard:
            return isKeyDown(static_cast<SDL_Scancode>(b.code));

        case InputSource::MouseButton:
            return getMouseButtonState(b.code);

        case InputSource::GamepadButton:
            return getButtonState(0, b.code);

        case InputSource::GamepadAxisPositive:
            return getGamepadAxisValue(0, static_cast<SDL_GamepadAxis>(b.code)) > 0.3f;

        case InputSource::GamepadAxisNegative:
            switch (static_cast<SDL_GamepadAxis>(b.code)) {
                case SDL_GAMEPAD_AXIS_LEFTX:
                case SDL_GAMEPAD_AXIS_LEFTY:
                case SDL_GAMEPAD_AXIS_RIGHTX:
                case SDL_GAMEPAD_AXIS_RIGHTY:
                    return getGamepadAxisValue(0, static_cast<SDL_GamepadAxis>(b.code)) < -0.3f;
                default: return false;
            }
    }
    return false;
}

void InputManager::refreshCommandState()
{
    // Rebind capture takes priority: when active, no normal sampling occurs and
    // all command queries return false (guarded in isCommandDown/Pressed/Released).
    bool rebindJustCompleted = false;
    if (m_rebindCommand != Command::COUNT) {
        captureRebind();
        rebindJustCompleted = (m_rebindCommand == Command::COUNT);
        if (!rebindJustCompleted) {
            return;
        }
    }

    m_previousDown = m_currentDown;
    for (size_t i = 0; i < kCommandCount; ++i) {
        m_currentDown[i] = std::any_of(m_bindings[i].begin(), m_bindings[i].end(),
            [this](const InputBinding& binding) {
                return sampleBinding(binding);
            });
    }

    // Rebind just finalized this frame: consume the input that captured the
    // binding by freezing previousDown = currentDown. Without this, the A
    // press that captures a gamepad binding would also look like a fresh
    // rising edge for MenuConfirm on the next frame — re-firing simulateClick
    // on the binding row and reopening the just-closed dialog (flash + a
    // third press needed to finally bind).
    if (rebindJustCompleted) {
        m_previousDown = m_currentDown;
    }
}

void InputManager::captureRebind()
{
    // DESIGN: m_prevMouse/Gamepad* arrays are primed by startRebinding() so any
    // input already held at rebind-start is invisible to the rising-edge checks.
    // At the END of this function (no-capture path) the arrays are refreshed so
    // subsequent calls require a genuine release-then-press to capture.
    //
    // Capture is filtered by m_rebindCategory: inputs from the opposite category
    // are ignored so a rebind of the controller slot never captures a key press
    // and vice versa. On success, any existing binding in the same category is
    // replaced so the opposite-category binding is preserved.

    const size_t cmdIdx = static_cast<size_t>(m_rebindCommand);
    const bool wantKbdMouse = (m_rebindCategory == DeviceCategory::KeyboardMouse);
    const bool wantController = !wantKbdMouse;

    // Replace any existing binding in the rebind category with newBinding and
    // finalize the rebind. Bindings in the other category are left untouched.
    auto commitBinding = [&](InputBinding newBinding) {
        auto& vec = m_bindings[cmdIdx];
        std::erase_if(vec, [&](const InputBinding& b) {
            return categoryOf(b.source) == m_rebindCategory;
        });
        vec.push_back(newBinding);
        m_rebindCommand = Command::COUNT;
    };

    // ESC cancels (keyboard: m_pressedThisFrame is already rising-edge-only).
    // ESC cancels regardless of rebind category so the user always has an out.
    if (!m_pressedThisFrame.empty()) {
        if (std::find(m_pressedThisFrame.begin(), m_pressedThisFrame.end(),
                      SDL_SCANCODE_ESCAPE) != m_pressedThisFrame.end()) {
            INPUT_INFO("Rebind cancelled by ESC");
            m_rebindCommand = Command::COUNT;
            return;
        }
        if (wantKbdMouse) {
            SDL_Scancode captured = m_pressedThisFrame.front();
            const char* name = SDL_GetScancodeName(captured);
            INPUT_INFO(std::format("Rebound '{}' keyboard slot to '{}'",
                commandDisplayName(m_rebindCommand), name ? name : "Unknown"));
            commitBinding({InputSource::Keyboard, static_cast<int>(captured)});
            return;
        }
        // Wanted controller input — ignore keyboard press and keep capturing.
    }

    // Mouse button rising edge (Keyboard/Mouse category only).
    if (wantKbdMouse) {
        for (int btn = 0; btn < 3; ++btn) {
            bool cur = getMouseButtonState(btn);
            bool prev = m_prevMouseButtonStates[static_cast<size_t>(btn)];
            if (cur && !prev) {
                static constexpr std::array<const char*, 3> kBtnNames{"left", "middle", "right"};
                INPUT_INFO(std::format("Rebound '{}' mouse slot to '{}'",
                    commandDisplayName(m_rebindCommand), kBtnNames[static_cast<size_t>(btn)]));
                commitBinding({InputSource::MouseButton, btn});
                return;
            }
        }
    }

    // Gamepad button rising edge (Controller category only).
    if (wantController && !m_gamepads.empty()) {
        const GamepadState& gp = m_gamepads[0];
        const bool havePrevBtn = !m_prevGamepadButtonStates.empty() &&
                                  m_prevGamepadButtonStates[0].size() == gp.buttonStates.size();

        for (int btn = 0; btn < static_cast<int>(gp.buttonStates.size()); ++btn) {
            bool cur = gp.buttonStates[static_cast<size_t>(btn)];
            bool prev = havePrevBtn && m_prevGamepadButtonStates[0][static_cast<size_t>(btn)];
            if (cur && !prev) {
                const char* btnStr = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(btn));
                INPUT_INFO(std::format("Rebound '{}' controller slot to gamepad '{}'",
                    commandDisplayName(m_rebindCommand), btnStr ? btnStr : "unknown"));
                commitBinding({InputSource::GamepadButton, btn});
                return;
            }
        }

        // Gamepad axis rising edge — requires deflection to cross threshold from below.
        // Triggers are positive-only axes.
        const bool havePrevAxis = !m_prevGamepadAxisPos.empty();
        struct AxisEntry {
            size_t positiveIdx;
            size_t negativeIdx;
            SDL_GamepadAxis axis;
            bool supportsNegative;
        };
        AxisEntry axes[] = {
            {0, 0, SDL_GAMEPAD_AXIS_LEFTX,         true},
            {1, 1, SDL_GAMEPAD_AXIS_LEFTY,         true},
            {2, 2, SDL_GAMEPAD_AXIS_RIGHTX,        true},
            {3, 3, SDL_GAMEPAD_AXIS_RIGHTY,        true},
            {4, 0, SDL_GAMEPAD_AXIS_LEFT_TRIGGER,  false},
            {5, 0, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, false},
        };
        for (auto [positiveIdx, negativeIdx, axis, supportsNegative] : axes) {
            const float val = getGamepadAxisValue(0, axis);
            if (val > 0.5f) {
                bool prev = havePrevAxis && m_prevGamepadAxisPos[0][positiveIdx];
                if (!prev) {
                    INPUT_INFO(std::format("Rebound '{}' controller slot to gamepad axis {} positive",
                        commandDisplayName(m_rebindCommand), static_cast<int>(axis)));
                    commitBinding({InputSource::GamepadAxisPositive, static_cast<int>(axis)});
                    return;
                }
            }
            if (supportsNegative && val < -0.5f) {
                bool prev = havePrevAxis && m_prevGamepadAxisNeg[0][negativeIdx];
                if (!prev) {
                    INPUT_INFO(std::format("Rebound '{}' controller slot to gamepad axis {} negative",
                        commandDisplayName(m_rebindCommand), static_cast<int>(axis)));
                    commitBinding({InputSource::GamepadAxisNegative, static_cast<int>(axis)});
                    return;
                }
            }
        }
    }

    // No capture this frame — update prev-arrays so the next call detects real edges.
    for (size_t i = 0; i < 3; ++i) {
        m_prevMouseButtonStates[i] = getMouseButtonState(static_cast<int>(i));
    }
    if (!m_gamepads.empty() && !m_prevGamepadButtonStates.empty()) {
        const GamepadState& gp = m_gamepads[0];
        const size_t btnCount = gp.buttonStates.size();
        if (m_prevGamepadButtonStates[0].size() == btnCount) {
            for (size_t b = 0; b < btnCount; ++b) {
                m_prevGamepadButtonStates[0][b] = gp.buttonStates[b];
            }
        }
        if (!m_prevGamepadAxisPos.empty()) {
            m_prevGamepadAxisPos[0][0] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTX) > 0.5f;
            m_prevGamepadAxisPos[0][1] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTY) > 0.5f;
            m_prevGamepadAxisPos[0][2] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTX) > 0.5f;
            m_prevGamepadAxisPos[0][3] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTY) > 0.5f;
            m_prevGamepadAxisPos[0][4] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f;
            m_prevGamepadAxisPos[0][5] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f;
            m_prevGamepadAxisNeg[0][0] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTX) < -0.5f;
            m_prevGamepadAxisNeg[0][1] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTY) < -0.5f;
            m_prevGamepadAxisNeg[0][2] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTX) < -0.5f;
            m_prevGamepadAxisNeg[0][3] = getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTY) < -0.5f;
        }
    }
}

// =============================================================================
// Command-layer — query
// =============================================================================

bool InputManager::isCommandDown(Command c) const
{
    if (m_rebindCommand != Command::COUNT) {
        return false;
    }
    return m_currentDown[static_cast<size_t>(c)];
}

bool InputManager::isCommandPressed(Command c) const
{
    if (m_rebindCommand != Command::COUNT) {
        return false;
    }
    const size_t i = static_cast<size_t>(c);
    return m_currentDown[i] && !m_previousDown[i];
}

bool InputManager::isCommandReleased(Command c) const
{
    if (m_rebindCommand != Command::COUNT) {
        return false;
    }
    const size_t i = static_cast<size_t>(c);
    return !m_currentDown[i] && m_previousDown[i];
}

// =============================================================================
// Command-layer — binding management
// =============================================================================

void InputManager::addBinding(Command c, InputBinding b)
{
    m_bindings[static_cast<size_t>(c)].push_back(b);
}

void InputManager::clearBindings(Command c)
{
    m_bindings[static_cast<size_t>(c)].clear();
}

std::span<const InputManager::InputBinding> InputManager::getBindings(Command c) const
{
    return m_bindings[static_cast<size_t>(c)];
}

std::optional<InputManager::InputBinding>
InputManager::getBindingForCategory(Command c, DeviceCategory cat) const
{
    const auto& v = m_bindings[static_cast<size_t>(c)];
    const auto it = std::find_if(v.begin(), v.end(), [cat](const InputBinding& b) {
        return categoryOf(b.source) == cat;
    });
    if (it != v.end()) {
        return *it;
    }
    return std::nullopt;
}

InputManager::BindingSnapshot InputManager::captureBindings() const
{
    BindingSnapshot snapshot{};
    std::copy(m_bindings.begin(), m_bindings.end(), snapshot.begin());
    return snapshot;
}

void InputManager::restoreBindings(const BindingSnapshot& snapshot)
{
    m_bindings = snapshot;
    cancelRebinding();
    m_currentDown.fill(false);
    m_previousDown.fill(false);
}

void InputManager::loadDefaultBindings()
{
    for (auto& v : m_bindings) {
        v.clear();
    }

    using S = InputSource;
    using C = Command;

    auto add = [this](C cmd, S src, int code) {
        m_bindings[static_cast<size_t>(cmd)].push_back({src, code});
    };

    // INVARIANT: for every command, this table emits at most one binding per
    // DeviceCategory. captureRebind() preserves this by replacing the existing
    // binding in the rebind category on successful capture.
    add(C::MoveUp,        S::Keyboard,            SDL_SCANCODE_W);
    add(C::MoveUp,        S::GamepadAxisNegative, SDL_GAMEPAD_AXIS_LEFTY);
    add(C::MoveDown,      S::Keyboard,            SDL_SCANCODE_S);
    add(C::MoveDown,      S::GamepadAxisPositive, SDL_GAMEPAD_AXIS_LEFTY);
    add(C::MoveLeft,      S::Keyboard,            SDL_SCANCODE_A);
    add(C::MoveLeft,      S::GamepadAxisNegative, SDL_GAMEPAD_AXIS_LEFTX);
    add(C::MoveRight,     S::Keyboard,            SDL_SCANCODE_D);
    add(C::MoveRight,     S::GamepadAxisPositive, SDL_GAMEPAD_AXIS_LEFTX);
    add(C::AttackLight,   S::Keyboard,            SDL_SCANCODE_F);
    add(C::AttackLight,   S::GamepadButton,       SDL_GAMEPAD_BUTTON_WEST);
    add(C::Interact,      S::Keyboard,            SDL_SCANCODE_E);
    add(C::Interact,      S::GamepadButton,       SDL_GAMEPAD_BUTTON_SOUTH);
    add(C::OpenInventory, S::Keyboard,            SDL_SCANCODE_I);
    add(C::OpenInventory, S::GamepadButton,       SDL_GAMEPAD_BUTTON_NORTH);          // Y/Triangle
    // Pause keyboard default is ESC so the GamePlayState ESC-to-pause behaviour
    // survives the raw-scancode → Command::Pause swap.
    add(C::Pause,         S::Keyboard,            SDL_SCANCODE_ESCAPE);
    add(C::Pause,         S::GamepadButton,       SDL_GAMEPAD_BUTTON_START);
    // WorldInteract is a click-to-move command that uses the mouse screen
    // position (see PlayerRunningState / PlayerIdleState). A gamepad button has
    // no corresponding screen position, so we intentionally leave the Controller
    // slot unbound — the Controls UI will show "(unbound)" for this row.
    add(C::WorldInteract, S::MouseButton,         LEFT);
    add(C::ZoomIn,        S::Keyboard,            SDL_SCANCODE_RIGHTBRACKET);
    add(C::ZoomIn,        S::GamepadButton,       SDL_GAMEPAD_BUTTON_DPAD_UP);
    add(C::ZoomOut,       S::Keyboard,            SDL_SCANCODE_LEFTBRACKET);
    add(C::ZoomOut,       S::GamepadButton,       SDL_GAMEPAD_BUTTON_DPAD_DOWN);

    // Hotbar selection — keyboard 1-9. No gamepad defaults in v1; gamepad
    // hotbar scheme (D-pad cycle vs face-button shortcuts) is a v2 design call.
    add(C::HotbarSlot1,   S::Keyboard,            SDL_SCANCODE_1);
    add(C::HotbarSlot2,   S::Keyboard,            SDL_SCANCODE_2);
    add(C::HotbarSlot3,   S::Keyboard,            SDL_SCANCODE_3);
    add(C::HotbarSlot4,   S::Keyboard,            SDL_SCANCODE_4);
    add(C::HotbarSlot5,   S::Keyboard,            SDL_SCANCODE_5);
    add(C::HotbarSlot6,   S::Keyboard,            SDL_SCANCODE_6);
    add(C::HotbarSlot7,   S::Keyboard,            SDL_SCANCODE_7);
    add(C::HotbarSlot8,   S::Keyboard,            SDL_SCANCODE_8);
    add(C::HotbarSlot9,   S::Keyboard,            SDL_SCANCODE_9);

    // Menu commands are dispatched through the action layer
    // (MenuNavigation -> InputManager::isCommandPressed), so they are
    // device-agnostic at runtime: any binding present below fires when the
    // bound key/button is pressed, and a command with no binding simply does
    // nothing. Arrow keys drive keyboard menu navigation; D-Pad drives
    // gamepad menu navigation.
    add(C::MenuConfirm,   S::Keyboard,            SDL_SCANCODE_RETURN);
    add(C::MenuConfirm,   S::GamepadButton,       SDL_GAMEPAD_BUTTON_SOUTH);           // A/Cross
    add(C::MenuCancel,    S::Keyboard,            SDL_SCANCODE_ESCAPE);
    add(C::MenuCancel,    S::GamepadButton,       SDL_GAMEPAD_BUTTON_EAST);            // B/Circle
    add(C::MenuUp,        S::Keyboard,            SDL_SCANCODE_UP);
    add(C::MenuUp,        S::GamepadButton,       SDL_GAMEPAD_BUTTON_DPAD_UP);
    add(C::MenuDown,      S::Keyboard,            SDL_SCANCODE_DOWN);
    add(C::MenuDown,      S::GamepadButton,       SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    add(C::MenuLeft,      S::Keyboard,            SDL_SCANCODE_LEFT);
    add(C::MenuLeft,      S::GamepadButton,       SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    add(C::MenuRight,     S::Keyboard,            SDL_SCANCODE_RIGHT);
    add(C::MenuRight,     S::GamepadButton,       SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
}

void InputManager::resetBindingsToDefaults()
{
    loadDefaultBindings();
    INPUT_INFO("Input bindings reset to defaults");
}

// =============================================================================
// Command-layer — rebind capture control
// =============================================================================

void InputManager::startRebinding(Command c, DeviceCategory cat)
{
    m_rebindCommand = c;
    m_rebindCategory = cat;

    // Prime prev-mouse so the click that opened the rebind UI is not captured
    for (size_t i = 0; i < 3; ++i) {
        m_prevMouseButtonStates[i] = getMouseButtonState(static_cast<int>(i));
    }

    // Prime prev-gamepad arrays from current state so any held input at rebind
    // start requires a release-then-press cycle before it can be captured.
    m_prevGamepadButtonStates.clear();
    m_prevGamepadAxisPos.clear();
    m_prevGamepadAxisNeg.clear();

    for (const auto& gp : m_gamepads) {
        m_prevGamepadButtonStates.push_back(gp.buttonStates);
    }

    if (!m_gamepads.empty()) {
        m_prevGamepadAxisPos.push_back({
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTX) > 0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTY) > 0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTX) > 0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTY) > 0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f,
        });
        m_prevGamepadAxisNeg.push_back({
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTX) < -0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_LEFTY) < -0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTX) < -0.5f,
            getGamepadAxisValue(0, SDL_GAMEPAD_AXIS_RIGHTY) < -0.5f,
        });
    }

    INPUT_INFO(std::format("Waiting for input to rebind '{}' ({})",
        commandDisplayName(c),
        cat == DeviceCategory::KeyboardMouse ? "keyboard/mouse" : "controller"));
}

void InputManager::cancelRebinding()
{
    m_rebindCommand = Command::COUNT;
}

bool InputManager::isRebinding() const
{
    return m_rebindCommand != Command::COUNT;
}

InputManager::Command InputManager::getRebindingCommand() const
{
    return m_rebindCommand;
}

InputManager::DeviceCategory InputManager::getRebindingCategory() const
{
    return m_rebindCategory;
}

// =============================================================================
// Command-layer — persistence
// =============================================================================

namespace
{
    // Maps a command to its JSON key string
    const char* commandJsonKey(InputManager::Command c)
    {
        using C = InputManager::Command;
        switch (c) {
            case C::MoveUp:        return "move_up";
            case C::MoveDown:      return "move_down";
            case C::MoveLeft:      return "move_left";
            case C::MoveRight:     return "move_right";
            case C::AttackLight:   return "attack_light";
            case C::Interact:      return "interact";
            case C::OpenInventory: return "open_inventory";
            case C::Pause:         return "pause";
            case C::WorldInteract: return "world_interact";
            case C::ZoomIn:        return "zoom_in";
            case C::ZoomOut:       return "zoom_out";
            case C::HotbarSlot1:   return "hotbar_slot_1";
            case C::HotbarSlot2:   return "hotbar_slot_2";
            case C::HotbarSlot3:   return "hotbar_slot_3";
            case C::HotbarSlot4:   return "hotbar_slot_4";
            case C::HotbarSlot5:   return "hotbar_slot_5";
            case C::HotbarSlot6:   return "hotbar_slot_6";
            case C::HotbarSlot7:   return "hotbar_slot_7";
            case C::HotbarSlot8:   return "hotbar_slot_8";
            case C::HotbarSlot9:   return "hotbar_slot_9";
            case C::MenuConfirm:   return "menu_confirm";
            case C::MenuCancel:    return "menu_cancel";
            case C::MenuUp:        return "menu_up";
            case C::MenuDown:      return "menu_down";
            case C::MenuLeft:      return "menu_left";
            case C::MenuRight:     return "menu_right";
            case C::COUNT:         return nullptr;
        }
        return nullptr;
    }

    // Maps JSON key string to Command
    std::optional<InputManager::Command> jsonKeyToCommand(const std::string& key)
    {
        using C = InputManager::Command;
        static constexpr size_t kCount = static_cast<size_t>(C::COUNT);
        for (size_t i = 0; i < kCount; ++i) {
            const char* k = commandJsonKey(static_cast<C>(i));
            if (k && key == k) {
                return static_cast<C>(i);
            }
        }
        return std::nullopt;
    }

    const char* sourceJsonKey(InputManager::InputSource src)
    {
        using S = InputManager::InputSource;
        switch (src) {
            case S::Keyboard:            return "keyboard";
            case S::MouseButton:         return "mouse_button";
            case S::GamepadButton:       return "gamepad_button";
            case S::GamepadAxisPositive: return "gamepad_axis_pos";
            case S::GamepadAxisNegative: return "gamepad_axis_neg";
        }
        return "keyboard";
    }

    std::optional<InputManager::InputSource> jsonKeyToSource(const std::string& key)
    {
        using S = InputManager::InputSource;
        if (key == "keyboard")            return S::Keyboard;
        if (key == "mouse_button")        return S::MouseButton;
        if (key == "gamepad_button")      return S::GamepadButton;
        if (key == "gamepad_axis_pos")    return S::GamepadAxisPositive;
        if (key == "gamepad_axis_neg")    return S::GamepadAxisNegative;
        return std::nullopt;
    }

    // Encode a binding code to its string representation
    std::string encodeBindingCode(InputManager::InputSource src, int code)
    {
        using S = InputManager::InputSource;
        switch (src) {
            case S::Keyboard: {
                const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
                return name ? name : "Unknown";
            }
            case S::MouseButton:
                switch (code) {
                    case 0:  return "left";
                    case 1:  return "middle";
                    case 2:  return "right";
                    default: return "left";
                }
            case S::GamepadButton: {
                const char* name = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(code));
                return name ? name : "unknown";
            }
            case S::GamepadAxisPositive:
            case S::GamepadAxisNegative:
                switch (static_cast<SDL_GamepadAxis>(code)) {
                    case SDL_GAMEPAD_AXIS_LEFTX:         return "leftx";
                    case SDL_GAMEPAD_AXIS_LEFTY:         return "lefty";
                    case SDL_GAMEPAD_AXIS_RIGHTX:        return "rightx";
                    case SDL_GAMEPAD_AXIS_RIGHTY:        return "righty";
                    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:  return "ltrigger";
                    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return "rtrigger";
                    default: return "leftx";
                }
        }
        return "unknown";
    }

    // Decode a binding code string to int for a given source
    std::optional<int> decodeBindingCode(InputManager::InputSource src, const std::string& codeStr)
    {
        using S = InputManager::InputSource;
        switch (src) {
            case S::Keyboard: {
                SDL_Scancode sc = SDL_GetScancodeFromName(codeStr.c_str());
                if (sc == SDL_SCANCODE_UNKNOWN) {
                    return std::nullopt;
                }
                return static_cast<int>(sc);
            }
            case S::MouseButton:
                if (codeStr == "left")   return 0;
                if (codeStr == "middle") return 1;
                if (codeStr == "right")  return 2;
                return std::nullopt;
            case S::GamepadButton: {
                SDL_GamepadButton btn = SDL_GetGamepadButtonFromString(codeStr.c_str());
                if (btn == SDL_GAMEPAD_BUTTON_INVALID) {
                    return std::nullopt;
                }
                return static_cast<int>(btn);
            }
            case S::GamepadAxisPositive:
            case S::GamepadAxisNegative: {
                if (codeStr == "leftx")   return SDL_GAMEPAD_AXIS_LEFTX;
                if (codeStr == "lefty")   return SDL_GAMEPAD_AXIS_LEFTY;
                if (codeStr == "rightx")  return SDL_GAMEPAD_AXIS_RIGHTX;
                if (codeStr == "righty")  return SDL_GAMEPAD_AXIS_RIGHTY;
                if (codeStr == "ltrigger") return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
                if (codeStr == "rtrigger") return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
} // anonymous namespace

bool InputManager::loadBindingsFromFile(const std::string& path)
{
    VoidLight::JsonReader reader;
    if (!reader.loadFromFile(path)) {
        INPUT_DEBUG(std::format("Failed to load input bindings from '{}': {}",
            path, reader.getLastError()));
        return false;
    }

    const VoidLight::JsonValue& root = reader.getRoot();
    if (!root.isObject()) {
        INPUT_WARN(std::format("Input bindings file '{}' has invalid root type", path));
        return false;
    }

    // Validate schema version before processing to guard against future format changes
    if (!root.hasKey("schema_version") ||
        !root["schema_version"].isNumber() ||
        root["schema_version"].asInt() != 1)
    {
        INPUT_WARN(std::format("Unsupported or missing schema_version in '{}'", path));
        return false;
    }

    if (!root.hasKey("commands")) {
        INPUT_WARN(std::format("Input bindings file '{}' missing 'commands' key", path));
        return false;
    }

    const VoidLight::JsonValue& commands = root["commands"];
    if (!commands.isObject()) {
        INPUT_WARN(std::format("Input bindings file '{}': 'commands' is not an object", path));
        return false;
    }

    // Snapshot defaults so we can back-fill any category missing from the file.
    // Without this, an older input_bindings.json that predates new commands or
    // new Controller defaults would silently leave those slots unbound.
    loadDefaultBindings();
    std::array<std::vector<InputBinding>, kCommandCount> defaults;
    for (size_t i = 0; i < kCommandCount; ++i) {
        defaults[i] = m_bindings[i];
    }

    const VoidLight::JsonObject& cmdObj = commands.asObject();
    for (const auto& [cmdKey, bindingsVal] : cmdObj) {
        auto cmd = jsonKeyToCommand(cmdKey);
        if (!cmd.has_value()) {
            INPUT_WARN(std::format("Unknown command key '{}' in bindings file — skipping", cmdKey));
            continue;
        }

        if (!bindingsVal.isArray()) {
            INPUT_WARN(std::format("Bindings for '{}' is not an array — skipping", cmdKey));
            continue;
        }

        m_bindings[static_cast<size_t>(*cmd)].clear();

        const VoidLight::JsonArray& arr = bindingsVal.asArray();
        for (const auto& entry : arr) {
            if (!entry.isObject()) {
                INPUT_WARN(std::format("Binding entry for '{}' is not an object — skipping", cmdKey));
                continue;
            }
            if (!entry.hasKey("source") || !entry.hasKey("code")) {
                INPUT_WARN(std::format("Binding entry for '{}' missing source/code — skipping", cmdKey));
                continue;
            }

            auto srcStr = entry["source"].tryAsString();
            auto codeStr = entry["code"].tryAsString();
            if (!srcStr.has_value() || !codeStr.has_value()) {
                INPUT_WARN(std::format("Binding entry for '{}' has non-string source/code — skipping", cmdKey));
                continue;
            }

            auto src = jsonKeyToSource(*srcStr);
            if (!src.has_value()) {
                INPUT_WARN(std::format("Unknown source '{}' in binding for '{}' — skipping", *srcStr, cmdKey));
                continue;
            }

            auto code = decodeBindingCode(*src, *codeStr);
            if (!code.has_value()) {
                INPUT_WARN(std::format("Unknown code '{}' for source '{}' in binding for '{}' — skipping",
                    *codeStr, *srcStr, cmdKey));
                continue;
            }

            m_bindings[static_cast<size_t>(*cmd)].push_back({*src, *code});
        }
    }

    // Back-fill any category that has no binding after the file load. This
    // heals older files that predate new commands or new Controller defaults
    // in place — no version key required.
    for (size_t i = 0; i < kCommandCount; ++i) {
        auto& current = m_bindings[i];
        auto hasCategory = [&](DeviceCategory cat) {
            return std::any_of(current.begin(), current.end(), [cat](const InputBinding& b) {
                return categoryOf(b.source) == cat;
            });
        };
        for (const auto& def : defaults[i]) {
            if (!hasCategory(categoryOf(def.source))) {
                current.push_back(def);
            }
        }
    }

    INPUT_INFO(std::format("Loaded input bindings from '{}'", path));
    return true;
}

bool InputManager::saveBindingsToFile(const std::string& path) const
{
    VoidLight::JsonObject commandsObj;
    for (size_t i = 0; i < kCommandCount; ++i) {
        const char* key = commandJsonKey(static_cast<Command>(i));
        if (!key || m_bindings[i].empty()) {
            continue;
        }

        VoidLight::JsonArray bindingsArr;
        for (const auto& b : m_bindings[i]) {
            VoidLight::JsonObject entry;
            entry["source"] = VoidLight::JsonValue(std::string(sourceJsonKey(b.source)));
            entry["code"]   = VoidLight::JsonValue(encodeBindingCode(b.source, b.code));
            bindingsArr.push_back(VoidLight::JsonValue(std::move(entry)));
        }
        commandsObj[key] = VoidLight::JsonValue(std::move(bindingsArr));
    }

    VoidLight::JsonObject rootObj;
    rootObj["schema_version"] = VoidLight::JsonValue(1);
    rootObj["commands"]       = VoidLight::JsonValue(std::move(commandsObj));
    VoidLight::JsonValue root(std::move(rootObj));

    std::ofstream file(path);
    if (!file.is_open()) {
        INPUT_ERROR(std::format("Failed to open '{}' for writing input bindings", path));
        return false;
    }

    file << root.toString(/*pretty=*/true) << '\n';

    INPUT_INFO(std::format("Saved input bindings to '{}'", path));
    return true;
}

// =============================================================================
// Command-layer — UI helpers
// =============================================================================

// ---------------------------------------------------------------------------
// Gamepad label policy
// ---------------------------------------------------------------------------
// Face buttons (SOUTH/EAST/WEST/NORTH) go through SDL_GetGamepadButtonLabel,
// which returns the vendor-specific SDL_GamepadButtonLabel enum (A/B/X/Y for
// Xbox, CROSS/CIRCLE/SQUARE/TRIANGLE for PlayStation). SDL3 handles the vendor
// detection natively here — we just translate its enum to a display string.
//
// For non-face buttons SDL does not expose a vendor-label API, so we branch on
// our own getGamepadVendor():
//   * Different printed labels → vendor-specific string (LB vs L1, LT vs L2).
//   * Same printed label → single universal string (D-Pad, Guide/PS excluded).
//   * Functionally-universal buttons → single string regardless of print
//     (START → "Start" — it is the pause button on both platforms even though
//     PS prints "OPTIONS" on the cap).
//   * Functionally-different buttons → vendor-specific even when the button
//     position matches (BACK → "Back" on Xbox for the secondary menu, "Share"
//     on PS for screenshot/capture — not interchangeable).

static const char* faceButtonLabel(SDL_GamepadButtonLabel label)
{
    switch (label) {
        case SDL_GAMEPAD_BUTTON_LABEL_A:        return "A";
        case SDL_GAMEPAD_BUTTON_LABEL_B:        return "B";
        case SDL_GAMEPAD_BUTTON_LABEL_X:        return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_Y:        return "Y";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS:    return "Cross";
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:   return "Circle";
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:   return "Square";
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "Triangle";
        case SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN:  return nullptr;
    }
    return nullptr;
}

// Non-face buttons. SDL has no built-in label API here, so we branch.
// Returns nullptr for unmapped buttons; caller falls back to SDL's raw name.
static const char* nonFaceButtonLabel(SDL_GamepadButton btn,
                                      InputManager::GamepadVendor vendor)
{
    using V = InputManager::GamepadVendor;
    const bool isPS = (vendor == V::PlayStation);
    switch (btn) {
        case SDL_GAMEPAD_BUTTON_BACK:           return isPS ? "Share"   : "Back";
        case SDL_GAMEPAD_BUTTON_GUIDE:          return isPS ? "PS"      : "Guide";
        case SDL_GAMEPAD_BUTTON_START:          return "Start";  // universal (pause)
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return isPS ? "L3"      : "L-Stick";
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return isPS ? "R3"      : "R-Stick";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return isPS ? "L1"      : "LB";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return isPS ? "R1"      : "RB";
        case SDL_GAMEPAD_BUTTON_DPAD_UP:        return "D-Pad Up";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return "D-Pad Down";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return "D-Pad Left";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return "D-Pad Right";
        default:                                return nullptr;
    }
}

// Trigger label (triggers are axes in SDL, not buttons). Xbox and Generic
// share the LT/RT convention; PlayStation uses L2/R2.
static const char* gamepadTriggerLabel(SDL_GamepadAxis axis,
                                       InputManager::GamepadVendor vendor)
{
    const bool isPS = (vendor == InputManager::GamepadVendor::PlayStation);
    switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:  return isPS ? "L2" : "LT";
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return isPS ? "R2" : "RT";
        default:                             return nullptr;
    }
}

InputManager::GamepadVendor InputManager::getGamepadVendor() const noexcept
{
    if (m_gamepads.empty() || !m_gamepads[0].pGamepad) {
        return GamepadVendor::Generic;
    }
    switch (SDL_GetGamepadType(m_gamepads[0].pGamepad)) {
        case SDL_GAMEPAD_TYPE_XBOX360:
        case SDL_GAMEPAD_TYPE_XBOXONE:
            return GamepadVendor::Xbox;
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5:
            return GamepadVendor::PlayStation;
        default:
            return GamepadVendor::Generic;
    }
}

std::string InputManager::describeBinding(InputBinding b) const
{
    using S = InputSource;
    switch (b.source) {
        case S::Keyboard: {
            const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(b.code));
            return name ? name : "Unknown Key";
        }
        case S::MouseButton:
            switch (b.code) {
                case 0:  return "Left Mouse";
                case 1:  return "Middle Mouse";
                case 2:  return "Right Mouse";
                default: return "Mouse";
            }
        case S::GamepadButton: {
            const auto btn = static_cast<SDL_GamepadButton>(b.code);
            // Face buttons: let SDL3 resolve the vendor label (A/B/X/Y or
            // Cross/Circle/Square/Triangle) from the connected controller.
            if (btn == SDL_GAMEPAD_BUTTON_SOUTH || btn == SDL_GAMEPAD_BUTTON_EAST ||
                btn == SDL_GAMEPAD_BUTTON_WEST  || btn == SDL_GAMEPAD_BUTTON_NORTH)
            {
                SDL_Gamepad* pad = m_gamepads.empty() ? nullptr : m_gamepads[0].pGamepad;
                const SDL_GamepadButtonLabel sdlLabel =
                    pad ? SDL_GetGamepadButtonLabel(pad, btn)
                        : SDL_GetGamepadButtonLabelForType(SDL_GAMEPAD_TYPE_XBOXONE, btn);
                if (const char* faceLabel = faceButtonLabel(sdlLabel)) {
                    return faceLabel;
                }
            }
            // All other buttons: our own vendor policy.
            if (const char* label = nonFaceButtonLabel(btn, getGamepadVendor())) {
                return label;
            }
            // Fallback for any new SDL button SDL3 adds later.
            const char* sdlName = SDL_GetGamepadStringForButton(btn);
            return sdlName ? std::format("Gamepad {}", sdlName) : "Gamepad Btn";
        }
        case S::GamepadAxisPositive:
        case S::GamepadAxisNegative: {
            const auto axis = static_cast<SDL_GamepadAxis>(b.code);
            const GamepadVendor vendor = getGamepadVendor();
            if (const char* triggerLabel = gamepadTriggerLabel(axis, vendor)) {
                return triggerLabel;
            }
            // Stick axis: direction matters.
            const char* axisName = [&] {
                switch (axis) {
                    case SDL_GAMEPAD_AXIS_LEFTX:  return "L-Stick X";
                    case SDL_GAMEPAD_AXIS_LEFTY:  return "L-Stick Y";
                    case SDL_GAMEPAD_AXIS_RIGHTX: return "R-Stick X";
                    case SDL_GAMEPAD_AXIS_RIGHTY: return "R-Stick Y";
                    default:                      return "Stick";
                }
            }();
            const char* sign = (b.source == S::GamepadAxisPositive) ? "+" : "-";
            return std::format("{} {}", axisName, sign);
        }
    }
    return "Unknown";
}

std::string InputManager::commandDisplayName(Command c) const
{
    using C = Command;
    switch (c) {
        case C::MoveUp:        return "Move Up";
        case C::MoveDown:      return "Move Down";
        case C::MoveLeft:      return "Move Left";
        case C::MoveRight:     return "Move Right";
        case C::AttackLight:   return "Attack (Light)";
        case C::Interact:      return "Interact";
        case C::OpenInventory: return "Open Inventory";
        case C::Pause:         return "Pause";
        case C::WorldInteract: return "World Interact";
        case C::ZoomIn:        return "Zoom In";
        case C::ZoomOut:       return "Zoom Out";
        case C::HotbarSlot1:   return "Hotbar Slot 1";
        case C::HotbarSlot2:   return "Hotbar Slot 2";
        case C::HotbarSlot3:   return "Hotbar Slot 3";
        case C::HotbarSlot4:   return "Hotbar Slot 4";
        case C::HotbarSlot5:   return "Hotbar Slot 5";
        case C::HotbarSlot6:   return "Hotbar Slot 6";
        case C::HotbarSlot7:   return "Hotbar Slot 7";
        case C::HotbarSlot8:   return "Hotbar Slot 8";
        case C::HotbarSlot9:   return "Hotbar Slot 9";
        case C::MenuConfirm:   return "Menu Confirm";
        case C::MenuCancel:    return "Menu Cancel";
        case C::MenuUp:        return "Menu Up";
        case C::MenuDown:      return "Menu Down";
        case C::MenuLeft:      return "Menu Left";
        case C::MenuRight:     return "Menu Right";
        case C::COUNT:         return "Unknown";
    }
    return "Unknown";
}

void InputManager::onKeyDown(const SDL_Event& event) {
  if (event.key.repeat) {
    return;
  }

  // Track this key as pressed this frame (for wasKeyPressed)
  // Check for duplicates to avoid multiple entries for the same key in one frame
  bool alreadyTracked = std::any_of(m_pressedThisFrame.begin(), m_pressedThisFrame.end(),
                                   [scancode = event.key.scancode](SDL_Scancode pressedKey) {
                                     return pressedKey == scancode;
                                   });

  if (!alreadyTracked) {
    m_pressedThisFrame.push_back(event.key.scancode);
  }
}

void InputManager::onKeyUp(const SDL_Event& /*event*/) {
  // Keyboard state is already updated by SDL event system

  // Key-specific processing can be handled by game states
  // using the isKeyDown() method
}

void InputManager::onMouseMove(const SDL_Event& event) {
  updateMousePositionFromWindowCoords(event.motion.x, event.motion.y);
}

void InputManager::onMouseButtonDown(const SDL_Event& event) {
  updateMousePositionFromWindowCoords(event.button.x, event.button.y);

  if (event.button.button == SDL_BUTTON_LEFT) {
    m_mouseButtonStates[LEFT] = true;
    INPUT_DEBUG("Mouse button Left clicked!");
  }
  if (event.button.button == SDL_BUTTON_MIDDLE) {
    m_mouseButtonStates[MIDDLE] = true;
    INPUT_DEBUG("Mouse button Middle clicked!");
  }
  if (event.button.button == SDL_BUTTON_RIGHT) {
    m_mouseButtonStates[RIGHT] = true;
    INPUT_DEBUG("Mouse button Right clicked!");
  }
}

void InputManager::onMouseButtonUp(const SDL_Event& event) {
  updateMousePositionFromWindowCoords(event.button.x, event.button.y);

  if (event.button.button == SDL_BUTTON_LEFT) {
    m_mouseButtonStates[LEFT] = false;
  }
  if (event.button.button == SDL_BUTTON_MIDDLE) {
    m_mouseButtonStates[MIDDLE] = false;
  }
  if (event.button.button == SDL_BUTTON_RIGHT) {
    m_mouseButtonStates[RIGHT] = false;
  }
}

void InputManager::onGamepadAxisMove(const SDL_Event& event) {
  auto index = findGamepadIndex(event.gaxis.which);
  if (!index.has_value()) {
    return;
  }

  const size_t whichOne = *index;
  GamepadState& gamepadState = m_gamepads[whichOne];

  // Get axis name for debug messages
  const char* axisName;
  switch (event.gaxis.axis)
  {
    case SDL_GAMEPAD_AXIS_LEFTX: axisName = "Left Stick X"; break;
    case SDL_GAMEPAD_AXIS_LEFTY: axisName = "Left Stick Y"; break;
    case SDL_GAMEPAD_AXIS_RIGHTX: axisName = "Right Stick X"; break;
    case SDL_GAMEPAD_AXIS_RIGHTY: axisName = "Right Stick Y"; break;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: axisName = "Left Trigger"; break;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: axisName = "Right Trigger"; break;
    default: axisName = "Unknown Axis";
  }

  if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
    gamepadState.leftStick.setX(normalizeGamepadAxisValue(event.gaxis.value, m_joystickDeadZone));
  }
  if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
    gamepadState.leftStick.setY(normalizeGamepadAxisValue(event.gaxis.value, m_joystickDeadZone));
  }
  if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
    gamepadState.rightStick.setX(normalizeGamepadAxisValue(event.gaxis.value, m_joystickDeadZone));
  }
  if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
    gamepadState.rightStick.setY(normalizeGamepadAxisValue(event.gaxis.value, m_joystickDeadZone));
  }

  // Process left trigger (L2/LT)
  if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
    gamepadState.leftTrigger = std::max(
        0.0f, normalizeGamepadAxisValue(event.gaxis.value, m_joystickDeadZone));
    INPUT_DEBUG_IF(event.gaxis.value > m_joystickDeadZone,
        std::format("Gamepad {} - {} pressed: {}", whichOne, axisName, event.gaxis.value));
  }

  // Process right trigger (R2/RT)
  if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
    gamepadState.rightTrigger = std::max(
        0.0f, normalizeGamepadAxisValue(event.gaxis.value, m_joystickDeadZone));
    INPUT_DEBUG_IF(event.gaxis.value > m_joystickDeadZone,
        std::format("Gamepad {} - {} pressed: {}", whichOne, axisName, event.gaxis.value));
  }
}

void InputManager::onGamepadButtonDown(const SDL_Event& event) {
  auto index = findGamepadIndex(event.gbutton.which);
  if (!index.has_value()) {
    return;
  }

  GamepadState& gamepadState = m_gamepads[*index];
  if (event.gbutton.button >= static_cast<int>(gamepadState.buttonStates.size())) {
    return;
  }

  gamepadState.buttonStates[event.gbutton.button] = true;

  // Get button name for debug message
  const char* buttonName;
  switch (event.gbutton.button) {
    case 0: buttonName = "A or CROSS"; break;
    case 1: buttonName = "B or CIRCLE"; break;
    case 2: buttonName = "X or SQUARE"; break;
    case 3: buttonName = "Y or TRIANGLE"; break;
    case 4: buttonName = "Back"; break;
    case 5: buttonName = "Guide"; break;
    case 6: buttonName = "Start"; break;
    case 7: buttonName = "Left Stick"; break;
    case 8: buttonName = "Right Stick"; break;
    case 9: buttonName = "Left Shoulder"; break;
    case 10: buttonName = "Right Shoulder"; break;
    case 11: buttonName = "D-Pad Up"; break;
    case 12: buttonName = "D-Pad Down"; break;
    case 13: buttonName = "D-Pad Left"; break;
    case 14: buttonName = "D-Pad Right"; break;
    default: buttonName = "Unknown";
  }

  // Debug message for button press with button name
  INPUT_DEBUG(std::format("Gamepad {} Button '{}' ({}) pressed!",
                          *index, buttonName, static_cast<int>(event.gbutton.button)));
}

void InputManager::onGamepadButtonUp(const SDL_Event& event) {
  auto index = findGamepadIndex(event.gbutton.which);
  if (!index.has_value()) {
    return;
  }

  GamepadState& gamepadState = m_gamepads[*index];
  if (event.gbutton.button >= static_cast<int>(gamepadState.buttonStates.size())) {
    return;
  }

  gamepadState.buttonStates[event.gbutton.button] = false;
}

void InputManager::onGamepadAdded(const SDL_Event& event) {
  if (openGamepad(event.gdevice.which)) {
    m_gamePadInitialized = true;
  }
}

void InputManager::onGamepadRemoved(const SDL_Event& event) {
  closeGamepad(event.gdevice.which);
  m_gamePadInitialized = !m_gamepads.empty();
}

void InputManager::onGamepadRemapped(const SDL_Event& event) {
  if (findGamepadIndex(event.gdevice.which).has_value()) {
    INPUT_INFO(std::format("Gamepad remapped: instance {}", event.gdevice.which));
    return;
  }

  if (openGamepad(event.gdevice.which)) {
    m_gamePadInitialized = true;
    INPUT_INFO(std::format("Opened remapped gamepad instance {}", event.gdevice.which));
  }
}

void InputManager::onFocusLost() {
  // SDL tracks keyboard state internally. Clear it on the main thread so held keys
  // don't remain logically pressed when focus returns.
  SDL_ResetKeyboard();

  m_pressedThisFrame.clear();
  reset();
  clearGamepadState();
}

void InputManager::clean() {
  if (m_isShutdown) {
    return;
  }

  // Close gamepad handles
  if (m_gamePadInitialized) {
    size_t count = m_gamepads.size();
    for (auto& gamepad : m_gamepads) {
      if (gamepad.pGamepad) {
        SDL_CloseGamepad(gamepad.pGamepad);
        gamepad.pGamepad = nullptr;
      }
    }
    m_gamepads.clear();
    m_gamePadInitialized = false;

    if (count > 0) {
      INPUT_INFO(std::format("Closed {} gamepad handles", count));
    }
  } else {
    INPUT_INFO("No gamepads to free");
  }

  // Clear mouse states
  m_mouseButtonStates.clear();

  // Set shutdown flag
  m_isShutdown = true;
  INPUT_INFO("InputManager resources cleaned");
}

std::optional<size_t> InputManager::findGamepadIndex(SDL_JoystickID instanceId) const {
  auto it = std::find_if(
      m_gamepads.begin(), m_gamepads.end(),
      [instanceId](const GamepadState& gamepadState) {
        return gamepadState.instanceId == instanceId;
      });

  if (it == m_gamepads.end()) {
    return std::nullopt;
  }

  return static_cast<size_t>(std::distance(m_gamepads.begin(), it));
}

bool InputManager::openGamepad(SDL_JoystickID instanceId) {
  if (!SDL_IsGamepad(instanceId)) {
    return false;
  }

  if (findGamepadIndex(instanceId).has_value()) {
    return true;
  }

  SDL_Gamepad* gamepad = SDL_OpenGamepad(instanceId);
  if (!gamepad) {
    INPUT_ERROR(std::format("Could not open gamepad {}: {}", instanceId, SDL_GetError()));
    return false;
  }

  GamepadState gamepadState;
  gamepadState.instanceId = instanceId;
  gamepadState.pGamepad = gamepad;
  gamepadState.buttonStates.assign(SDL_GAMEPAD_BUTTON_COUNT, false);

  const char* pName = SDL_GetGamepadName(gamepad);
  INPUT_INFO(std::format("Gamepad connected: {} (instance {})",
                         pName ? pName : "Unknown",
                         instanceId));

  m_gamepads.push_back(std::move(gamepadState));
  return true;
}

void InputManager::closeGamepad(SDL_JoystickID instanceId) {
  auto index = findGamepadIndex(instanceId);
  if (!index.has_value()) {
    return;
  }

  GamepadState& gamepadState = m_gamepads[*index];
  if (gamepadState.pGamepad) {
    SDL_CloseGamepad(gamepadState.pGamepad);
  }

  m_gamepads.erase(m_gamepads.begin() + static_cast<std::ptrdiff_t>(*index));
  INPUT_INFO(std::format("Gamepad disconnected: instance {}", instanceId));
}

void InputManager::updateMousePositionFromWindowCoords(float x, float y) {
  // GPU renders at pixel resolution, but SDL mouse events are in window coordinates.
  // Scale by pixel density to convert window coords to pixel coords.
  float scale = 1.0f;
  SDL_Window* window = GameEngine::Instance().getWindow();
  if (window) {
    scale = SDL_GetWindowPixelDensity(window);
  }

  m_mousePosition.setX(x * scale);
  m_mousePosition.setY(y * scale);
}

void InputManager::clearGamepadState() {
  for (auto& gamepadState : m_gamepads) {
    gamepadState.leftStick = Vector2D(0.0f, 0.0f);
    gamepadState.rightStick = Vector2D(0.0f, 0.0f);
    gamepadState.leftTrigger = 0.0f;
    gamepadState.rightTrigger = 0.0f;
    std::fill(gamepadState.buttonStates.begin(), gamepadState.buttonStates.end(),
              false);
  }
}

float InputManager::normalizeGamepadAxisValue(Sint16 value, int deadZone) {
  if (std::abs(value) <= deadZone) {
    return 0.0f;
  }

  const float absValue = static_cast<float>(std::abs(value));
  const float normalizedMagnitude =
      std::clamp((absValue - static_cast<float>(deadZone)) /
                     (32767.0f - static_cast<float>(deadZone)),
                 0.0f, 1.0f);

  return (value < 0) ? -normalizedMagnitude : normalizedMagnitude;
}
