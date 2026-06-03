/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "entities/Player.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "core/GameEngine.hpp"
#include "core/Logger.hpp"
#include "entities/playerStates/PlayerAttackingState.hpp"
#include "entities/playerStates/PlayerDyingState.hpp"
#include "entities/playerStates/PlayerHurtState.hpp"
#include "entities/playerStates/PlayerIdleState.hpp"
#include "entities/playerStates/PlayerRunningState.hpp"
#include "entities/resources/ItemResources.hpp"

#include "managers/CollisionManager.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ResourceTemplateManager.hpp"
#include "managers/TextureManager.hpp"
#include "managers/WorldManager.hpp"
#include "gpu/GPURenderer.hpp"
#include "gpu/GPUTypes.hpp"

#include <algorithm>
#include <format>
#include <unordered_map>
#include <vector>

namespace {

using ResourceQuantitySnapshot =
    std::unordered_map<VoidLight::ResourceHandle, int>;

void addTrackedResource(ResourceQuantitySnapshot& snapshot,
                        EntityDataManager& edm,
                        uint32_t inventoryIndex,
                        VoidLight::ResourceHandle handle) {
  if (!handle.isValid() || snapshot.contains(handle)) {
    return;
  }

  snapshot.emplace(handle, edm.getInventoryQuantity(inventoryIndex, handle));
}

ResourceQuantitySnapshot collectInventoryResourceSnapshot(
    EntityDataManager& edm,
    EntityHandle ownerHandle,
    uint32_t inventoryIndex,
    VoidLight::ResourceHandle explicitHandle) {
  ResourceQuantitySnapshot snapshot;
  if (!edm.isValidInventoryIndex(inventoryIndex)) {
    return snapshot;
  }

  addTrackedResource(snapshot, edm, inventoryIndex, explicitHandle);

  const size_t maxSlots = edm.getInventoryData(inventoryIndex).maxSlots;
  std::vector<InventorySlotData> slots(maxSlots);
  const size_t slotCount = edm.getInventorySlots(inventoryIndex, slots);
  for (size_t i = 0; i < slotCount; ++i) {
    addTrackedResource(snapshot, edm, inventoryIndex, slots[i].resourceHandle);
  }

  if (ownerHandle.isValid() && ownerHandle.hasHealth()) {
    const auto& charData = edm.getCharacterData(ownerHandle);
    for (const auto& equippedHandle : charData.equippedItems) {
      addTrackedResource(snapshot, edm, inventoryIndex, equippedHandle);
    }
  }

  return snapshot;
}

bool applyConsumableEffect(Player& player, const Consumable& consumable) {
  const float power = static_cast<float>(consumable.getEffectPower());
  switch (consumable.getEffect()) {
    case Consumable::ConsumableEffect::HealHP:
      player.heal(power);
      return true;
    case Consumable::ConsumableEffect::RestoreStamina:
      player.restoreStamina(power);
      return true;
    default:
      return false;
  }
}

bool isSupportedConsumableEffect(const Consumable& consumable) {
  switch (consumable.getEffect()) {
    case Consumable::ConsumableEffect::HealHP:
    case Consumable::ConsumableEffect::RestoreStamina:
      return true;
    default:
      return false;
  }
}

} // namespace

Player::Player() : Entity() {
  // Register with EntityDataManager FIRST - data must exist before any state
  // setup This establishes the single source of truth for all entity data
  auto &edm = EntityDataManager::Instance();
  if (edm.isInitialized()) {
    // Use default half-sizes, will be updated in ensurePhysicsBodyRegistered
    EntityHandle handle =
        edm.registerPlayer(getID(), m_initialPosition, 16.0f, 16.0f);
    setHandle(handle);
    edm.setCharacterBaseStats(handle, 100.0f, 100.0f, 25.0f, 50.0f,
                              m_movementSpeed);
  }

  m_textureID =
      "player"; // Texture ID as loaded by TextureManager from res/img directory

  // Animation properties
  m_currentFrame = 1;    // Start with first frame
  m_currentRow = 1;      // In TextureManager::drawFrame, rows start at 1
  m_numFrames = 2;       // Number of frames in the animation
  m_animSpeed = 100;     // Animation speed in milliseconds
  m_spriteSheetRows = 1; // Number of rows in the sprite sheet
  m_animationAccumulator = 0.0f; // deltaTime accumulator for animation timing
  m_flip = SDL_FLIP_NONE;        // Default flip direction

  // Set width and height based on texture dimensions if the texture is loaded
  loadDimensionsFromTexture();

  // Initialize animation system
  initializeAnimationMap();

  // Setup state manager and add states
  setupStates();

  // Setup inventory system - NOTE: Do NOT call setupInventory() here
  // because it can trigger shared_this() during construction.
  // Call setupInventory() after construction completes.
  // Set default state (now safe - EntityDataManager handle is valid)
  changeState("idle");

  // PLAYER_DEBUG("Player created");
}

// Helper method to get dimensions from the loaded texture
void Player::loadDimensionsFromTexture() {
  // Default dimensions in case texture loading fails
  m_width = 128;
  m_height = 128;    // Set height equal to the sprite sheet row height
  m_frameWidth = 64; // Default frame width (width/numFrames)

  // Cache TextureManager reference for better performance
  const TextureManager &texMgr = TextureManager::Instance();

  float width = 0.0f;
  float height = 0.0f;
  bool gotDimensions = false;

  if (auto gpuData = texMgr.getGPUTextureData(m_textureID); gpuData) {
    width = gpuData->width;
    height = gpuData->height;
    gotDimensions = (width > 0 && height > 0);
    if (gotDimensions) {
      PLAYER_DEBUG(std::format("Got dimensions from GPU texture: {}x{}", width, height));
    }
  }

  if (gotDimensions) {
    PLAYER_DEBUG(
        std::format("Original texture dimensions: {}x{}", width, height));

    // Store original dimensions for full sprite sheet
    m_width = static_cast<int>(width);
    m_height = static_cast<int>(height);

    // Calculate frame dimensions based on sprite sheet layout
    m_frameWidth = m_width / m_numFrames;           // Width per frame
    int frameHeight = m_height / m_spriteSheetRows; // Height per row

    // Update height to be the height of a single frame
    m_height = frameHeight;

    // Sync new dimensions to collision body if already registered
    Vector2D newHalfSize(m_frameWidth * 0.5f, m_height * 0.5f);
    CollisionManager::Instance().updateCollisionBodySize(getID(),
                                                         newHalfSize);

    PLAYER_DEBUG(
        std::format("Loaded texture dimensions: {}x{}", m_width, height));
    PLAYER_DEBUG(
        std::format("Frame dimensions: {}x{}", m_frameWidth, frameHeight));
    PLAYER_DEBUG(std::format("Sprite layout: {} columns x {} rows",
                             m_numFrames, m_spriteSheetRows));
  } else {
    PLAYER_ERROR(
        std::format("Texture '{}' not found in TextureManager", m_textureID));
  }
}

void Player::setupStates() {
  // Create and add states
  m_stateManager.addState("idle", std::make_unique<PlayerIdleState>(*this));
  m_stateManager.addState("running",
                          std::make_unique<PlayerRunningState>(*this));
  m_stateManager.addState("attacking",
                          std::make_unique<PlayerAttackingState>(*this));
  m_stateManager.addState("hurt", std::make_unique<PlayerHurtState>(*this));
  m_stateManager.addState("dying", std::make_unique<PlayerDyingState>(*this));
}

Player::~Player() {
  // Don't call virtual functions from destructors
  // Instead of calling clean(), directly handle cleanup here

  PLAYER_DEBUG("Cleaning up player resources");
  PLAYER_DEBUG("Player resources cleaned!");
}

void Player::changeState(const std::string &stateName) {
  if (m_stateManager.hasState(stateName)) {
    m_stateManager.setState(stateName);
  } else {
    PLAYER_ERROR(std::format("Player state not found: {}", stateName));
  }
}

std::string Player::getCurrentStateName() const {
  return m_stateManager.getCurrentStateName();
}

void Player::update(float deltaTime) {
  // Store position for render interpolation (must be first!)
  storePositionForInterpolation();

  // State machine handles input and sets velocity
  m_stateManager.update(deltaTime);

  // MOVEMENT INTEGRATION: Apply velocity to position (same as AIManager does
  // for NPCs) This is the core physics step that makes the player move Use
  // getPosition()/getVelocity() to read from EntityDataManager (single source
  // of truth)
  Vector2D currentVel = getVelocity();

  // Apply knockback impulse (decays over multiple frames).
  // Main-thread: can call EDM directly without going through BehaviorContext.
  auto& edm = EntityDataManager::Instance();
  const size_t playerIdx = edm.getIndex(m_handle);
  if (playerIdx != SIZE_MAX)
  {
    if (auto* kb = edm.getKnockback(playerIdx))
    {
      currentVel.setX(currentVel.getX() + kb->impulseX);
      currentVel.setY(currentVel.getY() + kb->impulseY);
      kb->impulseX *= Knockback::DECAY;
      kb->impulseY *= Knockback::DECAY;
      if (--kb->framesRemaining == 0)
      {
        edm.clearKnockback(playerIdx);
      }
    }
  }

  Vector2D newPos = getPosition() + (currentVel * deltaTime);

  // WORLD BOUNDS CONSTRAINT: Clamp player position to stay within world
  // boundaries PERFORMANCE: Use cached bounds instead of calling
  // WorldManager::Instance() every frame Auto-invalidate cache when world
  // version changes (new world loaded/generated)
  const uint64_t currentWorldVersion =
      WorldManager::Instance().getWorldVersion();
  if (!m_worldBoundsCached || m_cachedWorldVersion != currentWorldVersion) {
    refreshWorldBoundsCache();
  }

  // Always clamp if bounds are valid (maxX > minX)
  if (m_cachedWorldMaxX > m_cachedWorldMinX &&
      m_cachedWorldMaxY > m_cachedWorldMinY) {
    // Account for player half-size to prevent center from going out of bounds
    const float halfWidth = m_frameWidth * 0.5f;
    const float halfHeight = m_height * 0.5f;

    // Store original position before clamping
    const float originalX = newPos.getX();
    const float originalY = newPos.getY();

    // Clamp position to world bounds (with player size offset)
    const float clampedX = std::clamp(originalX, m_cachedWorldMinX + halfWidth,
                                      m_cachedWorldMaxX - halfWidth);
    const float clampedY = std::clamp(originalY, m_cachedWorldMinY + halfHeight,
                                      m_cachedWorldMaxY - halfHeight);

    // Update position and stop velocity if we hit a boundary
    if (clampedX != originalX) {
      newPos.setX(clampedX);
      currentVel.setX(0.0f); // Stop horizontal movement at edge
    }
    if (clampedY != originalY) {
      newPos.setY(clampedY);
      currentVel.setY(0.0f); // Stop vertical movement at edge
    }

    // Write velocity back if it was modified by boundary collision
    if (clampedX != originalX || clampedY != originalY) {
      setVelocity(currentVel);
    }
  }

  // Write position using movement method (preserves previousPosition for interpolation)
  updatePositionFromMovement(newPos);

  // Update collision body with new position and velocity
  auto &cm = CollisionManager::Instance();
  cm.updateCollisionBodyPosition(m_id, newPos);
  cm.updateCollisionBodyVelocity(m_id, currentVel);

  // If the texture dimensions haven't been loaded yet, try loading them
  if (m_frameWidth == 0 &&
      TextureManager::Instance().getGPUTextureData(m_textureID).has_value()) {
    loadDimensionsFromTexture();
  }
}

void Player::recordGPUVertices(VoidLight::GPURenderer& gpuRenderer,
                               float cameraX, float cameraY,
                               float interpolationAlpha) {
  // Get GPU texture for player
  auto gpuTextureData = TextureManager::Instance().getGPUTextureData(m_textureID);
  if (!gpuTextureData || !gpuTextureData->texture) {
    return;
  }

  // Get entity batch and vertex pool
  auto& entityBatch = gpuRenderer.getEntityBatch();
  auto& vertexPool = gpuRenderer.getEntityVertexPool();

  auto* writePtr = static_cast<VoidLight::SpriteVertex*>(vertexPool.getMappedPtr());
  if (!writePtr) {
    return;
  }

  // Get texture dimensions
  float texWidth = gpuTextureData->width;
  float texHeight = gpuTextureData->height;

  entityBatch.begin(writePtr, vertexPool.getMaxVertices(),
                    gpuTextureData->texture->get(), gpuRenderer.getNearestSampler(),
                    texWidth, texHeight,
                    static_cast<float>(gpuRenderer.getSceneTexture()->getHeight()));

  // Get interpolated position
  Vector2D interpPos = getInterpolatedPosition(interpolationAlpha);

  // Convert world coords to screen coords
  float renderX = interpPos.getX() - cameraX - (m_frameWidth / 2.0f);
  float renderY = interpPos.getY() - cameraY - (m_height / 2.0f);

  // Source rect from sprite sheet
  float srcX = static_cast<float>(m_frameWidth * m_currentFrame);
  float srcY = static_cast<float>(m_height * (m_currentRow - 1));
  float srcW = static_cast<float>(m_frameWidth);
  float srcH = static_cast<float>(m_height);

  // Calculate UV coordinates
  float u0 = srcX / texWidth;
  float v0 = srcY / texHeight;
  float u1 = (srcX + srcW) / texWidth;
  float v1 = (srcY + srcH) / texHeight;

  // Handle horizontal flip by swapping U coordinates
  if (m_flip == SDL_FLIP_HORIZONTAL) {
    std::swap(u0, u1);
  }

  // Draw the sprite using UV coordinates
  entityBatch.drawUV(u0, v0, u1, v1,
                     renderX, renderY,
                     static_cast<float>(m_frameWidth), static_cast<float>(m_height),
                     255, 255, 255, 255);

  entityBatch.end();
}

void Player::renderGPU(VoidLight::GPURenderer& gpuRenderer,
                       SDL_GPURenderPass* scenePass) {
  auto& entityBatch = gpuRenderer.getEntityBatch();
  auto& vertexPool = gpuRenderer.getEntityVertexPool();

  if (!entityBatch.hasSprites()) {
    return;
  }

  // Get scene texture for ortho matrix dimensions
  auto* sceneTexture = gpuRenderer.getSceneTexture();
  if (!sceneTexture) {
    return;
  }

  // Create orthographic projection
  float orthoMatrix[16];
  VoidLight::GPURenderer::createOrthoMatrix(
      0.0f, static_cast<float>(sceneTexture->getWidth()),
      0.0f, static_cast<float>(sceneTexture->getHeight()),
      orthoMatrix);

  // Push view-projection matrix
  gpuRenderer.pushViewProjection(scenePass, orthoMatrix);

  // Render using the sprite alpha pipeline (supports transparency)
  entityBatch.render(scenePass, gpuRenderer.getSpriteAlphaPipeline(),
                     vertexPool.getGPUBuffer());
}

void Player::clean() {
  // Clean up any resources
  PLAYER_DEBUG("Cleaning up player resources");

  // Destroy EDM inventory
  auto &edm = EntityDataManager::Instance();
  if (edm.isInitialized() && m_inventoryIndex != INVALID_INVENTORY_INDEX) {
    edm.destroyInventory(m_inventoryIndex);
    m_inventoryIndex = INVALID_INVENTORY_INDEX;
  }

  // Unregister from EntityDataManager
  if (edm.isInitialized()) {
    edm.unregisterEntity(getID());
  }
}

void Player::ensurePhysicsBodyRegistered() {
  // EDM-CENTRIC: Set collision layers directly in EDM
  // Movables are managed entirely by EDM - no CollisionManager storage entry
  // needed
  if (!hasValidHandle())
    return;

  auto &edm = EntityDataManager::Instance();
  size_t edmIdx = edm.getIndex(getHandle());
  if (edmIdx == SIZE_MAX)
    return;

  auto &hot = edm.getHotDataByIndex(edmIdx);

  // Player collides with everything except pets (pets pass through player)
  // Layer_Player is already set in registerPlayer(), just set mask
  hot.collisionMask = 0xFFFF & ~VoidLight::CollisionLayer::Layer_Pet;
  hot.setCollisionEnabled(true);
}

void Player::setVelocity(const Vector2D &velocity) {
  // Update EntityDataManager (single source of truth) via base class
  // EDM-CENTRIC: No CollisionManager entry for movables
  Entity::setVelocity(velocity);
}

void Player::setPosition(const Vector2D &position) {
  // Update EntityDataManager (single source of truth) via base class
  // EDM-CENTRIC: No CollisionManager entry for movables
  Entity::setPosition(position);
}

void Player::initializeInventory() {
  // Create EDM inventory with 20 slots (forces meaningful inventory decisions)
  auto &edm = EntityDataManager::Instance();
  m_inventoryIndex = edm.createInventory(20, false);  // Player inventory is not world resource storage.

  if (m_inventoryIndex == INVALID_INVENTORY_INDEX) {
    PLAYER_ERROR("Failed to create player inventory");
    return;
  }
  edm.setCharacterInventoryIndex(m_handle, m_inventoryIndex);

  // Give player some starting resources using ResourceTemplateManager
  const auto &templateManager = ResourceTemplateManager::Instance();

  m_goldHandle = templateManager.getHandleById("gold_coins");
  if (m_goldHandle.isValid()) {
    edm.addToInventory(m_inventoryIndex, m_goldHandle, 100);
  } else {
    PLAYER_WARN("Player::initializeInventory - gold_coins resource not found");
  }

  const auto healthPotionHandle = templateManager.getHandleById("health_potion");
  if (healthPotionHandle.isValid()) {
    edm.addToInventory(m_inventoryIndex, healthPotionHandle, 3);
  }

  PLAYER_DEBUG(std::format("Player EDM inventory initialized with index {}", m_inventoryIndex));
}

void Player::onResourceChanged(VoidLight::ResourceHandle resourceHandle,
                               int oldQuantity, int newQuantity) {
  // Use EventManager hub to trigger a ResourceChange (no registration needed)
  EventManager::Instance().triggerResourceChange(
      getHandle(), resourceHandle, oldQuantity, newQuantity, "player_action",
      EventManager::DispatchMode::Deferred);

  PLAYER_DEBUG(std::format(
      "Resource changed: {} from {} to {} - event dispatched to EventManager",
      resourceHandle.toString(), oldQuantity, newQuantity));
}

// Resource management - delegates to EntityDataManager
bool Player::addToInventory(VoidLight::ResourceHandle handle, int quantity) {
  if (m_inventoryIndex == INVALID_INVENTORY_INDEX) {
    PLAYER_WARN("Player::addToInventory - Inventory not initialized");
    return false;
  }
  int oldQty = EntityDataManager::Instance().getInventoryQuantity(m_inventoryIndex, handle);
  bool result = EntityDataManager::Instance().addToInventory(m_inventoryIndex, handle, quantity);
  if (result) {
    int newQty = EntityDataManager::Instance().getInventoryQuantity(m_inventoryIndex, handle);
    onResourceChanged(handle, oldQty, newQty);
  }
  return result;
}

bool Player::removeFromInventory(VoidLight::ResourceHandle handle, int quantity) {
  if (m_inventoryIndex == INVALID_INVENTORY_INDEX) {
    PLAYER_WARN("Player::removeFromInventory - Inventory not initialized");
    return false;
  }
  int oldQty = EntityDataManager::Instance().getInventoryQuantity(m_inventoryIndex, handle);
  bool result = EntityDataManager::Instance().removeFromInventory(m_inventoryIndex, handle, quantity);
  if (result) {
    int newQty = EntityDataManager::Instance().getInventoryQuantity(m_inventoryIndex, handle);
    onResourceChanged(handle, oldQty, newQty);
  }
  return result;
}

int Player::getInventoryQuantity(VoidLight::ResourceHandle handle) const {
  if (m_inventoryIndex == INVALID_INVENTORY_INDEX) {
    return 0;
  }
  return EntityDataManager::Instance().getInventoryQuantity(m_inventoryIndex, handle);
}

bool Player::hasInInventory(VoidLight::ResourceHandle handle, int quantity) const {
  if (m_inventoryIndex == INVALID_INVENTORY_INDEX) {
    return false;
  }
  return EntityDataManager::Instance().hasInInventory(m_inventoryIndex, handle, quantity);
}

// Currency convenience methods
int Player::getGold() const {
  if (!m_goldHandle.isValid()) {
    return 0;
  }
  return getInventoryQuantity(m_goldHandle);
}

bool Player::addGold(int amount) {
  if (amount <= 0) {
    return false;
  }
  if (!m_goldHandle.isValid()) {
    PLAYER_WARN("Player::addGold - gold resource not found");
    return false;
  }
  return addToInventory(m_goldHandle, amount);
}

bool Player::removeGold(int amount) {
  if (amount <= 0) {
    return false;
  }
  if (!m_goldHandle.isValid()) {
    PLAYER_WARN("Player::removeGold - gold resource not found");
    return false;
  }
  return removeFromInventory(m_goldHandle, amount);
}

bool Player::hasGold(int amount) const {
  return getGold() >= amount;
}

float Player::getMovementSpeed() const {
  if (!hasValidHandle()) {
    return m_movementSpeed;
  }

  return EntityDataManager::Instance().getCharacterData(m_handle).moveSpeed;
}

// Equipment management
bool Player::equipItem(VoidLight::ResourceHandle itemHandle) {
  auto& edm = EntityDataManager::Instance();
  const auto before = collectInventoryResourceSnapshot(
      edm, m_handle, m_inventoryIndex, itemHandle);
  const bool result = edm.equipCharacterItem(m_handle, itemHandle);
  if (!result) {
    return false;
  }

  const auto after = collectInventoryResourceSnapshot(
      edm, m_handle, m_inventoryIndex, itemHandle);
  for (const auto& [handle, oldQuantity] : before) {
    const auto afterIt = after.find(handle);
    const int newQuantity =
        afterIt != after.end() ? afterIt->second : 0;
    if (oldQuantity != newQuantity) {
      onResourceChanged(handle, oldQuantity, newQuantity);
    }
  }
  for (const auto& [handle, newQuantity] : after) {
    if (!before.contains(handle) && newQuantity != 0) {
      onResourceChanged(handle, 0, newQuantity);
    }
  }
  return true;
}

bool Player::unequipItem(const std::string &slotName) {
  auto& edm = EntityDataManager::Instance();
  const VoidLight::ResourceHandle equippedHandle =
      edm.getEquippedCharacterItem(m_handle, slotName);
  const auto before = collectInventoryResourceSnapshot(
      edm, m_handle, m_inventoryIndex, equippedHandle);
  const bool result = edm.unequipCharacterItem(m_handle, slotName);
  if (!result) {
    return false;
  }

  const auto after = collectInventoryResourceSnapshot(
      edm, m_handle, m_inventoryIndex, equippedHandle);
  for (const auto& [handle, oldQuantity] : before) {
    const auto afterIt = after.find(handle);
    const int newQuantity =
        afterIt != after.end() ? afterIt->second : 0;
    if (oldQuantity != newQuantity) {
      onResourceChanged(handle, oldQuantity, newQuantity);
    }
  }
  for (const auto& [handle, newQuantity] : after) {
    if (!before.contains(handle) && newQuantity != 0) {
      onResourceChanged(handle, 0, newQuantity);
    }
  }
  return true;
}

VoidLight::ResourceHandle
Player::getEquippedItem(const std::string &slotName) const {
  return EntityDataManager::Instance().getEquippedCharacterItem(m_handle, slotName);
}

// Crafting and consumption
bool Player::canCraft(const std::string &) const {
  // Simplified crafting check - in a real game you'd have a proper recipe
  // system
  return false;   // Not implemented yet
}

bool Player::craftItem(const std::string &) {
  // Simplified crafting - in a real game you'd have a proper recipe system
  return false;   // Not implemented yet
}

bool Player::consumeItem(VoidLight::ResourceHandle itemHandle) {
  if (m_inventoryIndex == INVALID_INVENTORY_INDEX) {
    PLAYER_WARN("Player::consumeItem - Inventory not initialized");
    return false;
  }

  if (!itemHandle.isValid()) {
    PLAYER_ERROR("Player::consumeItem - Invalid resource handle");
    return false;
  }

  auto &edm = EntityDataManager::Instance();
  if (!edm.hasInInventory(m_inventoryIndex, itemHandle, 1)) {
    PLAYER_WARN(std::format("Player::consumeItem - Item not available (handle: {})", itemHandle.toString()));
    return false;
  }

  const auto &templateManager = ResourceTemplateManager::Instance();
  auto itemTemplate = templateManager.getResourceTemplate(itemHandle);
  if (itemTemplate && itemTemplate->getType() == ResourceType::Ammunition) {
    PLAYER_WARN(std::format("Player::consumeItem - Ammunition is consumed by weapons only (handle: {})", itemHandle.toString()));
    return false;
  }

  if (!itemTemplate || !itemTemplate->isConsumable()) {
    PLAYER_WARN(std::format("Player::consumeItem - Item is not consumable (handle: {})", itemHandle.toString()));
    return false;
  }

  const auto consumable = std::dynamic_pointer_cast<Consumable>(itemTemplate);
  if (!consumable || !isSupportedConsumableEffect(*consumable)) {
    PLAYER_WARN(std::format("Player::consumeItem - Unsupported consumable effect (handle: {})", itemHandle.toString()));
    return false;
  }

  const int oldQuantity = edm.getInventoryQuantity(m_inventoryIndex, itemHandle);
  if (edm.removeFromInventory(m_inventoryIndex, itemHandle, 1)) {
    applyConsumableEffect(*this, *consumable);
    onResourceChanged(itemHandle, oldQuantity, oldQuantity - 1);
    PLAYER_DEBUG(
        std::format("Consumed item (handle: {})", itemHandle.toString()));
    return true;
  }

  return false;
}

void Player::refreshWorldBoundsCache() {
  const auto &worldMgr = WorldManager::Instance();
  worldMgr.getWorldBounds(m_cachedWorldMinX, m_cachedWorldMinY,
                          m_cachedWorldMaxX, m_cachedWorldMaxY);
  m_cachedWorldVersion = worldMgr.getWorldVersion();
  m_worldBoundsCached = true;
  PLAYER_DEBUG(
      std::format("World bounds cached: ({}, {}) to ({}, {}), version: {}",
                  m_cachedWorldMinX, m_cachedWorldMinY, m_cachedWorldMaxX,
                  m_cachedWorldMaxY, m_cachedWorldVersion));
}

// Animation abstraction methods

void Player::initializeAnimationMap() {
  // Default animation configuration mapping names to sprite sheet details
  // Current player sprite sheet: 1 row, 2 frames (shared for idle/running)
  // Can be extended when player sprite sheet is expanded with more rows
  m_animationMap = {
      {"idle", {0, 2, 150, true}}, // Row 0, 2 frames, 150ms, looping
      {"running",
       {0, 2, 100, true}}, // Row 0, 2 frames, 100ms, looping (same row as idle)
      {"attacking",
       {0, 2, 80, false}}, // Row 0, 2 frames, 80ms, play once (placeholder)
      {"hurt",
       {0, 2, 100, false}}, // Row 0, 2 frames, 100ms, play once (placeholder)
      {"dying",
       {0, 2, 120, false}} // Row 0, 2 frames, 120ms, play once (placeholder)
  };
}

void Player::playAnimation(const std::string &animName) {
  // Skip if already playing this animation - prevents jitter on state re-entry
  if (m_currentAnimationName == animName) {
    return;
  }

  auto it = m_animationMap.find(animName);
  if (it != m_animationMap.end()) {
    const auto &config = it->second;
    m_currentRow = config.row + 1; // TextureManager uses 1-based rows
    m_numFrames = config.frameCount;
    m_animSpeed = config.speed;
    m_animationLoops = config.loop;
    m_currentFrame = 0;
    m_animationAccumulator = 0.0f;     // Reset deltaTime accumulator
    m_currentAnimationName = animName; // Track current animation
  } else {
    PLAYER_WARN(std::format("Player animation not found: {}", animName));
  }
}

// Combat system methods - all stats stored in EntityDataManager::CharacterData

float Player::getHealth() const {
  return EntityDataManager::Instance().getCharacterData(m_handle).health;
}

float Player::getMaxHealth() const {
  return EntityDataManager::Instance().getCharacterData(m_handle).maxHealth;
}

float Player::getStamina() const {
  return EntityDataManager::Instance().getCharacterData(m_handle).stamina;
}

float Player::getMaxStamina() const {
  return EntityDataManager::Instance().getCharacterData(m_handle).maxStamina;
}

float Player::getAttackDamage() const {
  return EntityDataManager::Instance().getCharacterData(m_handle).attackDamage;
}

float Player::getAttackRange() const {
  return EntityDataManager::Instance().getCharacterData(m_handle).attackRange;
}

bool Player::isAlive() const {
  return EntityDataManager::Instance().getCharacterData(m_handle).health > 0.0f;
}

bool Player::canAttack(float staminaCost) const {
  return EntityDataManager::Instance().getCharacterData(m_handle).stamina >=
         staminaCost;
}

void Player::takeDamage(float damage, const Vector2D &knockback) {
  auto &charData = EntityDataManager::Instance().getCharacterData(m_handle);

  if (charData.health <= 0.0f) {
    return; // Already dead
  }

  charData.health = std::max(0.0f, charData.health - damage);

  // Apply knockback via transform
  if (knockback.length() > 0.001f) {
    auto &transform = EntityDataManager::Instance().getTransform(m_handle);
    transform.position = transform.position + knockback;
  }

  // Handle death or hurt
  if (charData.health <= 0.0f) {
    die();
  } else {
    changeState("hurt");
  }

  PLAYER_DEBUG(std::format("Player took {} damage, health: {}/{}", damage,
                           charData.health, charData.maxHealth));
}

void Player::heal(float amount) {
  auto &charData = EntityDataManager::Instance().getCharacterData(m_handle);

  if (charData.health <= 0.0f) {
    return; // Can't heal dead player
  }

  float oldHealth = charData.health;
  charData.health = std::min(charData.maxHealth, charData.health + amount);

  PLAYER_DEBUG(std::format("Player healed {} HP: {}/{} -> {}/{}", amount,
                           oldHealth, charData.maxHealth, charData.health,
                           charData.maxHealth));
}

void Player::die() {
  Vector2D pos = getPosition();
  PLAYER_INFO(
      std::format("Player died at position ({}, {})", pos.getX(), pos.getY()));

  changeState("dying");

  // Note: Game over / respawn logic would be handled by GamePlayState or
  // similar
}

void Player::setMaxHealth(float maxHealth) {
  auto &charData = EntityDataManager::Instance().getCharacterData(m_handle);
  charData.baseMaxHealth = std::max(1.0f, maxHealth);
  charData.maxHealth = charData.baseMaxHealth;
  charData.health = std::min(charData.health, charData.maxHealth);
}

void Player::setMaxStamina(float maxStamina) {
  auto &charData = EntityDataManager::Instance().getCharacterData(m_handle);
  charData.maxStamina = std::max(1.0f, maxStamina);
  charData.stamina = std::min(charData.stamina, charData.maxStamina);
}

void Player::consumeStamina(float amount) {
  auto &charData = EntityDataManager::Instance().getCharacterData(m_handle);
  charData.stamina = std::max(0.0f, charData.stamina - amount);
}

void Player::restoreStamina(float amount) {
  auto &charData = EntityDataManager::Instance().getCharacterData(m_handle);
  charData.stamina = std::min(charData.maxStamina, charData.stamina + amount);
}
