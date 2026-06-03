# TextureManager

**Code:** `include/managers/TextureManager.hpp`, `src/managers/TextureManager.cpp`

**Singleton Access:** Use `TextureManager::Instance()` to access the manager.

## Overview

`TextureManager` provides a centralized system for loading and managing GPU textures. It wraps `GPUTexture` objects and handles asynchronous upload via SDL3 GPU copy passes. The manager stores and provides texture data for render controllers and the sprite batch — it does not draw anything directly.

## Key Features

- **GPU Texture Loading**: Load PNG files as SDL3 GPU textures
- **Deferred Upload**: Textures are queued as `PendingTextureUpload` and flushed during the copy pass each frame
- **Texture Cache**: Keyed by string ID; textures are reused across frames
- **Thread-Safe Access**: `getGPUTextureData()` is guarded by a mutex

## API Reference

```cpp
// Load a texture from file — queues an upload, not immediately available
bool loadGPU(const std::string& fileName, const std::string& textureID);

// Flush pending uploads into the copy pass (call once per frame during beginFrame)
void processPendingUploads(SDL_GPUCopyPass* copyPass);

// Get GPU texture data for use in sprite batch / render controllers
std::optional<GPUTextureData> getGPUTextureData(const std::string& textureID) const;

bool hasGPUTextures() const;
bool hasPendingUploads() const;

void clean();
bool isShutdown() const;
```

### GPUTextureData struct

```cpp
struct GPUTextureData {
    std::shared_ptr<VoidLight::GPUTexture> texture;
    float width{0.0f};
    float height{0.0f};
};
```

## Usage Pattern

```cpp
// Loading phase (LoadingState / init)
TextureManager::Instance().loadGPU("res/img/player.png", "player");

// Each frame — in GameEngine beginFrame copy pass
TextureManager::Instance().processPendingUploads(copyPass);

// Render phase — retrieve texture for SpriteBatch
auto data = TextureManager::Instance().getGPUTextureData("player");
if (data) {
    spriteBatch.draw(data->texture->getGPUTexture(), srcRect, dstRect);
}
```

## Related Documentation

- **[GPURendering](../gpu/GPURendering.md)** — Frame structure and copy pass lifecycle
- **[SpriteBatch](../gpu/GPURendering.md)** — Batched sprite drawing
