#pragma once

#include "../core/Entity.h"
#include "LayerMask.h"
#include "SpatialCache.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <limits>
#include <mutex>

namespace VulkanMon {

struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;

    BoundingBox() : min(std::numeric_limits<float>::max()), max(-std::numeric_limits<float>::max()) {}
    BoundingBox(const glm::vec3& center, float radius)
        : min(center - radius), max(center + radius) {}
    BoundingBox(const glm::vec3& minPoint, const glm::vec3& maxPoint)
        : min(minPoint), max(maxPoint) {}

    bool contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool intersects(const BoundingBox& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    glm::vec3 getCenter() const { return (min + max) * 0.5f; }
    glm::vec3 getSize() const { return max - min; }
    float getVolume() const {
        glm::vec3 size = getSize();
        return size.x * size.y * size.z;
    }
};

struct Frustum {
    glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far

    void updateFromMatrix(const glm::mat4& viewProjectionMatrix);
    bool intersects(const BoundingBox& box) const;
    bool contains(const glm::vec3& point) const;
};

class OctreeNode {
public:
    static constexpr int MAX_ENTITIES_PER_NODE = 16;
    static constexpr int MAX_DEPTH = 8;

private:
    BoundingBox bounds_;
    std::vector<EntityID> entities_;
    std::unique_ptr<OctreeNode> children_[8];
    int depth_;
    bool is_leaf_;

public:
    OctreeNode(const BoundingBox& bounds, int depth = 0)
        : bounds_(bounds), depth_(depth), is_leaf_(true) {}

    // Core octree operations
    void insert(EntityID entity, const glm::vec3& position);
    void remove(EntityID entity, const glm::vec3& position);
    void update(EntityID entity, const glm::vec3& oldPos, const glm::vec3& newPos);

    // Subdivision with position lookup
    void subdivide(std::function<glm::vec3(EntityID)> getPosition);

    // Spatial queries
    void query(const BoundingBox& region, std::vector<EntityID>& results) const;
    void query(const Frustum& frustum, std::vector<EntityID>& results) const;
    void queryRadius(const glm::vec3& center, float radius, std::vector<EntityID>& results) const;

    // Utility
    void clear();
    const BoundingBox& getBounds() const { return bounds_; }
    size_t getEntityCount() const;
    void getStatistics(int& nodeCount, int& maxDepth, int& totalEntities) const;

private:
    int getChildIndex(const glm::vec3& position) const;
    BoundingBox getChildBounds(int childIndex) const;
    bool shouldSubdivide() const;
};

class SpatialManager {
public:
    // Performance tracking structure (public for external access)
    struct SpatialStats {
        size_t totalQueries = 0;
        size_t totalEntitiesReturned = 0;
        float lastQueryTimeMs = 0.0f;
        float averageQueryTimeMs = 0.0f;
        float cacheHitRate = 0.0f;
        size_t cacheSize = 0;
    };

private:
    std::unique_ptr<OctreeNode> octree_;
    BoundingBox worldBounds_;

    // Entity tracking for updates
    std::unordered_map<EntityID, glm::vec3> entityPositions_;
    std::unordered_map<EntityID, uint32_t> entityLayers_;

    // Performance tracking (thread-safe)
    mutable SpatialStats stats_;
    mutable std::mutex statsMutex_;

    // Query caching system
    mutable PredictiveSpatialCache cache_;

public:
    SpatialManager(const BoundingBox& worldBounds);
    ~SpatialManager() = default;

    // Entity management
    void addEntity(EntityID entity, const glm::vec3& position, uint32_t layers = LayerMask::None);
    void removeEntity(EntityID entity);
    void updateEntity(EntityID entity, const glm::vec3& newPosition);
    void updateEntityLayers(EntityID entity, uint32_t layers);

    // Spatial queries for Pokemon gameplay
    std::vector<EntityID> queryRegion(const BoundingBox& region, uint32_t layerMask = LayerMask::All) const;
    std::vector<EntityID> queryFrustum(const Frustum& frustum, uint32_t layerMask = LayerMask::All) const;
    std::vector<EntityID> queryRadius(const glm::vec3& center, float radius, uint32_t layerMask = LayerMask::All) const;

    // Pokemon-specific queries
    std::vector<EntityID> findCreaturesInRadius(const glm::vec3& center, float radius) const;
    std::vector<EntityID> findVisibleCreatures(const Frustum& cameraFrustum) const;

    // Nearest neighbor queries (for creature AI)
    std::vector<EntityID> findNearestEntities(const glm::vec3& position, int count, float maxDistance = std::numeric_limits<float>::max(), uint32_t layerMask = LayerMask::All) const;
    EntityID findNearestEntity(const glm::vec3& position, float maxDistance = std::numeric_limits<float>::max(), uint32_t layerMask = LayerMask::All) const;

    // Performance and debugging
    void getStatistics(int& nodeCount, int& maxDepth, int& totalEntities) const;
    SpatialStats getPerformanceStats() const {
        std::lock_guard<std::mutex> lock(statsMutex_);
        return stats_;
    }
    void clear();

    // Cache management
    void clearCache() { cache_.clear(); }
    void cleanupCache() { cache_.cleanup(); }

    const BoundingBox& getWorldBounds() const { return worldBounds_; }

private:
    void updateStatistics(float queryTimeMs, size_t entitiesReturned) const;
    bool passesLayerFilter(EntityID entity, uint32_t layerMask) const;
};

} // namespace VulkanMon