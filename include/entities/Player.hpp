/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "entities/Entity.hpp"
#include "entities/EntityStateManager.hpp"
#include "utils/ResourceHandle.hpp"
#include <cstdint>

namespace VoidLight {
class Camera;
class GPURenderer;
}  // Forward declarations

class Player : public Entity {
public:
  Player();
  ~Player() override;

  void update(float deltaTime) override;
  [[nodiscard]] EntityKind getKind() const override { return EntityKind::Player; }
  /**
   * @brief Record player vertices to GPU vertex pool
   * @param gpuRenderer GPU renderer instance
   * @param cameraX Camera X offset
   * @param cameraY Camera Y offset
   * @param interpolationAlpha Interpolation factor for smooth rendering
   */
  void recordGPUVertices(VoidLight::GPURenderer& gpuRenderer, float cameraX,
                         float cameraY, float interpolationAlpha);

  /**
   * @brief Render player using GPU pipeline
   * @param gpuRenderer GPU renderer instance
   * @param scenePass Active scene render pass
   */
  void renderGPU(VoidLight::GPURenderer& gpuRenderer, SDL_GPURenderPass* scenePass);

  void clean() override;

  // Sync movement with CollisionManager (player moves itself)
  void setVelocity(const Vector2D& velocity) override;
  void setPosition(const Vector2D& position) override;

  // State management
  void changeState(const std::string &stateName);
  std::string getCurrentStateName() const;
  // void setVelocity(const Vector2D& m_velocity); for later in save manager
  // void setFlip(SDL_FlipMode m_flip);

  // Player-specific accessor methods
  SDL_FlipMode getFlip() const override { return m_flip; }

  // Player-specific setter methods
  void setFlip(SDL_FlipMode flip) override { m_flip = flip; }

  // Animation abstraction - allows states to use named animations (override for custom logic)
  void playAnimation(const std::string& animName) override;

  // Movement speed accessor
  float getMovementSpeed() const;

  // Inventory management - uses EDM inventory directly
  [[nodiscard]] uint32_t getInventoryIndex() const { return m_inventoryIndex; }

  // Resource operations (delegate to EntityDataManager)
  bool addToInventory(VoidLight::ResourceHandle handle, int quantity);
  bool removeFromInventory(VoidLight::ResourceHandle handle, int quantity);
  [[nodiscard]] int getInventoryQuantity(VoidLight::ResourceHandle handle) const;
  [[nodiscard]] bool hasInInventory(VoidLight::ResourceHandle handle, int quantity = 1) const;

  // Equipment management
  bool equipItem(VoidLight::ResourceHandle itemHandle);
  bool unequipItem(const std::string &slotName);
  VoidLight::ResourceHandle
  getEquippedItem(const std::string &slotName) const;

  // Crafting and consumption
  bool canCraft(const std::string &recipeId) const;
  bool craftItem(const std::string &recipeId);
  bool consumeItem(VoidLight::ResourceHandle itemHandle);

  // Initialization - call after construction to setup inventory
  void initializeInventory();

  // Physics body registration - call after construction
  void ensurePhysicsBodyRegistered();

  // Cache invalidation - call when world changes (e.g., on WorldGeneratedEvent)
  void invalidateWorldBoundsCache() { m_worldBoundsCached = false; }

  // Camera access for player states (non-owning, must be cleared before camera teardown)
  void setCamera(VoidLight::Camera* camera) { mp_camera = camera; }
  VoidLight::Camera* getCamera() const { return mp_camera; }

  // Combat system - all stats stored in EntityDataManager::CharacterData
  void takeDamage(float damage, const Vector2D& knockback = Vector2D(0, 0));
  void heal(float amount);
  void die();
  bool isAlive() const;

  // Combat stat accessors (read from EntityDataManager)
  float getHealth() const;
  float getMaxHealth() const;
  float getStamina() const;
  float getMaxStamina() const;
  float getAttackDamage() const;
  float getAttackRange() const;

  // Combat stat setters (write to EntityDataManager)
  void setMaxHealth(float maxHealth);
  void setMaxStamina(float maxStamina);

  // Stamina management (for combat controller)
  bool canAttack(float staminaCost) const;
  void consumeStamina(float amount);
  void restoreStamina(float amount);

  // Currency convenience methods (wrapper around inventory gold_coins)
  [[nodiscard]] int getGold() const;
  bool addGold(int amount);
  bool removeGold(int amount);
  [[nodiscard]] bool hasGold(int amount) const;

  // Cached gold resource handle — avoids repeated ResourceTemplateManager lookups
  [[nodiscard]] VoidLight::ResourceHandle getGoldHandle() const noexcept { return m_goldHandle; }

private:
  void handleMovementInput(float deltaTime);
  void handleStateTransitions();
  void loadDimensionsFromTexture();
  void setupStates();
  void setupInventory();
  void initializeAnimationMap();
  void onResourceChanged(VoidLight::ResourceHandle resourceHandle,
                         int oldQuantity, int newQuantity);
  EntityStateManager m_stateManager;
  uint32_t m_inventoryIndex{0};       // EDM inventory index (0 = not created yet)
  int m_frameWidth{0};                // Width of a single animation frame
  int m_spriteSheetRows{0};           // Number of rows in the sprite sheet
  SDL_FlipMode m_flip{SDL_FLIP_NONE}; // Default flip direction
  // Note: Animation timing uses m_animationAccumulator inherited from Entity
  float m_movementSpeed{120.0f};      // Movement speed in pixels per second 120 run (2 px/frame at 60 FPS running speed)
  // m_animationMap and m_animationLoops inherited from Entity

  // PERFORMANCE: Cached world bounds to avoid WorldManager::Instance() call every frame
  // Refreshed automatically when bounds are invalid or world version changes
  float m_cachedWorldMinX{0.0f};
  float m_cachedWorldMinY{0.0f};
  float m_cachedWorldMaxX{0.0f};
  float m_cachedWorldMaxY{0.0f};
  bool m_worldBoundsCached{false};
  uint64_t m_cachedWorldVersion{0};  // Track world version for auto-invalidation

  void refreshWorldBoundsCache();

  // Bootstrap: Initial position before EntityDataManager registration
  // Used in ensurePhysicsBodyRegistered() then cleared
  Vector2D m_initialPosition{400.0f, 300.0f};

  // Non-owning camera pointer for player states (set by GamePlayState)
  VoidLight::Camera* mp_camera{nullptr};

  // PERFORMANCE: Cache gold resource handle to avoid repeated lookups in gold methods
  VoidLight::ResourceHandle m_goldHandle{};
};
#endif // PLAYER_HPP
