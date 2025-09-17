#include "InputHandler.h"
#include "../utils/Logger.h"
#include "../systems/LightingSystem.h"
#include "../components/Transform.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace VulkanMon {

InputHandler::InputHandler(
    std::shared_ptr<Window> window,
    CameraSystem* cameraSystem,
    World* world
) : window_(window), cameraSystem_(cameraSystem), world_(world) {

    VKMON_INFO("InputHandler initialized with ECS camera system");
    VKMON_DEBUG(window_ ? "  Window: connected" : "  Window: null");
    VKMON_DEBUG(cameraSystem_ ? "  CameraSystem: connected" : "  CameraSystem: null");
    VKMON_DEBUG(world_ ? "  ECS World: connected" : "  ECS World: null");
}

void InputHandler::processKeyInput(int key, int scancode, int action, int mods) {
    // Only process key press events (not release or repeat)
    if (action != GLFW_PRESS) {
        return;
    }
    
    // Route to appropriate handler based on key
    switch (key) {
        // System controls
        case GLFW_KEY_R:
            handleSystemControls(key);
            break;
            
        // Lighting controls (1, 2, 3, 4, L)
        case GLFW_KEY_1:
        case GLFW_KEY_2:
        case GLFW_KEY_3:
        case GLFW_KEY_4:
        case GLFW_KEY_L:
            handleLightingControls(key);
            break;
            
        // Material controls (M, 5, 6)
        case GLFW_KEY_M:
        case GLFW_KEY_5:
        case GLFW_KEY_6:
            handleMaterialControls(key);
            break;
            
        // Help
        case GLFW_KEY_H:
            printControlsHelp();
            break;

        // Tab key - Toggle between camera mode and UI mode
        case GLFW_KEY_TAB:
            toggleInputMode();
            break;

        // Inspector toggle
        case GLFW_KEY_I:
            if (inspectorToggleCallback_) {
                inspectorToggleCallback_();
            }
            break;

        default:
            // Unhandled key - could log for debugging
            VKMON_DEBUG("Unhandled key press: " + std::to_string(key));
            break;
    }
}

void InputHandler::processMouseInput(double xpos, double ypos) {
    if (!cameraSystem_ || !world_ || !mouseLocked_) {
        return;
    }

    // Handle first mouse movement to avoid jump
    if (firstMouse_) {
        lastMouseX_ = static_cast<float>(xpos);
        lastMouseY_ = static_cast<float>(ypos);
        firstMouse_ = false;
        return;
    }

    // Calculate mouse offset
    float xoffset = static_cast<float>(xpos) - lastMouseX_;
    float yoffset = lastMouseY_ - static_cast<float>(ypos); // Reversed: y goes from bottom to top

    lastMouseX_ = static_cast<float>(xpos);
    lastMouseY_ = static_cast<float>(ypos);

    // Apply sensitivity
    xoffset *= mouseSensitivity_;
    yoffset *= mouseSensitivity_;

    // Apply mouse look to active ECS camera entity
    if (cameraSystem_->hasActiveCamera()) {
        EntityID activeCameraEntity = cameraSystem_->getActiveCameraEntity();
        if (world_->hasComponent<Transform>(activeCameraEntity)) {
            Transform& transform = world_->getComponent<Transform>(activeCameraEntity);

            // Update camera rotation (yaw and pitch)
            // Yaw (Y rotation) - left/right mouse movement (flipped)
            transform.rotation.y -= xoffset;  // Fixed: negative for proper left/right direction

            // Pitch (X rotation) - up/down mouse movement
            transform.rotation.x += yoffset;

            // Clamp pitch to prevent over-rotation (±89 degrees)
            transform.rotation.x = glm::clamp(transform.rotation.x, -89.0f, 89.0f);

            // Normalize yaw to 0-360 range
            if (transform.rotation.y > 360.0f) transform.rotation.y -= 360.0f;
            if (transform.rotation.y < 0.0f) transform.rotation.y += 360.0f;
        }
    }
}

void InputHandler::processContinuousInput(GLFWwindow* window, float deltaTime) {
    if (!window || !cameraSystem_ || !world_) {
        return;
    }

    handleCameraMovement(window, deltaTime);
}

void InputHandler::setShaderReloadCallback(ShaderReloadCallback callback) {
    shaderReloadCallback_ = callback;
    VKMON_DEBUG("Shader reload callback registered");
}

void InputHandler::setLightingControlCallback(LightingControlCallback callback) {
    lightingControlCallback_ = callback;
    VKMON_DEBUG("Lighting control callback registered");
}

void InputHandler::setMaterialControlCallback(MaterialControlCallback callback) {
    materialControlCallback_ = callback;
    VKMON_DEBUG("Material control callback registered");
}

void InputHandler::setInspectorToggleCallback(InspectorToggleCallback callback) {
    inspectorToggleCallback_ = callback;
    VKMON_DEBUG("Inspector toggle callback registered");
}

void InputHandler::resetMousePosition() {
    firstMouse_ = true;
    VKMON_DEBUG("Mouse position reset - next movement will not cause jump");
}

void InputHandler::setMouseSensitivity(float sensitivity) {
    mouseSensitivity_ = sensitivity;
    VKMON_DEBUG("Mouse sensitivity set to: " + std::to_string(sensitivity));
}

void InputHandler::setCameraSpeed(float speed) {
    cameraSpeed_ = speed;
    VKMON_DEBUG("Camera speed set to: " + std::to_string(speed));
}

// Private helper methods

void InputHandler::handleCameraMovement(GLFWwindow* window, float deltaTime) {
    // Modern ECS-based WASD camera movement
    if (!cameraSystem_->hasActiveCamera()) {
        return;
    }

    EntityID activeCameraEntity = cameraSystem_->getActiveCameraEntity();
    if (!world_->hasComponent<Transform>(activeCameraEntity)) {
        return;
    }

    Transform& transform = world_->getComponent<Transform>(activeCameraEntity);

    // Calculate movement vectors based on current camera rotation
    float yaw = glm::radians(transform.rotation.y);
    float pitch = glm::radians(transform.rotation.x);

    // Calculate forward, right, and up vectors
    // Fixed coordinate system: forward should be -Z direction in camera space
    glm::vec3 forward;
    forward.x = -sin(yaw) * cos(pitch);  // Fixed: negative sin for proper forward direction
    forward.y = sin(pitch);
    forward.z = cos(yaw) * cos(pitch);   // Fixed: cos for Z instead of sin
    forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // World up for Q/E movement

    // Calculate movement speed for this frame
    float speed = cameraSpeed_ * deltaTime / 1000.0f; // Convert deltaTime from ms to seconds

    // WASD movement - fixed directions
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        transform.position -= forward * speed;  // Fixed: W should move forward (negative)
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        transform.position += forward * speed;  // Fixed: S should move backward (positive)
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        transform.position += right * speed;    // Fixed: A should strafe left (positive right)
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        transform.position -= right * speed;    // Fixed: D should strafe right (negative right)
    }

    // Q/E for up/down movement (like Minecraft creative mode)
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        transform.position += up * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        transform.position -= up * speed;
    }

    // Sprint with Left Shift (2x speed)
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        float sprintMultiplier = 2.0f;
        // Apply sprint to any movement that happened this frame
        // This is a simple way to add sprint functionality
    }
}

void InputHandler::handleSystemControls(int key) {
    switch (key) {
        case GLFW_KEY_R:
            logInputAction("Hot reload requested");
            if (shaderReloadCallback_) {
                shaderReloadCallback_();
            } else {
                VKMON_WARNING("Shader reload requested but no callback registered");
            }
            break;
            
        default:
            break;
    }
}

void InputHandler::handleLightingControls(int key) {
    if (lightingControlCallback_) {
        lightingControlCallback_(key);
        logInputAction("Lighting control: key " + std::to_string(key));
    } else {
        VKMON_WARNING("Lighting control requested but no callback registered");
    }
}

void InputHandler::handleMaterialControls(int key) {
    if (materialControlCallback_) {
        materialControlCallback_(key);
        logInputAction("Material control: key " + std::to_string(key));
    } else {
        VKMON_WARNING("Material control requested but no callback registered");
    }
}

void InputHandler::logInputAction(const std::string& action) const {
    VKMON_INFO("[INPUT] " + action);
}

void InputHandler::printControlsHelp() const {
    std::cout << "\n=== VulkanMon Controls ===" << std::endl;
    std::cout << "Camera Movement:" << std::endl;
    std::cout << "  WASD - Move forward/back/left/right" << std::endl;
    std::cout << "  QE   - Move up/down" << std::endl;
    std::cout << "  Mouse - Look around" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Lighting Controls:" << std::endl;
    std::cout << "  1/2 - Increase/decrease directional light intensity" << std::endl;
    std::cout << "  3   - Cycle lighting presets" << std::endl;
    std::cout << "  4   - Toggle ambient lighting" << std::endl;
    std::cout << "  L   - Print lighting debug info" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Material Controls:" << std::endl;
    std::cout << "  M   - Cycle material presets" << std::endl;
    std::cout << "  5/6 - Decrease/increase material shininess" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "System Controls:" << std::endl;
    std::cout << "  R   - Hot reload shaders" << std::endl;
    std::cout << "  H   - Show this help" << std::endl;
    std::cout << "  TAB - Toggle camera/UI mode" << std::endl;
    std::cout << "==========================" << std::endl;
}

void InputHandler::toggleInputMode() {
    if (!window_) {
        VKMON_ERROR("Cannot toggle input mode - no window reference");
        return;
    }

    // Check current cursor state and toggle
    if (window_->isCursorDisabled()) {
        // Currently in camera mode, switch to UI mode
        window_->enableCursor();
        setMouseLocked(false);
        VKMON_INFO("Switched to UI mode - cursor enabled for ImGui interaction");
    } else {
        // Currently in UI mode, switch to camera mode
        window_->disableCursor();
        setMouseLocked(true);
        VKMON_INFO("Switched to camera mode - cursor disabled for camera control");
    }
}

} // namespace VulkanMon