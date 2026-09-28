/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef GPU_PLATFORM_CONFIG_HPP
#define GPU_PLATFORM_CONFIG_HPP

#include <SDL3/SDL_gpu.h>

namespace VoidLight::GPUPlatformConfig {

enum class ShaderBinaryKind {
    SPIRV,
    MSL,
    DXIL
};

inline constexpr const char* getPreferredDriverName() {
#if defined(_WIN32)
    return "direct3d12";
#elif defined(__APPLE__)
    return "metal";
#else
    return "vulkan";
#endif
}

inline constexpr SDL_GPUShaderFormat getRequestedShaderFormats() {
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_INVALID;

#ifdef HE_GPU_SHADERFORMAT_SPIRV_AVAILABLE
    formats |= SDL_GPU_SHADERFORMAT_SPIRV;
#endif
#ifdef HE_GPU_SHADERFORMAT_MSL_AVAILABLE
    formats |= SDL_GPU_SHADERFORMAT_MSL;
#endif
#ifdef HE_GPU_SHADERFORMAT_DXBC_AVAILABLE
    formats |= SDL_GPU_SHADERFORMAT_DXBC;
#endif
#ifdef HE_GPU_SHADERFORMAT_DXIL_AVAILABLE
    formats |= SDL_GPU_SHADERFORMAT_DXIL;
#endif

    return formats;
}

inline constexpr ShaderBinaryKind getPreferredShaderBinaryKind() {
#if defined(_WIN32)
    return ShaderBinaryKind::DXIL;
#elif defined(__APPLE__)
    return ShaderBinaryKind::MSL;
#else
    return ShaderBinaryKind::SPIRV;
#endif
}

inline constexpr const char* getShaderBinaryExtension() {
    switch (getPreferredShaderBinaryKind()) {
        case ShaderBinaryKind::SPIRV:
            return ".spv";
        case ShaderBinaryKind::MSL:
            return ".metal";
        case ShaderBinaryKind::DXIL:
            return ".dxil";
    }

    return "";
}

} // namespace VoidLight::GPUPlatformConfig

#endif // GPU_PLATFORM_CONFIG_HPP
