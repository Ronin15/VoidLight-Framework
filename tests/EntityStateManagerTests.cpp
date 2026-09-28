/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE EntityStateManagerTests
#include <boost/test/unit_test.hpp>

#include "entities/EntityStateManager.hpp"
#include "entities/EntityState.hpp"
#include <memory>
#include <stdexcept>

// Mock EntityState that tracks lifecycle calls
class MockEntityState : public EntityState {
public:
    int enterCount = 0;
    int exitCount = 0;
    int updateCount = 0;
    float lastDeltaTime = 0.0f;

    void enter() override { ++enterCount; }
    void exit() override { ++exitCount; }
    void update(float deltaTime) override {
        ++updateCount;
        lastDeltaTime = deltaTime;
    }
};

// Helper to create mock states for tests
std::unique_ptr<MockEntityState> createMockState() {
    return std::make_unique<MockEntityState>();
}

// ============================================================================
// Basic State Management Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(AddState) {
    EntityStateManager manager;

    manager.addState("idle", createMockState());

    BOOST_CHECK(manager.hasState("idle"));
    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), ""); // Not set yet
}

BOOST_AUTO_TEST_CASE(AddMultipleStates) {
    EntityStateManager manager;

    manager.addState("idle", createMockState());
    manager.addState("walking", createMockState());
    manager.addState("running", createMockState());

    BOOST_CHECK(manager.hasState("idle"));
    BOOST_CHECK(manager.hasState("walking"));
    BOOST_CHECK(manager.hasState("running"));
}

BOOST_AUTO_TEST_CASE(AddDuplicateStateThrows) {
    EntityStateManager manager;

    manager.addState("idle", createMockState());

    BOOST_CHECK_THROW(manager.addState("idle", createMockState()), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(HasStateReturnsFalseForNonExistent) {
    EntityStateManager manager;

    manager.addState("idle", createMockState());

    BOOST_CHECK(!manager.hasState("nonexistent"));
    BOOST_CHECK(!manager.hasState(""));
}

BOOST_AUTO_TEST_CASE(GetCurrentStateNameEmptyWhenNoState) {
    EntityStateManager manager;

    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), "");

    manager.addState("idle", createMockState());
    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), ""); // Still empty until set
}

// ============================================================================
// State Transition Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(SetStateCallsEnter) {
    EntityStateManager manager;

    auto state = std::make_unique<MockEntityState>();
    MockEntityState* statePtr = state.get();
    manager.addState("idle", std::move(state));

    BOOST_CHECK_EQUAL(statePtr->enterCount, 0);

    manager.setState("idle");

    BOOST_CHECK_EQUAL(statePtr->enterCount, 1);
    BOOST_CHECK_EQUAL(statePtr->exitCount, 0);
    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), "idle");
}

BOOST_AUTO_TEST_CASE(SetStateTransitionCallsExitThenEnter) {
    EntityStateManager manager;

    auto idleState = std::make_unique<MockEntityState>();
    auto runningState = std::make_unique<MockEntityState>();
    MockEntityState* idlePtr = idleState.get();
    MockEntityState* runningPtr = runningState.get();

    manager.addState("idle", std::move(idleState));
    manager.addState("running", std::move(runningState));

    manager.setState("idle");
    BOOST_CHECK_EQUAL(idlePtr->enterCount, 1);
    BOOST_CHECK_EQUAL(idlePtr->exitCount, 0);

    manager.setState("running");

    BOOST_CHECK_EQUAL(idlePtr->exitCount, 1);
    BOOST_CHECK_EQUAL(runningPtr->enterCount, 1);
    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), "running");
}

BOOST_AUTO_TEST_CASE(SetSameStateTriggersCycle) {
    EntityStateManager manager;

    auto state = std::make_unique<MockEntityState>();
    MockEntityState* statePtr = state.get();
    manager.addState("idle", std::move(state));

    manager.setState("idle");
    BOOST_CHECK_EQUAL(statePtr->enterCount, 1);
    BOOST_CHECK_EQUAL(statePtr->exitCount, 0);

    manager.setState("idle");
    BOOST_CHECK_EQUAL(statePtr->enterCount, 2);
    BOOST_CHECK_EQUAL(statePtr->exitCount, 1);
}

BOOST_AUTO_TEST_CASE(SetNonExistentStateResetsCurrentState) {
    EntityStateManager manager;

    auto state = std::make_unique<MockEntityState>();
    MockEntityState* statePtr = state.get();
    manager.addState("idle", std::move(state));

    manager.setState("idle");
    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), "idle");

    manager.setState("nonexistent");

    BOOST_CHECK_EQUAL(statePtr->exitCount, 1);
    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), "");
}

// ============================================================================
// Update Propagation Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(UpdateCallsCurrentStateUpdate) {
    EntityStateManager manager;

    auto state = std::make_unique<MockEntityState>();
    MockEntityState* statePtr = state.get();
    manager.addState("idle", std::move(state));
    manager.setState("idle");

    BOOST_CHECK_EQUAL(statePtr->updateCount, 0);

    manager.update(0.016f);

    BOOST_CHECK_EQUAL(statePtr->updateCount, 1);
    BOOST_CHECK_CLOSE(statePtr->lastDeltaTime, 0.016f, 0.001f);
}

BOOST_AUTO_TEST_CASE(UpdatePassesDeltaTimeCorrectly) {
    EntityStateManager manager;

    auto state = std::make_unique<MockEntityState>();
    MockEntityState* statePtr = state.get();
    manager.addState("idle", std::move(state));
    manager.setState("idle");

    manager.update(0.033f);
    BOOST_CHECK_CLOSE(statePtr->lastDeltaTime, 0.033f, 0.001f);

    manager.update(0.05f);
    BOOST_CHECK_CLOSE(statePtr->lastDeltaTime, 0.05f, 0.001f);
}

BOOST_AUTO_TEST_CASE(UpdateWithNoCurrentStateIsNoOp) {
    EntityStateManager manager;

    // Should not crash when no current state
    BOOST_REQUIRE_NO_THROW(manager.update(0.016f));

    // Add state but don't set it
    manager.addState("idle", createMockState());
    BOOST_REQUIRE_NO_THROW(manager.update(0.016f));
}

BOOST_AUTO_TEST_CASE(UpdateOnlyAffectsCurrentState) {
    EntityStateManager manager;

    auto idleState = std::make_unique<MockEntityState>();
    auto runningState = std::make_unique<MockEntityState>();
    MockEntityState* idlePtr = idleState.get();
    MockEntityState* runningPtr = runningState.get();

    manager.addState("idle", std::move(idleState));
    manager.addState("running", std::move(runningState));
    manager.setState("idle");

    manager.update(0.016f);

    BOOST_CHECK_EQUAL(idlePtr->updateCount, 1);
    BOOST_CHECK_EQUAL(runningPtr->updateCount, 0);
}

// ============================================================================
// Remove State Tests
// ============================================================================


// ============================================================================
// Edge Cases
// ============================================================================


BOOST_AUTO_TEST_CASE(MultipleTransitions) {
    EntityStateManager manager;

    auto idleState = std::make_unique<MockEntityState>();
    auto walkingState = std::make_unique<MockEntityState>();
    auto runningState = std::make_unique<MockEntityState>();
    MockEntityState* idlePtr = idleState.get();
    MockEntityState* walkingPtr = walkingState.get();
    MockEntityState* runningPtr = runningState.get();

    manager.addState("idle", std::move(idleState));
    manager.addState("walking", std::move(walkingState));
    manager.addState("running", std::move(runningState));

    manager.setState("idle");
    manager.setState("walking");
    manager.setState("running");
    manager.setState("idle");

    BOOST_CHECK_EQUAL(idlePtr->enterCount, 2);
    BOOST_CHECK_EQUAL(idlePtr->exitCount, 1);
    BOOST_CHECK_EQUAL(walkingPtr->enterCount, 1);
    BOOST_CHECK_EQUAL(walkingPtr->exitCount, 1);
    BOOST_CHECK_EQUAL(runningPtr->enterCount, 1);
    BOOST_CHECK_EQUAL(runningPtr->exitCount, 1);
    BOOST_CHECK_EQUAL(manager.getCurrentStateName(), "idle");
}
