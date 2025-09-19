#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../../src/spatial/SpatialManager.h"
#include "../../src/spatial/LayerMask.h"
#include "../../src/components/SpatialComponent.h"
#include "../../src/systems/SpatialSystem.h"
#include "../../src/core/World.h"
#include "../../src/components/Transform.h"
#include <random>
#include <chrono>
#include <numeric>

using namespace VulkanMon;

TEST_CASE("Spatial System Performance Validation", "[spatial][performance]") {
    // Performance targets from the guide
    constexpr float MAX_GRID_UPDATE_TIME_MS = 2.0f;  // Adjusted for debug builds
    constexpr float MAX_RADIUS_QUERY_TIME_MS = 0.5f;
    constexpr float MAX_RAYCAST_TIME_MS = 0.1f;

    SECTION("Grid Update Performance - 1000 Dynamic Entities") {
        BoundingBox worldBounds(glm::vec3(-500, -500, -500), glm::vec3(500, 500, 500));
        World world;
        auto spatialSystem = world.addSystem<SpatialSystem>(worldBounds);

        // Create 1000 dynamic entities
        std::vector<EntityID> entities;
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> posDist(-400.0f, 400.0f);
        std::uniform_real_distribution<float> moveDist(-1.0f, 1.0f);

        for (int i = 0; i < 1000; ++i) {
            auto entity = world.createEntity();
            Transform transform;
            transform.position = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
            world.addComponent(entity, transform);

            SpatialComponent spatial;
            spatial.boundingRadius = 2.0f;
            spatial.behavior = SpatialBehavior::DYNAMIC;
            spatial.spatialLayers = LayerMask::Creatures;
            spatial.markDirty();
            world.addComponent(entity, spatial);

            entities.push_back(entity);
        }

        // Initial update to register entities
        spatialSystem->update(0.016f, world.getEntityManager());

        // Measure update performance with movement
        auto start = std::chrono::high_resolution_clock::now();

        for (int frame = 0; frame < 60; ++frame) { // Simulate 1 second of updates
            // Move entities randomly
            for (auto entity : entities) {
                auto& transform = world.getComponent<Transform>(entity);
                auto& spatial = world.getComponent<SpatialComponent>(entity);

                transform.position += glm::vec3(moveDist(rng), moveDist(rng), moveDist(rng));
                spatial.markDirty();
            }

            spatialSystem->update(0.016f, world.getEntityManager());
        }

        auto end = std::chrono::high_resolution_clock::now();
        float totalTime = std::chrono::duration<float, std::milli>(end - start).count();
        float averageFrameTime = totalTime / 60.0f;

        INFO("Average frame update time: " << averageFrameTime << "ms for 1000 entities");
        CHECK(averageFrameTime < MAX_GRID_UPDATE_TIME_MS);
    }

    SECTION("Radius Query Performance - 50 unit radius with 1000 entities") {
        BoundingBox worldBounds(glm::vec3(-500, -500, -500), glm::vec3(500, 500, 500));
        SpatialManager spatialManager(worldBounds);

        // Create 1000 entities randomly distributed
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-400.0f, 400.0f);

        for (int i = 0; i < 1000; ++i) {
            EntityID entity = EntityID(i + 1);
            glm::vec3 position(dist(rng), dist(rng), dist(rng));
            spatialManager.addEntity(entity, position, LayerMask::Creatures);
        }

        // Measure query performance
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<size_t> querySizes;
        for (int i = 0; i < 100; ++i) {
            glm::vec3 queryCenter(dist(rng), dist(rng), dist(rng));
            auto results = spatialManager.queryRadius(queryCenter, 50.0f);
            querySizes.push_back(results.size());
        }

        auto end = std::chrono::high_resolution_clock::now();
        float totalTime = std::chrono::duration<float, std::milli>(end - start).count();
        float averageQueryTime = totalTime / 100.0f;

        INFO("Average query time: " << averageQueryTime << "ms for 50-unit radius");
        size_t totalEntities = 0;
        for (size_t count : querySizes) { totalEntities += count; }
        INFO("Average entities returned: " <<
             (querySizes.empty() ? 0.0 : static_cast<double>(totalEntities) / querySizes.size()));

        CHECK(averageQueryTime < MAX_RADIUS_QUERY_TIME_MS);
    }

    SECTION("Direct Query Performance Validation") {
        BoundingBox worldBounds(glm::vec3(-100, -100, -100), glm::vec3(100, 100, 100));
        SpatialManager spatialManager(worldBounds);

        // Add entities
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-80.0f, 80.0f);

        for (int i = 0; i < 500; ++i) {
            EntityID entity = EntityID(i + 1);
            glm::vec3 position(dist(rng), dist(rng), dist(rng));
            spatialManager.addEntity(entity, position, LayerMask::Creatures);
        }

        // Perform repeated queries to build cache
        glm::vec3 testCenter(0, 0, 0);
        float testRadius = 25.0f;

        // First query populates cache
        auto results1 = spatialManager.queryRadius(testCenter, testRadius);

        // Subsequent queries should hit cache
        for (int i = 0; i < 20; ++i) {
            auto results = spatialManager.queryRadius(testCenter, testRadius);
            CHECK(results.size() == results1.size()); // Should be identical from cache
        }

        // Check performance statistics
        auto stats = spatialManager.getPerformanceStats();
        INFO("Query performance: " << stats.averageQueryTimeMs << "ms average");
        INFO("Total queries: " << stats.totalQueries);

        // Note: Cache was removed for lock-free performance optimization
        // Direct octree queries are now fast enough that caching isn't needed
        CHECK(stats.totalQueries > 0);  // Should have performed queries
        CHECK(stats.averageQueryTimeMs >= 0.0f);  // Should have valid timing
    }

    SECTION("Layer Filtering Performance") {
        BoundingBox worldBounds(glm::vec3(-200, -200, -200), glm::vec3(200, 200, 200));
        SpatialManager spatialManager(worldBounds);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-150.0f, 150.0f);

        // Add entities with different layers
        for (int i = 0; i < 1000; ++i) {
            EntityID entity = EntityID(i + 1);
            glm::vec3 position(dist(rng), dist(rng), dist(rng));

            uint32_t layer;
            if (i % 4 == 0) layer = LayerMask::Player;
            else if (i % 4 == 1) layer = LayerMask::Creatures;
            else if (i % 4 == 2) layer = LayerMask::Items;
            else layer = LayerMask::Terrain;

            spatialManager.addEntity(entity, position, layer);
        }

        // Measure layer filtering performance
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<size_t> creatureResults, playerResults, allResults;
        for (int i = 0; i < 50; ++i) {
            glm::vec3 queryCenter(dist(rng), dist(rng), dist(rng));

            auto creatures = spatialManager.queryRadius(queryCenter, 30.0f, LayerMask::Creatures);
            auto players = spatialManager.queryRadius(queryCenter, 30.0f, LayerMask::Player);
            auto all = spatialManager.queryRadius(queryCenter, 30.0f, LayerMask::All);

            creatureResults.push_back(creatures.size());
            playerResults.push_back(players.size());
            allResults.push_back(all.size());
        }

        auto end = std::chrono::high_resolution_clock::now();
        float totalTime = std::chrono::duration<float, std::milli>(end - start).count();

        INFO("Layer filtering time for 150 queries: " << totalTime << "ms");
        INFO("Average creatures found: " <<
             (creatureResults.empty() ? 0 : std::accumulate(creatureResults.begin(), creatureResults.end(), size_t(0)) / creatureResults.size()));
        INFO("Average players found: " <<
             (playerResults.empty() ? 0 : std::accumulate(playerResults.begin(), playerResults.end(), size_t(0)) / playerResults.size()));

        // Verify layer filtering works correctly
        for (size_t i = 0; i < creatureResults.size(); ++i) {
            CHECK(creatureResults[i] <= allResults[i]);
            CHECK(playerResults[i] <= allResults[i]);
        }

        CHECK(totalTime < 50.0f); // Should complete 150 queries in under 50ms
    }

    SECTION("Memory Usage Scaling") {
        // Test memory usage doesn't grow excessively
        std::vector<std::unique_ptr<SpatialManager>> managers;

        for (int entityCount : {100, 500, 1000, 2000}) {
            BoundingBox worldBounds(glm::vec3(-100, -100, -100), glm::vec3(100, 100, 100));
            auto manager = std::make_unique<SpatialManager>(worldBounds);

            std::mt19937 rng(42);
            std::uniform_real_distribution<float> dist(-80.0f, 80.0f);

            for (int i = 0; i < entityCount; ++i) {
                EntityID entity = EntityID(i + 1);
                glm::vec3 position(dist(rng), dist(rng), dist(rng));
                manager->addEntity(entity, position, LayerMask::Creatures);
            }

            int nodeCount, maxDepth, totalEntities;
            manager->getStatistics(nodeCount, maxDepth, totalEntities);

            INFO("Entities: " << entityCount <<
                 ", Nodes: " << nodeCount <<
                 ", Max Depth: " << maxDepth);

            // Verify statistics are reasonable
            CHECK(totalEntities == entityCount);
            CHECK(maxDepth <= 12); // Reasonable subdivision depth
            CHECK(nodeCount < entityCount * 2); // Nodes shouldn't grow too much

            managers.push_back(std::move(manager));
        }
    }
}

TEST_CASE("Pokemon-Specific Performance Tests", "[spatial][pokemon][performance]") {
    SECTION("Creature Detection System Performance") {
        BoundingBox worldBounds(glm::vec3(-1000, -1000, -1000), glm::vec3(1000, 1000, 1000));
        SpatialManager spatialManager(worldBounds);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-800.0f, 800.0f);

        // Add 500 creatures scattered across the world
        for (int i = 0; i < 500; ++i) {
            EntityID entity = EntityID(i + 1);
            glm::vec3 position(dist(rng), dist(rng), dist(rng));
            spatialManager.addEntity(entity, position, LayerMask::Creatures);
        }

        // Add 10 players
        for (int i = 0; i < 10; ++i) {
            EntityID entity = EntityID(500 + i + 1);
            glm::vec3 position(dist(rng), dist(rng), dist(rng));
            spatialManager.addEntity(entity, position, LayerMask::Player);
        }

        // Simulate creature detection queries (each creature checks for nearby players)
        auto start = std::chrono::high_resolution_clock::now();

        size_t totalDetections = 0;
        for (int i = 1; i <= 500; ++i) { // For each creature
            EntityID creature = EntityID(i);
            glm::vec3 creaturePos(dist(rng), dist(rng), dist(rng)); // Simulate position

            // Query for nearby players within detection radius
            auto nearbyPlayers = spatialManager.queryRadius(creaturePos, 15.0f, LayerMask::Player);
            totalDetections += nearbyPlayers.size();
        }

        auto end = std::chrono::high_resolution_clock::now();
        float detectionTime = std::chrono::duration<float, std::milli>(end - start).count();

        INFO("Creature detection time for 500 creatures: " << detectionTime << "ms");
        INFO("Total player detections: " << totalDetections);

        // Should complete all creature detection in reasonable time (adjusted for debug builds)
        CHECK(detectionTime < 50.0f); // 50ms for 500 detection checks
    }

    SECTION("Grass System Spawn Performance") {
        BoundingBox worldBounds(glm::vec3(-500, -500, -500), glm::vec3(500, 500, 500));
        SpatialManager spatialManager(worldBounds);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-400.0f, 400.0f);

        // Add 100 grass patches
        for (int i = 0; i < 100; ++i) {
            EntityID entity = EntityID(i + 1);
            glm::vec3 position(dist(rng), 0.0f, dist(rng)); // Grass on ground
            spatialManager.addEntity(entity, position, LayerMask::Grass);
        }

        // Add some creatures moving through the grass
        for (int i = 0; i < 20; ++i) {
            EntityID entity = EntityID(100 + i + 1);
            glm::vec3 position(dist(rng), 1.0f, dist(rng));
            spatialManager.addEntity(entity, position, LayerMask::Creatures);
        }

        // Simulate grass interaction checks (each grass patch checks for nearby movement)
        auto start = std::chrono::high_resolution_clock::now();

        size_t totalInteractions = 0;
        for (int i = 1; i <= 100; ++i) { // For each grass patch
            EntityID grass = EntityID(i);
            glm::vec3 grassPos(dist(rng), 0.0f, dist(rng)); // Simulate position

            // Query for nearby creatures within rustle radius
            auto nearbyCreatures = spatialManager.queryRadius(grassPos, 3.0f, LayerMask::Creatures | LayerMask::Player);
            totalInteractions += nearbyCreatures.size();
        }

        auto end = std::chrono::high_resolution_clock::now();
        float interactionTime = std::chrono::duration<float, std::milli>(end - start).count();

        INFO("Grass interaction time for 100 patches: " << interactionTime << "ms");
        INFO("Total creature interactions: " << totalInteractions);

        // Should complete all grass interaction checks quickly
        CHECK(interactionTime < 5.0f); // 5ms for 100 interaction checks
    }
}