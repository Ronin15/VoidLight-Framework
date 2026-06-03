# InputManager

**Code:** `include/managers/InputManager.hpp`, `src/managers/InputManager.cpp`

**Singleton Access:** Use `InputManager::Instance()` to access the manager.

## Overview

The InputManager provides centralized input handling for the VoidLight Engine, including keyboard, mouse, and gamepad input. It features automatic coordinate conversion for cross-platform compatibility, event-driven input detection, and seamless integration with the UI system.

## Key Features

- **Cross-Platform Coordinate Conversion**: Mouse events automatically scaled by pixel density for UI accuracy
- **Event-Driven Input**: Efficient key press/release detection with frame-based tracking; key repeat events are ignored for `wasKeyPressed`
- **Command Layer**: Gameplay and menu code can query action commands instead of raw SDL inputs
- **Rebinding and Persistence**: Keyboard/mouse and controller bindings can be captured separately and saved to JSON
- **Gamepad Support**: Multi-controller support with SDL3 gamepad API, normalized axis values in [-1.0, 1.0]
- **Hot-Plug Support**: Gamepads can be connected and disconnected at runtime via `SDL_EVENT_GAMEPAD_ADDED` / `SDL_EVENT_GAMEPAD_REMOVED`
- **Focus Loss Handling**: Keyboard and gamepad state is cleared on window focus loss to prevent stuck inputs
- **Window Event Interaction**: Focus, gamepad, keyboard, mouse, and pixel-density input handling. `GameEngine` owns window/display resize coordination.

## Quick Start

### Basic Input Handling

```cpp
// InputManager is automatically initialized by GameEngine
// Access the singleton instance
InputManager& input = InputManager::Instance();

// Check current key states
if (input.isKeyDown(SDL_SCANCODE_SPACE)) {
    // Space key is currently held down
    player.jump();
}

// Check for key press events (once per press)
if (input.wasKeyPressed(SDL_SCANCODE_RETURN)) {
    // Enter key was just pressed this frame
    confirmAction();
}

// Mouse input
const Vector2D& mousePos = input.getMousePosition();
bool leftClick = input.getMouseButtonState(LEFT);
```

## Coordinate System Integration

The InputManager converts SDL mouse window coordinates into the engine's
pixel-space coordinate system using `SDL_GetWindowPixelDensity(...)`. UI hit
testing and gameplay screen-to-world conversion should use
`getMousePosition()`.

## Input Detection Methods

### Command Input

The action-mapped command layer lets gameplay and menu code query commands:

```cpp
if (input.isCommandPressed(InputManager::Command::OpenInventory)) {
    inventory.toggleInventoryDisplay();
}

if (input.isCommandDown(InputManager::Command::MoveLeft)) {
    // continuous movement
}
```

Command queries:

- `isCommandPressed(Command)` returns a rising edge for this frame.
- `isCommandDown(Command)` returns current command state.
- `isCommandReleased(Command)` returns a falling edge for this frame.

Command state is refreshed once per frame by `GameEngine::handleEvents()` after SDL polling:

```text
clearFrameInput() -> SDL event routing -> refreshCommandState() -> GameState input reads
```

Do not read command edges from worker threads. Worker code should use state captured on the main thread or the lower-level raw input APIs where appropriate.

### Bindings and Rebinding

Bindings are grouped by `DeviceCategory`:

- `KeyboardMouse`
- `Controller`

The Controls tab uses `startRebinding(command, category)` to replace the binding only for the chosen category. Inputs from the opposite category are ignored while capture is active. Use `cancelRebinding()` to abort capture.

Persistence APIs:

```cpp
bool loadBindingsFromFile(const std::string& path);
bool saveBindingsToFile(const std::string& path) const;
void resetBindingsToDefaults();
```

`GameEngine` loads `res/input_bindings.json` during startup. `SettingsMenuState` saves it after applying settings or resetting controls.

### Default Command Map

Default keyboard/mouse bindings:

- movement: `W`, `A`, `S`, `D`
- attack: `F`
- interact: `E`
- inventory: `I`
- pause/menu cancel: `Escape`
- world interact: left mouse button
- zoom: `[` and `]`
- hotbar slots: `1` through `9`
- menu navigation: arrow keys and `Return`

Default controller bindings:

- movement: left stick
- attack: west face button
- interact/menu confirm: south face button
- inventory: north face button
- pause: start
- zoom: D-pad up/down
- menu navigation: D-pad

Intentional gaps:

- `WorldInteract` has no controller default because it requires a screen
  position.
- hotbar slots are keyboard-only by default.

### Keyboard Input

```cpp
// Current state checking (for continuous actions)
bool isWalking = input.isKeyDown(SDL_SCANCODE_W);
bool isRunning = input.isKeyDown(SDL_SCANCODE_LSHIFT);

// Event-based detection (for discrete actions)
if (input.wasKeyPressed(SDL_SCANCODE_TAB)) {
    toggleInventory();
}

// Multiple key combinations
if (input.isKeyDown(SDL_SCANCODE_LCTRL) && input.wasKeyPressed(SDL_SCANCODE_S)) {
    saveGame();
}
```

**Key Detection Features:**
- **Frame-Based Tracking**: `wasKeyPressed()` returns true only once per press
- **State Tracking**: `isKeyDown()` returns true while key is held
- **Automatic Cleanup**: Pressed keys list cleared each frame

### Mouse Input

```cpp
// Mouse position (automatically converted coordinates)
const Vector2D& mousePos = input.getMousePosition();
int mouseX = static_cast<int>(mousePos.getX());
int mouseY = static_cast<int>(mousePos.getY());

// Mouse button states
bool leftPressed = input.getMouseButtonState(LEFT);
bool rightPressed = input.getMouseButtonState(RIGHT);
bool middlePressed = input.getMouseButtonState(MIDDLE);
```

### Gamepad Input

```cpp
// Initialize gamepad support (optional, usually handled automatically)
input.initializeGamePad();

// Get joystick axis values — normalized to [-1.0, 1.0] with dead zone applied
float leftStickX = input.getAxisX(0, 1); // 0 = first gamepad, 1 = left stick
float leftStickY = input.getAxisY(0, 1); // stick 2 = right stick

// Get button state
bool aButton = input.getButtonState(0, 0); // 0 = first gamepad, 0 = A button
```

Gamepads are tracked by `SDL_JoystickID`. Connecting or disconnecting a controller during play is handled automatically — no manual re-initialization is required.

## Window and Display Events

`GameEngine` routes resize/display events and refreshes `FontManager`,
`UIManager`, display refresh rate, and cached pixel dimensions. `InputManager`
handles input events and focus-loss state cleanup.

## API Reference

### Core Methods

```cpp
// Initialization and cleanup
void initializeGamePad();
void clean();
bool isShutdown() const;

// Keyboard input
bool isKeyDown(SDL_Scancode key) const;
bool wasKeyPressed(SDL_Scancode key) const;
void clearFrameInput();

// Command input
bool isCommandPressed(Command c) const;
bool isCommandDown(Command c) const;
bool isCommandReleased(Command c) const;
void refreshCommandState();

// Joystick input — returns normalized [-1.0, 1.0] float with dead zone applied
float getAxisX(int joy, int stick) const;
float getAxisY(int joy, int stick) const;
bool getButtonState(int joy, int buttonNumber) const;

// Mouse input
bool getMouseButtonState(int buttonNumber) const;
const Vector2D& getMousePosition() const;

// Reset mouse button states
void reset();
```

### Mouse Button Constants

```cpp
enum mouse_buttons { LEFT = 0, MIDDLE = 1, RIGHT = 2 };
```

## Best Practices

- Let `GameEngine::handleEvents()` own `clearFrameInput()` and `refreshCommandState()` ordering.
- Use `isKeyDown()` for continuous actions, `wasKeyPressed()` for discrete actions.
- Prefer command queries for gameplay/menu actions that should be rebindable.
- Use `getMousePosition()` and `getMouseButtonState()` for UI and gameplay input.
- Use `initializeGamePad()` if you need to ensure gamepad support is ready.
- Always check for valid indices when querying gamepad state.
- Call `clean()` on shutdown to free resources.

## Integration with Other Systems

The InputManager works seamlessly with:
- **[GameEngine](../core/GameEngine.md)**: Automatic initialization and coordinate system integration
- **[UIManager](../ui/UIManager_Guide.md)**: Coordinate conversion ensures perfect mouse accuracy
- **[FontManager](FontManager.md)**: Window resize events trigger automatic font refresh
- **Game States**: Direct integration for player input and UI interaction

The InputManager serves as the bridge between raw SDL input events and the game's coordinate system, ensuring that all input handling works correctly across platforms and display configurations.
