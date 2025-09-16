#pragma once

#include "../core/Entity.h"
#include "../spatial/SpatialManager.h"
#include "../spatial/LayerMask.h"
#include <glm/glm.hpp>

namespace VulkanMon {

enum class SpatialBehavior : uint8_t {
    STATIC = 0,     // Never moves (trees, rocks, buildings)
    DYNAMIC = 1,    // Moves frequently (creatures, player)
    OCCASIONAL = 2  // Moves rarely (NPCs, some environmental objects)
};

struct SpatialComponent {
    VKMON_COMPONENT(SpatialComponent)

    // Spatial properties
    float boundingRadius = 1.0f;        // For sphere-based culling and queries
    BoundingBox localBounds;             // Local space bounding box
    SpatialBehavior behavior = SpatialBehavior::DYNAMIC;

    // Spatial optimization flags
    bool needsSpatialUpdate = true;      // Entity moved, needs octree update
    bool isVisible = true;               // Last frame visibility result
    float lastCullingDistance = 0.0f;    // Distance when last culled

    // Layer-based filtering for spatial queries
    uint32_t spatialLayers = LayerMask::None;  // What layers this entity belongs to

    // Pokemon-specific spatial data
    float detectionRadius = 15.0f;       // How far creature can detect others
    float territoryRadius = 25.0f;       // Territory size for territorial creatures
    glm::vec3 homePosition = glm::vec3(0.0f); // Original spawn position

    // Performance tracking
    uint32_t spatialQueryCount = 0;      // How many queries this entity made
    float timeSinceLastQuery = 1.0f;     // Throttle expensive queries (start >= interval for first query)

    // Default constructor
    SpatialComponent() = default;

    // Convenience constructor
    SpatialComponent(float radius, SpatialBehavior behav = SpatialBehavior::DYNAMIC, uint32_t layers = LayerMask::None)
        : boundingRadius(radius), behavior(behav), spatialLayers(layers) {
        localBounds = BoundingBox(glm::vec3(0.0f), radius);
    }

    // Methods
    BoundingBox getWorldBounds(const glm::vec3& worldPosition) const {
        glm::vec3 radiusVector(boundingRadius, boundingRadius, boundingRadius);
        return BoundingBox(worldPosition - radiusVector, worldPosition + radiusVector);
    }

    bool isNearHome(const glm::vec3& currentPos, float threshold = 5.0f) const {
        return glm::distance(currentPos, homePosition) <= threshold;
    }

    bool shouldThrottleQueries(float deltaTime, float maxQueryFrequency = 10.0f) {
        timeSinceLastQuery += deltaTime;
        float minInterval = 1.0f / maxQueryFrequency;

        if (timeSinceLastQuery >= minInterval) {
            timeSinceLastQuery = 0.0f;
            spatialQueryCount++;
            return false; // Don't throttle
        }
        return true; // Throttle this query
    }

    void markDirty() { needsSpatialUpdate = true; }

    void setHomePosition(const glm::vec3& position) {
        homePosition = position;
    }

    void setBoundingRadius(float radius) {
        boundingRadius = radius;
        localBounds = BoundingBox(glm::vec3(0.0f), radius);
    }

    void setSpatialLayers(uint32_t layers) {
        spatialLayers = layers;
    }

    void addSpatialLayer(uint32_t layer) {
        spatialLayers |= layer;
    }

    void removeSpatialLayer(uint32_t layer) {
        spatialLayers &= ~layer;
    }

    bool hasLayer(uint32_t layer) const {
        return LayerMask::contains(spatialLayers, layer);
    }
};

} // namespace VulkanMon