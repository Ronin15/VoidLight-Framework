/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef WORLD_GENERATOR_HPP
#define WORLD_GENERATOR_HPP

#include <memory>
#include <vector>
#include <random>
#include <functional>
#include <string>

namespace VoidLight {

struct WorldData;
struct WorldGenerationConfig;

// Progress callback type: void callback(float percentComplete, const std::string& statusMessage)
using WorldGenerationProgressCallback = std::function<void(float, const std::string&)>;

class WorldGenerator {
private:
    struct PerlinNoise {
        std::vector<int> permutation;
        
        explicit PerlinNoise(int seed);
        float noise(float x, float y) const;
        float fade(float t) const;
        float lerp(float t, float a, float b) const;
        float grad(int hash, float x, float y) const;
    };

    static std::unique_ptr<WorldData> generateNoiseMaps(
        const WorldGenerationConfig& config,
        std::vector<std::vector<float>>& elevationMap,
        std::vector<std::vector<float>>& humidityMap
    );
    
    static void assignBiomes(
        WorldData& world,
        const std::vector<std::vector<float>>& elevationMap,
        const std::vector<std::vector<float>>& humidityMap,
        const WorldGenerationConfig& config
    );
    
    static void createWaterBodies(
        WorldData& world,
        const std::vector<std::vector<float>>& elevationMap,
        const WorldGenerationConfig& config
    );
    
    static void distributeObstacles(
        WorldData& world,
        const WorldGenerationConfig& config
    );

    static void distributeDecorations(
        WorldData& world,
        const WorldGenerationConfig& config
    );

    static void calculateInitialResources(
        const WorldData& world
    );
    
    // Building generation helpers
    static void generateBuildings(
        WorldData& world,
        std::default_random_engine& rng
    );
    
    static bool canPlaceBuilding(
        const WorldData& world,
        int x, int y
    );
    
    static uint32_t createBuilding(
        WorldData& world,
        int x, int y,
        uint32_t& nextBuildingId
    );
    
    static void tryConnectBuildings(
        WorldData& world,
        int x, int y,
        uint32_t buildingId
    );

public:
    static std::unique_ptr<WorldData> generateWorld(
        const WorldGenerationConfig& config,
        const WorldGenerationProgressCallback& progressCallback = nullptr
    );
};

}

#endif