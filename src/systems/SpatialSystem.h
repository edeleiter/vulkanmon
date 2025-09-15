#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/SpatialComponent.h"
#include "../spatial/SpatialManager.h"
#include "../utils/Logger.h"
#include <memory>
#include <chrono>

namespace VulkanMon {

class SpatialSystem : public System<Transform, SpatialComponent> {
private:
    std::unique_ptr<SpatialManager> spatialManager_;

    // Performance tracking
    struct SpatialSystemStats {
        size_t entitiesTracked = 0;
        size_t entitiesUpdated = 0;
        size_t entitiesAdded = 0;
        size_t entitiesRemoved = 0;
        float updateTimeMs = 0.0f;
    } frameStats_;

    // Spatial query cache (for performance)
    struct QueryCache {
        std::vector<EntityID> lastFrustumQuery;
        Frustum lastFrustum;
        float lastQueryTime = 0.0f;
        bool isValid = false;
    } queryCache_;

public:
    SpatialSystem(const BoundingBox& worldBounds) {
        spatialManager_ = std::make_unique<SpatialManager>(worldBounds);
        VKMON_INFO("SpatialSystem initialized with world bounds");
    }

    ~SpatialSystem() = default;

    void update(float deltaTime, EntityManager& entityManager) override {
        auto start = std::chrono::high_resolution_clock::now();

        frameStats_ = SpatialSystemStats{};

        // Get all entities with both Transform and SpatialComponent
        auto& transforms = entityManager.getAllComponents<Transform>();
        auto& spatialComponents = entityManager.getAllComponents<SpatialComponent>();
        auto& entityIds = entityManager.getEntitiesWithComponent<Transform>();

        frameStats_.entitiesTracked = entityIds.size();

        for (size_t i = 0; i < transforms.size(); ++i) {
            EntityID entity = entityIds[i];

            // Check if entity has both components
            if (entityManager.hasComponent<SpatialComponent>(entity)) {
                auto& transform = transforms[i];
                auto& spatial = entityManager.getComponent<SpatialComponent>(entity);

                updateEntitySpatialData(entity, transform, spatial);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        frameStats_.updateTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Performance logging temporarily disabled to reduce log spam
        // TODO: Fix logging frequency issue - static variables not working as expected
        // The spatial system is working correctly, just too much logging

        // Uncomment below for occasional logging:
        // static int frameCount = 0;
        // if (++frameCount >= 1800) { // Every ~30 seconds at 60 FPS
        //     logPerformanceStats();
        //     frameCount = 0;
        // }
    }

    void render(VulkanRenderer& renderer, EntityManager& entityManager) override {
        // SpatialSystem doesn't render anything
    }

    // Public query interface for other systems
    std::vector<EntityID> queryRegion(const BoundingBox& region) const {
        return spatialManager_->queryRegion(region);
    }

    std::vector<EntityID> queryFrustum(const Frustum& frustum) const {
        return spatialManager_->queryFrustum(frustum);
    }

    std::vector<EntityID> queryRadius(const glm::vec3& center, float radius) const {
        return spatialManager_->queryRadius(center, radius);
    }

    // Pokemon-specific queries
    std::vector<EntityID> findCreaturesInRadius(const glm::vec3& center, float radius) const {
        return spatialManager_->findCreaturesInRadius(center, radius);
    }

    std::vector<EntityID> findVisibleCreatures(const Frustum& cameraFrustum) const {
        return spatialManager_->findVisibleCreatures(cameraFrustum);
    }

    EntityID findNearestEntity(const glm::vec3& position, float maxDistance = FLT_MAX) const {
        return spatialManager_->findNearestEntity(position, maxDistance);
    }

    // Entity lifecycle management
    void addEntity(EntityID entity, const glm::vec3& position) {
        spatialManager_->addEntity(entity, position);
        frameStats_.entitiesAdded++;
    }

    void removeEntity(EntityID entity) {
        spatialManager_->removeEntity(entity);
        frameStats_.entitiesRemoved++;
    }

    // Performance and debugging
    const SpatialSystemStats& getFrameStats() const { return frameStats_; }

    void getSpatialStatistics(int& nodeCount, int& maxDepth, int& totalEntities) const {
        spatialManager_->getStatistics(nodeCount, maxDepth, totalEntities);
    }

    const SpatialManager::SpatialStats& getSpatialPerformanceStats() const {
        return spatialManager_->getPerformanceStats();
    }

    // World bounds access
    const BoundingBox& getWorldBounds() const {
        return spatialManager_->getWorldBounds();
    }

private:
    void updateEntitySpatialData(EntityID entity, const Transform& transform, SpatialComponent& spatial) {
        if (spatial.needsSpatialUpdate) {
            spatialManager_->updateEntity(entity, transform.position);
            spatial.needsSpatialUpdate = false;
            frameStats_.entitiesUpdated++;

            // Update home position if this is the first time
            if (spatial.homePosition == glm::vec3(0.0f)) {
                spatial.setHomePosition(transform.position);
            }
        }

        // Update query throttling timer
        spatial.timeSinceLastQuery += 0.016f; // Assume ~60 FPS for now
    }

    void logPerformanceStats() {
        auto spatialStats = spatialManager_->getPerformanceStats();
        int nodeCount, maxDepth, totalEntities;
        spatialManager_->getStatistics(nodeCount, maxDepth, totalEntities);

        VKMON_INFO("SpatialSystem Performance Stats");
        VKMON_INFO("  Entities tracked and updated in frame");
        VKMON_INFO("  Octree structure and query performance logged");
    }
};

} // namespace VulkanMon