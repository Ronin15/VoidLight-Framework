/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_SHADER_MANAGER_HPP
#define GPU_SHADER_MANAGER_HPP

#include <SDL3/SDL_gpu.h>
#include "gpu/GPUPlatformConfig.hpp"
#include <cstddef>
#include <string>
#include <unordered_map>

namespace VoidLight {

/**
 * Shader resource information for creation.
 */
struct ShaderInfo {
    uint32_t numSamplers{0};
    uint32_t numStorageTextures{0};
    uint32_t numStorageBuffers{0};
    uint32_t numUniformBuffers{0};
};

struct ShaderCacheKey {
    std::string basePath;
    SDL_GPUShaderStage stage{SDL_GPU_SHADERSTAGE_VERTEX};
    ShaderInfo info{};

    bool operator==(const ShaderCacheKey& other) const noexcept {
        return basePath == other.basePath &&
            stage == other.stage &&
            info.numSamplers == other.info.numSamplers &&
            info.numStorageTextures == other.info.numStorageTextures &&
            info.numStorageBuffers == other.info.numStorageBuffers &&
            info.numUniformBuffers == other.info.numUniformBuffers;
    }
};

struct ShaderCacheKeyHash {
    size_t operator()(const ShaderCacheKey& key) const noexcept;
};

/**
 * Singleton manager for GPU shaders.
 *
 * Handles loading of platform-native shader binaries
 * based on the current GPU backend.
 */
class GPUShaderManager {
public:
    static GPUShaderManager& Instance();

    /**
     * Initialize the shader manager.
     * @param device GPU device
     * @return true on success
     */
    [[nodiscard]] bool init(SDL_GPUDevice* device);

    /**
     * Shutdown and release all shaders.
     */
    void shutdown();

    /**
     * Load a shader from file.
     *
     * Automatically selects the correct format based on GPU backend:
     * - Vulkan: loads .spv file
     * - Metal: loads .metal file
     * - Direct3D 12: loads .dxil file
     *
     * @param basePath Path without extension (e.g., "res/shaders/sprite.vert")
     * @param stage Shader stage (VERTEX or FRAGMENT)
     * @param info Resource binding information
     * @return Loaded shader, or nullptr on failure
     */
    SDL_GPUShader* loadShader(const std::string& basePath,
        SDL_GPUShaderStage stage,
        const ShaderInfo& info);

    /**
     * Get a previously loaded shader by name.
     * @param name Shader name (usually the basePath used during loading)
     * @return Shader pointer, or nullptr if not found
     */
    SDL_GPUShader* getShader(const std::string& name,
        SDL_GPUShaderStage stage,
        const ShaderInfo& info) const;

    /**
     * Check if a shader is already loaded.
     */
    bool hasShader(const std::string& name,
        SDL_GPUShaderStage stage,
        const ShaderInfo& info) const;

private:
    GPUShaderManager() = default;
    ~GPUShaderManager() = default;

    // Non-copyable
    GPUShaderManager(const GPUShaderManager&) = delete;
    GPUShaderManager& operator=(const GPUShaderManager&) = delete;

    /**
     * Load SPIR-V binary shader.
     */
    SDL_GPUShader* loadSPIRV(const std::string& path,
        SDL_GPUShaderStage stage,
        const ShaderInfo& info);

    /**
     * Load MSL (Metal Shading Language) source.
     */
    SDL_GPUShader* loadMSL(const std::string& path,
        SDL_GPUShaderStage stage,
        const ShaderInfo& info,
        const std::string& entryPoint);

    /**
     * Load DXIL binary shader.
     */
    SDL_GPUShader* loadDXIL(const std::string& path,
        SDL_GPUShaderStage stage,
        const ShaderInfo& info);

    std::string resolveShaderPath(const std::string& basePath,
        SDL_GPUShaderStage stage) const;
    ShaderCacheKey makeCacheKey(const std::string& basePath,
        SDL_GPUShaderStage stage,
        const ShaderInfo& info) const;

    SDL_GPUDevice* m_device{nullptr};
    std::unordered_map<ShaderCacheKey, SDL_GPUShader*, ShaderCacheKeyHash> m_shaders;
    GPUPlatformConfig::ShaderBinaryKind m_shaderBinaryKind{
        GPUPlatformConfig::ShaderBinaryKind::SPIRV};
};

} // namespace VoidLight

#endif // GPU_SHADER_MANAGER_HPP
