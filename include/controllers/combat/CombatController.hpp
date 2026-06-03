/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef COMBAT_CONTROLLER_HPP
#define COMBAT_CONTROLLER_HPP

/**
 * @file CombatController.hpp
 * @brief Frame-updatable controller for player combat mechanics
 *
 * CombatController handles:
 * - Attack execution and cooldowns
 * - Stamina consumption and regeneration
 * - Hit detection against NPCs (via AIManager)
 *
 * This is a frame-updatable controller (implements IUpdatable) because it
 * manages per-frame state: attack cooldowns, stamina regen, target timers.
 *
 * Ownership: ControllerRegistry owns the controller instance.
 */

#include "controllers/ControllerBase.hpp"
#include "controllers/IUpdatable.hpp"
#include "entities/EntityHandle.hpp"
#include <memory>

// Forward declarations
class Player;
class Entity;

class CombatController : public ControllerBase, public IUpdatable
{
public:
    /**
     * @brief Construct CombatController with required player reference
     * @param player Shared pointer to the player (required)
     * @note Enforces dependency at construction - cannot forget to set player
     */
    explicit CombatController(std::shared_ptr<Player> player)
        : mp_player(std::move(player)) {}

    ~CombatController() override = default;

    // Movable (inherited from base)
    CombatController(CombatController&&) noexcept = default;
    CombatController& operator=(CombatController&&) noexcept = default;

    // --- ControllerBase interface ---

    /**
     * @brief Subscribe to combat-related events
     * @note Called by ControllerRegistry::subscribeAll()
     */
    void subscribe() override;

    /**
     * @brief Get controller name for debugging
     * @return "CombatController"
     */
    [[nodiscard]] std::string_view getName() const override { return "CombatController"; }

    // --- IUpdatable interface ---

    /**
     * @brief Update combat state (cooldowns, stamina regen)
     * @param deltaTime Frame delta time in seconds
     * @note Called by ControllerRegistry::updateAll()
     */
    void update(float deltaTime) override;

    // --- Combat operations ---

    /**
     * @brief Attempt to perform an attack
     * @return true if attack was performed, false if blocked (cooldown, no stamina)
     * @note Uses AIManager::queryEntitiesInRadius() for hit detection
     */
    bool tryAttack();

    // Configuration constants
    static constexpr float ATTACK_STAMINA_COST{10.0f};
    static constexpr float STAMINA_REGEN_RATE{15.0f};     // per second
    static constexpr float ATTACK_COOLDOWN{0.5f};         // seconds between attacks

private:
    /**
     * @brief Execute the attack and detect hits
     * @param player Raw pointer to player (from locked weak_ptr)
     * @return true if an attack was actually performed
     */
    [[nodiscard]] bool performAttack(Player* player);

    /**
     * @brief Regenerate player stamina over time
     * @param player Raw pointer to player (from locked weak_ptr)
     * @param deltaTime Frame delta time in seconds
     */
    void regenerateStamina(Player* player, float deltaTime);

    // Player reference (set via setPlayer())
    std::weak_ptr<Player> mp_player;

    // Attack timing
    float m_attackCooldown{0.0f};

    // Reusable buffers to avoid per-frame allocations
    std::vector<EntityHandle> m_nearbyHandlesBuffer;  // Reused for queries
};

#endif // COMBAT_CONTROLLER_HPP
