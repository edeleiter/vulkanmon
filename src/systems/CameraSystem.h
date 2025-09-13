#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/Camera.h"
#include "../core/EntityManager.h"

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
};

} // namespace VulkanMon