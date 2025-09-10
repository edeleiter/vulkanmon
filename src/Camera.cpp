#include "Camera.h"
#include <GLFW/glfw3.h>

// Configuration constants from main.cpp
constexpr float CAMERA_SPEED = 2.5f;

Camera::Camera(glm::vec3 pos, glm::vec3 target, glm::vec3 up) 
    : position(pos), target(target), up(up) {
    lastFrameTime = std::chrono::high_resolution_clock::now();
}

void Camera::processInput(GLFWwindow* window) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastFrameTime).count();
    lastFrameTime = currentTime;
    
    float velocity = CAMERA_SPEED * deltaTime;
    
    // Calculate camera direction vectors with edge case handling
    glm::vec3 directionVec = target - position;
    
    // Handle case where position == target (zero direction vector)
    if (glm::length(directionVec) < 1e-6f) {
        // Use default forward direction if position and target are the same
        directionVec = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    glm::vec3 cameraDirection = glm::normalize(directionVec);
    
    // Handle case where up vector is zero or parallel to camera direction
    glm::vec3 safeUp = up;
    if (glm::length(safeUp) < 1e-6f) {
        // Use default up vector if current up is zero
        safeUp = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    
    glm::vec3 rightVec = glm::cross(cameraDirection, safeUp);
    if (glm::length(rightVec) < 1e-6f) {
        // Camera direction and up are parallel, use alternative up vector
        safeUp = glm::vec3(0.0f, 1.0f, 0.0f);
        rightVec = glm::cross(cameraDirection, safeUp);
    }
    glm::vec3 cameraRight = glm::normalize(rightVec);
    
    // WASD movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        position += velocity * cameraDirection;
        target += velocity * cameraDirection;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        position -= velocity * cameraDirection;  
        target -= velocity * cameraDirection;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        position -= velocity * cameraRight;
        target -= velocity * cameraRight;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        position += velocity * cameraRight;
        target += velocity * cameraRight;
    }
    
    // Optional: Add Q/E for up/down movement
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        position -= velocity * up;
        target -= velocity * up;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        position += velocity * up;
        target += velocity * up;
    }
}

glm::mat4 Camera::getViewMatrix() const {
    // Handle edge cases for glm::lookAt to prevent NaN values
    glm::vec3 safeTarget = target;
    glm::vec3 safeUp = up;
    
    // Handle case where position == target
    if (glm::length(target - position) < 1e-6f) {
        // Create a safe target slightly in front of the camera
        safeTarget = position + glm::vec3(0.0f, 0.0f, -1.0f);
    }
    
    // Handle case where up vector is zero
    if (glm::length(up) < 1e-6f) {
        safeUp = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    
    // Handle case where up vector is parallel to view direction
    glm::vec3 viewDirection = glm::normalize(safeTarget - position);
    if (glm::length(glm::cross(viewDirection, safeUp)) < 1e-6f) {
        // Up and view direction are parallel, use alternative up vector
        safeUp = glm::vec3(0.0f, 1.0f, 0.0f);
        
        // If still parallel, use another alternative
        if (glm::length(glm::cross(viewDirection, safeUp)) < 1e-6f) {
            safeUp = glm::vec3(1.0f, 0.0f, 0.0f);
        }
    }
    
    return glm::lookAt(position, safeTarget, safeUp);
}