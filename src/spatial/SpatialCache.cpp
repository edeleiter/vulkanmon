#include "SpatialCache.h"
#include "SpatialManager.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <functional>
#include <cmath>
#include <limits>

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
    // Simple approach: flatten frustum planes and use unified hash function
    float values[24]; // 6 planes * 4 components each
    for (int i = 0; i < 6; ++i) {
        values[i * 4 + 0] = frustum.planes[i].x;
        values[i * 4 + 1] = frustum.planes[i].y;
        values[i * 4 + 2] = frustum.planes[i].z;
        values[i * 4 + 3] = frustum.planes[i].w;
    }
    return improvedHashFunction(values, 24, layerMask);
}

bool PredictiveSpatialCache::isEntryValid(const SpatialCacheEntry& entry) const {
    float currentTime = getCurrentTime();
    return (currentTime - entry.timestamp) < CACHE_TTL_SECONDS;
}

void PredictiveSpatialCache::clear() {
    // Clear single-threaded cache
    cacheData_->cache.clear();
    cacheData_->hitCount = 0;
    cacheData_->missCount = 0;
    cacheData_->timestamp = getCurrentTime();
}

void PredictiveSpatialCache::cleanup() {
    evictOldEntries();
}

void PredictiveSpatialCache::evictOldEntries() {
    float currentTime = getCurrentTime();

    // First pass: remove expired entries
    auto it = cacheData_->cache.begin();
    while (it != cacheData_->cache.end()) {
        if ((currentTime - it->second.timestamp) > CACHE_TTL_SECONDS) {
            it = cacheData_->cache.erase(it);
        } else {
            ++it;
        }
    }

    // Second pass: use LRU eviction if still over limit
    if (cacheData_->cache.size() > MAX_CACHE_SIZE) {
        evictLRUEntries(MAX_CACHE_SIZE);
    }

    // Third pass: handle memory pressure
    evictBasedOnMemoryPressure();
}

bool PredictiveSpatialCache::tryGetRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask, std::vector<EntityID>& results) const {
    // Track metrics correctly: every call increments both query type AND total
    cacheData_->radiusQueryCount++;

    size_t hash = hashRadiusQuery(center, radius, layerMask);
    auto it = cacheData_->cache.find(hash);

    if (it != cacheData_->cache.end() && isEntryValid(it->second)) {
        // Validate parameters match (type-safe with std::variant)
        if (std::holds_alternative<SpatialCacheEntry::RadiusQuery>(it->second.params) &&
            it->second.layerMask == layerMask) {

            const auto& radiusQuery = std::get<SpatialCacheEntry::RadiusQuery>(it->second.params);
            if (glm::distance(radiusQuery.center, center) < 0.01f &&
                std::abs(radiusQuery.radius - radius) < 0.01f) {

                results = it->second.results;
                updateEntryAccess(it->second);
                cacheData_->hitCount++;
                return true;
            }
        }
    }

    cacheData_->missCount++;
    return false;
}

void PredictiveSpatialCache::cacheRadiusQuery(const glm::vec3& center, float radius, uint32_t layerMask, const std::vector<EntityID>& results) {
    // Check capacity before adding
    if (!hasCapacity()) {
        enforceMemoryLimits();

        // If still no capacity after cleanup, skip caching
        if (!hasCapacity()) {
            return;
        }
    }

    // Periodically clean up old entries and check health
    if (cacheData_->cache.size() % 100 == 0) {
        evictOldEntries();
        resetIfCorrupted(); // Auto-reset if cache becomes unhealthy
    }

    size_t hash = hashRadiusQuery(center, radius, layerMask);

    SpatialCacheEntry entry;
    entry.results = results;
    entry.timestamp = getCurrentTime();
    entry.layerMask = layerMask;
    entry.params = SpatialCacheEntry::RadiusQuery(center, radius);

    // Calculate spatial region for targeted invalidation
    entry.regionMin = center - glm::vec3(radius);
    entry.regionMax = center + glm::vec3(radius);

    // Initialize LRU tracking
    float currentTime = getCurrentTime();
    entry.creationTime = currentTime;
    entry.lastAccessTime = currentTime;
    entry.accessCount = 1;

    // Detect hash collision before overwriting
    if (cacheData_->cache.find(hash) != cacheData_->cache.end()) {
        #ifdef DEBUG_VERBOSE
        VKMON_WARNING("Radius query cache hash collision - overwriting existing entry");
        #endif
    }

    cacheData_->cache[hash] = std::move(entry);
}

bool PredictiveSpatialCache::tryGetRegionQuery(const BoundingBox& region, uint32_t layerMask, std::vector<EntityID>& results) const {
    // Track metrics correctly
    cacheData_->regionQueryCount++;

    size_t hash = hashRegionQuery(region, layerMask);
    auto it = cacheData_->cache.find(hash);

    if (it != cacheData_->cache.end() && isEntryValid(it->second)) {
        if (std::holds_alternative<SpatialCacheEntry::RegionQuery>(it->second.params) &&
            it->second.layerMask == layerMask) {

            const auto& regionQuery = std::get<SpatialCacheEntry::RegionQuery>(it->second.params);
            const auto& cachedMin = regionQuery.min;
            const auto& cachedMax = regionQuery.max;

            // Check if bounding boxes match
            if (glm::distance(cachedMin, region.min) < 0.01f &&
                glm::distance(cachedMax, region.max) < 0.01f) {

                results = it->second.results;
                updateEntryAccess(it->second);
                cacheData_->hitCount++;
                return true;
            }
        }
    }

    cacheData_->missCount++;
    return false;
}

void PredictiveSpatialCache::cacheRegionQuery(const BoundingBox& region, uint32_t layerMask, const std::vector<EntityID>& results) {
    // Check capacity and enforce limits
    if (!hasCapacity()) {
        enforceMemoryLimits();
        if (!hasCapacity()) {
            return; // Skip caching if still no capacity
        }
    }

    if (cacheData_->cache.size() % 100 == 0) {
        evictOldEntries();
        resetIfCorrupted();
    }

    size_t hash = hashRegionQuery(region, layerMask);

    SpatialCacheEntry entry;
    entry.results = results;
    entry.timestamp = getCurrentTime();
    entry.layerMask = layerMask;
    entry.params = SpatialCacheEntry::RegionQuery(region.min, region.max);

    // Calculate spatial region for targeted invalidation
    entry.regionMin = region.min;
    entry.regionMax = region.max;

    // Initialize LRU tracking
    float currentTime = getCurrentTime();
    entry.creationTime = currentTime;
    entry.lastAccessTime = currentTime;
    entry.accessCount = 1;

    // Detect hash collision before overwriting
    if (cacheData_->cache.find(hash) != cacheData_->cache.end()) {
        #ifdef DEBUG_VERBOSE
        VKMON_WARNING("Region query cache hash collision - overwriting existing entry");
        #endif
    }

    cacheData_->cache[hash] = std::move(entry);
}

bool PredictiveSpatialCache::tryGetFrustumQuery(const Frustum& frustum, uint32_t layerMask, std::vector<EntityID>& results) const {
    // Track metrics correctly
    cacheData_->frustumQueryCount++;

    size_t hash = hashFrustumQuery(frustum, layerMask);
    auto it = cacheData_->cache.find(hash);

    if (it != cacheData_->cache.end() && isEntryValid(it->second)) {
        if (std::holds_alternative<SpatialCacheEntry::FrustumQuery>(it->second.params) &&
            it->second.layerMask == layerMask) {

            const auto& frustumQuery = std::get<SpatialCacheEntry::FrustumQuery>(it->second.params);
            if (frustumQuery.frustumHash == hash) {

                results = it->second.results;
                updateEntryAccess(it->second);
                cacheData_->hitCount++;
                return true;
            }
        }
    }

    cacheData_->missCount++;
    return false;
}

void PredictiveSpatialCache::cacheFrustumQuery(const Frustum& frustum, uint32_t layerMask, const std::vector<EntityID>& results) {
    // Check capacity and enforce limits
    if (!hasCapacity()) {
        enforceMemoryLimits();
        if (!hasCapacity()) {
            return; // Skip caching if still no capacity
        }
    }

    if (cacheData_->cache.size() % 100 == 0) {
        evictOldEntries();
        resetIfCorrupted();
    }

    size_t hash = hashFrustumQuery(frustum, layerMask);

    SpatialCacheEntry entry;
    entry.results = results;
    entry.timestamp = getCurrentTime();
    entry.layerMask = layerMask;
    entry.params = SpatialCacheEntry::FrustumQuery(hash);

    // Calculate frustum bounding box from plane intersections
    // Simple approach: find min/max extents from all 8 frustum corners
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    // Conservative approximation: use plane distances to estimate bounds
    for (int i = 0; i < 6; ++i) {
        const auto& plane = frustum.planes[i];
        float distance = std::abs(plane.w); // Distance from origin

        minX = std::min(minX, -distance);
        maxX = std::max(maxX, distance);
        minY = std::min(minY, -distance);
        maxY = std::max(maxY, distance);
        minZ = std::min(minZ, -distance);
        maxZ = std::max(maxZ, distance);
    }

    entry.regionMin = glm::vec3(minX, minY, minZ);
    entry.regionMax = glm::vec3(maxX, maxY, maxZ);

    // Initialize LRU tracking
    float currentTime = getCurrentTime();
    entry.creationTime = currentTime;
    entry.lastAccessTime = currentTime;
    entry.accessCount = 1;

    // Detect hash collision before overwriting
    if (cacheData_->cache.find(hash) != cacheData_->cache.end()) {
        #ifdef DEBUG_VERBOSE
        VKMON_WARNING("Frustum query cache hash collision - overwriting existing entry");
        #endif
    }

    cacheData_->cache[hash] = std::move(entry);
}

float PredictiveSpatialCache::getCacheHitRate() const {
    size_t totalQueries = cacheData_->hitCount + cacheData_->missCount;
    if (totalQueries == 0) return 0.0f;
    return static_cast<float>(cacheData_->hitCount) / static_cast<float>(totalQueries);
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

// Regional invalidation implementation
void PredictiveSpatialCache::invalidateRegion(const glm::vec3& regionMin, const glm::vec3& regionMax) {
    auto it = cacheData_->cache.begin();
    size_t invalidatedCount = 0;

    while (it != cacheData_->cache.end()) {
        if (regionsIntersect(it->second.regionMin, it->second.regionMax, regionMin, regionMax)) {
            it = cacheData_->cache.erase(it);
            invalidatedCount++;
        } else {
            ++it;
        }
    }

    #ifdef DEBUG_VERBOSE
    if (invalidatedCount > 0) {
        VKMON_DEBUG("Regional invalidation: " + std::to_string(invalidatedCount) + " entries removed");
    }
    #endif
}

void PredictiveSpatialCache::invalidateEntityMovement(const glm::vec3& oldPos, const glm::vec3& newPos, float radius) {
    // Calculate bounding box that encompasses both old and new positions plus radius
    glm::vec3 minBounds = glm::min(oldPos - glm::vec3(radius), newPos - glm::vec3(radius));
    glm::vec3 maxBounds = glm::max(oldPos + glm::vec3(radius), newPos + glm::vec3(radius));

    invalidateRegion(minBounds, maxBounds);
}

bool PredictiveSpatialCache::regionsIntersect(const glm::vec3& min1, const glm::vec3& max1,
                                             const glm::vec3& min2, const glm::vec3& max2) const {
    // AABB intersection test - regions intersect if they overlap on all 3 axes
    return (min1.x <= max2.x && max1.x >= min2.x) &&
           (min1.y <= max2.y && max1.y >= min2.y) &&
           (min1.z <= max2.z && max1.z >= min2.z);
}

glm::vec3 PredictiveSpatialCache::calculateQueryRegionMin(const std::variant<SpatialCacheEntry::RadiusQuery,
                                                                           SpatialCacheEntry::RegionQuery,
                                                                           SpatialCacheEntry::FrustumQuery>& params) const {
    if (std::holds_alternative<SpatialCacheEntry::RadiusQuery>(params)) {
        const auto& radiusQuery = std::get<SpatialCacheEntry::RadiusQuery>(params);
        return radiusQuery.center - glm::vec3(radiusQuery.radius);
    } else if (std::holds_alternative<SpatialCacheEntry::RegionQuery>(params)) {
        const auto& regionQuery = std::get<SpatialCacheEntry::RegionQuery>(params);
        return regionQuery.min;
    } else {
        // FrustumQuery - return conservative world bounds
        return glm::vec3(-100.0f, -100.0f, -100.0f);
    }
}

glm::vec3 PredictiveSpatialCache::calculateQueryRegionMax(const std::variant<SpatialCacheEntry::RadiusQuery,
                                                                           SpatialCacheEntry::RegionQuery,
                                                                           SpatialCacheEntry::FrustumQuery>& params) const {
    if (std::holds_alternative<SpatialCacheEntry::RadiusQuery>(params)) {
        const auto& radiusQuery = std::get<SpatialCacheEntry::RadiusQuery>(params);
        return radiusQuery.center + glm::vec3(radiusQuery.radius);
    } else if (std::holds_alternative<SpatialCacheEntry::RegionQuery>(params)) {
        const auto& regionQuery = std::get<SpatialCacheEntry::RegionQuery>(params);
        return regionQuery.max;
    } else {
        // FrustumQuery - return conservative world bounds
        return glm::vec3(100.0f, 100.0f, 100.0f);
    }
}

// Simple, efficient hash function for all numeric data
size_t PredictiveSpatialCache::improvedHashFunction(const float* values, size_t count, uint32_t layerMask) const {
    size_t hash = 0;

    for (size_t i = 0; i < count; ++i) {
        // Adaptive precision: scale based on magnitude for better distribution
        float value = values[i];
        float absValue = std::abs(value);

        int quantized;
        if (absValue < HIGH_PRECISION_THRESHOLD) {
            quantized = static_cast<int>(std::round(value * HIGH_PRECISION_FACTOR));
        } else if (absValue < MEDIUM_PRECISION_THRESHOLD) {
            quantized = static_cast<int>(std::round(value * MEDIUM_PRECISION_FACTOR));
        } else {
            quantized = static_cast<int>(std::round(value * STANDARD_PRECISION_FACTOR));
        }

        hash ^= std::hash<int>{}(quantized) << (i % 16);
    }

    hash ^= std::hash<uint32_t>{}(layerMask) << 24;
    return hash;
}

// Hash analysis implementation
PredictiveSpatialCache::HashAnalysis PredictiveSpatialCache::analyzeHashDistribution() const {
    HashAnalysis analysis;
    analysis.totalEntries = cacheData_->cache.size();

    if (analysis.totalEntries == 0) {
        return analysis;
    }

    // Count unique hashes and detect collisions
    std::unordered_set<size_t> uniqueHashes;
    std::unordered_map<size_t, size_t> hashCounts;

    for (const auto& [hash, entry] : cacheData_->cache) {
        uniqueHashes.insert(hash);
        hashCounts[hash]++;
    }

    analysis.uniqueHashes = uniqueHashes.size();
    analysis.collisions = analysis.totalEntries - analysis.uniqueHashes;
    analysis.collisionRate = static_cast<float>(analysis.collisions) / static_cast<float>(analysis.totalEntries);

    // Calculate average chain length
    if (analysis.uniqueHashes > 0) {
        analysis.averageChainLength = static_cast<float>(analysis.totalEntries) / static_cast<float>(analysis.uniqueHashes);
    }

    return analysis;
}

// Comprehensive error handling implementation
PredictiveSpatialCache::CacheResult PredictiveSpatialCache::tryGetFrustumQuerySafe(const Frustum& frustum, uint32_t layerMask, std::vector<EntityID>& results) const {
    try {
        // Check cache health before operation
        auto health = checkCacheHealth();
        if (health.requiresReset) {
            return CacheResult::CorruptionDetected;
        }

        if (!hasCapacity()) {
            return CacheResult::MemoryFull;
        }

        // Attempt normal frustum query
        if (tryGetFrustumQuery(frustum, layerMask, results)) {
            return CacheResult::Hit;
        } else {
            return CacheResult::Miss;
        }
    } catch (const std::exception& e) {
        #ifdef DEBUG_VERBOSE
        VKMON_ERROR("Cache operation failed: " + std::string(e.what()));
        #endif
        return CacheResult::Error;
    } catch (...) {
        #ifdef DEBUG_VERBOSE
        VKMON_ERROR("Unknown cache operation failure");
        #endif
        return CacheResult::Error;
    }
}

bool PredictiveSpatialCache::hasCapacity() const {
    return cacheData_->cache.size() < MAX_CACHE_ENTRIES;
}

void PredictiveSpatialCache::enforceMemoryLimits() {
    // Calculate approximate memory usage
    size_t estimatedMemory = cacheData_->cache.size() * (sizeof(SpatialCacheEntry) +
                                                         sizeof(std::pair<size_t, SpatialCacheEntry>) +
                                                         64); // Average EntityID vector size

    // Check hard limits
    if (cacheData_->cache.size() > MAX_CACHE_ENTRIES || estimatedMemory > MAX_MEMORY_BYTES) {
        // Aggressive cleanup - remove 25% of oldest entries
        size_t entriesToRemove = cacheData_->cache.size() / 4;

        std::vector<std::pair<size_t, float>> timestampedEntries;
        for (const auto& [hash, entry] : cacheData_->cache) {
            timestampedEntries.emplace_back(hash, entry.timestamp);
        }

        // Sort by timestamp (oldest first)
        std::sort(timestampedEntries.begin(), timestampedEntries.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        for (size_t i = 0; i < entriesToRemove && i < timestampedEntries.size(); ++i) {
            cacheData_->cache.erase(timestampedEntries[i].first);
        }

        #ifdef DEBUG_VERBOSE
        VKMON_WARNING("Memory limit enforcement: removed " + std::to_string(entriesToRemove) + " cache entries");
        #endif
    }
}

PredictiveSpatialCache::CacheHealth PredictiveSpatialCache::checkCacheHealth() const {
    CacheHealth health;

    if (cacheData_->cache.empty()) {
        return health; // Empty cache is healthy
    }

    // Analyze hash distribution
    auto hashAnalysis = analyzeHashDistribution();
    health.averageChainLength = hashAnalysis.averageChainLength;
    health.totalCollisions = hashAnalysis.collisions;

    // Check hash distribution health
    health.hashDistributionHealthy = (hashAnalysis.collisionRate <= MAX_COLLISION_RATE) &&
                                   (hashAnalysis.averageChainLength <= MAX_CHAIN_LENGTH);

    // Estimate memory usage
    health.memoryUsageBytes = cacheData_->cache.size() * (sizeof(SpatialCacheEntry) +
                                                         sizeof(std::pair<size_t, SpatialCacheEntry>) +
                                                         64); // Average vector overhead

    // Check memory health
    health.memoryHealthy = (cacheData_->cache.size() <= MAX_CACHE_ENTRIES) &&
                          (health.memoryUsageBytes <= MAX_MEMORY_BYTES);

    // Determine if reset is required
    health.requiresReset = !health.memoryHealthy ||
                          !health.hashDistributionHealthy ||
                          (hashAnalysis.collisionRate > 0.8f); // Critical collision rate

    return health;
}

bool PredictiveSpatialCache::resetIfCorrupted() {
    auto health = checkCacheHealth();

    if (health.requiresReset) {
        clear();

        #ifdef DEBUG_VERBOSE
        VKMON_WARNING("Cache reset due to corruption detection");
        VKMON_INFO("Health stats - Memory: " + std::to_string(health.memoryUsageBytes) + " bytes, "
                   "Collisions: " + std::to_string(health.totalCollisions) + ", "
                   "Chain length: " + std::to_string(health.averageChainLength));
        #endif

        return true;
    }

    return false;
}

// LRU eviction strategy implementation
void PredictiveSpatialCache::updateEntryAccess(SpatialCacheEntry& entry) const {
    entry.lastAccessTime = getCurrentTime();
    entry.accessCount++;
}

void PredictiveSpatialCache::evictLRUEntries(size_t targetCount) {
    if (cacheData_->cache.size() <= targetCount) {
        return; // Already at or below target
    }

    // Create list of entries with LRU scoring
    std::vector<std::pair<size_t, float>> lruScores;
    float currentTime = getCurrentTime();

    for (const auto& [hash, entry] : cacheData_->cache) {
        // LRU score: combination of recency and frequency
        float timeSinceAccess = currentTime - entry.lastAccessTime;
        float accessFrequency = static_cast<float>(entry.accessCount) /
                               std::max(0.1f, currentTime - entry.creationTime);

        // Higher score = more likely to be evicted
        float lruScore = timeSinceAccess / (1.0f + accessFrequency);
        lruScores.emplace_back(hash, lruScore);
    }

    // Sort by LRU score (highest first - most eligible for eviction)
    std::sort(lruScores.begin(), lruScores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Remove entries until we reach target count
    size_t entriesToRemove = cacheData_->cache.size() - targetCount;
    for (size_t i = 0; i < entriesToRemove && i < lruScores.size(); ++i) {
        cacheData_->cache.erase(lruScores[i].first);
        cacheData_->totalEvictions++; // Track evictions for metrics
    }

    #ifdef DEBUG_VERBOSE
    VKMON_INFO("LRU eviction: removed " + std::to_string(entriesToRemove) + " entries, "
               "cache size now: " + std::to_string(cacheData_->cache.size()));
    #endif
}

void PredictiveSpatialCache::evictBasedOnMemoryPressure() {
    auto health = checkCacheHealth();

    if (!health.memoryHealthy) {
        // Aggressive eviction under memory pressure
        size_t targetSize = std::min(cacheData_->cache.size() / 2, MAX_CACHE_SIZE / 2);
        evictLRUEntries(targetSize);

        #ifdef DEBUG_VERBOSE
        VKMON_WARNING("Memory pressure eviction: reduced cache to " + std::to_string(targetSize) + " entries");
        #endif
    } else if (health.averageChainLength > MAX_CHAIN_LENGTH) {
        // Moderate eviction for hash distribution health
        size_t targetSize = static_cast<size_t>(cacheData_->cache.size() * 0.8f);
        evictLRUEntries(targetSize);

        #ifdef DEBUG_VERBOSE
        VKMON_INFO("Hash distribution eviction: reduced cache to " + std::to_string(targetSize) + " entries");
        #endif
    }
}

// Production-grade metrics implementation
PredictiveSpatialCache::DetailedCacheMetrics PredictiveSpatialCache::getDetailedMetrics() const {
    DetailedCacheMetrics metrics;
    float currentTime = getCurrentTime();

    // Basic cache metrics
    metrics.totalQueries = cacheData_->hitCount + cacheData_->missCount;
    if (metrics.totalQueries > 0) {
        metrics.hitRate = static_cast<float>(cacheData_->hitCount) / static_cast<float>(metrics.totalQueries);
        metrics.missRate = static_cast<float>(cacheData_->missCount) / static_cast<float>(metrics.totalQueries);
    }

    metrics.cacheSize = cacheData_->cache.size();
    metrics.totalEvictions = cacheData_->totalEvictions;

    // Memory usage estimation
    metrics.memoryUsageBytes = metrics.cacheSize * (sizeof(SpatialCacheEntry) +
                                                   sizeof(std::pair<size_t, SpatialCacheEntry>) +
                                                   64); // Average vector overhead

    // Query type breakdown
    metrics.radiusQueries = cacheData_->radiusQueryCount;
    metrics.regionQueries = cacheData_->regionQueryCount;
    metrics.frustumQueries = cacheData_->frustumQueryCount;

    // Performance timing
    if (cacheData_->queryTimeCount > 0) {
        metrics.averageQueryTime = cacheData_->totalQueryTime / static_cast<float>(cacheData_->queryTimeCount);
    }

    // LRU metrics
    if (!cacheData_->cache.empty()) {
        float totalAccessCount = 0.0f;
        float totalAge = 0.0f;

        for (const auto& [hash, entry] : cacheData_->cache) {
            totalAccessCount += static_cast<float>(entry.accessCount);
            totalAge += (currentTime - entry.creationTime);
        }

        metrics.averageAccessCount = totalAccessCount / static_cast<float>(metrics.cacheSize);
        metrics.averageEntryAge = totalAge / static_cast<float>(metrics.cacheSize);
    }

    // Eviction rate calculation
    if (currentTime > 1.0f) { // Avoid division by zero
        metrics.evictionRate = static_cast<float>(metrics.totalEvictions) / currentTime;
    }

    // Performance alerts (using documented thresholds)
    metrics.hitRateAlert = (metrics.hitRate < MEMORY_PRESSURE_THRESHOLD) && (metrics.totalQueries > 100);
    metrics.memoryAlert = (metrics.memoryUsageBytes > MAX_MEMORY_BYTES * MEMORY_PRESSURE_THRESHOLD);

    auto hashAnalysis = analyzeHashDistribution();
    metrics.collisionAlert = (hashAnalysis.collisionRate > MAX_COLLISION_RATE);

    return metrics;
}

} // namespace VulkanMon