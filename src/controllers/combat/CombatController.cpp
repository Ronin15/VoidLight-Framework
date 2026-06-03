/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "controllers/combat/CombatController.hpp"
#include "core/Logger.hpp"
#include "entities/Player.hpp"
#include "entities/resources/EquipmentResources.hpp"
#include "events/EntityEvents.hpp"
#include "managers/AIManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/UIManager.hpp"
#include <format>

namespace {
constexpr const char* EVENT_LOG = "event_log";
constexpr float PLAYER_PROJECTILE_SPAWN_OFFSET = 20.0f;

bool equipFirstAvailableMeleeWeapon(Player& player, EntityDataManager& edm) {
  const EntityHandle handle = player.getHandle();
  if (!handle.isValid() || !handle.hasHealth()) {
    return false;
  }

  const size_t idx = edm.getIndex(handle);
  if (idx == SIZE_MAX) {
    return false;
  }

  const auto& charData = edm.getCharacterDataByIndex(idx);
  if (!charData.hasInventory()) {
    return false;
  }

  const size_t maxSlots = edm.getInventoryData(charData.inventoryIndex).maxSlots;
  auto& resourceManager = ResourceTemplateManager::Instance();
  for (size_t slot = 0; slot < maxSlots; ++slot) {
    const InventorySlotData inventorySlot =
        edm.getInventorySlot(charData.inventoryIndex, slot);
    if (inventorySlot.isEmpty()) {
      continue;
    }

    auto resourceTemplate =
        resourceManager.getResourceTemplate(inventorySlot.resourceHandle);
    const auto equipment =
        std::dynamic_pointer_cast<Equipment>(resourceTemplate);
    if (!equipment ||
        equipment->getEquipmentSlot() != Equipment::EquipmentSlot::Weapon ||
        equipment->getWeaponMode() != Equipment::WeaponMode::Melee) {
      continue;
    }

    return player.equipItem(inventorySlot.resourceHandle);
  }

  return false;
}

void dispatchResourceChange(EntityHandle ownerHandle,
                            const InventoryResourceChange& change,
                            const std::string& reason) {
  if (!change.isValid()) {
    return;
  }

  EventManager::Instance().triggerResourceChange(
      ownerHandle, change.resourceHandle, change.oldQuantity,
      change.newQuantity, reason);
}
}

void CombatController::subscribe() {
  if (checkAlreadySubscribed()) {
    return;
  }

  // CombatController doesn't need to subscribe to any events currently
  // It drives combat, rather than reacting to events
  // Future: could subscribe to damage events from other sources

  setSubscribed(true);
  COMBAT_INFO("CombatController subscribed");
}

void CombatController::update(float deltaTime) {
  auto player = mp_player.lock();
  if (!player) {
    return;
  }

  // Update attack cooldown
  if (m_attackCooldown > 0.0f) {
    m_attackCooldown -= deltaTime;
    if (m_attackCooldown < 0.0f) {
      m_attackCooldown = 0.0f;
    }
  }

  // Regenerate stamina when not attacking
  if (m_attackCooldown <= 0.0f) {
    regenerateStamina(player.get(), deltaTime);
  }

}

bool CombatController::tryAttack() {
  auto player = mp_player.lock();
  if (!player) {
    return false;
  }

  // Check cooldown
  if (m_attackCooldown > 0.0f) {
    COMBAT_DEBUG(
        std::format("Attack on cooldown: {:.2f}s remaining", m_attackCooldown));
    return false;
  }

  // Check stamina
  if (!player->canAttack(ATTACK_STAMINA_COST)) {
    COMBAT_DEBUG(
        std::format("Not enough stamina to attack. Need {:.1f}, have {:.1f}",
                    ATTACK_STAMINA_COST, player->getStamina()));
    return false;
  }

  if (!performAttack(player.get())) {
    return false;
  }

  // Consume stamina and start cooldown
  float oldStamina = player->getStamina();
  player->consumeStamina(ATTACK_STAMINA_COST);
  m_attackCooldown = ATTACK_COOLDOWN;

  COMBAT_INFO(std::format("Player attacking! Stamina: {:.1f} -> {:.1f}",
                          oldStamina, player->getStamina()));

  // Transition player to attacking state
  player->changeState("attacking");

  return true;
}

bool CombatController::performAttack(Player *player) {
  if (!player) {
    return false;
  }

  // Cache manager references at function scope
  auto& edm = EntityDataManager::Instance();
  auto& aiMgr = AIManager::Instance();
  const Vector2D playerPos = player->getPosition();
  EntityHandle playerHandle = player->getHandle();
  CharacterData activeCharData = edm.getCharacterData(playerHandle);

  // Determine attack direction based on player facing
  float attackDirX = (player->getFlip() == SDL_FLIP_HORIZONTAL) ? -1.0f : 1.0f;

  if (activeCharData.combatStyle == CharacterData::CombatStyle::Ranged) {
    if (activeCharData.projectileSpeed <= 0.0f) {
      COMBAT_WARN("Player ranged weapon has no projectile speed configured");
      return false;
    }

    InventoryResourceChange ammoChange{};
    if (!edm.consumeRequiredAmmoForRangedAttack(playerHandle, &ammoChange)) {
      if (!equipFirstAvailableMeleeWeapon(*player, edm)) {
        COMBAT_INFO("Player has no compatible ammunition or melee fallback weapon");
        UIManager::Instance().addEventLogEntry(EVENT_LOG, "No ammunition!");
        return false;
      }
      activeCharData = edm.getCharacterData(playerHandle);
    } else {
      dispatchResourceChange(playerHandle, ammoChange, "ammo_consumed");
      const Vector2D direction(attackDirX, 0.0f);
      const Vector2D spawnPos =
          playerPos + direction * PLAYER_PROJECTILE_SPAWN_OFFSET;
      const Vector2D velocity = direction * activeCharData.projectileSpeed;
      const float lifetime =
          (activeCharData.attackRange / activeCharData.projectileSpeed) + 0.5f;
      edm.createProjectile(spawnPos, velocity, playerHandle,
                           activeCharData.attackDamage, lifetime);
      UIManager::Instance().addEventLogEntry(EVENT_LOG, "Fired ranged attack!");
      return true;
    }
  }

  // Query nearby entity handles from AIManager (EntityHandle-based API)
  // Reuse buffer to avoid per-frame allocation
  m_nearbyHandlesBuffer.clear();  // Keeps capacity
  aiMgr.scanActiveHandlesInRadius(playerPos, activeCharData.attackRange,
                             m_nearbyHandlesBuffer, true);

  for (const auto &handle : m_nearbyHandlesBuffer) {
    if (!handle.isValid())
      continue;

    // Phase 2 EDM Migration: Use handle.getKind() instead of EntityPtr
    if (handle.getKind() != EntityKind::NPC) {
      continue;
    }

    size_t idx = edm.getIndex(handle);
    if (idx == SIZE_MAX)
      continue;

    // Get entity data from EDM (single source of truth)
    auto &hotData = edm.getHotDataByIndex(idx);

    // Use EDM's isAlive() instead of Entity method
    if (!hotData.isAlive()) {
      continue;
    }

    const Vector2D npcPos = hotData.transform.position;
    const Vector2D diff = npcPos - playerPos;
    // Check if in attack direction (180 degree arc in front of player)
    float dotProduct = diff.getX() * attackDirX;
    if (dotProduct < 0.0f) {
      // NPC is behind the player
      continue;
    }

    // Hit detected - calculate knockback direction
    Vector2D knockback = diff.normalized() * 20.0f;

    // Record pre-damage health for UI logging
    float oldHealth = edm.getCharacterData(handle).health;

    // Dispatch the mutable damage payload. EventManager applies combat results
    // on the main thread immediately for player attacks.
    auto& eventMgr = EventManager::Instance();
    auto damageEvent = eventMgr.acquireDamageEvent();
    damageEvent->configure(playerHandle, handle, activeCharData.attackDamage,
                           knockback);
    eventMgr.dispatchEvent(
        damageEvent, EventManager::DispatchMode::Immediate);

    const float newHealth = edm.getCharacterData(handle).health;
    const bool wasLethal = newHealth <= 0.0f;

    COMBAT_INFO(
        std::format("Hit entity {} for {:.1f} damage! HP: {:.1f} -> {:.1f}",
                    handle.getId(), activeCharData.attackDamage, oldHealth,
                    newHealth));

    UIManager::Instance().addEventLogEntry(
        EVENT_LOG,
        std::format("Hit Enemy #{} for {:.0f} damage!", handle.getId(),
                    activeCharData.attackDamage));

    // Kill notification for UI
    if (wasLethal) {
      COMBAT_INFO(std::format("Entity {} killed!", handle.getId()));

      UIManager::Instance().addEventLogEntry(
          EVENT_LOG,
          std::format("Defeated Enemy #{}!", handle.getId()));
    }
  }

  return true;
}

void CombatController::regenerateStamina(Player *player, float deltaTime) {
  if (!player) {
    return;
  }

  float currentStamina = player->getStamina();
  float maxStamina = player->getMaxStamina();

  if (currentStamina < maxStamina) {
    float regenAmount = STAMINA_REGEN_RATE * deltaTime;
    player->restoreStamina(regenAmount);
  }
}
