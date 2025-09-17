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

Frustum CameraSystem::getActiveCameraFrustum(EntityManager& entityManager) {
    Frustum frustum;

    // Get view-projection matrix from active camera
    glm::mat4 viewProjectionMatrix = getActiveViewProjectionMatrix(entityManager);

    // Generate frustum planes from matrix
    frustum.updateFromMatrix(viewProjectionMatrix);

    return frustum;
}

// =============================================================================
// UNIFIED CAMERA INTERFACE - Clean matrix providers
// =============================================================================

glm::mat4 CameraSystem::getActiveViewMatrix() {
    if (!cachedEntityManager_ || activeCameraEntity == INVALID_ENTITY) {
        VKMON_WARNING("CameraSystem: No active camera or EntityManager for view matrix");
        // Return default view matrix (camera at Z=25 looking at origin)
        return glm::lookAt(glm::vec3(0.0f, 15.0f, 25.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    Camera* camera = getActiveCamera(*cachedEntityManager_);
    if (camera) {
        // Ensure camera matrices are up to date
        Transform* cameraTransform = getActiveCameraTransform(*cachedEntityManager_);
        if (cameraTransform) {
            camera->updateViewMatrix(cameraTransform->position, cameraTransform->getForward(), cameraTransform->getUp());
        }
        return camera->viewMatrix;
    }

    // Fallback
    VKMON_WARNING("CameraSystem: Active camera component not found");
    return glm::lookAt(glm::vec3(0.0f, 15.0f, 25.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 CameraSystem::getActiveProjectionMatrix() {
    if (!cachedEntityManager_ || activeCameraEntity == INVALID_ENTITY) {
        VKMON_WARNING("CameraSystem: No active camera or EntityManager for projection matrix");
        // Return default projection matrix using unified config
        return glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 200.0f);
    }

    Camera* camera = getActiveCamera(*cachedEntityManager_);
    if (camera) {
        // Ensure projection matrix is up to date
        camera->updateProjectionMatrix();
        return camera->projectionMatrix;
    }

    // Fallback
    VKMON_WARNING("CameraSystem: Active camera component not found");
    return glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 200.0f);
}

bool CameraSystem::hasActiveCamera() {
    return cachedEntityManager_ != nullptr &&
           activeCameraEntity != INVALID_ENTITY &&
           cachedEntityManager_->hasComponent<Camera>(activeCameraEntity);
}

glm::vec3 CameraSystem::getActiveCameraPosition() {
    if (!cachedEntityManager_) {
        VKMON_WARNING("CameraSystem: No cached EntityManager for camera position");
        return glm::vec3(0.0f, 8.0f, 15.0f);  // ECS camera fallback position
    }

    if (activeCameraEntity == INVALID_ENTITY) {
        VKMON_WARNING("CameraSystem: No active camera entity for position");
        return glm::vec3(0.0f, 8.0f, 15.0f);  // ECS camera fallback position
    }

    if (cachedEntityManager_->hasComponent<Transform>(activeCameraEntity)) {
        Transform& transform = cachedEntityManager_->getComponent<Transform>(activeCameraEntity);
        return transform.position;
    }

    // Fallback
    VKMON_WARNING("CameraSystem: Active camera transform not found");
    return glm::vec3(0.0f, 8.0f, 15.0f);  // ECS camera fallback position
}

} // namespace VulkanMon