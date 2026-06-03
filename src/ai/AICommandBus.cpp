/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/AICommandBus.hpp"
#include <algorithm>

namespace VoidLight {

void AICommandBus::enqueueBehaviorMessage(EntityHandle targetHandle, size_t targetEdmIndex,
                                          uint8_t messageId, uint8_t param) {
    const uint64_t sequence = m_nextMessageSequence.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingMessages.push_back({targetHandle, targetEdmIndex, messageId, param, sequence});
}

void AICommandBus::enqueueBehaviorTransition(EntityHandle targetHandle, size_t targetEdmIndex,
                                             const BehaviorConfigData& config) {
    const uint64_t sequence = m_nextTransitionSequence.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingTransitions.push_back({targetHandle, targetEdmIndex, config, sequence});
}

void AICommandBus::enqueueFactionChange(EntityHandle targetHandle, size_t targetEdmIndex,
                                        uint8_t oldFaction, uint8_t newFaction) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingFactionChanges.push_back({targetHandle, targetEdmIndex, oldFaction, newFaction});
}

void AICommandBus::enqueueMeleeFallbackEquip(EntityHandle targetHandle,
                                             size_t targetEdmIndex) {
    const uint64_t sequence = m_nextEquipmentSequence.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingMeleeFallbackEquips.push_back({targetHandle, targetEdmIndex, sequence});
}

void AICommandBus::enqueueRangedAttack(EntityHandle attackerHandle,
                                       size_t attackerEdmIndex,
                                       const Vector2D& attackerPos,
                                       const Vector2D& targetPos,
                                       float damage,
                                       float attackRange,
                                       float projectileSpeed) {
    const uint64_t sequence =
        m_nextRangedAttackSequence.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingRangedAttacks.push_back({attackerHandle, attackerEdmIndex,
                                      attackerPos, targetPos, damage,
                                      attackRange, projectileSpeed, sequence});
}

void AICommandBus::clearBehaviorMessages(EntityHandle targetHandle, size_t targetEdmIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::erase_if(m_pendingMessages, [targetHandle, targetEdmIndex](const BehaviorMessageCommand& cmd) {
        return cmd.targetEdmIndex == targetEdmIndex && cmd.targetHandle == targetHandle;
    });
}

void AICommandBus::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingMessages.clear();
    m_pendingTransitions.clear();
    m_pendingFactionChanges.clear();
    m_pendingMeleeFallbackEquips.clear();
    m_pendingRangedAttacks.clear();
}

void AICommandBus::drainBehaviorMessages(std::vector<BehaviorMessageCommand>& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    out.clear();
    out.swap(m_pendingMessages);
}

void AICommandBus::drainBehaviorTransitions(std::vector<BehaviorTransitionCommand>& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    out.clear();
    out.swap(m_pendingTransitions);
}

void AICommandBus::drainFactionChanges(std::vector<FactionChangeCommand>& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    out.clear();
    out.swap(m_pendingFactionChanges);
}

void AICommandBus::drainMeleeFallbackEquips(std::vector<EquipmentSwapCommand>& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    out.clear();
    out.swap(m_pendingMeleeFallbackEquips);
}

void AICommandBus::drainRangedAttacks(std::vector<RangedAttackCommand>& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    out.clear();
    out.swap(m_pendingRangedAttacks);
}

} // namespace VoidLight
