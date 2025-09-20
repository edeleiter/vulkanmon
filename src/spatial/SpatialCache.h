#pragma once

#include "../core/Entity.h"
#include "LayerMask.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <variant>
#include <atomic>
#include <memory>

namespace VulkanMon {

// Forward declarations
struct BoundingBox;
struct Frustum;

struct SpatialCacheEntry {
    std::vector<EntityID> results;
    float timestamp;
    uint32_t layerMask;

    // Spatial region covered by this cache entry (for targeted invalidation)
    glm::vec3 regionMin;
    glm::vec3 regionMax;

    // LRU tracking for intelligent eviction (mutable for const method updates)
    mutable float lastAccessTime;
    mutable float creationTime;
    mutable size_t accessCount;

    // Query parameters for type-safe validation (replaces unsafe union)
    struct RadiusQuery {
        glm::vec3 center;
        float radius;

        RadiusQuery(const glm::vec3& c, float r) : center(c), radius(r) {}
    };

    struct RegionQuery {
        glm::vec3 min;
        glm::vec3 max;

        RegionQuery(const glm::vec3& minBounds, const glm::vec3& maxBounds)
            : min(minBounds), max(maxBounds) {}
    };

    struct FrustumQuery {
        size_t frustumHash;

        FrustumQuery(size_t hash) : frustumHash(hash) {}
    };

    // Type-safe variant instead of unsafe union
    std::variant<RadiusQuery, RegionQuery, FrustumQuery> params;

    // Default constructor with proper initialization
    SpatialCacheEntry() : timestamp(0.0f), layerMask(0),
                          regionMin(0.0f), regionMax(0.0f),
                          lastAccessTime(0.0f), creationTime(0.0f), accessCount(0),
                          params(FrustumQuery(0)) {}
};

// SINGLE-THREADED SPATIAL CACHE: Optimized for main-thread ECS systems
//
// THREADING MODEL DOCUMENTATION:
// - This cache is designed for SINGLE-THREADED use only (main thread)
// - No atomic operations or mutexes - simple std::unique_ptr and direct access
// - Safe because VulkanMon ECS systems run on main thread sequentially
// - Performance optimized: zero synchronization overhead
// - If multi-threading needed in future, consider separate cache per thread
//   or implement proper lock-free double buffering with atomic swaps
struct SpatialCacheData {
    std::unordered_map<size_t, SpatialCacheEntry> cache;
    size_t hitCount = 0;
    size_t missCount = 0;
    float timestamp = 0.0f;

    // Detailed tracking for production metrics (mutable for const method access)
    mutable size_t totalEvictions = 0;
    mutable size_t radiusQueryCount = 0;
    mutable size_t regionQueryCount = 0;
    mutable size_t frustumQueryCount = 0;
    mutable float totalQueryTime = 0.0f;
    mutable size_t queryTimeCount = 0;
};

// PREDICTIVE SPATIAL CACHE
//
// THREAD SAFETY: SINGLE-THREADED ONLY
// This class is NOT thread-safe and must only be accessed from the main thread.
// All ECS systems in VulkanMon run sequentially on the main thread, making this safe.
//
// DESIGN PHILOSOPHY: "Simple is Powerful"
// - Single unified hash approach instead of complex mixed systems
// - Documented constants replace magic numbers
// - Regional invalidation instead of nuclear cache clearing
// - LRU eviction for intelligent memory management
//
// PERFORMANCE OPTIMIZATIONS:
// - Adaptive hash precision: 5 decimal places for small values, 3 for large
// - Regional invalidation: Only affects spatially-relevant cache entries
// - Memory pressure triggers: Cleanup at 80% capacity, 50MB limit for Pokemon overworld
// - Production-grade error handling with graceful degradation
//
// POKEMON-SPECIFIC TUNING:
// - 10.0f influence radius for creature interactions
// - 1000 entry limit optimal for 125-creature scenes
// - 20% collision rate threshold for good hash distribution
class PredictiveSpatialCache {
private:
    static constexpr float CACHE_TTL_SECONDS = 1.0f; // 1 second cache lifetime for static scenes
    static constexpr size_t MAX_CACHE_SIZE = 1000;   // Maximum cached entries
    static constexpr float PREDICTION_RADIUS = 20.0f; // Radius for predictive caching

    // Memory and health limits (Pokemon-optimized values)
    static constexpr size_t MAX_CACHE_ENTRIES = 1000;         // Optimal for 125-creature scenes
    static constexpr size_t MAX_MEMORY_BYTES = 50 * 1024 * 1024; // 50MB for Pokemon overworld
    static constexpr float MAX_COLLISION_RATE = 0.2f;         // 20% collision rate (good hash distribution)
    static constexpr float MAX_CHAIN_LENGTH = 2.0f;           // Max 2x entries per hash bucket
    static constexpr float MEMORY_PRESSURE_THRESHOLD = 0.8f;  // Trigger cleanup at 80% capacity

    // Hash precision constants (adaptive quantization)
    static constexpr float HIGH_PRECISION_THRESHOLD = 0.01f;   // Values below this get high precision
    static constexpr float MEDIUM_PRECISION_THRESHOLD = 1.0f;  // Values below this get medium precision
    static constexpr float HIGH_PRECISION_FACTOR = 100000.0f;  // 5 decimal places for small values
    static constexpr float MEDIUM_PRECISION_FACTOR = 10000.0f; // 4 decimal places for medium values
    static constexpr float STANDARD_PRECISION_FACTOR = 1000.0f; // 3 decimal places for large values

    // SINGLE-THREADED DESIGN: Simple and fast for main-thread spatial queries
    std::unique_ptr<SpatialCacheData> cacheData_;

public:
    PredictiveSpatialCache() {
        // Initialize single-threaded cache (simplified from misleading lock-free design)
        cacheData_ = std::make_unique<SpatialCacheData>();
    }

    ~PredictiveSpatialCache() = default;

    // Cache management
    void clear();
    void cleanup(); // Remove expired entries

    // Regional invalidation - targeted cache clearing for entity movement
    void invalidateRegion(const glm::vec3& regionMin, const glm::vec3& regionMax);
    void invalidateEntityMovement(const glm::vec3& oldPos, const glm::vec3& newPos, float radius = 5.0f);

    // Query cache methods
    bool tryGetRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask, std::vector<EntityID>& results) const;
    void cacheRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask, const std::vector<EntityID>& results);

    bool tryGetRegionQuery(const BoundingBox& region, uint32_t layerMask, std::vector<EntityID>& results) const;
    void cacheRegionQuery(const BoundingBox& region, uint32_t layerMask, const std::vector<EntityID>& results);

    bool tryGetFrustumQuery(const Frustum& frustum, uint32_t layerMask, std::vector<EntityID>& results) const;
    void cacheFrustumQuery(const Frustum& frustum, uint32_t layerMask, const std::vector<EntityID>& results);

    // Predictive caching - pre-populate cache with nearby queries
    void prefetchNearbyQueries(const glm::vec3& center, float radius);

    // Performance statistics
    float getCacheHitRate() const;
    size_t getCacheSize() const {
        return cacheData_->cache.size();
    }
    void resetStatistics() {
        cacheData_->hitCount = 0;
        cacheData_->missCount = 0;
    }

    // Hash analysis and collision detection
    struct HashAnalysis {
        size_t totalEntries = 0;
        size_t uniqueHashes = 0;
        size_t collisions = 0;
        float averageChainLength = 0.0f;
        float collisionRate = 0.0f;
    };
    HashAnalysis analyzeHashDistribution() const;

    // Production-grade metrics and monitoring
    struct DetailedCacheMetrics {
        float hitRate = 0.0f;
        float missRate = 0.0f;
        size_t totalQueries = 0;
        float averageQueryTime = 0.0f;
        size_t cacheSize = 0;
        size_t memoryUsageBytes = 0;
        float evictionRate = 0.0f;
        size_t totalEvictions = 0;

        // Query type breakdown
        size_t radiusQueries = 0;
        size_t regionQueries = 0;
        size_t frustumQueries = 0;

        // LRU metrics
        float averageAccessCount = 0.0f;
        float averageEntryAge = 0.0f;

        // Performance alerts
        bool hitRateAlert = false;        // Hit rate dropped below threshold
        bool memoryAlert = false;         // Memory usage too high
        bool collisionAlert = false;      // Hash collision rate too high
    };
    DetailedCacheMetrics getDetailedMetrics() const;

    // Error handling and memory management
    enum class CacheResult { Hit, Miss, Error, MemoryFull, CorruptionDetected };

    struct CacheHealth {
        bool memoryHealthy = true;
        bool hashDistributionHealthy = true;
        float averageChainLength = 0.0f;
        size_t totalCollisions = 0;
        size_t memoryUsageBytes = 0;
        bool requiresReset = false;
    };

    CacheResult tryGetFrustumQuerySafe(const Frustum& frustum, uint32_t layerMask, std::vector<EntityID>& results) const;
    bool hasCapacity() const;
    void enforceMemoryLimits();
    CacheHealth checkCacheHealth() const;
    bool resetIfCorrupted();

private:
    float getCurrentTime() const;
    size_t hashRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask) const;
    size_t hashRegionQuery(const BoundingBox& region, uint32_t layerMask) const;
    size_t hashFrustumQuery(const Frustum& frustum, uint32_t layerMask) const;

    bool isEntryValid(const SpatialCacheEntry& entry) const;
    void evictOldEntries();

    // LRU eviction strategy
    void evictLRUEntries(size_t targetCount);
    void evictBasedOnMemoryPressure();
    void updateEntryAccess(SpatialCacheEntry& entry) const;

    // Regional invalidation helpers
    bool regionsIntersect(const glm::vec3& min1, const glm::vec3& max1,
                         const glm::vec3& min2, const glm::vec3& max2) const;
    glm::vec3 calculateQueryRegionMin(const std::variant<SpatialCacheEntry::RadiusQuery,
                                                        SpatialCacheEntry::RegionQuery,
                                                        SpatialCacheEntry::FrustumQuery>& params) const;
    glm::vec3 calculateQueryRegionMax(const std::variant<SpatialCacheEntry::RadiusQuery,
                                                        SpatialCacheEntry::RegionQuery,
                                                        SpatialCacheEntry::FrustumQuery>& params) const;

    // Hash function helpers (simplified approach)
    size_t improvedHashFunction(const float* values, size_t count, uint32_t layerMask) const;

    // Predictive caching helpers
    std::vector<glm::vec3> generateNearbyPoints(const glm::vec3& center, float radius) const;
};

} // namespace VulkanMon