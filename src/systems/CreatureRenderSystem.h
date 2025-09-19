#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"
#include "../game/CreatureDetectionSystem.h" // For CreatureComponent
#include "../rendering/VulkanRenderer.h"
#include "../spatial/SpatialManager.h"
#include "../utils/Logger.h"
#include "CameraSystem.h"
#include "SpatialSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <chrono>
#include <cassert>

namespace VulkanMon {

// InstanceData is defined in VulkanRenderer.h
// Batch of identical creatures for instanced rendering
struct InstancedBatch {
    std::string meshPath;
    uint32_t baseMaterialId;
    std::vector<InstanceData> instances;
    std::vector<EntityID> entityIds;  // For debugging/selection

    // Performance tracking
    size_t maxInstances = 0;
    float totalRenderTimeMs = 0.0f;

    void clear() {
        instances.clear();
        entityIds.clear();
    }

    void reserve(size_t count) {
        instances.reserve(count);
        entityIds.reserve(count);
        maxInstances = std::max(maxInstances, count);
    }

    void addInstance(EntityID entity, const InstanceData& data) {
        instances.push_back(data);
        entityIds.push_back(entity);
    }

    size_t size() const { return instances.size(); }
    bool empty() const { return instances.empty(); }
};

// Rendering statistics for performance monitoring
struct CreatureRenderStats {
    size_t totalCreatures = 0;
    size_t renderedCreatures = 0;
    size_t culledCreatures = 0;
    size_t instanceBatches = 0;
    size_t totalDrawCalls = 0;

    float frustumCullingTimeMs = 0.0f;
    float batchingTimeMs = 0.0f;
    float renderingTimeMs = 0.0f;
    float totalFrameTimeMs = 0.0f;

    // Performance efficiency metrics
    float cullingEfficiency() const {
        return totalCreatures > 0 ? (float)culledCreatures / totalCreatures : 0.0f;
    }

    float instancingEfficiency() const {
        return totalDrawCalls > 0 ? (float)renderedCreatures / totalDrawCalls : 1.0f;
    }

    void reset() {
        *this = CreatureRenderStats{};
    }
};

/**
 * CreatureRenderSystem - High-performance instanced rendering for massive creature counts
 *
 * Key Features:
 * - GPU instancing for identical creatures (dramatically reduces draw calls)
 * - Spatial frustum culling integration for performance
 * - Distance-based LOD system ready for implementation
 * - Performance monitoring and statistics
 * - Seamless integration with existing ECS architecture
 *
 * Performance Goals:
 * - 200+ creatures at 60+ FPS (minimum)
 * - 300+ creatures at 60+ FPS (target)
 * - Sub-millisecond batching overhead
 * - Memory efficient instance data management
 */
class CreatureRenderSystem : public System<Transform, Renderable, CreatureComponent> {
private:
    // System dependencies (non-owning, lifetime managed by World)
    CameraSystem* cameraSystem_ = nullptr;   // Safe: World guarantees lifetime
    SpatialSystem* spatialSystem_ = nullptr; // Safe: World guarantees lifetime

    // Instanced rendering data
    std::unordered_map<std::string, InstancedBatch> instanceBatches_;

    // Performance and configuration
    CreatureRenderStats frameStats_;
    float maxRenderDistance_ = 1000.0f;
    bool enableFrustumCulling_ = true;
    bool enableDistanceCulling_ = true;

    // Performance optimization
    static constexpr size_t BATCH_RESERVE_SIZE = 128;

    // Frame-based performance logging - the engine heartbeat
    size_t renderFrameCount_ = 0;
    static constexpr size_t LOG_EVERY_N_RENDERS = 1800;    // ~10 seconds at 180 FPS
    static constexpr float LOG_INTERVAL_SECONDS = 10.0f;   // Documentation

public:
    explicit CreatureRenderSystem(CameraSystem* cameraSystem = nullptr, SpatialSystem* spatialSystem = nullptr);
    virtual ~CreatureRenderSystem() = default;

    // Core System interface
    void update(float deltaTime, EntityManager& entityManager) override;
    void render(VulkanRenderer& renderer, EntityManager& entityManager) override;

    // System dependencies (with safety logging)
    void setCameraSystem(CameraSystem* cameraSystem) {
        cameraSystem_ = cameraSystem;
        VKMON_INFO("CreatureRenderSystem: CameraSystem reference updated");
    }

    void setSpatialSystem(SpatialSystem* spatialSystem) {
        spatialSystem_ = spatialSystem;
        VKMON_INFO("CreatureRenderSystem: SpatialSystem reference updated");
    }

    // Configuration
    void setMaxRenderDistance(float distance) { maxRenderDistance_ = distance; }
    float getMaxRenderDistance() const { return maxRenderDistance_; }

    void setEnableFrustumCulling(bool enabled) { enableFrustumCulling_ = enabled; }
    bool getFrustumCullingEnabled() const { return enableFrustumCulling_; }

    void setEnableDistanceCulling(bool enabled) { enableDistanceCulling_ = enabled; }
    bool getDistanceCullingEnabled() const { return enableDistanceCulling_; }

    // Performance monitoring
    const CreatureRenderStats& getFrameStats() const { return frameStats_; }
    void logPerformanceStats();

    // Debug and inspection
    size_t getBatchCount() const { return instanceBatches_.size(); }
    std::vector<std::string> getBatchMeshPaths() const;
    const InstancedBatch* getBatch(const std::string& meshPath) const;

private:
    // Core rendering pipeline
    std::vector<EntityID> collectVisibleCreatures(EntityManager& entityManager, const glm::vec3& cameraPos);
    void buildInstanceBatches(const std::vector<EntityID>& visibleCreatures, EntityManager& entityManager, const glm::vec3& cameraPos);
    void renderInstanceBatches(VulkanRenderer& renderer);

    // Culling operations
    std::vector<EntityID> performFrustumCulling(EntityManager& entityManager);
    std::vector<EntityID> performDistanceCulling(const std::vector<EntityID>& candidates, EntityManager& entityManager, const glm::vec3& cameraPos);

    // Batch management
    void clearBatches();
    void reserveBatchCapacity(const std::vector<EntityID>& entities, EntityManager& entityManager);
    std::string generateBatchKey(const std::string& meshPath, uint32_t materialId) const;

    // Performance utilities (none needed - frame counter handles this)
};

} // namespace VulkanMon