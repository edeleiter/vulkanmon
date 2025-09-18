#pragma once

#include "../core/Entity.h"
#include "LayerMask.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <variant>

namespace VulkanMon {

// Forward declarations
struct BoundingBox;
struct Frustum;

struct SpatialCacheEntry {
    std::vector<EntityID> results;
    float timestamp;
    uint32_t layerMask;

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
    SpatialCacheEntry() : timestamp(0.0f), layerMask(0), params(FrustumQuery(0)) {}
};

class PredictiveSpatialCache {
private:
    static constexpr float CACHE_TTL_SECONDS = 0.1f; // 100ms cache lifetime
    static constexpr size_t MAX_CACHE_SIZE = 1000;   // Maximum cached entries
    static constexpr float PREDICTION_RADIUS = 20.0f; // Radius for predictive caching

    std::unordered_map<size_t, SpatialCacheEntry> cache_;
    mutable size_t hitCount_ = 0;
    mutable size_t missCount_ = 0;

public:
    PredictiveSpatialCache() = default;
    ~PredictiveSpatialCache() = default;

    // Cache management
    void clear();
    void cleanup(); // Remove expired entries

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
    size_t getCacheSize() const { return cache_.size(); }
    void resetStatistics() { hitCount_ = 0; missCount_ = 0; }

private:
    float getCurrentTime() const;
    size_t hashRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask) const;
    size_t hashRegionQuery(const BoundingBox& region, uint32_t layerMask) const;
    size_t hashFrustumQuery(const Frustum& frustum, uint32_t layerMask) const;

    bool isEntryValid(const SpatialCacheEntry& entry) const;
    void evictOldEntries();

    // Predictive caching helpers
    std::vector<glm::vec3> generateNearbyPoints(const glm::vec3& center, float radius) const;
};

} // namespace VulkanMon