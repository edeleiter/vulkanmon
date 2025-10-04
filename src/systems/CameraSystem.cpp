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

    // Process camera follow behavior for cameras with CameraFollowComponent
    auto followCameras = entityManager.getEntitiesWithComponent<CameraFollowComponent>();
    for (EntityID entity : followCameras) {
        if (entityManager.hasComponent<Transform>(entity) &&
            entityManager.hasComponent<Camera>(entity)) {
            processCameraFollow(entity, entityManager, deltaTime);
        }
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

void CameraSystem::processCameraFollow(EntityID entity, EntityManager& entityManager, float deltaTime) {
    // deltaTime is already in seconds (World::update converts from ms)
    float deltaSeconds = deltaTime;

    auto& follow = entityManager.getComponent<CameraFollowComponent>(entity);
    auto& cameraTransform = entityManager.getComponent<Transform>(entity);

    // Check if follow is active and has valid target
    if (!follow.isActive()) {
        return;
    }

    // Validate target exists and has Transform
    if (!entityManager.hasComponent<Transform>(follow.targetEntity)) {
        VKMON_WARNING("CameraFollowComponent: Target entity " + std::to_string(follow.targetEntity) +
                      " does not have Transform component");
        return;
    }

    const auto& targetTransform = entityManager.getComponent<Transform>(follow.targetEntity);

    // Calculate desired camera position based on follow mode
    glm::vec3 desiredPosition = calculateFollowPosition(targetTransform, follow);

    // Apply position smoothing
    cameraTransform.position = smoothPosition(cameraTransform.position, desiredPosition,
                                             follow.positionSmoothness, deltaSeconds);

    // Calculate look-at position (target position + look-at offset)
    glm::vec3 lookAtPosition = targetTransform.position + follow.lookAtOffset;

    // Update camera orientation to look at target
    glm::vec3 forward = glm::normalize(lookAtPosition - cameraTransform.position);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(right, forward);

    // Apply rotation smoothing if needed
    if (follow.rotationSmoothness > 0.0f) {
        // Smooth rotation by lerping the forward vector
        glm::vec3 currentForward = cameraTransform.getForward();
        forward = glm::normalize(glm::mix(forward, currentForward, follow.rotationSmoothness));

        // Recalculate right and up after smoothing
        right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        up = glm::cross(right, forward);
    }

    // Calculate rotation quaternion from forward and up vectors
    // Build rotation matrix from basis vectors
    glm::mat3 rotationMatrix;
    rotationMatrix[0] = right;    // X-axis
    rotationMatrix[1] = up;       // Y-axis
    rotationMatrix[2] = -forward; // Z-axis (negated because forward is -Z in camera space)

    // Convert to quaternion and set rotation
    cameraTransform.setRotation(glm::quat_cast(rotationMatrix));
}

glm::vec3 CameraSystem::calculateFollowPosition(const Transform& targetTransform,
                                                const CameraFollowComponent& follow) {
    glm::vec3 position;

    switch (follow.mode) {
        case CameraFollowComponent::FollowMode::ThirdPerson: {
            // Position camera relative to target's rotation if followRotation is true
            if (follow.followRotation) {
                // Transform offset by target's rotation
                glm::mat4 rotationMatrix = glm::mat4_cast(targetTransform.rotation);
                glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(follow.offset, 0.0f));
                position = targetTransform.position + rotatedOffset;
            } else {
                // Use world-space offset
                position = targetTransform.position + follow.offset;
            }
            break;
        }

        case CameraFollowComponent::FollowMode::Orbit: {
            // Maintain distance from target, following rotation
            glm::mat4 rotationMatrix = glm::mat4_cast(targetTransform.rotation);
            glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(follow.offset, 0.0f));
            position = targetTransform.position + rotatedOffset;
            break;
        }

        case CameraFollowComponent::FollowMode::TopDown: {
            // Follow XZ position, maintain fixed Y offset
            position = glm::vec3(targetTransform.position.x,
                               targetTransform.position.y + follow.offset.y,
                               targetTransform.position.z);
            break;
        }

        case CameraFollowComponent::FollowMode::FirstPerson: {
            // Match target position with small offset
            glm::mat4 rotationMatrix = glm::mat4_cast(targetTransform.rotation);
            glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(follow.offset, 0.0f));
            position = targetTransform.position + rotatedOffset;
            break;
        }

        case CameraFollowComponent::FollowMode::Free:
        case CameraFollowComponent::FollowMode::Custom:
        default: {
            // No follow behavior for Free/Custom modes
            position = targetTransform.position + follow.offset;
            break;
        }
    }

    return position;
}

glm::vec3 CameraSystem::smoothPosition(const glm::vec3& current, const glm::vec3& target,
                                       float smoothness, float deltaTime) {
    if (smoothness <= 0.0f) {
        // No smoothing - instant movement
        return target;
    }

    // Exponential smoothing using lerp
    // Higher smoothness = slower movement
    float t = 1.0f - glm::exp(-deltaTime / glm::max(smoothness, 0.001f));
    return glm::mix(current, target, t);
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