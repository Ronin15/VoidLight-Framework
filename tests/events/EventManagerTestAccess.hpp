// Test helper for EventManager state reset. Not part of public API.
#ifndef TESTS_EVENTS_EVENT_MANAGER_TEST_ACCESS_HPP
#define TESTS_EVENTS_EVENT_MANAGER_TEST_ACCESS_HPP

class EventManagerTestAccess {
public:
    // Fully reset EventManager to a clean state for test isolation
    static void reset();
};

#endif // TESTS_EVENTS_EVENT_MANAGER_TEST_ACCESS_HPP
