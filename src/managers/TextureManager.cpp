/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "managers/TextureManager.hpp"
#include "core/Logger.hpp"
#include "gpu/GPUDevice.hpp"
#include "gpu/GPUTransferBuffer.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>

bool TextureManager::loadGPU(const std::string& fileName, const std::string& textureID) {
    std::lock_guard<std::mutex> lock(m_gpuTextureMutex);

    if (m_gpuTextureMap.find(textureID) != m_gpuTextureMap.end()) {
        TEXTURE_INFO(std::format("GPU texture already loaded: {}", textureID));
        return true;
    }

    if (std::filesystem::exists(fileName) && std::filesystem::is_directory(fileName)) {
        TEXTURE_INFO(std::format("Loading GPU textures from directory: {}", fileName));

        bool loadedAny = false;
        int texturesLoaded = 0;

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(fileName)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto& filePath = entry.path();
                std::string extension = filePath.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                if (extension != ".png") {
                    continue;
                }

                const std::string fullPath = filePath.string();
                const std::string filename = filePath.stem().string();
                const std::string combinedID =
                    textureID.empty() ? filename : std::format("{}_{}", textureID, filename);

                if (loadSingleGPUTexture(fullPath, combinedID)) {
                    loadedAny = true;
                    ++texturesLoaded;
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            TEXTURE_ERROR(std::format("GPU texture filesystem error: {}", e.what()));
        } catch (const std::exception& e) {
            TEXTURE_ERROR(std::format("GPU texture load error: {}", e.what()));
        }

        TEXTURE_INFO(std::format("Loaded {} GPU textures from directory: {}", texturesLoaded, fileName));
        return loadedAny;
    }

    return loadSingleGPUTexture(fileName, textureID);
}

bool TextureManager::loadSingleGPUTexture(const std::string& fileName, const std::string& textureID) {
    SDL_Surface* rawSurface = SDL_LoadPNG(fileName.c_str());
    if (!rawSurface) {
        TEXTURE_ERROR(std::format("GPU texture: could not load image: {} - {}", fileName, SDL_GetError()));
        return false;
    }

    SDL_Surface* convertedSurface = rawSurface;
    if (rawSurface->format != SDL_PIXELFORMAT_ABGR8888) {
        convertedSurface = SDL_ConvertSurface(rawSurface, SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(rawSurface);
        if (!convertedSurface) {
            TEXTURE_ERROR(std::format("GPU texture: could not convert surface: {}", SDL_GetError()));
            return false;
        }
    }

    SDL_PremultiplyAlpha(convertedSurface->w, convertedSurface->h,
        SDL_PIXELFORMAT_ABGR8888, convertedSurface->pixels, convertedSurface->pitch,
        SDL_PIXELFORMAT_ABGR8888, convertedSurface->pixels, convertedSurface->pitch,
        false);

    const auto& gpuDevice = VoidLight::GPUDevice::Instance();
    if (!gpuDevice.isInitialized()) {
        TEXTURE_ERROR("GPU texture: GPUDevice not initialized");
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    const uint32_t width = static_cast<uint32_t>(convertedSurface->w);
    const uint32_t height = static_cast<uint32_t>(convertedSurface->h);

    auto gpuTexture = std::make_shared<VoidLight::GPUTexture>(
        gpuDevice.get(),
        width,
        height,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER);

    if (!gpuTexture->isValid()) {
        TEXTURE_ERROR(std::format("GPU texture: failed to create texture for: {}", textureID));
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    m_gpuTextureMap[textureID] = GPUTextureData{
        .texture = std::move(gpuTexture),
        .width = static_cast<float>(width),
        .height = static_cast<float>(height)};

    m_pendingUploads.push_back(PendingTextureUpload{
        textureID,
        std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)>(convertedSurface, SDL_DestroySurface),
        width,
        height});

    TEXTURE_INFO(std::format("GPU texture queued for upload: {} ({}x{})", textureID, width, height));
    return true;
}

void TextureManager::processPendingUploads(SDL_GPUCopyPass* copyPass) {
    if (!copyPass || m_pendingUploads.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_gpuTextureMutex);

    const auto& gpuDevice = VoidLight::GPUDevice::Instance();
    if (!gpuDevice.isInitialized()) {
        TEXTURE_ERROR("GPU texture upload: GPUDevice not initialized");
        return;
    }

    for (auto& pending : m_pendingUploads) {
        auto it = m_gpuTextureMap.find(pending.textureID);
        if (it == m_gpuTextureMap.end()) {
            TEXTURE_WARN(std::format("GPU texture upload: texture not found: {}", pending.textureID));
            continue;
        }

        SDL_Surface* surface = pending.surface.get();
        if (!surface) {
            TEXTURE_WARN(std::format("GPU texture upload: null surface for: {}", pending.textureID));
            continue;
        }

        const uint32_t dataSize = static_cast<uint32_t>(surface->pitch * surface->h);

        VoidLight::GPUTransferBuffer transferBuffer(
            gpuDevice.get(),
            SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            dataSize);

        if (!transferBuffer.isValid()) {
            TEXTURE_ERROR(std::format("GPU texture upload: failed to create transfer buffer for: {}", pending.textureID));
            continue;
        }

        void* mapped = transferBuffer.map(false);
        if (!mapped) {
            TEXTURE_ERROR(std::format("GPU texture upload: failed to map transfer buffer for: {}", pending.textureID));
            continue;
        }

        std::memcpy(mapped, surface->pixels, dataSize);
        transferBuffer.unmap();

        SDL_GPUTextureTransferInfo srcInfo{};
        srcInfo.transfer_buffer = transferBuffer.get();
        srcInfo.offset = 0;
        srcInfo.pixels_per_row = pending.width;
        srcInfo.rows_per_layer = pending.height;

        SDL_GPUTextureRegion dstRegion{};
        dstRegion.texture = it->second.texture->get();
        dstRegion.x = 0;
        dstRegion.y = 0;
        dstRegion.z = 0;
        dstRegion.w = pending.width;
        dstRegion.h = pending.height;
        dstRegion.d = 1;

        SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);

        TEXTURE_DEBUG(std::format("GPU texture uploaded: {} ({}x{})", pending.textureID, pending.width, pending.height));
    }

    m_pendingUploads.clear();
}

std::optional<GPUTextureData> TextureManager::getGPUTextureData(const std::string& textureID) const {
    std::lock_guard<std::mutex> lock(m_gpuTextureMutex);

    const auto it = m_gpuTextureMap.find(textureID);
    if (it != m_gpuTextureMap.end()) {
        return it->second;
    }

    return std::nullopt;
}

void TextureManager::clean() {
    if (m_isShutdown) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_gpuTextureMutex);
    m_gpuTextureMap.clear();
    m_pendingUploads.clear();
    m_isShutdown = true;
}
