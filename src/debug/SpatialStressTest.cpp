#include "SpatialStressTest.h"
#include "../spatial/LayerMask.h"
#include <random>
#include <algorithm>
#include <fstream>

namespace VulkanMon {

bool SpatialStressTest::initializeTest() {
    if (!world_ || !spatialManager_) {
        VKMON_ERROR("SpatialStressTest: Invalid world or spatial manager");
        return false;
    }

    VKMON_INFO("Initializing spatial stress test with " + std::to_string(config_.entityCount) + " entities");

    // Clear previous test data
    cleanup();

    // Reset metrics
    currentMetrics_ = PerformanceMetrics{};
    peakMetrics_ = PerformanceMetrics{};
    queryTimeHistory_.clear();
    frameCount_ = 0;

    // Spawn test entities
    spawnTestEntities();

    testStartTime_ = std::chrono::high_resolution_clock::now();

    VKMON_INFO("Stress test initialized - " + std::to_string(testEntities_.size()) + " entities spawned");
    return true;
}

void SpatialStressTest::runSingleFrame(float deltaTime) {
    auto frameStart = std::chrono::high_resolution_clock::now();

    // Update entity positions if movement is enabled
    if (config_.enableMovement) {
        updateEntityPositions(deltaTime);
    }

    // Perform spatial queries to stress the system
    auto spatialStart = std::chrono::high_resolution_clock::now();

    performRadiusQueries();
    performRegionQueries();

    auto spatialEnd = std::chrono::high_resolution_clock::now();

    // Calculate timing
    auto frameEnd = std::chrono::high_resolution_clock::now();
    currentMetrics_.totalFrameTimeMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
    currentMetrics_.spatialSystemTimeMs = std::chrono::duration<float, std::milli>(spatialEnd - spatialStart).count();

    // Update performance metrics
    updatePerformanceMetrics();

    frameCount_++;

    // Log performance warnings if thresholds exceeded
    if (currentMetrics_.spatialSystemTimeMs > 5.0f) {
        logPerformanceWarning("Spatial system time", currentMetrics_.spatialSystemTimeMs, 5.0f);
    }

    if (currentMetrics_.totalFrameTimeMs > 16.67f) { // 60 FPS threshold
        logPerformanceWarning("Frame time", currentMetrics_.totalFrameTimeMs, 16.67f);
    }
}

void SpatialStressTest::completeTest() {
    auto testEnd = std::chrono::high_resolution_clock::now();
    float testDurationMs = std::chrono::duration<float, std::milli>(testEnd - testStartTime_).count();

    VKMON_INFO("Stress test completed - Duration: " + std::to_string(testDurationMs / 1000.0f) + "s");
    VKMON_INFO("Total frames processed: " + std::to_string(frameCount_));

    printPerformanceReport();
}

void SpatialStressTest::cleanup() {
    if (!world_) return;

    // Remove all test entities
    for (EntityID entity : testEntities_) {
        world_->destroyEntity(entity);
    }
    testEntities_.clear();

    VKMON_DEBUG("Stress test cleanup completed - " + std::to_string(testEntities_.size()) + " entities removed");
}

void SpatialStressTest::spawnTestEntities() {
    testEntities_.reserve(config_.entityCount);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-config_.worldSize, config_.worldSize);
    std::uniform_real_distribution<float> radiusDist(0.5f, 2.0f);

    for (size_t i = 0; i < config_.entityCount; ++i) {
        glm::vec3 position(posDist(gen), posDist(gen), posDist(gen));
        float boundingRadius = radiusDist(gen);

        EntityID entity = createTestEntity(position, boundingRadius);
        if (entity != INVALID_ENTITY) {
            testEntities_.push_back(entity);
        }
    }

    currentMetrics_.totalEntities = testEntities_.size();
    currentMetrics_.activeEntities = testEntities_.size();
}

EntityID SpatialStressTest::createTestEntity(const glm::vec3& position, float boundingRadius) {
    EntityID entity = world_->createEntity();

    // Transform component
    Transform transform;
    transform.position = position;
    transform.scale = glm::vec3(boundingRadius);
    world_->addComponent(entity, transform);

    // Spatial component for spatial system
    SpatialComponent spatial;
    spatial.spatialLayers = LayerMask::Creatures;
    spatial.boundingRadius = boundingRadius;
    spatial.behavior = config_.enableMovement ? SpatialBehavior::DYNAMIC : SpatialBehavior::STATIC;
    world_->addComponent(entity, spatial);

    // Creature component for AI behavior
    CreatureComponent creature;
    creature.detectionRadius = config_.queryRadius;
    creature.state = CreatureState::WANDERING;
    creature.type = CreatureComponent::CreatureType::NEUTRAL;
    world_->addComponent(entity, creature);

    return entity;
}

void SpatialStressTest::updateEntityPositions(float deltaTime) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> moveDist(-1.0f, 1.0f);

    float moveSpeed = 2.0f; // units per second

    for (EntityID entity : testEntities_) {
        if (world_->hasComponent<Transform>(entity)) {
            auto& transform = world_->getComponent<Transform>(entity);

            // Random movement
            glm::vec3 movement(moveDist(gen), 0.0f, moveDist(gen));
            movement = glm::normalize(movement) * moveSpeed * deltaTime;

            transform.position += movement;

            // Keep within world bounds
            transform.position.x = std::clamp(transform.position.x, -config_.worldSize, config_.worldSize);
            transform.position.z = std::clamp(transform.position.z, -config_.worldSize, config_.worldSize);
        }
    }
}

void SpatialStressTest::performRadiusQueries() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> entityDist(0, testEntities_.size() - 1);

    // Query from 10% of entities each frame to simulate detection checks
    size_t queriesToPerform = std::max(1ul, testEntities_.size() / 10);

    for (size_t i = 0; i < queriesToPerform; ++i) {
        EntityID queryEntity = testEntities_[entityDist(gen)];

        if (world_->hasComponent<Transform>(queryEntity)) {
            auto& transform = world_->getComponent<Transform>(queryEntity);

            auto queryStart = std::chrono::high_resolution_clock::now();

            auto results = spatialManager_->queryRadius(
                transform.position,
                config_.queryRadius,
                LayerMask::Creatures
            );

            auto queryEnd = std::chrono::high_resolution_clock::now();
            float queryTimeMs = std::chrono::duration<float, std::milli>(queryEnd - queryStart).count();

            trackQueryPerformance(queryTimeMs);
            currentMetrics_.totalQueries++;
        }
    }
}

void SpatialStressTest::performRegionQueries() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-config_.worldSize * 0.8f, config_.worldSize * 0.8f);
    std::uniform_real_distribution<float> sizeDist(5.0f, 20.0f);

    // Perform a few region queries each frame
    for (int i = 0; i < 3; ++i) {
        glm::vec3 center(posDist(gen), 0.0f, posDist(gen));
        float size = sizeDist(gen);

        BoundingBox region;
        region.min = center - glm::vec3(size);
        region.max = center + glm::vec3(size);

        auto queryStart = std::chrono::high_resolution_clock::now();

        auto results = spatialManager_->queryRegion(region, LayerMask::Creatures);

        auto queryEnd = std::chrono::high_resolution_clock::now();
        float queryTimeMs = std::chrono::duration<float, std::milli>(queryEnd - queryStart).count();

        trackQueryPerformance(queryTimeMs);
        currentMetrics_.totalQueries++;
    }
}

void SpatialStressTest::performFrustumQueries() {
    // Implementation for frustum queries (camera-based)
    // This would test view frustum culling performance
    // For now, we'll focus on radius and region queries
}

void SpatialStressTest::updatePerformanceMetrics() {
    // Update current metrics
    currentMetrics_.totalEntities = testEntities_.size();
    currentMetrics_.activeEntities = testEntities_.size();

    // Get spatial manager statistics
    if (spatialManager_) {
        auto stats = spatialManager_->getPerformanceStats();
        currentMetrics_.avgQueryTimeMs = stats.averageQueryTimeMs;
        currentMetrics_.totalQueries = stats.totalQueries;
    }

    // Update peak metrics
    peakMetrics_.maxQueryTimeMs = std::max(peakMetrics_.maxQueryTimeMs, currentMetrics_.maxQueryTimeMs);
    peakMetrics_.totalFrameTimeMs = std::max(peakMetrics_.totalFrameTimeMs, currentMetrics_.totalFrameTimeMs);
    peakMetrics_.spatialSystemTimeMs = std::max(peakMetrics_.spatialSystemTimeMs, currentMetrics_.spatialSystemTimeMs);
}

void SpatialStressTest::trackQueryPerformance(float queryTimeMs) {
    queryTimeHistory_.push_back(queryTimeMs);

    // Update min/max/avg
    currentMetrics_.maxQueryTimeMs = std::max(currentMetrics_.maxQueryTimeMs, queryTimeMs);
    currentMetrics_.minQueryTimeMs = std::min(currentMetrics_.minQueryTimeMs, queryTimeMs);

    // Calculate rolling average
    if (!queryTimeHistory_.empty()) {
        float sum = 0.0f;
        for (float time : queryTimeHistory_) {
            sum += time;
        }
        currentMetrics_.avgQueryTimeMs = sum / queryTimeHistory_.size();
    }

    // Keep history manageable
    if (queryTimeHistory_.size() > 1000) {
        queryTimeHistory_.erase(queryTimeHistory_.begin(), queryTimeHistory_.begin() + 500);
    }
}

bool SpatialStressTest::isPerformanceAcceptable() const {
    // Performance thresholds
    const float MAX_FRAME_TIME_MS = 16.67f;  // 60 FPS
    const float MAX_SPATIAL_TIME_MS = 5.0f;  // 5ms for all spatial queries
    const float MAX_AVG_QUERY_TIME_MS = 0.1f; // 0.1ms per query

    return currentMetrics_.totalFrameTimeMs <= MAX_FRAME_TIME_MS &&
           currentMetrics_.spatialSystemTimeMs <= MAX_SPATIAL_TIME_MS &&
           currentMetrics_.avgQueryTimeMs <= MAX_AVG_QUERY_TIME_MS;
}

void SpatialStressTest::printPerformanceReport() const {
    VKMON_INFO("=== SPATIAL STRESS TEST PERFORMANCE REPORT ===");
    VKMON_INFO("Entities: " + std::to_string(currentMetrics_.totalEntities));
    VKMON_INFO("Total Queries: " + std::to_string(currentMetrics_.totalQueries));
    VKMON_INFO("Frames Processed: " + std::to_string(frameCount_));
    VKMON_INFO("");
    VKMON_INFO("Query Performance:");
    VKMON_INFO("  Average: " + std::to_string(currentMetrics_.avgQueryTimeMs) + "ms");
    VKMON_INFO("  Maximum: " + std::to_string(currentMetrics_.maxQueryTimeMs) + "ms");
    VKMON_INFO("  Minimum: " + std::to_string(currentMetrics_.minQueryTimeMs) + "ms");
    VKMON_INFO("");
    VKMON_INFO("Frame Performance:");
    VKMON_INFO("  Total Frame Time: " + std::to_string(currentMetrics_.totalFrameTimeMs) + "ms");
    VKMON_INFO("  Spatial System Time: " + std::to_string(currentMetrics_.spatialSystemTimeMs) + "ms");
    VKMON_INFO("");
    VKMON_INFO("Query Performance:");
    VKMON_INFO("  Total Queries: " + std::to_string(currentMetrics_.totalQueries));
    VKMON_INFO("  Average Query Time: " + std::to_string(currentMetrics_.avgQueryTimeMs) + "ms");
    VKMON_INFO("");
    VKMON_INFO("Performance Acceptable: " + std::string(isPerformanceAcceptable() ? "YES" : "NO"));
    VKMON_INFO("==============================================");
}

void SpatialStressTest::printDetailedAnalysis() const {
    VKMON_INFO("=== DETAILED PERFORMANCE ANALYSIS ===");

    if (queryTimeHistory_.size() > 100) {
        // Calculate percentiles
        auto sortedTimes = queryTimeHistory_;
        std::sort(sortedTimes.begin(), sortedTimes.end());

        size_t p50_idx = sortedTimes.size() * 0.5;
        size_t p95_idx = sortedTimes.size() * 0.95;
        size_t p99_idx = sortedTimes.size() * 0.99;

        VKMON_INFO("Query Time Percentiles:");
        VKMON_INFO("  50th: " + std::to_string(sortedTimes[p50_idx]) + "ms");
        VKMON_INFO("  95th: " + std::to_string(sortedTimes[p95_idx]) + "ms");
        VKMON_INFO("  99th: " + std::to_string(sortedTimes[p99_idx]) + "ms");
    }

    // Performance recommendations
    VKMON_INFO("");
    VKMON_INFO("Performance Recommendations:");

    if (currentMetrics_.avgQueryTimeMs > 0.1f) {
        VKMON_INFO("  - Query time high: Consider spatial optimizations");
    }

    if (currentMetrics_.totalQueries > 1000 && currentMetrics_.avgQueryTimeMs > 0.5f) {
        VKMON_INFO("  - High query frequency with slow queries: Consider spatial optimizations");
    }

    if (currentMetrics_.spatialSystemTimeMs > 5.0f) {
        VKMON_INFO("  - High spatial system overhead: Consider multithreading");
    }

    VKMON_INFO("====================================");
}

void SpatialStressTest::logPerformanceWarning(const std::string& metric, float currentValue, float threshold) const {
    VKMON_WARNING("Performance threshold exceeded - " + metric + ": " +
                 std::to_string(currentValue) + "ms (threshold: " + std::to_string(threshold) + "ms)");
}

glm::vec3 SpatialStressTest::generateRandomPosition() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-config_.worldSize, config_.worldSize);

    return glm::vec3(dist(gen), 0.0f, dist(gen));
}

// StressTestRunner Implementation

bool StressTestRunner::runTestScenario(TestScenario scenario) {
    TestResult result;
    result.scenario = scenario;

    auto testConfig = getConfigForScenario(scenario);

    VKMON_INFO("Running stress test scenario: " + std::to_string(static_cast<int>(scenario)));

    SpatialStressTest test(world_, spatialManager_);
    test.setConfig(testConfig);

    auto testStart = std::chrono::high_resolution_clock::now();

    if (!test.initializeTest()) {
        result.passed = false;
        result.failureReason = "Failed to initialize test";
        results_.push_back(result);
        return false;
    }

    // Run test for 5 seconds (300 frames at 60 FPS)
    for (int frame = 0; frame < 300; ++frame) {
        test.runSingleFrame(1.0f / 60.0f);
    }

    test.completeTest();

    auto testEnd = std::chrono::high_resolution_clock::now();
    result.testDurationSeconds = std::chrono::duration<float>(testEnd - testStart).count();

    result.metrics = test.getCurrentMetrics();
    result.passed = evaluateTestResult(test, scenario);

    if (!result.passed) {
        result.failureReason = "Performance thresholds exceeded";
    }

    results_.push_back(result);
    return result.passed;
}

SpatialStressTest::StressTestConfig StressTestRunner::getConfigForScenario(TestScenario scenario) const {
    SpatialStressTest::StressTestConfig config;

    switch (scenario) {
        case TestScenario::BASELINE_50:
            config.entityCount = 50;
            config.queryRadius = 10.0f;
            config.enableMovement = false;
            break;

        case TestScenario::STANDARD_200:
            config.entityCount = 200;
            config.queryRadius = 10.0f;
            config.enableMovement = true;
            break;

        case TestScenario::HEAVY_500:
            config.entityCount = 500;
            config.queryRadius = 15.0f;
            config.enableMovement = true;
            break;

        case TestScenario::EXTREME_1000:
            config.entityCount = 1000;
            config.queryRadius = 20.0f;
            config.enableMovement = true;
            break;

        case TestScenario::CONCURRENT_QUERIES:
            config.entityCount = 200;
            config.queryRadius = 10.0f;
            config.enableConcurrentQueries = true;
            break;

        case TestScenario::MIXED_LOAD:
            config.entityCount = 200; // Will vary during test
            config.queryRadius = 10.0f;
            config.enableMovement = true;
            break;
    }

    return config;
}

bool StressTestRunner::evaluateTestResult(const SpatialStressTest& test, TestScenario scenario) const {
    const auto& metrics = test.getCurrentMetrics();

    // Different thresholds for different scenarios
    switch (scenario) {
        case TestScenario::BASELINE_50:
            return metrics.avgQueryTimeMs <= 0.05f && metrics.totalFrameTimeMs <= 10.0f;

        case TestScenario::STANDARD_200:
            return metrics.avgQueryTimeMs <= 0.1f && metrics.totalFrameTimeMs <= 16.67f;

        case TestScenario::HEAVY_500:
            return metrics.avgQueryTimeMs <= 0.2f && metrics.totalFrameTimeMs <= 20.0f;

        case TestScenario::EXTREME_1000:
            // More lenient thresholds for extreme test
            return metrics.avgQueryTimeMs <= 0.5f && metrics.totalFrameTimeMs <= 30.0f;

        default:
            return test.isPerformanceAcceptable();
    }
}

bool StressTestRunner::runAllScenarios() {
    bool allPassed = true;

    std::vector<TestScenario> scenarios = {
        TestScenario::BASELINE_50,
        TestScenario::STANDARD_200,
        TestScenario::HEAVY_500,
        TestScenario::EXTREME_1000
    };

    for (auto scenario : scenarios) {
        bool passed = runTestScenario(scenario);
        allPassed = allPassed && passed;
    }

    printSummaryReport();
    return allPassed;
}

void StressTestRunner::printSummaryReport() const {
    VKMON_INFO("=== STRESS TEST SUMMARY REPORT ===");

    for (const auto& result : results_) {
        std::string scenarioName = "Scenario " + std::to_string(static_cast<int>(result.scenario));
        std::string status = result.passed ? "PASSED" : "FAILED";

        VKMON_INFO(scenarioName + ": " + status);
        VKMON_INFO("  Entities: " + std::to_string(result.metrics.totalEntities));
        VKMON_INFO("  Avg Query Time: " + std::to_string(result.metrics.avgQueryTimeMs) + "ms");
        VKMON_INFO("  Frame Time: " + std::to_string(result.metrics.totalFrameTimeMs) + "ms");

        if (!result.passed) {
            VKMON_INFO("  Failure: " + result.failureReason);
        }
        VKMON_INFO("");
    }

    VKMON_INFO("================================");
}

} // namespace VulkanMon