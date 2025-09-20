#pragma once

#include "../core/Entity.h"
#include "../config/CameraConfig.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace VulkanMon {

struct Camera {
    VKMON_COMPONENT(Camera)

    // Camera parameters - using unified configuration defaults
    float fov = Config::Camera::DEFAULT_FOV;              // Field of view in degrees
    float nearPlane = Config::Camera::DEFAULT_NEAR_PLANE; // Near clipping plane
    float farPlane = Config::Camera::DEFAULT_FAR_PLANE;   // Far clipping plane
    float aspectRatio = Config::Camera::DEFAULT_ASPECT_RATIO; // Aspect ratio

    // Camera state
    bool isActive = true;       // Whether this camera is currently active
    int priority = 0;           // Higher priority cameras are preferred

    // Cached matrices (updated when transform changes)
    mutable glm::mat4 viewMatrix = glm::mat4(1.0f);
    mutable glm::mat4 projectionMatrix = glm::mat4(1.0f);
    mutable glm::mat4 viewProjectionMatrix = glm::mat4(1.0f);
    mutable bool isDirty = true;

    // Camera type
    enum class Type {
        PERSPECTIVE,
        ORTHOGRAPHIC
    } type = Type::PERSPECTIVE;

    // Orthographic camera parameters (only used if type == ORTHOGRAPHIC)
    float orthoLeft = -10.0f;
    float orthoRight = 10.0f;
    float orthoBottom = -10.0f;
    float orthoTop = 10.0f;

    // Constructor
    Camera() = default;

    Camera(float fieldOfView, float aspect, float near, float far)
        : fov(fieldOfView), aspectRatio(aspect), nearPlane(near), farPlane(far) {}

    // Update projection matrix (call when camera parameters change)
    void updateProjectionMatrix() const {
        if (type == Type::PERSPECTIVE) {
            projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        } else {
            projectionMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
        }
        isDirty = true;
    }

    // Update view matrix from transform (call when transform changes)
    void updateViewMatrix(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) const {
        viewMatrix = glm::lookAt(position, position + forward, up);
        isDirty = true;
    }

    // Get combined view-projection matrix
    const glm::mat4& getViewProjectionMatrix() const {
        if (isDirty) {
            viewProjectionMatrix = projectionMatrix * viewMatrix;
            isDirty = false;
        }
        return viewProjectionMatrix;
    }

    // Helper methods
    void setPerspective(float fieldOfView, float aspect, float near, float far) {
        type = Type::PERSPECTIVE;
        fov = fieldOfView;
        aspectRatio = aspect;
        nearPlane = near;
        farPlane = far;
        updateProjectionMatrix();
    }

    void setOrthographic(float left, float right, float bottom, float top, float near, float far) {
        type = Type::ORTHOGRAPHIC;
        orthoLeft = left;
        orthoRight = right;
        orthoBottom = bottom;
        orthoTop = top;
        nearPlane = near;
        farPlane = far;
        updateProjectionMatrix();
    }

    void setActive(bool active) { isActive = active; }
    void setPriority(int prio) { priority = prio; }

    // Check if this camera should be used for rendering
    bool shouldRender() const { return isActive; }
};

} // namespace VulkanMon