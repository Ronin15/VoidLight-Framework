/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef WORLD_MANAGER_HPP
#define WORLD_MANAGER_HPP

#include "world/WorldData.hpp"
#include "world/WorldGenerator.hpp"
#include "managers/GameTimeManager.hpp"
#include "utils/Vector2D.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <string>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include "managers/EventManager.hpp"

namespace VoidLight {
class Camera;  // Forward declaration for camera pointer storage

class GPURenderer;  // Forward declaration for GPU rendering
class GPUTexture;   // Forward declaration for GPU texture
class SpriteBatch;  // Forward declaration for sprite batch

// World object definition loaded from JSON
struct WorldObjectDef {
    std::string id;
    std::string name;
    std::string textureId;
    bool seasonal{false};
    bool blocking{false};
    bool harvestable{false};
    int buildingSize{0};  // For buildings: 1=hut, 2=house, 3=large, 4=cityhall
};

// World objects data loaded from world_objects.json
struct WorldObjectsData {
    std::string version;
    std::unordered_map<std::string, WorldObjectDef> biomes;
    std::unordered_map<std::string, WorldObjectDef> obstacles;
    std::unordered_map<std::string, WorldObjectDef> decorations;
    std::unordered_map<std::string, WorldObjectDef> buildings;
    bool loaded{false};
};

class TileRenderer {
private:
    static constexpr float TILE_SIZE = 32.0f;  // Use float for smooth movement
    static constexpr int VIEWPORT_PADDING = 2;
    static constexpr int SPRITE_OVERHANG = 64;  // Padding for sprites extending beyond tile bounds (2 tiles)

    // Chunk-based rendering - smaller chunks = faster per-chunk render, more chunks total
    static constexpr int CHUNK_SIZE = 16;  // 16x16 tiles per chunk (256 tiles vs 1024)

public:
    TileRenderer();
    ~TileRenderer();

    // Disable copy/move for event handler safety
    TileRenderer(const TileRenderer&) = delete;
    TileRenderer& operator=(const TileRenderer&) = delete;

    void invalidateChunk(int chunkX, int chunkY);
    void clearChunkCache();

    /**
     * @brief Record visible tile vertices for GPU rendering
     *
     * Records all visible tile sprites to the sprite batch using atlas coordinates.
     * Assumes batch recording is already begin()-ed by GPUSceneRecorder.
     * No chunk textures needed - renders directly from tile data each frame.
     *
     * @param spriteBatch Sprite batch to draw to (already begin()-ed)
     * @param cameraX Camera X offset (floored for pixel-perfect alignment)
     * @param cameraY Camera Y offset (floored for pixel-perfect alignment)
     * @param viewportWidth Viewport width at 1x scale
     * @param viewportHeight Viewport height at 1x scale
     * @param zoom Current zoom level
     * @param season Current season for seasonal textures
     */
    void recordGPUTiles(SpriteBatch& spriteBatch, float cameraX, float cameraY,
                        float viewportWidth, float viewportHeight, float zoom,
                        Season season);

    /**
     * @brief Get the atlas GPU texture
     * @return Pointer to GPU texture, or nullptr if not using atlas
     */
    GPUTexture* getAtlasGPUTexture() const;

    // Season management
    void subscribeToSeasonEvents();
    void unsubscribeFromSeasonEvents();
    [[nodiscard]] bool isSubscribedToSeasons() const { return m_subscribedToSeasons; }
    Season getCurrentSeason() const { return m_currentSeason; }
    void setCurrentSeason(Season season);

    // World objects data access
    const WorldObjectsData& getWorldObjectsData() const { return m_worldObjects; }

private:
    // Load world object definitions from JSON
    void loadWorldObjects();
    void updateCachedTextureIDs();

    void onSeasonChange(const EventData& data);

    // Season tracking
    Season m_currentSeason{Season::Spring};
    EventManager::HandlerToken m_seasonToken{};
    bool m_subscribedToSeasons{false};

    // World object definitions loaded from JSON
    WorldObjectsData m_worldObjects;

    // Atlas-based rendering (single texture, source rects from JSON)
    // Pre-loaded seasonal coords - eliminates runtime lookups on season change
    struct AtlasCoords {
        float x{0}, y{0}, w{0}, h{0};
    };

    // All tile type coords per season (indexed by Season enum)
    struct SeasonalTileCoords {
        AtlasCoords biome_default;
        AtlasCoords biome_desert;
        AtlasCoords biome_forest;
        AtlasCoords biome_plains;
        AtlasCoords biome_mountain;
        AtlasCoords biome_swamp;
        AtlasCoords biome_haunted;
        AtlasCoords biome_celestial;
        AtlasCoords biome_ocean;
        AtlasCoords obstacle_water;
        AtlasCoords obstacle_tree;
        AtlasCoords obstacle_rock;
        AtlasCoords building_hut;
        AtlasCoords building_house;
        AtlasCoords building_large;
        AtlasCoords building_cityhall;
        // Ore deposit coords
        AtlasCoords obstacle_iron_deposit;
        AtlasCoords obstacle_gold_deposit;
        AtlasCoords obstacle_copper_deposit;
        AtlasCoords obstacle_mithril_deposit;
        AtlasCoords obstacle_limestone_deposit;
        AtlasCoords obstacle_coal_deposit;
        // Gem deposit coords
        AtlasCoords obstacle_emerald_deposit;
        AtlasCoords obstacle_ruby_deposit;
        AtlasCoords obstacle_sapphire_deposit;
        AtlasCoords obstacle_diamond_deposit;
        AtlasCoords decoration_flower_blue;
        AtlasCoords decoration_flower_pink;
        AtlasCoords decoration_flower_white;
        AtlasCoords decoration_flower_yellow;
        AtlasCoords decoration_mushroom_purple;
        AtlasCoords decoration_mushroom_tan;
        AtlasCoords decoration_grass_small;
        AtlasCoords decoration_grass_large;
        AtlasCoords decoration_bush;
        AtlasCoords decoration_stump_small;
        AtlasCoords decoration_stump_medium;
        AtlasCoords decoration_rock_small;
        AtlasCoords decoration_dead_log_hz;
        AtlasCoords decoration_dead_log_vertical;
        AtlasCoords decoration_lily_pad;
        AtlasCoords decoration_water_flower;
    };

    SeasonalTileCoords m_seasonalCoords[4];  // Indexed by Season enum (Spring=0, Summer=1, Fall=2, Winter=3)
    bool m_useAtlas{false};                  // True if atlas loaded successfully

    std::shared_ptr<GPUTexture> m_atlasGPUOwner{};
    GPUTexture* m_atlasGPUPtr{nullptr};  // GPU atlas texture pointer

    // GPU rendering buffers (member variables to avoid static in threaded code)
    struct GPUSprite {
        float screenX, screenY;         // Destination position
        float srcX, srcY, srcW, srcH;   // Atlas source rect
        float dstW, dstH;               // Destination dimensions
    };
    struct GPUYSortedSprite : GPUSprite {
        float sortY;                    // Y value for sorting (bottom of sprite)
    };
    std::vector<GPUSprite> m_gpuDecoBuffer;
    std::vector<GPUYSortedSprite> m_gpuObstacleBuffer;

    // Debug tracking for GPU viewport changes (avoids static variables)
    float m_lastGPUViewportW{0.0f};
    float m_lastGPUViewportH{0.0f};

    void initAtlasCoords();
};

}

class WorldManager {
public:
    static WorldManager& Instance() {
        static WorldManager instance;
        return instance;
    }

    bool init();
    void clean();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }
    bool isShutdown() const { return m_isShutdown; }
    void prepareForStateTransition();
    void setupEventHandlers();

    bool loadNewWorld(const VoidLight::WorldGenerationConfig& config,
                     const VoidLight::WorldGenerationProgressCallback& progressCallback = nullptr);
    bool loadWorld(const std::string& worldId);
    void unloadWorld();

    std::optional<VoidLight::Tile> getTileCopyAt(int x, int y) const;
    std::optional<VoidLight::Biome> getTileBiomeAt(int x, int y) const;
    std::optional<VoidLight::ObstacleType> getTileObstacleTypeAt(int x, int y) const;

    bool isValidPosition(int x, int y) const;
    std::string getCurrentWorldId() const;
    bool hasActiveWorld() const;

    void update();

    /**
     * @brief Set active camera for chunk visibility (called by states with world rendering)
     */
    // Non-owning observer used for chunk prefetching; callers must clear this before destroying the camera.
    void setActiveCamera(VoidLight::Camera* camera) { mp_activeCamera = camera; }

    /**
     * @brief Record world tile vertices for GPU rendering
     *
     * Records all visible tile sprites to the sprite batch.
     * Uses the existing atlas texture coordinates for each tile type.
     * Batch lifecycle is managed by caller (GPUSceneRecorder) - this just draws.
     *
     * @param spriteBatch Sprite batch to draw to (already begin()-ed)
     * @param cameraX Camera X offset (floored for pixel-perfect alignment)
     * @param cameraY Camera Y offset (floored for pixel-perfect alignment)
     * @param viewWidth Viewport width at 1x scale
     * @param viewHeight Viewport height at 1x scale
     * @param zoom Current zoom level
     */
    void recordGPU(VoidLight::SpriteBatch& spriteBatch, float cameraX, float cameraY,
                   float viewWidth, float viewHeight, float zoom);

    bool handleHarvestResource(int entityId, int targetX, int targetY);
    bool modifyTile(int x, int y, const std::function<void(VoidLight::Tile&)>& mutator);
    bool updateTile(int x, int y, const VoidLight::Tile& newTile);

    void enableRendering(bool enable) { m_renderingEnabled = enable; }
    bool isRenderingEnabled() const { return m_renderingEnabled; }

    // Chunk cache management (delegates to TileRenderer)
    void invalidateChunk(int chunkX, int chunkY);
    void clearChunkCache();

    // Season management (delegates to TileRenderer)
    void subscribeToSeasonEvents();
    void unsubscribeFromSeasonEvents();
    Season getCurrentSeason() const;
    void setCurrentSeason(Season season);

    void setCamera(int x, int y) { m_cameraX = x; m_cameraY = y; }
    void setCameraViewport(int width, int height) {
        m_viewportWidth = width;
        m_viewportHeight = height;
    }

    template <typename Func>
    decltype(auto) withWorldDataRead(Func&& func) const {
        using Result = std::invoke_result_t<Func, const VoidLight::WorldData*>;
        static_assert(!std::is_reference_v<Result>,
                      "withWorldDataRead() callbacks must not return references");
        static_assert(!std::is_pointer_v<std::remove_cv_t<Result>>,
                      "withWorldDataRead() callbacks must not return pointers");

        std::shared_lock<std::shared_mutex> lock(m_worldMutex);
        if constexpr (std::is_void_v<Result>) {
            std::forward<Func>(func)(m_currentWorld.get());
        } else {
            return std::forward<Func>(func)(m_currentWorld.get());
        }
    }

    /**
     * @brief Gets the current world version for change detection
     * @return Current world version (increments on tile changes)
     */
    uint64_t getWorldVersion() const { return m_worldVersion.load(std::memory_order_acquire); }

    /**
     * @brief Gets the world dimensions in tiles
     * @param width Output world width
     * @param height Output world height
     * @return True if world is loaded and dimensions are valid
     */
    bool getWorldDimensions(int& width, int& height) const;

    /**
     * @brief Gets world bounds in world coordinates
     * @param minX Output minimum X coordinate
     * @param minY Output minimum Y coordinate
     * @param maxX Output maximum X coordinate
     * @param maxY Output maximum Y coordinate
     * @return True if world is loaded and bounds are valid
     */
    bool getWorldBounds(float& minX, float& minY, float& maxX, float& maxY) const;

private:
    WorldManager() = default;
    ~WorldManager() {
        if (!m_isShutdown) {
            clean();
        }
    }
    WorldManager(const WorldManager&) = delete;
    WorldManager& operator=(const WorldManager&) = delete;

    void fireTileChangedEvent(int x, int y, const VoidLight::Tile& tile);
    void fireWorldLoadedEvent(const std::string& worldId);
    void fireWorldUnloadedEvent(const std::string& worldId);
    void initializeWorldResources();
    std::optional<std::string> unloadWorldLocked();  // Assumes caller already holds lock
    bool applyTileUpdateLocked(int x, int y, const VoidLight::Tile& newTile);

    std::unique_ptr<VoidLight::WorldData> m_currentWorld;
    std::unique_ptr<VoidLight::TileRenderer> m_tileRenderer;

    mutable std::mutex m_loadMutex;
    mutable std::shared_mutex m_worldMutex;
    std::atomic<bool> m_initialized{false};
    bool m_isShutdown{false};

    // World version tracking for change detection by other systems
    std::atomic<uint64_t> m_worldVersion{0};

    bool m_renderingEnabled{true};
    int m_cameraX{0};
    int m_cameraY{0};
    int m_viewportWidth{80};
    int m_viewportHeight{25};

    VoidLight::Camera* mp_activeCamera{nullptr};
};

#endif // WORLD_MANAGER_HPP
