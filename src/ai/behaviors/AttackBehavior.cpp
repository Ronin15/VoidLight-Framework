/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/BehaviorExecutors.hpp"
#include "ai/AICommandBus.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "events/EntityEvents.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace {

// ============================================================================
// THREAD-LOCAL STATE
// ============================================================================

thread_local std::mt19937 s_rng{std::random_device{}()};
thread_local std::uniform_real_distribution<float> s_damageRoll{0.0f, 1.0f};
thread_local std::uniform_real_distribution<float> s_criticalRoll{0.0f, 1.0f};
thread_local std::uniform_real_distribution<float> s_specialRoll{0.0f, 1.0f};
thread_local std::uniform_real_distribution<float> s_angleVariation{-0.5f, 0.5f};
thread_local std::vector<EventManager::DeferredEvent> t_deferredDamageEvents;
thread_local std::vector<size_t> s_scanBuffer;

// ============================================================================
// CONSTANTS
// ============================================================================

constexpr float COMBAT_ENTER_RANGE_MULT = 1.2f;
constexpr float COMBAT_EXIT_RANGE_MULT = 2.0f;
constexpr float COMBO_DAMAGE_PER_LEVEL = 0.15f;
constexpr float COMBO_TIMEOUT = 2.0f;
constexpr float SPECIAL_ATTACK_MULTIPLIER = 1.5f;
constexpr float RETREAT_SPEED_MULTIPLIER = 1.3f;
constexpr float STRAFE_INTERVAL = 3.0f;
constexpr float PRESSURE_STRAFE_MULTIPLIER = 0.75f;
constexpr float RESET_RECOVERY_DISTANCE_MULT = 1.65f;
constexpr float RESET_SAFE_DISTANCE_MULT = 2.2f;
constexpr float RESET_CONFIDENCE_TARGET = 0.55f;
constexpr float RECENT_ATTACK_WINDOW = 1.25f;

constexpr float CHARGE_SPEED_MULTIPLIER = 2.0f;
constexpr float CHARGE_DISTANCE_THRESHOLD_MULT = 1.5f;
constexpr float FEAR_FLEE_THRESHOLD = 0.7f;
constexpr float BRAVERY_FLEE_THRESHOLD = 0.3f;
constexpr float TARGET_SCAN_RANGE_MULTIPLIER = 6.0f;
constexpr float MIN_TARGET_SCAN_RANGE = 250.0f;

enum class AttackState : uint8_t {
    SEEKING = 0,
    ASSESSING = 1,
    APPROACHING = 2,
    ATTACKING = 3,
    RECOVERING = 4,
    TACTICAL_RESET = 5,
    PRESSURING = 6,
    DISENGAGING = 7
};

enum class AttackMode : uint8_t {
    MELEE = 0,
    RANGED = 1,
    CHARGE = 2,
    AMBUSH = 3,
    COORDINATED = 4,
    HIT_AND_RUN = 5,
    BERSERKER = 6
};

enum class CombatDecision : uint8_t {
    SeekTarget = 0,
    Approach = 1,
    Pressure = 2,
    Attack = 3,
    TacticalReset = 4,
    Disengage = 5,
    Recover = 6
};

struct CombatContext {
    AttackMode attackMode{AttackMode::MELEE};
    float attackRange{0.0f};
    float optimalRange{0.0f};
    float minimumRange{0.0f};
    float desiredRange{0.0f};
    float attackInterval{1.0f};
    float healthRatio{1.0f};
    float bravery{0.5f};
    float aggression{0.5f};
    float composure{0.5f};
    float loyalty{0.5f};
    float fear{0.0f};
    float pressureScore{0.0f};
    bool berserkerMode{false};
    bool recentlyAttacked{false};
    bool attackReady{false};
    bool canAttackFromRange{false};
    bool targetTooFar{false};
    bool targetTooClose{false};
    uint8_t combatEncounterCount{0};
};

float getEffectiveAttackRange(const CharacterData& charData, AttackMode attackMode,
                              const VoidLight::AttackBehaviorConfig& config) {
    const bool usesMeleeReach =
        attackMode == AttackMode::MELEE ||
        attackMode == AttackMode::AMBUSH ||
        attackMode == AttackMode::COORDINATED ||
        attackMode == AttackMode::BERSERKER ||
        attackMode == AttackMode::HIT_AND_RUN;

    if (usesMeleeReach && charData.attackRange > 0.0f) {
        return charData.attackRange;
    }

    return config.attackRange;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

Vector2D normalizeDir(const Vector2D& v) {
    float len = v.length();
    if (len < 0.0001f) return Vector2D(0, 0);
    return v * (1.0f / len);
}

void updateTimers(VoidLight::AttackStateData& attack, float deltaTime) {
    attack.attackTimer += deltaTime;
    attack.stateChangeTimer += deltaTime;
    attack.damageTimer += deltaTime;
    if (attack.comboTimer > 0.0f) {
        attack.comboTimer -= deltaTime;
    }
    attack.strafeTimer += deltaTime;
}

void resetFailedRangedAttack(VoidLight::AttackStateData& attack) {
    attack.currentState = static_cast<uint8_t>(AttackState::SEEKING);
    attack.stateChangeTimer = 0.0f;
    attack.recoveryTimer = 0.0f;
    attack.canAttack = true;
    attack.lastAttackHit = false;
}

void processAttackMessages(BehaviorData& shared, VoidLight::AttackStateData& attack,
                           const VoidLight::AttackBehaviorConfig&) {
    for (uint8_t i = 0; i < shared.pendingMessageCount; ++i) {
        uint8_t msgId = shared.pendingMessages[i].messageId;
        switch (msgId) {
            case BehaviorMessage::ATTACK_TARGET:
                break;

            case BehaviorMessage::RETREAT:
                attack.currentState = static_cast<uint8_t>(AttackState::TACTICAL_RESET);
                attack.isRetreating = true;
                attack.stateChangeTimer = 0.0f;
                break;

            case BehaviorMessage::PANIC:
                attack.currentState = static_cast<uint8_t>(AttackState::DISENGAGING);
                attack.isRetreating = true;
                attack.stateChangeTimer = 0.0f;
                break;

            case BehaviorMessage::RANGED_ATTACK_FAILED:
                resetFailedRangedAttack(attack);
                break;

            default:
                break;
        }
    }
    shared.pendingMessageCount = 0;
}

void changeState(VoidLight::AttackStateData& attack, AttackState newState) {
    uint8_t newStateVal = static_cast<uint8_t>(newState);
    if (attack.currentState != newStateVal) {
        attack.currentState = newStateVal;
        attack.stateChangeTimer = 0.0f;

        switch (newState) {
            case AttackState::ATTACKING:
                attack.recoveryTimer = 0.0f;
                attack.canAttack = false;
                attack.isRetreating = false;
                break;
            case AttackState::RECOVERING:
                attack.recoveryTimer = 0.0f;
                attack.isRetreating = false;
                break;
            case AttackState::TACTICAL_RESET:
            case AttackState::DISENGAGING:
                attack.isRetreating = true;
                break;
            default:
                attack.isRetreating = false;
                break;
        }
    }
}

float calculateDamage(const VoidLight::AttackStateData& attack,
                      const VoidLight::AttackBehaviorConfig& config) {
    float baseDamage = config.attackDamage;

    float variation = (s_damageRoll(s_rng) - 0.5f) * 2.0f * config.damageVariation;
    baseDamage *= (1.0f + variation);

    if (s_criticalRoll(s_rng) < config.criticalHitChance) {
        baseDamage *= config.criticalHitMultiplier;
    }

    if (config.comboAttacks && attack.currentCombo > 0) {
        float comboMultiplier = 1.0f + (attack.currentCombo * COMBO_DAMAGE_PER_LEVEL);
        baseDamage *= comboMultiplier;
    }

    if (attack.isCharging) {
        baseDamage *= config.chargeDamageMultiplier;
    }

    return baseDamage;
}

Vector2D calculateKnockbackVector(const Vector2D& attackerPos, const Vector2D& targetPos) {
    return normalizeDir(targetPos - attackerPos);
}

void applyDamageToTarget(EntityHandle targetHandle, float damage, const Vector2D& knockback,
                         EntityHandle attackerHandle) {
    if (!targetHandle.isValid()) return;

    Vector2D scaledKnockback = knockback * 0.1f;

    auto damageEvent = EventManager::Instance().acquireDamageEvent();
    damageEvent->configure(attackerHandle, targetHandle, damage, scaledKnockback);

    EventData eventData;
    eventData.typeId = EventTypeId::Combat;
    eventData.setActive(true);
    eventData.event = damageEvent;

    t_deferredDamageEvents.push_back({EventTypeId::Combat, std::move(eventData)});
}

bool hasRequiredAmmoForRangedAttack(const CharacterData& charData) {
    const auto weaponSlotIndex =
        Equipment::equipmentSlotIndex(Equipment::EquipmentSlot::Weapon);
    if (!weaponSlotIndex.has_value()) {
        return true;
    }

    const VoidLight::ResourceHandle weaponHandle =
        charData.equippedItems[*weaponSlotIndex];
    if (!weaponHandle.isValid()) {
        return true;
    }

    auto resourceTemplate =
        ResourceTemplateManager::Instance().getResourceTemplate(weaponHandle);
    const auto weapon = std::dynamic_pointer_cast<Equipment>(resourceTemplate);
    if (!weapon || weapon->getWeaponMode() != Equipment::WeaponMode::Ranged ||
        weapon->getAmmoTypeRequired().empty()) {
        return true;
    }

    if (!charData.hasInventory()) {
        return false;
    }

    auto& edm = EntityDataManager::Instance();
    const size_t maxSlots = edm.getInventoryData(charData.inventoryIndex).maxSlots;
    for (size_t slot = 0; slot < maxSlots; ++slot) {
        const InventorySlotData inventorySlot =
            edm.getInventorySlot(charData.inventoryIndex, slot);
        if (inventorySlot.isEmpty()) {
            continue;
        }

        auto ammoTemplate =
            ResourceTemplateManager::Instance().getResourceTemplate(
                inventorySlot.resourceHandle);
        const auto ammunition =
            std::dynamic_pointer_cast<Ammunition>(ammoTemplate);
        if (ammunition &&
            ammunition->getAmmoType() == weapon->getAmmoTypeRequired()) {
            return true;
        }
    }

    return false;
}

bool fireProjectile(const Vector2D& attackerPos, const Vector2D& targetPos,
                    EntityHandle attackerHandle, size_t attackerIndex,
                    const CharacterData& charData, float damage,
                    float attackRange, float projectileSpeed) {
    if (projectileSpeed <= 0.0f) {
        return false;
    }

    Vector2D direction = targetPos - attackerPos;
    float dist = direction.length();
    if (dist < 1.0f) return false;

    if (attackerIndex == SIZE_MAX) {
        return false;
    }

    if (!hasRequiredAmmoForRangedAttack(charData)) {
        VoidLight::AICommandBus::Instance().enqueueMeleeFallbackEquip(
            attackerHandle, attackerIndex);
        return false;
    }

    VoidLight::AICommandBus::Instance().enqueueRangedAttack(
        attackerHandle, attackerIndex, attackerPos, targetPos, damage,
        attackRange, projectileSpeed);
    return true;
}

void moveToPosition(BehaviorContext& ctx, const Vector2D& targetPos, float speed) {
    if (speed <= 0.0f) return;

    Vector2D entityPos = ctx.transform.position;
    Vector2D direction = targetPos - entityPos;
    float distance = direction.length();

    if (distance > 5.0f) {
        direction = direction * (1.0f / distance);
        ctx.transform.velocity = direction * speed;
    } else {
        ctx.transform.velocity = Vector2D(0, 0);
    }
}

bool isAttackTargetCandidate(size_t selfIdx, size_t candidateIdx, const CharacterData& selfCharData) {
    if (candidateIdx == SIZE_MAX || candidateIdx == selfIdx) return false;

    auto& edm = EntityDataManager::Instance();
    const auto& targetHot = edm.getHotDataByIndex(candidateIdx);
    if (!targetHot.isAlive()) return false;

    const uint8_t myFaction = selfCharData.faction;
    const uint8_t targetFaction = edm.getCharacterDataByIndex(candidateIdx).faction;
    if (targetFaction == myFaction) return false;

    return true;
}

bool tryAcquireTarget(BehaviorContext& ctx, VoidLight::AttackStateData& attack,
                      const VoidLight::AttackBehaviorConfig& config,
                      Vector2D& targetPos) {
    auto& edm = EntityDataManager::Instance();

    EntityHandle bestTarget{};
    float bestDistanceSq = std::numeric_limits<float>::max();

    if (ctx.playerValid && ctx.playerHandle.isValid()) {
        const size_t playerIdx = edm.getIndex(ctx.playerHandle);
        if (isAttackTargetCandidate(ctx.edmIndex, playerIdx, ctx.characterData)) {
            targetPos = edm.getHotDataByIndex(playerIdx).transform.position;
            bestTarget = ctx.playerHandle;
            bestDistanceSq = Vector2D::distanceSquared(ctx.transform.position, targetPos);
        }
    }

    const float scanRange =
        std::max(config.attackRange * TARGET_SCAN_RANGE_MULTIPLIER, MIN_TARGET_SCAN_RANGE);
    AIManager::Instance().scanActiveIndicesInRadius(
        ctx.transform.position, scanRange, s_scanBuffer, false);

    for (size_t candidateIdx : s_scanBuffer) {
        if (!isAttackTargetCandidate(ctx.edmIndex, candidateIdx, ctx.characterData)) {
            continue;
        }

        const Vector2D candidatePos = edm.getHotDataByIndex(candidateIdx).transform.position;
        const float distanceSq =
            Vector2D::distanceSquared(ctx.transform.position, candidatePos);
        if (distanceSq >= bestDistanceSq) {
            continue;
        }

        bestTarget = edm.getHandle(candidateIdx);
        bestDistanceSq = distanceSq;
        targetPos = candidatePos;
    }

    if (!bestTarget.isValid()) {
        return false;
    }

    attack.hasTarget = true;
    ctx.memoryData.lastTarget = bestTarget;
    return true;
}

bool canAttackFromCurrentRange(AttackMode attackMode, float targetDistance,
                               float attackRange, float minimumRange) {
    if (targetDistance > attackRange) {
        return false;
    }

    if (attackMode == AttackMode::RANGED && minimumRange > 0.0f) {
        return targetDistance >= minimumRange;
    }

    return true;
}

float getAttackInterval(const VoidLight::AttackBehaviorConfig& config,
                        float effectiveCooldown) {
    const float speedInterval = config.attackSpeed > 0.0f
        ? 1.0f / config.attackSpeed
        : effectiveCooldown;
    return std::max(effectiveCooldown, speedInterval);
}

CombatContext buildCombatContext(const BehaviorContext& ctx,
                                 const VoidLight::AttackBehaviorConfig& config,
                                 const VoidLight::AttackStateData& attack,
                                 AttackMode attackMode,
                                 float attackRange,
                                 float effectiveCooldown) {
    CombatContext combat;
    combat.attackMode = attackMode;
    combat.attackRange = attackRange;
    combat.optimalRange = attackRange * config.optimalRangeMultiplier;
    combat.minimumRange = attackRange * config.minimumRangeMultiplier;
    combat.desiredRange = std::max(combat.minimumRange + 8.0f, combat.optimalRange);
    combat.attackInterval = getAttackInterval(config, effectiveCooldown);
    combat.healthRatio = ctx.characterData.maxHealth > 0.0f
        ? ctx.characterData.health / ctx.characterData.maxHealth
        : 1.0f;

    if (ctx.memoryData.isValid()) {
        combat.bravery = ctx.memoryData.personality.bravery;
        combat.aggression = std::clamp(ctx.memoryData.personality.aggression +
                                           ctx.memoryData.emotions.aggression * 0.5f,
                                       0.0f, 1.0f);
        combat.composure = ctx.memoryData.personality.composure;
        combat.loyalty = ctx.memoryData.personality.loyalty;
        combat.fear = ctx.memoryData.emotions.fear;
        combat.combatEncounterCount = ctx.memoryData.combatEncounters;
    }

    const float crowdBoost =
        std::min(0.25f, static_cast<float>(ctx.sharedState.cachedNearbyCount) * 0.04f);
    combat.bravery = std::clamp(combat.bravery + crowdBoost, 0.0f, 1.0f);
    combat.recentlyAttacked = Behaviors::isUnderRecentAttack(ctx, RECENT_ATTACK_WINDOW);
    combat.berserkerMode = combat.aggression > 0.85f && combat.bravery > 0.65f;
    combat.attackReady =
        attack.currentState != static_cast<uint8_t>(AttackState::ATTACKING) &&
        (attack.currentState != static_cast<uint8_t>(AttackState::RECOVERING) ||
         attack.stateChangeTimer > config.recoveryTime) &&
        attack.attackTimer >= combat.attackInterval;
    combat.canAttackFromRange =
        combat.attackReady &&
        canAttackFromCurrentRange(attackMode, attack.targetDistance,
                                  attackRange, combat.minimumRange);
    combat.targetTooFar = attack.targetDistance > combat.attackRange;
    combat.targetTooClose = combat.minimumRange > 0.0f &&
                            attack.targetDistance < combat.minimumRange;

    const float healthPressure = std::clamp((0.65f - combat.healthRatio) / 0.65f, 0.0f, 1.0f);
    const float fearPressure = combat.fear * (1.15f - combat.bravery * 0.45f);
    const float recentPressure = combat.recentlyAttacked ? 0.25f : 0.0f;
    const float lowComposurePressure = (1.0f - combat.composure) * 0.25f;
    const float aggressionRelief = combat.aggression * 0.28f;
    combat.pressureScore = std::clamp(healthPressure + fearPressure + recentPressure +
                                          lowComposurePressure - aggressionRelief,
                                      0.0f, 1.0f);
    return combat;
}

bool shouldDisengage(const CombatContext& combat) {
    if (combat.berserkerMode) {
        return false;
    }

    const bool criticalHealth = combat.healthRatio < 0.18f;
    const bool overwhelmed = combat.pressureScore > 0.92f;
    const bool fearful = combat.fear > FEAR_FLEE_THRESHOLD &&
                         combat.bravery < BRAVERY_FLEE_THRESHOLD;
    return (criticalHealth && combat.bravery < 0.45f) || overwhelmed || fearful;
}

bool shouldTacticalReset(const CombatContext& combat,
                         const VoidLight::AttackStateData& attack) {
    if (combat.berserkerMode || combat.attackMode == AttackMode::BERSERKER) {
        return false;
    }

    const bool newPressure =
        !attack.hasHandledTacticalRetreat ||
        attack.lastTacticalRetreatEncounter != combat.combatEncounterCount ||
        combat.recentlyAttacked ||
        combat.pressureScore > attack.pressureScore + 0.15f;
    return newPressure && combat.pressureScore >= 0.48f;
}

bool tacticalResetRecovered(const CombatContext& combat,
                            const VoidLight::AttackStateData& attack) {
    const bool hasSafeSpace = attack.targetDistance >= combat.attackRange * RESET_SAFE_DISTANCE_MULT;
    const bool hasRecoveredSpace =
        attack.targetDistance >= combat.attackRange * RESET_RECOVERY_DISTANCE_MULT &&
        combat.pressureScore < 0.55f;
    const bool confidenceRecovered =
        attack.resetConfidence >= RESET_CONFIDENCE_TARGET &&
        combat.pressureScore < 0.45f;
    return hasSafeSpace || hasRecoveredSpace || confidenceRecovered;
}

CombatDecision chooseCombatDecision(const CombatContext& combat,
                                    const VoidLight::AttackStateData& attack) {
    const auto currentState = static_cast<AttackState>(attack.currentState);
    if (currentState == AttackState::ATTACKING) {
        return CombatDecision::Attack;
    }

    if (currentState == AttackState::DISENGAGING) {
        return CombatDecision::Disengage;
    }

    if (currentState == AttackState::TACTICAL_RESET) {
        if (combat.canAttackFromRange && combat.pressureScore < 0.9f) {
            return CombatDecision::Attack;
        }

        if (!shouldDisengage(combat)) {
            return tacticalResetRecovered(combat, attack)
                ? CombatDecision::Pressure
                : CombatDecision::TacticalReset;
        }
    }

    if (shouldDisengage(combat)) {
        return CombatDecision::Disengage;
    }

    if (currentState == AttackState::RECOVERING) {
        return shouldTacticalReset(combat, attack)
            ? CombatDecision::TacticalReset
            : CombatDecision::Recover;
    }

    if (combat.canAttackFromRange && combat.pressureScore < 0.72f) {
        return CombatDecision::Attack;
    }

    if (shouldTacticalReset(combat, attack)) {
        return CombatDecision::TacticalReset;
    }

    if (combat.targetTooFar) {
        return CombatDecision::Approach;
    }

    return CombatDecision::Pressure;
}

void markTacticalRetreatEncounter(VoidLight::AttackStateData& attack,
                                  const NPCMemoryData& memoryData) {
    attack.lastTacticalRetreatEncounter = memoryData.combatEncounters;
    attack.hasHandledTacticalRetreat = true;
}

void recordResolvedAttack(BehaviorContext& ctx, VoidLight::AttackStateData& attack,
                          EntityHandle targetHandle,
                          const VoidLight::AttackBehaviorConfig& config) {
    auto& edm = EntityDataManager::Instance();

    if (ctx.characterData.faction > 1) {
        size_t targetIdx = edm.getIndex(targetHandle);
        if (targetIdx != SIZE_MAX && edm.getCharacterDataByIndex(targetIdx).faction == 0) {
            edm.setFaction(edm.getHandle(ctx.edmIndex), 1);
        }
    }

    ctx.memoryData.lastTarget = targetHandle;

    attack.attackTimer = 0.0f;
    attack.lastAttackHit = true;

    if (config.comboAttacks) {
        if (attack.comboTimer > 0.0f) {
            attack.currentCombo = std::min(attack.currentCombo + 1, config.maxCombo);
        } else {
            attack.currentCombo = 1;
            attack.comboTimer = COMBO_TIMEOUT;
        }
    }
}

void broadcastRetreatToAllies(const BehaviorContext& ctx) {
    uint8_t myFaction = ctx.characterData.faction;
    AIManager::Instance().scanFactionInRadius(
        myFaction, ctx.transform.position, 200.0f, s_scanBuffer, true);
    for (size_t idx : s_scanBuffer) {
        if (idx == ctx.edmIndex) continue;
        Behaviors::deferBehaviorMessage(idx, BehaviorMessage::RETREAT);
    }
}

void applyTacticalResetMovement(BehaviorContext& ctx, VoidLight::AttackStateData& attack,
                                const CombatContext& combat,
                                const Vector2D& entityPos,
                                const Vector2D& targetPos,
                                float moveSpeed) {
    Vector2D away = normalizeDir(entityPos - targetPos);
    if (away.lengthSquared() < 0.0001f) {
        away = Vector2D(1.0f, 0.0f);
    }

    Vector2D perpendicular(-away.getY(), away.getX());
    const float strafeSign = attack.strafeDirectionInt >= 0 ? 1.0f : -1.0f;
    const float lateralWeight = std::clamp(combat.composure, 0.2f, 0.8f);
    const Vector2D resetDir = normalizeDir(away + perpendicular * strafeSign * lateralWeight);

    const float urgency = 0.85f + combat.pressureScore * 0.65f;
    ctx.transform.velocity = resetDir * (moveSpeed * urgency);

    if (attack.targetDistance >= combat.attackRange * RESET_RECOVERY_DISTANCE_MULT &&
        combat.pressureScore < 0.55f) {
        attack.resetConfidence =
            std::clamp(attack.resetConfidence + ctx.deltaTime * (0.6f + combat.composure),
                       0.0f, 1.0f);
    } else {
        attack.resetConfidence =
            std::clamp(attack.resetConfidence - ctx.deltaTime * 0.35f, 0.0f, 1.0f);
    }
}

void applyPressureMovement(BehaviorContext& ctx, VoidLight::AttackStateData& attack,
                           const CombatContext& combat,
                           const Vector2D& entityPos,
                           const Vector2D& targetPos,
                           float moveSpeed) {
    Vector2D away = normalizeDir(entityPos - targetPos);
    if (away.lengthSquared() < 0.0001f) {
        ctx.transform.velocity = Vector2D(0, 0);
        return;
    }

    if (combat.targetTooClose) {
        const float backoffSpeed = moveSpeed * (0.65f + combat.pressureScore * 0.35f);
        ctx.transform.velocity = away * backoffSpeed;
        return;
    }

    if (attack.targetDistance > combat.desiredRange + 12.0f) {
        moveToPosition(ctx, targetPos, moveSpeed * 0.65f);
        return;
    }

    if (attack.strafeTimer > STRAFE_INTERVAL * (1.25f - combat.composure * 0.5f)) {
        attack.strafeTimer = 0.0f;
        attack.strafeDirectionInt *= -1;
    }

    Vector2D perpendicular(-away.getY(), away.getX());
    const float strafeSign = attack.strafeDirectionInt >= 0 ? 1.0f : -1.0f;
    const float forwardBias = combat.aggression > 0.65f ? -0.18f : 0.08f;
    const Vector2D pressureDir =
        normalizeDir(perpendicular * strafeSign + away * forwardBias);
    ctx.transform.velocity = pressureDir * (moveSpeed * PRESSURE_STRAFE_MULTIPLIER);
}

bool executeAttackAction(BehaviorContext& ctx, VoidLight::AttackStateData& attack,
                         const Vector2D& targetPos,
                         const VoidLight::AttackBehaviorConfig& config) {
    auto& edm = EntityDataManager::Instance();
    Vector2D entityPos = ctx.transform.position;

    float damage = calculateDamage(attack, config);

    if (ctx.memoryData.isValid()) {
        float personalityBonus = 1.0f + (ctx.memoryData.personality.aggression * 0.2f);
        float emotionalBonus = 1.0f + (ctx.memoryData.emotions.aggression * 0.2f);
        damage *= personalityBonus * emotionalBonus;
    }

    Vector2D knockback = calculateKnockbackVector(entityPos, targetPos) * config.knockbackForce;
    EntityHandle attackerHandle = edm.getHandle(ctx.edmIndex);

    EntityHandle targetHandle{};
    if (attack.hasExplicitTarget && attack.explicitTarget.isValid()) {
        targetHandle = attack.explicitTarget;
    } else if (ctx.memoryData.lastTarget.isValid()) {
        targetHandle = ctx.memoryData.lastTarget;
    } else if (ctx.memoryData.lastAttacker.isValid()) {
        targetHandle = ctx.memoryData.lastAttacker;
    }
    if (!targetHandle.isValid()) return false;

    AttackMode mode = static_cast<AttackMode>(attack.attackMode);
    bool attackResolved = false;
    if (mode == AttackMode::RANGED) {
        attackResolved = fireProjectile(entityPos, targetPos, attackerHandle,
                                        ctx.edmIndex, ctx.characterData, damage,
                                        config.attackRange, config.projectileSpeed);
    } else {
        applyDamageToTarget(targetHandle, damage, knockback, attackerHandle);
        attackResolved = true;
    }
    if (!attackResolved) return false;

    recordResolvedAttack(ctx, attack, targetHandle, config);

    return true;
}

// ============================================================================
// MODE-SPECIFIC POSITIONING FUNCTIONS
// ============================================================================

bool applyRangedPositioning(BehaviorContext& ctx, const Vector2D& entityPos, const Vector2D& targetPos,
                            VoidLight::AttackStateData& attack, const VoidLight::AttackBehaviorConfig& config,
                            float moveSpeed) {
    float optimalRange = config.attackRange * config.optimalRangeMultiplier;
    float minimumRange = config.attackRange * config.minimumRangeMultiplier;

    if (attack.targetDistance < minimumRange && minimumRange > 0.0f) {
        Vector2D direction = normalizeDir(entityPos - targetPos);
        Vector2D backoffPos = targetPos + direction * (optimalRange * 0.8f);
        moveToPosition(ctx, backoffPos, moveSpeed);
        return true;
    }

    if (attack.targetDistance > config.attackRange) {
        moveToPosition(ctx, targetPos, moveSpeed);
        return true;
    }

    return false;
}

bool applyChargePositioning(BehaviorContext& ctx, const Vector2D& entityPos, const Vector2D& targetPos,
                            VoidLight::AttackStateData& attack, const VoidLight::AttackBehaviorConfig& config,
                            float moveSpeed) {
    float chargeDistance = config.attackRange * CHARGE_DISTANCE_THRESHOLD_MULT;

    if (attack.targetDistance > chargeDistance && !attack.isCharging) {
        attack.isCharging = true;
    }

    if (attack.isCharging) {
        Vector2D chargeDir = normalizeDir(targetPos - entityPos);
        ctx.transform.velocity = chargeDir * (moveSpeed * CHARGE_SPEED_MULTIPLIER);
        return true;
    }

    return false;
}

bool applyHitAndRunPositioning(BehaviorContext& ctx, const Vector2D& entityPos, const Vector2D& targetPos,
                               VoidLight::AttackStateData& attack, const VoidLight::AttackBehaviorConfig& config,
                               float moveSpeed) {
    if (attack.currentState == static_cast<uint8_t>(AttackState::RECOVERING) ||
        attack.currentState == static_cast<uint8_t>(AttackState::TACTICAL_RESET)) {

        Vector2D retreatDir = normalizeDir(entityPos - targetPos);
        float rangeScaling = 1.0f + (config.optimalRangeMultiplier - 0.5f) * 0.4f;
        Vector2D retreatVelocity = retreatDir * (moveSpeed * RETREAT_SPEED_MULTIPLIER * rangeScaling);
        ctx.transform.velocity = retreatVelocity;
        return true;
    }

    return false;
}

void applyBerserkerModifiers(float& effectiveCooldown) {
    effectiveCooldown *= 0.5f;
}

} // anonymous namespace

namespace Behaviors {

void initAttack(size_t edmIndex, const VoidLight::AttackBehaviorConfig& config,
                VoidLight::AttackStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Attack);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    state.lastTargetPosition = Vector2D(0, 0);
    state.attackPosition = Vector2D(0, 0);
    state.retreatPosition = Vector2D(0, 0);
    state.strafeVector = Vector2D(0, 0);
    state.tacticalResetAnchor = Vector2D(0, 0);
    state.currentState = 0;
    state.attackMode = config.attackMode;
    state.attackTimer = 0.0f;
    state.stateChangeTimer = 0.0f;
    state.damageTimer = 0.0f;
    state.comboTimer = 0.0f;
    state.strafeTimer = 0.0f;
    state.currentHealth = 100.0f;
    state.maxHealth = 100.0f;
    state.currentStamina = 100.0f;
    state.targetDistance = 0.0f;
    state.attackChargeTime = 0.0f;
    state.recoveryTimer = 0.0f;
    state.scanCooldown = 0.0f;
    state.pressureScore = 0.0f;
    state.desiredCombatRange = 0.0f;
    state.resetConfidence = 0.0f;
    state.currentCombo = 0;
    state.attacksInCombo = 0;
    state.strafeDirectionInt = 1;
    state.lastTacticalRetreatEncounter = 0;
    state.lastCombatDecision = 0;
    state.preferredAttackAngle = 0.0f;
    state.inCombat = false;
    state.hasTarget = false;
    state.isCharging = false;
    state.isRetreating = false;
    state.canAttack = true;
    state.lastAttackHit = false;
    state.specialAttackReady = false;
    state.circleStrafing = false;
    state.flanking = false;
    state.hasExplicitTarget = false;
    state.comboEnabled = false;
    state.hasHandledTacticalRetreat = false;
    state.explicitTarget = EntityHandle{};

    shared.setInitialized(true);
}

void executeAttack(BehaviorContext& ctx, const VoidLight::AttackBehaviorConfig& config,
                   VoidLight::AttackStateData& attack) {
    if (!ctx.sharedState.isValid()) return;

    auto& shared = ctx.sharedState;

    processAttackMessages(shared, attack, config);
    attack.attackMode = config.attackMode;

    Vector2D targetPos;
    bool hasTarget = false;
    auto& edm = EntityDataManager::Instance();

    if (attack.hasExplicitTarget && attack.explicitTarget.isValid()) {
        size_t targetIdx = edm.getIndex(attack.explicitTarget);
        if (targetIdx != SIZE_MAX) {
            const auto& targetHot = edm.getHotDataByIndex(targetIdx);
            if (targetHot.isAlive()) {
                targetPos = targetHot.transform.position;
                hasTarget = true;
            }
        }
        if (!hasTarget) {
            attack.hasExplicitTarget = false;
            attack.explicitTarget = EntityHandle{};
        }
    }

    if (!hasTarget && ctx.memoryData.lastTarget.isValid()) {
        size_t targetIdx = edm.getIndex(ctx.memoryData.lastTarget);
        if (targetIdx != SIZE_MAX && edm.getHotDataByIndex(targetIdx).isAlive()) {
            targetPos = edm.getHotDataByIndex(targetIdx).transform.position;
            hasTarget = true;
        }
    }

    if (!hasTarget && ctx.memoryData.lastAttacker.isValid()) {
        size_t attackerIdx = edm.getIndex(ctx.memoryData.lastAttacker);
        if (attackerIdx != SIZE_MAX && edm.getHotDataByIndex(attackerIdx).isAlive()) {
            targetPos = edm.getHotDataByIndex(attackerIdx).transform.position;
            hasTarget = true;
        }
    }

    if (!hasTarget && ctx.playerValid && ctx.playerHandle.isValid()) {
        size_t playerIdx = edm.getIndex(ctx.playerHandle);
        if (isAttackTargetCandidate(ctx.edmIndex, playerIdx, ctx.characterData)) {
            targetPos = edm.getHotDataByIndex(playerIdx).transform.position;
            hasTarget = true;
            ctx.memoryData.lastTarget = ctx.playerHandle;
        }
    }

    if (!hasTarget) {
        hasTarget = tryAcquireTarget(ctx, attack, config, targetPos);
    }

    if (!hasTarget) {
        attack.hasTarget = false;
        ctx.transform.velocity = Vector2D(0, 0);
        if (attack.inCombat || attack.stateChangeTimer > 1.5f) {
            switchBehavior(ctx.edmIndex, BehaviorType::Idle);
        }
        return;
    }

    Vector2D entityPos = ctx.transform.position;

    updateTimers(attack, ctx.deltaTime);

    attack.hasTarget = true;
    attack.lastTargetPosition = targetPos;
    float distSquared = (entityPos - targetPos).lengthSquared();
    attack.targetDistance = std::sqrt(distSquared);

    AttackMode attackMode = static_cast<AttackMode>(attack.attackMode);
    const float attackRange = getEffectiveAttackRange(ctx.characterData, attackMode, config);

    if (!attack.inCombat && attack.targetDistance <= attackRange * COMBAT_ENTER_RANGE_MULT) {
        attack.inCombat = true;
    } else if (attack.inCombat && attack.targetDistance > attackRange * COMBAT_EXIT_RANGE_MULT) {
        attack.inCombat = false;
        changeState(attack, AttackState::SEEKING);
    }

    bool berserkerMode = false;
    if (ctx.memoryData.isValid()) {
        float aggression = ctx.memoryData.emotions.aggression;
        float personalAggression = ctx.memoryData.personality.aggression;
        berserkerMode = (aggression > 0.8f && personalAggression > 0.7f);
    }

    float effectiveCooldown = config.attackCooldown;

    if (berserkerMode || attackMode == AttackMode::BERSERKER) {
        applyBerserkerModifiers(effectiveCooldown);
    }

    AttackState currentState = static_cast<AttackState>(attack.currentState);
    CombatContext combat = buildCombatContext(ctx, config, attack, attackMode,
                                              attackRange, effectiveCooldown);
    combat.berserkerMode = combat.berserkerMode || berserkerMode;
    attack.canAttack = combat.attackReady;
    attack.desiredCombatRange = combat.desiredRange;
    if (combat.attackReady) {
        attack.specialAttackReady = true;
    }

    if (currentState == AttackState::ATTACKING) {
        if (s_specialRoll(s_rng) < config.specialAttackChance && attack.specialAttackReady) {
            float specialDamage = calculateDamage(attack, config) * SPECIAL_ATTACK_MULTIPLIER;
            Vector2D knockback = calculateKnockbackVector(entityPos, targetPos) * config.knockbackForce * SPECIAL_ATTACK_MULTIPLIER;
            EntityHandle targetHandle{};
            if (attack.hasExplicitTarget && attack.explicitTarget.isValid()) {
                targetHandle = attack.explicitTarget;
            } else if (ctx.memoryData.lastTarget.isValid()) {
                targetHandle = ctx.memoryData.lastTarget;
            } else if (ctx.memoryData.lastAttacker.isValid()) {
                targetHandle = ctx.memoryData.lastAttacker;
            }
            if (targetHandle.isValid()) {
                EntityHandle attackerHandle = edm.getHandle(ctx.edmIndex);
                AttackMode specialMode = static_cast<AttackMode>(attack.attackMode);
                bool specialResolved = true;
                if (specialMode == AttackMode::RANGED) {
                    specialResolved = fireProjectile(entityPos, targetPos, attackerHandle,
                                                     ctx.edmIndex, ctx.characterData,
                                                     specialDamage, config.attackRange,
                                                     config.projectileSpeed);
                } else {
                    applyDamageToTarget(targetHandle, specialDamage, knockback, attackerHandle);

                    if (config.aoeRadius > 0.0f)
                    {
                        s_scanBuffer.clear();
                        AIManager::Instance().scanActiveIndicesInRadius(
                            targetPos, config.aoeRadius, s_scanBuffer, false);
                        uint8_t myFaction = ctx.characterData.faction;

                        for (size_t aoeIdx : s_scanBuffer)
                        {
                            if (aoeIdx == ctx.edmIndex) continue;
                            const auto& aoeHot = edm.getHotDataByIndex(aoeIdx);
                            if (!aoeHot.isAlive()) continue;
                            EntityHandle aoeTarget = edm.getHandle(aoeIdx);
                            if (aoeTarget == targetHandle) continue;
                            if (config.avoidFriendlyFire &&
                                edm.getCharacterDataByIndex(aoeIdx).faction == myFaction) continue;

                            float distSq = Vector2D::distanceSquared(targetPos, aoeHot.transform.position);
                            float dist = std::sqrt(distSq);
                            float falloff = 1.0f - (dist / config.aoeRadius) * 0.5f;
                            float aoeDamage = specialDamage * falloff;
                            Vector2D aoeKnockback = calculateKnockbackVector(targetPos, aoeHot.transform.position)
                                                    * config.knockbackForce * 0.5f;
                            applyDamageToTarget(aoeTarget, aoeDamage, aoeKnockback, attackerHandle);
                        }
                    }
                }
                if (!specialResolved) {
                    if (specialMode == AttackMode::RANGED) {
                        resetFailedRangedAttack(attack);
                    }
                    return;
                }
                recordResolvedAttack(ctx, attack, targetHandle, config);
            }
            attack.specialAttackReady = false;
        } else {
            if (!executeAttackAction(ctx, attack, targetPos, config)) {
                if (static_cast<AttackMode>(attack.attackMode) == AttackMode::RANGED) {
                    resetFailedRangedAttack(attack);
                }
                return;
            }
        }
        changeState(attack, AttackState::RECOVERING);
        return;
    }

    const CombatDecision decision = chooseCombatDecision(combat, attack);
    attack.lastCombatDecision = static_cast<uint8_t>(decision);
    attack.pressureScore = combat.pressureScore;

    if (decision == CombatDecision::Disengage) {
        broadcastRetreatToAllies(ctx);
        changeState(attack, AttackState::DISENGAGING);
        switchBehavior(ctx.edmIndex, BehaviorType::Flee);
        return;
    }

    if (decision == CombatDecision::Attack) {
        changeState(attack, AttackState::ATTACKING);
        return;
    }

    if (decision == CombatDecision::TacticalReset) {
        if (currentState != AttackState::TACTICAL_RESET) {
            attack.tacticalResetAnchor = entityPos;
            attack.resetConfidence = 0.0f;
            broadcastRetreatToAllies(ctx);
            changeState(attack, AttackState::TACTICAL_RESET);
        }
        markTacticalRetreatEncounter(attack, ctx.memoryData);
        applyTacticalResetMovement(ctx, attack, combat, entityPos, targetPos,
                                   shared.moveSpeed);
        return;
    }

    if (decision == CombatDecision::Recover) {
        if (attack.stateChangeTimer <= config.recoveryTime) {
            if (attackMode == AttackMode::HIT_AND_RUN) {
                applyHitAndRunPositioning(ctx, entityPos, targetPos, attack,
                                          config, shared.moveSpeed);
            } else {
                applyPressureMovement(ctx, attack, combat, entityPos, targetPos,
                                      shared.moveSpeed * 0.65f);
            }
            return;
        }
        changeState(attack, AttackState::ASSESSING);
        return;
    }

    if (decision == CombatDecision::Approach) {
        changeState(attack, AttackState::APPROACHING);
        Vector2D toTarget = normalizeDir(targetPos - entityPos);
        const float desiredStopDistance = std::max(combat.desiredRange, combat.minimumRange + 8.0f);
        Vector2D approachPos = targetPos - toTarget * desiredStopDistance;
        moveToPosition(ctx, approachPos, shared.moveSpeed);
        return;
    }

    changeState(attack, AttackState::PRESSURING);

    bool modeHandled = false;
    switch (attackMode) {
        case AttackMode::RANGED:
            modeHandled = applyRangedPositioning(ctx, entityPos, targetPos, attack, config, shared.moveSpeed);
            break;
        case AttackMode::CHARGE:
            modeHandled = applyChargePositioning(ctx, entityPos, targetPos, attack, config, shared.moveSpeed);
            break;
        case AttackMode::HIT_AND_RUN:
            modeHandled = applyHitAndRunPositioning(ctx, entityPos, targetPos, attack, config, shared.moveSpeed);
            break;
        default:
            break;
    }

    if (modeHandled) {
        return;
    }

    applyPressureMovement(ctx, attack, combat, entityPos, targetPos, shared.moveSpeed);
}

void collectDeferredDamageEvents(std::vector<EventManager::DeferredEvent>& out) {
    out.insert(out.end(), std::make_move_iterator(t_deferredDamageEvents.begin()),
                          std::make_move_iterator(t_deferredDamageEvents.end()));
    t_deferredDamageEvents.clear();
}

} // namespace Behaviors
