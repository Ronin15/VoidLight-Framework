/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE NPCMemoryTests
#include <boost/test/unit_test.hpp>

#include "ai/BehaviorExecutors.hpp"
#include "core/ThreadSystem.hpp"
#include "managers/AIManager.hpp"
#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include "entities/EntityHandle.hpp"
#include "utils/Vector2D.hpp"
#include <cmath>
#include <vector>

// Test tolerance for floating-point comparisons
constexpr float EPSILON = 0.001f;

bool approxEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

struct ThreadSystemFixture {
    ThreadSystemFixture() {
        if (!VoidLight::ThreadSystem::Instance().init()) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }
    ~ThreadSystemFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};
BOOST_GLOBAL_FIXTURE(ThreadSystemFixture);

// ============================================================================
// Test Fixture
// ============================================================================

class NPCMemoryTestFixture {
public:
    NPCMemoryTestFixture() {
        edm = &EntityDataManager::Instance();
        BOOST_REQUIRE(edm->init());
        CollisionManager::Instance().init();
        PathfinderManager::Instance().init();
        AIManager::Instance().init();
    }

    ~NPCMemoryTestFixture() {
        AIManager::Instance().clean();
        PathfinderManager::Instance().clean();
        CollisionManager::Instance().clean();
        edm->clean();
    }

protected:
    EntityDataManager* edm;

    // Helper to create a test NPC and get its EDM index
    std::pair<EntityHandle, size_t> createTestNPC(float x = 100.0f, float y = 100.0f) {
        EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(x, y), "Human", "Guard");
        if (!handle.isValid()) {
            // Fallback if race/class not loaded
            handle = edm->registerPlayer(1, Vector2D(x, y), 16.0f, 16.0f);
        }
        size_t index = edm->getIndex(handle);
        return {handle, index};
    }
};

// ============================================================================
// MEMORY DATA STRUCTURE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(MemoryStructureTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestMemoryEntrySize) {
    // MemoryEntry should be compact (under 40 bytes)
    BOOST_CHECK(sizeof(MemoryEntry) <= 40);
}

BOOST_AUTO_TEST_CASE(TestEmotionalStateSize) {
    // EmotionalState should be exactly 16 bytes
    BOOST_CHECK_EQUAL(sizeof(EmotionalState), 16);
}

BOOST_AUTO_TEST_CASE(TestNPCMemoryDataSize) {
    // NPCMemoryData must be exactly 448 bytes (7 cache lines) — layout locked by DOD discipline.
    // First 64 B: fields read every frame by every behavior.
    // Next 64 B: lastTarget — combat behaviors fault one extra line, not multiple.
    // Remainder: event-driven and Guard-only fields.
    BOOST_CHECK_EQUAL(sizeof(NPCMemoryData), 448u);
}

BOOST_AUTO_TEST_CASE(TestMemoryEntryClearing) {
    MemoryEntry entry;
    entry.subject = EntityHandle{123, EntityKind::NPC, 1};
    entry.location = Vector2D{100.0f, 200.0f};
    entry.timestamp = 10.0f;
    entry.value = 50.0f;
    entry.type = MemoryType::DamageReceived;
    entry.importance = 100;
    entry.flags = MemoryEntry::FLAG_VALID;

    entry.clear();

    BOOST_CHECK(!entry.isValid());
    BOOST_CHECK_EQUAL(entry.timestamp, 0.0f);
    BOOST_CHECK_EQUAL(entry.value, 0.0f);
    BOOST_CHECK_EQUAL(entry.importance, 0);
}

BOOST_AUTO_TEST_CASE(TestEmotionalStateDecay) {
    EmotionalState emotions;
    emotions.aggression = 1.0f;
    emotions.fear = 0.8f;
    emotions.curiosity = 0.5f;
    emotions.suspicion = 0.3f;

    // Decay at 10% per second for 1 second
    emotions.decay(0.1f, 1.0f);

    BOOST_CHECK(approxEqual(emotions.aggression, 0.9f));
    BOOST_CHECK(approxEqual(emotions.fear, 0.72f));
    BOOST_CHECK(approxEqual(emotions.curiosity, 0.45f));
    BOOST_CHECK(approxEqual(emotions.suspicion, 0.27f));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// MEMORY INITIALIZATION TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(MemoryInitTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestMemoryDataPreallocated) {
    auto [handle, index] = createTestNPC();

    // Memory data should exist for the entity (pre-allocated with entity)
    BOOST_CHECK(index < 1000000);  // Valid index
    BOOST_CHECK(edm->hasMemoryData(index));

    // Verify default emotional state on a freshly created NPC
    const auto& memData = edm->getMemoryData(index);
    BOOST_CHECK_EQUAL(memData.emotions.fear, 0.0f);
}

BOOST_AUTO_TEST_CASE(TestInitMemoryData) {
    auto [handle, index] = createTestNPC();

    edm->initMemoryData(index);

    BOOST_CHECK(edm->hasMemoryData(index));

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(memData.isValid());
    BOOST_CHECK_EQUAL(memData.memoryCount, 0);
    BOOST_CHECK_EQUAL(memData.combatEncounters, 0);
    BOOST_CHECK(!memData.hasOverflow());
}

BOOST_AUTO_TEST_CASE(TestClearMemoryData) {
    auto [handle, index] = createTestNPC();

    edm->initMemoryData(index);

    // Add some data
    MemoryEntry entry;
    entry.type = MemoryType::ThreatSpotted;
    entry.flags = MemoryEntry::FLAG_VALID;
    edm->addMemory(index, entry);

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(memData.memoryCount > 0);

    edm->clearMemoryData(index);

    // Memory should be cleared but structure still accessible
    auto& cleared = edm->getMemoryData(index);
    BOOST_CHECK(!cleared.isValid());
    BOOST_CHECK_EQUAL(cleared.memoryCount, 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ADD MEMORY TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(AddMemoryTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestAddSingleMemory) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    MemoryEntry entry;
    entry.subject = EntityHandle{999, EntityKind::Player, 1};
    entry.location = Vector2D{50.0f, 75.0f};
    entry.timestamp = 5.0f;
    entry.value = 25.0f;
    entry.type = MemoryType::AttackedBy;
    entry.importance = 200;
    entry.flags = MemoryEntry::FLAG_VALID;

    edm->addMemory(index, entry);

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK_EQUAL(memData.memoryCount, 1);
}

BOOST_AUTO_TEST_CASE(TestAddMultipleMemories) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    for (int i = 0; i < 5; ++i) {
        MemoryEntry entry;
        entry.type = MemoryType::LocationVisited;
        entry.timestamp = static_cast<float>(i);
        entry.flags = MemoryEntry::FLAG_VALID;
        edm->addMemory(index, entry);
    }

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK_EQUAL(memData.memoryCount, 5);
}

BOOST_AUTO_TEST_CASE(TestInlineMemoryCircularBuffer) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    // Add more than INLINE_MEMORY_COUNT memories without overflow
    for (int i = 0; i < 10; ++i) {
        MemoryEntry entry;
        entry.type = MemoryType::ThreatSpotted;
        entry.timestamp = static_cast<float>(i);
        entry.flags = MemoryEntry::FLAG_VALID;
        edm->addMemory(index, entry, false);  // No overflow
    }

    auto& memData = edm->getMemoryData(index);
    // Should cap at INLINE_MEMORY_COUNT (circular buffer overwrites oldest)
    BOOST_CHECK_EQUAL(memData.memoryCount, NPCMemoryData::INLINE_MEMORY_COUNT);
    BOOST_CHECK(!memData.hasOverflow());
}

BOOST_AUTO_TEST_CASE(TestMemoryOverflow) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    // Add more than INLINE_MEMORY_COUNT memories with overflow enabled
    for (int i = 0; i < 10; ++i) {
        MemoryEntry entry;
        entry.type = MemoryType::WitnessedCombat;
        entry.timestamp = static_cast<float>(i);
        entry.flags = MemoryEntry::FLAG_VALID;
        edm->addMemory(index, entry, true);  // Use overflow
    }

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK_EQUAL(memData.memoryCount, 10);
    BOOST_CHECK(memData.hasOverflow());

    // Verify overflow sidecar entry exists, keyed by edmIdx
    auto* overflow = edm->getMemoryOverflow(index);
    BOOST_CHECK(overflow != nullptr);
    BOOST_CHECK_EQUAL(overflow->extraMemories.size(), 10 - NPCMemoryData::INLINE_MEMORY_COUNT);
}

BOOST_AUTO_TEST_CASE(TestMemoryOverflowSidecarHasEntry) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    // No overflow before inline slots fill
    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(!memData.hasOverflow());

    // Fill inline slots and push one into overflow
    for (size_t i = 0; i <= NPCMemoryData::INLINE_MEMORY_COUNT; ++i) {
        MemoryEntry entry;
        entry.type = MemoryType::WitnessedCombat;
        entry.timestamp = static_cast<float>(i);
        entry.flags = MemoryEntry::FLAG_VALID;
        edm->addMemory(index, entry, true);
    }

    // FLAG_HAS_OVERFLOW must be set and sidecar must have an entry for this edmIdx
    BOOST_CHECK(memData.hasOverflow());
    BOOST_CHECK(edm->getMemoryOverflow(index) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// FIND MEMORY TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(FindMemoryTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestFindMemoriesByType) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    // Add mixed memory types
    for (int i = 0; i < 3; ++i) {
        MemoryEntry entry;
        entry.type = MemoryType::DamageReceived;
        entry.value = static_cast<float>(i * 10);
        entry.flags = MemoryEntry::FLAG_VALID;
        edm->addMemory(index, entry);
    }

    MemoryEntry otherEntry;
    otherEntry.type = MemoryType::Interaction;
    otherEntry.flags = MemoryEntry::FLAG_VALID;
    edm->addMemory(index, otherEntry);

    std::vector<const MemoryEntry*> results;
    edm->findMemoriesByType(index, MemoryType::DamageReceived, results);

    BOOST_CHECK_EQUAL(results.size(), 3);
    for (const auto* mem : results) {
        BOOST_CHECK_EQUAL(static_cast<int>(mem->type), static_cast<int>(MemoryType::DamageReceived));
    }
}

BOOST_AUTO_TEST_CASE(TestFindMemoriesByTypeWithLimit) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    for (int i = 0; i < 5; ++i) {
        MemoryEntry entry;
        entry.type = MemoryType::ThreatSpotted;
        entry.flags = MemoryEntry::FLAG_VALID;
        edm->addMemory(index, entry);
    }

    std::vector<const MemoryEntry*> results;
    edm->findMemoriesByType(index, MemoryType::ThreatSpotted, results, 2);

    BOOST_CHECK_EQUAL(results.size(), 2);
}

BOOST_AUTO_TEST_CASE(TestFindMemoriesOfEntity) {
    auto [handle, index] = createTestNPC();
    auto [targetHandle, targetIndex] = createTestNPC(200.0f, 200.0f);
    edm->initMemoryData(index);

    // Add memories about the target
    MemoryEntry entry1;
    entry1.subject = targetHandle;
    entry1.type = MemoryType::AttackedBy;
    entry1.flags = MemoryEntry::FLAG_VALID;
    edm->addMemory(index, entry1);

    MemoryEntry entry2;
    entry2.subject = targetHandle;
    entry2.type = MemoryType::DamageReceived;
    entry2.flags = MemoryEntry::FLAG_VALID;
    edm->addMemory(index, entry2);

    // Add memory about different entity
    MemoryEntry entry3;
    entry3.subject = EntityHandle{9999, EntityKind::NPC, 1};
    entry3.type = MemoryType::AllySpotted;
    entry3.flags = MemoryEntry::FLAG_VALID;
    edm->addMemory(index, entry3);

    std::vector<const MemoryEntry*> results;
    edm->findMemoriesOfEntity(index, targetHandle, results);

    BOOST_CHECK_EQUAL(results.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// EMOTIONAL STATE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(EmotionalStateTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestModifyEmotions) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    edm->modifyEmotions(index, 0.5f, 0.3f, 0.2f, 0.1f);

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(approxEqual(memData.emotions.aggression, 0.5f));
    BOOST_CHECK(approxEqual(memData.emotions.fear, 0.3f));
    BOOST_CHECK(approxEqual(memData.emotions.curiosity, 0.2f));
    BOOST_CHECK(approxEqual(memData.emotions.suspicion, 0.1f));
}

BOOST_AUTO_TEST_CASE(TestEmotionsClamping) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    // Try to set emotions above 1.0
    edm->modifyEmotions(index, 2.0f, 2.0f, 2.0f, 2.0f);

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(approxEqual(memData.emotions.aggression, 1.0f));
    BOOST_CHECK(approxEqual(memData.emotions.fear, 1.0f));

    // Try to set emotions below 0.0
    edm->modifyEmotions(index, -3.0f, -3.0f, -3.0f, -3.0f);

    BOOST_CHECK(approxEqual(memData.emotions.aggression, 0.0f));
    BOOST_CHECK(approxEqual(memData.emotions.fear, 0.0f));
}

BOOST_AUTO_TEST_CASE(TestUpdateEmotionalDecay) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    edm->modifyEmotions(index, 1.0f, 1.0f, 1.0f, 1.0f);

    // Decay over 2 seconds at 5% per second
    edm->updateEmotionalDecay(index, 2.0f, 0.05f);

    auto& memData = edm->getMemoryData(index);
    // 1.0 * (1 - 0.05 * 2) = 0.9
    BOOST_CHECK(approxEqual(memData.emotions.aggression, 0.9f));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// COMBAT EVENT TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(CombatEventTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestRecordCombatEventReceived) {
    auto [handle, index] = createTestNPC();
    EntityHandle attacker{999, EntityKind::Player, 1};

    edm->recordCombatEvent(index, attacker, handle, 25.0f, true, 10.0f);

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(memData.isValid());
    BOOST_CHECK(memData.lastAttacker == attacker);
    BOOST_CHECK(approxEqual(memData.totalDamageReceived, 25.0f));
    BOOST_CHECK_EQUAL(memData.combatEncounters, 1);
    BOOST_CHECK(memData.lastCombatTime < Behaviors::COMBAT_TIMEOUT_SECONDS);
}

BOOST_AUTO_TEST_CASE(TestRecordCombatEventDealt) {
    auto [handle, index] = createTestNPC();
    EntityHandle target{888, EntityKind::NPC, 1};

    edm->recordCombatEvent(index, handle, target, 30.0f, false, 15.0f);

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(memData.lastTarget == target);
    BOOST_CHECK(approxEqual(memData.totalDamageDealt, 30.0f));
}

BOOST_AUTO_TEST_CASE(TestMultipleCombatEvents) {
    auto [handle, index] = createTestNPC();
    EntityHandle attacker{999, EntityKind::Player, 1};

    edm->recordCombatEvent(index, attacker, handle, 10.0f, true, 1.0f);
    edm->recordCombatEvent(index, attacker, handle, 15.0f, true, 2.0f);
    edm->recordCombatEvent(index, attacker, handle, 20.0f, true, 3.0f);

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(approxEqual(memData.totalDamageReceived, 45.0f));
    BOOST_CHECK_EQUAL(memData.combatEncounters, 3);
    BOOST_CHECK(approxEqual(memData.lastCombatTime, 0.0f));  // Delta semantics: always reset to 0
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// LOCATION HISTORY TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(LocationHistoryTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestAddLocationToHistory) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    edm->addLocationToHistory(index, Vector2D{100.0f, 100.0f});
    edm->addLocationToHistory(index, Vector2D{200.0f, 200.0f});

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK_EQUAL(memData.locationCount, 2);
}

BOOST_AUTO_TEST_CASE(TestLocationHistoryCircular) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    // Add more locations than INLINE_LOCATION_COUNT
    for (int i = 0; i < 10; ++i) {
        edm->addLocationToHistory(index, Vector2D{static_cast<float>(i * 100), 0.0f});
    }

    auto& memData = edm->getMemoryData(index);
    // Should cap at INLINE_LOCATION_COUNT
    BOOST_CHECK_EQUAL(memData.locationCount, NPCMemoryData::INLINE_LOCATION_COUNT);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ENTITY DESTRUCTION CLEANUP TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(CleanupTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestMemoryClearedOnEntityDestruction) {
    auto [handle, index] = createTestNPC();
    edm->initMemoryData(index);

    // Add some memories with overflow
    for (int i = 0; i < 15; ++i) {
        MemoryEntry entry;
        entry.type = MemoryType::WitnessedCombat;
        entry.flags = MemoryEntry::FLAG_VALID;
        edm->addMemory(index, entry, true);
    }

    auto& memData = edm->getMemoryData(index);
    BOOST_CHECK(memData.hasOverflow());

    // Destroy the entity
    edm->destroyEntity(handle);
    edm->processDestructionQueue();

    // Overflow sidecar entry should be cleaned up (keyed by edmIdx)
    auto* overflow = edm->getMemoryOverflow(index);
    BOOST_CHECK(overflow == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// PERSONALITY TRAITS TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(PersonalityTraitsTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestPersonalityTraitsSize) {
    // PersonalityTraits should be exactly 16 bytes
    BOOST_CHECK_EQUAL(sizeof(PersonalityTraits), 16);
}

BOOST_AUTO_TEST_CASE(TestPersonalityTraitsDefaults) {
    PersonalityTraits traits;

    // All traits should default to 0.5 (neutral)
    BOOST_CHECK(approxEqual(traits.bravery, 0.5f));
    BOOST_CHECK(approxEqual(traits.aggression, 0.5f));
    BOOST_CHECK(approxEqual(traits.composure, 0.5f));
    BOOST_CHECK(approxEqual(traits.loyalty, 0.5f));
}

BOOST_AUTO_TEST_CASE(TestPersonalityTraitsClearing) {
    PersonalityTraits traits;
    traits.bravery = 0.8f;
    traits.aggression = 0.2f;
    traits.composure = 0.9f;
    traits.loyalty = 0.1f;

    traits.clear();

    // Should reset to defaults
    BOOST_CHECK(approxEqual(traits.bravery, 0.5f));
    BOOST_CHECK(approxEqual(traits.aggression, 0.5f));
    BOOST_CHECK(approxEqual(traits.composure, 0.5f));
    BOOST_CHECK(approxEqual(traits.loyalty, 0.5f));
}

BOOST_AUTO_TEST_CASE(TestPersonalityRandomization) {
    PersonalityTraits traits;
    std::mt19937 rng{42};  // Fixed seed for reproducibility

    traits.randomize(rng);

    // All traits should be in valid range [0, 1]
    BOOST_CHECK(traits.bravery >= 0.0f && traits.bravery <= 1.0f);
    BOOST_CHECK(traits.aggression >= 0.0f && traits.aggression <= 1.0f);
    BOOST_CHECK(traits.composure >= 0.0f && traits.composure <= 1.0f);
    BOOST_CHECK(traits.loyalty >= 0.0f && traits.loyalty <= 1.0f);

    // With normal distribution, most values should be near 0.5
    // (This is probabilistic but with fixed seed we know the result)
}

BOOST_AUTO_TEST_CASE(TestEffectiveResilienceCalculation) {
    PersonalityTraits traits;

    // Test with neutral personality (all 0.5)
    float classResilience = 0.8f;  // Guard-like high resilience
    float effective = traits.getEffectiveResilience(classResilience);

    // 60% class (0.48) + 40% personality average (0.2) = 0.68
    // Personality average = (0.5 + 0.5) / 2 * 0.4 = 0.2
    BOOST_CHECK(approxEqual(effective, 0.68f));

    // Test with brave and composed personality
    traits.bravery = 0.9f;
    traits.composure = 0.9f;
    effective = traits.getEffectiveResilience(classResilience);
    // 60% class (0.48) + 40% personality average (0.36) = 0.84
    BOOST_CHECK(approxEqual(effective, 0.84f));

    // Test with cowardly and reactive personality
    traits.bravery = 0.1f;
    traits.composure = 0.1f;
    effective = traits.getEffectiveResilience(classResilience);
    // 60% class (0.48) + 40% personality average (0.04) = 0.52
    BOOST_CHECK(approxEqual(effective, 0.52f));
}

BOOST_AUTO_TEST_CASE(TestNPCSpawnHasPersonality) {
    auto [handle, index] = createTestNPC();

    edm->initMemoryData(index);
    auto& memData = edm->getMemoryData(index);

    BOOST_CHECK(memData.isValid());

    // Personality should be randomized (not all defaults)
    // At least one trait should differ from 0.5 (default)
    // This is probabilistic but extremely likely to pass
    bool hasVariation =
        !approxEqual(memData.personality.bravery, 0.5f) ||
        !approxEqual(memData.personality.aggression, 0.5f) ||
        !approxEqual(memData.personality.composure, 0.5f) ||
        !approxEqual(memData.personality.loyalty, 0.5f);

    BOOST_CHECK_MESSAGE(hasVariation,
        "Personality should be randomized on spawn, not all 0.5 defaults");

    // All values should still be in valid range
    BOOST_CHECK(memData.personality.bravery >= 0.0f && memData.personality.bravery <= 1.0f);
    BOOST_CHECK(memData.personality.aggression >= 0.0f && memData.personality.aggression <= 1.0f);
    BOOST_CHECK(memData.personality.composure >= 0.0f && memData.personality.composure <= 1.0f);
    BOOST_CHECK(memData.personality.loyalty >= 0.0f && memData.personality.loyalty <= 1.0f);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// EMOTIONAL RESILIENCE TESTS
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(EmotionalResilienceTests, NPCMemoryTestFixture)

BOOST_AUTO_TEST_CASE(TestClassInfoHasResilience) {
    // Guards should have high resilience
    const ClassInfo* guardClass = edm->getClassInfo("Guard");
    BOOST_REQUIRE(guardClass != nullptr);
    BOOST_CHECK(guardClass->emotionalResilience > 0.6f);  // Guards are stoic

    // Merchants should have low resilience
    const ClassInfo* merchantClass = edm->getClassInfo("GeneralMerchant");
    BOOST_REQUIRE(merchantClass != nullptr);
    BOOST_CHECK(merchantClass->emotionalResilience < 0.4f);  // Merchants panic easily
}

BOOST_AUTO_TEST_CASE(TestCharacterDataInheritsResilience) {
    // Create a Guard NPC
    EntityHandle handle = edm->createNPCWithRaceClass(Vector2D(100, 100), "Human", "Guard");
    BOOST_REQUIRE(handle.isValid());

    size_t index = edm->getIndex(handle);
    uint32_t typeIndex = edm->getHotDataByIndex(index).typeLocalIndex;
    const auto& charData = edm->getCharacterDataByIndex(typeIndex);

    // Should have Guard's resilience
    const ClassInfo* guardClass = edm->getClassInfo("Guard");
    BOOST_REQUIRE(guardClass != nullptr);
    BOOST_CHECK(approxEqual(charData.emotionalResilience, guardClass->emotionalResilience));
}

BOOST_AUTO_TEST_CASE(TestResilienceDoesNotAffectRecordedCombatTotals) {
    // Recording combat in EDM is pure data and should not vary by class.
    EntityHandle guardHandle = edm->createNPCWithRaceClass(Vector2D(100, 100), "Human", "Guard");
    EntityHandle merchantHandle = edm->createNPCWithRaceClass(Vector2D(200, 200), "Human", "GeneralMerchant");

    BOOST_REQUIRE(guardHandle.isValid());
    BOOST_REQUIRE(merchantHandle.isValid());

    size_t guardIdx = edm->getIndex(guardHandle);
    size_t merchantIdx = edm->getIndex(merchantHandle);

    // Initialize memory data
    edm->initMemoryData(guardIdx);
    edm->initMemoryData(merchantIdx);

    // Record same combat event for both
    EntityHandle attacker{999, EntityKind::NPC, 1};
    float damage = 50.0f;

    edm->recordCombatEvent(guardIdx, attacker, EntityHandle{}, damage, true, 0.0f);
    edm->recordCombatEvent(merchantIdx, attacker, EntityHandle{}, damage, true, 0.0f);

    auto& guardMem = edm->getMemoryData(guardIdx);
    auto& merchantMem = edm->getMemoryData(merchantIdx);

    BOOST_CHECK(approxEqual(guardMem.totalDamageReceived, damage));
    BOOST_CHECK(approxEqual(merchantMem.totalDamageReceived, damage));
    BOOST_CHECK_EQUAL(guardMem.combatEncounters, 1);
    BOOST_CHECK_EQUAL(merchantMem.combatEncounters, 1);
}

BOOST_AUTO_TEST_CASE(TestBraveryDoesNotAffectRecordedCombatTotals) {
    auto [handle, index] = createTestNPC();

    edm->initMemoryData(index);
    auto& memData = edm->getMemoryData(index);

    memData.personality.bravery = 0.1f;

    EntityHandle attacker{999, EntityKind::NPC, 1};
    edm->recordCombatEvent(index, attacker, EntityHandle{}, 30.0f, true, 0.0f);

    const float cowardDamage = memData.totalDamageReceived;
    const uint32_t cowardEncounters = memData.combatEncounters;

    memData.personality.bravery = 0.9f;
    edm->recordCombatEvent(index, attacker, EntityHandle{}, 30.0f, true, 1.0f);

    BOOST_CHECK(approxEqual(cowardDamage, 30.0f));
    BOOST_CHECK_EQUAL(cowardEncounters, 1);
    BOOST_CHECK(approxEqual(memData.totalDamageReceived, 60.0f));
    BOOST_CHECK_EQUAL(memData.combatEncounters, 2);
}

BOOST_AUTO_TEST_SUITE_END()
