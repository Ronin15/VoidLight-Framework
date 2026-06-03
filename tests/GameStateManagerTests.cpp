/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE GameStateManagerTests
#include <boost/test/unit_test.hpp>
#include <SDL3/SDL_gpu.h>
#include "managers/GameStateManager.hpp"
#include "gameStates/GameState.hpp"
#include <memory>

// Mock GameState for testing
class MockGameState : public GameState {
public:
    explicit MockGameState(GameStateId id, bool enterResult = true)
        : m_id(id), m_enterCalled(false), m_exitCalled(false),
          m_updateCalled(false), m_renderCalled(false), m_handleInputCalled(false),
          m_pauseCalled(false), m_resumeCalled(false), m_enterResult(enterResult) {}

    bool enter() override {
        m_enterCalled = true;
        return m_enterResult;
    }

    void update(float deltaTime) override {
        m_updateCalled = true;
        m_lastDeltaTime = deltaTime;
    }

    void recordGPUVertices([[maybe_unused]] VoidLight::GPURenderer& gpuRenderer,
                           [[maybe_unused]] float interpolationAlpha = 1.0f) override {
        m_renderCalled = true;
    }

    void handleInput() override {
        m_handleInputCalled = true;
    }

    bool exit() override {
        m_exitCalled = true;
        return true;
    }

    void pause() override {
        m_pauseCalled = true;
    }

    void resume() override {
        m_resumeCalled = true;
    }

    GameStateId getStateId() const override {
        return m_id;
    }

    // Test helper methods
    bool wasEnterCalled() const { return m_enterCalled; }
    bool wasExitCalled() const { return m_exitCalled; }
    bool wasUpdateCalled() const { return m_updateCalled; }
    bool wasRenderCalled() const { return m_renderCalled; }
    bool wasHandleInputCalled() const { return m_handleInputCalled; }
    bool wasPauseCalled() const { return m_pauseCalled; }
    bool wasResumeCalled() const { return m_resumeCalled; }
    float getLastDeltaTime() const { return m_lastDeltaTime; }

    void resetFlags() {
        m_enterCalled = m_exitCalled = m_updateCalled = m_renderCalled =
        m_handleInputCalled = m_pauseCalled = m_resumeCalled = false;
    }

private:
    GameStateId m_id;
    bool m_enterCalled, m_exitCalled, m_updateCalled, m_renderCalled,
         m_handleInputCalled, m_pauseCalled, m_resumeCalled;
    bool m_enterResult;
    float m_lastDeltaTime{0.0f};
};

struct GameStateManagerFixture {
    GameStateManager manager;

    GameStateManagerFixture() = default;
    ~GameStateManagerFixture() = default;
};

BOOST_FIXTURE_TEST_SUITE(GameStateManagerTestSuite, GameStateManagerFixture)

BOOST_AUTO_TEST_CASE(TestInitialState) {
    // Manager should start empty
    BOOST_CHECK(!manager.hasState(GameStateId::LOGO));
    BOOST_CHECK(manager.getState(GameStateId::LOGO) == nullptr);
}

BOOST_AUTO_TEST_CASE(TestAddState) {
    auto mockState = std::make_unique<MockGameState>(GameStateId::LOGO);

    // Add state
    manager.addState(std::move(mockState));

    // State should be registered but not active
    BOOST_CHECK(manager.hasState(GameStateId::LOGO));
    BOOST_CHECK(manager.getState(GameStateId::LOGO) != nullptr);
    BOOST_CHECK(manager.getState(GameStateId::LOGO)->getStateId() == GameStateId::LOGO);
}

BOOST_AUTO_TEST_CASE(TestAddDuplicateState) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOGO);

    // Add first state
    manager.addState(std::move(mockState1));

    // Adding duplicate should throw
    BOOST_CHECK_THROW(manager.addState(std::move(mockState2)), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(TestPushState) {
    auto mockState = std::make_unique<MockGameState>(GameStateId::LOGO);
    MockGameState* statePtr = mockState.get();

    manager.addState(std::move(mockState));

    // Push state should call enter()
    manager.pushState(GameStateId::LOGO);
    BOOST_CHECK(statePtr->wasEnterCalled());
}

BOOST_AUTO_TEST_CASE(TestPushNonexistentState) {
    // Pushing nonexistent state should not crash (logs error)
    BOOST_CHECK_NO_THROW(manager.pushState(GameStateId::COUNT));
}

BOOST_AUTO_TEST_CASE(TestPushStateFailureResumesPreviousState) {
    auto currentState = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto failingState = std::make_unique<MockGameState>(GameStateId::LOADING, false);
    MockGameState* currentStatePtr = currentState.get();
    MockGameState* failingStatePtr = failingState.get();

    manager.addState(std::move(currentState));
    manager.addState(std::move(failingState));

    manager.pushState(GameStateId::LOGO);
    currentStatePtr->resetFlags();
    failingStatePtr->resetFlags();

    manager.pushState(GameStateId::LOADING);

    BOOST_CHECK(currentStatePtr->wasPauseCalled());
    BOOST_CHECK(currentStatePtr->wasResumeCalled());
    BOOST_CHECK(failingStatePtr->wasEnterCalled());

    currentStatePtr->resetFlags();
    manager.update(0.016f);
    BOOST_CHECK(currentStatePtr->wasUpdateCalled());
}

BOOST_AUTO_TEST_CASE(TestPopState) {
    auto mockState = std::make_unique<MockGameState>(GameStateId::LOGO);
    MockGameState* statePtr = mockState.get();

    manager.addState(std::move(mockState));
    manager.pushState(GameStateId::LOGO);

    statePtr->resetFlags();

    // Pop state should call exit()
    manager.popState();
    BOOST_CHECK(statePtr->wasExitCalled());
}

BOOST_AUTO_TEST_CASE(TestPopEmptyStack) {
    // Popping empty stack should not crash
    BOOST_CHECK_NO_THROW(manager.popState());
}

BOOST_AUTO_TEST_CASE(TestChangeState) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);
    MockGameState* state1Ptr = mockState1.get();
    MockGameState* state2Ptr = mockState2.get();

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));

    // Push first state
    manager.pushState(GameStateId::LOGO);
    BOOST_CHECK(state1Ptr->wasEnterCalled());

    state1Ptr->resetFlags();
    state2Ptr->resetFlags();

    // Change to second state
    manager.changeState(GameStateId::LOADING);

    // First state should exit, second should enter
    BOOST_CHECK(state1Ptr->wasExitCalled());
    BOOST_CHECK(state2Ptr->wasEnterCalled());
}

BOOST_AUTO_TEST_CASE(TestChangeStateClearingStackExitsAllActiveStates) {
    auto menuState = std::make_unique<MockGameState>(GameStateId::MAIN_MENU);
    auto gameState = std::make_unique<MockGameState>(GameStateId::GAME_PLAY);
    auto pauseState = std::make_unique<MockGameState>(GameStateId::PAUSE);
    MockGameState* menuPtr = menuState.get();
    MockGameState* gamePtr = gameState.get();
    MockGameState* pausePtr = pauseState.get();

    manager.addState(std::move(menuState));
    manager.addState(std::move(gameState));
    manager.addState(std::move(pauseState));

    manager.pushState(GameStateId::MAIN_MENU);
    manager.changeState(GameStateId::GAME_PLAY);
    manager.pushState(GameStateId::PAUSE);

    menuPtr->resetFlags();
    gamePtr->resetFlags();
    pausePtr->resetFlags();

    manager.changeStateClearingStack(GameStateId::MAIN_MENU);

    BOOST_CHECK(pausePtr->wasExitCalled());
    BOOST_CHECK(gamePtr->wasExitCalled());
    BOOST_CHECK(!gamePtr->wasResumeCalled());
    BOOST_CHECK(menuPtr->wasEnterCalled());

    menuPtr->resetFlags();
    gamePtr->resetFlags();
    pausePtr->resetFlags();
    manager.update(0.016f);

    BOOST_CHECK(menuPtr->wasUpdateCalled());
    BOOST_CHECK(!gamePtr->wasUpdateCalled());
    BOOST_CHECK(!pausePtr->wasUpdateCalled());
}

BOOST_AUTO_TEST_CASE(TestImmediateStateChange) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);
    MockGameState* state1Ptr = mockState1.get();
    MockGameState* state2Ptr = mockState2.get();

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));

    manager.pushState(GameStateId::LOGO);
    state1Ptr->resetFlags();
    state2Ptr->resetFlags();

    // State change should happen immediately
    manager.changeState(GameStateId::LOADING);

    // Change should happen immediately
    BOOST_CHECK(state1Ptr->wasExitCalled());
    BOOST_CHECK(state2Ptr->wasEnterCalled());
}

BOOST_AUTO_TEST_CASE(TestUpdate) {
    auto mockState = std::make_unique<MockGameState>(GameStateId::LOGO);
    MockGameState* statePtr = mockState.get();

    manager.addState(std::move(mockState));
    manager.pushState(GameStateId::LOGO);

    statePtr->resetFlags();

    // Update should call update on active state
    const float deltaTime = 0.016f;
    manager.update(deltaTime);

    BOOST_CHECK(statePtr->wasUpdateCalled());
    BOOST_CHECK_EQUAL(statePtr->getLastDeltaTime(), deltaTime);
}

BOOST_AUTO_TEST_CASE(TestUpdateEmptyStack) {
    // Update with no active states should not crash
    BOOST_CHECK_NO_THROW(manager.update(0.016f));
}

BOOST_AUTO_TEST_CASE(TestRender) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);
    MockGameState* state1Ptr = mockState1.get();
    MockGameState* state2Ptr = mockState2.get();

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));

    // Push both states to create a stack
    manager.pushState(GameStateId::LOGO);
    manager.pushState(GameStateId::LOADING);

    state1Ptr->resetFlags();
    state2Ptr->resetFlags();

    // GPU vertex recording should only call the top (current) active state
    auto* gpuRenderer = reinterpret_cast<VoidLight::GPURenderer*>(0x1);
    manager.recordGPUVertices(*gpuRenderer, 1.0f);

    BOOST_CHECK(!state1Ptr->wasRenderCalled()); // State1 is paused, should not render
    BOOST_CHECK(state2Ptr->wasRenderCalled());  // State2 is active, should render
}

BOOST_AUTO_TEST_CASE(TestRenderEmptyStack) {
    // GPU vertex recording with no active states should not crash
    auto* gpuRenderer = reinterpret_cast<VoidLight::GPURenderer*>(0x1);
    BOOST_CHECK_NO_THROW(manager.recordGPUVertices(*gpuRenderer, 1.0f));
}

BOOST_AUTO_TEST_CASE(TestHandleInput) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);
    MockGameState* state1Ptr = mockState1.get();
    MockGameState* state2Ptr = mockState2.get();

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));

    // Push both states
    manager.pushState(GameStateId::LOGO);
    manager.pushState(GameStateId::LOADING);

    state1Ptr->resetFlags();
    state2Ptr->resetFlags();

    // HandleInput should only call handleInput on top state
    manager.handleInput();

    BOOST_CHECK(!state1Ptr->wasHandleInputCalled()); // Bottom state should not handle input
    BOOST_CHECK(state2Ptr->wasHandleInputCalled());  // Top state should handle input
}

BOOST_AUTO_TEST_CASE(TestHandleInputEmptyStack) {
    // HandleInput with no active states should not crash
    BOOST_CHECK_NO_THROW(manager.handleInput());
}

BOOST_AUTO_TEST_CASE(TestPauseResume) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);
    MockGameState* state1Ptr = mockState1.get();
    MockGameState* state2Ptr = mockState2.get();

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));

    // Push first state
    manager.pushState(GameStateId::LOGO);
    state1Ptr->resetFlags();

    // Push second state should pause first
    manager.pushState(GameStateId::LOADING);
    BOOST_CHECK(state1Ptr->wasPauseCalled());
    BOOST_CHECK(state2Ptr->wasEnterCalled());

    state1Ptr->resetFlags();
    state2Ptr->resetFlags();

    // Pop second state should resume first
    manager.popState();
    BOOST_CHECK(state2Ptr->wasExitCalled());
    BOOST_CHECK(state1Ptr->wasResumeCalled());
}

BOOST_AUTO_TEST_CASE(TestRemoveState) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));

    // Push both states
    manager.pushState(GameStateId::LOGO);
    manager.pushState(GameStateId::LOADING);

    // Get the shared pointers from the manager and cast them to MockGameState
    auto state1Shared = std::dynamic_pointer_cast<MockGameState>(manager.getState(GameStateId::LOGO));
    auto state2Shared = std::dynamic_pointer_cast<MockGameState>(manager.getState(GameStateId::LOADING));

    BOOST_REQUIRE(state1Shared != nullptr);
    BOOST_REQUIRE(state2Shared != nullptr);

    state1Shared->resetFlags();
    state2Shared->resetFlags();

    // Remove active state
    manager.removeState(GameStateId::LOADING);

    // State2 should exit, State1 should resume
    BOOST_CHECK(state2Shared->wasExitCalled());
    BOOST_CHECK(state1Shared->wasResumeCalled());

    // State should no longer be registered
    BOOST_CHECK(!manager.hasState(GameStateId::LOADING));
    BOOST_CHECK(manager.getState(GameStateId::LOADING) == nullptr);
}

BOOST_AUTO_TEST_CASE(TestRemoveNonexistentState) {
    // Removing nonexistent state should not crash
    BOOST_CHECK_NO_THROW(manager.removeState(GameStateId::LOGO));
}

BOOST_AUTO_TEST_CASE(TestClearAllStates) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));

    // Push both states
    manager.pushState(GameStateId::LOGO);
    manager.pushState(GameStateId::LOADING);

    // Get the shared pointers from the manager and cast them to MockGameState
    auto state1Shared = std::dynamic_pointer_cast<MockGameState>(manager.getState(GameStateId::LOGO));
    auto state2Shared = std::dynamic_pointer_cast<MockGameState>(manager.getState(GameStateId::LOADING));

    BOOST_REQUIRE(state1Shared != nullptr);
    BOOST_REQUIRE(state2Shared != nullptr);

    state1Shared->resetFlags();
    state2Shared->resetFlags();

    // Clear all states
    manager.clearAllStates();

    // Both states should exit
    BOOST_CHECK(state1Shared->wasExitCalled());
    BOOST_CHECK(state2Shared->wasExitCalled());

    // States should no longer be registered
    BOOST_CHECK(!manager.hasState(GameStateId::LOGO));
    BOOST_CHECK(!manager.hasState(GameStateId::LOADING));
}

BOOST_AUTO_TEST_CASE(TestStateStackBehavior) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::LOGO);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::LOADING);
    auto mockState3 = std::make_unique<MockGameState>(GameStateId::MAIN_MENU);
    MockGameState* state1Ptr = mockState1.get();
    MockGameState* state2Ptr = mockState2.get();
    MockGameState* state3Ptr = mockState3.get();

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));
    manager.addState(std::move(mockState3));

    // Push states to create a stack
    manager.pushState(GameStateId::LOGO);
    manager.pushState(GameStateId::LOADING);
    manager.pushState(GameStateId::MAIN_MENU);

    // Reset flags
    state1Ptr->resetFlags();
    state2Ptr->resetFlags();
    state3Ptr->resetFlags();

    // Only top state should receive update and input
    manager.update(0.016f);
    manager.handleInput();

    BOOST_CHECK(!state1Ptr->wasUpdateCalled());
    BOOST_CHECK(!state2Ptr->wasUpdateCalled());
    BOOST_CHECK(state3Ptr->wasUpdateCalled());

    BOOST_CHECK(!state1Ptr->wasHandleInputCalled());
    BOOST_CHECK(!state2Ptr->wasHandleInputCalled());
    BOOST_CHECK(state3Ptr->wasHandleInputCalled());

    // Only the top state should render (correct behavior for game state management)
    state1Ptr->resetFlags();
    state2Ptr->resetFlags();
    state3Ptr->resetFlags();

    auto* gpuRenderer = reinterpret_cast<VoidLight::GPURenderer*>(0x1);
    manager.recordGPUVertices(*gpuRenderer, 1.0f);

    BOOST_CHECK(!state1Ptr->wasRenderCalled());
    BOOST_CHECK(!state2Ptr->wasRenderCalled());
    BOOST_CHECK(state3Ptr->wasRenderCalled());
}

BOOST_AUTO_TEST_CASE(TestComplexStateTransitions) {
    auto mockState1 = std::make_unique<MockGameState>(GameStateId::MAIN_MENU);
    auto mockState2 = std::make_unique<MockGameState>(GameStateId::GAME_PLAY);
    auto mockState3 = std::make_unique<MockGameState>(GameStateId::PAUSE);
    MockGameState* menuPtr = mockState1.get();
    MockGameState* gamePtr = mockState2.get();
    MockGameState* pausePtr = mockState3.get();

    manager.addState(std::move(mockState1));
    manager.addState(std::move(mockState2));
    manager.addState(std::move(mockState3));

    // Start with menu
    manager.pushState(GameStateId::MAIN_MENU);
    BOOST_CHECK(menuPtr->wasEnterCalled());

    // Change to game (menu exits, game enters)
    menuPtr->resetFlags();
    gamePtr->resetFlags();
    manager.changeState(GameStateId::GAME_PLAY);
    BOOST_CHECK(menuPtr->wasExitCalled());
    BOOST_CHECK(gamePtr->wasEnterCalled());

    // Push pause (game pauses, pause enters)
    gamePtr->resetFlags();
    pausePtr->resetFlags();
    manager.pushState(GameStateId::PAUSE);
    BOOST_CHECK(gamePtr->wasPauseCalled());
    BOOST_CHECK(pausePtr->wasEnterCalled());

    // Pop pause (pause exits, game resumes)
    gamePtr->resetFlags();
    pausePtr->resetFlags();
    manager.popState();
    BOOST_CHECK(pausePtr->wasExitCalled());
    BOOST_CHECK(gamePtr->wasResumeCalled());
}

BOOST_AUTO_TEST_SUITE_END()
