#include "EventManagerTestAccess.hpp"

#include "managers/EventManager.hpp"
#include <stdexcept>

void EventManagerTestAccess::reset() {
    // Ensure any in-flight work is ended and internal state is consistent
    EventManager::Instance().prepareForStateTransition();
    // Clear handlers explicitly to avoid cross-test interference
    EventManager::Instance().clearAllHandlers();
    // Full teardown and re-init
    EventManager::Instance().clean();
    if (!EventManager::Instance().init()) {
        throw std::runtime_error("EventManager re-init failed in EventManagerTestAccess::reset()");
    }
}

