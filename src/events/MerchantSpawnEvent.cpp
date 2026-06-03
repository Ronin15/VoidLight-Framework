/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "events/MerchantSpawnEvent.hpp"

namespace {

MerchantSpawnParameters makeMerchantSpawnParameters(const std::string& merchantClass,
                                                    const std::string& merchantRace) {
  MerchantSpawnParameters params;
  params.merchantClass = merchantClass;
  params.merchantRace = merchantRace;
  return params;
}

} // namespace

MerchantSpawnEvent::MerchantSpawnEvent(const std::string& name,
                                       const std::string& merchantClass,
                                       const std::string& merchantRace)
    : MerchantSpawnEvent(name, makeMerchantSpawnParameters(merchantClass, merchantRace)) {}

MerchantSpawnEvent::MerchantSpawnEvent(const std::string& name,
                                       const MerchantSpawnParameters& params)
    : NPCSpawnEvent(name, toNPCSpawnParameters(params)), m_params(params) {}

void MerchantSpawnEvent::setMerchantSpawnParameters(const MerchantSpawnParameters& params) {
  m_params = params;
  setSpawnParameters(toNPCSpawnParameters(params));
}

SpawnParameters MerchantSpawnEvent::toNPCSpawnParameters(const MerchantSpawnParameters& params) {
  SpawnParameters spawnParams(params.merchantClass, params.count, params.spawnRadius,
                              params.merchantRace);
  spawnParams.aiBehavior = "Idle";
  spawnParams.worldWide = params.worldWide;
  spawnParams.fadeIn = true;
  spawnParams.fadeTime = 0.25f;
  spawnParams.playSpawnEffect = false;
  return spawnParams;
}
