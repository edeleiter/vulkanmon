#pragma once

#include "Window.h"
#include "InputHandler.h"
#include "Camera.h"
#include "../utils/Logger.h"
#include "../rendering/ResourceManager.h"
#include "../io/AssetManager.h"
#include "../io/ModelLoader.h"
#include "../systems/LightingSystem.h"
#include "../systems/MaterialSystem.h"
#include "../rendering/VulkanRenderer.h"
#include "World.h"
#include "../systems/RenderSystem.h"
#include "../systems/CameraSystem.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"

#include <memory>
#include <chrono>
#include <functional>

/**
 * VulkanMon Application
 * 
 * Main application class that orchestrates all engine systems and manages the application lifecycle.
 * Extracted from main.cpp to create clean separation of concerns and testable architecture.
 * 
 * Responsibilities:
 * - Application initialization and shutdown
 * - System coordination and dependency injection
 * - Main game loop execution and timing
 * - High-level input event handling
 * - Error handling and recovery
 * - Configuration management
 * 
 * Design Philosophy:
 * - Single responsibility: Application orchestration only
 * - Dependency injection: All systems passed as shared_ptr
 * - RAII compliance: Automatic resource cleanup
 * - Testable: Systems can be mocked for testing
 * - Clean interfaces: Modern C++20 patterns
 */

namespace VulkanMon {

// Forward declarations
class Model;

class Application {
public:
    // Configuration constants
    static constexpr int DEFAULT_WINDOW_WIDTH = 800;
    static constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    static constexpr float DEFAULT_CAMERA_SPEED = 2.5f;
    static constexpr float DEFAULT_CAMERA_FOV = 45.0f;
    static constexpr float DEFAULT_NEAR_PLANE = 0.1f;
    static constexpr float DEFAULT_FAR_PLANE = 10.0f;
    
    /**
     * Create Application with default configuration
     */
    Application();
    
    /**
     * Destructor - automatic cleanup of all systems
     */
    ~Application();
    
    // Move-only semantics (RAII compliance)
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = default;
    Application& operator=(Application&&) = default;
    
    /**
     * Initialize all engine systems
     * Must be called before run()
     * 
     * @throws std::runtime_error if initialization fails
     */
    void initialize();
    
    /**
     * Run the main application loop
     * Blocks until application should exit
     * 
     * @throws std::runtime_error if critical error occurs
     */
    void run();
    
    /**
     * Request application shutdown
     * Will cause run() to return cleanly
     */
    void shutdown();
    
    /**
     * Check if application is running
     * 
     * @return true if main loop is active
     */
    bool isRunning() const { return running_; }
    
    /**
     * Get current application statistics
     * 
     * @return Frame time in milliseconds
     */
    float getFrameTime() const { return frameTime_; }
    
    /**
     * Get frames per second
     * 
     * @return Current FPS
     */
    float getFPS() const { return fps_; }

private:
    // Application state
    bool initialized_ = false;
    bool running_ = false;
    
    // Frame timing
    std::chrono::high_resolution_clock::time_point lastFrameTime_;
    float frameTime_ = 0.0f;
    float fps_ = 0.0f;
    
    // Core engine systems (owned)
    std::shared_ptr<Window> window_;
    std::shared_ptr<::Camera> camera_;
    std::shared_ptr<InputHandler> inputHandler_;
    std::shared_ptr<ResourceManager> resourceManager_;
    std::shared_ptr<AssetManager> assetManager_;
    std::shared_ptr<ModelLoader> modelLoader_;
    std::shared_ptr<LightingSystem> lightingSystem_;
    std::shared_ptr<MaterialSystem> materialSystem_;
    std::shared_ptr<VulkanRenderer> renderer_;

    // ECS World and systems
    std::unique_ptr<World> world_;
    RenderSystem* renderSystem_ = nullptr;  // Owned by World
    CameraSystem* cameraSystem_ = nullptr;  // Owned by World
    
    // Current loaded model
    std::shared_ptr<Model> currentModel_;
    
    // Current material settings
    struct MaterialState {
        int currentPreset = 0;
        float shininess = 32.0f;
    } materialState_;
    
    // Current lighting settings  
    struct LightingState {
        int currentPreset = 0;
        bool ambientEnabled = true;
    } lightingState_;
    
    // System initialization methods
    void initializeLogger();
    void initializeWindow();
    void initializeCamera();
    void initializeCoreEngineSystems();
    void initializeRenderer();
    void initializeInputSystem();
    void initializeECS();           // Initialize ECS World and systems
    void createTestScene();         // Create test entities for ECS
    void loadTestAssets();
    
    // Main loop methods
    void processFrame();
    void updateFrameTiming();
    void processInput(float deltaTime);
    void updateSystems(float deltaTime);
    void updateECS(float deltaTime);        // Update ECS World
    void render(float deltaTime);
    
    // Input handling methods
    void handleShaderReload();
    void handleLightingControl(int key);
    void handleMaterialControl(int key);
    void adjustDirectionalLightIntensity(float delta);
    void cycleLightingPreset();
    void toggleAmbientLighting();
    void printLightingInfo();
    void adjustMaterialShininess(float delta);
    void cycleMaterialPreset();
    
    // System coordination methods
    void setupInputCallbacks();
    void connectSystemCallbacks();
    
    // Error handling
    void handleCriticalError(const std::exception& error);
    void logSystemState() const;
    
    // Cleanup methods
    void cleanup();
};

} // namespace VulkanMon