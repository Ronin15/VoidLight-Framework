/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ThreadSafeAIManagerTests
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "core/ThreadSystem.hpp"
#include "core/WorkerBudget.hpp"
#include "managers/AIManager.hpp"
#include "managers/BackgroundSimulationManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "managers/EntityDataManager.hpp"

// Test helper for data-driven NPCs
class TestNPC {
public:
  explicit TestNPC(const Vector2D &pos = Vector2D(0, 0)) {
    auto& edm = EntityDataManager::Instance();
    m_handle = edm.createNPCWithRaceClass(pos, "Human", "Guard");
    m_initialPosition = pos;
  }

  static std::shared_ptr<TestNPC> create(const Vector2D &pos = Vector2D(0, 0)) {
    return std::make_shared<TestNPC>(pos);
  }

  [[nodiscard]] EntityHandle getHandle() const { return m_handle; }

  // Check if entity was processed (position or velocity changed)
  bool wasUpdated() const {
    if (!m_handle.isValid()) return false;

    auto& edm = EntityDataManager::Instance();
    size_t index = edm.getIndex(m_handle);
    if (index == SIZE_MAX) return false;

    auto& transform = edm.getTransformByIndex(index);
    Vector2D currentPos = transform.position;
    Vector2D velocity = transform.velocity;

    bool positionMoved = (currentPos - m_initialPosition).length() > 0.01f;
    bool hasVelocity = velocity.length() > 0.01f;

    return positionMoved || hasVelocity;
  }

  void resetInitialPosition() {
    if (m_handle.isValid()) {
      auto& edm = EntityDataManager::Instance();
      size_t index = edm.getIndex(m_handle);
      if (index != SIZE_MAX) {
        m_initialPosition = edm.getTransformByIndex(index).position;
      }
    }
  }

private:
  EntityHandle m_handle;
  Vector2D m_initialPosition;
};

// Global fixture for test setup and cleanup
struct ThreadSafeAIFixture {
  ThreadSafeAIFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
      throw std::runtime_error("ThreadSystem::init() failed");
    }
    if (!EntityDataManager::Instance().init()) {
      throw std::runtime_error("EntityDataManager::init() failed");
    }
    if (!CollisionManager::Instance().init()) {
      throw std::runtime_error("CollisionManager::init() failed");
    }
    if (!PathfinderManager::Instance().init()) {
      throw std::runtime_error("PathfinderManager::init() failed");
    }
    VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    if (!AIManager::Instance().init()) {
      throw std::runtime_error("AIManager::init() failed");
    }
    if (!BackgroundSimulationManager::Instance().init()) {
      throw std::runtime_error("BackgroundSimulationManager::init() failed");
    }
  }

  ~ThreadSafeAIFixture() {
    BackgroundSimulationManager::Instance().clean();
    AIManager::Instance().clean();
    VoidLight::WorkerBudgetManager::Instance().prepareForStateTransition();
    PathfinderManager::Instance().clean();
    CollisionManager::Instance().clean();
    EntityDataManager::Instance().clean();
    VoidLight::ThreadSystem::Instance().clean();
  }
};

BOOST_GLOBAL_FIXTURE(ThreadSafeAIFixture);

// Helper to update AI with proper tier calculation
void updateAI(float deltaTime, const Vector2D& referencePoint = Vector2D(500.0f, 500.0f)) {
  BackgroundSimulationManager::Instance().invalidateTiers();
  BackgroundSimulationManager::Instance().update(referencePoint, deltaTime);
  AIManager::Instance().update(deltaTime);
}

void destroyTestEntity(EntityHandle handle) {
  if (!handle.isValid()) {
    return;
  }

  AIManager::Instance().unassignBehavior(handle);
  auto& edm = EntityDataManager::Instance();
  edm.destroyEntity(handle);
  edm.processDestructionQueue();
}

void destroyTestEntities(const std::vector<EntityHandle>& handles) {
  auto& edm = EntityDataManager::Instance();
  for (const auto& handle : handles) {
    AIManager::Instance().unassignBehavior(handle);
    edm.destroyEntity(handle);
  }
  edm.processDestructionQueue();
}

void primeAIThreadingDecision(size_t workloadSize) {
  auto& budgetMgr = VoidLight::WorkerBudgetManager::Instance();
  budgetMgr.prepareForStateTransition();
  for (int sample = 0; sample < 10; ++sample) {
    budgetMgr.reportExecution(VoidLight::SystemType::AI, workloadSize, false, 1, 5.0);
  }
  BOOST_REQUIRE(budgetMgr.shouldUseThreading(VoidLight::SystemType::AI, workloadSize).shouldThread);
}

// ===========================================================================
// Test Cases
// ===========================================================================

BOOST_AUTO_TEST_SUITE(ThreadSafeAIManagerTests)

// Test basic entity registration and behavior assignment
BOOST_AUTO_TEST_CASE(BasicEntityRegistration)
{
  auto npc = TestNPC::create(Vector2D(100.0f, 100.0f));
  EntityHandle handle = npc->getHandle();

  BOOST_REQUIRE(handle.isValid());

  AIManager::Instance().assignBehavior(handle, "Wander");
  BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

  // Clean up
  destroyTestEntity(handle);
}

BOOST_AUTO_TEST_CASE(MainThreadBehaviorAssignmentBeforeThreadedUpdate)
{
  constexpr int TOTAL_ENTITIES = 160;

  std::vector<std::shared_ptr<TestNPC>> npcs;
  std::vector<EntityHandle> allHandles;
  npcs.reserve(TOTAL_ENTITIES);
  allHandles.reserve(TOTAL_ENTITIES);

  for (int i = 0; i < TOTAL_ENTITIES; ++i) {
    Vector2D pos(static_cast<float>(i % 40) * 10.0f,
                 static_cast<float>(i / 40) * 10.0f);
    auto npc = TestNPC::create(pos);
    EntityHandle handle = npc->getHandle();
    BOOST_REQUIRE(handle.isValid());
    npcs.push_back(npc);
    allHandles.push_back(handle);
    AIManager::Instance().assignBehavior(handle, "Wander");
  }

  for (const auto& handle : allHandles) {
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
  }

  primeAIThreadingDecision(allHandles.size());
  updateAI(0.016f, Vector2D(200.0f, 200.0f));

  // Clean up
  destroyTestEntities(allHandles);
}

// Test AI update with multiple entities
BOOST_AUTO_TEST_CASE(MultipleEntityUpdate)
{
  constexpr int NUM_ENTITIES = 50;
  std::vector<std::shared_ptr<TestNPC>> npcs;
  std::vector<EntityHandle> handles;

  // Create and register entities
  for (int i = 0; i < NUM_ENTITIES; ++i) {
    Vector2D pos(i * 20.0f, i * 20.0f);
    auto npc = TestNPC::create(pos);
    EntityHandle handle = npc->getHandle();
    AIManager::Instance().assignBehavior(handle, "Wander");
    npcs.push_back(npc);
    handles.push_back(handle);
  }

  // Run AI update
  updateAI(0.016f);

  // Check that entities have behaviors assigned
  for (const auto& handle : handles) {
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
  }

  // Clean up
  destroyTestEntities(handles);
}

BOOST_AUTO_TEST_CASE(MainThreadBehaviorReassignmentBeforeUpdate)
{
  constexpr int NUM_ENTITIES = 120;
  std::vector<std::shared_ptr<TestNPC>> npcs;
  std::vector<EntityHandle> handles;

  // Create entities
  for (int i = 0; i < NUM_ENTITIES; ++i) {
    Vector2D pos(i * 20.0f, i * 20.0f);
    auto npc = TestNPC::create(pos);
    handles.push_back(npc->getHandle());
    npcs.push_back(npc);
  }

  for (const auto& handle : handles) {
    AIManager::Instance().assignBehavior(handle, "Idle");
  }
  updateAI(0.016f, Vector2D(200.0f, 200.0f));

  for (const auto& handle : handles) {
    AIManager::Instance().assignBehavior(handle, "Wander");
  }
  primeAIThreadingDecision(handles.size());
  updateAI(0.016f, Vector2D(200.0f, 200.0f));

  // Verify all entities have behaviors
  for (const auto& handle : handles) {
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
  }

  // Clean up
  destroyTestEntities(handles);
}

// Test message sending
BOOST_AUTO_TEST_CASE(MessageSending)
{
  auto npc = TestNPC::create(Vector2D(100.0f, 100.0f));
  EntityHandle handle = npc->getHandle();

  AIManager::Instance().assignBehavior(handle, "Idle");
  const size_t initialUpdates = AIManager::Instance().getBehaviorUpdateCount();

  // Legacy string message API was removed - message system now uses BehaviorMessage queue

  // Update to process messages
  updateAI(0.016f);

  // Clean up
  destroyTestEntity(handle);

  BOOST_CHECK_GT(AIManager::Instance().getBehaviorUpdateCount(), initialUpdates);
}

// Test rapid assignment/unassignment
BOOST_AUTO_TEST_CASE(RapidAssignmentUnassignment)
{
  auto npc = TestNPC::create(Vector2D(100.0f, 100.0f));
  EntityHandle handle = npc->getHandle();

  for (int i = 0; i < 100; ++i) {
    AIManager::Instance().assignBehavior(handle, "Wander");
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    AIManager::Instance().unassignBehavior(handle);
    // After unassign, hasBehavior should return false
  }

  // Clean up
  destroyTestEntity(handle);
}

// Test global pause
BOOST_AUTO_TEST_CASE(GlobalPause)
{
  auto npc = TestNPC::create(Vector2D(100.0f, 100.0f));
  EntityHandle handle = npc->getHandle();
  npc->resetInitialPosition();

  AIManager::Instance().assignBehavior(handle, "Wander");

  // Pause and update - entity should not be updated
  AIManager::Instance().setGlobalPause(true);
  BOOST_CHECK(AIManager::Instance().isGloballyPaused());

  updateAI(0.016f);

  // Resume
  AIManager::Instance().setGlobalPause(false);
  BOOST_CHECK(!AIManager::Instance().isGloballyPaused());

  // Clean up
  destroyTestEntity(handle);
}

// Test different behavior types
BOOST_AUTO_TEST_CASE(DifferentBehaviorTypes)
{
  std::vector<std::string> behaviorTypes = {"Idle", "Wander", "Patrol", "Guard"};
  std::vector<std::shared_ptr<TestNPC>> npcs;
  std::vector<EntityHandle> handles;

  for (size_t i = 0; i < behaviorTypes.size(); ++i) {
    Vector2D pos(i * 50.0f, i * 50.0f);
    auto npc = TestNPC::create(pos);
    EntityHandle handle = npc->getHandle();

    AIManager::Instance().assignBehavior(handle, behaviorTypes[i]);
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));

    npcs.push_back(npc);
    handles.push_back(handle);
  }

  // Update all
  updateAI(0.016f);

  // Verify behaviors still assigned
  for (const auto& handle : handles) {
    BOOST_CHECK(AIManager::Instance().hasBehavior(handle));
  }

  // Clean up
  destroyTestEntities(handles);
}

// Test high entity count
BOOST_AUTO_TEST_CASE(HighEntityCount)
{
  constexpr int NUM_ENTITIES = 500;
  std::vector<std::shared_ptr<TestNPC>> npcs;
  std::vector<EntityHandle> handles;

  // Create many entities
  for (int i = 0; i < NUM_ENTITIES; ++i) {
    float x = static_cast<float>(i % 50) * 20.0f;
    float y = static_cast<float>(i / 50) * 20.0f;
    auto npc = TestNPC::create(Vector2D(x, y));
    EntityHandle handle = npc->getHandle();
    AIManager::Instance().assignBehavior(handle, "Wander");
    npcs.push_back(npc);
    handles.push_back(handle);
  }

  primeAIThreadingDecision(handles.size());

  // Update enough frames to exercise the learned WorkerBudget threaded path.
  for (int frame = 0; frame < 12; ++frame) {
    updateAI(0.016f);
  }

  // Verify no crashes and entities have behaviors
  int assignedCount = 0;
  for (const auto& handle : handles) {
    if (AIManager::Instance().hasBehavior(handle)) {
      assignedCount++;
    }
  }

  BOOST_CHECK_EQUAL(assignedCount, NUM_ENTITIES);

  // Clean up
  destroyTestEntities(handles);
}

BOOST_AUTO_TEST_SUITE_END()
