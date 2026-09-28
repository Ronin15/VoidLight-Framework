/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUShaderManager.hpp"
#include "core/Logger.hpp"
#include "utils/ResourcePath.hpp"
#include <format>
#include <fstream>
#include <vector>

namespace VoidLight {

namespace {

bool readShaderFile(const std::string& path, std::vector<uint8_t>& buffer) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        GAMEENGINE_ERROR(std::format("Failed to open shader file: {}", path));
        return false;
    }

    const std::streampos pos = file.tellg();
    if (pos == std::streampos(-1)) {
        GAMEENGINE_ERROR(std::format("Failed to get file size for shader: {}", path));
        return false;
    }

    const std::streamsize size = static_cast<std::streamsize>(pos);
    file.seekg(0, std::ios::beg);
    if (!file) {
        GAMEENGINE_ERROR(std::format("Failed to seek in shader file: {}", path));
        return false;
    }

    buffer.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        GAMEENGINE_ERROR(std::format("Failed to read shader file: {}", path));
        return false;
    }

    return true;
}

} // namespace

size_t ShaderCacheKeyHash::operator()(const ShaderCacheKey& key) const noexcept {
    size_t hash = std::hash<std::string>{}(key.basePath);
    hash ^= static_cast<size_t>(key.stage) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<size_t>(key.info.numSamplers) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<size_t>(key.info.numStorageTextures) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<size_t>(key.info.numStorageBuffers) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<size_t>(key.info.numUniformBuffers) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    return hash;
}

GPUShaderManager& GPUShaderManager::Instance() {
    static GPUShaderManager instance;
    return instance;
}

bool GPUShaderManager::init(SDL_GPUDevice* device) {
    if (!device) {
        GAMEENGINE_ERROR("GPUShaderManager::init: null device");
        return false;
    }

    m_device = device;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        m_shaderBinaryKind = GPUPlatformConfig::ShaderBinaryKind::DXIL;
        GAMEENGINE_INFO("GPUShaderManager: using DXIL shaders");
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        m_shaderBinaryKind = GPUPlatformConfig::ShaderBinaryKind::MSL;
        GAMEENGINE_INFO("GPUShaderManager: using MSL shaders");
    } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        m_shaderBinaryKind = GPUPlatformConfig::ShaderBinaryKind::SPIRV;
        GAMEENGINE_INFO("GPUShaderManager: using SPIR-V shaders");
    } else {
        GAMEENGINE_ERROR("GPUShaderManager: no supported shader format");
        return false;
    }

    return true;
}

void GPUShaderManager::shutdown() {
    size_t count = m_shaders.size();
    for (auto& [name, shader] : m_shaders) {
        if (shader && m_device) {
            SDL_ReleaseGPUShader(m_device, shader);
        }
    }
    m_shaders.clear();
    m_device = nullptr;

    GAMEENGINE_INFO(std::format("GPUShaderManager shutdown: released {} shaders", count));
}

SDL_GPUShader* GPUShaderManager::loadShader(const std::string& basePath,
    SDL_GPUShaderStage stage,
    const ShaderInfo& info) {
    if (!m_device) {
        GAMEENGINE_ERROR("GPUShaderManager::loadShader: not initialized");
        return nullptr;
    }

    const ShaderCacheKey key = makeCacheKey(basePath, stage, info);
    auto it = m_shaders.find(key);
    if (it != m_shaders.end()) {
        return it->second;
    }

    const std::string resolvedPath = resolveShaderPath(basePath, stage);
    SDL_GPUShader* shader = nullptr;

    switch (m_shaderBinaryKind) {
        case GPUPlatformConfig::ShaderBinaryKind::SPIRV:
            shader = loadSPIRV(resolvedPath, stage, info);
            break;
        case GPUPlatformConfig::ShaderBinaryKind::MSL: {
            const std::string entryPoint =
                (stage == SDL_GPU_SHADERSTAGE_VERTEX) ? "vertexMain" : "fragmentMain";
            shader = loadMSL(resolvedPath, stage, info, entryPoint);
            break;
        }
        case GPUPlatformConfig::ShaderBinaryKind::DXIL:
            shader = loadDXIL(resolvedPath, stage, info);
            break;
    }

    if (shader) {
        m_shaders.emplace(key, shader);
        GAMEENGINE_DEBUG(std::format("Loaded shader: {}", resolvedPath));
    }

    return shader;
}

SDL_GPUShader* GPUShaderManager::getShader(const std::string& name,
    SDL_GPUShaderStage stage,
    const ShaderInfo& info) const {
    auto it = m_shaders.find(makeCacheKey(name, stage, info));
    return (it != m_shaders.end()) ? it->second : nullptr;
}

bool GPUShaderManager::hasShader(const std::string& name,
    SDL_GPUShaderStage stage,
    const ShaderInfo& info) const {
    return m_shaders.find(makeCacheKey(name, stage, info)) != m_shaders.end();
}

SDL_GPUShader* GPUShaderManager::loadSPIRV(const std::string& path,
    SDL_GPUShaderStage stage,
    const ShaderInfo& info) {
    std::vector<uint8_t> buffer;
    if (!readShaderFile(path, buffer)) {
        return nullptr;
    }

    SDL_GPUShaderCreateInfo createInfo{};
    createInfo.code = buffer.data();
    createInfo.code_size = buffer.size();
    createInfo.entrypoint = "main";
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.stage = stage;
    createInfo.num_samplers = info.numSamplers;
    createInfo.num_storage_textures = info.numStorageTextures;
    createInfo.num_storage_buffers = info.numStorageBuffers;
    createInfo.num_uniform_buffers = info.numUniformBuffers;

    SDL_GPUShader* shader = SDL_CreateGPUShader(m_device, &createInfo);

    if (!shader) {
        GAMEENGINE_ERROR(std::format("Failed to create SPIR-V shader {}: {}", path, SDL_GetError()));
    }

    return shader;
}

SDL_GPUShader* GPUShaderManager::loadMSL(const std::string& path,
    SDL_GPUShaderStage stage,
    const ShaderInfo& info,
    const std::string& entryPoint) {
    std::vector<uint8_t> buffer;
    if (!readShaderFile(path, buffer)) {
        return nullptr;
    }

    SDL_GPUShaderCreateInfo createInfo{};
    createInfo.code = buffer.data();
    createInfo.code_size = buffer.size();
    createInfo.entrypoint = entryPoint.c_str();
    createInfo.format = SDL_GPU_SHADERFORMAT_MSL;
    createInfo.stage = stage;
    createInfo.num_samplers = info.numSamplers;
    createInfo.num_storage_textures = info.numStorageTextures;
    createInfo.num_storage_buffers = info.numStorageBuffers;
    createInfo.num_uniform_buffers = info.numUniformBuffers;

    SDL_GPUShader* shader = SDL_CreateGPUShader(m_device, &createInfo);

    if (!shader) {
        GAMEENGINE_ERROR(std::format("Failed to create MSL shader {}: {}", path, SDL_GetError()));
    }

    return shader;
}

SDL_GPUShader* GPUShaderManager::loadDXIL(const std::string& path,
    SDL_GPUShaderStage stage,
    const ShaderInfo& info) {
    std::vector<uint8_t> buffer;
    if (!readShaderFile(path, buffer)) {
        return nullptr;
    }

    SDL_GPUShaderCreateInfo createInfo{};
    createInfo.code = buffer.data();
    createInfo.code_size = buffer.size();
    createInfo.entrypoint = "main";
    createInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
    createInfo.stage = stage;
    createInfo.num_samplers = info.numSamplers;
    createInfo.num_storage_textures = info.numStorageTextures;
    createInfo.num_storage_buffers = info.numStorageBuffers;
    createInfo.num_uniform_buffers = info.numUniformBuffers;

    SDL_GPUShader* shader = SDL_CreateGPUShader(m_device, &createInfo);

    if (!shader) {
        GAMEENGINE_ERROR(std::format("Failed to create DXIL shader {}: {}", path, SDL_GetError()));
    }

    return shader;
}

std::string GPUShaderManager::resolveShaderPath(const std::string& basePath,
    SDL_GPUShaderStage) const {
    switch (m_shaderBinaryKind) {
        case GPUPlatformConfig::ShaderBinaryKind::SPIRV:
            return ResourcePath::resolve(basePath + ".spv");
        case GPUPlatformConfig::ShaderBinaryKind::MSL:
            return ResourcePath::resolve(basePath + ".metal");
        case GPUPlatformConfig::ShaderBinaryKind::DXIL:
            return ResourcePath::resolve(basePath + ".dxil");
    }

    return ResourcePath::resolve(basePath);
}

ShaderCacheKey GPUShaderManager::makeCacheKey(const std::string& basePath,
    SDL_GPUShaderStage stage,
    const ShaderInfo& info) const {
    return ShaderCacheKey{resolveShaderPath(basePath, stage), stage, info};
}

} // namespace VoidLight
