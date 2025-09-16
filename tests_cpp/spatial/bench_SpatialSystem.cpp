#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../../src/spatial/SpatialManager.h"
#include "../../src/components/SpatialComponent.h"
#include "../../src/systems/SpatialSystem.h"
#include "../../src/core/World.h"
#include "../../src/components/Transform.h"
#include <random>
#include <chrono>
#include <numeric>

using namespace VulkanMon;

TEST_CASE("SpatialSystem Performance Benchmarks", "[spatial][benchmark]") {
    // Setup for performance tests
    BoundingBox worldBounds(glm::vec3(-500, -500, -500), glm::vec3(500, 500, 500));

    SECTION("Spatial Query Performance - 1000 Entities") {
        SpatialManager spatialManager(worldBounds);

        // Create 1000 entities randomly distributed
        std::vector<EntityID> entities;
        std::mt19937 rng(42); // Fixed seed for reproducible results
        std::uniform_real_distribution<float> dist(-400.0f, 400.0f);

        for (int i = 0; i < 1000; ++i) {
            EntityID entity = EntityID(i + 1);
            glm::vec3 position(dist(rng), dist(rng), dist(rng));
            spatialManager.addEntity(entity, position);
            entities.push_back(entity);
        }

        BENCHMARK("Radius Query - 50 unit radius") {
            auto results = spatialManager.queryRadius(glm::vec3(0, 0, 0), 50.0f);
            return results.size();
        };

        BENCHMARK("Radius Query - 100 unit radius") {
            auto results = spatialManager.queryRadius(glm::vec3(0, 0, 0), 100.0f);
            return results.size();
        };

        BENCHMARK("Nearest Entity Query") {
            auto nearest = spatialManager.findNearestEntity(glm::vec3(0, 0, 0), 1000.0f);
            return nearest != EntityID(0) ? 1 : 0;
        };

        BENCHMARK("Region Query - 100x100x100 box") {
            BoundingBox region(glm::vec3(-50, -50, -50), glm::vec3(50, 50, 50));
            auto results = spatialManager.queryRegion(region);
            return results.size();
        };
    }

    SECTION("Dynamic Entity Updates Performance") {
        World world;
        auto spatialSystem = world.addSystem<SpatialSystem>(worldBounds);

        // Create dynamic entities
        std::vector<EntityID> entities;
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> posDist(-100.0f, 100.0f);

        for (int i = 0; i < 500; ++i) {
            auto entity = world.createEntity();
            Transform transform;
            transform.position = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
            world.addComponent(entity, transform);

            SpatialComponent spatial;
            spatial.boundingRadius = 2.0f;
            spatial.behavior = SpatialBehavior::DYNAMIC;
            spatial.markDirty();
            world.addComponent(entity, spatial);

            entities.push_back(entity);
        }

        // Initial update to register entities
        spatialSystem->update(0.016f, world.getEntityManager());

        BENCHMARK("Dynamic Entity Update - 500 entities") {
            // Simulate movement
            std::uniform_real_distribution<float> moveDist(-1.0f, 1.0f);
            for (auto entity : entities) {
                auto& transform = world.getComponent<Transform>(entity);
                auto& spatial = world.getComponent<SpatialComponent>(entity);

                transform.position += glm::vec3(moveDist(rng), moveDist(rng), moveDist(rng));
                spatial.markDirty();
            }

            spatialSystem->update(0.016f, world.getEntityManager());
            return entities.size();
        };
    }

    SECTION("Octree Subdivision Performance") {
        SpatialManager spatialManager(worldBounds);

        BENCHMARK("Dense Entity Addition - 2000 entities in small area") {
            // Add many entities in a concentrated area to force subdivision
            std::mt19937 rng(42);
            std::uniform_real_distribution<float> denseDist(-25.0f, 25.0f);

            for (int i = 0; i < 2000; ++i) {
                EntityID entity = EntityID(i + 1);
                glm::vec3 position(denseDist(rng), denseDist(rng), denseDist(rng));
                spatialManager.addEntity(entity, position);
            }

            return 2000;
        };
    }
}

// Stress test for very large numbers of entities
TEST_CASE("Spatial System Stress Tests", "[spatial][stress]") {
    SECTION("10,000 Entity Stress Test") {
        BoundingBox worldBounds(glm::vec3(-1000, -1000, -1000), glm::vec3(1000, 1000, 1000));
        SpatialManager spatialManager(worldBounds);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-800.0f, 800.0f);

        // Add 10,000 entities
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; ++i) {
            EntityID entity = EntityID(i + 1);
            glm::vec3 position(dist(rng), dist(rng), dist(rng));
            spatialManager.addEntity(entity, position);
        }

        auto addTime = std::chrono::high_resolution_clock::now();

        // Perform multiple queries
        std::vector<size_t> queryResults;
        for (int i = 0; i < 100; ++i) {
            glm::vec3 queryCenter(dist(rng), dist(rng), dist(rng));
            auto results = spatialManager.queryRadius(queryCenter, 100.0f);
            queryResults.push_back(results.size());
        }

        auto queryTime = std::chrono::high_resolution_clock::now();

        // Calculate timing
        auto addDuration = std::chrono::duration<float, std::milli>(addTime - start).count();
        auto queryDuration = std::chrono::duration<float, std::milli>(queryTime - addTime).count();

        // Verify performance targets from the guide
        INFO("Add time: " << addDuration << "ms for 10,000 entities");
        INFO("Query time: " << queryDuration << "ms for 100 queries");
        INFO("Average entities per query: " <<
             (queryResults.empty() ? 0 :
              std::accumulate(queryResults.begin(), queryResults.end(), 0) / queryResults.size()));

        // Performance targets from the guide (adjusted for debug builds)
        CHECK(addDuration < 1000.0f); // Should add 10k entities in under 1 second
        CHECK(queryDuration < 150.0f);  // Should complete 100 queries in under 150ms

        // Get octree statistics
        int nodeCount, maxDepth, totalEntities;
        spatialManager.getStatistics(nodeCount, maxDepth, totalEntities);

        INFO("Octree nodes: " << nodeCount);
        INFO("Max depth: " << maxDepth);
        INFO("Total entities tracked: " << totalEntities);

        CHECK(totalEntities == 10000);
        CHECK(maxDepth <= 12); // Reasonable subdivision depth
    }
}

// Memory usage tests
TEST_CASE("Spatial System Memory Tests", "[spatial][memory]") {
    SECTION("Memory usage per entity") {
        BoundingBox worldBounds(glm::vec3(-100, -100, -100), glm::vec3(100, 100, 100));

        // Test memory usage scaling
        for (int entityCount : {100, 500, 1000, 2000}) {
            SpatialManager spatialManager(worldBounds);

            std::mt19937 rng(42);
            std::uniform_real_distribution<float> dist(-80.0f, 80.0f);

            for (int i = 0; i < entityCount; ++i) {
                EntityID entity = EntityID(i + 1);
                glm::vec3 position(dist(rng), dist(rng), dist(rng));
                spatialManager.addEntity(entity, position);
            }

            int nodeCount, maxDepth, totalEntities;
            spatialManager.getStatistics(nodeCount, maxDepth, totalEntities);

            INFO("Entities: " << entityCount <<
                 ", Nodes: " << nodeCount <<
                 ", Max Depth: " << maxDepth);

            // Memory usage should scale reasonably
            // Guide target: ~100 bytes per entity + octree overhead
            float bytesPerEntity = (nodeCount * 64 + totalEntities * 24) / float(entityCount);
            INFO("Estimated bytes per entity: " << bytesPerEntity);

            CHECK(bytesPerEntity < 200.0f); // Reasonable memory usage
        }
    }
}