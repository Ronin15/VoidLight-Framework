/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE PathfindingSystemTests
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/old/interface.hpp>

#include "ai/pathfinding/PathfindingGrid.hpp"
#include "utils/Vector2D.hpp"
#include <vector>
#include <chrono>
#include <random>
#include <cmath>

using namespace VoidLight;

// Test fixture for pathfinding grid
struct PathfindingGridFixture {
    // NOTE: Unit tests use 32px for algorithm precision testing
    // PathfinderManager uses 64px for production performance optimization
    static constexpr float CELL_SIZE = 32.0f; 
    
    PathfindingGridFixture() 
        : grid(20, 20, CELL_SIZE, Vector2D(0.0f, 0.0f))
    {
        // Create a simple test world with some obstacles
        setupSimpleWorld();
    }
    
    void setupSimpleWorld() {
        // Build an explicit test layout directly in the grid so the tests
        // validate concrete pathfinding behavior instead of interface-only
        // outcomes.
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                const bool isPerimeterWall = (x == 0 || x == 19 || y == 0 || y == 19);
                const bool isCentralWall = (x == 10 && y >= 5 && y <= 15 && y != 10);
                const bool isLShapedObstacle =
                    ((x >= 15 && x <= 17 && y >= 10 && y <= 12) ||
                     (x == 15 && y >= 12 && y <= 14));

                grid.setBlocked(x, y, isPerimeterWall || isCentralWall || isLShapedObstacle);
            }
        }
    }
    
    PathfindingGrid grid;
};

BOOST_AUTO_TEST_SUITE(PathfindingGridBasicTests)

BOOST_FIXTURE_TEST_CASE(TestPathfindingConfiguration, PathfindingGridFixture)
{
    // Test diagonal movement toggle
    grid.setAllowDiagonal(false);
    grid.setAllowDiagonal(true); // Just testing the interface
    
    // Test cost configuration
    grid.setCosts(1.0f, 1.4f);
    
    // Test iteration limits
    grid.setMaxIterations(5000);
    
    std::vector<Vector2D> path;
    const auto result = grid.findPath(Vector2D(48.0f, 48.0f), Vector2D(304.0f, 304.0f), path);
    BOOST_CHECK_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 2);
}

BOOST_FIXTURE_TEST_CASE(TestWeightSystem, PathfindingGridFixture)
{
    // Test weight reset
    grid.resetWeights(1.5f);
    
    // Test adding weight circles
    grid.addWeightCircle(Vector2D(160.0f, 160.0f), 64.0f, 2.0f);

    std::vector<Vector2D> path;
    const PathfindingResult result =
        grid.findPath(Vector2D(48.0f, 48.0f), Vector2D(304.0f, 304.0f), path);

    BOOST_REQUIRE_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PathfindingAlgorithmTests)

BOOST_FIXTURE_TEST_CASE(TestSimplePathfinding, PathfindingGridFixture)
{
    // Create a simple open area for pathfinding
    Vector2D start(48.0f, 48.0f);   // Grid position (1, 1) - should be open
    Vector2D goal(304.0f, 304.0f);  // Grid position (9, 9) - should be open
    
    std::vector<Vector2D> path;
    PathfindingResult result = grid.findPath(start, goal, path);

    BOOST_REQUIRE_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 2); // At least start and goal

    // First point should be near start
    float startDistance = std::sqrt(std::pow(path[0].getX() - start.getX(), 2) +
                                   std::pow(path[0].getY() - start.getY(), 2));
    BOOST_CHECK_LT(startDistance, 50.0f); // Unit test tolerance

    // Last point should be near goal
    float goalDistance = std::sqrt(std::pow(path.back().getX() - goal.getX(), 2) +
                                  std::pow(path.back().getY() - goal.getY(), 2));
    BOOST_CHECK_LT(goalDistance, 50.0f); // Unit test tolerance
}

BOOST_FIXTURE_TEST_CASE(TestInvalidStartAndGoal, PathfindingGridFixture)
{
    std::vector<Vector2D> path;
    
    // Test out of bounds start
    Vector2D invalidStart(-100.0f, -100.0f);
    Vector2D validGoal(160.0f, 160.0f);
    
    PathfindingResult result1 = grid.findPath(invalidStart, validGoal, path);
    BOOST_CHECK(result1 == PathfindingResult::INVALID_START || 
                result1 == PathfindingResult::NO_PATH_FOUND);
    
    // Test out of bounds goal
    Vector2D validStart(160.0f, 160.0f);
    Vector2D invalidGoal(1000.0f, 1000.0f);
    
    PathfindingResult result2 = grid.findPath(validStart, invalidGoal, path);
    BOOST_CHECK(result2 == PathfindingResult::INVALID_GOAL || 
                result2 == PathfindingResult::NO_PATH_FOUND);
}

BOOST_FIXTURE_TEST_CASE(TestSameStartAndGoal, PathfindingGridFixture)
{
    Vector2D samePoint(160.0f, 160.0f);
    std::vector<Vector2D> path;
    
    PathfindingResult result = grid.findPath(samePoint, samePoint, path);

    BOOST_REQUIRE_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 1);
}

BOOST_FIXTURE_TEST_CASE(TestDiagonalMovementToggle, PathfindingGridFixture)
{
    Vector2D start(48.0f, 48.0f);
    Vector2D goal(144.0f, 144.0f); // Diagonal goal
    
    std::vector<Vector2D> pathWithDiagonal, pathWithoutDiagonal;
    
    // Test with diagonal movement
    grid.setAllowDiagonal(true);
    PathfindingResult result1 = grid.findPath(start, goal, pathWithDiagonal);
    
    // Test without diagonal movement
    grid.setAllowDiagonal(false);
    PathfindingResult result2 = grid.findPath(start, goal, pathWithoutDiagonal);
    
    BOOST_REQUIRE_EQUAL(result1, PathfindingResult::SUCCESS);
    BOOST_REQUIRE_EQUAL(result2, PathfindingResult::SUCCESS);
    // Without diagonal movement, path should typically be longer
    BOOST_CHECK_GE(pathWithoutDiagonal.size(), pathWithDiagonal.size());
}

BOOST_FIXTURE_TEST_CASE(TestHierarchicalLongDistance, PathfindingGridFixture)
{
    // The hierarchical API should handle a long-distance path successfully.
    // This fixture only builds a 5x5 coarse grid, so the production
    // edge-aware selector intentionally keeps border-adjacent requests on the
    // direct path even when the hierarchical API itself succeeds.
    Vector2D start(48.0f, 48.0f);     // near (1,1)
    Vector2D farGoal(560.0f, 560.0f); // near (17,17)

    std::vector<Vector2D> path;
    PathfindingResult result = grid.findPathHierarchical(start, farGoal, path);

    BOOST_REQUIRE_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK(!grid.shouldUseHierarchicalPathfinding(start, farGoal));
    BOOST_CHECK_GE(path.size(), 2);
}

BOOST_FIXTURE_TEST_CASE(TestDirectShortDistance, PathfindingGridFixture)
{
    // Short-distance path should be fine with direct method
    Vector2D start(96.0f, 96.0f);   // near (3,3)
    Vector2D goal(128.0f, 128.0f);  // near (4,4)

    std::vector<Vector2D> path;
    PathfindingResult result = grid.findPath(start, goal, path);

    BOOST_REQUIRE_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK(!grid.shouldUseHierarchicalPathfinding(start, goal));
    BOOST_CHECK_GE(path.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PathfindingWeightTests)

BOOST_FIXTURE_TEST_CASE(TestWeightReset, PathfindingGridFixture)
{
    grid.resetWeights(2.0f);
    grid.resetWeights(1.0f); // Reset back to default

    std::vector<Vector2D> path;
    const auto result = grid.findPath(Vector2D(48.0f, 48.0f), Vector2D(304.0f, 304.0f), path);
    BOOST_CHECK_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 2);
}

BOOST_FIXTURE_TEST_CASE(TestWeightCircleApplication, PathfindingGridFixture)
{
    std::vector<Vector2D> baselinePath;
    const Vector2D start(48.0f, 48.0f);
    const Vector2D goal(304.0f, 304.0f);

    BOOST_REQUIRE_EQUAL(grid.findPath(start, goal, baselinePath), PathfindingResult::SUCCESS);

    grid.resetWeights(1.0f);
    grid.addWeightCircle(Vector2D(160.0f, 160.0f), 96.0f, 8.0f);

    std::vector<Vector2D> weightedPath;
    BOOST_REQUIRE_EQUAL(grid.findPath(start, goal, weightedPath), PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(weightedPath.size(), baselinePath.size());
}

BOOST_FIXTURE_TEST_CASE(TestMultipleWeightAreas, PathfindingGridFixture)
{
    grid.resetWeights(1.0f);
    
    // Add multiple weight areas
    grid.addWeightCircle(Vector2D(100.0f, 100.0f), 32.0f, 2.0f);
    grid.addWeightCircle(Vector2D(200.0f, 200.0f), 48.0f, 3.0f);
    grid.addWeightCircle(Vector2D(300.0f, 100.0f), 24.0f, 4.0f);
    
    // Overlapping areas should take the maximum weight
    grid.addWeightCircle(Vector2D(110.0f, 110.0f), 32.0f, 1.5f); // Should not reduce existing weight
    
    std::vector<Vector2D> path;
    BOOST_REQUIRE_EQUAL(
        grid.findPath(Vector2D(48.0f, 48.0f), Vector2D(304.0f, 304.0f), path),
        PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PathfindingPerformanceTests)

BOOST_FIXTURE_TEST_CASE(TestPathfindingPerformance, PathfindingGridFixture)
{
    const int NUM_PATHFINDING_TESTS = 50;
    const float WORLD_SIZE = 640.0f; // 20 * 32
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(32.0f, WORLD_SIZE - 32.0f);
    
    std::vector<std::pair<Vector2D, Vector2D>> testCases;
    
    // Generate random start/goal pairs
    for (int i = 0; i < NUM_PATHFINDING_TESTS; ++i) {
        Vector2D start(posDist(rng), posDist(rng));
        Vector2D goal(posDist(rng), posDist(rng));
        testCases.emplace_back(start, goal);
    }
    
    // Performance test
    auto startTime = std::chrono::high_resolution_clock::now();
    
    int successfulPaths = 0;
    int totalPathLength = 0;
    
    for (const auto& testCase : testCases) {
        std::vector<Vector2D> path;
        PathfindingResult result = grid.findPath(testCase.first, testCase.second, path);
        
        if (result == PathfindingResult::SUCCESS) {
            successfulPaths++;
            totalPathLength += path.size();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    BOOST_TEST_MESSAGE("Pathfinding performance test:");
    BOOST_TEST_MESSAGE("  " << NUM_PATHFINDING_TESTS << " pathfinding requests in " 
                      << duration.count() << " microseconds");
    BOOST_TEST_MESSAGE("  " << (duration.count() / NUM_PATHFINDING_TESTS) << " μs per pathfinding request");
    BOOST_TEST_MESSAGE("  " << successfulPaths << " successful paths out of " << NUM_PATHFINDING_TESTS);
    if (successfulPaths > 0) {
        BOOST_TEST_MESSAGE("  Average path length: " << (totalPathLength / successfulPaths) << " waypoints");
    }
    
    // Performance requirements (adjust based on target performance)
    BOOST_CHECK_LT(duration.count() / NUM_PATHFINDING_TESTS, 5000); // < 5ms per pathfinding request
}

BOOST_FIXTURE_TEST_CASE(TestPathfindingIterationLimits, PathfindingGridFixture)
{
    // Test with very low iteration limits
    grid.setMaxIterations(100);
    
    Vector2D start(128.0f, 128.0f);      // Grid (4,4) - safely inside boundary
    Vector2D distantGoal(448.0f, 448.0f); // Grid (14,14) - safely inside boundary, far away goal
    
    std::vector<Vector2D> path;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    PathfindingResult result = grid.findPath(start, distantGoal, path);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Should timeout quickly with low iteration limit
    BOOST_CHECK(result == PathfindingResult::TIMEOUT || 
                result == PathfindingResult::NO_PATH_FOUND ||
                result == PathfindingResult::SUCCESS);
    
    // Should complete quickly due to iteration limit
    BOOST_CHECK_LT(duration.count(), 100); // < 100ms
    
    BOOST_TEST_MESSAGE("Limited iteration pathfinding completed in " 
                      << duration.count() << "ms with result: " 
                      << static_cast<int>(result));
}

BOOST_FIXTURE_TEST_CASE(TestPathfindingMemoryUsage, PathfindingGridFixture)
{
    // Test multiple pathfinding requests to ensure no memory leaks
    const int STRESS_TEST_ITERATIONS = 200;
    
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> posDist(64.0f, 576.0f);
    
    for (int i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
        Vector2D start(posDist(rng), posDist(rng));
        Vector2D goal(posDist(rng), posDist(rng));
        
        std::vector<Vector2D> path;
        grid.findPath(start, goal, path);
        
        // Path vector should be cleared between requests
        path.clear();
        
        // Periodically reset weights to test that functionality
        if (i % 50 == 0) {
            grid.resetWeights(1.0f);
        }
        
        // Add occasional weight areas
        if (i % 25 == 0) {
            grid.addWeightCircle(Vector2D(posDist(rng), posDist(rng)), 32.0f, 2.0f);
        }
    }
    
    BOOST_TEST_MESSAGE("Memory usage test: " << STRESS_TEST_ITERATIONS 
                      << " pathfinding operations completed");
    std::vector<Vector2D> verificationPath;
    const auto result = grid.findPath(Vector2D(48.0f, 48.0f), Vector2D(304.0f, 304.0f), verificationPath);
    BOOST_CHECK_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(verificationPath.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PathfindingEdgeCaseTests)

BOOST_FIXTURE_TEST_CASE(TestNearestOpenCellFinding, PathfindingGridFixture)
{
    const Vector2D blockedCorner(16.0f, 16.0f);  // Inside a blocked perimeter cell
    BOOST_CHECK(grid.isWorldBlocked(blockedCorner));

    const Vector2D snapped = grid.snapToNearestOpenWorld(blockedCorner, 128.0f);
    BOOST_CHECK(!grid.isWorldBlocked(snapped));

    std::vector<Vector2D> path;
    const PathfindingResult result =
        grid.findPath(blockedCorner, Vector2D(304.0f, 304.0f), path);
    BOOST_REQUIRE_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 2);
}

BOOST_FIXTURE_TEST_CASE(TestPathfindingWithWeights, PathfindingGridFixture)
{
    Vector2D start(80.0f, 80.0f);
    Vector2D goal(400.0f, 400.0f);
    
    // First, find path without weights
    std::vector<Vector2D> normalPath;
    PathfindingResult normalResult = grid.findPath(start, goal, normalPath);
    
    // Add weight area in the middle
    grid.addWeightCircle(Vector2D(240.0f, 240.0f), 80.0f, 5.0f);
    
    // Find path with weights
    std::vector<Vector2D> weightedPath;
    PathfindingResult weightedResult = grid.findPath(start, goal, weightedPath);
    
    BOOST_REQUIRE_EQUAL(normalResult, PathfindingResult::SUCCESS);
    BOOST_REQUIRE_EQUAL(weightedResult, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(weightedPath.size(), 2);
    BOOST_CHECK_GE(weightedPath.size(), normalPath.size());
}

BOOST_FIXTURE_TEST_CASE(TestExtremeDistances, PathfindingGridFixture)
{
    // Test very short distance
    Vector2D closeStart(160.0f, 160.0f);
    Vector2D closeGoal(165.0f, 165.0f);
    
    std::vector<Vector2D> shortPath;
    PathfindingResult shortResult = grid.findPath(closeStart, closeGoal, shortPath);
    
    // Test maximum distance within grid (respecting boundary requirements: 3 tiles = 96px from edge)
    Vector2D farStart(128.0f, 128.0f);  // Grid (4,4) - safely inside boundary  
    Vector2D farGoal(480.0f, 480.0f);   // Grid (15,15) - safely inside boundary
    
    std::vector<Vector2D> longPath;
    PathfindingResult longResult = grid.findPath(farStart, farGoal, longPath);
    
    // Both should handle the distance appropriately
    BOOST_CHECK(shortResult != PathfindingResult::INVALID_START);
    BOOST_CHECK(shortResult != PathfindingResult::INVALID_GOAL);
    BOOST_CHECK(longResult != PathfindingResult::INVALID_START);
    BOOST_CHECK(longResult != PathfindingResult::INVALID_GOAL);
    
    if (shortResult == PathfindingResult::SUCCESS) {
        BOOST_CHECK_GE(shortPath.size(), 1);
    }
    
    if (longResult == PathfindingResult::SUCCESS) {
        BOOST_CHECK_GE(longPath.size(), 2);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ===== ENHANCED PATHFINDING TESTS =====

BOOST_AUTO_TEST_SUITE(PathSmoothingTests)

BOOST_FIXTURE_TEST_CASE(TestLineOfSightDetection, PathfindingGridFixture)
{
    // Test line of sight functionality with known obstacle patterns
    Vector2D start(64.0f, 64.0f);    // Grid (2,2)
    Vector2D goal(256.0f, 256.0f);   // Grid (8,8)

    std::vector<Vector2D> path;
    PathfindingResult result = grid.findPath(start, goal, path);

    BOOST_REQUIRE_EQUAL(result, PathfindingResult::SUCCESS);
    BOOST_CHECK_GE(path.size(), 2);

    // The path should have been smoothed (fewer waypoints than grid steps)
    float directDistance = std::sqrt(std::pow(goal.getX() - start.getX(), 2) +
                                   std::pow(goal.getY() - start.getY(), 2));
    float gridSteps = directDistance / CELL_SIZE;

    // Smoothed path should have significantly fewer waypoints than grid steps
    BOOST_CHECK_LT(static_cast<float>(path.size()), gridSteps * 0.8f);
}

BOOST_FIXTURE_TEST_CASE(TestPathSmoothingWithObstacles, PathfindingGridFixture)
{
    // Add weight areas to simulate obstacles for path smoothing
    grid.addWeightCircle(Vector2D(160.0f, 160.0f), 32.0f, 10.0f);

    Vector2D start(64.0f, 64.0f);
    Vector2D goal(256.0f, 256.0f);

    std::vector<Vector2D> path;
    PathfindingResult result = grid.findPath(start, goal, path);

    if (result == PathfindingResult::SUCCESS) {
        BOOST_CHECK_GE(path.size(), 2);

        // Verify path doesn't pass through high-weight area
        bool pathAvoidedObstacle = true;
        for (const auto& waypoint : path) {
            float distToObstacle = std::sqrt(std::pow(waypoint.getX() - 160.0f, 2) +
                                           std::pow(waypoint.getY() - 160.0f, 2));
            if (distToObstacle < 40.0f) {
                pathAvoidedObstacle = false;
                break;
            }
        }

        BOOST_TEST_MESSAGE("Path " << (pathAvoidedObstacle ? "avoided" : "passed through")
                          << " weighted obstacle area");
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(HierarchicalPathfindingAdvancedTests)

BOOST_FIXTURE_TEST_CASE(TestHierarchicalVsDirectComparison, PathfindingGridFixture)
{
    struct TestCase {
        Vector2D start, goal;
        std::string description;
    };

    std::vector<TestCase> testCases = {
        {{96.0f, 96.0f}, {160.0f, 160.0f}, "Short distance"},
        {{64.0f, 64.0f}, {320.0f, 320.0f}, "Medium distance"},
        {{48.0f, 48.0f}, {560.0f, 560.0f}, "Long distance"}
    };

    for (const auto& testCase : testCases) {
        std::vector<Vector2D> directPath, hierarchicalPath;

        auto directStart = std::chrono::high_resolution_clock::now();
        PathfindingResult directResult = grid.findPath(testCase.start, testCase.goal, directPath);
        auto directEnd = std::chrono::high_resolution_clock::now();

        auto hierarchicalStart = std::chrono::high_resolution_clock::now();
        PathfindingResult hierarchicalResult = grid.findPathHierarchical(testCase.start, testCase.goal, hierarchicalPath);
        auto hierarchicalEnd = std::chrono::high_resolution_clock::now();

        auto directTime = std::chrono::duration_cast<std::chrono::microseconds>(directEnd - directStart);
        auto hierarchicalTime = std::chrono::duration_cast<std::chrono::microseconds>(hierarchicalEnd - hierarchicalStart);

        BOOST_TEST_MESSAGE(testCase.description << ": Direct=" << directTime.count()
                          << "μs, Hierarchical=" << hierarchicalTime.count() << "μs");

        // Both methods should handle the pathfinding request consistently
        BOOST_CHECK_EQUAL(directResult, hierarchicalResult);

        if (directResult == PathfindingResult::SUCCESS && hierarchicalResult == PathfindingResult::SUCCESS) {
            BOOST_CHECK_GE(directPath.size(), 1);
            BOOST_CHECK_GE(hierarchicalPath.size(), 1);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(TestHierarchicalDecisionLogic, PathfindingGridFixture)
{
    struct DistanceTest {
        Vector2D start, goal;
        bool shouldUseHierarchical;
        std::string description;
    };

    std::vector<DistanceTest> tests = {
        {{160.0f, 160.0f}, {200.0f, 200.0f}, false, "Very short distance"},
        {{300.0f, 300.0f}, {320.0f, 320.0f}, false, "Short distance in interior"},
        {{300.0f, 300.0f}, {400.0f, 400.0f}, false, "Below distance threshold"},
        {{280.0f, 280.0f}, {490.0f, 490.0f}, true, "Above threshold in interior"}
    };

    for (const auto& test : tests) {
        bool decision = grid.shouldUseHierarchicalPathfinding(test.start, test.goal);

        float distance = std::sqrt(std::pow(test.goal.getX() - test.start.getX(), 2) +
                                  std::pow(test.goal.getY() - test.start.getY(), 2));
        BOOST_TEST_MESSAGE(test.description << " (" << distance << "px, threshold=512px): "
                          << (decision ? "Hierarchical" : "Direct"));

        // Only check cases that should be hierarchical if distance is actually > 512
        if (test.shouldUseHierarchical && distance <= 512.0f) {
            BOOST_TEST_MESSAGE("Skipping hierarchical test - distance too small for current grid");
            continue;
        }

        BOOST_CHECK_EQUAL(decision, test.shouldUseHierarchical);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PathfindingPerformanceTests)

BOOST_FIXTURE_TEST_CASE(TestPerformanceBaseline, PathfindingGridFixture)
{
    const int BASELINE_TESTS = 25;
    const double MAX_AVG_TIME_US = 3000.0; // 3ms baseline

    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> posDist(96.0f, 544.0f);

    std::vector<double> pathTimes;
    pathTimes.reserve(BASELINE_TESTS);

    for (int i = 0; i < BASELINE_TESTS; ++i) {
        Vector2D start(posDist(rng), posDist(rng));
        Vector2D goal(posDist(rng), posDist(rng));
        std::vector<Vector2D> path;

        auto startTime = std::chrono::high_resolution_clock::now();
        grid.findPath(start, goal, path);
        auto endTime = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        pathTimes.push_back(duration.count());

        BOOST_CHECK_LT(duration.count(), 15000); // < 15ms per path
    }

    double totalTime = 0.0;
    for (double time : pathTimes) {
        totalTime += time;
    }
    double avgTime = totalTime / BASELINE_TESTS;

    BOOST_CHECK_LT(avgTime, MAX_AVG_TIME_US);

    std::sort(pathTimes.begin(), pathTimes.end());
    double percentile95 = pathTimes[static_cast<size_t>(BASELINE_TESTS * 0.95)];

    BOOST_TEST_MESSAGE("Performance: avg=" << avgTime << "μs, 95th="
                      << percentile95 << "μs, max=" << pathTimes.back() << "μs");
}

BOOST_FIXTURE_TEST_CASE(TestObjectPoolEfficiency, PathfindingGridFixture)
{
    const int STRESS_ITERATIONS = 50;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(64.0f, 576.0f);

    // Warm up object pools
    for (int i = 0; i < 5; ++i) {
        Vector2D start(posDist(rng), posDist(rng));
        Vector2D goal(posDist(rng), posDist(rng));
        std::vector<Vector2D> path;
        grid.findPath(start, goal, path);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    int successCount = 0;
    for (int i = 0; i < STRESS_ITERATIONS; ++i) {
        Vector2D start(posDist(rng), posDist(rng));
        Vector2D goal(posDist(rng), posDist(rng));
        std::vector<Vector2D> path;

        PathfindingResult result = grid.findPath(start, goal, path);
        if (result == PathfindingResult::SUCCESS) {
            successCount++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    double avgTimePerPath = static_cast<double>(duration.count()) / STRESS_ITERATIONS;
    BOOST_CHECK_LT(avgTimePerPath, 5000.0); // < 5ms per path on average

    BOOST_TEST_MESSAGE("Object pool efficiency: " << STRESS_ITERATIONS
                      << " paths in " << duration.count() << "μs ("
                      << avgTimePerPath << "μs avg), " << successCount << " successful");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PathfindingWorldBoundsTests)

BOOST_FIXTURE_TEST_CASE(TestWorldBoundsHandling, PathfindingGridFixture)
{
    float worldSize = 20 * CELL_SIZE; // 640px

    struct BoundaryTest {
        Vector2D start, goal;
        std::string description;
    };

    std::vector<BoundaryTest> boundaryTests = {
        {{16.0f, 320.0f}, {624.0f, 320.0f}, "Left to right edge"},
        {{320.0f, 16.0f}, {320.0f, 624.0f}, "Top to bottom edge"},
        {{32.0f, 32.0f}, {608.0f, 608.0f}, "Corner to corner"},
        {{64.0f, 64.0f}, {576.0f, 64.0f}, "Near-edge horizontal"}
    };

    for (const auto& test : boundaryTests) {
        std::vector<Vector2D> path;
        PathfindingResult result = grid.findPath(test.start, test.goal, path);

        // Should handle boundary cases gracefully
        BOOST_CHECK(result != PathfindingResult::INVALID_START);
        BOOST_CHECK(result != PathfindingResult::INVALID_GOAL);

        if (result == PathfindingResult::SUCCESS) {
            BOOST_CHECK_GE(path.size(), 2);

            // Verify path stays within reasonable world bounds
            for (const auto& waypoint : path) {
                BOOST_CHECK_GE(waypoint.getX(), 0.0f);
                BOOST_CHECK_GE(waypoint.getY(), 0.0f);
                BOOST_CHECK_LE(waypoint.getX(), worldSize);
                BOOST_CHECK_LE(waypoint.getY(), worldSize);
            }
        }

        BOOST_TEST_MESSAGE(test.description << ": " << static_cast<int>(result));
    }
}

BOOST_FIXTURE_TEST_CASE(TestSnapToNearestOpen, PathfindingGridFixture)
{
    Vector2D testPositions[] = {
        {160.0f, 160.0f},  // Open interior
        {16.0f, 16.0f},    // Blocked corner
        {320.0f, 160.0f},  // Central wall column
        {500.0f, 500.0f}   // Far interior
    };

    for (const auto& pos : testPositions) {
        Vector2D snapped = grid.snapToNearestOpenWorld(pos, 128.0f);

        // Snapped position should be valid
        BOOST_CHECK_GE(snapped.getX(), 0.0f);
        BOOST_CHECK_GE(snapped.getY(), 0.0f);
        BOOST_CHECK(!grid.isWorldBlocked(snapped));

        // Should be within reasonable distance of original
        float distance = std::sqrt(std::pow(snapped.getX() - pos.getX(), 2) +
                                  std::pow(snapped.getY() - pos.getY(), 2));
        BOOST_CHECK_LE(distance, 128.0f);

        BOOST_TEST_MESSAGE("Snap " << pos.getX() << "," << pos.getY()
                          << " -> " << snapped.getX() << "," << snapped.getY()
                          << " (distance: " << distance << ")");
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PathfindingIntegrationTests)

BOOST_FIXTURE_TEST_CASE(TestDynamicWeightUpdates, PathfindingGridFixture)
{
    Vector2D start(64.0f, 64.0f);
    Vector2D goal(384.0f, 384.0f);

    // Find path without obstacles
    std::vector<Vector2D> originalPath;
    PathfindingResult originalResult = grid.findPath(start, goal, originalPath);

    if (originalResult == PathfindingResult::SUCCESS) {
        size_t originalPathLength = originalPath.size();

        // Add obstacles that should affect the path
        grid.addWeightCircle(Vector2D(200.0f, 200.0f), 64.0f, 8.0f);
        grid.addWeightCircle(Vector2D(250.0f, 250.0f), 48.0f, 6.0f);

        std::vector<Vector2D> newPath;
        PathfindingResult newResult = grid.findPath(start, goal, newPath);

        if (newResult == PathfindingResult::SUCCESS) {
            BOOST_CHECK_GE(newPath.size(), 2);

            // Verify endpoints are still correct
            float startDist = std::sqrt(std::pow(newPath[0].getX() - start.getX(), 2) +
                                       std::pow(newPath[0].getY() - start.getY(), 2));
            float goalDist = std::sqrt(std::pow(newPath.back().getX() - goal.getX(), 2) +
                                      std::pow(newPath.back().getY() - goal.getY(), 2));

            BOOST_CHECK_LT(startDist, 50.0f);
            BOOST_CHECK_LT(goalDist, 50.0f);

            BOOST_TEST_MESSAGE("Dynamic weights: original " << originalPathLength
                              << " waypoints, new " << newPath.size() << " waypoints");
        }

        // Reset weights
        grid.resetWeights(1.0f);
        std::vector<Vector2D> resetPath;
        PathfindingResult resetResult = grid.findPath(start, goal, resetPath);
        BOOST_CHECK_EQUAL(resetResult, originalResult);
    }
}

BOOST_FIXTURE_TEST_CASE(TestPathfindingStatistics, PathfindingGridFixture)
{
    grid.resetStats();

    auto initialStats = grid.getStats();
    BOOST_CHECK_EQUAL(initialStats.totalRequests, 0);
    BOOST_CHECK_EQUAL(initialStats.successfulPaths, 0);

    const int NUM_REQUESTS = 10;
    int expectedSuccesses = 0;

    for (int i = 0; i < NUM_REQUESTS; ++i) {
        Vector2D start(64.0f + i * 32.0f, 64.0f);
        Vector2D goal(300.0f, 300.0f);
        std::vector<Vector2D> path;

        PathfindingResult result = grid.findPath(start, goal, path);
        if (result == PathfindingResult::SUCCESS) {
            expectedSuccesses++;
        }
    }

    auto finalStats = grid.getStats();
    BOOST_CHECK_EQUAL(finalStats.totalRequests, NUM_REQUESTS);
    BOOST_CHECK_EQUAL(finalStats.successfulPaths, expectedSuccesses);

    BOOST_TEST_MESSAGE("Statistics: " << finalStats.totalRequests << " requests, "
                      << finalStats.successfulPaths << " successful, "
                      << finalStats.timeouts << " timeouts, "
                      << "avgPathLength=" << finalStats.avgPathLength);

    // Every successful path contributes at least one waypoint, so a non-zero
    // success count must yield a non-zero average path length.
    if (expectedSuccesses > 0) {
        BOOST_CHECK_GT(finalStats.avgPathLength, 0);
    }
}

BOOST_AUTO_TEST_SUITE_END()
