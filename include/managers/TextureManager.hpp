/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef TEXTURE_MANAGER_HPP
#define TEXTURE_MANAGER_HPP

#include "gpu/GPUTexture.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct GPUTextureData {
    std::shared_ptr<VoidLight::GPUTexture> texture;
    float width{0.0f};
    float height{0.0f};
};

struct PendingTextureUpload {
    std::string textureID;
    std::unique_ptr<SDL_Surface, void(*)(SDL_Surface*)> surface;
    uint32_t width{0};
    uint32_t height{0};
};

class TextureManager {
 public:
  ~TextureManager() {
    if (!m_isShutdown) {
      clean();
    }
  }

  static TextureManager& Instance() {
    static TextureManager instance;
    return instance;
  }

  [[nodiscard]] bool loadGPU(const std::string& fileName, const std::string& textureID);
  void processPendingUploads(SDL_GPUCopyPass* copyPass);
  std::optional<GPUTextureData> getGPUTextureData(const std::string& textureID) const;
  bool hasGPUTextures() const { return !m_gpuTextureMap.empty(); }
  bool hasPendingUploads() const { return !m_pendingUploads.empty(); }

  void clean();
  bool isShutdown() const { return m_isShutdown; }

 private:
  [[nodiscard]] bool loadSingleGPUTexture(const std::string& fileName, const std::string& textureID);

  bool m_isShutdown{false};
  std::unordered_map<std::string, GPUTextureData> m_gpuTextureMap{};
  std::vector<PendingTextureUpload> m_pendingUploads{};
  mutable std::mutex m_gpuTextureMutex{};

  TextureManager() = default;
  TextureManager(const TextureManager&) = delete;
  TextureManager& operator=(const TextureManager&) = delete;
};

#endif  // TEXTURE_MANAGER_HPP
