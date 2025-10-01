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

    // Update view and projection matrices for all cameras
    for (size_t i = 0; i < cameras.size(); ++i) {
        EntityID entity = entityIds[i];

        if (hasRequiredComponents(entity, entityManager)) {
            Transform& transform = entityManager.getComponent<Transform>(entity);
            Camera& camera = cameras[i];

            // Update projection matrix (aspect ratio from window, camera FOV/near/far)
            camera.updateProjectionMatrix();

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

void CameraSystem::handleWindowResize(int width, int height, EntityManager& entityManager) {
    // Validate dimensions to prevent division by zero
    if (width <= 0 || height <= 0) {
        VKMON_WARNING("CameraSystem: Invalid window dimensions for resize: " +
                      std::to_string(width) + "x" + std::to_string(height));
        return;
    }

    if (activeCameraEntity == INVALID_ENTITY) {
        VKMON_WARNING("CameraSystem: No active camera entity for resize handling");
        return; // No active camera to update
    }

    Camera* camera = getActiveCamera(entityManager);
    if (!camera) {
        VKMON_WARNING("CameraSystem: Active camera component not found for entity " + std::to_string(static_cast<uint32_t>(activeCameraEntity)));
        return; // Camera component not found
    }

    // Calculate new aspect ratio
    float newAspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float oldAspectRatio = camera->aspectRatio;

    // Update camera aspect ratio and regenerate projection matrix
    if (camera->type == Camera::Type::PERSPECTIVE) {
        camera->setPerspective(camera->fov, newAspectRatio, camera->nearPlane, camera->farPlane);

        VKMON_INFO("Camera aspect ratio updated from " + std::to_string(oldAspectRatio) +
                   " to " + std::to_string(newAspectRatio) +
                   " for window size " + std::to_string(width) + "x" + std::to_string(height));
    }
    // Note: Orthographic cameras don't use aspect ratio in the same way,
    // they would need different handling if needed in the future
}

// =============================================================================
// UNIFIED CAMERA INTERFACE - Clean matrix providers
// =============================================================================

glm::mat4 CameraSystem::getActiveViewMatrix(EntityManager& entityManager) {
    if (activeCameraEntity == INVALID_ENTITY) {
        VKMON_WARNING("CameraSystem: No active camera for view matrix");
        // Return default view matrix (camera at Z=25 looking at origin)
        return glm::lookAt(glm::vec3(0.0f, 15.0f, 25.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    Camera* camera = getActiveCamera(entityManager);
    if (camera) {
        // Pure getter - matrices are updated in the update() method
        return camera->viewMatrix;
    }

    // Fallback
    VKMON_WARNING("CameraSystem: Active camera component not found");
    return glm::lookAt(glm::vec3(0.0f, 15.0f, 25.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 CameraSystem::getActiveProjectionMatrix(EntityManager& entityManager) {
    if (activeCameraEntity == INVALID_ENTITY) {
        VKMON_WARNING("CameraSystem: No active camera for projection matrix");
        // Return default projection matrix using unified config
        return glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 200.0f);
    }

    Camera* camera = getActiveCamera(entityManager);
    if (camera) {
        // Pure getter - projection matrix should be updated elsewhere when needed
        return camera->projectionMatrix;
    }

    // Fallback
    VKMON_WARNING("CameraSystem: Active camera component not found");
    return glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.1f, 200.0f);
}

bool CameraSystem::hasActiveCamera(EntityManager& entityManager) {
    return activeCameraEntity != INVALID_ENTITY &&
           entityManager.hasComponent<Camera>(activeCameraEntity);
}

CameraSystem::Ray CameraSystem::screenToWorldRay(float screenX, float screenY, float screenWidth, float screenHeight, EntityManager& entityManager) {
    Ray ray;

    // Default ray pointing forward if no active camera
    if (!hasActiveCamera(entityManager)) {
        VKMON_WARNING("CameraSystem: No active camera for ray projection, using default ray");
        ray.origin = glm::vec3(0.0f, 15.0f, 25.0f);
        ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
        return ray;
    }

    Camera* camera = getActiveCamera(entityManager);
    Transform* transform = getActiveCameraTransform(entityManager);

    if (!camera || !transform) {
        VKMON_WARNING("CameraSystem: Invalid camera/transform for ray projection");
        ray.origin = glm::vec3(0.0f, 15.0f, 25.0f);
        ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
        return ray;
    }

    // Convert screen coordinates to NDC (Normalized Device Coordinates)
    // Screen Y is typically inverted (0 at top), so flip it
    float ndcX = (2.0f * screenX) / screenWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY) / screenHeight;  // Flip Y axis

    // Create ray in clip space (NDC with depth)
    glm::vec4 rayClipSpace = glm::vec4(ndcX, ndcY, -1.0f, 1.0f);

    // Transform to eye space (camera space)
    glm::mat4 invProjection = glm::inverse(camera->projectionMatrix);
    glm::vec4 rayEyeSpace = invProjection * rayClipSpace;

    // Only XY are relevant in eye space, set Z to forward, W to 0 (direction vector)
    rayEyeSpace = glm::vec4(rayEyeSpace.x, rayEyeSpace.y, -1.0f, 0.0f);

    // Transform to world space
    glm::mat4 invView = glm::inverse(camera->viewMatrix);
    glm::vec4 rayWorldSpace = invView * rayEyeSpace;

    // Extract world space ray
    ray.origin = transform->position;  // Camera position is ray origin
    ray.direction = glm::normalize(glm::vec3(rayWorldSpace.x, rayWorldSpace.y, rayWorldSpace.z));

    // Debug logging for ray projection
    VKMON_DEBUG("Ray projection: Screen(" + std::to_string(screenX) + "," + std::to_string(screenY) +
                ") -> Origin(" + std::to_string(ray.origin.x) + "," + std::to_string(ray.origin.y) + "," + std::to_string(ray.origin.z) +
                ") Dir(" + std::to_string(ray.direction.x) + "," + std::to_string(ray.direction.y) + "," + std::to_string(ray.direction.z) + ")");

    return ray;
}


} // namespace VulkanMon