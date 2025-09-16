#include "SpatialManager.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <chrono>
#include <limits>

namespace VulkanMon {

// Frustum implementation
void Frustum::updateFromMatrix(const glm::mat4& viewProjectionMatrix) {
    // Extract frustum planes from view-projection matrix
    const glm::mat4& m = viewProjectionMatrix;

    // Left plane
    planes[0] = glm::vec4(
        m[0][3] + m[0][0],
        m[1][3] + m[1][0],
        m[2][3] + m[2][0],
        m[3][3] + m[3][0]
    );

    // Right plane
    planes[1] = glm::vec4(
        m[0][3] - m[0][0],
        m[1][3] - m[1][0],
        m[2][3] - m[2][0],
        m[3][3] - m[3][0]
    );

    // Bottom plane
    planes[2] = glm::vec4(
        m[0][3] + m[0][1],
        m[1][3] + m[1][1],
        m[2][3] + m[2][1],
        m[3][3] + m[3][1]
    );

    // Top plane
    planes[3] = glm::vec4(
        m[0][3] - m[0][1],
        m[1][3] - m[1][1],
        m[2][3] - m[2][1],
        m[3][3] - m[3][1]
    );

    // Near plane
    planes[4] = glm::vec4(
        m[0][3] + m[0][2],
        m[1][3] + m[1][2],
        m[2][3] + m[2][2],
        m[3][3] + m[3][2]
    );

    // Far plane
    planes[5] = glm::vec4(
        m[0][3] - m[0][2],
        m[1][3] - m[1][2],
        m[2][3] - m[2][2],
        m[3][3] - m[3][2]
    );

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
    }
}

bool Frustum::intersects(const BoundingBox& box) const {
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& plane = planes[i];
        glm::vec3 normal = glm::vec3(plane);

        // Get the positive vertex (farthest along plane normal)
        glm::vec3 positiveVertex = box.min;
        if (normal.x >= 0) positiveVertex.x = box.max.x;
        if (normal.y >= 0) positiveVertex.y = box.max.y;
        if (normal.z >= 0) positiveVertex.z = box.max.z;

        // If positive vertex is behind plane, box is outside frustum
        if (glm::dot(normal, positiveVertex) + plane.w < 0) {
            return false;
        }
    }
    return true;
}

bool Frustum::contains(const glm::vec3& point) const {
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& plane = planes[i];
        if (glm::dot(glm::vec3(plane), point) + plane.w < 0) {
            return false;
        }
    }
    return true;
}

// OctreeNode implementation
void OctreeNode::insert(EntityID entity, const glm::vec3& position) {
    if (!bounds_.contains(position)) {
        return; // Position is outside this node's bounds
    }

    if (is_leaf_) {
        entities_.push_back(entity);

        // Note: Subdivision now requires position lookup from SpatialManager
        // This will be handled by SpatialManager::insert() when needed
    } else {
        // Insert into appropriate child
        int childIndex = getChildIndex(position);
        if (children_[childIndex]) {
            children_[childIndex]->insert(entity, position);
        }
    }
}

void OctreeNode::remove(EntityID entity, const glm::vec3& position) {
    if (!bounds_.contains(position)) {
        return;
    }

    if (is_leaf_) {
        auto it = std::find(entities_.begin(), entities_.end(), entity);
        if (it != entities_.end()) {
            entities_.erase(it);
        }
    } else {
        int childIndex = getChildIndex(position);
        if (children_[childIndex]) {
            children_[childIndex]->remove(entity, position);
        }
    }
}

void OctreeNode::update(EntityID entity, const glm::vec3& oldPos, const glm::vec3& newPos) {
    // If both positions are in this node, handle the update internally
    bool oldInBounds = bounds_.contains(oldPos);
    bool newInBounds = bounds_.contains(newPos);

    if (oldInBounds && !newInBounds) {
        remove(entity, oldPos);
    } else if (!oldInBounds && newInBounds) {
        insert(entity, newPos);
    } else if (oldInBounds && newInBounds) {
        // Both positions in bounds - update children if not leaf
        if (!is_leaf_) {
            int oldChildIndex = getChildIndex(oldPos);
            int newChildIndex = getChildIndex(newPos);

            if (oldChildIndex != newChildIndex) {
                if (children_[oldChildIndex]) {
                    children_[oldChildIndex]->remove(entity, oldPos);
                }
                if (children_[newChildIndex]) {
                    children_[newChildIndex]->insert(entity, newPos);
                }
            } else {
                if (children_[oldChildIndex]) {
                    children_[oldChildIndex]->update(entity, oldPos, newPos);
                }
            }
        }
    }
}

void OctreeNode::query(const BoundingBox& region, std::vector<EntityID>& results) const {
    if (!bounds_.intersects(region)) {
        return;
    }

    if (is_leaf_) {
        // Only add entities that need further filtering - let SpatialManager handle position checks
        results.insert(results.end(), entities_.begin(), entities_.end());
    } else {
        for (int i = 0; i < 8; ++i) {
            if (children_[i]) {
                children_[i]->query(region, results);
            }
        }
    }
}

void OctreeNode::query(const Frustum& frustum, std::vector<EntityID>& results) const {
    if (!frustum.intersects(bounds_)) {
        return;
    }

    if (is_leaf_) {
        results.insert(results.end(), entities_.begin(), entities_.end());
    } else {
        for (int i = 0; i < 8; ++i) {
            if (children_[i]) {
                children_[i]->query(frustum, results);
            }
        }
    }
}

void OctreeNode::queryRadius(const glm::vec3& center, float radius, std::vector<EntityID>& results) const {
    // Create bounding box for sphere
    BoundingBox sphereBounds(center, radius);

    if (!bounds_.intersects(sphereBounds)) {
        return;
    }

    if (is_leaf_) {
        results.insert(results.end(), entities_.begin(), entities_.end());
    } else {
        for (int i = 0; i < 8; ++i) {
            if (children_[i]) {
                children_[i]->queryRadius(center, radius, results);
            }
        }
    }
}

void OctreeNode::clear() {
    entities_.clear();
    for (int i = 0; i < 8; ++i) {
        children_[i].reset();
    }
    is_leaf_ = true;
}

size_t OctreeNode::getEntityCount() const {
    size_t count = entities_.size();
    if (!is_leaf_) {
        for (int i = 0; i < 8; ++i) {
            if (children_[i]) {
                count += children_[i]->getEntityCount();
            }
        }
    }
    return count;
}

void OctreeNode::getStatistics(int& nodeCount, int& maxDepth, int& totalEntities) const {
    nodeCount++;
    maxDepth = std::max(maxDepth, depth_);
    totalEntities += static_cast<int>(entities_.size());

    if (!is_leaf_) {
        for (int i = 0; i < 8; ++i) {
            if (children_[i]) {
                children_[i]->getStatistics(nodeCount, maxDepth, totalEntities);
            }
        }
    }
}

void OctreeNode::subdivide(const std::function<glm::vec3(EntityID)>& getPosition) {
    if (depth_ >= MAX_DEPTH) {
        return; // Max depth reached
    }

    is_leaf_ = false;

    // Create child nodes
    for (int i = 0; i < 8; ++i) {
        BoundingBox childBounds = getChildBounds(i);
        children_[i] = std::make_unique<OctreeNode>(childBounds, depth_ + 1);
    }

    // Redistribute entities to children using position lookup
    std::vector<EntityID> entitiesToRedistribute = std::move(entities_);
    entities_.clear();

    for (EntityID entity : entitiesToRedistribute) {
        glm::vec3 position = getPosition(entity);
        int childIndex = getChildIndex(position);
        if (children_[childIndex]) {
            children_[childIndex]->insert(entity, position);
        }
    }
}

int OctreeNode::getChildIndex(const glm::vec3& position) const {
    glm::vec3 center = bounds_.getCenter();

    int index = 0;
    if (position.x > center.x) index |= 1;
    if (position.y > center.y) index |= 2;
    if (position.z > center.z) index |= 4;

    return index;
}

BoundingBox OctreeNode::getChildBounds(int childIndex) const {
    glm::vec3 center = bounds_.getCenter();
    glm::vec3 size = bounds_.getSize() * 0.5f;

    glm::vec3 childMin = bounds_.min;
    glm::vec3 childMax = center;

    if (childIndex & 1) { // x bit
        childMin.x = center.x;
        childMax.x = bounds_.max.x;
    }
    if (childIndex & 2) { // y bit
        childMin.y = center.y;
        childMax.y = bounds_.max.y;
    }
    if (childIndex & 4) { // z bit
        childMin.z = center.z;
        childMax.z = bounds_.max.z;
    }

    return BoundingBox(childMin, childMax);
}

bool OctreeNode::shouldSubdivide() const {
    return is_leaf_ &&
           entities_.size() > MAX_ENTITIES_PER_NODE &&
           depth_ < MAX_DEPTH;
}

// SpatialManager implementation
SpatialManager::SpatialManager(const BoundingBox& worldBounds)
    : worldBounds_(worldBounds) {
    octree_ = std::make_unique<OctreeNode>(worldBounds);
    VKMON_INFO("SpatialManager initialized with world bounds");
}

void SpatialManager::addEntity(EntityID entity, const glm::vec3& position, uint32_t layers) {
    auto it = entityPositions_.find(entity);
    if (it != entityPositions_.end()) {
        VKMON_WARNING("Entity already exists in spatial manager, updating position");
        updateEntity(entity, position);
        updateEntityLayers(entity, layers);
        return;
    }

    entityPositions_[entity] = position;
    entityLayers_[entity] = layers;
    octree_->insert(entity, position);

    // Clear cache when spatial structure changes
    cache_.clear();
}

void SpatialManager::removeEntity(EntityID entity) {
    auto it = entityPositions_.find(entity);
    if (it == entityPositions_.end()) {
        VKMON_WARNING("Attempted to remove entity that doesn't exist in spatial manager");
        return;
    }

    octree_->remove(entity, it->second);
    entityPositions_.erase(it);
    entityLayers_.erase(entity);

    // Clear cache when spatial structure changes
    cache_.clear();
}

void SpatialManager::updateEntity(EntityID entity, const glm::vec3& newPosition) {
    auto it = entityPositions_.find(entity);
    if (it == entityPositions_.end()) {
        VKMON_WARNING("Attempted to update entity that doesn't exist in spatial manager");
        addEntity(entity, newPosition);
        return;
    }

    glm::vec3 oldPosition = it->second;
    it->second = newPosition;

    octree_->update(entity, oldPosition, newPosition);

    // Clear cache when spatial structure changes
    cache_.clear();
}

void SpatialManager::updateEntityLayers(EntityID entity, uint32_t layers) {
    auto it = entityPositions_.find(entity);
    if (it == entityPositions_.end()) {
        VKMON_WARNING("Attempted to update layers for entity that doesn't exist in spatial manager");
        return;
    }

    entityLayers_[entity] = layers;
}

std::vector<EntityID> SpatialManager::queryRegion(const BoundingBox& region, uint32_t layerMask) const {
    // Try cache first
    std::vector<EntityID> cachedResults;
    if (cache_.tryGetRegionQuery(region, layerMask, cachedResults)) {
        updateStatistics(0.0f, cachedResults.size()); // Cache hit, near-zero time
        return cachedResults;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<EntityID> results;
    octree_->query(region, results);

    // Apply position and layer filtering
    std::vector<EntityID> filteredResults;
    for (EntityID entity : results) {
        auto it = entityPositions_.find(entity);
        if (it != entityPositions_.end()) {
            // Check if entity position is actually within the region
            if (region.contains(it->second) && passesLayerFilter(entity, layerMask)) {
                filteredResults.push_back(entity);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    float queryTime = std::chrono::duration<float, std::milli>(end - start).count();
    updateStatistics(queryTime, filteredResults.size());

    // Cache the result
    cache_.cacheRegionQuery(region, layerMask, filteredResults);

    return filteredResults;
}

std::vector<EntityID> SpatialManager::queryFrustum(const Frustum& frustum, uint32_t layerMask) const {
    // Try cache first
    std::vector<EntityID> cachedResults;
    if (cache_.tryGetFrustumQuery(frustum, layerMask, cachedResults)) {
        updateStatistics(0.0f, cachedResults.size()); // Cache hit, near-zero time
        return cachedResults;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<EntityID> results;
    octree_->query(frustum, results);

    // Apply layer filtering
    std::vector<EntityID> filteredResults;
    for (EntityID entity : results) {
        if (passesLayerFilter(entity, layerMask)) {
            filteredResults.push_back(entity);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    float queryTime = std::chrono::duration<float, std::milli>(end - start).count();
    updateStatistics(queryTime, filteredResults.size());

    // Cache the result
    cache_.cacheFrustumQuery(frustum, layerMask, filteredResults);

    return filteredResults;
}

std::vector<EntityID> SpatialManager::queryRadius(const glm::vec3& center, float radius, uint32_t layerMask) const {
    // Try cache first
    std::vector<EntityID> cachedResults;
    if (cache_.tryGetRadiusQuery(center, radius, layerMask, cachedResults)) {
        updateStatistics(0.0f, cachedResults.size()); // Cache hit, near-zero time
        return cachedResults;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<EntityID> results;
    octree_->queryRadius(center, radius, results);

    // Filter results by actual distance and layers (octree query returns candidates)
    std::vector<EntityID> filteredResults;
    for (EntityID entity : results) {
        auto it = entityPositions_.find(entity);
        if (it != entityPositions_.end()) {
            float distance = glm::distance(center, it->second);
            if (distance <= radius && passesLayerFilter(entity, layerMask)) {
                filteredResults.push_back(entity);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    float queryTime = std::chrono::duration<float, std::milli>(end - start).count();
    updateStatistics(queryTime, filteredResults.size());

    // Cache the result
    cache_.cacheRadiusQuery(center, radius, layerMask, filteredResults);

    return filteredResults;
}

std::vector<EntityID> SpatialManager::findCreaturesInRadius(const glm::vec3& center, float radius) const {
    return queryRadius(center, radius, LayerMask::Creatures);
}

std::vector<EntityID> SpatialManager::findVisibleCreatures(const Frustum& cameraFrustum) const {
    return queryFrustum(cameraFrustum, LayerMask::Creatures);
}

std::vector<EntityID> SpatialManager::findNearestEntities(const glm::vec3& position, int count, float maxDistance, uint32_t layerMask) const {
    // Query a region and sort by distance
    BoundingBox searchRegion(position, maxDistance);
    std::vector<EntityID> candidates = queryRegion(searchRegion, layerMask);

    // Calculate distances and sort
    std::vector<std::pair<float, EntityID>> distanceEntityPairs;
    for (EntityID entity : candidates) {
        auto it = entityPositions_.find(entity);
        if (it != entityPositions_.end()) {
            float distance = glm::distance(position, it->second);
            if (distance <= maxDistance) {
                distanceEntityPairs.emplace_back(distance, entity);
            }
        }
    }

    // Sort by distance
    std::sort(distanceEntityPairs.begin(), distanceEntityPairs.end());

    // Return top N
    std::vector<EntityID> results;
    int resultCount = std::min(count, static_cast<int>(distanceEntityPairs.size()));
    for (int i = 0; i < resultCount; ++i) {
        results.push_back(distanceEntityPairs[i].second);
    }

    return results;
}

EntityID SpatialManager::findNearestEntity(const glm::vec3& position, float maxDistance, uint32_t layerMask) const {
    auto nearest = findNearestEntities(position, 1, maxDistance, layerMask);
    return nearest.empty() ? INVALID_ENTITY : nearest[0];
}

void SpatialManager::getStatistics(int& nodeCount, int& maxDepth, int& totalEntities) const {
    nodeCount = 0;
    maxDepth = 0;
    totalEntities = 0;
    octree_->getStatistics(nodeCount, maxDepth, totalEntities);
}

void SpatialManager::clear() {
    octree_->clear();
    entityPositions_.clear();
    entityLayers_.clear();
    cache_.clear();
    stats_ = SpatialStats{};
}

void SpatialManager::updateStatistics(float queryTimeMs, size_t entitiesReturned) const {
    stats_.totalQueries++;
    stats_.totalEntitiesReturned += entitiesReturned;
    stats_.lastQueryTimeMs = queryTimeMs;

    // Update running average
    float alpha = 0.1f; // Smoothing factor
    stats_.averageQueryTimeMs = stats_.averageQueryTimeMs * (1.0f - alpha) + queryTimeMs * alpha;

    // Update cache statistics
    stats_.cacheHitRate = cache_.getCacheHitRate();
    stats_.cacheSize = cache_.getCacheSize();
}

bool SpatialManager::passesLayerFilter(EntityID entity, uint32_t layerMask) const {
    if (layerMask == LayerMask::All) {
        return true;
    }

    auto it = entityLayers_.find(entity);
    if (it == entityLayers_.end()) {
        return layerMask == LayerMask::All; // If no layers set, only pass if querying all
    }

    return LayerMask::contains(layerMask, it->second);
}

} // namespace VulkanMon