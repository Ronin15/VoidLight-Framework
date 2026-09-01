/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE UIManagerFunctionalTests
#include <boost/test/unit_test.hpp>

#include <atomic>

#include "managers/InputManager.hpp"
#include "managers/UIManager.hpp"
#include "utils/TextureSource.hpp"

namespace {

void moveMouseTo(float x, float y) {
    SDL_Event event{};
    event.motion.x = x;
    event.motion.y = y;
    InputManager::Instance().onMouseMove(event);
}

void setLeftMouseButton(float x, float y, bool pressed) {
    SDL_Event event{};
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    if (pressed) {
        InputManager::Instance().onMouseButtonDown(event);
    } else {
        InputManager::Instance().onMouseButtonUp(event);
    }
}

} // namespace

// ============================================================================
// FIXTURE: UIManagerFixture
// ============================================================================

struct UIManagerFixture {
    UIManagerFixture() {
        BOOST_REQUIRE(UIManager::Instance().init());
        InputManager::Instance().reset();

        // Set initial window size
        UIManager::Instance().onWindowResize(800, 600);
    }

    ~UIManagerFixture() {
        InputManager::Instance().reset();
        UIManager::Instance().prepareForStateTransition();
    }
};

// ============================================================================
// TEST SUITE: UIPositioningTests
// ============================================================================
// Tests that validate UI positioning modes work correctly

BOOST_FIXTURE_TEST_SUITE(UIPositioningTests, UIManagerFixture)

// ----------------------------------------------------------------------------
// Test: ABSOLUTE positioning (backward compatibility)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestAbsolutePositioning) {
    auto& ui = UIManager::Instance();

    // Create button with absolute positioning
    ui.createButton("abs_button", UIRect{100, 50, 200, 40}, "Absolute");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::ABSOLUTE;
    positioning.offsetX = 100;
    positioning.offsetY = 50;

    ui.setComponentPositioning("abs_button", positioning);

    // Verify button exists
    BOOST_CHECK(ui.hasComponent("abs_button"));

    // Window resize should NOT move absolute positioned elements
    ui.onWindowResize(1024, 768);

    // Component should still exist after resize
    BOOST_CHECK(ui.hasComponent("abs_button"));

    ui.removeComponent("abs_button");
}

// ----------------------------------------------------------------------------
// Test: CENTERED_H positioning (horizontal center)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCenteredHorizontalPositioning) {
    auto& ui = UIManager::Instance();

    // Create button centered horizontally
    ui.createButton("centered_h_button", UIRect{350, 50, 100, 40}, "CenterH");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::CENTERED_H;
    positioning.offsetX = 0;      // No horizontal offset
    positioning.offsetY = 50;     // 50 pixels from top
    positioning.fixedWidth = 100;

    ui.setComponentPositioning("centered_h_button", positioning);

    // Window resize should reposition horizontally
    ui.onWindowResize(1024, 768);

    // Button should still exist and be centered
    BOOST_CHECK(ui.hasComponent("centered_h_button"));

    ui.removeComponent("centered_h_button");
}

// ----------------------------------------------------------------------------
// Test: CENTERED_V positioning (vertical center)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCenteredVerticalPositioning) {
    auto& ui = UIManager::Instance();

    // Create button centered vertically
    ui.createButton("centered_v_button", UIRect{50, 280, 100, 40}, "CenterV");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::CENTERED_V;
    positioning.offsetX = 50;     // 50 pixels from left
    positioning.offsetY = 0;      // No vertical offset
    positioning.fixedHeight = 40;

    ui.setComponentPositioning("centered_v_button", positioning);

    // Window resize should reposition vertically
    ui.onWindowResize(800, 768);

    BOOST_CHECK(ui.hasComponent("centered_v_button"));

    ui.removeComponent("centered_v_button");
}

// ----------------------------------------------------------------------------
// Test: CENTERED_BOTH positioning (center both axes)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCenteredBothPositioning) {
    auto& ui = UIManager::Instance();

    // Create button centered on both axes
    ui.createButton("centered_both", UIRect{350, 280, 100, 40}, "Center");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::CENTERED_BOTH;
    positioning.offsetX = 0;
    positioning.offsetY = 0;
    positioning.fixedWidth = 100;
    positioning.fixedHeight = 40;

    ui.setComponentPositioning("centered_both", positioning);

    // Window resize should center on both axes
    ui.onWindowResize(1024, 768);

    BOOST_CHECK(ui.hasComponent("centered_both"));

    ui.removeComponent("centered_both");
}

// ----------------------------------------------------------------------------
// Test: TOP_ALIGNED positioning
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestTopAlignedPositioning) {
    auto& ui = UIManager::Instance();

    // Create button aligned to top edge
    ui.createButton("top_aligned", UIRect{350, 20, 100, 40}, "Top");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::TOP_ALIGNED;
    positioning.offsetX = 0;      // Horizontally centered
    positioning.offsetY = 20;     // 20 pixels from top
    positioning.fixedWidth = 100;

    ui.setComponentPositioning("top_aligned", positioning);

    ui.onWindowResize(1024, 768);

    BOOST_CHECK(ui.hasComponent("top_aligned"));

    ui.removeComponent("top_aligned");
}

// ----------------------------------------------------------------------------
// Test: BOTTOM_ALIGNED positioning
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestBottomAlignedPositioning) {
    auto& ui = UIManager::Instance();

    // Create button aligned to bottom edge
    ui.createButton("bottom_aligned", UIRect{350, 540, 100, 40}, "Bottom");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::BOTTOM_ALIGNED;
    positioning.offsetX = 0;      // Horizontally centered
    positioning.offsetY = 20;     // 20 pixels from bottom
    positioning.fixedWidth = 100;
    positioning.fixedHeight = 40;

    ui.setComponentPositioning("bottom_aligned", positioning);

    ui.onWindowResize(1024, 768);

    BOOST_CHECK(ui.hasComponent("bottom_aligned"));

    ui.removeComponent("bottom_aligned");
}

// ----------------------------------------------------------------------------
// Test: LEFT_ALIGNED positioning
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestLeftAlignedPositioning) {
    auto& ui = UIManager::Instance();

    // Create button aligned to left edge
    ui.createButton("left_aligned", UIRect{20, 280, 100, 40}, "Left");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::LEFT_ALIGNED;
    positioning.offsetX = 20;     // 20 pixels from left
    positioning.offsetY = 0;      // Vertically centered
    positioning.fixedHeight = 40;

    ui.setComponentPositioning("left_aligned", positioning);

    ui.onWindowResize(800, 768);

    BOOST_CHECK(ui.hasComponent("left_aligned"));

    ui.removeComponent("left_aligned");
}

// ----------------------------------------------------------------------------
// Test: RIGHT_ALIGNED positioning
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestRightAlignedPositioning) {
    auto& ui = UIManager::Instance();

    // Create button aligned to right edge
    ui.createButton("right_aligned", UIRect{680, 280, 100, 40}, "Right");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::RIGHT_ALIGNED;
    positioning.offsetX = 20;     // 20 pixels from right edge
    positioning.offsetY = 0;      // Vertically centered
    positioning.fixedWidth = 100;
    positioning.fixedHeight = 40;

    ui.setComponentPositioning("right_aligned", positioning);

    ui.onWindowResize(1024, 768);

    BOOST_CHECK(ui.hasComponent("right_aligned"));

    ui.removeComponent("right_aligned");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: UICallbackTests
// ============================================================================
// Tests that validate UI callbacks fire correctly

BOOST_FIXTURE_TEST_SUITE(UICallbackTests, UIManagerFixture)

// ----------------------------------------------------------------------------
// Test: onClick callback fires when button is clicked
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestOnClickCallback) {
    auto& ui = UIManager::Instance();

    ui.createButton("click_button", UIRect{100, 100, 150, 50}, "Click Me");

    // Set up callback with atomic flag
    std::atomic<bool> buttonClicked{false};

    ui.setOnClick("click_button", [&buttonClicked]() {
        buttonClicked.store(true, std::memory_order_release);
    });

    ui.simulateClick("click_button");
    ui.update(0.016f);
    BOOST_CHECK(buttonClicked.load(std::memory_order_acquire));

    // Clean up
    ui.removeComponent("click_button");
}

// ----------------------------------------------------------------------------
// Test: onValueChanged callback for progress bar
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestOnValueChangedCallback) {
    auto& ui = UIManager::Instance();

    ui.createProgressBar("progress", UIRect{100, 100, 300, 30}, 0.0f, 100.0f);

    // Set up callback to track value changes
    std::atomic<float> lastValue{-1.0f};

    ui.setOnValueChanged("progress", [&lastValue](float newValue) {
        lastValue.store(newValue, std::memory_order_release);
    });

    // Update progress bar value
    ui.updateProgressBar("progress", 0.5f); // 50%

    BOOST_CHECK_CLOSE(lastValue.load(std::memory_order_acquire), 0.5f, 0.001f);

    ui.removeComponent("progress");
}

// ----------------------------------------------------------------------------
// Test: onTextChanged callback for text updates
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestOnTextChangedCallback) {
    auto& ui = UIManager::Instance();

    ui.createLabel("label", UIRect{100, 100, 200, 30}, "Initial");

    // Set up callback to track text changes
    std::atomic<bool> textChanged{false};

    ui.setOnTextChanged("label", [&textChanged](const std::string&) {
        textChanged.store(true, std::memory_order_release);
    });

    ui.setText("label", "Initial");
    BOOST_CHECK(!textChanged.load(std::memory_order_acquire));

    ui.setText("label", "Updated");
    BOOST_CHECK(textChanged.load(std::memory_order_acquire));

    ui.removeComponent("label");
}

// ----------------------------------------------------------------------------
// Test: Multiple callbacks can be set independently
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestMultipleIndependentCallbacks) {
    auto& ui = UIManager::Instance();

    ui.createButton("button1", UIRect{100, 100, 100, 40}, "Button 1");
    ui.createButton("button2", UIRect{220, 100, 100, 40}, "Button 2");

    std::atomic<int> button1Clicks{0};
    std::atomic<int> button2Clicks{0};

    ui.setOnClick("button1", [&button1Clicks]() {
        button1Clicks.fetch_add(1, std::memory_order_relaxed);
    });

    ui.setOnClick("button2", [&button2Clicks]() {
        button2Clicks.fetch_add(1, std::memory_order_relaxed);
    });

    ui.simulateClick("button1");
    ui.simulateClick("button2");
    ui.simulateClick("button2");
    ui.update(0.016f);

    BOOST_CHECK_EQUAL(button1Clicks.load(std::memory_order_relaxed), 1);
    BOOST_CHECK_EQUAL(button2Clicks.load(std::memory_order_relaxed), 2);

    ui.removeComponent("button1");
    ui.removeComponent("button2");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: UIComponentCreationTests
// ============================================================================
// Tests that validate UI component creation

BOOST_FIXTURE_TEST_SUITE(UIComponentCreationTests, UIManagerFixture)

// ----------------------------------------------------------------------------
// Test: Create various button types
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCreateButtonVariants) {
    auto& ui = UIManager::Instance();

    ui.createButton("normal_button", UIRect{100, 50, 150, 40}, "Normal");
    ui.createButtonDanger("danger_button", UIRect{100, 100, 150, 40}, "Danger");
    ui.createButtonSuccess("success_button", UIRect{100, 150, 150, 40}, "Success");
    ui.createButtonWarning("warning_button", UIRect{100, 200, 150, 40}, "Warning");

    BOOST_CHECK(ui.hasComponent("normal_button"));
    BOOST_CHECK(ui.hasComponent("danger_button"));
    BOOST_CHECK(ui.hasComponent("success_button"));
    BOOST_CHECK(ui.hasComponent("warning_button"));

    ui.removeComponent("normal_button");
    ui.removeComponent("danger_button");
    ui.removeComponent("success_button");
    ui.removeComponent("warning_button");
}

// ----------------------------------------------------------------------------
// Test: Create text components (label, title)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCreateTextComponents) {
    auto& ui = UIManager::Instance();

    ui.createLabel("label1", UIRect{100, 50, 200, 30}, "This is a label");
    ui.createTitle("title1", UIRect{100, 100, 300, 40}, "This is a title");

    BOOST_CHECK(ui.hasComponent("label1"));
    BOOST_CHECK(ui.hasComponent("title1"));

    ui.removeComponent("label1");
    ui.removeComponent("title1");
}

BOOST_AUTO_TEST_CASE(TestLateCreatedTextComponentParticipatesInInputOrder) {
    auto& ui = UIManager::Instance();
    ui.setGlobalScale(1.0f);

    ui.createButton("seed_button", UIRect{10, 10, 80, 30}, "Seed");
    moveMouseTo(20.0f, 20.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("seed_button") == UIState::HOVERED);

    ui.createLabel("late_label", UIRect{200, 200, 100, 30}, "Late");
    moveMouseTo(210.0f, 210.0f);
    ui.update(0.0f);

    BOOST_CHECK(ui.getComponentState("late_label") == UIState::NORMAL);

    ui.removeComponent("seed_button");
    ui.removeComponent("late_label");
}

BOOST_AUTO_TEST_CASE(TestMouseHoverHighlightOnlyAppliesToButtons) {
    auto& ui = UIManager::Instance();
    ui.setGlobalScale(1.0f);

    ui.createButton("hover_button", UIRect{10, 10, 90, 30}, "Button");
    ui.createLabel("hover_label", UIRect{10, 60, 90, 30}, "Label");
    ui.createPanel("hover_panel", UIRect{10, 110, 90, 30});
    ui.createCheckbox("hover_checkbox", UIRect{10, 160, 90, 30}, "Check");
    ui.createSlider("hover_slider", UIRect{10, 210, 90, 30}, 0.0f, 1.0f);
    ui.createInputField("hover_input", UIRect{10, 260, 90, 30}, "Input");
    ui.createList("hover_list", UIRect{10, 310, 90, 60});

    moveMouseTo(20.0f, 20.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("hover_button") == UIState::HOVERED);

    moveMouseTo(20.0f, 70.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("hover_label") == UIState::NORMAL);

    moveMouseTo(20.0f, 120.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("hover_panel") == UIState::NORMAL);

    moveMouseTo(20.0f, 170.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("hover_checkbox") == UIState::NORMAL);

    moveMouseTo(20.0f, 220.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("hover_slider") == UIState::NORMAL);

    moveMouseTo(20.0f, 270.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("hover_input") == UIState::NORMAL);

    moveMouseTo(20.0f, 320.0f);
    ui.update(0.0f);
    BOOST_CHECK(ui.getComponentState("hover_list") == UIState::NORMAL);

    ui.removeComponent("hover_button");
    ui.removeComponent("hover_label");
    ui.removeComponent("hover_panel");
    ui.removeComponent("hover_checkbox");
    ui.removeComponent("hover_slider");
    ui.removeComponent("hover_input");
    ui.removeComponent("hover_list");
}

BOOST_AUTO_TEST_CASE(TestNonButtonMouseInteractionsStillWorkWithoutHoverHighlight) {
    auto& ui = UIManager::Instance();
    ui.setGlobalScale(1.0f);

    ui.createCheckbox("click_checkbox", UIRect{100, 100, 120, 30}, "Check");
    setLeftMouseButton(110.0f, 110.0f, true);
    ui.update(0.0f);
    setLeftMouseButton(110.0f, 110.0f, false);
    ui.update(0.0f);

    BOOST_CHECK(ui.getChecked("click_checkbox"));
    BOOST_CHECK(ui.getComponentState("click_checkbox") == UIState::NORMAL);

    ui.createSlider("drag_slider", UIRect{100, 150, 100, 30}, 0.0f, 1.0f);
    setLeftMouseButton(150.0f, 165.0f, true);
    ui.update(0.0f);
    setLeftMouseButton(150.0f, 165.0f, false);
    ui.update(0.0f);

    BOOST_CHECK_CLOSE(ui.getValue("drag_slider"), 0.5f, 0.001f);
    BOOST_CHECK(ui.getComponentState("drag_slider") == UIState::NORMAL);

    ui.removeComponent("click_checkbox");
    ui.removeComponent("drag_slider");
}

BOOST_AUTO_TEST_CASE(TestKeyboardSelectionCanStillHighlightNonButtonControls) {
    auto& ui = UIManager::Instance();
    ui.setGlobalScale(1.0f);

    ui.createSlider("keyboard_slider", UIRect{100, 100, 100, 30}, 0.0f, 1.0f);
    ui.setKeyboardSelection("keyboard_slider");
    moveMouseTo(400.0f, 400.0f);
    ui.update(0.0f);

    BOOST_CHECK(ui.getComponentState("keyboard_slider") == UIState::HOVERED);

    ui.clearKeyboardSelection();
    ui.removeComponent("keyboard_slider");
}

// ----------------------------------------------------------------------------
// Test: Create panel container
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCreatePanel) {
    auto& ui = UIManager::Instance();

    ui.createPanel("panel1", UIRect{100, 100, 400, 300});

    BOOST_CHECK(ui.hasComponent("panel1"));

    ui.removeComponent("panel1");
}

// ----------------------------------------------------------------------------
// Test: Create progress bar with min/max values
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCreateProgressBar) {
    auto& ui = UIManager::Instance();

    ui.createProgressBar("progress1", UIRect{100, 100, 300, 25}, 0.0f, 100.0f);

    BOOST_CHECK(ui.hasComponent("progress1"));

    // Update progress value
    ui.updateProgressBar("progress1", 0.75f); // 75%

    ui.removeComponent("progress1");
}

// ----------------------------------------------------------------------------
// Test: Create input field with placeholder
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCreateInputField) {
    auto& ui = UIManager::Instance();

    ui.createInputField("input1", UIRect{100, 100, 250, 30}, "Enter username...");

    BOOST_CHECK(ui.hasComponent("input1"));

    ui.removeComponent("input1");
}

BOOST_AUTO_TEST_CASE(TestCreateAtlasImage) {
    auto& ui = UIManager::Instance();

    const UIRect sourceRect{16, 32, 24, 24};
    ui.createPanel("image_parent", UIRect{80, 80, 80, 80});
    ui.createAtlasImage("atlas_image", UIRect{100, 100, 32, 32},
                        "atlas", sourceRect, "image_parent");

    BOOST_CHECK(ui.hasComponent("atlas_image"));
    BOOST_CHECK_EQUAL(ui.getTexture("atlas_image"), "atlas");
    const UIRect storedRect = ui.getImageSourceRect("atlas_image");
    BOOST_CHECK_EQUAL(storedRect.x, sourceRect.x);
    BOOST_CHECK_EQUAL(storedRect.y, sourceRect.y);
    BOOST_CHECK_EQUAL(storedRect.width, sourceRect.width);
    BOOST_CHECK_EQUAL(storedRect.height, sourceRect.height);

    const UIRect updatedRect{48, 64, 16, 16};
    ui.setImageSourceRect("atlas_image", updatedRect);
    const UIRect newRect = ui.getImageSourceRect("atlas_image");
    BOOST_CHECK_EQUAL(newRect.x, updatedRect.x);
    BOOST_CHECK_EQUAL(newRect.y, updatedRect.y);
    BOOST_CHECK_EQUAL(newRect.width, updatedRect.width);
    BOOST_CHECK_EQUAL(newRect.height, updatedRect.height);

    ui.setTexture("atlas_image", "");
    BOOST_CHECK(ui.getTexture("atlas_image").empty());

    ui.setComponentVisible("image_parent", false);
    BOOST_CHECK(ui.hasComponent("atlas_image"));

    ui.removeComponent("image_parent");
    BOOST_CHECK(!ui.hasComponent("atlas_image"));
}

BOOST_AUTO_TEST_CASE(TestSetImageSourceAppliesTextureAndSourceRectTogether) {
    auto& ui = UIManager::Instance();

    ui.createImage("image_source", UIRect{100, 100, 32, 32});

    ui.setImageSource("image_source", TextureSource{"atlas", 4, 8, 16, 20, true});
    BOOST_CHECK_EQUAL(ui.getTexture("image_source"), "atlas");
    UIRect storedRect = ui.getImageSourceRect("image_source");
    BOOST_CHECK_EQUAL(storedRect.x, 4);
    BOOST_CHECK_EQUAL(storedRect.y, 8);
    BOOST_CHECK_EQUAL(storedRect.width, 16);
    BOOST_CHECK_EQUAL(storedRect.height, 20);

    ui.setImageSource("image_source", TextureSource{"bow_icon", 0, 0, 0, 0, false});
    BOOST_CHECK_EQUAL(ui.getTexture("image_source"), "bow_icon");
    storedRect = ui.getImageSourceRect("image_source");
    BOOST_CHECK_EQUAL(storedRect.x, 0);
    BOOST_CHECK_EQUAL(storedRect.y, 0);
    BOOST_CHECK_EQUAL(storedRect.width, 0);
    BOOST_CHECK_EQUAL(storedRect.height, 0);

    ui.setImageSource("image_source", TextureSource{});
    BOOST_CHECK(ui.getTexture("image_source").empty());
    storedRect = ui.getImageSourceRect("image_source");
    BOOST_CHECK_EQUAL(storedRect.x, 0);
    BOOST_CHECK_EQUAL(storedRect.y, 0);
    BOOST_CHECK_EQUAL(storedRect.width, 0);
    BOOST_CHECK_EQUAL(storedRect.height, 0);

    ui.removeComponent("image_source");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: UIComponentManagementTests
// ============================================================================
// Tests that validate component lifecycle management

BOOST_FIXTURE_TEST_SUITE(UIComponentManagementTests, UIManagerFixture)

// ----------------------------------------------------------------------------
// Test: Remove component
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestRemoveComponent) {
    auto& ui = UIManager::Instance();

    ui.createButton("temp_button", UIRect{100, 100, 100, 40}, "Temp");
    BOOST_CHECK(ui.hasComponent("temp_button"));

    ui.removeComponent("temp_button");
    BOOST_CHECK(!ui.hasComponent("temp_button"));
}

// ----------------------------------------------------------------------------
// Test: Set text on existing component
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestSetComponentText) {
    auto& ui = UIManager::Instance();

    ui.createLabel("label", UIRect{100, 100, 200, 30}, "Original Text");
    BOOST_CHECK(ui.hasComponent("label"));

    ui.setText("label", "Updated Text");

    // Component should still exist
    BOOST_CHECK(ui.hasComponent("label"));

    ui.removeComponent("label");
}

// ----------------------------------------------------------------------------
// Test: Enable/disable component
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestEnableDisableComponent) {
    auto& ui = UIManager::Instance();

    ui.createButton("toggle_button", UIRect{100, 100, 100, 40}, "Toggle");

    // Disable component
    ui.setComponentEnabled("toggle_button", false);
    BOOST_CHECK(ui.hasComponent("toggle_button"));

    // Re-enable component
    ui.setComponentEnabled("toggle_button", true);
    BOOST_CHECK(ui.hasComponent("toggle_button"));

    ui.removeComponent("toggle_button");
}

// ----------------------------------------------------------------------------
// Test: Show/hide component
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestShowHideComponent) {
    auto& ui = UIManager::Instance();

    ui.createButton("visibility_button", UIRect{100, 100, 100, 40}, "Visible");

    // Hide component
    ui.setComponentVisible("visibility_button", false);
    BOOST_CHECK(ui.hasComponent("visibility_button"));

    // Show component
    ui.setComponentVisible("visibility_button", true);
    BOOST_CHECK(ui.hasComponent("visibility_button"));

    ui.removeComponent("visibility_button");
}

// ----------------------------------------------------------------------------
// Test: Set component Z-order
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestSetComponentZOrder) {
    auto& ui = UIManager::Instance();

    ui.createButton("background", UIRect{100, 100, 100, 40}, "Back");
    ui.createButton("foreground", UIRect{120, 120, 100, 40}, "Front");

    ui.setComponentZOrder("background", 1);
    ui.setComponentZOrder("foreground", 10);

    BOOST_CHECK(ui.hasComponent("background"));
    BOOST_CHECK(ui.hasComponent("foreground"));

    ui.removeComponent("background");
    ui.removeComponent("foreground");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// TEST SUITE: UIWindowResizeTests
// ============================================================================
// Tests that validate UI responds correctly to window resize events

BOOST_FIXTURE_TEST_SUITE(UIWindowResizeTests, UIManagerFixture)

// ----------------------------------------------------------------------------
// Test: Window resize triggers repositioning
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestWindowResizeTriggersRepositioning) {
    auto& ui = UIManager::Instance();

    // Create centered button
    ui.createButton("centered", UIRect{350, 280, 100, 40}, "Center");

    UIPositioning positioning;
    positioning.mode = UIPositionMode::CENTERED_BOTH;
    positioning.offsetX = 0;
    positioning.offsetY = 0;
    positioning.fixedWidth = 100;
    positioning.fixedHeight = 40;

    ui.setComponentPositioning("centered", positioning);

    // Resize window
    ui.onWindowResize(1024, 768);

    BOOST_CHECK(ui.hasComponent("centered"));
    UIRect bounds = ui.getBounds("centered");
    int expectedWidth = static_cast<int>(positioning.fixedWidth * ui.getGlobalScale());
    int expectedHeight = static_cast<int>(positioning.fixedHeight * ui.getGlobalScale());
    BOOST_CHECK_EQUAL(bounds.width, expectedWidth);
    BOOST_CHECK_EQUAL(bounds.height, expectedHeight);
    BOOST_CHECK_EQUAL(bounds.x, (1024 - expectedWidth) / 2);
    BOOST_CHECK_EQUAL(bounds.y, (768 - expectedHeight) / 2);

    // Resize again to different dimensions
    ui.onWindowResize(1280, 720);

    BOOST_CHECK(ui.hasComponent("centered"));
    bounds = ui.getBounds("centered");
    expectedWidth = static_cast<int>(positioning.fixedWidth * ui.getGlobalScale());
    expectedHeight = static_cast<int>(positioning.fixedHeight * ui.getGlobalScale());
    BOOST_CHECK_EQUAL(bounds.width, expectedWidth);
    BOOST_CHECK_EQUAL(bounds.height, expectedHeight);
    BOOST_CHECK_EQUAL(bounds.x, (1280 - expectedWidth) / 2);
    BOOST_CHECK_EQUAL(bounds.y, (720 - expectedHeight) / 2);

    ui.removeComponent("centered");
}

// ----------------------------------------------------------------------------
// Test: Multiple window resizes preserve component state
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestMultipleResizesPreserveState) {
    auto& ui = UIManager::Instance();

    ui.createButton("resize_test", UIRect{100, 100, 120, 40}, "Resize");

    // Perform multiple resizes
    ui.onWindowResize(1024, 768);
    ui.onWindowResize(800, 600);
    ui.onWindowResize(1280, 1024);
    ui.onWindowResize(1920, 1080);

    // Component should survive all resizes
    BOOST_CHECK(ui.hasComponent("resize_test"));

    ui.removeComponent("resize_test");
}

BOOST_AUTO_TEST_SUITE_END()
