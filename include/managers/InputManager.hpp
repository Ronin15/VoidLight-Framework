/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef INPUT_MANAGER_HPP
#define INPUT_MANAGER_HPP

#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "utils/Vector2D.hpp"

enum mouse_buttons { LEFT = 0, MIDDLE = 1, RIGHT = 2 };

class InputManager {
 public:
    // -------------------------------------------------------------------------
    // Command-pattern types
    // -------------------------------------------------------------------------

    enum class Command : uint32_t
    {
        // Movement — composed into direction vector at call site
        MoveUp, MoveDown, MoveLeft, MoveRight,
        // Gameplay
        AttackLight, Interact, OpenInventory, Pause,
        WorldInteract,   // LMB world click (mouse pos read from getMousePosition())
        ZoomIn, ZoomOut,
        // Hotbar selection (v1: select-only; use action TBD)
        HotbarSlot1, HotbarSlot2, HotbarSlot3, HotbarSlot4, HotbarSlot5,
        HotbarSlot6, HotbarSlot7, HotbarSlot8, HotbarSlot9,
        // Menu
        MenuConfirm, MenuCancel, MenuUp, MenuDown, MenuLeft, MenuRight,
        COUNT
    };

    enum class InputSource : uint8_t
    {
        Keyboard,             // code = SDL_Scancode
        MouseButton,          // code = 0/1/2 (LEFT/MIDDLE/RIGHT)
        GamepadButton,        // code = SDL_GamepadButton
        GamepadAxisPositive,  // code = SDL_GamepadAxis, active when axis > +0.3
        GamepadAxisNegative,  // code = SDL_GamepadAxis, stick axes only, active when axis < -0.3
    };

    // Device grouping used by the Controls UI and by rebind capture filtering.
    // Keyboard + mouse share one category; all gamepad inputs share the other.
    enum class DeviceCategory : uint8_t
    {
        KeyboardMouse = 0,
        Controller    = 1,
    };

    static constexpr DeviceCategory categoryOf(InputSource s) noexcept
    {
        return (s == InputSource::Keyboard || s == InputSource::MouseButton)
                   ? DeviceCategory::KeyboardMouse
                   : DeviceCategory::Controller;
    }

    struct InputBinding
    {
        InputSource source;
        int code;
    };
    using BindingSnapshot = std::array<std::vector<InputBinding>, static_cast<size_t>(Command::COUNT)>;

    // -------------------------------------------------------------------------
    // Command query — main thread only.
    // refreshCommandState() writes m_currentDown/m_previousDown;
    // these methods read them. Callers from worker threads must use the raw
    // isKeyDown/getButtonState APIs instead.
    // -------------------------------------------------------------------------
    bool isCommandPressed(Command c) const;   // rising edge this frame
    bool isCommandDown(Command c) const;      // currently active
    bool isCommandReleased(Command c) const;  // falling edge this frame

    // -------------------------------------------------------------------------
    // Binding management
    // -------------------------------------------------------------------------
    void addBinding(Command c, InputBinding b);
    void clearBindings(Command c);
    std::span<const InputBinding> getBindings(Command c) const;
    // Returns the first binding for this command whose source belongs to the
    // given device category, or std::nullopt if none exists.
    std::optional<InputBinding> getBindingForCategory(Command c, DeviceCategory cat) const;
    BindingSnapshot captureBindings() const;
    void restoreBindings(const BindingSnapshot& snapshot);
    void resetBindingsToDefaults();

    // -------------------------------------------------------------------------
    // Rebind capture ("press any key" UX)
    // -------------------------------------------------------------------------
    // Rebinds the binding for command `c` in device category `cat`. During
    // capture, inputs from the opposite category are ignored; on successful
    // capture, any existing binding in that category is replaced.
    void startRebinding(Command c, DeviceCategory cat);
    void cancelRebinding();
    bool isRebinding() const;
    Command getRebindingCommand() const;
    DeviceCategory getRebindingCategory() const;

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------
    [[nodiscard]] bool loadBindingsFromFile(const std::string& path);
    [[nodiscard]] bool saveBindingsToFile(const std::string& path) const;

    // -------------------------------------------------------------------------
    // UI helpers
    // -------------------------------------------------------------------------
    std::string describeBinding(InputBinding b) const;   // "F", "Left Mouse", "A", "Cross"
    std::string commandDisplayName(Command c) const;     // "Attack (Light)"

    // Controller family for vendor-specific button labels. Detected from
    // SDL_GetGamepadType on the primary connected gamepad. Generic covers
    // unknown/standard pads modeled after Xbox (which is SDL's internal
    // convention) — labels default to Xbox-style. We don't ship
    // Nintendo-specific labels; Switch pads fall through to Generic.
    enum class GamepadVendor : uint8_t {
        Xbox,
        PlayStation,
        Generic,
    };

    [[nodiscard]] GamepadVendor getGamepadVendor() const noexcept;

    // -------------------------------------------------------------------------
    // Existing interface
    // -------------------------------------------------------------------------
    ~InputManager() {
        if (!m_isShutdown) {
            clean();
        }
    }

    static InputManager& Instance()
    {
        static InputManager instance;
        return instance;
    }

    [[nodiscard]] bool init();
    bool isInitialized() const { return m_isInitialized; }
    void initializeGamePad();
    void reset();
    void clean();
    bool isShutdown() const { return m_isShutdown; }

    bool isKeyDown(SDL_Scancode key) const;
    bool wasKeyPressed(SDL_Scancode key) const;
    void clearFrameInput();

    // True while at least one gamepad is currently connected and opened.
    // Used by menu navigation to gate UI-focus highlighting to controller users.
    bool isGamepadConnected() const noexcept { return !m_gamepads.empty(); }

    float getAxisX(int joy, int stick) const;
    float getAxisY(int joy, int stick) const;
    bool getButtonState(int joy, int buttonNumber) const;

    bool getMouseButtonState(int buttonNumber) const;
    const Vector2D& getMousePosition() const;

    void onKeyDown(const SDL_Event& event);
    void onKeyUp(const SDL_Event& event);
    void onMouseMove(const SDL_Event& event);
    void onMouseButtonDown(const SDL_Event& event);
    void onMouseButtonUp(const SDL_Event& event);
    void onGamepadAxisMove(const SDL_Event& event);
    void onGamepadButtonDown(const SDL_Event& event);
    void onGamepadButtonUp(const SDL_Event& event);
    void onGamepadAdded(const SDL_Event& event);
    void onGamepadRemoved(const SDL_Event& event);
    void onGamepadRemapped(const SDL_Event& event);
    void onFocusLost();

    // Called from GameEngine::handleEvents() after the SDL poll loop,
    // before game states read input. Resolves command edges for this frame,
    // or drives rebind capture if startRebinding() is active.
    // Main thread only — see command query comment above.
    void refreshCommandState();

 private:
    struct GamepadState {
        SDL_JoystickID instanceId{0};
        SDL_Gamepad* pGamepad{nullptr};
        Vector2D leftStick{0.0f, 0.0f};
        Vector2D rightStick{0.0f, 0.0f};
        float leftTrigger{0.0f};
        float rightTrigger{0.0f};
        std::vector<bool> buttonStates{};
    };

    // -------------------------------------------------------------------------
    // Command-layer storage
    // -------------------------------------------------------------------------
    static constexpr size_t kCommandCount = static_cast<size_t>(Command::COUNT);
    std::array<std::vector<InputBinding>, kCommandCount> m_bindings{};
    std::array<bool, kCommandCount> m_currentDown{};
    std::array<bool, kCommandCount> m_previousDown{};

    Command m_rebindCommand{Command::COUNT};   // COUNT = none
    DeviceCategory m_rebindCategory{DeviceCategory::KeyboardMouse};

    bool sampleBinding(const InputBinding& b) const;
    void captureRebind();          // called from refreshCommandState() while rebinding
    void loadDefaultBindings();

    // -------------------------------------------------------------------------
    // Raw input storage
    // -------------------------------------------------------------------------
    const bool* m_keystates{nullptr};
    std::vector<SDL_Scancode> m_pressedThisFrame{};

    std::vector<GamepadState> m_gamepads{};
    const int m_joystickDeadZone{10000};
    bool m_gamePadInitialized{false};

    std::vector<bool> m_mouseButtonStates{};
    // Previous input states for rebind-capture edge detection (primed by startRebinding())
    std::array<bool, 3> m_prevMouseButtonStates{};
    std::vector<std::vector<bool>> m_prevGamepadButtonStates;    // parallel to m_gamepads
    std::vector<std::array<bool, 6>> m_prevGamepadAxisPos;       // sticks + triggers positive threshold
    std::vector<std::array<bool, 4>> m_prevGamepadAxisNeg;       // stick axes negative threshold
    Vector2D m_mousePosition{0.0f, 0.0f};

    bool m_isInitialized{false};
    bool m_isShutdown{false};

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    std::optional<size_t> findGamepadIndex(SDL_JoystickID instanceId) const;
    bool openGamepad(SDL_JoystickID instanceId);
    void closeGamepad(SDL_JoystickID instanceId);
    void updateMousePositionFromWindowCoords(float x, float y);
    void clearGamepadState();
    float getGamepadAxisValue(int joy, SDL_GamepadAxis axis) const;
    static float normalizeGamepadAxisValue(Sint16 value, int deadZone);

    InputManager();
};

#endif  // INPUT_MANAGER_HPP
