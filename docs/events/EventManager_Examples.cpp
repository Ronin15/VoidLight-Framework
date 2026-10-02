/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

/**
 * @file EventManager_Examples.cpp
 * @brief Branch-accurate EventManager usage examples
 *
 * These snippets reflect the current dispatch-hub design:
 * - handler registration by EventTypeId
 * - deferred queue draining via update()
 * - token-based teardown
 * - worker-thread batch enqueue
 */

#include "managers/EventManager.hpp"
#include "events/ResourceChangeEvent.hpp"
#include <iostream>
#include <vector>

//=============================================================================
// Example 1: Register a state-scoped handler
//=============================================================================

void example1_StateScopedHandler() {
    auto& eventMgr = EventManager::Instance();
    eventMgr.init();

    EventManager::HandlerToken token = eventMgr.registerHandlerWithToken(
        EventTypeId::ResourceChange,
        [](const EventData& data) {
            std::cout << "Resource change received\n";
            (void)data;
        });

    eventMgr.removeHandler(token);
}

//=============================================================================
// Example 2: Deferred dispatch in the main update loop
//=============================================================================

void example2_DeferredDispatch() {
    auto& eventMgr = EventManager::Instance();

    auto event = std::make_shared<ResourceChangeEvent>();
    event->set({}, 0, 5);

    EventData data;
    data.event = event;
    data.typeId = EventTypeId::ResourceChange;
    data.setActive(true);

    std::vector<EventManager::DeferredEvent> batch;
    batch.push_back({EventTypeId::ResourceChange, data});

    eventMgr.enqueueBatch(std::move(batch));
    eventMgr.update();  // drains deferred queue
}

//=============================================================================
// Example 3: Worker-thread batch enqueue pattern
//=============================================================================

void example3_WorkerBatchPattern() {
    std::vector<EventManager::DeferredEvent> localDeferredEvents;
    localDeferredEvents.reserve(8);

    // Fill localDeferredEvents inside worker processing...

    EventManager::Instance().enqueueBatch(std::move(localDeferredEvents));
}

//=============================================================================
// Example 4: Test-time deterministic draining
//=============================================================================

void example4_TestDrain() {
    auto& eventMgr = EventManager::Instance();
    eventMgr.drainAllDeferredEvents();
}

//=============================================================================
// Example 5: State transition cleanup
//=============================================================================

void example5_StateTransitionCleanup() {
    auto& eventMgr = EventManager::Instance();
    eventMgr.prepareForStateTransition();
}
