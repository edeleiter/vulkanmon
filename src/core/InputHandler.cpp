#include "InputHandler.h"
#include "../utils/Logger.h"
#include "../systems/LightingSystem.h"
#include <iostream>

namespace VulkanMon {

InputHandler::InputHandler(std::shared_ptr<Camera> camera, std::shared_ptr<Window> window)
    : camera_(camera), window_(window) {

    VKMON_DEBUG("InputHandler initialized");
    VKMON_DEBUG(camera_ ? "  Camera: connected" : "  Camera: null");
    VKMON_DEBUG(window_ ? "  Window: connected" : "  Window: null");
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
    if (!camera_ || !mouseLocked_) {
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
    
    // For now, we'll just store the mouse movement
    // The current Camera class doesn't support mouse look, so we'll leave this for future enhancement
    // camera_->processMouseMovement(xoffset, yoffset);
}

void InputHandler::processContinuousInput(GLFWwindow* window, float deltaTime) {
    if (!window || !camera_) {
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
    // Use the existing Camera::processInput method
    // This maintains compatibility with the current camera implementation
    if (camera_) {
        camera_->processInput(window);
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