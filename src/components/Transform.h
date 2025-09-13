#pragma once

#include "../core/Entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VulkanMon {

struct Transform {
    VKMON_COMPONENT(Transform)

    // Position in world space
    glm::vec3 position = glm::vec3(0.0f);

    // Rotation as quaternion (more stable than Euler angles)
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    // Scale factors
    glm::vec3 scale = glm::vec3(1.0f);

    // Cached model matrix (updated when transform changes)
    mutable glm::mat4 modelMatrix = glm::mat4(1.0f);
    mutable bool isDirty = true;

    // Get the model matrix (automatically recalculates if dirty)
    const glm::mat4& getModelMatrix() const {
        if (isDirty) {
            updateModelMatrix();
        }
        return modelMatrix;
    }

    // Mark transform as dirty (needs recalculation)
    void markDirty() {
        isDirty = true;
    }

    // Set position and mark dirty
    void setPosition(const glm::vec3& newPosition) {
        position = newPosition;
        markDirty();
    }

    // Set rotation and mark dirty
    void setRotation(const glm::quat& newRotation) {
        rotation = glm::normalize(newRotation);
        markDirty();
    }

    // Set rotation from Euler angles (degrees)
    void setRotationEuler(float pitch, float yaw, float roll) {
        rotation = glm::quat(glm::radians(glm::vec3(pitch, yaw, roll)));
        markDirty();
    }

    // Set scale and mark dirty
    void setScale(const glm::vec3& newScale) {
        scale = newScale;
        markDirty();
    }

    // Set uniform scale
    void setScale(float uniformScale) {
        scale = glm::vec3(uniformScale);
        markDirty();
    }

    // Get forward vector (local Z-axis)
    glm::vec3 getForward() const {
        return rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    }

    // Get right vector (local X-axis)
    glm::vec3 getRight() const {
        return rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // Get up vector (local Y-axis)
    glm::vec3 getUp() const {
        return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    }

private:
    // Update the cached model matrix
    void updateModelMatrix() const {
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

        modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        isDirty = false;
    }
};

} // namespace VulkanMon