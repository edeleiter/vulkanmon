#include "InputHandler.h"
#include "../utils/Logger.h"
#include "../systems/LightingSystem.h"
#include "../components/Transform.h"
#include "../components/CharacterControllerComponent.h"
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
    if (cameraSystem_->hasActiveCamera(world_->getEntityManager())) {
        EntityID activeCameraEntity = cameraSystem_->getActiveCameraEntity();
        if (world_->hasComponent<Transform>(activeCameraEntity)) {
            Transform& transform = world_->getComponent<Transform>(activeCameraEntity);

            // GIMBAL LOCK FIX: Update yaw and pitch directly without quaternion conversion
            // Yaw (Y rotation) - left/right mouse movement (negative for proper direction)
            cameraYaw_ -= xoffset;

            // Pitch (X rotation) - up/down mouse movement
            cameraPitch_ += yoffset;

            // Clamp pitch to prevent gimbal lock (±89 degrees)
            cameraPitch_ = glm::clamp(cameraPitch_, -89.0f, 89.0f);

            // Normalize yaw to 0-360 range
            if (cameraYaw_ > 360.0f) cameraYaw_ -= 360.0f;
            if (cameraYaw_ < 0.0f) cameraYaw_ += 360.0f;

            // Build rotation directly from yaw and pitch (avoids gimbal lock)
            transform.setRotationEuler(cameraPitch_, cameraYaw_, 0.0f);
        }
    }
}

void InputHandler::processMouseButtonInput(int button, int action, int mods) {
    // Only process button press events (not release or repeat)
    if (action != GLFW_PRESS) {
        return;
    }

    // Handle left mouse button for projectile spawning
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (projectileSpawnCallback_) {
            // Pass current mouse position to callback
            projectileSpawnCallback_(lastMouseX_, lastMouseY_);
            logInputAction("Projectile spawn requested (Left Click)");
        }
    }

    // Handle right mouse button for alternative projectile types (future use)
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (projectileSpawnCallback_) {
            // Could pass different parameters for different projectile types
            projectileSpawnCallback_(lastMouseX_, lastMouseY_);
            logInputAction("Alternative projectile spawn requested (Right Click)");
        }
    }
}

void InputHandler::processContinuousInput(GLFWwindow* window, float deltaTime) {
    if (!window || !cameraSystem_ || !world_) {
        return;
    }

    // Handle player movement if player entity is set
    if (playerEntity_ != 0) {
        handlePlayerMovement(window, deltaTime);
    } else {
        // Fallback to camera movement for development/testing
        handleCameraMovement(window, deltaTime);
    }
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

void InputHandler::setProjectileSpawnCallback(ProjectileSpawnCallback callback) {
    projectileSpawnCallback_ = callback;
    VKMON_DEBUG("Projectile spawn callback registered");
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
    if (!cameraSystem_->hasActiveCamera(world_->getEntityManager())) {
        return;
    }

    EntityID activeCameraEntity = cameraSystem_->getActiveCameraEntity();
    if (!world_->hasComponent<Transform>(activeCameraEntity)) {
        return;
    }

    Transform& transform = world_->getComponent<Transform>(activeCameraEntity);

    // Use Transform's direction vectors (calculated from quaternion rotation)
    glm::vec3 forward = transform.getForward();
    glm::vec3 right = transform.getRight();
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // World up for Q/E movement

    // Calculate movement speed for this frame
    float speed = cameraSpeed_ * deltaTime / 1000.0f; // Convert deltaTime from ms to seconds

    // WASD movement - correct directions
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        transform.position += forward * speed;  // W moves forward
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        transform.position -= forward * speed;  // S moves backward
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        transform.position -= right * speed;    // A strafes left
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        transform.position += right * speed;    // D strafes right
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

void InputHandler::handlePlayerMovement(GLFWwindow* window, float deltaTime) {
    // Check if player entity exists and has CharacterControllerComponent
    if (!world_->hasComponent<CharacterControllerComponent>(playerEntity_)) {
        return;
    }

    auto& controller = world_->getComponent<CharacterControllerComponent>(playerEntity_);

    // Calculate input direction in world space
    glm::vec3 inputDirection(0.0f);

    // WASD input
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        inputDirection.z -= 1.0f; // Forward (negative Z)
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        inputDirection.z += 1.0f; // Backward (positive Z)
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        inputDirection.x -= 1.0f; // Left (negative X)
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        inputDirection.x += 1.0f; // Right (positive X)
    }

    // Normalize input direction if not zero
    if (glm::length(inputDirection) > 0.0f) {
        inputDirection = glm::normalize(inputDirection);
        static int logCount = 0;
        if (logCount++ < 3) {
            VKMON_INFO("Player input detected: (" +
                      std::to_string(inputDirection.x) + "," +
                      std::to_string(inputDirection.y) + "," +
                      std::to_string(inputDirection.z) + ")");
        }
    }

    // Set input direction on controller
    controller.inputDirection = inputDirection;

    // Handle sprint (Left Shift)
    controller.wantsToSprint = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

    // Handle jump (Space)
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        controller.wantsToJump = true;
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