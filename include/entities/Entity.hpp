/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "entities/EntityHandle.hpp"  // EntityKind, SimulationTier, EntityHandle
#include "utils/UniqueID.hpp"
#include "utils/Vector2D.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declaration for EntityDataManager
class EntityDataManager;

// Forward declarations
class Entity; // Forward declare for smart pointers
class InputHandler;
namespace VoidLight {
    class Camera;
}

// Smart pointer type aliases
using EntityPtr = std::shared_ptr<Entity>;
using EntityWeakPtr = std::weak_ptr<Entity>;

// Note: EntityID is defined in EntityHandle.hpp (included above).
// Note: EntityKind enum is now defined in EntityHandle.hpp
// This provides the expanded entity type list (Player, NPC, DroppedItem, Container,
// Harvestable, Projectile, AreaEffect, Prop, Trigger) and SimulationTier enum.

/**
 * @brief Animation configuration for sprite sheet handling
 * Unified struct used by NPC and Player for named animations
 */
struct AnimationConfig {
    int row;           // Sprite sheet row (0-based)
    int frameCount;    // Number of frames in animation
    int speed;         // Milliseconds per frame
    bool loop{true};   // Whether animation loops (default true, Player uses false for attacks)

    AnimationConfig() : row(0), frameCount(1), speed(100), loop(true) {}
    AnimationConfig(int r, int fc, int s)
        : row(r), frameCount(fc), speed(s), loop(true) {}
    AnimationConfig(int r, int fc, int s, bool l)
        : row(r), frameCount(fc), speed(s), loop(l) {}
};

/**
 * @brief Pure virtual base class for all game objects.
 *
 * This class defines the common interface for all entities in the game,
 * including players, NPCs, items, and other interactive objects. It uses
 * the Component-Entity-System (CES) architecture, where entities are
 * composed of multiple components that define their behavior and
 * appearance.
 */
class Entity : public std::enable_shared_from_this<Entity> {
 public:
  /**
   * @brief Construct a new Entity object and assign it a unique ID.
   */
  Entity() : m_id(VoidLight::UniqueID::generate()) {}

  /**
   * @brief Virtual destructor
   *
   * IMPORTANT: Do NOT call shared_from_this() or any method that uses it
   * (like shared_this()) in the destructor. By the time the destructor runs,
   * all shared_ptrs to this object have been destroyed, and calling
   * shared_from_this() will throw std::bad_weak_ptr.
   */
  virtual ~Entity() = default;

  /**
   * @brief Update the entity's state.
   *
   * This method is called once per frame for each entity. It should
   * update the entity's position, handle input, and perform any other
   * necessary calculations.
   *
   * @param deltaTime The time elapsed since the last frame, in seconds.
   */
  virtual void update(float deltaTime) = 0;

  /**
   * @brief Clean up the entity's resources before destruction
   *
   * This method is called explicitly before an entity is destroyed.
   * It's safe to use shared_from_this() here.
   *
   * IMPORTANT: All entity management operations (such as unassigning from AIManager)
   * should happen here, NOT in the destructor.
   */
  virtual void clean() = 0;

  /**
   * @brief Get the entity's kind for fast type checking without RTTI
   *
   * Use this instead of dynamic_cast in hot paths (e.g., collision filtering,
   * combat hit detection). Each Entity subclass must implement this.
   *
   * @return EntityKind enum value identifying the concrete type
   */
  [[nodiscard]] virtual EntityKind getKind() const = 0;

  /**
   * @brief Helper to get a shared_ptr to this object
   *
   * IMPORTANT: Never call this in constructors or destructors!
   * Only use this when the object is managed by a std::shared_ptr.
   *
   * @return A shared_ptr to this object
   * @throws std::bad_weak_ptr if called from constructor/destructor or if the object
   *         is not managed by a std::shared_ptr
   */
  EntityPtr shared_this() { return shared_from_this(); }

  /**
   * @brief Helper to get a weak_ptr to this object
   *
   * IMPORTANT: Never call this in constructors or destructors!
   * Only use this when the object is managed by a std::shared_ptr.
   *
   * @return A weak_ptr to this object
   * @throws std::bad_weak_ptr if called from constructor/destructor or if the object
   *         is not managed by a std::shared_ptr
   */
  EntityWeakPtr weak_this() { return shared_from_this(); }

  // Accessor methods
  EntityID getID() const { return m_id; }

  /**
   * @brief Get entity handle for EntityDataManager access
   * @return EntityHandle (may be invalid if not registered)
   */
  [[nodiscard]] EntityHandle getHandle() const { return m_handle; }

  /**
   * @brief Check if entity is registered with EntityDataManager
   */
  [[nodiscard]] bool hasValidHandle() const { return m_handle.isValid(); }

  /**
   * @brief Check if entity is in Active simulation tier (should be rendered/updated)
   * @return true if in Active tier, false if Background/Hibernated or no valid handle
   */
  [[nodiscard]] bool isInActiveTier() const;

  // Transform accessors - redirect to EntityDataManager when handle is valid
  // Phase 4: EntityDataManager is the single source of truth for transforms
  Vector2D getPosition() const;
  Vector2D getPreviousPosition() const;
  Vector2D getVelocity() const;
  Vector2D getAcceleration() const;

  /**
   * @brief Get interpolated position for smooth rendering.
   *
   * Uses linear interpolation between previous and current position
   * based on the interpolation alpha from the game loop.
   *
   * Note: With the single-threaded main loop (update completes before render),
   * this is now a simple calculation without atomics.
   *
   * @param alpha Interpolation factor (0.0 = previous position, 1.0 = current position)
   * @return Interpolated position for rendering
   */
  Vector2D getInterpolatedPosition(float alpha) const;

  /**
   * @brief Store current position for interpolation before updating.
   *
   * Call this at the START of update() before modifying position.
   * This enables smooth rendering interpolation between fixed timestep updates.
   */
  void storePositionForInterpolation();

  /**
   * @brief Update position from movement (preserves interpolation state).
   *
   * Use this for smooth movement updates (physics integration, AI movement).
   * Unlike setPosition(), this does NOT reset previousPosition.
   * Call storePositionForInterpolation() before this each frame.
   */
  void updatePositionFromMovement(const Vector2D& position);

  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }
  const std::string& getTextureID() const { return m_textureID; }
  int getCurrentFrame() const { return m_currentFrame; }
  int getCurrentRow() const { return m_currentRow; }
  int getNumFrames() const { return m_numFrames; }
  int getAnimSpeed() const { return m_animSpeed; }
  float getAnimationAccumulator() const { return m_animationAccumulator; }
  const std::string& getCurrentAnimationName() const { return m_currentAnimationName; }

  // Setter methods - redirect to EntityDataManager when handle is valid

  /**
   * @brief Set entity position directly (teleport).
   *
   * This resets both current and previous position to prevent
   * interpolation artifacts when teleporting/spawning.
   * Redirects to EntityDataManager when handle is valid.
   */
  virtual void setPosition(const Vector2D& position);

  /**
   * @brief Set entity velocity.
   * Redirects to EntityDataManager when handle is valid.
   */
  virtual void setVelocity(const Vector2D& velocity);

  /**
   * @brief Set entity acceleration.
   * Redirects to EntityDataManager when handle is valid.
   */
  virtual void setAcceleration(const Vector2D& acceleration);
  virtual void setWidth(int width) { m_width = width; }
  virtual void setHeight(int height) { m_height = height; }
  virtual void setTextureID(const std::string& id) { m_textureID = id; }
  virtual void setCurrentFrame(int frame) { m_currentFrame = frame; }
  virtual void setCurrentRow(int row) { m_currentRow = row; }
  virtual void setNumFrames(int numFrames) { m_numFrames = numFrames; }
  virtual void setAnimSpeed(int speed) { m_animSpeed = speed; }
  virtual void setAnimationAccumulator(float acc) { m_animationAccumulator = acc; }

  // Used for rendering flipping - to be implemented by derived classes
  virtual void setFlip(SDL_FlipMode) {}
  virtual SDL_FlipMode getFlip() const { return SDL_FLIP_NONE; }

  /**
   * @brief Play a named animation from the animation map
   *
   * Looks up the animation config by name and sets the sprite sheet row,
   * frame count, animation speed, and loop flag. Does nothing if animation
   * name is not found in the map.
   *
   * @param animName The name of the animation (e.g., "idle", "walking", "attacking")
   */
  virtual void playAnimation(const std::string& animName);

  // Note: initializeAnimationMap() is implemented separately in Player and NPC
  // as a private non-virtual method called from their respective constructors.
  // No base class virtual is needed since it's never called polymorphically.

 protected:
  /**
   * @brief Set the entity handle after registration with EntityDataManager
   *
   * Called by derived classes after they register with EntityDataManager.
   * Once set, transform accessors redirect to EntityDataManager.
   */
  void setHandle(EntityHandle handle) { m_handle = handle; }

  /**
   * @brief Register entity with EntityDataManager (for test entities)
   *
   * Convenience method for derived classes (especially test entities) that need to
   * register with EntityDataManager. Production code (NPC, Player) uses specific
   * methods (registerNPC, registerPlayer) for full registration.
   *
   * @param position Initial entity position
   * @param halfWidth Half width for collision bounds
   * @param halfHeight Half height for collision bounds
   * @param kind Entity kind (defaults to NPC for test entities)
   */
  void registerWithDataManager(const Vector2D& position, float halfWidth = 16.0f,
                                float halfHeight = 16.0f, EntityKind kind = EntityKind::NPC);

  const EntityID m_id;
  EntityHandle m_handle;  // Handle for EntityDataManager access (Phase 4)

  int m_width{0};
  int m_height{0};
  std::string m_textureID{};
  int m_currentFrame{0};
  int m_currentRow{0};
  int m_numFrames{0};
  int m_animSpeed{0};

  // Animation abstraction - maps animation names to sprite sheet configurations
  std::unordered_map<std::string, AnimationConfig> m_animationMap;
  bool m_animationLoops{true};  // Whether current animation loops or plays once

  // Animation timing - uses deltaTime accumulation for synchronized timing with physics
  std::string m_currentAnimationName;      // Current animation name (for skip-if-same optimization)
  float m_animationAccumulator{0.0f};      // Accumulates deltaTime for frame advancement
};
#endif  // ENTITY_HPP
