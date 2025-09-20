#include "RenderSystem.h"
#include "SpatialSystem.h"
#include <cassert>
#include "../utils/Logger.h"
#include "../game/CreatureDetectionSystem.h"  // For CreatureComponent

namespace VulkanMon {

void RenderSystem::update(float deltaTime, EntityManager& entityManager) {
    // Reset statistics
    renderedObjectCount = 0;
    culledObjectCount = 0;

    // Future: Update animations, LOD calculations, etc.
}

void RenderSystem::render(VulkanRenderer& renderer, EntityManager& entityManager) {
    // Debug safety assertions (zero cost in release builds)
    assert(cameraSystem && "CameraSystem must be set before rendering");
    // PERFORMANCE TEST: Allow rendering without spatial system (will use fallback)
    // assert(spatialSystem && "SpatialSystem must be set before rendering");

    // Reset statistics
    renderedObjectCount = 0;
    culledObjectCount = 0;

    // Get camera position for distance calculations (fallback)
    glm::vec3 cameraPos(0.0f, 80.0f, 120.0f);  // Updated fallback to match actual camera position
    if (cameraSystem) {
        cameraPos = cameraSystem->getActiveCameraPosition(entityManager);
    } else {
        VKMON_WARNING("RenderSystem: No CameraSystem available - using fallback position");
    }

    // Collect potentially visible entities using spatial culling
    std::vector<EntityID> candidateEntities;

    if (spatialSystem && cameraSystem) {
        // NEW: Use spatial frustum culling for efficiency
        Frustum cameraFrustum = cameraSystem->getActiveCameraFrustum(entityManager);
        candidateEntities = spatialSystem->queryFrustum(cameraFrustum);

        #ifdef DEBUG_VERBOSE
        VKMON_DEBUG("SpatialSystem frustum culling returned " + std::to_string(candidateEntities.size()) + " candidates");
        #endif
    } else {
        // Fallback: Get all entities with Transform component
        candidateEntities = entityManager.getEntitiesWithComponent<Transform>();

        #ifdef DEBUG_VERBOSE
        VKMON_DEBUG("Using fallback entity collection: " + std::to_string(candidateEntities.size()) + " entities");
        #endif
    }

    // Build render commands from candidate entities
    std::vector<RenderCommand> renderCommands;

    for (EntityID entity : candidateEntities) {
        // Check if entity has both Transform and Renderable components
        if (hasRequiredComponents(entity, entityManager)) {
            // IMPORTANT: Skip entities with CreatureComponent - they're handled by CreatureRenderSystem
            if (entityManager.hasComponent<CreatureComponent>(entity)) {
                continue;
            }
            Transform& transform = entityManager.getComponent<Transform>(entity);
            Renderable& renderable = entityManager.getComponent<Renderable>(entity);

            // Skip if not visible
            if (!renderable.isVisible) {
                culledObjectCount++;
                continue;
            }

            // Calculate distance to camera for LOD and additional culling
            float distance = glm::length(transform.position - cameraPos);

            // Additional distance-based culling for very far objects
            if (!renderable.shouldRender(distance) || distance > maxRenderDistance) {
                culledObjectCount++;
                continue;
            }

            // Add to render commands
            renderCommands.push_back({
                entity,
                &transform,
                &renderable,
                distance
            });
        }
    }

    // Sort render commands for optimal rendering
    std::sort(renderCommands.begin(), renderCommands.end());

    // Submit render commands to renderer
    for (const auto& cmd : renderCommands) {
        submitRenderCommand(renderer, cmd);
        renderedObjectCount++;
    }

    // Log rendering statistics (only in debug builds)
    #ifdef DEBUG
    if ((renderedObjectCount + culledObjectCount) > 0) {
        std::string cullingMethod = (spatialSystem && cameraSystem) ? "spatial+frustum" : "fallback";
        VKMON_DEBUG("RenderSystem (" + cullingMethod + "): Rendered " + std::to_string(renderedObjectCount) +
                   " objects, culled " + std::to_string(culledObjectCount));
    }
    #endif
}

void RenderSystem::submitRenderCommand(VulkanRenderer& renderer, const RenderCommand& cmd) {
    // Get the model matrix from transform
    const glm::mat4& modelMatrix = cmd.transform->getModelMatrix();

    // Submit render command to VulkanRenderer
    renderer.renderECSObject(
        modelMatrix,
        cmd.renderable->meshPath,
        cmd.renderable->materialId
    );

    // Log render command for debugging (verbose)
    #ifdef DEBUG_VERBOSE
    VKMON_DEBUG("Submitted ECS render command for entity " + std::to_string(cmd.entity) +
               " with mesh: " + cmd.renderable->meshPath +
               ", material: " + std::to_string(cmd.renderable->materialId));
    #endif
}

} // namespace VulkanMon