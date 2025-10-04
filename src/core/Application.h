#pragma once

#include "Window.h"
#include "InputHandler.h"
// Old Camera.h removed - using unified ECS camera system
#include "../utils/Logger.h"
#include "../rendering/ResourceManager.h"
#include "../io/AssetManager.h"
#include "../io/ModelLoader.h"
#include "../systems/LightingSystem.h"
#include "../systems/MaterialSystem.h"
#include "../rendering/VulkanRenderer.h"
#include "World.h"
#include "../systems/RenderSystem.h"
#include "../systems/CreatureRenderSystem.h"
#include "../systems/CameraSystem.h"
#include "../systems/SpatialSystem.h"
#include "../systems/PhysicsSystem.h"
#include "../systems/ProjectileSystem.h"
#include "../systems/CharacterControllerSystem.h"
#include "../game/CreatureDetectionSystem.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"
#include "../debug/ECSInspector.h"
#include "../config/CameraConfig.h"

#include <memory>
#include <chrono>
#include <functional>

/**
 * @brief Main VulkanMon game engine application class
 * @details Orchestrates all engine systems and manages the complete application lifecycle.
 *          Extracted from main.cpp to create clean separation of concerns and testable architecture.
 *
 * Key Responsibilities:
 * - Application initialization and shutdown
 * - System coordination and lifecycle management
 * - Main game loop execution and timing
 * - Input event handling and game controls
 * - Error handling and recovery
 * - Game state management (materials, lighting, demo features)
 *
 * Design Philosophy:
 * - System coordination: Manages application lifecycle and coordinates engine systems
 * - VulkanRenderer integration: Core systems created and managed by VulkanRenderer
 * - RAII compliance: Automatic resource cleanup
 * - Testable: Systems can be mocked for testing
 * - Clean interfaces: Modern C++20 patterns
 *
 * @example Basic Usage
 * @code
 * #include "core/Application.h"
 *
 * int main() {
 *     try {
 *         Application app;
 *         app.initialize();  // REQUIRED: Initialize all systems first
 *         app.run();         // Start main engine loop
 *     } catch (const std::exception& e) {
 *         std::cerr << "Engine error: " << e.what() << std::endl;
 *         return -1;
 *     }
 *     return 0;
 * }
 * @endcode
 *
 * @see World
 * @see PhysicsSystem
 * @see VulkanRenderer
 * @since Version 7.1
 */

namespace VulkanMon {

// Forward declarations
class Model;

class Application {
public:
    // Configuration constants moved to Config::Camera namespace for single source of truth

    /**
     * @brief Create Application with default configuration
     * @details Initializes all engine systems in the correct dependency order
     */
    Application();

    /**
     * @brief Destructor - automatic cleanup of all systems
     * @details RAII-compliant cleanup of Vulkan resources and engine systems
     */
    ~Application();

    // Move-only semantics (RAII compliance)
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = default;
    Application& operator=(Application&&) = default;

    /**
     * @brief Initialize all engine systems
     * @details Must be called before run(). Initializes Window, VulkanRenderer (which creates core systems),
     *          ECS World, and connects all subsystems for stable operation.
     * @throws std::runtime_error if initialization fails
     * @see run()
     */
    void initialize();

    /**
     * @brief Run the main application loop
     * @details Blocks until application should exit. Handles frame timing, input processing,
     *          ECS updates, physics simulation, and rendering in optimized order.
     * @throws std::runtime_error if critical error occurs
     * @note Call initialize() first
     * @see initialize()
     */
    void run();

    /**
     * @brief Update camera data in VulkanRenderer from ECS CameraSystem
     * @details Part of unified camera architecture - bridges ECS camera data to renderer.
     *          Updates view matrix, projection matrix, and camera position for consistent
     *          rendering, spatial culling, and lighting calculations.
     * @note Called every frame to ensure VulkanRenderer uses current ECS camera state
     * @see CameraSystem
     * @see VulkanRenderer
     */
    void updateCameraMatrices();

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

    /**
     * Toggle ECS Inspector visibility
     */
    void toggleInspector();

    /**
     * Handle window resize event
     * Called by Window resize callback
     *
     * @param width New window width
     * @param height New window height
     */
    void handleWindowResize(int width, int height);


private:
    // Application state
    bool initialized_ = false;
    bool running_ = false;
    bool inspectorEnabled_ = false; // Start with inspector hidden for clean startup

    // Frame timing
    std::chrono::high_resolution_clock::time_point lastFrameTime_;
    float frameTime_ = 0.0f;
    float fps_ = 0.0f;

    // Core engine systems (owned)
    std::shared_ptr<Window> window_;
    std::shared_ptr<InputHandler> inputHandler_;
    // NOTE: Camera system now handled entirely through ECS - no dedicated camera member needed
    std::shared_ptr<ResourceManager> resourceManager_;
    std::shared_ptr<AssetManager> assetManager_;
    std::shared_ptr<ModelLoader> modelLoader_;
    std::shared_ptr<LightingSystem> lightingSystem_;
    std::shared_ptr<MaterialSystem> materialSystem_;
    std::shared_ptr<VulkanRenderer> renderer_;

    // ECS World and systems
    std::unique_ptr<World> world_;
    RenderSystem* renderSystem_ = nullptr;        // Owned by World
    CreatureRenderSystem* creatureRenderSystem_ = nullptr;  // Owned by World
    CameraSystem* cameraSystem_ = nullptr;        // Owned by World
    SpatialSystem* spatialSystem_ = nullptr;  // Owned by World
    PhysicsSystem* physicsSystem_ = nullptr;  // Owned by World
    ProjectileSystem* projectileSystem_ = nullptr;  // Owned by World
    CreatureDetectionSystem* creatureDetectionSystem_ = nullptr;  // Owned by World
    CharacterControllerSystem* characterControllerSystem_ = nullptr;  // Owned by World

    // Player entity for playable demo
    EntityID playerEntity_ = 0;

    // Debug tools
    std::unique_ptr<Debug::ECSInspector> ecsInspector_;

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

    // Dynamic falling cube spawner state
    std::vector<EntityID> fallingCubes_;
    static constexpr size_t MAX_FALLING_CUBES = 20; // Limit to prevent memory issues

    // System initialization methods
    void initializeLogger();
    void initializeWindow();
    void initializeCamera();
    void initializeCoreEngineSystems();
    void initializeRenderer();
    void initializeInputSystem();
    void initializeECS();           // Initialize ECS World and systems
    void connectDeferredSystems();  // Connect systems that need renderer resources
    void createProjectileTestScene(); // Create clean projectile test scene
    void preloadSceneAssets();      // Preload all models used by Renderable components

    // Main loop methods
    void processFrame();
    void updateFrameTiming();
    void processInput(float deltaTime);
    void updateSystems(float deltaTime);
    void updateECS(float deltaTime);        // Update ECS World
    void updateImGui(float deltaTime);      // Update ImGui interface
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
    void logSystemState();

    // Physics demonstration methods
    void makeRandomCubeFall();
    void cleanupFallenCubes();

    // Physics testing methods
    void testRaycastSystem();
    void testSphereOverlapQueries();
    void measureSpatialCollisionPerformance();
    void runPhysicsValidationTests();

    // Cleanup methods
    void cleanup();
};

} // namespace VulkanMon