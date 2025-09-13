#include "CameraSystem.h"
#include "../utils/Logger.h"
#include <limits>

namespace VulkanMon {

void CameraSystem::update(float deltaTime, EntityManager& entityManager) {
    // Find the active camera (highest priority active camera)
    EntityID bestCamera = INVALID_ENTITY;
    int highestPriority = std::numeric_limits<int>::min();

    auto& cameras = entityManager.getAllComponents<Camera>();
    auto& entityIds = entityManager.getEntitiesWithComponent<Camera>();

    for (size_t i = 0; i < cameras.size(); ++i) {
        EntityID entity = entityIds[i];

        if (hasRequiredComponents(entity, entityManager)) {
            Camera& camera = cameras[i];

            if (camera.shouldRender() && camera.priority > highestPriority) {
                bestCamera = entity;
                highestPriority = camera.priority;
            }
        }
    }

    // Log camera changes
    if (activeCameraEntity != bestCamera) {
        if (bestCamera != INVALID_ENTITY) {
            VKMON_DEBUG("CameraSystem: Active camera changed to entity " + std::to_string(bestCamera));
        } else {
            VKMON_WARNING("CameraSystem: No active camera found");
        }
        activeCameraEntity = bestCamera;
    }

    // Update view matrices for all cameras based on their transforms
    for (size_t i = 0; i < cameras.size(); ++i) {
        EntityID entity = entityIds[i];

        if (hasRequiredComponents(entity, entityManager)) {
            Transform& transform = entityManager.getComponent<Transform>(entity);
            Camera& camera = cameras[i];

            // Update view matrix from transform
            camera.updateViewMatrix(
                transform.position,
                transform.getForward(),
                transform.getUp()
            );
        }
    }
}

Camera* CameraSystem::getActiveCamera(EntityManager& entityManager) {
    if (activeCameraEntity == INVALID_ENTITY) {
        return nullptr;
    }

    if (entityManager.hasComponent<Camera>(activeCameraEntity)) {
        return &entityManager.getComponent<Camera>(activeCameraEntity);
    }

    return nullptr;
}

Transform* CameraSystem::getActiveCameraTransform(EntityManager& entityManager) {
    if (activeCameraEntity == INVALID_ENTITY) {
        return nullptr;
    }

    if (entityManager.hasComponent<Transform>(activeCameraEntity)) {
        return &entityManager.getComponent<Transform>(activeCameraEntity);
    }

    return nullptr;
}

glm::mat4 CameraSystem::getActiveViewProjectionMatrix(EntityManager& entityManager) {
    Camera* camera = this->getActiveCamera(entityManager);
    if (camera) {
        return camera->getViewProjectionMatrix();
    }

    // Return identity matrix if no active camera
    VKMON_WARNING("CameraSystem: No active camera for view-projection matrix");
    return glm::mat4(1.0f);
}

glm::vec3 CameraSystem::getActiveCameraPosition(EntityManager& entityManager) {
    Transform* transform = this->getActiveCameraTransform(entityManager);
    if (transform) {
        return transform->position;
    }

    // Return origin if no active camera
    return glm::vec3(0.0f);
}

} // namespace VulkanMon