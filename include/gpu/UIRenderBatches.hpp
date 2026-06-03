/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#ifndef UI_RENDER_BATCHES_HPP
#define UI_RENDER_BATCHES_HPP

#include <SDL3/SDL_gpu.h>
#include <cstdint>

namespace VoidLight {

enum class UITextPipelineKind : uint8_t {
    Alpha,
    SDF,
    Color
};

struct UITextureDrawBatch {
    SDL_GPUTexture* texture{nullptr};
    uint32_t vertexOffset{0};
    uint32_t vertexCount{0};
};

struct UITextDrawBatch {
    SDL_GPUTexture* texture{nullptr};
    UITextPipelineKind pipeline{UITextPipelineKind::Alpha};
    uint32_t vertexOffset{0};
    uint32_t vertexCount{0};
};

} // namespace VoidLight

#endif // UI_RENDER_BATCHES_HPP
