#pragma once

#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/SpatialComponent.h"
#include "../game/CreatureDetectionSystem.h"
#include "../spatial/SpatialManager.h"
#include "../utils/Logger.h"
#include <chrono>
#include <vector>
#include <memory>

namespace VulkanMon {

/**
 * SpatialStressTest - Validates thread-safe spatial system under high load
 *
 * Following our development philosophy:
 * - "Simple is Powerful" - Direct stress testing without complex features
 * - "Test as we go" - Immediate validation of our thread safety implementation
 * - "Document often" - Comprehensive performance tracking and reporting
 */
class SpatialStressTest {
public:
    struct StressTestConfig {
        size_t entityCount = 200;          // Number of entities to spawn
        float worldSize = 100.0f;          // World bounds (-worldSize to +worldSize)
        float queryRadius = 10.0f;         // Detection radius for queries
        float queryFrequency = 0.2f;       // Queries per second per entity
        bool enableMovement = false;       // Whether entities move during test
        bool enableConcurrentQueries = false; // Future: test multithreaded queries
    };

    struct PerformanceMetrics {
        // Entity management
        size_t totalEntities = 0;
        size_t activeEntities = 0;

        // Spatial query performance
        float avgQueryTimeMs = 0.0f;
        float maxQueryTimeMs = 0.0f;
        float minQueryTimeMs = 999.0f;
        size_t totalQueries = 0;

        // Cache performance
        float cacheHitRate = 0.0f;
        size_t cacheSize = 0;

        // Octree performance
        size_t octreeDepth = 0;
        float avgEntitiesPerNode = 0.0f;

        // Frame timing
        float totalFrameTimeMs = 0.0f;
        float spatialSystemTimeMs = 0.0f;

        // Memory usage
        size_t memoryUsageMB = 0;
    };

private:
    World* world_ = nullptr;
    SpatialManager* spatialManager_ = nullptr;
    std::vector<EntityID> testEntities_;

    StressTestConfig config_;
    PerformanceMetrics currentMetrics_;
    PerformanceMetrics peakMetrics_;

    // Performance tracking
    std::chrono::high_resolution_clock::time_point testStartTime_;
    std::vector<float> queryTimeHistory_;
    size_t frameCount_ = 0;

public:
    SpatialStressTest(World* world, SpatialManager* spatialManager)
        : world_(world), spatialManager_(spatialManager) {
        VKMON_INFO("SpatialStressTest initialized");
    }

    ~SpatialStressTest() {
        cleanup();
    }

    // Configuration
    void setConfig(const StressTestConfig& config) { config_ = config; }
    const StressTestConfig& getConfig() const { return config_; }

    // Test lifecycle
    bool initializeTest();
    void runSingleFrame(float deltaTime);
    void completeTest();
    void cleanup();

    // Performance monitoring
    const PerformanceMetrics& getCurrentMetrics() const { return currentMetrics_; }
    const PerformanceMetrics& getPeakMetrics() const { return peakMetrics_; }

    // Reporting
    void printPerformanceReport() const;
    void printDetailedAnalysis() const;

    // Entity management
    void spawnTestEntities();
    void updateEntityPositions(float deltaTime);

    // Query patterns
    void performRadiusQueries();
    void performRegionQueries();
    void performFrustumQueries();

    // Performance analysis
    void updatePerformanceMetrics();
    void trackQueryPerformance(float queryTimeMs);
    bool isPerformanceAcceptable() const;

private:
    EntityID createTestEntity(const glm::vec3& position, float boundingRadius = 1.0f);
    glm::vec3 generateRandomPosition() const;
    void logPerformanceWarning(const std::string& metric, float currentValue, float threshold) const;
};

/**
 * StressTestRunner - Orchestrates different stress test scenarios
 */
class StressTestRunner {
public:
    enum class TestScenario {
        BASELINE_50,        // 50 entities - baseline performance
        STANDARD_200,       // 200 entities - target load
        HEAVY_500,          // 500 entities - stress test
        EXTREME_1000,       // 1000 entities - breaking point test
        CONCURRENT_QUERIES, // Multi-threaded query test (future)
        MIXED_LOAD         // Variable entity count during test
    };

    struct TestResult {
        TestScenario scenario;
        bool passed = false;
        PerformanceMetrics metrics;
        std::string failureReason;
        float testDurationSeconds = 0.0f;
    };

private:
    World* world_;
    SpatialManager* spatialManager_;
    std::vector<TestResult> results_;

public:
    StressTestRunner(World* world, SpatialManager* spatialManager)
        : world_(world), spatialManager_(spatialManager) {}

    // Test execution
    bool runTestScenario(TestScenario scenario);
    bool runAllScenarios();

    // Results
    const std::vector<TestResult>& getResults() const { return results_; }
    void printSummaryReport() const;
    void exportResultsToFile(const std::string& filename) const;

private:
    SpatialStressTest::StressTestConfig getConfigForScenario(TestScenario scenario) const;
    bool evaluateTestResult(const SpatialStressTest& test, TestScenario scenario) const;
};

} // namespace VulkanMon