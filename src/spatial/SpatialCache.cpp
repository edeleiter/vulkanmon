#include "SpatialCache.h"
#include "SpatialManager.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <functional>
#include <cmath>

namespace VulkanMon {

float PredictiveSpatialCache::getCurrentTime() const {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float>(now - startTime).count();
}

size_t PredictiveSpatialCache::hashRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask) const {
    // Hash function for radius queries
    size_t h1 = std::hash<float>{}(center.x);
    size_t h2 = std::hash<float>{}(center.y);
    size_t h3 = std::hash<float>{}(center.z);
    size_t h4 = std::hash<float>{}(radius);
    size_t h5 = std::hash<uint32_t>{}(layerMask);

    // Combine hashes using a method that reduces collisions
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
}

size_t PredictiveSpatialCache::hashRegionQuery(const BoundingBox& region, uint32_t layerMask) const {
    size_t h1 = std::hash<float>{}(region.min.x);
    size_t h2 = std::hash<float>{}(region.min.y);
    size_t h3 = std::hash<float>{}(region.min.z);
    size_t h4 = std::hash<float>{}(region.max.x);
    size_t h5 = std::hash<float>{}(region.max.y);
    size_t h6 = std::hash<float>{}(region.max.z);
    size_t h7 = std::hash<uint32_t>{}(layerMask);

    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^ (h7 << 6);
}

size_t PredictiveSpatialCache::hashFrustumQuery(const Frustum& frustum, uint32_t layerMask) const {
    // Hash frustum planes for cache key
    size_t hash = 0;
    for (int i = 0; i < 6; ++i) {
        hash ^= std::hash<float>{}(frustum.planes[i].x) << (i * 4 + 0);
        hash ^= std::hash<float>{}(frustum.planes[i].y) << (i * 4 + 1);
        hash ^= std::hash<float>{}(frustum.planes[i].z) << (i * 4 + 2);
        hash ^= std::hash<float>{}(frustum.planes[i].w) << (i * 4 + 3);
    }
    hash ^= std::hash<uint32_t>{}(layerMask) << 24;
    return hash;
}

bool PredictiveSpatialCache::isEntryValid(const SpatialCacheEntry& entry) const {
    float currentTime = getCurrentTime();
    return (currentTime - entry.timestamp) < CACHE_TTL_SECONDS;
}

void PredictiveSpatialCache::clear() {
    cache_.clear();
    resetStatistics();
}

void PredictiveSpatialCache::cleanup() {
    evictOldEntries();
}

void PredictiveSpatialCache::evictOldEntries() {
    float currentTime = getCurrentTime();

    auto it = cache_.begin();
    while (it != cache_.end()) {
        if ((currentTime - it->second.timestamp) > CACHE_TTL_SECONDS) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }

    // If still too large, remove oldest entries
    if (cache_.size() > MAX_CACHE_SIZE) {
        std::vector<std::pair<size_t, float>> timestampedEntries;
        for (const auto& [hash, entry] : cache_) {
            timestampedEntries.emplace_back(hash, entry.timestamp);
        }

        // Sort by timestamp (oldest first)
        std::sort(timestampedEntries.begin(), timestampedEntries.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        // Remove oldest entries
        size_t entriesToRemove = cache_.size() - MAX_CACHE_SIZE;
        for (size_t i = 0; i < entriesToRemove; ++i) {
            cache_.erase(timestampedEntries[i].first);
        }
    }
}

bool PredictiveSpatialCache::tryGetRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask, std::vector<EntityID>& results) const {
    size_t hash = hashRadiusQuery(center, radius, layerMask);
    auto it = cache_.find(hash);

    if (it != cache_.end() && isEntryValid(it->second)) {
        // Validate parameters match
        if (it->second.type == SpatialCacheEntry::QueryType::RADIUS &&
            it->second.layerMask == layerMask &&
            glm::distance(it->second.params.radius.center, center) < 0.01f &&
            std::abs(it->second.params.radius.radius - radius) < 0.01f) {

            results = it->second.results;
            hitCount_++;
            return true;
        }
    }

    missCount_++;
    return false;
}

void PredictiveSpatialCache::cacheRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask, const std::vector<EntityID>& results) {
    // Periodically clean up old entries
    if (cache_.size() % 100 == 0) {
        evictOldEntries();
    }

    size_t hash = hashRadiusQuery(center, radius, layerMask);

    SpatialCacheEntry entry;
    entry.results = results;
    entry.timestamp = getCurrentTime();
    entry.layerMask = layerMask;
    entry.type = SpatialCacheEntry::QueryType::RADIUS;
    entry.params.radius.center = center;
    entry.params.radius.radius = radius;

    cache_[hash] = std::move(entry);
}

bool PredictiveSpatialCache::tryGetRegionQuery(const BoundingBox& region, uint32_t layerMask, std::vector<EntityID>& results) const {
    size_t hash = hashRegionQuery(region, layerMask);
    auto it = cache_.find(hash);

    if (it != cache_.end() && isEntryValid(it->second)) {
        if (it->second.type == SpatialCacheEntry::QueryType::REGION &&
            it->second.layerMask == layerMask) {

            const auto& cachedMin = it->second.params.region.min;
            const auto& cachedMax = it->second.params.region.max;

            // Check if bounding boxes match
            if (glm::distance(cachedMin, region.min) < 0.01f &&
                glm::distance(cachedMax, region.max) < 0.01f) {

                results = it->second.results;
                hitCount_++;
                return true;
            }
        }
    }

    missCount_++;
    return false;
}

void PredictiveSpatialCache::cacheRegionQuery(const BoundingBox& region, uint32_t layerMask, const std::vector<EntityID>& results) {
    if (cache_.size() % 100 == 0) {
        evictOldEntries();
    }

    size_t hash = hashRegionQuery(region, layerMask);

    SpatialCacheEntry entry;
    entry.results = results;
    entry.timestamp = getCurrentTime();
    entry.layerMask = layerMask;
    entry.type = SpatialCacheEntry::QueryType::REGION;
    entry.params.region.min = region.min;
    entry.params.region.max = region.max;

    cache_[hash] = std::move(entry);
}

bool PredictiveSpatialCache::tryGetFrustumQuery(const Frustum& frustum, uint32_t layerMask, std::vector<EntityID>& results) const {
    size_t hash = hashFrustumQuery(frustum, layerMask);
    auto it = cache_.find(hash);

    if (it != cache_.end() && isEntryValid(it->second)) {
        if (it->second.type == SpatialCacheEntry::QueryType::FRUSTUM &&
            it->second.layerMask == layerMask &&
            it->second.params.frustumHash == hash) {

            results = it->second.results;
            hitCount_++;
            return true;
        }
    }

    missCount_++;
    return false;
}

void PredictiveSpatialCache::cacheFrustumQuery(const Frustum& frustum, uint32_t layerMask, const std::vector<EntityID>& results) {
    if (cache_.size() % 100 == 0) {
        evictOldEntries();
    }

    size_t hash = hashFrustumQuery(frustum, layerMask);

    SpatialCacheEntry entry;
    entry.results = results;
    entry.timestamp = getCurrentTime();
    entry.layerMask = layerMask;
    entry.type = SpatialCacheEntry::QueryType::FRUSTUM;
    entry.params.frustumHash = hash;

    cache_[hash] = std::move(entry);
}

float PredictiveSpatialCache::getCacheHitRate() const {
    size_t totalQueries = hitCount_ + missCount_;
    if (totalQueries == 0) return 0.0f;
    return static_cast<float>(hitCount_) / static_cast<float>(totalQueries);
}

void PredictiveSpatialCache::prefetchNearbyQueries(const glm::vec3& center, float radius) {
    // Generate nearby query points for predictive caching
    auto nearbyPoints = generateNearbyPoints(center, radius);

    // This would require access to the spatial manager to perform actual queries
    // For now, just log that predictive caching was requested
    #ifdef DEBUG_VERBOSE
    VKMON_DEBUG("Predictive caching requested for " + std::to_string(nearbyPoints.size()) + " nearby points");
    #endif
}

std::vector<glm::vec3> PredictiveSpatialCache::generateNearbyPoints(const glm::vec3& center, float radius) const {
    std::vector<glm::vec3> points;

    // Generate points in a grid pattern around the center
    const int gridSize = 3; // 3x3 grid
    float step = PREDICTION_RADIUS / gridSize;

    for (int x = -gridSize/2; x <= gridSize/2; ++x) {
        for (int z = -gridSize/2; z <= gridSize/2; ++z) {
            if (x == 0 && z == 0) continue; // Skip center point

            glm::vec3 point = center + glm::vec3(x * step, 0, z * step);
            points.push_back(point);
        }
    }

    return points;
}

} // namespace VulkanMon