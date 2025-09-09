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
    
    // Calculate camera direction vectors
    glm::vec3 cameraDirection = glm::normalize(target - position);
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDirection, up));
    
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
    return glm::lookAt(position, target, up);
}