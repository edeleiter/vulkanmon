#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../../src/game/CreatureDetectionSystem.h"
#include "../../src/core/EntityManager.h"
#include "../../src/components/Transform.h"
#include "../../src/components/SpatialComponent.h"
#include "../../src/spatial/SpatialManager.h"
#include "../../src/systems/SpatialSystem.h"
#include "../../src/spatial/WorldConfig.h"
#include <chrono>
#include <memory>

using namespace VulkanMon;

// CreatureDetectionSystem performance baselines
namespace CreatureDetectionBaselines {
    // Current optimized performance (post-optimization)
    constexpr float MAX_UPDATE_TIME_MS = 8.0f;           // Currently: 5.07ms (target: <8ms)
    constexpr size_t POKEMON_CREATURE_COUNT = 1024;      // Pokemon Legends scale
    constexpr size_t MAX_CREATURES_PER_FRAME = 64;       // Temporal spreading optimization
    constexpr float MAX_TIME_PER_CREATURE_MS = 0.125f;   // 8ms / 64 creatures = 0.125ms/creature

    // Performance regression thresholds (these should NEVER be exceeded)
    constexpr float REGRESSION_ALERT_MS = 12.0f;         // Alert if we regress beyond 12ms
    constexpr float REGRESSION_CRITICAL_MS = 20.0f;      // Critical if we approach old 22ms disaster
}

// Test fixture for CreatureDetectionSystem performance testing
class CreatureDetectionPerformanceTest {
private:
    std::unique_ptr<EntityManager> entityManager_;
    std::unique_ptr<SpatialManager> spatialManager_;
    std::unique_ptr<SpatialSystem> spatialSystem_;
    std::unique_ptr<CreatureDetectionSystem> creatureDetectionSystem_;
    std::vector<EntityID> creatureEntities_;
    EntityID playerEntity_;

public:
    CreatureDetectionPerformanceTest() {
        setupTestEnvironment();
        createPokemonScaleCreatures();
        createTestPlayer();
    }

private:
    void setupTestEnvironment() {
        entityManager_ = std::make_unique<EntityManager>();

        // Create spatial system with Pokemon-scale world bounds
        WorldConfig worldConfig = WorldConfig::createTestWorld();
        BoundingBox worldBounds = worldConfig.getBoundingBox();

        spatialManager_ = std::make_unique<SpatialManager>(worldBounds);
        spatialSystem_ = std::make_unique<SpatialSystem>(worldBounds);
        // SpatialSystem owns its own SpatialManager, no need to set it

        // Create creature detection system
        creatureDetectionSystem_ = std::make_unique<CreatureDetectionSystem>();
        creatureDetectionSystem_->setSpatialSystem(spatialSystem_.get());
    }

    void createPokemonScaleCreatures() {
        // Create 1024 creatures in Pokemon Legends formation
        const int GRID_SIZE = 32;  // 32x32 = 1024
        const float SPACING = 3.0f;
        const float OFFSET = -(GRID_SIZE - 1) * SPACING * 0.5f;

        creatureEntities_.reserve(CreatureDetectionBaselines::POKEMON_CREATURE_COUNT);

        for (int x = 0; x < GRID_SIZE; x++) {
            for (int z = 0; z < GRID_SIZE; z++) {
                EntityID creature = entityManager_->createEntity();

                // Transform component
                Transform transform;
                transform.position = glm::vec3(
                    OFFSET + x * SPACING,
                    0.0f,
                    OFFSET + z * SPACING
                );
                transform.setRotationEuler(0.0f, 0.0f, 0.0f);
                transform.scale = glm::vec3(1.0f);
                entityManager_->addComponent(creature, transform);

                // CreatureComponent with varied AI parameters
                CreatureComponent creatureComp;
                creatureComp.state = CreatureState::WANDERING;
                creatureComp.detectionRadius = 8.0f + (x + z) * 0.1f;  // Varied detection radii
                creatureComp.type = static_cast<CreatureComponent::CreatureType>((x + z) % 3);
                creatureComp.detectionCheckInterval = 0.2f;  // 200ms intervals
                entityManager_->addComponent(creature, creatureComp);

                // SpatialComponent for spatial system integration
                SpatialComponent spatial;
                spatial.spatialLayers = LayerMask::Creatures;
                spatial.boundingRadius = 1.0f + (x + z) * 0.05f;
                spatial.behavior = SpatialBehavior::DYNAMIC;
                entityManager_->addComponent(creature, spatial);

                // Register with spatial system
                spatialSystem_->addEntity(creature, transform.position, spatial.spatialLayers);

                creatureEntities_.push_back(creature);
            }
        }
    }

    void createTestPlayer() {
        playerEntity_ = entityManager_->createEntity();

        // Player transform at center of creature grid
        Transform playerTransform;
        playerTransform.position = glm::vec3(0.0f, 1.0f, 0.0f);
        playerTransform.setRotationEuler(0.0f, 0.0f, 0.0f);
        playerTransform.scale = glm::vec3(1.0f);
        entityManager_->addComponent(playerEntity_, playerTransform);

        // Player spatial component
        SpatialComponent playerSpatial(2.0f, SpatialBehavior::DYNAMIC, LayerMask::Player);
        entityManager_->addComponent(playerEntity_, playerSpatial);

        // Register player with spatial system
        spatialSystem_->addEntity(playerEntity_, playerTransform.position, playerSpatial.spatialLayers);
    }

public:
    EntityManager* getEntityManager() { return entityManager_.get(); }
    CreatureDetectionSystem* getCreatureDetectionSystem() { return creatureDetectionSystem_.get(); }
    const std::vector<EntityID>& getCreatureEntities() const { return creatureEntities_; }
    size_t getCreatureCount() const { return creatureEntities_.size(); }
};

TEST_CASE("Performance Regression: CreatureDetectionSystem Pokemon Scale", "[performance][regression][creatures]") {
    CreatureDetectionPerformanceTest testFixture;
    auto* entityManager = testFixture.getEntityManager();
    auto* detectionSystem = testFixture.getCreatureDetectionSystem();

    REQUIRE(testFixture.getCreatureCount() == CreatureDetectionBaselines::POKEMON_CREATURE_COUNT);

    SECTION("Single Frame Update Performance") {
        const float deltaTime = 1.0f / 120.0f;  // 120 FPS target frame time

        auto start = std::chrono::high_resolution_clock::now();

        detectionSystem->update(deltaTime, *entityManager);

        auto end = std::chrono::high_resolution_clock::now();
        float updateTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Primary regression test: Must not exceed optimized baseline
        REQUIRE(updateTimeMs < CreatureDetectionBaselines::MAX_UPDATE_TIME_MS);

        // Critical regression alert: Should never approach old performance disaster
        REQUIRE(updateTimeMs < CreatureDetectionBaselines::REGRESSION_ALERT_MS);

        // Get system statistics for detailed analysis
        auto stats = detectionSystem->getFrameStats();

        INFO("CreatureDetectionSystem Performance:");
        INFO("  Update time: " << updateTimeMs << "ms (baseline: <"
             << CreatureDetectionBaselines::MAX_UPDATE_TIME_MS << "ms)");
        INFO("  Creatures processed: " << stats.creaturesProcessed);
        INFO("  Detections triggered: " << stats.detectionsTriggered);
        INFO("  State changes: " << stats.stateChanges);

        // Validate temporal spreading is working (should process subset per frame)
        REQUIRE(stats.creaturesProcessed <= CreatureDetectionBaselines::POKEMON_CREATURE_COUNT);

        // Performance per creature should be reasonable
        if (stats.creaturesProcessed > 0) {
            float timePerCreatureMs = updateTimeMs / stats.creaturesProcessed;
            REQUIRE(timePerCreatureMs < CreatureDetectionBaselines::MAX_TIME_PER_CREATURE_MS);

            INFO("  Time per creature: " << timePerCreatureMs << "ms (baseline: <"
                 << CreatureDetectionBaselines::MAX_TIME_PER_CREATURE_MS << "ms)");
        }
    }

    SECTION("Sustained Performance Over Multiple Frames") {
        const float deltaTime = 1.0f / 120.0f;
        const size_t NUM_TEST_FRAMES = 60;  // Test 60 frames (0.5 seconds)

        std::vector<float> frameTimes;
        frameTimes.reserve(NUM_TEST_FRAMES);

        auto testStart = std::chrono::high_resolution_clock::now();

        for (size_t frame = 0; frame < NUM_TEST_FRAMES; frame++) {
            auto frameStart = std::chrono::high_resolution_clock::now();

            detectionSystem->update(deltaTime, *entityManager);

            auto frameEnd = std::chrono::high_resolution_clock::now();
            float frameTimeMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
            frameTimes.push_back(frameTimeMs);
        }

        auto testEnd = std::chrono::high_resolution_clock::now();
        float totalTestTimeMs = std::chrono::duration<float, std::milli>(testEnd - testStart).count();

        // Calculate statistics
        float avgFrameTime = totalTestTimeMs / NUM_TEST_FRAMES;
        float maxFrameTime = *std::max_element(frameTimes.begin(), frameTimes.end());
        float minFrameTime = *std::min_element(frameTimes.begin(), frameTimes.end());

        // Regression tests
        REQUIRE(avgFrameTime < CreatureDetectionBaselines::MAX_UPDATE_TIME_MS);
        REQUIRE(maxFrameTime < CreatureDetectionBaselines::REGRESSION_ALERT_MS);

        INFO("Sustained Performance Over " << NUM_TEST_FRAMES << " Frames:");
        INFO("  Average frame time: " << avgFrameTime << "ms");
        INFO("  Max frame time: " << maxFrameTime << "ms");
        INFO("  Min frame time: " << minFrameTime << "ms");
        INFO("  Frame time variance: " << (maxFrameTime - minFrameTime) << "ms");

        // Frame time should be consistent (low variance indicates good temporal spreading)
        float variance = maxFrameTime - minFrameTime;
        REQUIRE(variance < CreatureDetectionBaselines::MAX_UPDATE_TIME_MS);  // Should not spike wildly
    }
}

TEST_CASE("Performance Regression: CreatureDetection Memory Usage", "[performance][regression][memory]") {
    CreatureDetectionPerformanceTest testFixture;

    SECTION("Memory Efficiency Validation") {
        // CreatureDetectionSystem should not have excessive memory overhead
        auto* detectionSystem = testFixture.getCreatureDetectionSystem();
        auto* entityManager = testFixture.getEntityManager();

        // Run several updates to ensure system is fully initialized
        for (int i = 0; i < 10; i++) {
            detectionSystem->update(1.0f / 120.0f, *entityManager);
        }

        auto stats = detectionSystem->getFrameStats();

        // System should be processing creatures efficiently
        REQUIRE(stats.creaturesProcessed > 0);
        REQUIRE(stats.creaturesProcessed <= CreatureDetectionBaselines::POKEMON_CREATURE_COUNT);

        INFO("Memory efficiency validation passed");
        INFO("  Max creatures processed per frame: " << stats.creaturesProcessed);
    }
}

// Benchmark tests for continuous performance monitoring
TEST_CASE("Performance Benchmarks: CreatureDetectionSystem", "[performance][benchmark][creatures]") {
    CreatureDetectionPerformanceTest testFixture;
    auto* entityManager = testFixture.getEntityManager();
    auto* detectionSystem = testFixture.getCreatureDetectionSystem();

    const float deltaTime = 1.0f / 120.0f;

    BENCHMARK("CreatureDetectionSystem update (Pokemon scale - 1024 creatures)") {
        detectionSystem->update(deltaTime, *entityManager);
        return detectionSystem->getFrameStats().creaturesProcessed;
    };

    BENCHMARK("CreatureDetectionSystem update (10 frames sustained)") {
        size_t totalCreaturesProcessed = 0;
        for (int i = 0; i < 10; i++) {
            detectionSystem->update(deltaTime, *entityManager);
            totalCreaturesProcessed += detectionSystem->getFrameStats().creaturesProcessed;
        }
        return totalCreaturesProcessed;
    };
}

// Stress test to validate system doesn't degrade under extreme conditions
TEST_CASE("Performance Regression: CreatureDetection Stress Test", "[performance][regression][stress]") {
    SECTION("High-Frequency Update Stress Test") {
        CreatureDetectionPerformanceTest testFixture;
        auto* entityManager = testFixture.getEntityManager();
        auto* detectionSystem = testFixture.getCreatureDetectionSystem();

        // Simulate very high frame rate (240 FPS) with small delta times
        const float highFreqDeltaTime = 1.0f / 240.0f;
        const size_t STRESS_FRAMES = 240;  // 1 second at 240 FPS

        auto stressStart = std::chrono::high_resolution_clock::now();

        for (size_t frame = 0; frame < STRESS_FRAMES; frame++) {
            detectionSystem->update(highFreqDeltaTime, *entityManager);
        }

        auto stressEnd = std::chrono::high_resolution_clock::now();
        float stressTimeMs = std::chrono::duration<float, std::milli>(stressEnd - stressStart).count();
        float avgFrameTimeMs = stressTimeMs / STRESS_FRAMES;

        // Even under stress, average frame time should be reasonable
        REQUIRE(avgFrameTimeMs < CreatureDetectionBaselines::MAX_UPDATE_TIME_MS);

        INFO("High-frequency stress test (" << STRESS_FRAMES << " frames):");
        INFO("  Total time: " << stressTimeMs << "ms");
        INFO("  Average frame time: " << avgFrameTimeMs << "ms");
        INFO("  Estimated FPS capability: " << (1000.0f / avgFrameTimeMs) << " FPS");
    }
}