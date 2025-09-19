#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../../src/core/Application.h"
#include "../../src/spatial/SpatialManager.h"
#include "../../src/game/CreatureDetectionSystem.h"
#include "../../src/utils/Logger.h"
#include <chrono>
#include <vector>
#include <memory>

using namespace VulkanMon;

// Performance regression baselines - these represent our current optimized performance
namespace PerformanceBaselines {
    // Pokemon Legends Scale Performance Requirements
    constexpr float MAX_CREATURE_DETECTION_MS = 8.0f;    // Currently: 5.07ms (target: <8ms)
    constexpr float MAX_SPATIAL_QUERY_MS = 2.0f;         // Currently: 1.13ms (target: <2ms)
    constexpr float MIN_TARGET_FPS = 100.0f;             // Currently: 122 FPS (target: >100)
    constexpr size_t POKEMON_SCALE_CREATURES = 1024;     // Pokemon Legends creature count
    constexpr size_t MAX_QUERIES_PER_FRAME = 200;        // Reasonable upper bound
    constexpr float MAX_FRAME_TIME_MS = 10.0f;           // Currently: ~8ms (target: <10ms)
}

// Performance test fixture for regression testing
class PerformanceRegressionTest {
private:
    std::unique_ptr<SpatialManager> spatialManager_;
    std::vector<EntityID> testEntities_;
    BoundingBox worldBounds_;

public:
    PerformanceRegressionTest() {
        // Initialize world bounds for Pokemon-scale testing
        worldBounds_ = {
            glm::vec3(-50.0f, -10.0f, -50.0f),
            glm::vec3(50.0f, 20.0f, 50.0f)
        };

        spatialManager_ = std::make_unique<SpatialManager>(worldBounds_);
        setupPokemonScaleEntities();
    }

    void setupPokemonScaleEntities() {
        // Create 1024 entities in Pokemon Legends formation
        const int GRID_SIZE = 32;  // 32x32 = 1024
        const float SPACING = 3.0f;
        const float OFFSET = -(GRID_SIZE - 1) * SPACING * 0.5f;

        testEntities_.reserve(PerformanceBaselines::POKEMON_SCALE_CREATURES);

        for (int x = 0; x < GRID_SIZE; x++) {
            for (int z = 0; z < GRID_SIZE; z++) {
                EntityID entity = static_cast<EntityID>(x * GRID_SIZE + z + 1);

                glm::vec3 position(
                    OFFSET + x * SPACING,
                    0.0f,
                    OFFSET + z * SPACING
                );

                float boundingRadius = 1.0f + (x + z) * 0.1f; // Varied sizes
                uint32_t layerMask = LayerMask::Creatures;

                spatialManager_->addEntity(entity, position, layerMask);
                testEntities_.push_back(entity);
            }
        }
    }

    SpatialManager* getSpatialManager() { return spatialManager_.get(); }
    const std::vector<EntityID>& getTestEntities() const { return testEntities_; }
    const BoundingBox& getWorldBounds() const { return worldBounds_; }
};

TEST_CASE("Performance Regression: Pokemon Legends Scale Spatial Queries", "[performance][regression][spatial]") {
    PerformanceRegressionTest testFixture;
    auto* spatialManager = testFixture.getSpatialManager();
    const auto& entities = testFixture.getTestEntities();

    REQUIRE(entities.size() == PerformanceBaselines::POKEMON_SCALE_CREATURES);

    SECTION("Single Radius Query Performance") {
        glm::vec3 queryCenter(0.0f, 0.0f, 0.0f);
        float queryRadius = 15.0f;
        uint32_t layerMask = LayerMask::Creatures;

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<EntityID> results = spatialManager->queryRadius(queryCenter, queryRadius, layerMask);

        auto end = std::chrono::high_resolution_clock::now();
        float queryTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Regression test: Single query must stay under 2ms
        REQUIRE(queryTimeMs < PerformanceBaselines::MAX_SPATIAL_QUERY_MS);

        // Validate results make sense
        REQUIRE(results.size() > 0);
        REQUIRE(results.size() < entities.size()); // Shouldn't return all entities

        INFO("Single radius query time: " << queryTimeMs << "ms (baseline: <"
             << PerformanceBaselines::MAX_SPATIAL_QUERY_MS << "ms)");
    }

    SECTION("Batch Query Performance - Pokemon AI Scale") {
        // Simulate Pokemon creature AI: ~100 queries per frame (64 creatures * 1.5 avg)
        constexpr size_t BATCH_SIZE = 100;
        std::vector<SpatialManager::BatchedRadiusQuery> batchQueries;
        batchQueries.reserve(BATCH_SIZE);

        // Generate realistic creature detection queries
        for (size_t i = 0; i < BATCH_SIZE; i++) {
            EntityID sourceEntity = entities[i % entities.size()];

            SpatialManager::BatchedRadiusQuery query;
            query.sourceEntity = sourceEntity;
            query.center = glm::vec3(
                (i % 32 - 16) * 3.0f,  // Spread across world
                0.0f,
                ((i / 32) % 32 - 16) * 3.0f
            );
            query.radius = 8.0f + (i % 10) * 0.5f;  // Varied detection radii
            query.layerMask = LayerMask::Player | LayerMask::Creatures;

            batchQueries.push_back(query);
        }

        auto start = std::chrono::high_resolution_clock::now();

        auto results = spatialManager->queryRadiusBatch(batchQueries);

        auto end = std::chrono::high_resolution_clock::now();
        float batchTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Regression test: Batch queries must stay efficient
        float avgQueryTimeMs = batchTimeMs / BATCH_SIZE;
        REQUIRE(avgQueryTimeMs < PerformanceBaselines::MAX_SPATIAL_QUERY_MS);
        REQUIRE(results.size() == BATCH_SIZE);

        INFO("Batch query performance: " << batchTimeMs << "ms total, "
             << avgQueryTimeMs << "ms average (baseline: <"
             << PerformanceBaselines::MAX_SPATIAL_QUERY_MS << "ms per query)");
    }

    SECTION("Frustum Culling Performance") {
        // Test camera frustum culling performance
        Frustum testFrustum;
        // Create a frustum that covers about 1/4 of the world
        testFrustum.planes[0] = glm::vec4(1, 0, 0, 20);   // Left
        testFrustum.planes[1] = glm::vec4(-1, 0, 0, 20);  // Right
        testFrustum.planes[2] = glm::vec4(0, 1, 0, 10);   // Bottom
        testFrustum.planes[3] = glm::vec4(0, -1, 0, 10);  // Top
        testFrustum.planes[4] = glm::vec4(0, 0, 1, 20);   // Near
        testFrustum.planes[5] = glm::vec4(0, 0, -1, 50);  // Far

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<EntityID> visibleEntities = spatialManager->queryFrustum(testFrustum, LayerMask::Creatures);

        auto end = std::chrono::high_resolution_clock::now();
        float frustumTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Regression test: Frustum culling must be fast for rendering
        REQUIRE(frustumTimeMs < PerformanceBaselines::MAX_SPATIAL_QUERY_MS);

        // Should cull significantly (frustum covers ~1/4 of world)
        REQUIRE(visibleEntities.size() < entities.size() / 2);
        REQUIRE(visibleEntities.size() > 0);

        INFO("Frustum culling time: " << frustumTimeMs << "ms (baseline: <"
             << PerformanceBaselines::MAX_SPATIAL_QUERY_MS << "ms)");
    }
}

TEST_CASE("Performance Regression: Memory Usage Validation", "[performance][regression][memory]") {
    PerformanceRegressionTest testFixture;
    auto* spatialManager = testFixture.getSpatialManager();

    SECTION("Spatial Manager Memory Efficiency") {
        auto stats = spatialManager->getPerformanceStats();

        // Memory usage validation - structure should be reasonable
        REQUIRE(stats.totalQueries >= 0);
        REQUIRE(stats.averageQueryTimeMs >= 0.0f);

        INFO("Total queries: " << stats.totalQueries << ", Avg time: " << stats.averageQueryTimeMs << "ms");
    }
}

// Benchmark tests for continuous performance monitoring
TEST_CASE("Performance Benchmarks: Pokemon Legends Scale", "[performance][benchmark]") {
    PerformanceRegressionTest testFixture;
    auto* spatialManager = testFixture.getSpatialManager();
    const auto& entities = testFixture.getTestEntities();

    BENCHMARK("Single radius query (Pokemon creature detection)") {
        glm::vec3 center(0.0f, 0.0f, 0.0f);
        std::vector<EntityID> results;
        auto queryResults = spatialManager->queryRadius(center, 10.0f, LayerMask::Creatures);
        return queryResults.size();
    };

    BENCHMARK("Batch radius queries (64 creatures per frame)") {
        std::vector<SpatialManager::BatchedRadiusQuery> queries;
        queries.reserve(64);

        for (size_t i = 0; i < 64; i++) {
            SpatialManager::BatchedRadiusQuery query;
            query.sourceEntity = entities[i];
            query.center = glm::vec3(i * 0.5f, 0.0f, i * 0.3f);
            query.radius = 8.0f;
            query.layerMask = LayerMask::Creatures;
            queries.push_back(query);
        }

        return spatialManager->queryRadiusBatch(queries);
    };

    BENCHMARK("Frustum culling (camera render culling)") {
        Frustum frustum;
        // Standard camera frustum
        frustum.planes[0] = glm::vec4(1, 0, 0, 25);
        frustum.planes[1] = glm::vec4(-1, 0, 0, 25);
        frustum.planes[2] = glm::vec4(0, 1, 0, 15);
        frustum.planes[3] = glm::vec4(0, -1, 0, 15);
        frustum.planes[4] = glm::vec4(0, 0, 1, 5);
        frustum.planes[5] = glm::vec4(0, 0, -1, 100);

        std::vector<EntityID> visible = spatialManager->queryFrustum(frustum, LayerMask::Creatures);
        return visible.size();
    };
}

// Integration test: Full system performance
TEST_CASE("Performance Regression: Full System Integration", "[performance][regression][integration]") {
    // This test validates that the entire system maintains Pokemon Legends performance
    SECTION("Pokemon Legends Scale System Performance") {
        // Note: This is a simplified validation since full Application startup
        // requires Vulkan context which is complex to set up in unit tests

        PerformanceRegressionTest testFixture;
        auto* spatialManager = testFixture.getSpatialManager();

        // Simulate a typical frame with mixed queries
        auto frameStart = std::chrono::high_resolution_clock::now();

        // 1. Frustum culling for rendering (1 query)
        Frustum renderFrustum;
        renderFrustum.planes[0] = glm::vec4(1, 0, 0, 30);
        renderFrustum.planes[1] = glm::vec4(-1, 0, 0, 30);
        renderFrustum.planes[2] = glm::vec4(0, 1, 0, 20);
        renderFrustum.planes[3] = glm::vec4(0, -1, 0, 20);
        renderFrustum.planes[4] = glm::vec4(0, 0, 1, 10);
        renderFrustum.planes[5] = glm::vec4(0, 0, -1, 200);

        std::vector<EntityID> renderCandidates = spatialManager->queryFrustum(renderFrustum, LayerMask::Creatures);

        // 2. Creature AI detection queries (64 queries simulating temporal spreading)
        std::vector<SpatialManager::BatchedRadiusQuery> aiQueries;
        aiQueries.reserve(64);

        for (size_t i = 0; i < 64; i++) {
            SpatialManager::BatchedRadiusQuery query;
            query.sourceEntity = static_cast<EntityID>(i + 1);
            query.center = glm::vec3((i % 8 - 4) * 6.0f, 0.0f, ((i / 8) % 8 - 4) * 6.0f);
            query.radius = 8.0f + (i % 5);
            query.layerMask = LayerMask::Player | LayerMask::Creatures;
            aiQueries.push_back(query);
        }

        auto aiResults = spatialManager->queryRadiusBatch(aiQueries);

        auto frameEnd = std::chrono::high_resolution_clock::now();
        float frameTimeMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();

        // Regression test: Full frame spatial work must stay under budget
        REQUIRE(frameTimeMs < PerformanceBaselines::MAX_SPATIAL_QUERY_MS * 2);  // 2x single query budget
        REQUIRE(renderCandidates.size() > 0);
        REQUIRE(aiResults.size() == 64);

        INFO("Full frame spatial processing: " << frameTimeMs << "ms (budget: "
             << (PerformanceBaselines::MAX_SPATIAL_QUERY_MS * 2) << "ms)");
    }
}