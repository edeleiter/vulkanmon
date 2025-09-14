#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"
#include "../rendering/VulkanRenderer.h"
#include "../utils/Logger.h"
#include "CameraSystem.h"
#include <vector>
#include <algorithm>

namespace VulkanMon {

class RenderSystem : public System<Transform, Renderable> {
private:
    // Rendering statistics
    size_t renderedObjectCount = 0;
    size_t culledObjectCount = 0;

    // Simple frustum culling distance (will be enhanced later)
    float maxRenderDistance = 1000.0f;

    // Reference to camera system for getting active camera
    CameraSystem* cameraSystem = nullptr;

    // Render command structure for sorting
    struct RenderCommand {
        EntityID entity;
        Transform* transform;
        Renderable* renderable;
        float distanceToCamera;

        // Comparator for sorting (back-to-front for transparency, front-to-back for opaque)
        bool operator<(const RenderCommand& other) const {
            // First sort by render layer
            if (renderable->renderLayer != other.renderable->renderLayer) {
                return renderable->renderLayer < other.renderable->renderLayer;
            }

            // Then by material (to minimize state changes)
            if (renderable->materialId != other.renderable->materialId) {
                return renderable->materialId < other.renderable->materialId;
            }

            // Finally by distance (front-to-back for opaque objects)
            return distanceToCamera < other.distanceToCamera;
        }
    };

public:
    // Constructor
    RenderSystem(CameraSystem* camSystem = nullptr) : cameraSystem(camSystem) {}

    // Set camera system reference
    void setCameraSystem(CameraSystem* camSystem) { cameraSystem = camSystem; }

    void update(float deltaTime, EntityManager& entityManager) override;

    void render(VulkanRenderer& renderer, EntityManager& entityManager) override {
        // Get camera position for distance calculations
        glm::vec3 cameraPos(0.0f, 0.0f, 5.0f); // Default fallback position

        if (cameraSystem) {
            cameraPos = cameraSystem->getActiveCameraPosition(entityManager);
        }

        // Collect all render commands
        std::vector<RenderCommand> renderCommands;

        auto& transforms = entityManager.getAllComponents<Transform>();
        auto& entityIds = entityManager.getEntitiesWithComponent<Transform>();

        for (size_t i = 0; i < transforms.size(); ++i) {
            EntityID entity = entityIds[i];

            // Check if entity has both Transform and Renderable components
            if (hasRequiredComponents(entity, entityManager)) {
                Transform& transform = transforms[i];
                Renderable& renderable = entityManager.getComponent<Renderable>(entity);

                // Skip if not visible
                if (!renderable.isVisible) {
                    culledObjectCount++;
                    continue;
                }

                // Calculate distance to camera
                float distance = glm::length(transform.position - cameraPos);

                // Frustum culling (simple distance-based for now)
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
            VKMON_DEBUG("RenderSystem: Rendered " + std::to_string(renderedObjectCount) +
                       " objects, culled " + std::to_string(culledObjectCount));
        }
        #endif
    }

    // Get rendering statistics
    size_t getRenderedObjectCount() const { return renderedObjectCount; }
    size_t getCulledObjectCount() const { return culledObjectCount; }
    size_t getTotalObjectCount() const { return renderedObjectCount + culledObjectCount; }

    // Configuration
    void setMaxRenderDistance(float distance) { maxRenderDistance = distance; }
    float getMaxRenderDistance() const { return maxRenderDistance; }

private:
    void submitRenderCommand(VulkanRenderer& renderer, const RenderCommand& cmd);
};

} // namespace VulkanMon