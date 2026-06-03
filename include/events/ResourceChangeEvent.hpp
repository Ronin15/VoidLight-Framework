/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef RESOURCE_CHANGE_EVENT_HPP
#define RESOURCE_CHANGE_EVENT_HPP

#include "entities/EntityHandle.hpp"
#include "events/Event.hpp"
#include "utils/ResourceHandle.hpp"
#include <string>

/**
 * @brief Event fired when a resource quantity changes in an inventory
 *
 * This event is triggered whenever resources are added, removed, or modified
 * in any EDM-backed inventory. It allows systems to react to inventory changes
 * such as updating UI displays, triggering achievements, or logging
 * transactions.
 */
class ResourceChangeEvent : public Event {
public:
  /**
   * @brief Constructs a resource change event
   * @param ownerHandle Handle of entity that owns the inventory
   * @param resourceHandle Handle of the resource that changed
   * @param oldQuantity Previous quantity of the resource
   * @param newQuantity New quantity of the resource
   * @param changeReason Optional reason for the change (e.g., "crafted",
   * "consumed", "traded")
   */
  ResourceChangeEvent(EntityHandle ownerHandle,
                      VoidLight::ResourceHandle resourceHandle,
                      int oldQuantity, int newQuantity,
                      const std::string &changeReason = "");

  ~ResourceChangeEvent() override = default;

  // Event interface implementation
  void update() override {}
  void execute() override {}
  void reset() override {
    Event::resetCooldown();
    m_ownerHandle = EntityHandle{};
    m_resourceHandle = VoidLight::ResourceHandle{};
    m_oldQuantity = 0;
    m_newQuantity = 0;
    m_changeReason.clear();
  }
  void clean() override {}
  std::string getName() const override { return "ResourceChange"; }
  bool checkConditions() override { return true; }
  std::string getType() const override { return EVENT_TYPE; }
  std::string getTypeName() const override { return "ResourceChangeEvent"; }
  EventTypeId getTypeId() const override { return EventTypeId::ResourceChange; }
  static const std::string EVENT_TYPE;

  // Resource change data
  EntityHandle getOwnerHandle() const { return m_ownerHandle; }
  VoidLight::ResourceHandle getResourceHandle() const {
    return m_resourceHandle;
  }
  int getOldQuantity() const { return m_oldQuantity; }
  int getNewQuantity() const { return m_newQuantity; }
  int getQuantityChange() const { return m_newQuantity - m_oldQuantity; }
  const std::string &getChangeReason() const { return m_changeReason; }

  // Convenience methods
  bool isIncrease() const { return m_newQuantity > m_oldQuantity; }
  bool isDecrease() const { return m_newQuantity < m_oldQuantity; }
  bool isResourceAdded() const {
    return m_oldQuantity == 0 && m_newQuantity > 0;
  }
  bool isResourceRemoved() const {
    return m_oldQuantity > 0 && m_newQuantity == 0;
  }

  // Pooling support - set all fields for reuse
  void set(EntityHandle ownerHandle, VoidLight::ResourceHandle resourceHandle,
           int oldQuantity, int newQuantity, const std::string &changeReason) {
    m_ownerHandle = ownerHandle;
    m_resourceHandle = resourceHandle;
    m_oldQuantity = oldQuantity;
    m_newQuantity = newQuantity;
    m_changeReason = changeReason;
  }

private:
  EntityHandle m_ownerHandle; // Handle of entity that owns the inventory
  VoidLight::ResourceHandle
      m_resourceHandle;       // Handle of the resource that changed
  int m_oldQuantity;          // Previous quantity
  int m_newQuantity;          // New quantity
  std::string m_changeReason; // Reason for the change
};

#endif // RESOURCE_CHANGE_EVENT_HPP
