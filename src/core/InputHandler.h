#pragma once

#include "Window.h"
#include "../systems/LightingSystem.h"
#include "../systems/MaterialSystem.h"
#include "../systems/CameraSystem.h"
#include "World.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <functional>

/**
 * VulkanMon Input Processing and System Control
 * 
 * Handles all user input including camera movement, system controls, and hot reloading.
 * Follows "Simple is Powerful" - focused solely on input processing and command routing.
 * 
 * Responsibilities:
 * - Camera movement (WASD + mouse look)
 * - Material system controls (M, 5, 6 keys)
 * - Lighting system controls (1, 2, 3, 4, L keys)
 * - Hot reloading and system commands (R key)
 * - Input state management (mouse position, sensitivity)
 * 
 * Design Philosophy:
 * - Single responsibility: Input processing only
 * - Clean callbacks: Modern std::function interfaces
 * - System coordination: Routes commands to appropriate systems
 * - Testable: Can be mocked with fake input events
 */

namespace VulkanMon {

class InputHandler {
public:
    // Callback types for actions InputHandler cannot perform directly
    using ShaderReloadCallback = std::function<void()>;
    using LightingControlCallback = std::function<void(int key)>;
    using MaterialControlCallback = std::function<void(int key)>;
    using InspectorToggleCallback = std::function<void()>;
    using ProjectileSpawnCallback = std::function<void(double mouseX, double mouseY)>;

    /**
     * Create InputHandler with ECS camera system and window references
     * Modern ECS-based architecture for WASD + mouse camera controls
     *
     * @param window Window system for cursor mode control
     * @param cameraSystem ECS camera system for movement controls (raw pointer, owned by World)
     * @param world ECS world for accessing camera entities
     */
    InputHandler(
        std::shared_ptr<Window> window,
        CameraSystem* cameraSystem,
        World* world
    );
    
    /**
     * Destructor - no special cleanup needed
     */
    ~InputHandler() = default;
    
    // Move-only semantics
    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;
    InputHandler(InputHandler&&) = default;
    InputHandler& operator=(InputHandler&&) = default;
    
    /**
     * Process discrete key input events
     * Called when a key is pressed, released, or repeated
     * 
     * @param key GLFW key code
     * @param scancode Platform-specific scan code
     * @param action GLFW_PRESS, GLFW_RELEASE, or GLFW_REPEAT
     * @param mods Modifier keys (shift, ctrl, alt, etc.)
     */
    void processKeyInput(int key, int scancode, int action, int mods);
    
    /**
     * Process mouse movement input
     * Called when mouse position changes
     *
     * @param xpos Mouse X position in screen coordinates
     * @param ypos Mouse Y position in screen coordinates
     */
    void processMouseInput(double xpos, double ypos);

    /**
     * Process mouse button input
     * Called when mouse buttons are pressed/released
     *
     * @param button Mouse button (GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, etc.)
     * @param action GLFW_PRESS, GLFW_RELEASE, or GLFW_REPEAT
     * @param mods Modifier keys (shift, ctrl, alt, etc.)
     */
    void processMouseButtonInput(int button, int action, int mods);
    
    /**
     * Process continuous input (held keys)
     * Called every frame to handle WASD movement
     * 
     * @param window GLFW window for polling key states
     * @param deltaTime Time since last frame in seconds
     */
    void processContinuousInput(GLFWwindow* window, float deltaTime);
    
    /**
     * Register shader reload callback
     * InputHandler cannot reload shaders directly (belongs to renderer)
     * 
     * @param callback Function to call when shader reload is requested
     */
    void setShaderReloadCallback(ShaderReloadCallback callback);
    
    /**
     * Register lighting control callback
     * 
     * @param callback Function to call with lighting control key
     */
    void setLightingControlCallback(LightingControlCallback callback);
    
    /**
     * Register material control callback
     *
     * @param callback Function to call with material control key
     */
    void setMaterialControlCallback(MaterialControlCallback callback);

    /**
     * Register inspector toggle callback
     *
     * @param callback Function to call when inspector toggle is requested (I key)
     */
    void setInspectorToggleCallback(InspectorToggleCallback callback);

    /**
     * Register projectile spawn callback
     *
     * @param callback Function to call when projectile spawn is requested (mouse click)
     */
    void setProjectileSpawnCallback(ProjectileSpawnCallback callback);
    
    /**
     * Reset mouse position tracking
     * Call when mouse capture is enabled/disabled
     */
    void resetMousePosition();
    
    /**
     * Configure mouse sensitivity
     * 
     * @param sensitivity Mouse sensitivity multiplier (default: 0.1f)
     */
    void setMouseSensitivity(float sensitivity);
    
    /**
     * Configure camera movement speed
     * 
     * @param speed Camera movement speed multiplier (default: 2.5f)
     */
    void setCameraSpeed(float speed);
    
    /**
     * Check if mouse is locked to window center
     * 
     * @return true if mouse is locked for camera look
     */
    bool isMouseLocked() const { return mouseLocked_; }
    
    /**
     * Enable/disable mouse lock for camera controls
     *
     * @param locked true to lock mouse for camera look
     */
    void setMouseLocked(bool locked) { mouseLocked_ = locked; }

    /**
     * Toggle between camera mode and UI interaction mode
     * Camera mode: cursor disabled, mouse controls camera
     * UI mode: cursor enabled, can interact with ImGui
     */
    void toggleInputMode();

private:
    // System references (not owned)
    std::shared_ptr<Window> window_;
    CameraSystem* cameraSystem_;
    World* world_;
    
    // Mouse input state
    bool firstMouse_ = true;
    float lastMouseX_ = 400.0f;  // Default to window center
    float lastMouseY_ = 300.0f;  // Default to window center
    float mouseSensitivity_ = 0.005f;  // Pokemon-style action game sensitivity (reduced from 0.01f)
    bool mouseLocked_ = true;
    
    // Camera movement configuration
    float cameraSpeed_ = 2.5f;

    // GIMBAL LOCK FIX: Track yaw and pitch directly to avoid quaternion conversion issues
    float cameraYaw_ = 0.0f;   // Y-axis rotation (left/right)
    float cameraPitch_ = -15.0f; // X-axis rotation (up/down) - matches initial camera setup

    // Callbacks for actions we cannot perform directly
    ShaderReloadCallback shaderReloadCallback_;
    LightingControlCallback lightingControlCallback_;
    MaterialControlCallback materialControlCallback_;
    InspectorToggleCallback inspectorToggleCallback_;
    ProjectileSpawnCallback projectileSpawnCallback_;
    
    // Input processing helpers
    void handleCameraMovement(GLFWwindow* window, float deltaTime);
    void handleSystemControls(int key);
    void handleLightingControls(int key);
    void handleMaterialControls(int key);
    
    // Debug and feedback
    void logInputAction(const std::string& action) const;
    void printControlsHelp() const;
};

} // namespace VulkanMon