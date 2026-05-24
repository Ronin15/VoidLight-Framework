/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

/**
 * @file ControllerRegistryTests.cpp
 * @brief Unit tests for ControllerRegistry
 *
 * Tests cover:
 * - Controller registration (add<T>)
 * - Controller retrieval (get<T>, has<T>)
 * - Batch operations (subscribeAll, unsubscribeAll, suspendAll, resumeAll)
 * - IUpdatable detection and updateAll()
 * - Lifecycle management
 */

#define BOOST_TEST_MODULE ControllerRegistryTests
#include <boost/test/unit_test.hpp>

#include "controllers/ControllerRegistry.hpp"
#include "controllers/ControllerBase.hpp"
#include "controllers/IUpdatable.hpp"
#include "events/Event.hpp"
#include "managers/EventManager.hpp"
#include <atomic>
#include <memory>
#include <string_view>

// --- Test Fixtures ---

/**
 * @brief Mock controller for testing (event-only, no IUpdatable)
 */
class MockEventController : public ControllerBase
{
public:
    void subscribe() override
    {
        if (checkAlreadySubscribed()) {
            return;
        }
        m_subscribeCount++;
        setSubscribed(true);
    }

    [[nodiscard]] std::string_view getName() const override { return "MockEventController"; }

    int getSubscribeCount() const { return m_subscribeCount; }

private:
    int m_subscribeCount{0};
};

/**
 * @brief Mock controller for testing (with IUpdatable)
 */
class MockUpdatableController : public ControllerBase, public IUpdatable
{
public:
    void subscribe() override
    {
        if (checkAlreadySubscribed()) {
            return;
        }
        m_subscribeCount++;
        setSubscribed(true);
    }

    void update(float deltaTime) override
    {
        m_updateCount++;
        m_lastDeltaTime = deltaTime;
        m_totalTime += deltaTime;
    }

    [[nodiscard]] std::string_view getName() const override { return "MockUpdatableController"; }

    int getSubscribeCount() const { return m_subscribeCount; }
    int getUpdateCount() const { return m_updateCount; }
    float getLastDeltaTime() const { return m_lastDeltaTime; }
    float getTotalTime() const { return m_totalTime; }

private:
    int m_subscribeCount{0};
    int m_updateCount{0};
    float m_lastDeltaTime{0.0f};
    float m_totalTime{0.0f};
};

/**
 * @brief Second mock updatable for multi-controller tests
 */
class MockUpdatableController2 : public ControllerBase, public IUpdatable
{
public:
    void subscribe() override
    {
        if (checkAlreadySubscribed()) {
            return;
        }
        setSubscribed(true);
    }

    void update([[maybe_unused]] float deltaTime) override
    {
        m_updateCount++;
    }

    [[nodiscard]] std::string_view getName() const override { return "MockUpdatableController2"; }

    int getUpdateCount() const { return m_updateCount; }

private:
    int m_updateCount{0};
};

class TokenOwningController : public ControllerBase
{
public:
    static void resetEventCount()
    {
        s_eventCount.store(0, std::memory_order_relaxed);
    }

    static int eventCount()
    {
        return s_eventCount.load(std::memory_order_relaxed);
    }

    void subscribe() override
    {
        if (checkAlreadySubscribed()) {
            return;
        }

        addHandlerToken(EventManager::Instance().registerHandlerWithToken(
            EventTypeId::Custom,
            [](const EventData&) {
                s_eventCount.fetch_add(1, std::memory_order_relaxed);
            }));
        setSubscribed(true);
    }

    [[nodiscard]] std::string_view getName() const override { return "TokenOwningController"; }

private:
    static inline std::atomic<int> s_eventCount{0};
};

class TokenCleanupEvent : public Event
{
public:
    void update() override {}
    void execute() override {}
    void reset() override {}
    void clean() override {}

    [[nodiscard]] std::string getName() const override { return "TokenCleanupEvent"; }
    [[nodiscard]] std::string getType() const override { return "Custom"; }
    [[nodiscard]] std::string getTypeName() const override { return "TokenCleanupEvent"; }
    [[nodiscard]] EventTypeId getTypeId() const override { return EventTypeId::Custom; }
    [[nodiscard]] bool checkConditions() override { return true; }
};

struct RegistryFixture
{
    RegistryFixture()
    {
        // Initialize EventManager for tests that need it
        EventManager::Instance().prepareForStateTransition();
        BOOST_REQUIRE(EventManager::Instance().init());
    }

    ~RegistryFixture()
    {
        EventManager::Instance().clearAllHandlers();
    }

    ControllerRegistry registry;
};

// --- Registration Tests ---

BOOST_AUTO_TEST_SUITE(RegistrationTests)

BOOST_FIXTURE_TEST_CASE(TestAddController, RegistryFixture)
{
    BOOST_CHECK(registry.empty());
    BOOST_CHECK_EQUAL(registry.size(), 0u);

    auto& controller = registry.add<MockEventController>();

    BOOST_CHECK(!registry.empty());
    BOOST_CHECK_EQUAL(registry.size(), 1u);
    BOOST_CHECK_EQUAL(controller.getName(), "MockEventController");
}

BOOST_FIXTURE_TEST_CASE(TestAddMultipleControllers, RegistryFixture)
{
    registry.add<MockEventController>();
    registry.add<MockUpdatableController>();

    BOOST_CHECK_EQUAL(registry.size(), 2u);
}

BOOST_FIXTURE_TEST_CASE(TestDuplicateAddReturnsSame, RegistryFixture)
{
    auto& first = registry.add<MockEventController>();
    auto& second = registry.add<MockEventController>();

    // Should return the same instance
    BOOST_CHECK_EQUAL(&first, &second);
    BOOST_CHECK_EQUAL(registry.size(), 1u);
}

BOOST_FIXTURE_TEST_CASE(TestAddWithConstructorArgs, RegistryFixture)
{
    // MockEventController has default constructor, just verify it works
    auto& controller = registry.add<MockEventController>();
    BOOST_CHECK_EQUAL(controller.getName(), "MockEventController");
}

BOOST_AUTO_TEST_SUITE_END()

// --- Retrieval Tests ---

BOOST_AUTO_TEST_SUITE(RetrievalTests)

BOOST_FIXTURE_TEST_CASE(TestGetExistingController, RegistryFixture)
{
    registry.add<MockEventController>();

    auto* controller = registry.get<MockEventController>();

    BOOST_REQUIRE(controller != nullptr);
    BOOST_CHECK_EQUAL(controller->getName(), "MockEventController");
}

BOOST_FIXTURE_TEST_CASE(TestGetNonExistingController, RegistryFixture)
{
    // Don't add MockEventController

    auto* controller = registry.get<MockEventController>();

    BOOST_CHECK(controller == nullptr);
}

BOOST_FIXTURE_TEST_CASE(TestGetCorrectType, RegistryFixture)
{
    registry.add<MockEventController>();
    registry.add<MockUpdatableController>();

    auto* eventCtrl = registry.get<MockEventController>();
    auto* updatableCtrl = registry.get<MockUpdatableController>();

    BOOST_REQUIRE(eventCtrl != nullptr);
    BOOST_REQUIRE(updatableCtrl != nullptr);
    BOOST_CHECK_EQUAL(eventCtrl->getName(), "MockEventController");
    BOOST_CHECK_EQUAL(updatableCtrl->getName(), "MockUpdatableController");
}

BOOST_FIXTURE_TEST_CASE(TestHasController, RegistryFixture)
{
    BOOST_CHECK(!registry.has<MockEventController>());

    registry.add<MockEventController>();

    BOOST_CHECK(registry.has<MockEventController>());
    BOOST_CHECK(!registry.has<MockUpdatableController>());
}

BOOST_FIXTURE_TEST_CASE(TestConstGet, RegistryFixture)
{
    registry.add<MockEventController>();

    const ControllerRegistry& constRegistry = registry;
    const auto* controller = constRegistry.get<MockEventController>();

    BOOST_REQUIRE(controller != nullptr);
    BOOST_CHECK_EQUAL(controller->getName(), "MockEventController");
}

BOOST_AUTO_TEST_SUITE_END()

// --- Batch Subscribe/Unsubscribe Tests ---

BOOST_AUTO_TEST_SUITE(BatchSubscribeTests)

BOOST_FIXTURE_TEST_CASE(TestSubscribeAll, RegistryFixture)
{
    auto& ctrl1 = registry.add<MockEventController>();
    auto& ctrl2 = registry.add<MockUpdatableController>();

    BOOST_CHECK(!ctrl1.isSubscribed());
    BOOST_CHECK(!ctrl2.isSubscribed());

    registry.subscribeAll();

    BOOST_CHECK(ctrl1.isSubscribed());
    BOOST_CHECK(ctrl2.isSubscribed());
}

BOOST_FIXTURE_TEST_CASE(TestUnsubscribeAll, RegistryFixture)
{
    auto& ctrl1 = registry.add<MockEventController>();
    auto& ctrl2 = registry.add<MockUpdatableController>();

    registry.subscribeAll();
    BOOST_CHECK(ctrl1.isSubscribed());
    BOOST_CHECK(ctrl2.isSubscribed());

    registry.unsubscribeAll();

    BOOST_CHECK(!ctrl1.isSubscribed());
    BOOST_CHECK(!ctrl2.isSubscribed());
}

BOOST_FIXTURE_TEST_CASE(TestSubscribeCountTracking, RegistryFixture)
{
    auto& ctrl = registry.add<MockEventController>();

    registry.subscribeAll();
    BOOST_CHECK_EQUAL(ctrl.getSubscribeCount(), 1);

    // Subscribing again should be idempotent
    registry.subscribeAll();
    BOOST_CHECK_EQUAL(ctrl.getSubscribeCount(), 1);

    // After unsubscribe and re-subscribe
    registry.unsubscribeAll();
    registry.subscribeAll();
    BOOST_CHECK_EQUAL(ctrl.getSubscribeCount(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

// --- Suspend/Resume Tests ---

BOOST_AUTO_TEST_SUITE(SuspendResumeTests)

BOOST_FIXTURE_TEST_CASE(TestSuspendAll, RegistryFixture)
{
    auto& ctrl1 = registry.add<MockEventController>();
    auto& ctrl2 = registry.add<MockUpdatableController>();

    registry.subscribeAll();
    BOOST_CHECK(ctrl1.isSubscribed());
    BOOST_CHECK(ctrl2.isSubscribed());
    BOOST_CHECK(!ctrl1.isSuspended());
    BOOST_CHECK(!ctrl2.isSuspended());

    registry.suspendAll();

    BOOST_CHECK(!ctrl1.isSubscribed());  // Default suspend unsubscribes
    BOOST_CHECK(!ctrl2.isSubscribed());
    BOOST_CHECK(ctrl1.isSuspended());
    BOOST_CHECK(ctrl2.isSuspended());
}

BOOST_FIXTURE_TEST_CASE(TestResumeAll, RegistryFixture)
{
    auto& ctrl1 = registry.add<MockEventController>();
    auto& ctrl2 = registry.add<MockUpdatableController>();

    registry.subscribeAll();
    registry.suspendAll();

    BOOST_CHECK(ctrl1.isSuspended());
    BOOST_CHECK(ctrl2.isSuspended());

    registry.resumeAll();

    BOOST_CHECK(ctrl1.isSubscribed());  // Default resume re-subscribes
    BOOST_CHECK(ctrl2.isSubscribed());
    BOOST_CHECK(!ctrl1.isSuspended());
    BOOST_CHECK(!ctrl2.isSuspended());
}

BOOST_FIXTURE_TEST_CASE(TestSuspendResumeIdempotent, RegistryFixture)
{
    auto& ctrl = registry.add<MockEventController>();

    registry.subscribeAll();

    // Double suspend should be safe
    registry.suspendAll();
    registry.suspendAll();
    BOOST_CHECK(ctrl.isSuspended());

    // Double resume should be safe
    registry.resumeAll();
    registry.resumeAll();
    BOOST_CHECK(!ctrl.isSuspended());
}

BOOST_AUTO_TEST_SUITE_END()

// --- IUpdatable and updateAll Tests ---

BOOST_AUTO_TEST_SUITE(UpdateTests)

BOOST_FIXTURE_TEST_CASE(TestUpdateAllCallsUpdatables, RegistryFixture)
{
    auto& updatable = registry.add<MockUpdatableController>();

    registry.subscribeAll();

    BOOST_CHECK_EQUAL(updatable.getUpdateCount(), 0);

    registry.updateAll(0.016f);

    BOOST_CHECK_EQUAL(updatable.getUpdateCount(), 1);
    BOOST_CHECK_CLOSE(updatable.getLastDeltaTime(), 0.016f, 0.0001f);
}

BOOST_FIXTURE_TEST_CASE(TestUpdateAllSkipsNonUpdatables, RegistryFixture)
{
    registry.add<MockEventController>();  // Not IUpdatable
    auto& updatable = registry.add<MockUpdatableController>();

    registry.subscribeAll();
    registry.updateAll(0.016f);

    // Only the updatable should have been called
    BOOST_CHECK_EQUAL(updatable.getUpdateCount(), 1);
}

BOOST_FIXTURE_TEST_CASE(TestUpdateAllMultipleUpdatables, RegistryFixture)
{
    auto& updatable1 = registry.add<MockUpdatableController>();
    auto& updatable2 = registry.add<MockUpdatableController2>();

    registry.subscribeAll();
    registry.updateAll(0.016f);

    BOOST_CHECK_EQUAL(updatable1.getUpdateCount(), 1);
    BOOST_CHECK_EQUAL(updatable2.getUpdateCount(), 1);
}

BOOST_FIXTURE_TEST_CASE(TestUpdateAllAccumulatesTime, RegistryFixture)
{
    auto& updatable = registry.add<MockUpdatableController>();

    registry.subscribeAll();

    registry.updateAll(0.016f);
    registry.updateAll(0.016f);
    registry.updateAll(0.016f);

    BOOST_CHECK_EQUAL(updatable.getUpdateCount(), 3);
    BOOST_CHECK_CLOSE(updatable.getTotalTime(), 0.048f, 0.0001f);
}

BOOST_FIXTURE_TEST_CASE(TestUpdateAllSkipsSuspended, RegistryFixture)
{
    auto& updatable = registry.add<MockUpdatableController>();

    registry.subscribeAll();
    registry.updateAll(0.016f);
    BOOST_CHECK_EQUAL(updatable.getUpdateCount(), 1);

    registry.suspendAll();
    registry.updateAll(0.016f);  // Should be skipped

    BOOST_CHECK_EQUAL(updatable.getUpdateCount(), 1);  // Still 1

    registry.resumeAll();
    registry.updateAll(0.016f);

    BOOST_CHECK_EQUAL(updatable.getUpdateCount(), 2);  // Now 2
}

BOOST_AUTO_TEST_SUITE_END()

// --- Clear and Lifecycle Tests ---

BOOST_AUTO_TEST_SUITE(LifecycleTests)

BOOST_FIXTURE_TEST_CASE(TestClear, RegistryFixture)
{
    registry.add<MockEventController>();
    registry.add<MockUpdatableController>();
    registry.subscribeAll();

    BOOST_CHECK_EQUAL(registry.size(), 2u);

    registry.clear();

    BOOST_CHECK(registry.empty());
    BOOST_CHECK_EQUAL(registry.size(), 0u);
    BOOST_CHECK(!registry.has<MockEventController>());
    BOOST_CHECK(!registry.has<MockUpdatableController>());
}

BOOST_FIXTURE_TEST_CASE(TestClearUnsubscribesFirst, RegistryFixture)
{
    auto& ctrl = registry.add<MockEventController>();
    registry.subscribeAll();
    BOOST_CHECK(ctrl.isSubscribed());

    // Store pointer before clear (controller will be destroyed)
    bool wasSubscribed = ctrl.isSubscribed();
    BOOST_CHECK(wasSubscribed);

    registry.clear();

    // After clear, registry is empty
    BOOST_CHECK(registry.empty());
}

BOOST_FIXTURE_TEST_CASE(TestClearRemovesEventHandlerTokens, RegistryFixture)
{
    TokenOwningController::resetEventCount();
    registry.add<TokenOwningController>();
    registry.subscribeAll();

    auto event = std::make_shared<TokenCleanupEvent>();
    BOOST_REQUIRE(EventManager::Instance().dispatchEvent(
        event, EventManager::DispatchMode::Immediate));
    BOOST_CHECK_EQUAL(TokenOwningController::eventCount(), 1);

    registry.clear();

    BOOST_CHECK(!EventManager::Instance().dispatchEvent(
        event, EventManager::DispatchMode::Immediate));
    BOOST_CHECK_EQUAL(TokenOwningController::eventCount(), 1);
}

BOOST_FIXTURE_TEST_CASE(TestReAddAfterClear, RegistryFixture)
{
    registry.add<MockEventController>();
    registry.clear();

    // Should be able to add again
    auto& ctrl = registry.add<MockEventController>();
    BOOST_CHECK_EQUAL(registry.size(), 1u);
    BOOST_CHECK_EQUAL(ctrl.getName(), "MockEventController");
}

BOOST_FIXTURE_TEST_CASE(TestMoveConstruction, RegistryFixture)
{
    registry.add<MockEventController>();
    registry.add<MockUpdatableController>();
    registry.subscribeAll();

    ControllerRegistry movedRegistry = std::move(registry);

    BOOST_CHECK_EQUAL(movedRegistry.size(), 2u);
    BOOST_CHECK(movedRegistry.has<MockEventController>());
    BOOST_CHECK(movedRegistry.has<MockUpdatableController>());
}

BOOST_FIXTURE_TEST_CASE(TestMoveAssignment, RegistryFixture)
{
    registry.add<MockEventController>();
    registry.subscribeAll();

    ControllerRegistry otherRegistry;
    otherRegistry = std::move(registry);

    BOOST_CHECK_EQUAL(otherRegistry.size(), 1u);
    BOOST_CHECK(otherRegistry.has<MockEventController>());
}

BOOST_AUTO_TEST_SUITE_END()

// --- Empty Registry Edge Cases ---

BOOST_AUTO_TEST_SUITE(EmptyRegistryTests)

BOOST_FIXTURE_TEST_CASE(TestSubscribeAllOnEmpty, RegistryFixture)
{
    // Should not crash
    registry.subscribeAll();
    BOOST_CHECK(registry.empty());
}

BOOST_FIXTURE_TEST_CASE(TestUnsubscribeAllOnEmpty, RegistryFixture)
{
    // Should not crash
    registry.unsubscribeAll();
    BOOST_CHECK(registry.empty());
}

BOOST_FIXTURE_TEST_CASE(TestSuspendAllOnEmpty, RegistryFixture)
{
    // Should not crash
    registry.suspendAll();
    BOOST_CHECK(registry.empty());
}

BOOST_FIXTURE_TEST_CASE(TestResumeAllOnEmpty, RegistryFixture)
{
    // Should not crash
    registry.resumeAll();
    BOOST_CHECK(registry.empty());
}

BOOST_FIXTURE_TEST_CASE(TestUpdateAllOnEmpty, RegistryFixture)
{
    // Should not crash
    registry.updateAll(0.016f);
    BOOST_CHECK(registry.empty());
}

BOOST_FIXTURE_TEST_CASE(TestClearOnEmpty, RegistryFixture)
{
    // Should not crash
    registry.clear();
    BOOST_CHECK(registry.empty());
}

BOOST_AUTO_TEST_SUITE_END()
