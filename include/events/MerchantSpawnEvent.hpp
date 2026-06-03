/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef MERCHANT_SPAWN_EVENT_HPP
#define MERCHANT_SPAWN_EVENT_HPP

#include "events/NPCSpawnEvent.hpp"

struct MerchantSpawnParameters {
  std::string merchantClass{"GeneralMerchant"};
  std::string merchantRace{"Human"};
  int count{1};
  float spawnRadius{0.0f};
  bool worldWide{false};
};

class MerchantSpawnEvent : public NPCSpawnEvent {
public:
  MerchantSpawnEvent(const std::string& name,
                     const std::string& merchantClass,
                     const std::string& merchantRace = "Human");
  MerchantSpawnEvent(const std::string& name,
                     const MerchantSpawnParameters& params);
  ~MerchantSpawnEvent() override = default;

  std::string getType() const override { return "MerchantSpawn"; }
  std::string getTypeName() const override { return "MerchantSpawnEvent"; }
  EventTypeId getTypeId() const override { return EventTypeId::MerchantSpawn; }

  [[nodiscard]] const MerchantSpawnParameters& getMerchantSpawnParameters() const {
    return m_params;
  }
  void setMerchantSpawnParameters(const MerchantSpawnParameters& params);

private:
  static SpawnParameters toNPCSpawnParameters(const MerchantSpawnParameters& params);

  MerchantSpawnParameters m_params{};
};

#endif // MERCHANT_SPAWN_EVENT_HPP
