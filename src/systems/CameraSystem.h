#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/Camera.h"
#include "../core/EntityManager.h"
#include "../spatial/SpatialManager.h"

namespace VulkanMon {

class CameraSystem : public System<Transform, Camera> {
private:
    EntityID activeCameraEntity = INVALID_ENTITY;

public:
    void update(float deltaTime, EntityManager& entityManager) override;

    // Get the currently active camera entity
    EntityID getActiveCameraEntity() const {
        return activeCameraEntity;
    }

    // Get the active camera component (returns nullptr if no active camera)
    Camera* getActiveCamera(EntityManager& entityManager);

    // Get the active camera transform (returns nullptr if no active camera)
    Transform* getActiveCameraTransform(EntityManager& entityManager);

    // Helper to get view-projection matrix from active camera
    glm::mat4 getActiveViewProjectionMatrix(EntityManager& entityManager);

    // Helper to get camera position for distance calculations
    glm::vec3 getActiveCameraPosition(EntityManager& entityManager);

    // Generate frustum from active camera for spatial culling
    Frustum getActiveCameraFrustum(EntityManager& entityManager);

    // Window resize handling - updates camera aspect ratio
    void handleWindowResize(int width, int height, EntityManager& entityManager);

    // =========================================================================
    // UNIFIED CAMERA INTERFACE - Simple matrix providers (require EntityManager parameter)
    // =========================================================================

    /// Get view matrix from active camera
    glm::mat4 getActiveViewMatrix(EntityManager& entityManager);

    /// Get projection matrix from active camera
    glm::mat4 getActiveProjectionMatrix(EntityManager& entityManager);

    /// Check if active camera is available
    bool hasActiveCamera(EntityManager& entityManager);
};

} // namespace VulkanMon