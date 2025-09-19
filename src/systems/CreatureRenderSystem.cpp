#include "CreatureRenderSystem.h"
#include <cassert>
#include <algorithm>
#include <sstream>

namespace VulkanMon {

CreatureRenderSystem::CreatureRenderSystem(CameraSystem* cameraSystem, SpatialSystem* spatialSystem)
    : cameraSystem_(cameraSystem), spatialSystem_(spatialSystem) {
    VKMON_INFO("CreatureRenderSystem initialized - GPU instancing enabled");
    VKMON_INFO("Target performance: 200+ creatures at 60+ FPS");

    // Pre-reserve space for common batch counts
    instanceBatches_.reserve(16); // Expect up to 16 different creature types
}

void CreatureRenderSystem::update(float deltaTime, EntityManager& entityManager) {
    // Future: Update creature animations, behavior states, etc.
    // For now, just reset statistics for the next frame
    frameStats_.reset();
}

void CreatureRenderSystem::render(VulkanRenderer& renderer, EntityManager& entityManager) {
    // Debug safety assertions (zero cost in release builds)
    assert(cameraSystem_ && "CameraSystem must be set before rendering");
    assert(spatialSystem_ && "SpatialSystem must be set before rendering");

    auto frameStart = std::chrono::high_resolution_clock::now();

    // Step 1: Get camera position for distance calculations
    glm::vec3 cameraPos(0.0f, 80.0f, 120.0f);  // Updated fallback to match actual camera position
    if (cameraSystem_) {
        cameraPos = cameraSystem_->getActiveCameraPosition(entityManager);
    } else {
        VKMON_WARNING("CreatureRenderSystem: No CameraSystem available - using fallback position");
    }

    // Step 2: Collect visible creatures using spatial culling
    auto cullStart = std::chrono::high_resolution_clock::now();
    std::vector<EntityID> visibleCreatures = collectVisibleCreatures(entityManager, cameraPos);
    auto cullEnd = std::chrono::high_resolution_clock::now();

    frameStats_.frustumCullingTimeMs = std::chrono::duration<float, std::milli>(cullEnd - cullStart).count();
    frameStats_.totalCreatures = entityManager.getEntitiesWithComponent<CreatureComponent>().size();
    frameStats_.renderedCreatures = visibleCreatures.size();
    frameStats_.culledCreatures = frameStats_.totalCreatures - frameStats_.renderedCreatures;

    // Step 3: Build instanced batches from visible creatures
    auto batchStart = std::chrono::high_resolution_clock::now();
    buildInstanceBatches(visibleCreatures, entityManager, cameraPos);
    auto batchEnd = std::chrono::high_resolution_clock::now();

    frameStats_.batchingTimeMs = std::chrono::duration<float, std::milli>(batchEnd - batchStart).count();
    frameStats_.instanceBatches = instanceBatches_.size();

    // Step 4: Render all instanced batches
    auto renderStart = std::chrono::high_resolution_clock::now();
    renderInstanceBatches(renderer);
    auto renderEnd = std::chrono::high_resolution_clock::now();

    frameStats_.renderingTimeMs = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();

    // Calculate total frame time
    auto frameEnd = std::chrono::high_resolution_clock::now();
    frameStats_.totalFrameTimeMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();

    // Frame-counter based performance logging - the engine heartbeat
    renderFrameCount_++;
    if (renderFrameCount_ % LOG_EVERY_N_RENDERS == 0) {
        logPerformanceStats();
    }

    // Clear batches for next frame
    clearBatches();
}

std::vector<EntityID> CreatureRenderSystem::collectVisibleCreatures(EntityManager& entityManager, const glm::vec3& cameraPos) {
    std::vector<EntityID> candidates;

    // Step 1: Spatial frustum culling (most efficient)
    if (enableFrustumCulling_ && spatialSystem_ && cameraSystem_) {
        candidates = performFrustumCulling(entityManager);

        #ifdef DEBUG_VERBOSE
        VKMON_DEBUG("Frustum culling found " + std::to_string(candidates.size()) + " creature candidates");
        #endif
    } else {
        // Fallback: Get all creatures
        candidates = entityManager.getEntitiesWithComponent<CreatureComponent>();

        #ifdef DEBUG_VERBOSE
        VKMON_DEBUG("Using fallback: " + std::to_string(candidates.size()) + " total creatures");
        #endif
    }

    // Step 2: Distance culling (secondary optimization)
    if (enableDistanceCulling_ && maxRenderDistance_ > 0.0f) {
        candidates = performDistanceCulling(candidates, entityManager, cameraPos);

        #ifdef DEBUG_VERBOSE
        VKMON_DEBUG("Distance culling reduced to " + std::to_string(candidates.size()) + " creatures");
        #endif
    }

    return candidates;
}

std::vector<EntityID> CreatureRenderSystem::performFrustumCulling(EntityManager& entityManager) {
    try {
        Frustum cameraFrustum = cameraSystem_->getActiveCameraFrustum(entityManager);

        // Query spatial system for creatures in camera frustum
        return spatialSystem_->queryFrustum(cameraFrustum, LayerMask::Creatures);
    } catch (const std::exception& e) {
        VKMON_WARNING("Frustum culling failed: " + std::string(e.what()) + " - falling back to all creatures");
        return entityManager.getEntitiesWithComponent<CreatureComponent>();
    }
}

std::vector<EntityID> CreatureRenderSystem::performDistanceCulling(const std::vector<EntityID>& candidates,
                                                                   EntityManager& entityManager,
                                                                   const glm::vec3& cameraPos) {
    std::vector<EntityID> visibleCreatures;
    visibleCreatures.reserve(candidates.size());

    for (EntityID entity : candidates) {
        if (!hasRequiredComponents(entity, entityManager)) {
            continue;
        }

        const Transform& transform = entityManager.getComponent<Transform>(entity);
        const Renderable& renderable = entityManager.getComponent<Renderable>(entity);

        // Skip if not visible
        if (!renderable.isVisible) {
            continue;
        }

        // Distance culling
        float distance = glm::length(transform.position - cameraPos);
        if (renderable.shouldRender(distance) && distance <= maxRenderDistance_) {
            visibleCreatures.push_back(entity);
        }
    }

    return visibleCreatures;
}

void CreatureRenderSystem::buildInstanceBatches(const std::vector<EntityID>& visibleCreatures,
                                               EntityManager& entityManager,
                                               const glm::vec3& cameraPos) {
    // Pre-reserve batch capacity for performance
    reserveBatchCapacity(visibleCreatures, entityManager);

    // Group creatures by mesh and material for instancing
    for (EntityID entity : visibleCreatures) {
        if (!hasRequiredComponents(entity, entityManager)) {
            continue;
        }

        const Transform& transform = entityManager.getComponent<Transform>(entity);
        const Renderable& renderable = entityManager.getComponent<Renderable>(entity);

        // Calculate distance for LOD (future enhancement)
        float distance = glm::length(transform.position - cameraPos);

        // Create instance data
        InstanceData instanceData(
            transform.getModelMatrix(),
            renderable.materialId,
            0.0f // LOD level - will be calculated later
        );

        // Generate batch key (mesh + material combination)
        std::string batchKey = generateBatchKey(renderable.meshPath, renderable.materialId);

        // Add to appropriate batch
        auto& batch = instanceBatches_[batchKey];
        if (batch.meshPath.empty()) {
            // Initialize new batch
            batch.meshPath = renderable.meshPath;
            batch.baseMaterialId = renderable.materialId;
        }

        batch.addInstance(entity, instanceData);
    }
}

void CreatureRenderSystem::renderInstanceBatches(VulkanRenderer& renderer) {
    frameStats_.totalDrawCalls = 0;

    for (auto& [batchKey, batch] : instanceBatches_) {
        if (batch.empty()) {
            continue;
        }

        auto batchStart = std::chrono::high_resolution_clock::now();

        // Use new instanced rendering method
        renderer.renderInstancedCreatures(batch.meshPath, batch.instances, batch.baseMaterialId);

        // Count this as one draw call (instanced rendering)
        frameStats_.totalDrawCalls++;

        auto batchEnd = std::chrono::high_resolution_clock::now();
        batch.totalRenderTimeMs = std::chrono::duration<float, std::milli>(batchEnd - batchStart).count();
    }

    #ifdef DEBUG_VERBOSE
    if (!instanceBatches_.empty()) {
        VKMON_DEBUG("Rendered " + std::to_string(frameStats_.renderedCreatures) +
                   " creatures in " + std::to_string(frameStats_.instanceBatches) +
                   " batches with " + std::to_string(frameStats_.totalDrawCalls) + " draw calls");
    }
    #endif
}

void CreatureRenderSystem::clearBatches() {
    for (auto& [key, batch] : instanceBatches_) {
        batch.clear();
    }
}

void CreatureRenderSystem::reserveBatchCapacity(const std::vector<EntityID>& entities, EntityManager& entityManager) {
    // Estimate batch sizes by counting meshes
    std::unordered_map<std::string, size_t> meshCounts;

    for (EntityID entity : entities) {
        if (hasRequiredComponents(entity, entityManager)) {
            const Renderable& renderable = entityManager.getComponent<Renderable>(entity);
            meshCounts[renderable.meshPath]++;
        }
    }

    // Reserve capacity for each batch
    for (const auto& [meshPath, count] : meshCounts) {
        for (auto& [batchKey, batch] : instanceBatches_) {
            if (batch.meshPath == meshPath) {
                batch.reserve(count);
                break;
            }
        }
    }
}

std::string CreatureRenderSystem::generateBatchKey(const std::string& meshPath, uint32_t materialId) const {
    return meshPath + "_mat" + std::to_string(materialId);
}

void CreatureRenderSystem::logPerformanceStats() {
    // Calculate estimated FPS from frame time
    float estimatedFPS = frameStats_.totalFrameTimeMs > 0.0f ?
                        (1000.0f / frameStats_.totalFrameTimeMs) : 0.0f;

    std::ostringstream stats;
    stats << "CreatureRenderSystem Performance Report (Frame " << renderFrameCount_ << "):\n"
          << "  Creatures: " << frameStats_.renderedCreatures << "/" << frameStats_.totalCreatures
          << " (" << (frameStats_.cullingEfficiency() * 100.0f) << "% culled)\n"
          << "  Batches: " << frameStats_.instanceBatches << " (efficiency: "
          << frameStats_.instancingEfficiency() << " creatures/draw)\n"
          << "  Timing: Culling=" << frameStats_.frustumCullingTimeMs
          << "ms, Batching=" << frameStats_.batchingTimeMs
          << "ms, Rendering=" << frameStats_.renderingTimeMs
          << "ms, Total=" << frameStats_.totalFrameTimeMs << "ms\n"
          << "  Draw Calls: " << frameStats_.totalDrawCalls
          << " | Est. FPS: ~" << static_cast<int>(estimatedFPS);

    VKMON_INFO(stats.str());

    // No timer reset needed - frame counter handles periodicity automatically
}

std::vector<std::string> CreatureRenderSystem::getBatchMeshPaths() const {
    std::vector<std::string> paths;
    paths.reserve(instanceBatches_.size());

    for (const auto& [key, batch] : instanceBatches_) {
        paths.push_back(batch.meshPath);
    }

    return paths;
}

const InstancedBatch* CreatureRenderSystem::getBatch(const std::string& meshPath) const {
    for (const auto& [key, batch] : instanceBatches_) {
        if (batch.meshPath == meshPath) {
            return &batch;
        }
    }
    return nullptr;
}

} // namespace VulkanMon