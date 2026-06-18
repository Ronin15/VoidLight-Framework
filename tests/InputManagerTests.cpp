/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE InputManagerTests
#include <boost/test/unit_test.hpp>

#include <SDL3/SDL.h>
#include "managers/InputManager.hpp"
#include "utils/Vector2D.hpp"
#include <cstdlib>

// Global fixture for SDL and InputManager initialization
struct InputManagerTestFixture {
    InputManagerTestFixture() {
        // Tests only inject SDL events; they do not need a real display.
#if defined(_WIN32)
        _putenv_s("SDL_VIDEODRIVER", "offscreen");
#else
        setenv("SDL_VIDEODRIVER", "offscreen", 1);
#endif
        // Initialize SDL Video subsystem (needed for event processing)
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error("Failed to initialize SDL: " + std::string(SDL_GetError()));
        }

        // Initialize InputManager (singleton)
        // Note: InputManager will be in a clean state for first test
    }

    ~InputManagerTestFixture() {
        // Clean up InputManager
        if (!InputManager::Instance().isShutdown()) {
            InputManager::Instance().clean();
        }

        // Quit SDL
        SDL_Quit();
    }

    // Helper to inject a keyboard event into SDL's event queue
    void injectKeyEvent(SDL_Scancode scancode, bool isDown, bool repeat = false) {
        SDL_Event event;
        SDL_zero(event);
        event.type = isDown ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = scancode;
        event.key.mod = SDL_KMOD_NONE;
        event.key.repeat = repeat;

        SDL_PushEvent(&event);
    }

    // Helper to inject a mouse button event
    void injectMouseButtonEvent(int button, bool isDown, float x, float y) {
        SDL_Event event;
        SDL_zero(event);
        event.type = isDown ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.button = button;
        event.button.x = x;
        event.button.y = y;
        event.button.clicks = 1;

        SDL_PushEvent(&event);
    }

    // Helper to inject a mouse motion event
    void injectMouseMotionEvent(float x, float y) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.x = x;
        event.motion.y = y;
        event.motion.xrel = 0.0f;
        event.motion.yrel = 0.0f;

        SDL_PushEvent(&event);
    }

    void injectGamepadDeviceEvent(Uint32 eventType, SDL_JoystickID instanceId) {
        SDL_Event event;
        SDL_zero(event);
        event.type = eventType;
        event.gdevice.which = instanceId;
        SDL_PushEvent(&event);
    }

    // Helper to clear all pending events
    void clearEventQueue() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Discard all events
        }
    }
};

BOOST_GLOBAL_FIXTURE(InputManagerTestFixture);

// Static helper functions for use in tests
namespace TestHelpers {
    void injectKeyEvent(SDL_Scancode scancode, bool isDown, bool repeat = false) {
        SDL_Event event;
        SDL_zero(event);
        event.type = isDown ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = scancode;
        event.key.mod = SDL_KMOD_NONE;
        event.key.repeat = repeat;
        SDL_PushEvent(&event);
    }

    void injectMouseButtonEvent(int button, bool isDown, float x, float y) {
        SDL_Event event;
        SDL_zero(event);
        event.type = isDown ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.button = button;
        event.button.x = x;
        event.button.y = y;
        event.button.clicks = 1;
        SDL_PushEvent(&event);
    }

    void injectMouseMotionEvent(float x, float y) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.x = x;
        event.motion.y = y;
        event.motion.xrel = 0.0f;
        event.motion.yrel = 0.0f;
        SDL_PushEvent(&event);
    }

    void injectGamepadDeviceEvent(Uint32 eventType, SDL_JoystickID instanceId) {
        SDL_Event event;
        SDL_zero(event);
        event.type = eventType;
        event.gdevice.which = instanceId;
        SDL_PushEvent(&event);
    }

    SDL_JoystickID attachVirtualGamepad() {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.axis_mask =
            (1u << SDL_GAMEPAD_AXIS_LEFTX) |
            (1u << SDL_GAMEPAD_AXIS_LEFTY) |
            (1u << SDL_GAMEPAD_AXIS_RIGHTX) |
            (1u << SDL_GAMEPAD_AXIS_RIGHTY) |
            (1u << SDL_GAMEPAD_AXIS_LEFT_TRIGGER) |
            (1u << SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        desc.button_mask = 0xFFFFFFFFu;
        desc.name = "Test Virtual Gamepad";
        return SDL_AttachVirtualJoystick(&desc);
    }

    void clearEventQueue() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Discard all events
        }
    }

    // Process pending SDL events and route to InputManager handlers
    // This simulates what GameEngine::handleEvents() does after the refactoring
    void processEvents() {
        InputManager& inputMgr = InputManager::Instance();
        inputMgr.clearFrameInput();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_KEY_DOWN:
                    inputMgr.onKeyDown(event);
                    break;
                case SDL_EVENT_KEY_UP:
                    inputMgr.onKeyUp(event);
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    inputMgr.onMouseMove(event);
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    inputMgr.onMouseButtonDown(event);
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    inputMgr.onMouseButtonUp(event);
                    break;
                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    inputMgr.onGamepadAxisMove(event);
                    break;
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    inputMgr.onGamepadButtonDown(event);
                    break;
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                    inputMgr.onGamepadButtonUp(event);
                    break;
                case SDL_EVENT_GAMEPAD_ADDED:
                    inputMgr.onGamepadAdded(event);
                    break;
                case SDL_EVENT_GAMEPAD_REMOVED:
                    inputMgr.onGamepadRemoved(event);
                    break;
                case SDL_EVENT_GAMEPAD_REMAPPED:
                    inputMgr.onGamepadRemapped(event);
                    break;
                default:
                    break;
            }
        }
    }
}

// ============================================================================
// KEYBOARD STATE TRACKING TESTS
// Note: isKeyDown() relies on SDL_GetKeyboardState() which only tracks real
// hardware input and cannot be faked with injected events. We test
// wasKeyPressed() which uses InputManager's own m_pressedThisFrame tracking.
// ============================================================================

BOOST_AUTO_TEST_SUITE(KeyboardStateTests)

BOOST_AUTO_TEST_CASE(TestKeyPressedDetection) {
    TestHelpers::clearEventQueue();

    // Initially, no keys should be pressed this frame
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_A));

    // Inject key down event
    TestHelpers::injectKeyEvent(SDL_SCANCODE_A, true);
    TestHelpers::processEvents();

    // Key should be detected as pressed this frame
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_A));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestKeyPressedOnlyOncePerPress) {
    TestHelpers::clearEventQueue();

    // Inject key down event
    TestHelpers::injectKeyEvent(SDL_SCANCODE_B, true);
    TestHelpers::processEvents();

    // Key should be detected as pressed this frame
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_B));

    // On next frame, wasKeyPressed should return false (only true on press frame)
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_B));

    // Still false on subsequent frames
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_B));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestKeyPressAfterRelease) {
    TestHelpers::clearEventQueue();

    // Press key
    TestHelpers::injectKeyEvent(SDL_SCANCODE_C, true);
    TestHelpers::processEvents();
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_C));

    // Release key and update
    TestHelpers::injectKeyEvent(SDL_SCANCODE_C, false);
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_C));

    // Press again - should be detected as new press
    TestHelpers::injectKeyEvent(SDL_SCANCODE_C, true);
    TestHelpers::processEvents();
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_C));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestKeyRepeatIgnoredForPressedThisFrame) {
    TestHelpers::clearEventQueue();

    TestHelpers::injectKeyEvent(SDL_SCANCODE_J, true, true);
    TestHelpers::processEvents();

    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_J));

    TestHelpers::injectKeyEvent(SDL_SCANCODE_J, true, false);
    TestHelpers::processEvents();

    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_J));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestMultipleKeysSimultaneous) {
    TestHelpers::clearEventQueue();

    // Press multiple keys in same frame
    TestHelpers::injectKeyEvent(SDL_SCANCODE_W, true);
    TestHelpers::injectKeyEvent(SDL_SCANCODE_A, true);
    TestHelpers::injectKeyEvent(SDL_SCANCODE_S, true);
    TestHelpers::processEvents();

    // All keys should be detected as pressed this frame
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_W));
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_A));
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_S));

    // Next frame, none should be "pressed this frame"
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_W));
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_A));
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_S));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestPressedClearedAcrossFrames) {
    TestHelpers::clearEventQueue();

    // Press key
    TestHelpers::injectKeyEvent(SDL_SCANCODE_SPACE, true);
    TestHelpers::processEvents();

    // Key pressed this frame
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_SPACE));

    // Frame 2: Not "pressed this frame" anymore
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_SPACE));

    // Frame 3: Still not pressed this frame
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_SPACE));

    // Release
    TestHelpers::injectKeyEvent(SDL_SCANCODE_SPACE, false);
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_SPACE));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// MOUSE STATE TRACKING TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(MouseStateTests)

BOOST_AUTO_TEST_CASE(TestMouseButtonDown) {
    TestHelpers::clearEventQueue();

    // Initially, mouse button should not be down
    BOOST_CHECK(!InputManager::Instance().getMouseButtonState(LEFT));

    // Inject left mouse button down
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_LEFT, true, 100.0f, 200.0f);
    TestHelpers::processEvents();

    // Button should be detected as down
    BOOST_CHECK(InputManager::Instance().getMouseButtonState(LEFT));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestMouseButtonRelease) {
    TestHelpers::clearEventQueue();

    // Press button
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_LEFT, true, 100.0f, 200.0f);
    TestHelpers::processEvents();
    BOOST_CHECK(InputManager::Instance().getMouseButtonState(LEFT));

    // Release button
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_LEFT, false, 100.0f, 200.0f);
    TestHelpers::processEvents();

    // Button should no longer be down
    BOOST_CHECK(!InputManager::Instance().getMouseButtonState(LEFT));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestMultipleMouseButtons) {
    TestHelpers::clearEventQueue();

    // Press left and right buttons
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_LEFT, true, 100.0f, 200.0f);
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_RIGHT, true, 100.0f, 200.0f);
    TestHelpers::processEvents();

    // Both should be detected
    BOOST_CHECK(InputManager::Instance().getMouseButtonState(LEFT));
    BOOST_CHECK(InputManager::Instance().getMouseButtonState(RIGHT));

    // Middle should not be down
    BOOST_CHECK(!InputManager::Instance().getMouseButtonState(MIDDLE));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestMousePositionTracking) {
    TestHelpers::clearEventQueue();

    // Inject mouse motion event
    TestHelpers::injectMouseMotionEvent(150.0f, 250.0f);
    TestHelpers::processEvents();

    // Check position
    const Vector2D& pos = InputManager::Instance().getMousePosition();
    BOOST_CHECK_EQUAL(pos.getX(), 150.0f);
    BOOST_CHECK_EQUAL(pos.getY(), 250.0f);

    // Move mouse again
    TestHelpers::injectMouseMotionEvent(300.0f, 400.0f);
    TestHelpers::processEvents();

    const Vector2D& pos2 = InputManager::Instance().getMousePosition();
    BOOST_CHECK_EQUAL(pos2.getX(), 300.0f);
    BOOST_CHECK_EQUAL(pos2.getY(), 400.0f);

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestMouseButtonWithPosition) {
    TestHelpers::clearEventQueue();

    // Press button at specific position
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_LEFT, true, 123.0f, 456.0f);
    TestHelpers::processEvents();

    // Verify button state
    BOOST_CHECK(InputManager::Instance().getMouseButtonState(LEFT));

    const Vector2D& pos = InputManager::Instance().getMousePosition();
    BOOST_CHECK_EQUAL(pos.getX(), 123.0f);
    BOOST_CHECK_EQUAL(pos.getY(), 456.0f);

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// STATE TRANSITION TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(StateTransitionTests)

BOOST_AUTO_TEST_CASE(TestPressedHeldReleasedCycle) {
    TestHelpers::clearEventQueue();

    // Frame 1: Press
    TestHelpers::injectKeyEvent(SDL_SCANCODE_E, true);
    TestHelpers::processEvents();
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_E));

    // Frame 2: Held (not pressed this frame)
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_E));

    // Frame 3: Still held (still not pressed)
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_E));

    // Frame 4: Released (not pressed on release frame)
    TestHelpers::injectKeyEvent(SDL_SCANCODE_E, false);
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_E));

    // Frame 5: Still released
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_E));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestRapidPressRelease) {
    TestHelpers::clearEventQueue();

    // Press and release in same frame
    TestHelpers::injectKeyEvent(SDL_SCANCODE_F, true);
    TestHelpers::injectKeyEvent(SDL_SCANCODE_F, false);
    TestHelpers::processEvents();

    // wasKeyPressed should still be true (detected the press)
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_F));

    // Next frame, should not be pressed
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_F));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestMultipleUpdatesEmptyQueue) {
    TestHelpers::clearEventQueue();

    // Press key
    TestHelpers::injectKeyEvent(SDL_SCANCODE_G, true);
    TestHelpers::processEvents();
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_G));

    // Multiple updates with no events should keep wasKeyPressed false
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_G));

    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_G));

    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_G));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestResetMouseButtons) {
    TestHelpers::clearEventQueue();

    // Press mouse buttons
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_LEFT, true, 100.0f, 100.0f);
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_RIGHT, true, 100.0f, 100.0f);
    TestHelpers::processEvents();

    BOOST_CHECK(InputManager::Instance().getMouseButtonState(LEFT));
    BOOST_CHECK(InputManager::Instance().getMouseButtonState(RIGHT));

    // Reset should clear mouse button states
    InputManager::Instance().reset();

    BOOST_CHECK(!InputManager::Instance().getMouseButtonState(LEFT));
    BOOST_CHECK(!InputManager::Instance().getMouseButtonState(RIGHT));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

BOOST_AUTO_TEST_SUITE(EdgeCaseTests)

BOOST_AUTO_TEST_CASE(TestSameKeyPressedMultipleTimes) {
    TestHelpers::clearEventQueue();

    // Press same key multiple times in one frame
    TestHelpers::injectKeyEvent(SDL_SCANCODE_H, true);
    TestHelpers::injectKeyEvent(SDL_SCANCODE_H, true);
    TestHelpers::injectKeyEvent(SDL_SCANCODE_H, true);
    TestHelpers::processEvents();

    // Should be detected as pressed this frame (deduplicated)
    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_H));

    // Next frame should not be pressed
    TestHelpers::processEvents();
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_H));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestAlternatingKeyStates) {
    TestHelpers::clearEventQueue();

    // Alternate press/release over multiple frames
    for (int i = 0; i < 5; ++i) {
        // Press
        TestHelpers::injectKeyEvent(SDL_SCANCODE_I, true);
        TestHelpers::processEvents();
        BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_I));

        // Release
        TestHelpers::injectKeyEvent(SDL_SCANCODE_I, false);
        TestHelpers::processEvents();
        BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_I));
    }

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestNoEventsProcessing) {
    TestHelpers::clearEventQueue();

    // Call update with no events
    TestHelpers::processEvents();

    // Should not crash, no keys should be pressed this frame
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_A));
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_B));
    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_C));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestMousePositionWithoutMotionEvent) {
    TestHelpers::clearEventQueue();

    // Get position without any motion events
    const Vector2D& pos = InputManager::Instance().getMousePosition();

    // Should return some position (default or last known)
    // Just verify it doesn't crash and returns finite values
    BOOST_CHECK(std::isfinite(pos.getX()));
    BOOST_CHECK(std::isfinite(pos.getY()));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestUnknownGamepadEventsDoNotCreateState) {
    TestHelpers::clearEventQueue();

    SDL_Event axisEvent;
    SDL_zero(axisEvent);
    axisEvent.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    axisEvent.gaxis.which = 999999u;
    axisEvent.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    axisEvent.gaxis.value = SDL_JOYSTICK_AXIS_MAX;
    InputManager::Instance().onGamepadAxisMove(axisEvent);

    SDL_Event buttonEvent;
    SDL_zero(buttonEvent);
    buttonEvent.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    buttonEvent.gbutton.which = 999999u;
    buttonEvent.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    InputManager::Instance().onGamepadButtonDown(buttonEvent);

    BOOST_CHECK_EQUAL(InputManager::Instance().getAxisX(0, 1), 0.0f);
    BOOST_CHECK(!InputManager::Instance().getButtonState(0, SDL_GAMEPAD_BUTTON_SOUTH));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestVirtualGamepadLifecycle) {
    TestHelpers::clearEventQueue();

    const SDL_JoystickID instanceId = TestHelpers::attachVirtualGamepad();
    BOOST_REQUIRE_NE(instanceId, 0u);

    TestHelpers::injectGamepadDeviceEvent(SDL_EVENT_GAMEPAD_ADDED, instanceId);
    TestHelpers::processEvents();

    SDL_Event axisEvent;
    SDL_zero(axisEvent);
    axisEvent.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    axisEvent.gaxis.which = instanceId;
    axisEvent.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    axisEvent.gaxis.value = SDL_JOYSTICK_AXIS_MAX;
    InputManager::Instance().onGamepadAxisMove(axisEvent);

    BOOST_CHECK_GT(InputManager::Instance().getAxisX(0, 1), 0.0f);

    TestHelpers::injectGamepadDeviceEvent(SDL_EVENT_GAMEPAD_REMOVED, instanceId);
    TestHelpers::processEvents();

    BOOST_CHECK_EQUAL(InputManager::Instance().getAxisX(0, 1), 0.0f);

    SDL_DetachVirtualJoystick(instanceId);
    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_CASE(TestFocusLostClearsCachedInputState) {
    TestHelpers::clearEventQueue();

    TestHelpers::injectKeyEvent(SDL_SCANCODE_K, true);
    TestHelpers::injectMouseButtonEvent(SDL_BUTTON_LEFT, true, 42.0f, 84.0f);
    TestHelpers::processEvents();

    BOOST_CHECK(InputManager::Instance().wasKeyPressed(SDL_SCANCODE_K));
    BOOST_CHECK(InputManager::Instance().getMouseButtonState(LEFT));

    InputManager::Instance().onFocusLost();

    BOOST_CHECK(!InputManager::Instance().wasKeyPressed(SDL_SCANCODE_K));
    BOOST_CHECK(!InputManager::Instance().getMouseButtonState(LEFT));

    TestHelpers::clearEventQueue();
}

BOOST_AUTO_TEST_SUITE_END()
