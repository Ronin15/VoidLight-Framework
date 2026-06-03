/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE InputManagerCommandTests
#include <boost/test/unit_test.hpp>

#include <SDL3/SDL.h>
#include "managers/InputManager.hpp"
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

// =============================================================================
// Global fixture — SDL + InputManager lifecycle
// =============================================================================

struct CommandTestFixture
{
    CommandTestFixture()
    {
        setenv("SDL_VIDEODRIVER", "offscreen", 1);
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
            throw std::runtime_error(
                std::string("SDL_Init failed: ") + SDL_GetError());
        }

        InputManager& mgr = InputManager::Instance();
        if (!mgr.isInitialized()) {
            if (!mgr.init()) {
                throw std::runtime_error("InputManager::init() failed");
            }
        }
    }

    ~CommandTestFixture()
    {
        if (!InputManager::Instance().isShutdown()) {
            InputManager::Instance().clean();
        }
        SDL_Quit();
    }
};

BOOST_GLOBAL_FIXTURE(CommandTestFixture);

// =============================================================================
// Test helpers
// =============================================================================

namespace TestHelpers
{
    void clearEventQueue()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {}
    }

    // Simulate one full input frame: clear pressed keys, drain SDL event queue,
    // route events to InputManager handlers, then resolve command state.
    // Mirrors GameEngine::handleEvents() behaviour.
    void processFrame()
    {
        InputManager& mgr = InputManager::Instance();
        mgr.clearFrameInput();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_KEY_DOWN:      mgr.onKeyDown(event);          break;
                case SDL_EVENT_KEY_UP:        mgr.onKeyUp(event);            break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: mgr.onMouseButtonDown(event); break;
                case SDL_EVENT_MOUSE_BUTTON_UP:   mgr.onMouseButtonUp(event);   break;
                case SDL_EVENT_GAMEPAD_AXIS_MOTION: mgr.onGamepadAxisMove(event); break;
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN: mgr.onGamepadButtonDown(event); break;
                case SDL_EVENT_GAMEPAD_BUTTON_UP:   mgr.onGamepadButtonUp(event);   break;
                case SDL_EVENT_GAMEPAD_ADDED:       mgr.onGamepadAdded(event);      break;
                case SDL_EVENT_GAMEPAD_REMOVED:     mgr.onGamepadRemoved(event);    break;
                case SDL_EVENT_GAMEPAD_REMAPPED:    mgr.onGamepadRemapped(event);   break;
                default: break;
            }
        }

        mgr.refreshCommandState();
    }

    void injectKeyDown(SDL_Scancode sc)
    {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.scancode = sc;
        e.key.repeat = false;
        SDL_PushEvent(&e);
    }

    void injectKeyUp(SDL_Scancode sc)
    {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_KEY_UP;
        e.key.scancode = sc;
        SDL_PushEvent(&e);
    }

    void injectGamepadDeviceEvent(SDL_EventType eventType, SDL_JoystickID instanceId)
    {
        SDL_Event e;
        SDL_zero(e);
        e.type = eventType;
        e.gdevice.which = instanceId;
        SDL_PushEvent(&e);
    }

    void injectGamepadAxisMotion(SDL_JoystickID instanceId, SDL_GamepadAxis axis, Sint16 value)
    {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        e.gaxis.which = instanceId;
        e.gaxis.axis = axis;
        e.gaxis.value = value;
        SDL_PushEvent(&e);
    }

    SDL_JoystickID attachVirtualGamepad()
    {
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
        desc.name = "Command Test Virtual Gamepad";
        return SDL_AttachVirtualJoystick(&desc);
    }

    struct ScopedVirtualGamepad
    {
        SDL_JoystickID instanceId{0};

        ScopedVirtualGamepad()
            : instanceId(attachVirtualGamepad())
        {
            if (instanceId != 0) {
                injectGamepadDeviceEvent(SDL_EVENT_GAMEPAD_ADDED, instanceId);
                processFrame();
            }
        }

        ~ScopedVirtualGamepad()
        {
            if (instanceId != 0) {
                injectGamepadDeviceEvent(SDL_EVENT_GAMEPAD_REMOVED, instanceId);
                processFrame();
                SDL_DetachVirtualJoystick(instanceId);
                clearEventQueue();
            }
        }

        ScopedVirtualGamepad(const ScopedVirtualGamepad&) = delete;
        ScopedVirtualGamepad& operator=(const ScopedVirtualGamepad&) = delete;
    };

    // Resets bindings to defaults and clears any queued events for a clean test state
    void resetState()
    {
        clearEventQueue();
        InputManager::Instance().resetBindingsToDefaults();
        // Two empty frames to flush any lingering edge state
        processFrame();
        processFrame();
    }
}

// =============================================================================
// 1. Default bindings — isCommandDown reflects a simulated key-down
// =============================================================================

BOOST_AUTO_TEST_SUITE(DefaultBindings)

BOOST_AUTO_TEST_CASE(AttackLightDefaultKeyF)
{
    // isKeyDown() reflects real hardware state which cannot be injected in
    // headless tests. We test through the command layer by manipulating bindings
    // to use a source we can control: m_pressedThisFrame via wasKeyPressed.
    // Verify that default bindings include keyboard F for AttackLight.
    TestHelpers::resetState();

    auto& mgr = InputManager::Instance();
    auto bindings = mgr.getBindings(InputManager::Command::AttackLight);

    bool foundF = false;
    for (const auto& b : bindings) {
        if (b.source == InputManager::InputSource::Keyboard &&
            b.code == static_cast<int>(SDL_SCANCODE_F)) {
            foundF = true;
            break;
        }
    }
    BOOST_CHECK(foundF);
}

BOOST_AUTO_TEST_CASE(DefaultBindingsCoverAllMovement)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    using C = InputManager::Command;
    for (auto cmd : {C::MoveUp, C::MoveDown, C::MoveLeft, C::MoveRight}) {
        BOOST_CHECK_MESSAGE(!mgr.getBindings(cmd).empty(),
            std::format("Expected bindings for command {}", static_cast<int>(cmd)));
    }
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 2. Rising / falling edge fire exactly once
// =============================================================================

BOOST_AUTO_TEST_SUITE(EdgeDetection)

BOOST_AUTO_TEST_CASE(PressedRisingEdgeOnce)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    // wasKeyPressed tracks m_pressedThisFrame (populated by onKeyDown).
    // Inject a key press and process the frame — the key should appear exactly
    // once in that frame's pressed list.
    TestHelpers::injectKeyDown(SDL_SCANCODE_F);
    TestHelpers::processFrame();

    // Within the frame the key was injected, wasKeyPressed is true
    BOOST_CHECK(mgr.wasKeyPressed(SDL_SCANCODE_F));

    // Next frame without re-injecting: clearFrameInput() at frame start wipes it
    TestHelpers::processFrame();
    BOOST_CHECK(!mgr.wasKeyPressed(SDL_SCANCODE_F));

    // Still false on subsequent frames
    TestHelpers::processFrame();
    BOOST_CHECK(!mgr.wasKeyPressed(SDL_SCANCODE_F));
}

BOOST_AUTO_TEST_CASE(CommandIsReleasedOnlyWhenPreviouslyDown)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    // With no hardware input active, isCommandReleased must be false on first frame
    TestHelpers::processFrame();
    BOOST_CHECK(!mgr.isCommandReleased(InputManager::Command::AttackLight));
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 3. Adding a second binding — either binding activates the command
// =============================================================================

BOOST_AUTO_TEST_SUITE(MultipleBindings)

BOOST_AUTO_TEST_CASE(TwoBindingsBothPresent)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    mgr.clearBindings(InputManager::Command::AttackLight);
    mgr.addBinding(InputManager::Command::AttackLight,
                   {InputManager::InputSource::Keyboard, SDL_SCANCODE_F});
    mgr.addBinding(InputManager::Command::AttackLight,
                   {InputManager::InputSource::Keyboard, SDL_SCANCODE_J});

    auto bindings = mgr.getBindings(InputManager::Command::AttackLight);
    BOOST_REQUIRE_EQUAL(bindings.size(), 2u);
    BOOST_CHECK_EQUAL(bindings[0].code, static_cast<int>(SDL_SCANCODE_F));
    BOOST_CHECK_EQUAL(bindings[1].code, static_cast<int>(SDL_SCANCODE_J));
}

BOOST_AUTO_TEST_CASE(ClearBindingsWorks)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    mgr.addBinding(InputManager::Command::ZoomIn,
                   {InputManager::InputSource::Keyboard, SDL_SCANCODE_KP_PLUS});
    BOOST_CHECK(!mgr.getBindings(InputManager::Command::ZoomIn).empty());

    mgr.clearBindings(InputManager::Command::ZoomIn);
    BOOST_CHECK(mgr.getBindings(InputManager::Command::ZoomIn).empty());

    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 4. JSON round-trip: save → resetBindingsToDefaults → load → bindings match
// =============================================================================

BOOST_AUTO_TEST_SUITE(JsonRoundTrip)

BOOST_AUTO_TEST_CASE(SaveLoadRoundTrip)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    // Capture the default AttackLight binding for comparison after reload
    std::vector<InputManager::InputBinding> originalBindings;
    for (const auto& b : mgr.getBindings(InputManager::Command::AttackLight)) {
        originalBindings.push_back(b);
    }

    const std::string tmpPath = "/tmp/voidlight_test_bindings.json";

    BOOST_REQUIRE(mgr.saveBindingsToFile(tmpPath));

    // Modify a binding then reload from file
    mgr.clearBindings(InputManager::Command::AttackLight);
    mgr.addBinding(InputManager::Command::AttackLight,
                   {InputManager::InputSource::Keyboard, SDL_SCANCODE_G});

    BOOST_REQUIRE(mgr.loadBindingsFromFile(tmpPath));

    auto reloaded = mgr.getBindings(InputManager::Command::AttackLight);
    BOOST_REQUIRE_EQUAL(reloaded.size(), originalBindings.size());
    for (size_t i = 0; i < originalBindings.size(); ++i) {
        BOOST_CHECK_EQUAL(static_cast<int>(reloaded[i].source),
                          static_cast<int>(originalBindings[i].source));
        BOOST_CHECK_EQUAL(reloaded[i].code, originalBindings[i].code);
    }

    std::filesystem::remove(tmpPath);
    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 5. Rebind capture — inject key via pressedThisFrame, verify binding update
// =============================================================================

BOOST_AUTO_TEST_SUITE(RebindCapture)

BOOST_AUTO_TEST_CASE(CaptureNewKeyBinding)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    mgr.clearBindings(InputManager::Command::AttackLight);
    mgr.addBinding(InputManager::Command::AttackLight,
                   {InputManager::InputSource::Keyboard, SDL_SCANCODE_F});

    // Start rebinding the keyboard/mouse slot
    mgr.startRebinding(InputManager::Command::AttackLight,
                       InputManager::DeviceCategory::KeyboardMouse);
    BOOST_CHECK(mgr.isRebinding());
    BOOST_CHECK(mgr.getRebindingCommand() == InputManager::Command::AttackLight);

    // Inject a key press for SDL_SCANCODE_J — this populates m_pressedThisFrame
    // via onKeyDown, which captureRebind() reads.
    TestHelpers::injectKeyDown(SDL_SCANCODE_J);
    // refreshCommandState() dispatches to captureRebind() when rebinding is active,
    // mirroring the single call path from GameEngine::handleEvents().
    TestHelpers::processFrame();

    BOOST_CHECK(!mgr.isRebinding());

    auto bindings = mgr.getBindings(InputManager::Command::AttackLight);
    BOOST_REQUIRE(!bindings.empty());
    BOOST_CHECK_EQUAL(bindings[0].code, static_cast<int>(SDL_SCANCODE_J));
    BOOST_CHECK(bindings[0].source == InputManager::InputSource::Keyboard);

    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_CASE(EscCancelsRebind)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    mgr.startRebinding(InputManager::Command::Pause,
                       InputManager::DeviceCategory::KeyboardMouse);
    BOOST_CHECK(mgr.isRebinding());

    // Inject ESC — captureRebind() is driven via refreshCommandState() inside processFrame()
    TestHelpers::injectKeyDown(SDL_SCANCODE_ESCAPE);
    TestHelpers::processFrame();

    BOOST_CHECK(!mgr.isRebinding());

    // Binding should be unchanged (still has default ESC keyboard binding)
    auto bindings = mgr.getBindings(InputManager::Command::Pause);
    bool hasEsc = false;
    for (const auto& b : bindings) {
        if (b.source == InputManager::InputSource::Keyboard &&
            b.code == static_cast<int>(SDL_SCANCODE_ESCAPE)) {
            hasEsc = true;
        }
    }
    BOOST_CHECK(hasEsc);
}

// Captures the core commitBinding() invariant: rebinding one device category
// must leave the other category's binding untouched. Regression guard for a
// class of bugs where "replace-in-category" silently clobbers cross-category
// bindings.
BOOST_AUTO_TEST_CASE(KeyboardRebindPreservesControllerBinding)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    using C  = InputManager::Command;
    using DC = InputManager::DeviceCategory;

    // AttackLight has defaults in both categories (keyboard + GamepadButton WEST).
    auto controllerBefore = mgr.getBindingForCategory(C::AttackLight, DC::Controller);
    BOOST_REQUIRE(controllerBefore.has_value());
    BOOST_REQUIRE(mgr.getBindingForCategory(C::AttackLight, DC::KeyboardMouse).has_value());

    // Rebind the keyboard slot to a new scancode.
    mgr.startRebinding(C::AttackLight, DC::KeyboardMouse);
    BOOST_REQUIRE(mgr.isRebinding());

    TestHelpers::injectKeyDown(SDL_SCANCODE_K);
    TestHelpers::processFrame();

    BOOST_CHECK(!mgr.isRebinding());

    // Keyboard slot updated to the new key.
    auto kbdAfter = mgr.getBindingForCategory(C::AttackLight, DC::KeyboardMouse);
    BOOST_REQUIRE(kbdAfter.has_value());
    BOOST_CHECK(kbdAfter->source == InputManager::InputSource::Keyboard);
    BOOST_CHECK_EQUAL(kbdAfter->code, static_cast<int>(SDL_SCANCODE_K));

    // Controller slot must be unchanged.
    auto controllerAfter = mgr.getBindingForCategory(C::AttackLight, DC::Controller);
    BOOST_REQUIRE(controllerAfter.has_value());
    BOOST_CHECK(controllerAfter->source == controllerBefore->source);
    BOOST_CHECK_EQUAL(controllerAfter->code, controllerBefore->code);

    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_CASE(BindingSnapshotRestoreRevertsRebindAndCancelsCapture)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    using C = InputManager::Command;
    using DC = InputManager::DeviceCategory;

    const auto snapshot = mgr.captureBindings();
    auto original = mgr.getBindingForCategory(C::AttackLight, DC::KeyboardMouse);
    BOOST_REQUIRE(original.has_value());

    mgr.startRebinding(C::AttackLight, DC::KeyboardMouse);
    BOOST_REQUIRE(mgr.isRebinding());

    TestHelpers::injectKeyDown(SDL_SCANCODE_K);
    TestHelpers::processFrame();
    BOOST_REQUIRE(!mgr.isRebinding());

    auto rebound = mgr.getBindingForCategory(C::AttackLight, DC::KeyboardMouse);
    BOOST_REQUIRE(rebound.has_value());
    BOOST_CHECK_EQUAL(rebound->code, static_cast<int>(SDL_SCANCODE_K));

    mgr.startRebinding(C::Pause, DC::KeyboardMouse);
    BOOST_REQUIRE(mgr.isRebinding());

    mgr.restoreBindings(snapshot);

    BOOST_CHECK(!mgr.isRebinding());
    auto restored = mgr.getBindingForCategory(C::AttackLight, DC::KeyboardMouse);
    BOOST_REQUIRE(restored.has_value());
    BOOST_CHECK(restored->source == original->source);
    BOOST_CHECK_EQUAL(restored->code, original->code);

    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 6. Malformed JSON → falls back to defaults without crashing
// =============================================================================

BOOST_AUTO_TEST_SUITE(MalformedJson)

BOOST_AUTO_TEST_CASE(GarbageJsonKeepsDefaults)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    const std::string tmpPath = "/tmp/voidlight_bad_bindings.json";

    {
        std::ofstream f(tmpPath);
        f << "{ this is not valid JSON !!! }";
    }

    // Should return false but not crash; bindings remain at defaults
    bool result = mgr.loadBindingsFromFile(tmpPath);
    BOOST_CHECK(!result);

    // Default AttackLight binding must still be intact
    auto bindings = mgr.getBindings(InputManager::Command::AttackLight);
    bool hasF = false;
    for (const auto& b : bindings) {
        if (b.source == InputManager::InputSource::Keyboard &&
            b.code == static_cast<int>(SDL_SCANCODE_F)) {
            hasF = true;
        }
    }
    BOOST_CHECK(hasF);

    std::filesystem::remove(tmpPath);
}

BOOST_AUTO_TEST_CASE(MissingFileKeepsDefaults)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    bool result = mgr.loadBindingsFromFile("/tmp/voidlight_nonexistent_XYZZY.json");
    BOOST_CHECK(!result);

    // Defaults must still be present
    BOOST_CHECK(!mgr.getBindings(InputManager::Command::MoveUp).empty());
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 7. Schema version validation — bad/missing version must be rejected
// =============================================================================

BOOST_AUTO_TEST_SUITE(SchemaVersionValidation)

BOOST_AUTO_TEST_CASE(SchemaVersionTwoIsRejected)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    const std::string tmpPath = "/tmp/voidlight_v2_bindings.json";
    {
        std::ofstream f(tmpPath);
        f << "{\n";
        f << "  \"schema_version\": 2,\n";
        f << "  \"commands\": {}\n";
        f << "}\n";
    }

    // loadBindingsFromFile must refuse a schema_version it doesn't recognise
    bool result = mgr.loadBindingsFromFile(tmpPath);
    BOOST_CHECK(!result);

    // Bindings must remain at defaults after the rejection
    auto bindings = mgr.getBindings(InputManager::Command::AttackLight);
    bool hasF = false;
    for (const auto& b : bindings) {
        if (b.source == InputManager::InputSource::Keyboard &&
            b.code == static_cast<int>(SDL_SCANCODE_F)) {
            hasF = true;
        }
    }
    BOOST_CHECK(hasF);

    std::filesystem::remove(tmpPath);
}

BOOST_AUTO_TEST_CASE(MissingSchemaVersionIsRejected)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    const std::string tmpPath = "/tmp/voidlight_noschema_bindings.json";
    {
        std::ofstream f(tmpPath);
        f << "{\n";
        f << "  \"commands\": {}\n";
        f << "}\n";
    }

    bool result = mgr.loadBindingsFromFile(tmpPath);
    BOOST_CHECK(!result);

    // Defaults must be intact
    BOOST_CHECK(!mgr.getBindings(InputManager::Command::MoveUp).empty());

    std::filesystem::remove(tmpPath);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 8. Gamepad held-input at rebind start must not be captured immediately
// =============================================================================

BOOST_AUTO_TEST_SUITE(GamepadRebindEdgeDetection)

BOOST_AUTO_TEST_CASE(StartRebindingPrimesPrevStateWithoutSpuriousCapture)
{
    // Without a real gamepad open (m_gamepads is empty in headless), we cannot
    // drive the actual held-button path; injecting SDL_EVENT_GAMEPAD_BUTTON_DOWN
    // with which=0 is a no-op because onGamepadButtonDown() filters on open
    // gamepads. What this test DOES verify: startRebinding() primes the prev-
    // state arrays and a subsequent refreshCommandState() (→ captureRebind())
    // does not spuriously complete the rebind when no rising edge occurred.
    // For real held-button coverage, a fixture that opens a mock SDL_Gamepad
    // is required — see voidlight-test-suite-generator TODO.

    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    // Simulate gamepad button A (index 0) being held at the moment we start rebinding.
    // We inject a button-down SDL event and route it so the internal state is set.
    {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        e.gbutton.which = 0;   // instance id 0 — may not match real hardware
        e.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
        // Push and drain so onGamepadButtonDown is called if a gamepad is open.
        // Without a real gamepad, m_gamepads will be empty, so the call is a no-op.
        // The real guarded behaviour is tested via the prev-state arrays which
        // startRebinding() fills from whatever m_gamepads contains at call time.
        // This test therefore validates that startRebinding() primes the prev-arrays
        // and that subsequent refreshCommandState() (→ captureRebind()) doesn't
        // spuriously capture when no actual rising edge occurred.
        mgr.onGamepadButtonDown(e);
    }

    mgr.startRebinding(InputManager::Command::Pause,
                       InputManager::DeviceCategory::Controller);
    BOOST_CHECK(mgr.isRebinding());

    // One frame with no keyboard/mouse input and no new gamepad rising edge
    TestHelpers::clearEventQueue();
    mgr.clearFrameInput();
    mgr.refreshCommandState();

    // Without a rising edge the rebind must still be active
    BOOST_CHECK(mgr.isRebinding());

    // Cleanup: cancel and verify
    mgr.cancelRebinding();
    BOOST_CHECK(!mgr.isRebinding());

    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_CASE(TriggerAxisPositiveBindingActivatesCommand)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    TestHelpers::ScopedVirtualGamepad gamepad;
    BOOST_REQUIRE_NE(gamepad.instanceId, 0u);
    BOOST_REQUIRE(mgr.isGamepadConnected());

    using C = InputManager::Command;
    using S = InputManager::InputSource;

    mgr.clearBindings(C::AttackLight);
    mgr.addBinding(C::AttackLight,
                   {S::GamepadAxisPositive, SDL_GAMEPAD_AXIS_LEFT_TRIGGER});

    TestHelpers::injectGamepadAxisMotion(
        gamepad.instanceId, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
    TestHelpers::processFrame();

    BOOST_CHECK(mgr.isCommandPressed(C::AttackLight));
    BOOST_CHECK(mgr.isCommandDown(C::AttackLight));

    mgr.clearBindings(C::AttackLight);
    mgr.addBinding(C::AttackLight,
                   {S::GamepadAxisNegative, SDL_GAMEPAD_AXIS_LEFT_TRIGGER});
    TestHelpers::processFrame();

    BOOST_CHECK(!mgr.isCommandDown(C::AttackLight));

    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_CASE(TriggerAxisCanBeCapturedForControllerRebind)
{
    TestHelpers::resetState();
    auto& mgr = InputManager::Instance();

    TestHelpers::ScopedVirtualGamepad gamepad;
    BOOST_REQUIRE_NE(gamepad.instanceId, 0u);
    BOOST_REQUIRE(mgr.isGamepadConnected());

    using C = InputManager::Command;
    using DC = InputManager::DeviceCategory;
    using S = InputManager::InputSource;

    mgr.startRebinding(C::WorldInteract, DC::Controller);
    BOOST_REQUIRE(mgr.isRebinding());

    TestHelpers::injectGamepadAxisMotion(
        gamepad.instanceId, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
    TestHelpers::processFrame();

    BOOST_CHECK(!mgr.isRebinding());

    auto binding = mgr.getBindingForCategory(C::WorldInteract, DC::Controller);
    BOOST_REQUIRE(binding.has_value());
    BOOST_CHECK(binding->source == S::GamepadAxisPositive);
    BOOST_CHECK_EQUAL(binding->code, static_cast<int>(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

    mgr.resetBindingsToDefaults();
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// 9. describeBinding and commandDisplayName return non-empty strings
// =============================================================================

BOOST_AUTO_TEST_SUITE(UIHelpers)

BOOST_AUTO_TEST_CASE(DescribeBindingKeyboard)
{
    auto& mgr = InputManager::Instance();
    InputManager::InputBinding b{InputManager::InputSource::Keyboard, SDL_SCANCODE_F};
    BOOST_CHECK(!mgr.describeBinding(b).empty());
}

BOOST_AUTO_TEST_CASE(DescribeBindingMouseButton)
{
    auto& mgr = InputManager::Instance();
    InputManager::InputBinding b{InputManager::InputSource::MouseButton, LEFT};
    std::string desc = mgr.describeBinding(b);
    BOOST_CHECK(!desc.empty());
    BOOST_CHECK_EQUAL(desc, "Left Mouse");
}

BOOST_AUTO_TEST_CASE(CommandDisplayNameNonEmpty)
{
    auto& mgr = InputManager::Instance();
    using C = InputManager::Command;
    constexpr size_t kCount = static_cast<size_t>(C::COUNT);
    for (size_t i = 0; i < kCount; ++i) {
        BOOST_CHECK(!mgr.commandDisplayName(static_cast<C>(i)).empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
