/* Copyright (c) 2025 Hammer Forged Games
 * Licensed under the MIT License */

#include "gpu/GPUDevice.hpp"
#include "gpu/GPURenderer.hpp"
#include "gpu/GPUTypes.hpp"
#include "managers/TextureManager.hpp"
#include "utils/ResourcePath.hpp"
#include "utils/FrameProfiler.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class BenchmarkMode : uint8_t {
    Particle = 0,
    Primitive,
    Sprite,
    UI,
    Mixed
};

const char* toString(BenchmarkMode mode) {
    switch (mode) {
    case BenchmarkMode::Particle: return "particle";
    case BenchmarkMode::Primitive: return "primitive";
    case BenchmarkMode::Sprite: return "sprite";
    case BenchmarkMode::UI: return "ui";
    case BenchmarkMode::Mixed: return "mixed";
    default: return "unknown";
    }
}

BenchmarkMode parseMode(const std::string& value) {
    if (value == "particle") {
        return BenchmarkMode::Particle;
    }
    if (value == "primitive") {
        return BenchmarkMode::Primitive;
    }
    if (value == "sprite") {
        return BenchmarkMode::Sprite;
    }
    if (value == "ui") {
        return BenchmarkMode::UI;
    }
    if (value == "mixed") {
        return BenchmarkMode::Mixed;
    }

    throw std::runtime_error(std::format("Unknown benchmark mode: {}", value));
}

struct BenchmarkConfig {
    uint32_t m_warmupFrames{120};
    uint32_t m_measureFrames{300};
    uint32_t m_quadCount{2000};
    BenchmarkMode m_mode{BenchmarkMode::Particle};

    static BenchmarkConfig parseArgs(int argc, char* argv[]) {
        BenchmarkConfig config;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];

            if (arg == "--warmup" && i + 1 < argc) {
                config.m_warmupFrames = static_cast<uint32_t>(std::stoul(argv[++i]));
            } else if (arg == "--frames" && i + 1 < argc) {
                config.m_measureFrames = static_cast<uint32_t>(std::stoul(argv[++i]));
            } else if (arg == "--quads" && i + 1 < argc) {
                config.m_quadCount = static_cast<uint32_t>(std::stoul(argv[++i]));
            } else if (arg == "--mode" && i + 1 < argc) {
                config.m_mode = parseMode(argv[++i]);
            } else if (arg == "--help") {
                std::cout << "GPUFrameTimingBenchmark\n\n"
                          << "  --mode name  workload mode: particle, primitive, sprite, ui, mixed\n"
                          << "  --warmup N   warmup frames before sampling (default: 120)\n"
                          << "  --frames N   measured frames (default: 300)\n"
                          << "  --quads N    quads per active workload (default: 2000)\n";
                std::exit(0);
            }
        }

        return config;
    }
};

struct SampleSet {
    std::vector<double> m_frameTimes;
    std::vector<double> m_swapchainTimes;
    std::vector<double> m_uploadTimes;
    std::vector<double> m_submitTimes;

    void reserve(size_t count) {
        m_frameTimes.reserve(count);
        m_swapchainTimes.reserve(count);
        m_uploadTimes.reserve(count);
        m_submitTimes.reserve(count);
    }

    void add(double frameMs, double swapchainMs, double uploadMs, double submitMs) {
        m_frameTimes.push_back(frameMs);
        m_swapchainTimes.push_back(swapchainMs);
        m_uploadTimes.push_back(uploadMs);
        m_submitTimes.push_back(submitMs);
    }

    static double average(const std::vector<double>& values) {
        if (values.empty()) {
            return 0.0;
        }

        const double sum = std::accumulate(values.begin(), values.end(), 0.0);
        return sum / static_cast<double>(values.size());
    }
};

void writeColorQuads(VoidLight::GPUVertexPool& pool,
                     uint32_t quadCount,
                     float viewportWidth,
                     float viewportHeight,
                     float quadSize,
                     uint32_t colorSeed) {
    auto* vertices = static_cast<VoidLight::ColorVertex*>(pool.getMappedPtr());
    if (!vertices) {
        pool.setWrittenVertexCount(0);
        return;
    }

    const uint32_t maxQuads = static_cast<uint32_t>(pool.getMaxVertices() / 6);
    quadCount = std::min(quadCount, maxQuads);

    const float columns = 40.0f;
    const float spacing = quadSize + 4.0f;

    uint32_t offset = 0;
    for (uint32_t i = 0; i < quadCount; ++i) {
        const float x = 8.0f + static_cast<float>(i % static_cast<uint32_t>(columns)) * spacing;
        const float y = 8.0f + static_cast<float>(i / static_cast<uint32_t>(columns)) * spacing;
        if (y + quadSize > viewportHeight || x + quadSize > viewportWidth) {
            break;
        }

        const uint8_t r = static_cast<uint8_t>(64 + ((i + colorSeed) * 13) % 191);
        const uint8_t g = static_cast<uint8_t>(64 + ((i + colorSeed) * 29) % 191);
        const uint8_t b = static_cast<uint8_t>(64 + ((i + colorSeed) * 53) % 191);

        vertices[offset + 0] = {x,            y,            r, g, b, 255};
        vertices[offset + 1] = {x + quadSize, y,            r, g, b, 255};
        vertices[offset + 2] = {x + quadSize, y + quadSize, r, g, b, 255};
        vertices[offset + 3] = {x + quadSize, y + quadSize, r, g, b, 255};
        vertices[offset + 4] = {x,            y + quadSize, r, g, b, 255};
        vertices[offset + 5] = {x,            y,            r, g, b, 255};
        offset += 6;
    }

    pool.setWrittenVertexCount(offset);
}

void renderColorQuads(VoidLight::GPURenderer& renderer,
                      VoidLight::GPUVertexPool& pool,
                      SDL_GPUGraphicsPipeline* pipeline,
                      SDL_GPURenderPass* pass,
                      float width,
                      float height) {
    if (!pass || !pipeline) {
        return;
    }

    const size_t vertexCount = pool.getPendingVertexCount();
    if (vertexCount == 0) {
        return;
    }

    float orthoMatrix[16];
    VoidLight::GPURenderer::createOrthoMatrix(0.0f, width, height, 0.0f, orthoMatrix);

    renderer.pushViewProjection(pass, orthoMatrix);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);

    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = pool.getGPUBuffer();
    vertexBinding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);

    SDL_DrawGPUPrimitives(pass, static_cast<uint32_t>(vertexCount), 1, 0, 0);
}

void writeSpriteQuads(VoidLight::SpriteBatch& batch,
                      VoidLight::GPUVertexPool& pool,
                      const GPUTextureData& atlasData,
                      SDL_GPUSampler* sampler,
                      uint32_t quadCount,
                      float width,
                      float height,
                      float quadSize) {
    auto* vertices = static_cast<VoidLight::SpriteVertex*>(pool.getMappedPtr());
    if (!vertices) {
        pool.setWrittenVertexCount(0);
        return;
    }

    batch.begin(vertices, pool.getMaxVertices(), atlasData.texture->get(), sampler,
                atlasData.width, atlasData.height, height);

    const float columns = 40.0f;
    const float spacing = quadSize + 4.0f;

    for (uint32_t i = 0; i < quadCount; ++i) {
        const float x = 8.0f + static_cast<float>(i % static_cast<uint32_t>(columns)) * spacing;
        const float y = 8.0f + static_cast<float>(i / static_cast<uint32_t>(columns)) * spacing;
        if (y + quadSize > height || x + quadSize > width) {
            break;
        }

        batch.draw(0.0f, 0.0f, 16.0f, 16.0f, x, y, quadSize, quadSize);
    }
}

void renderSpriteBatch(VoidLight::GPURenderer& renderer,
                       VoidLight::SpriteBatch& batch,
                       VoidLight::GPUVertexPool& pool,
                       SDL_GPURenderPass* pass,
                       SDL_GPUGraphicsPipeline* pipeline,
                       float width,
                       float height) {
    if (!pass) {
        return;
    }

    float orthoMatrix[16];
    VoidLight::GPURenderer::createOrthoMatrix(0.0f, width, height, 0.0f, orthoMatrix);
    renderer.pushViewProjection(pass, orthoMatrix);
    batch.render(pass, pipeline, pool.getGPUBuffer());
}

void writeUISprites(VoidLight::GPUVertexPool& pool,
                    uint32_t quadCount,
                    float width,
                    float height) {
    auto* vertices = static_cast<VoidLight::SpriteVertex*>(pool.getMappedPtr());
    if (!vertices) {
        pool.setWrittenVertexCount(0);
        return;
    }

    const uint32_t maxQuads = static_cast<uint32_t>(pool.getMaxVertices() / 6);
    quadCount = std::min(quadCount, maxQuads);
    const float columns = 30.0f;
    const float quadSize = 18.0f;
    const float spacing = quadSize + 8.0f;

    uint32_t offset = 0;
    for (uint32_t i = 0; i < quadCount; ++i) {
        const float x = 16.0f + static_cast<float>(i % static_cast<uint32_t>(columns)) * spacing;
        const float y = 16.0f + static_cast<float>(i / static_cast<uint32_t>(columns)) * spacing;
        if (y + quadSize > height || x + quadSize > width) {
            break;
        }

        const float top = height - y;
        const float bottom = top - quadSize;
        const uint8_t a = static_cast<uint8_t>(128 + (i % 128));
        vertices[offset + 0] = {x,            top,    0.0f, 0.0f, 255, 255, 255, a};
        vertices[offset + 1] = {x + quadSize, top,    1.0f, 0.0f, 255, 255, 255, a};
        vertices[offset + 2] = {x + quadSize, bottom, 1.0f, 1.0f, 255, 255, 255, a};
        vertices[offset + 3] = {x + quadSize, bottom, 1.0f, 1.0f, 255, 255, 255, a};
        vertices[offset + 4] = {x,            bottom, 0.0f, 1.0f, 255, 255, 255, a};
        vertices[offset + 5] = {x,            top,    0.0f, 0.0f, 255, 255, 255, a};
        offset += 6;
    }

    pool.setWrittenVertexCount(offset);
}

void renderUISprites(VoidLight::GPURenderer& renderer,
                     const GPUTextureData& atlasData,
                     SDL_GPURenderPass* pass) {
    if (!pass) {
        return;
    }

    auto& pool = renderer.getUIVertexPool();
    const size_t vertexCount = pool.getPendingVertexCount();
    if (vertexCount == 0) {
        return;
    }

    const std::array imageBatches{
        VoidLight::UITextureDrawBatch{
            .texture = atlasData.texture->get(),
            .vertexOffset = 0,
            .vertexCount = static_cast<uint32_t>(vertexCount)
        }
    };
    renderer.renderUIBatches(pass, 0, imageBatches, {});
}

} // namespace

int main(int argc, char* argv[]) {
    const BenchmarkConfig config = BenchmarkConfig::parseArgs(argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << std::format("SDL_Init failed: {}\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "GPU Frame Timing Benchmark",
        1280, 720,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << std::format("SDL_CreateWindow failed: {}\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    auto& gpuDevice = VoidLight::GPUDevice::Instance();
    auto& renderer = VoidLight::GPURenderer::Instance();
    auto& textureMgr = TextureManager::Instance();
    auto& profiler = VoidLight::FrameProfiler::Instance();
    profiler.suppressFrames(config.m_warmupFrames + config.m_measureFrames + 8);

    if (!gpuDevice.init(window)) {
        std::cerr << "GPUDevice::init failed\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!renderer.init()) {
        std::cerr << "GPURenderer::init failed\n";
        gpuDevice.shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (config.m_mode == BenchmarkMode::Sprite ||
        config.m_mode == BenchmarkMode::UI ||
        config.m_mode == BenchmarkMode::Mixed) {
        const std::string atlasPath = VoidLight::ResourcePath::resolve("res/img/atlas.png");
        if (!textureMgr.loadGPU(atlasPath, "atlas")) {
            std::cerr << std::format("TextureManager::loadGPU failed for {}\n", atlasPath);
            renderer.shutdown();
            gpuDevice.shutdown();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }

    SampleSet samples;
    samples.reserve(config.m_measureFrames);

    const uint32_t totalFrames = config.m_warmupFrames + config.m_measureFrames;
    for (uint32_t frame = 0; frame < totalFrames; ++frame) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                frame = totalFrames;
                break;
            }
        }

        profiler.beginFrame();
        const auto frameStart = std::chrono::steady_clock::now();

        if (!renderer.beginFrame()) {
            profiler.endFrame();
            continue;
        }

        const float sceneWidth = static_cast<float>(renderer.getViewportWidth());
        const float sceneHeight = static_cast<float>(renderer.getViewportHeight());

        if (config.m_mode == BenchmarkMode::Particle || config.m_mode == BenchmarkMode::Mixed) {
            const uint32_t count = (config.m_mode == BenchmarkMode::Mixed) ? config.m_quadCount / 5 : config.m_quadCount;
            writeColorQuads(renderer.getParticleVertexPool(), count, sceneWidth, sceneHeight, 14.0f, 0);
        } else {
            renderer.getParticleVertexPool().setWrittenVertexCount(0);
        }

        if (config.m_mode == BenchmarkMode::Primitive || config.m_mode == BenchmarkMode::Mixed) {
            const uint32_t count = (config.m_mode == BenchmarkMode::Mixed) ? config.m_quadCount / 5 : config.m_quadCount;
            writeColorQuads(renderer.getPrimitiveVertexPool(), count, sceneWidth, sceneHeight, 18.0f, 17);
        } else {
            renderer.getPrimitiveVertexPool().setWrittenVertexCount(0);
        }

        auto atlasData = textureMgr.getGPUTextureData("atlas");
        if ((config.m_mode == BenchmarkMode::Sprite || config.m_mode == BenchmarkMode::UI || config.m_mode == BenchmarkMode::Mixed) &&
            (!atlasData || !atlasData->texture)) {
            std::cerr << "Atlas GPU texture data unavailable\n";
            renderer.endFrame();
            profiler.endFrame();
            break;
        }

        if (config.m_mode == BenchmarkMode::Sprite || config.m_mode == BenchmarkMode::Mixed) {
            const uint32_t count = (config.m_mode == BenchmarkMode::Mixed) ? config.m_quadCount / 5 : config.m_quadCount;
            writeSpriteQuads(renderer.getSpriteBatch(), renderer.getSpriteVertexPool(), *atlasData,
                             renderer.getNearestSampler(), count, sceneWidth, sceneHeight, 16.0f);
            renderer.getSpriteBatch().end();
        }

        if (config.m_mode == BenchmarkMode::Mixed) {
            const uint32_t count = config.m_quadCount / 5;
            writeSpriteQuads(renderer.getEntityBatch(), renderer.getEntityVertexPool(), *atlasData,
                             renderer.getNearestSampler(), count, sceneWidth, sceneHeight, 24.0f);
            renderer.getEntityBatch().end();
        }

        if (config.m_mode == BenchmarkMode::UI || config.m_mode == BenchmarkMode::Mixed) {
            const uint32_t count = (config.m_mode == BenchmarkMode::Mixed) ? config.m_quadCount / 5 : config.m_quadCount;
            writeUISprites(renderer.getUIVertexPool(), count, sceneWidth, sceneHeight);
        } else {
            renderer.getUIVertexPool().setWrittenVertexCount(0);
        }

        SDL_GPURenderPass* scenePass = renderer.beginScenePass();
        renderColorQuads(renderer, renderer.getParticleVertexPool(),
                         renderer.getParticlePipeline(), scenePass, sceneWidth, sceneHeight);
        renderColorQuads(renderer, renderer.getPrimitiveVertexPool(),
                         renderer.getPrimitivePipeline(), scenePass, sceneWidth, sceneHeight);
        if (atlasData && atlasData->texture) {
            renderSpriteBatch(renderer, renderer.getSpriteBatch(), renderer.getSpriteVertexPool(),
                              scenePass, renderer.getSpriteAlphaPipeline(), sceneWidth, sceneHeight);
            renderSpriteBatch(renderer, renderer.getEntityBatch(), renderer.getEntityVertexPool(),
                              scenePass, renderer.getSpriteAlphaPipeline(), sceneWidth, sceneHeight);
        }

        SDL_GPURenderPass* swapchainPass = renderer.beginSwapchainPass();
        if (swapchainPass) {
            renderer.renderComposite(swapchainPass);
            if (atlasData && atlasData->texture) {
                renderUISprites(renderer, *atlasData, swapchainPass);
            }
        }

        renderer.endFrame();

        const auto frameEnd = std::chrono::steady_clock::now();
        profiler.endFrame();

        if (frame >= config.m_warmupFrames) {
            const double frameMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
            samples.add(
                frameMs,
                profiler.getRenderTimeMs(VoidLight::RenderPhase::GPUSwapchainWait),
                profiler.getRenderTimeMs(VoidLight::RenderPhase::GPUUpload),
                profiler.getRenderTimeMs(VoidLight::RenderPhase::GPUSubmit)
            );
        }
    }

    std::cout << std::format("GPU frame benchmark: mode={}, {} warmup, {} measured, {} quads/workload\n",
                             toString(config.m_mode), config.m_warmupFrames,
                             config.m_measureFrames, config.m_quadCount);
    std::cout << std::format("  Avg frame time:   {:.3f} ms\n", SampleSet::average(samples.m_frameTimes));
    std::cout << std::format("  Avg swapchain:    {:.3f} ms\n", SampleSet::average(samples.m_swapchainTimes));
    std::cout << std::format("  Avg GPU upload:   {:.3f} ms\n", SampleSet::average(samples.m_uploadTimes));
    std::cout << std::format("  Avg GPU submit:   {:.3f} ms\n", SampleSet::average(samples.m_submitTimes));

    textureMgr.clean();
    renderer.shutdown();
    gpuDevice.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
