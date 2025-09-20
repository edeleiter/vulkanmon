#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"
#include "../rendering/VulkanRenderer.h"
#include "../utils/Logger.h"
#include "CameraSystem.h"
#include <vector>
#include <algorithm>
#include <cassert>

// Forward declaration to avoid circular dependency
namespace VulkanMon {
    class SpatialSystem;
}

namespace VulkanMon {

class RenderSystem : public System<Transform, Renderable> {
private:
    // Rendering statistics
    size_t renderedObjectCount = 0;
    size_t culledObjectCount = 0;

    // Simple frustum culling distance (will be enhanced later)
    float maxRenderDistance = 1000.0f;

    // References to other systems (non-owning, lifetime managed by World)
    CameraSystem* cameraSystem = nullptr;   // Safe: World guarantees lifetime
    SpatialSystem* spatialSystem = nullptr; // Safe: World guarantees lifetime

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
    RenderSystem(CameraSystem* camSystem = nullptr, SpatialSystem* spatialSys = nullptr)
        : cameraSystem(camSystem), spatialSystem(spatialSys) {}

    // Set system references (with safety logging)
    void setCameraSystem(CameraSystem* camSystem) {
        cameraSystem = camSystem;
        VKMON_INFO("RenderSystem: CameraSystem reference updated");
    }

    void setSpatialSystem(SpatialSystem* spatialSys) {
        spatialSystem = spatialSys;
        VKMON_INFO("RenderSystem: SpatialSystem reference updated");
    }

    void update(float deltaTime, EntityManager& entityManager) override;

    void render(VulkanRenderer& renderer, EntityManager& entityManager) override;

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