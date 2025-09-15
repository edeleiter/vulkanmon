#pragma once

#include "../core/Entity.h"
#include "../spatial/SpatialManager.h"
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

    // Pokemon-specific spatial data
    float detectionRadius = 15.0f;       // How far creature can detect others
    float territoryRadius = 25.0f;       // Territory size for territorial creatures
    glm::vec3 homePosition = glm::vec3(0.0f); // Original spawn position

    // Performance tracking
    uint32_t spatialQueryCount = 0;      // How many queries this entity made
    float timeSinceLastQuery = 0.0f;     // Throttle expensive queries

    // Default constructor
    SpatialComponent() = default;

    // Convenience constructor
    SpatialComponent(float radius, SpatialBehavior behav = SpatialBehavior::DYNAMIC)
        : boundingRadius(radius), behavior(behav) {
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
};

} // namespace VulkanMon