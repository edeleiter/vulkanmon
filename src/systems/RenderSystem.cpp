#include "RenderSystem.h"
#include "../utils/Logger.h"

namespace VulkanMon {

void RenderSystem::update(float deltaTime, EntityManager& entityManager) {
    // Reset statistics
    renderedObjectCount = 0;
    culledObjectCount = 0;

    // Future: Update animations, LOD calculations, etc.
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