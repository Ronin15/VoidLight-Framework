/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "world/WorldGenerator.hpp"
#include "world/WorldData.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <format>

namespace VoidLight {

// ============================================================================
// WORLD GENERATION SPAWN CONFIGURATION
// ============================================================================
// All spawn rates and parameters in one place for easy tuning.
// Higher values = more frequent spawning (where applicable)

namespace WorldSpawnConfig {

// ----------------------------------------------------------------------------
// BIOME ASSIGNMENT THRESHOLDS
// ----------------------------------------------------------------------------
namespace Biome {
    // Humidity thresholds (0.0 = dry, 1.0 = wet)
    constexpr float DESERT_HUMIDITY_MAX = 0.35f;
    constexpr float SWAMP_HUMIDITY_MIN = 0.70f;
    constexpr float SWAMP_ELEVATION_MAX = 0.45f;

    // PLAINS: moderate humidity, mid elevation (temperate default)
    constexpr float PLAINS_HUMIDITY_MIN = 0.35f;
    constexpr float PLAINS_HUMIDITY_MAX = 0.60f;
    constexpr float PLAINS_ELEVATION_MIN = 0.35f;
    constexpr float PLAINS_ELEVATION_MAX = 0.65f;

    // FOREST: higher humidity with moderate-to-high elevation
    constexpr float FOREST_HUMIDITY_MIN = 0.55f;
    constexpr float FOREST_ELEVATION_MIN = 0.40f;

    // Special biome spawn chances (applied when no other biome matches)
    constexpr float HAUNTED_CHANCE = 0.03f;    // 3% (reduced from 5%)
    constexpr float CELESTIAL_CHANCE = 0.03f;  // 3% (reduced from 5%)
}

// ----------------------------------------------------------------------------
// RIVER/WATER GENERATION
// ----------------------------------------------------------------------------
namespace Water {
    constexpr int RIVER_DENSITY_DIVISOR = 1000;       // Rivers = area / this
    constexpr float RIVER_START_ELEVATION_OFFSET = 0.2f;  // Min elevation above water level to start river
    constexpr int RIVER_MAX_FLOW_STEPS = 50;          // Max river length in tiles
}

// ----------------------------------------------------------------------------
// OBSTACLE SPAWN RATES (Trees, Rocks, Water obstacles)
// ----------------------------------------------------------------------------
namespace Obstacles {
    // Per-biome spawn chances (0.0 - 1.0)
    constexpr float PLAINS_CHANCE = 0.12f;    // Sparse trees in open grassland
    constexpr float FOREST_CHANCE = 0.40f;
    constexpr float MOUNTAIN_CHANCE = 0.30f;
    constexpr float SWAMP_CHANCE = 0.20f;
    constexpr float DESERT_CHANCE = 0.10f;
    constexpr float HAUNTED_CHANCE = 0.30f;
    constexpr float CELESTIAL_CHANCE = 0.15f;

    // Swamp obstacle type distribution
    constexpr float SWAMP_TREE_RATIO = 0.70f;   // 70% trees, 30% water

    // Haunted obstacle type distribution
    constexpr float HAUNTED_TREE_RATIO = 0.60f; // 60% trees, 40% rocks

    // Clustering behavior (prevents dense blobs)
    constexpr int MAX_NEIGHBORS_ALLOWED = 2;     // Block if more than this many neighbors
    constexpr float CLUSTER_GROWTH_FOREST = 0.50f;  // Chance to extend cluster in forest
    constexpr float CLUSTER_GROWTH_OTHER = 0.20f;   // Chance to extend cluster in other biomes
}

// ----------------------------------------------------------------------------
// DEPOSIT SPAWN RATES (Ore and Gem deposits in MOUNTAIN biome)
// ----------------------------------------------------------------------------
namespace Deposits {
    // Base chance for any deposit to spawn when placing a MOUNTAIN obstacle
    constexpr float BASE_CHANCE = 0.08f;  // 8% of MOUNTAIN rocks become deposits

    // Per-resource rarity weights (must sum to 1.0)
    // Common ores (80% of deposits)
    constexpr float IRON_WEIGHT = 0.25f;       // 25% of deposits
    constexpr float COPPER_WEIGHT = 0.20f;     // 20%
    constexpr float COAL_WEIGHT = 0.20f;       // 20%
    constexpr float LIMESTONE_WEIGHT = 0.15f;  // 15%

    // Rare ores (10% of deposits)
    constexpr float GOLD_WEIGHT = 0.08f;       // 8%
    constexpr float MITHRIL_WEIGHT = 0.02f;    // 2% (very rare)

    // Gems (10% of deposits - all very rare)
    constexpr float EMERALD_WEIGHT = 0.03f;    // 3%
    constexpr float RUBY_WEIGHT = 0.025f;      // 2.5%
    constexpr float SAPPHIRE_WEIGHT = 0.025f;  // 2.5%
    constexpr float DIAMOND_WEIGHT = 0.01f;    // 1% (rarest)
}

// ----------------------------------------------------------------------------
// VILLAGE/BUILDING SPAWN CONFIGURATION
// ----------------------------------------------------------------------------
namespace Buildings {
    constexpr int BUILDING_SIZE = 2;           // 2x2 tiles per building
    constexpr int MAX_CONNECTED_SIZE = 4;      // Max connected building size (hut->house->large->cityhall)

    // Village clustering parameters
    constexpr int VILLAGE_DENSITY_DIVISOR = 8000;   // Villages = area / this (e.g., 200x200 = ~5 villages)
    constexpr int VILLAGE_MIN_DISTANCE = 40;        // Minimum tiles between village centers
    constexpr int VILLAGE_RADIUS = 12;              // Max radius for building placement from center
    constexpr int VILLAGE_MIN_BUILDINGS = 3;        // Minimum buildings per village
    constexpr int VILLAGE_MAX_BUILDINGS = 8;        // Maximum buildings per village

    // Per-biome village spawn weight (higher = more likely to have villages)
    constexpr float PLAINS_VILLAGE_WEIGHT = 1.0f;   // Most common
    constexpr float FOREST_VILLAGE_WEIGHT = 0.6f;
    constexpr float DESERT_VILLAGE_WEIGHT = 0.3f;
    constexpr float SWAMP_VILLAGE_WEIGHT = 0.2f;
    constexpr float HAUNTED_VILLAGE_WEIGHT = 0.4f;
    constexpr float CELESTIAL_VILLAGE_WEIGHT = 0.3f;
}

// ----------------------------------------------------------------------------
// DECORATION SPAWN RATES & WEIGHTS
// ----------------------------------------------------------------------------
namespace Decorations {
    // Per-biome spawn chances (0.0 - 1.0)
    constexpr float PLAINS_CHANCE = 0.35f;     // High decoration density (flowers, grass)
    constexpr float FOREST_CHANCE = 0.25f;
    constexpr float CELESTIAL_CHANCE = 0.20f;
    constexpr float SWAMP_CHANCE = 0.30f;
    constexpr float HAUNTED_CHANCE = 0.25f;
    constexpr float MOUNTAIN_CHANCE = 0.15f;
    constexpr float WATER_CHANCE = 0.15f;      // Water decorations (lily pads, water flowers)
    constexpr float DEFAULT_CHANCE = 0.15f;

    // Decoration type weights (relative frequency within biome)
    // Higher weight = more likely to be selected
    constexpr float FLOWER_WEIGHT = 10.0f;
    constexpr float GRASS_WEIGHT = 15.0f;
    constexpr float MUSHROOM_WEIGHT = 8.0f;
    constexpr float BUSH_WEIGHT = 8.0f;
    constexpr float STUMP_WEIGHT = 5.0f;
    constexpr float ROCK_WEIGHT = 5.0f;
    constexpr float DEAD_LOG_WEIGHT = 2.0f;      // Rare - large decorations
    constexpr float LILY_PAD_WEIGHT = 10.0f;
    constexpr float WATER_FLOWER_WEIGHT = 8.0f;
}

} // namespace WorldSpawnConfig

// Weighted decoration entry
struct WeightedDecoration {
    DecorationType type;
    float weight;
};

// Select decoration based on weights
inline DecorationType selectWeightedDecoration(
    const std::vector<WeightedDecoration>& decorations,
    std::default_random_engine& rng) {

    float totalWeight = std::accumulate(decorations.begin(), decorations.end(), 0.0f,
        [](float sum, const auto& d) { return sum + d.weight; });

    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    float roll = dist(rng);

    float cumulative = 0.0f;
    for (const auto& d : decorations) {
        cumulative += d.weight;
        if (roll < cumulative) {
            return d.type;
        }
    }
    return decorations.back().type;
}

WorldGenerator::PerlinNoise::PerlinNoise(int seed) {
  permutation.resize(256);
  std::iota(permutation.begin(), permutation.end(), 0);

  std::default_random_engine engine(seed);
  std::shuffle(permutation.begin(), permutation.end(), engine);

  auto copy = permutation;
  permutation.insert(permutation.end(), copy.begin(), copy.end());
}

float WorldGenerator::PerlinNoise::fade(float t) const {
  return t * t * t * (t * (t * 6 - 15) + 10);
}

float WorldGenerator::PerlinNoise::lerp(float t, float a, float b) const {
  return a + t * (b - a);
}

float WorldGenerator::PerlinNoise::grad(int hash, float x, float y) const {
  int h = hash & 15;
  float u = h < 8 ? x : y;
  float v = h < 4 ? y : h == 12 || h == 14 ? x : 0;
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float WorldGenerator::PerlinNoise::noise(float x, float y) const {
  int X = static_cast<int>(std::floor(x)) & 255;
  int Y = static_cast<int>(std::floor(y)) & 255;

  x -= std::floor(x);
  y -= std::floor(y);

  float u = fade(x);
  float v = fade(y);

  int A = permutation[X] + Y;
  int AA = permutation[A];
  int AB = permutation[A + 1];
  int B = permutation[X + 1] + Y;
  int BA = permutation[B];
  int BB = permutation[B + 1];

  return lerp(
      v, lerp(u, grad(permutation[AA], x, y), grad(permutation[BA], x - 1, y)),
      lerp(u, grad(permutation[AB], x, y - 1),
           grad(permutation[BB], x - 1, y - 1)));
}

std::unique_ptr<WorldData>
WorldGenerator::generateWorld(const WorldGenerationConfig &config,
                              const WorldGenerationProgressCallback& progressCallback) {
  WORLD_MANAGER_INFO(std::format("Generating world: {}x{} with seed {}",
                                 config.width, config.height, config.seed));

  // Report initial progress
  if (progressCallback) {
    progressCallback(0.0f, "Initializing world generation...");
  }

  std::vector<std::vector<float>> elevationMap, humidityMap;
  auto world = generateNoiseMaps(config, elevationMap, humidityMap);

  // Progress: Noise maps complete (30%)
  if (progressCallback) {
    progressCallback(30.0f, "Generating terrain...");
  }

  assignBiomes(*world, elevationMap, humidityMap, config);

  // Progress: Biomes assigned (50%)
  if (progressCallback) {
    progressCallback(50.0f, "Creating biomes...");
  }

  createWaterBodies(*world, elevationMap, config);

  // Progress: Water bodies created (70%)
  if (progressCallback) {
    progressCallback(70.0f, "Placing water...");
  }

  distributeObstacles(*world, config);

  // Progress: Obstacles distributed (80%)
  if (progressCallback) {
    progressCallback(80.0f, "Distributing obstacles...");
  }

  distributeDecorations(*world, config);

  // Progress: Decorations distributed (90%)
  if (progressCallback) {
    progressCallback(90.0f, "Adding decorations...");
  }

  calculateInitialResources(*world);

  // Progress: Complete (100%)
  if (progressCallback) {
    progressCallback(100.0f, "Finalizing world...");
  }

  WORLD_MANAGER_INFO("World generation completed successfully");
  return world;
}

std::unique_ptr<WorldData> WorldGenerator::generateNoiseMaps(
    const WorldGenerationConfig &config,
    std::vector<std::vector<float>> &elevationMap,
    std::vector<std::vector<float>> &humidityMap) {
  auto world = std::make_unique<WorldData>();
  world->worldId = std::format("generated_{}", config.seed);
  world->grid.resize(config.height, std::vector<Tile>(config.width));

  elevationMap.resize(config.height, std::vector<float>(config.width));
  humidityMap.resize(config.height, std::vector<float>(config.width));

  PerlinNoise elevationNoise(config.seed);
  PerlinNoise humidityNoise(config.seed + 1000);

  for (int y = 0; y < config.height; ++y) {
    for (int x = 0; x < config.width; ++x) {
      float elevation = elevationNoise.noise(x * config.elevationFrequency,
                                             y * config.elevationFrequency);
      elevation = (elevation + 1.0f) * 0.5f; // Normalize to [0, 1]

      float humidity = humidityNoise.noise(x * config.humidityFrequency,
                                           y * config.humidityFrequency);
      humidity = (humidity + 1.0f) * 0.5f; // Normalize to [0, 1]

      elevationMap[y][x] = elevation;
      humidityMap[y][x] = humidity;
      world->grid[y][x].elevation = elevation;
    }
  }

  return world;
}

void WorldGenerator::assignBiomes(
    WorldData &world, const std::vector<std::vector<float>> &elevationMap,
    const std::vector<std::vector<float>> &humidityMap,
    const WorldGenerationConfig &config) {
  int height = world.grid.size();
  int width = world.grid[0].size();

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float elevation = elevationMap[y][x];
      float humidity = humidityMap[y][x];

      Biome biome;

      namespace BiomeCfg = WorldSpawnConfig::Biome;

      // Water check first (lowest elevation)
      if (elevation < config.waterLevel) {
        biome = Biome::OCEAN;
        world.grid[y][x].isWater = true;
      }
      // Mountain check (highest elevation)
      else if (elevation >= config.mountainLevel) {
        biome = Biome::MOUNTAIN;
      }
      // Land biomes based on humidity and elevation
      else {
        // DESERT: Low humidity, any non-water elevation
        if (humidity < BiomeCfg::DESERT_HUMIDITY_MAX) {
          biome = Biome::DESERT;
        }
        // SWAMP: High humidity AND low elevation
        else if (humidity > BiomeCfg::SWAMP_HUMIDITY_MIN &&
                 elevation < BiomeCfg::SWAMP_ELEVATION_MAX) {
          biome = Biome::SWAMP;
        }
        // FOREST: High humidity with moderate-to-high elevation
        else if (humidity >= BiomeCfg::FOREST_HUMIDITY_MIN &&
                 elevation >= BiomeCfg::FOREST_ELEVATION_MIN) {
          biome = Biome::FOREST;
        }
        // PLAINS: Moderate humidity, mid elevation (temperate default)
        else if (humidity >= BiomeCfg::PLAINS_HUMIDITY_MIN &&
                 humidity <= BiomeCfg::PLAINS_HUMIDITY_MAX &&
                 elevation >= BiomeCfg::PLAINS_ELEVATION_MIN &&
                 elevation <= BiomeCfg::PLAINS_ELEVATION_MAX) {
          biome = Biome::PLAINS;
        }
        // Remaining land: check for special biomes, else default to PLAINS
        else {
          std::default_random_engine rng(config.seed + x * 1000 + y);
          std::uniform_real_distribution<float> dist(0.0f, 1.0f);

          float special = dist(rng);
          if (special < BiomeCfg::HAUNTED_CHANCE) {
            biome = Biome::HAUNTED;
          } else if (special < BiomeCfg::HAUNTED_CHANCE + BiomeCfg::CELESTIAL_CHANCE) {
            biome = Biome::CELESTIAL;
          } else {
            // Default to PLAINS instead of FOREST for uncategorized mid-terrain
            biome = Biome::PLAINS;
          }
        }
      }

      world.grid[y][x].biome = biome;
    }
  }
}

void WorldGenerator::createWaterBodies(
    WorldData &world, const std::vector<std::vector<float>> &elevationMap,
    const WorldGenerationConfig &config) {
  int height = world.grid.size();
  int width = world.grid[0].size();

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (elevationMap[y][x] < config.waterLevel) {
        world.grid[y][x].isWater = true;
        world.grid[y][x].biome = Biome::OCEAN;
        world.grid[y][x].obstacleType = ObstacleType::NONE;
      }
    }
  }

  // Create rivers by connecting low elevation areas
  namespace WaterCfg = WorldSpawnConfig::Water;

  std::default_random_engine rng(config.seed + 5000);
  std::uniform_int_distribution<int> xDist(0, width - 1);
  std::uniform_int_distribution<int> yDist(0, height - 1);

  int riverCount = std::max(1, (width * height) / WaterCfg::RIVER_DENSITY_DIVISOR);

  for (int i = 0; i < riverCount; ++i) {
    int startX = xDist(rng);
    int startY = yDist(rng);

    if (elevationMap[startY][startX] > config.waterLevel + WaterCfg::RIVER_START_ELEVATION_OFFSET) {
      int currentX = startX;
      int currentY = startY;

      // Flow downhill for up to configured steps
      for (int step = 0; step < WaterCfg::RIVER_MAX_FLOW_STEPS; ++step) {
        float currentElevation = elevationMap[currentY][currentX];

        // Find lowest neighboring tile
        int bestX = currentX;
        int bestY = currentY;
        float lowestElevation = currentElevation;

        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0)
              continue;

            int newX = currentX + dx;
            int newY = currentY + dy;

            if (newX >= 0 && newX < width && newY >= 0 && newY < height) {
              float neighborElevation = elevationMap[newY][newX];
              if (neighborElevation < lowestElevation) {
                lowestElevation = neighborElevation;
                bestX = newX;
                bestY = newY;
              }
            }
          }
        }

        // If we found a lower neighbor, create water and continue
        if (bestX != currentX || bestY != currentY) {
          if (!world.grid[currentY][currentX].isWater) {
            world.grid[currentY][currentX].isWater = true;
            // Preserve original biome so rivers get biome-appropriate decorations
            world.grid[currentY][currentX].obstacleType = ObstacleType::NONE;
          }
          currentX = bestX;
          currentY = bestY;
        } else {
          break; // No lower neighbor found, stop the river
        }
      }
    }
  }
}

void WorldGenerator::distributeObstacles(WorldData &world,
                                         const WorldGenerationConfig &config) {
  namespace ObsCfg = WorldSpawnConfig::Obstacles;
  namespace DepCfg = WorldSpawnConfig::Deposits;

  int height = static_cast<int>(world.grid.size());
  int width = static_cast<int>(world.grid[0].size());

  std::default_random_engine rng(config.seed + 10000);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  // Helper function to select deposit type based on weighted probabilities
  auto selectDepositType = [](float roll) -> ObstacleType {
    // Cumulative probability selection using weights from config
    float cumulative = 0.0f;

    // Common ores
    cumulative += DepCfg::IRON_WEIGHT;
    if (roll < cumulative) return ObstacleType::IRON_DEPOSIT;

    cumulative += DepCfg::COPPER_WEIGHT;
    if (roll < cumulative) return ObstacleType::COPPER_DEPOSIT;

    cumulative += DepCfg::COAL_WEIGHT;
    if (roll < cumulative) return ObstacleType::COAL_DEPOSIT;

    cumulative += DepCfg::LIMESTONE_WEIGHT;
    if (roll < cumulative) return ObstacleType::LIMESTONE_DEPOSIT;

    // Rare ores
    cumulative += DepCfg::GOLD_WEIGHT;
    if (roll < cumulative) return ObstacleType::GOLD_DEPOSIT;

    cumulative += DepCfg::MITHRIL_WEIGHT;
    if (roll < cumulative) return ObstacleType::MITHRIL_DEPOSIT;

    // Gems
    cumulative += DepCfg::EMERALD_WEIGHT;
    if (roll < cumulative) return ObstacleType::EMERALD_DEPOSIT;

    cumulative += DepCfg::RUBY_WEIGHT;
    if (roll < cumulative) return ObstacleType::RUBY_DEPOSIT;

    cumulative += DepCfg::SAPPHIRE_WEIGHT;
    if (roll < cumulative) return ObstacleType::SAPPHIRE_DEPOSIT;

    cumulative += DepCfg::DIAMOND_WEIGHT;
    if (roll < cumulative) return ObstacleType::DIAMOND_DEPOSIT;

    // Fallback (should never reach due to weight distribution)
    return ObstacleType::IRON_DEPOSIT;
  };

  // Count nearby obstacles (for density-aware spacing)
  auto countNearbyObstacles = [&](int cx, int cy) -> int {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        int nx = cx + dx, ny = cy + dy;
        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
          if (world.grid[ny][nx].obstacleType != ObstacleType::NONE) {
            ++count;
          }
        }
      }
    }
    return count;
  };

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      Tile &tile = world.grid[y][x];

      if (tile.isWater) {
        continue; // No obstacles in water
      }

      float obstacleChance = 0.0f;
      ObstacleType obstacleType = ObstacleType::NONE;

      switch (tile.biome) {
      case Biome::PLAINS:
        obstacleChance = ObsCfg::PLAINS_CHANCE;
        obstacleType = ObstacleType::TREE;  // Sparse trees in open grassland
        break;
      case Biome::FOREST:
        obstacleChance = ObsCfg::FOREST_CHANCE;
        obstacleType = ObstacleType::TREE;
        break;
      case Biome::MOUNTAIN:
        obstacleChance = ObsCfg::MOUNTAIN_CHANCE;
        // Check if this should be a deposit instead of a rock
        if (dist(rng) < DepCfg::BASE_CHANCE) {
          obstacleType = selectDepositType(dist(rng));
        } else {
          obstacleType = ObstacleType::ROCK;
        }
        break;
      case Biome::SWAMP:
        obstacleChance = ObsCfg::SWAMP_CHANCE;
        obstacleType =
            dist(rng) < ObsCfg::SWAMP_TREE_RATIO ? ObstacleType::TREE : ObstacleType::WATER;
        break;
      case Biome::DESERT:
        obstacleChance = ObsCfg::DESERT_CHANCE;
        obstacleType = ObstacleType::ROCK;
        break;
      case Biome::HAUNTED:
        obstacleChance = ObsCfg::HAUNTED_CHANCE;
        obstacleType =
            dist(rng) < ObsCfg::HAUNTED_TREE_RATIO ? ObstacleType::TREE : ObstacleType::ROCK;
        break;
      case Biome::CELESTIAL:
        obstacleChance = ObsCfg::CELESTIAL_CHANCE;
        obstacleType = ObstacleType::ROCK;
        break;
      default:
        break;
      }

      // Smart density: organic cluster growth with natural variation
      if (dist(rng) < obstacleChance) {
        int nearbyCount = countNearbyObstacles(x, y);
        bool canPlace = false;

        if (nearbyCount == 0) {
          // No neighbors - always allow (start new cluster or isolated tree)
          canPlace = true;
        } else if (nearbyCount <= ObsCfg::MAX_NEIGHBORS_ALLOWED) {
          // Within limit - chance to extend cluster (organic growth)
          float clusterChance = (tile.biome == Biome::FOREST)
              ? ObsCfg::CLUSTER_GROWTH_FOREST
              : ObsCfg::CLUSTER_GROWTH_OTHER;
          canPlace = dist(rng) < clusterChance;
        }
        // Too many neighbors - skip (prevents blob formations)

        if (canPlace) {
          tile.obstacleType = obstacleType;
        }
      }
    }
  }

  // Second pass: Generate multi-tile buildings with connection logic
  generateBuildings(world, rng);
}

void WorldGenerator::distributeDecorations(WorldData& world,
                                           const WorldGenerationConfig& config) {
  namespace DecoCfg = WorldSpawnConfig::Decorations;

  int height = static_cast<int>(world.grid.size());
  int width = static_cast<int>(world.grid[0].size());

  std::default_random_engine rng(config.seed + 20000);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  // Pre-allocated vector for weighted decorations
  std::vector<WeightedDecoration> weightedDecorations;
  weightedDecorations.reserve(16);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      Tile& tile = world.grid[y][x];

      // Skip tiles with buildings
      if (tile.buildingId > 0) {
        continue;
      }

      float decorationChance = 0.0f;
      weightedDecorations.clear();

      // Water tiles get water-specific decorations
      // Check both isWater flag AND water obstacles in swamp (puddles)
      bool isWaterTile = tile.isWater || tile.obstacleType == ObstacleType::WATER;
      if (isWaterTile && (tile.biome == Biome::SWAMP || tile.biome == Biome::FOREST)) {
        decorationChance = DecoCfg::WATER_CHANCE;
        weightedDecorations = {
            {DecorationType::LILY_PAD, DecoCfg::LILY_PAD_WEIGHT},
            {DecorationType::WATER_FLOWER, DecoCfg::WATER_FLOWER_WEIGHT}
        };
      } else if (tile.obstacleType != ObstacleType::NONE) {
        // Skip land tiles with obstacles (trees, rocks)
        continue;
      } else {
        // Land decorations by biome
        switch (tile.biome) {
          case Biome::PLAINS:
            decorationChance = DecoCfg::PLAINS_CHANCE;
            weightedDecorations = {
                {DecorationType::FLOWER_BLUE, DecoCfg::FLOWER_WEIGHT * 1.5f},
                {DecorationType::FLOWER_PINK, DecoCfg::FLOWER_WEIGHT * 1.5f},
                {DecorationType::FLOWER_WHITE, DecoCfg::FLOWER_WEIGHT * 1.5f},
                {DecorationType::FLOWER_YELLOW, DecoCfg::FLOWER_WEIGHT * 1.5f},
                {DecorationType::GRASS_SMALL, DecoCfg::GRASS_WEIGHT * 2.0f},
                {DecorationType::GRASS_LARGE, DecoCfg::GRASS_WEIGHT * 2.0f},
                {DecorationType::BUSH, DecoCfg::BUSH_WEIGHT * 0.5f},
                {DecorationType::ROCK_SMALL, DecoCfg::ROCK_WEIGHT * 0.5f}
            };
            break;

          case Biome::FOREST:
            decorationChance = DecoCfg::FOREST_CHANCE;
            weightedDecorations = {
                {DecorationType::FLOWER_BLUE, DecoCfg::FLOWER_WEIGHT},
                {DecorationType::FLOWER_PINK, DecoCfg::FLOWER_WEIGHT},
                {DecorationType::FLOWER_WHITE, DecoCfg::FLOWER_WEIGHT},
                {DecorationType::FLOWER_YELLOW, DecoCfg::FLOWER_WEIGHT},
                {DecorationType::GRASS_SMALL, DecoCfg::GRASS_WEIGHT},
                {DecorationType::GRASS_LARGE, DecoCfg::GRASS_WEIGHT},
                {DecorationType::BUSH, DecoCfg::BUSH_WEIGHT},
                {DecorationType::STUMP_SMALL, DecoCfg::STUMP_WEIGHT},
                {DecorationType::STUMP_MEDIUM, DecoCfg::STUMP_WEIGHT},
                {DecorationType::DEAD_LOG_HZ, DecoCfg::DEAD_LOG_WEIGHT},
                {DecorationType::DEAD_LOG_VERTICAL, DecoCfg::DEAD_LOG_WEIGHT}
            };
            break;

          case Biome::CELESTIAL:
            decorationChance = DecoCfg::CELESTIAL_CHANCE;
            weightedDecorations = {
                {DecorationType::FLOWER_BLUE, DecoCfg::FLOWER_WEIGHT},
                {DecorationType::FLOWER_WHITE, DecoCfg::FLOWER_WEIGHT},
                {DecorationType::GRASS_SMALL, DecoCfg::GRASS_WEIGHT}
            };
            break;

          case Biome::SWAMP:
            decorationChance = DecoCfg::SWAMP_CHANCE;
            weightedDecorations = {
                {DecorationType::MUSHROOM_PURPLE, DecoCfg::MUSHROOM_WEIGHT},
                {DecorationType::MUSHROOM_TAN, DecoCfg::MUSHROOM_WEIGHT},
                {DecorationType::GRASS_LARGE, DecoCfg::GRASS_WEIGHT},
                {DecorationType::STUMP_SMALL, DecoCfg::STUMP_WEIGHT},
                {DecorationType::DEAD_LOG_HZ, DecoCfg::DEAD_LOG_WEIGHT},
                {DecorationType::DEAD_LOG_VERTICAL, DecoCfg::DEAD_LOG_WEIGHT}
            };
            break;

          case Biome::HAUNTED:
            decorationChance = DecoCfg::HAUNTED_CHANCE;
            weightedDecorations = {
                {DecorationType::MUSHROOM_PURPLE, DecoCfg::MUSHROOM_WEIGHT},
                {DecorationType::MUSHROOM_TAN, DecoCfg::MUSHROOM_WEIGHT},
                {DecorationType::STUMP_SMALL, DecoCfg::STUMP_WEIGHT},
                {DecorationType::STUMP_MEDIUM, DecoCfg::STUMP_WEIGHT},
                {DecorationType::DEAD_LOG_HZ, DecoCfg::DEAD_LOG_WEIGHT},
                {DecorationType::DEAD_LOG_VERTICAL, DecoCfg::DEAD_LOG_WEIGHT}
            };
            break;

          case Biome::MOUNTAIN:
            decorationChance = DecoCfg::MOUNTAIN_CHANCE;
            weightedDecorations = {
                {DecorationType::GRASS_SMALL, DecoCfg::GRASS_WEIGHT},
                {DecorationType::FLOWER_WHITE, DecoCfg::FLOWER_WEIGHT},
                {DecorationType::ROCK_SMALL, DecoCfg::ROCK_WEIGHT}
            };
            break;

          case Biome::DESERT:
          case Biome::OCEAN:
            continue;

          default:
            decorationChance = DecoCfg::DEFAULT_CHANCE;
            weightedDecorations = {
                {DecorationType::GRASS_SMALL, DecoCfg::GRASS_WEIGHT},
                {DecorationType::GRASS_LARGE, DecoCfg::GRASS_WEIGHT}
            };
            break;
        }
      }

      if (!weightedDecorations.empty() && dist(rng) < decorationChance) {
        tile.decorationType = selectWeightedDecoration(weightedDecorations, rng);
      }
    }
  }
}

void WorldGenerator::calculateInitialResources(const WorldData &world) {
  WORLD_MANAGER_INFO(std::format("Calculating initial resources for world: {}", world.worldId));

  int treeCount = 0;
  int rockCount = 0;
  int waterCount = 0;

  for (const auto &row : world.grid) {
    for (const auto &tile : row) {
      switch (tile.obstacleType) {
      case ObstacleType::TREE:
        treeCount++;
        break;
      case ObstacleType::ROCK:
        rockCount++;
        break;
      case ObstacleType::WATER:
        waterCount++;
        break;
      default:
        break;
      }
    }
  }

  WORLD_MANAGER_INFO(std::format("Initial resources - Trees: {}, Rocks: {}, Water: {}",
                                 treeCount, rockCount, waterCount));
}

void WorldGenerator::generateBuildings(WorldData& world, std::default_random_engine& rng) {
  namespace BldgCfg = WorldSpawnConfig::Buildings;

  int height = static_cast<int>(world.grid.size());
  int width = height > 0 ? static_cast<int>(world.grid[0].size()) : 0;

  if (width <= BldgCfg::BUILDING_SIZE || height <= BldgCfg::BUILDING_SIZE) return;

  // World must be large enough for village placement (VILLAGE_RADIUS on each side)
  int minWorldSize = 2 * BldgCfg::VILLAGE_RADIUS + 2;
  if (width < minWorldSize || height < minWorldSize) return;

  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  std::uniform_int_distribution<int> xDist(BldgCfg::VILLAGE_RADIUS, width - BldgCfg::VILLAGE_RADIUS - 1);
  std::uniform_int_distribution<int> yDist(BldgCfg::VILLAGE_RADIUS, height - BldgCfg::VILLAGE_RADIUS - 1);
  uint32_t nextBuildingId = 1;

  // Calculate number of villages based on world size
  int targetVillages = std::max(1, (width * height) / BldgCfg::VILLAGE_DENSITY_DIVISOR);

  // Helper to get biome village weight
  auto getBiomeWeight = [](Biome biome) -> float {
    switch (biome) {
      case Biome::PLAINS: return BldgCfg::PLAINS_VILLAGE_WEIGHT;
      case Biome::FOREST: return BldgCfg::FOREST_VILLAGE_WEIGHT;
      case Biome::DESERT: return BldgCfg::DESERT_VILLAGE_WEIGHT;
      case Biome::SWAMP: return BldgCfg::SWAMP_VILLAGE_WEIGHT;
      case Biome::HAUNTED: return BldgCfg::HAUNTED_VILLAGE_WEIGHT;
      case Biome::CELESTIAL: return BldgCfg::CELESTIAL_VILLAGE_WEIGHT;
      case Biome::MOUNTAIN:
      case Biome::OCEAN:
      default: return 0.0f;
    }
  };

  // Helper to check if position is valid for village center
  auto isValidVillageCenter = [&](int cx, int cy) -> bool {
    if (cx < BldgCfg::VILLAGE_RADIUS || cx >= width - BldgCfg::VILLAGE_RADIUS ||
        cy < BldgCfg::VILLAGE_RADIUS || cy >= height - BldgCfg::VILLAGE_RADIUS) {
      return false;
    }
    const Tile& tile = world.grid[cy][cx];
    return !tile.isWater && tile.biome != Biome::MOUNTAIN && tile.biome != Biome::OCEAN;
  };

  // Store village centers to enforce minimum distance
  std::vector<std::pair<int, int>> villageCenters;

  // Helper to check distance from existing villages
  auto isFarEnoughFromVillages = [&](int x, int y) -> bool {
    const int minDistSq = BldgCfg::VILLAGE_MIN_DISTANCE * BldgCfg::VILLAGE_MIN_DISTANCE;
    return std::none_of(villageCenters.begin(), villageCenters.end(),
        [x, y](const auto& center) {
            int dx = x - center.first;
            int dy = y - center.second;
            return dx * dx + dy * dy < minDistSq;
        });
  };

  // Find village center locations
  int maxAttempts = targetVillages * 50;  // Allow many attempts to find valid spots
  for (int attempt = 0; attempt < maxAttempts && static_cast<int>(villageCenters.size()) < targetVillages; ++attempt) {
    int cx = xDist(rng);
    int cy = yDist(rng);

    if (!isValidVillageCenter(cx, cy)) continue;
    if (!isFarEnoughFromVillages(cx, cy)) continue;

    // Check biome suitability
    float biomeWeight = getBiomeWeight(world.grid[cy][cx].biome);
    if (biomeWeight <= 0.0f || dist(rng) > biomeWeight) continue;

    villageCenters.emplace_back(cx, cy);
  }

  // Generate buildings for each village
  std::uniform_int_distribution<int> buildingCountDist(BldgCfg::VILLAGE_MIN_BUILDINGS, BldgCfg::VILLAGE_MAX_BUILDINGS);

  for (const auto& center : villageCenters) {
    int villageX = center.first;
    int villageY = center.second;
    int targetBuildings = buildingCountDist(rng);
    int buildingsPlaced = 0;

    // Try to place buildings within village radius, favoring positions near center
    int placementAttempts = targetBuildings * 20;
    for (int attempt = 0; attempt < placementAttempts && buildingsPlaced < targetBuildings; ++attempt) {
      // Generate offset from center with bias toward center (gaussian-like distribution)
      float angle = dist(rng) * 2.0f * 3.14159f;
      float radiusFactor = dist(rng) * dist(rng);  // Square for center bias
      float radius = radiusFactor * static_cast<float>(BldgCfg::VILLAGE_RADIUS);

      int bx = villageX + static_cast<int>(radius * std::cos(angle));
      int by = villageY + static_cast<int>(radius * std::sin(angle));

      // Validate position
      if (bx < 0 || bx >= width - 1 || by < 0 || by >= height - 1) continue;
      if (world.grid[by][bx].buildingId > 0) continue;
      if (!canPlaceBuilding(world, bx, by)) continue;

      // Create building
      uint32_t newBuildingId = createBuilding(world, bx, by, nextBuildingId);
      if (newBuildingId > 0) {
        tryConnectBuildings(world, bx, by, newBuildingId);
        buildingsPlaced++;
      }
    }
  }

  WORLD_MANAGER_DEBUG(std::format("Generated {} villages with buildings", villageCenters.size()));
}

bool WorldGenerator::canPlaceBuilding(const WorldData& world, int x, int y) {
  int height = static_cast<int>(world.grid.size());
  int width = height > 0 ? static_cast<int>(world.grid[0].size()) : 0;

  // Check bounds for 2x2 building
  if (x < 0 || y < 0 || x >= width - 1 || y >= height - 1) {
    return false;
  }

  // Check all 4 tiles of the 2x2 area
  for (int dy = 0; dy < 2; ++dy) {
    for (int dx = 0; dx < 2; ++dx) {
      const Tile& tile = world.grid[y + dy][x + dx];
      
      // Can't place on water, existing obstacles, or mountain biome
      if (tile.isWater || 
          tile.obstacleType != ObstacleType::NONE ||
          tile.biome == Biome::MOUNTAIN ||
          tile.biome == Biome::OCEAN ||
          tile.buildingId > 0) {
        return false;
      }
    }
  }

  return true;
}

uint32_t WorldGenerator::createBuilding(WorldData& world, int x, int y, uint32_t& nextBuildingId) {
  int height = static_cast<int>(world.grid.size());
  int width = height > 0 ? static_cast<int>(world.grid[0].size()) : 0;

  // Validate bounds before creating building
  if (x < 0 || y < 0 || x >= width - 1 || y >= height - 1) {
    WORLD_MANAGER_ERROR(std::format("createBuilding: Invalid position ({}, {}) - out of bounds", x, y));
    return 0;
  }

  uint32_t buildingId = nextBuildingId++;

  // Mark all 4 tiles as part of this building (2x2)
  int tilesMarked = 0;
  for (int dy = 0; dy < 2; ++dy) {
    for (int dx = 0; dx < 2; ++dx) {
      int tileX = x + dx;
      int tileY = y + dy;

      // Double-check bounds for safety
      if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) {
        WORLD_MANAGER_ERROR(std::format("createBuilding: Tile ({}, {}) out of bounds during building creation",
                                        tileX, tileY));
        continue;
      }

      Tile& tile = world.grid[tileY][tileX];

      // Verify tile is available before marking
      if (tile.obstacleType != ObstacleType::NONE || tile.buildingId > 0) {
        WORLD_MANAGER_WARN(std::format("createBuilding: Tile ({}, {}) already occupied", tileX, tileY));
        continue;
      }

      tile.obstacleType = ObstacleType::BUILDING;
      tile.buildingId = buildingId;
      tile.buildingSize = 1; // Start as size 1 (hut)
      tile.isTopLeftOfBuilding = (dx == 0 && dy == 0);  // Only top-left renders the building
      tilesMarked++;
    }
  }

  // Validate that all 4 tiles were successfully marked
  if (tilesMarked != 4) {
    WORLD_MANAGER_ERROR(std::format("createBuilding: Building {} at ({}, {}) only marked {}/4 tiles",
                                    buildingId, x, y, tilesMarked));
  } else {
    WORLD_MANAGER_DEBUG(std::format("createBuilding: Building {} created at ({}, {}) covering tiles ({}-{}, {}-{})",
                                    buildingId, x, y, x, x+1, y, y+1));
  }

  return buildingId;
}

void WorldGenerator::tryConnectBuildings(WorldData& world, int x, int y, uint32_t buildingId) {
  namespace BldgCfg = WorldSpawnConfig::Buildings;

  int height = static_cast<int>(world.grid.size());
  int width = height > 0 ? static_cast<int>(world.grid[0].size()) : 0;

  std::vector<uint32_t> connectedBuildings;
  connectedBuildings.push_back(buildingId);

  // Check for adjacent buildings to connect to (check all sides of the 2x2 building)
  // For a 2x2 building at (x,y), check where other 2x2 buildings would be adjacent
  
  // Check left side - building at (x-2, y) would be directly adjacent
  if (x >= 2) {
    for (int dy = 0; dy < 2; ++dy) {
      if (y + dy < height) {
        const Tile& leftTile = world.grid[y + dy][x - 1]; // Check the edge tile
        if (leftTile.buildingId > 0 && leftTile.buildingId != buildingId) {
          if (std::find(connectedBuildings.begin(), connectedBuildings.end(), leftTile.buildingId) == connectedBuildings.end()) {
            connectedBuildings.push_back(leftTile.buildingId);
          }
        }
      }
    }
  }

  // Check right side - building at (x+2, y) would be directly adjacent
  if (x + 2 < width) {
    for (int dy = 0; dy < 2; ++dy) {
      if (y + dy < height) {
        const Tile& rightTile = world.grid[y + dy][x + 2]; // Check the edge tile
        if (rightTile.buildingId > 0 && rightTile.buildingId != buildingId) {
          if (std::find(connectedBuildings.begin(), connectedBuildings.end(), rightTile.buildingId) == connectedBuildings.end()) {
            connectedBuildings.push_back(rightTile.buildingId);
          }
        }
      }
    }
  }

  // Check top side - building at (x, y-2) would be directly adjacent
  if (y >= 1) {
    for (int dx = 0; dx < 2; ++dx) {
      if (x + dx < width) {
        const Tile& topTile = world.grid[y - 1][x + dx]; // Check the edge tile
        if (topTile.buildingId > 0 && topTile.buildingId != buildingId) {
          if (std::find(connectedBuildings.begin(), connectedBuildings.end(), topTile.buildingId) == connectedBuildings.end()) {
            connectedBuildings.push_back(topTile.buildingId);
          }
        }
      }
    }
  }

  // Check bottom side - building at (x, y+2) would be directly adjacent
  if (y + 2 < height) {
    for (int dx = 0; dx < 2; ++dx) {
      if (x + dx < width) {
        const Tile& bottomTile = world.grid[y + 2][x + dx]; // Check the edge tile
        if (bottomTile.buildingId > 0 && bottomTile.buildingId != buildingId) {
          if (std::find(connectedBuildings.begin(), connectedBuildings.end(), bottomTile.buildingId) == connectedBuildings.end()) {
            connectedBuildings.push_back(bottomTile.buildingId);
          }
        }
      }
    }
  }

  // Update building sizes for all connected buildings
  uint8_t newSize = static_cast<uint8_t>(std::min(
      static_cast<uint32_t>(BldgCfg::MAX_CONNECTED_SIZE),
      static_cast<uint32_t>(connectedBuildings.size())));
  
  for (uint32_t connectedId : connectedBuildings) {
    // Update all tiles belonging to each connected building
    for (int row = 0; row < height; ++row) {
      for (int col = 0; col < width; ++col) {
        if (world.grid[row][col].buildingId == connectedId) {
          world.grid[row][col].buildingSize = newSize;
        }
      }
    }
  }
}

} // namespace VoidLight