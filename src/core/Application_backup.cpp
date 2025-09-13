#include "Application.h"
#include "../utils/Utils.h"
#include "../utils/Logger.h"
#include <iostream>
#include <stdexcept>

using namespace VulkanMon;

Application::Application() {
    // Application created - ready for initialization
}

Application::~Application() {
    if (running_) {
        shutdown();
    }
    cleanup();
    VKMON_INFO("VulkanMon Application destroyed");
}

void Application::initialize() {
    if (initialized_) {
        VKMON_WARNING("Application already initialized");
        return;
    }
    
    try {
        VKMON_INFO("VulkanMon starting up...");
        
        // Initialize systems in dependency order
        initializeLogger();
        initializeWindow();
        initializeCamera();
        initializeCoreEngineSystems();
        initializeRenderer();
        initializeECS();            // Initialize ECS after renderer is ready
        initializeInputSystem();
        createTestScene();          // Create test entities after ECS is set up
        loadTestAssets();
        
        initialized_ = true;
        VKMON_INFO("VulkanMon ready!");
        
    } catch (const std::exception& e) {
        VKMON_ERROR("Application initialization failed: " + std::string(e.what()));
        cleanup();
        throw;
    }
}

void Application::run() {
    if (!initialized_) {
        throw std::runtime_error("Application not initialized. Call initialize() first.");
    }
    
    running_ = true;
    lastFrameTime_ = std::chrono::high_resolution_clock::now();

    try {
        while (running_ && !window_->shouldClose()) {
            processFrame();
        }
    } catch (const std::exception& e) {
        handleCriticalError(e);
        throw;
    }

    running_ = false;
    
    // Wait for device to be idle before cleanup
    if (renderer_ && renderer_->isInitialized()) {
        vkDeviceWaitIdle(renderer_->getDevice());
    }
}

void Application::shutdown() {
    if (!running_) {
        return;
    }
    
    VKMON_INFO("Application shutdown requested");
    running_ = false;
}

void Application::initializeLogger() {
    Logger::getInstance().enableConsoleOutput(true);
    Logger::getInstance().setLogLevel(LogLevel::INFO_LEVEL);
}

void Application::initializeWindow() {
    window_ = std::make_unique<Window>(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "VulkanMon");
    window_->initialize();
    // Window logs its own initialization success
}

void Application::initializeCamera() {
    camera_ = std::make_shared<::Camera>();
    VKMON_INFO("Camera system initialized");
}

void Application::initializeCoreEngineSystems() {
    // Core systems will be initialized after renderer is ready
}

void Application::initializeRenderer() {
    // Create placeholder systems that renderer will initialize
    resourceManager_ = nullptr;  // Will be created by renderer
    assetManager_ = nullptr;     // Will be created after resourceManager
    modelLoader_ = nullptr;      // Will be created after assetManager
    lightingSystem_ = nullptr;   // Will be created after resourceManager
    materialSystem_ = nullptr;   // Will be created after resourceManager
    
    // Create renderer - it will log its own initialization progress
    renderer_ = std::make_shared<VulkanRenderer>(
        window_,
        camera_,
        resourceManager_,
        assetManager_,
        modelLoader_,
        lightingSystem_,
        materialSystem_
    );
    
    renderer_->initialize();
    // Renderer logs its own success - no duplicate needed
}

void Application::initializeInputSystem() {
    inputHandler_ = std::make_unique<InputHandler>(camera_);
    setupInputCallbacks();
    connectSystemCallbacks();
    VKMON_INFO("Input system initialized");
}

void Application::loadTestAssets() {
    VKMON_INFO("Loading test assets...");
    
    // Load test model - this will be handled by the renderer for now
    // Later we'll move this to a proper asset loading system
    
    VKMON_INFO("Test assets loaded");
}

void Application::setupInputCallbacks() {
    if (!inputHandler_) {
        VKMON_WARNING("Cannot setup input callbacks: InputHandler not initialized");
        return;
    }
    
    // Register InputHandler callbacks with Window
    window_->setKeyCallback([this](int key, int scancode, int action, int mods) {
        inputHandler_->processKeyInput(key, scancode, action, mods);
    });
    
    window_->setMouseCallback([this](double xpos, double ypos) {
        inputHandler_->processMouseInput(xpos, ypos);
    });
    
    VKMON_INFO("Input callbacks registered with Window system");
}

void Application::connectSystemCallbacks() {
    if (!inputHandler_) {
        VKMON_WARNING("Cannot connect system callbacks: InputHandler not initialized");
        return;
    }
    
    // Register system control callbacks
    inputHandler_->setShaderReloadCallback([this]() {
        handleShaderReload();
    });
    
    inputHandler_->setLightingControlCallback([this](int key) {
        handleLightingControl(key);
    });
    
    inputHandler_->setMaterialControlCallback([this](int key) {
        handleMaterialControl(key);
    });
    
    VKMON_INFO("System callbacks connected");
}

void Application::processFrame() {
    updateFrameTiming();
    
    // Poll window events
    window_->pollEvents();
    
    // Process input
    processInput(frameTime_);
    
    // Update systems
    updateSystems(frameTime_);

    // Update ECS
    updateECS(frameTime_);

    // Render frame
    render(frameTime_);
}

void Application::updateFrameTiming() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    frameTime_ = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;
    
    // Calculate FPS (smoothed)
    static float fpsSum = 0.0f;
    static int fpsCount = 0;
    static const int FPS_SMOOTH_FRAMES = 60;
    
    if (frameTime_ > 0.0f) {
        fpsSum += 1.0f / frameTime_;
        fpsCount++;
        
        if (fpsCount >= FPS_SMOOTH_FRAMES) {
            fps_ = fpsSum / fpsCount;
            fpsSum = 0.0f;
            fpsCount = 0;
        }
    }
}

void Application::processInput(float deltaTime) {
    if (inputHandler_) {
        inputHandler_->processContinuousInput(window_->getWindow(), deltaTime);
    }
}

void Application::updateSystems(float deltaTime) {
    // Update systems that need per-frame updates
    // Camera updates are handled by InputHandler
    // Material and lighting updates are event-driven
    
    // Future: Update game logic, animations, physics, etc.
}

void Application::render(float deltaTime) {
    if (renderer_ && renderer_->isInitialized()) {
        // VulkanRenderer will call back to ECS during renderFrame()
        renderer_->renderFrame(deltaTime);
    }
}

void Application::handleShaderReload() {
    VKMON_INFO("Reloading shaders...");
    
    try {
        if (renderer_) {
            renderer_->reloadShaders();
            VKMON_INFO("Hot reload complete - new shaders active!");
        }
    } catch (const std::exception& e) {
        VKMON_ERROR("Hot reload failed: " + std::string(e.what()));
        VKMON_WARNING("Application continuing with previous shaders");
    }
}

void Application::handleLightingControl(int key) {
    switch (key) {
        case GLFW_KEY_1:
            adjustDirectionalLightIntensity(0.1f);
            break;
        case GLFW_KEY_2:
            adjustDirectionalLightIntensity(-0.1f);
            break;
        case GLFW_KEY_3:
            cycleLightingPreset();
            break;
        case GLFW_KEY_4:
            toggleAmbientLighting();
            break;
        case GLFW_KEY_L:
            printLightingInfo();
            break;
    }
}

void Application::handleMaterialControl(int key) {
    switch (key) {
        case GLFW_KEY_M:
            cycleMaterialPreset();
            break;
        case GLFW_KEY_5:
            adjustMaterialShininess(-10.0f);
            break;
        case GLFW_KEY_6:
            adjustMaterialShininess(10.0f);
            break;
    }
}

void Application::adjustDirectionalLightIntensity(float delta) {
    // Placeholder for future lighting system integration
    VKMON_INFO("[LIGHTING] Directional light intensity adjustment: " + std::to_string(delta));
}

void Application::cycleLightingPreset() {
    lightingState_.currentPreset = (lightingState_.currentPreset + 1) % 4;
    VKMON_INFO("[LIGHTING] Cycling to preset " + std::to_string(lightingState_.currentPreset));
}

void Application::toggleAmbientLighting() {
    lightingState_.ambientEnabled = !lightingState_.ambientEnabled;
    VKMON_INFO("[LIGHTING] Ambient lighting: " + std::string(lightingState_.ambientEnabled ? "ON" : "OFF"));
}

void Application::printLightingInfo() {
    VKMON_INFO("\n[LIGHTING DEBUG INFO]");
    VKMON_INFO("Current preset: " + std::to_string(lightingState_.currentPreset));
    VKMON_INFO("Ambient enabled: " + std::string(lightingState_.ambientEnabled ? "true" : "false"));
    VKMON_INFO("\nControls:");
    VKMON_INFO("  1/2: Adjust directional light intensity");
    VKMON_INFO("  3: Cycle lighting presets");
    VKMON_INFO("  4: Toggle ambient lighting");
    VKMON_INFO("  5/6: Adjust material shininess");
    VKMON_INFO("  M: Cycle material presets");
    VKMON_INFO("  L: Show this debug info");
    VKMON_INFO("  R: Reload shaders");
}

void Application::adjustMaterialShininess(float delta) {
    float oldShininess = materialState_.shininess;
    materialState_.shininess = std::max(1.0f, std::min(256.0f, materialState_.shininess + delta));
    VKMON_INFO("[MATERIAL SHININESS] " + std::to_string(oldShininess) + " -> " + 
               std::to_string(materialState_.shininess) + " (delta: " + std::to_string(delta) + ")");
}

void Application::cycleMaterialPreset() {
    // Call VulkanRenderer to actually cycle the materials
    if (renderer_) {
        renderer_->cycleMaterialPreset();
    }
    
    // Keep local state in sync for any future needs
    materialState_.currentPreset = (materialState_.currentPreset + 1) % 5;
}

void Application::initializeECS() {
    VKMON_DEBUG("Initializing ECS World and systems...");

    // Create ECS World
    world_ = std::make_unique<World>();
    world_->initialize();

    // Add camera system to handle ECS camera entities
    cameraSystem_ = world_->addSystem<CameraSystem>();

    // Add render system to handle rendering ECS entities
    renderSystem_ = world_->addSystem<RenderSystem>(cameraSystem_);

    // Register ECS render callback with VulkanRenderer
    if (renderer_) {
        renderer_->setECSRenderCallback([this](VulkanRenderer& renderer) {
            if (world_) {
                world_->render(renderer);
            }
        });
        VKMON_DEBUG("ECS render callback registered with VulkanRenderer");
    } else {
        VKMON_WARNING("Cannot register ECS callback: VulkanRenderer not available");
    }

    VKMON_INFO("ECS systems initialized successfully");
}

void Application::createTestScene() {
    if (!world_) {
        VKMON_WARNING("Cannot create test scene: ECS World not initialized");
        return;
    }

    VKMON_DEBUG("Creating ECS multi-object test scene...");

    // Create multiple test objects to demonstrate ECS rendering

    // Object 1: Center cube (original position)
    EntityID cube1 = world_->createEntity();
    Transform transform1;
    transform1.position = glm::vec3(0.0f, 0.0f, 0.0f);
    transform1.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    transform1.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    world_->addComponent(cube1, transform1);

    Renderable renderable1;
    renderable1.meshPath = "cube";
    renderable1.texturePath = "default";
    renderable1.materialId = 0;  // Default material
    renderable1.isVisible = true;
    renderable1.renderLayer = 0;
    world_->addComponent(cube1, renderable1);

    // Object 2: Left cube with different material
    EntityID cube2 = world_->createEntity();
    Transform transform2;
    transform2.position = glm::vec3(-3.0f, 0.0f, 0.0f);
    transform2.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    transform2.scale = glm::vec3(0.8f, 0.8f, 0.8f);
    world_->addComponent(cube2, transform2);

    Renderable renderable2;
    renderable2.meshPath = "cube";
    renderable2.texturePath = "default";
    renderable2.materialId = 1;  // Gold material
    renderable2.isVisible = true;
    renderable2.renderLayer = 0;
    world_->addComponent(cube2, renderable2);

    // Object 3: Right cube with different material
    EntityID cube3 = world_->createEntity();
    Transform transform3;
    transform3.position = glm::vec3(3.0f, 0.0f, 0.0f);
    transform3.rotation = glm::vec3(0.0f, -45.0f, 0.0f);
    transform3.scale = glm::vec3(1.2f, 1.2f, 1.2f);
    world_->addComponent(cube3, transform3);

    Renderable renderable3;
    renderable3.meshPath = "cube";
    renderable3.texturePath = "default";
    renderable3.materialId = 2;  // Ruby material
    renderable3.isVisible = true;
    renderable3.renderLayer = 0;
    world_->addComponent(cube3, renderable3);

    // Object 4: Above center cube
    EntityID cube4 = world_->createEntity();
    Transform transform4;
    transform4.position = glm::vec3(0.0f, 2.5f, 0.0f);
    transform4.rotation = glm::vec3(45.0f, 45.0f, 45.0f);
    transform4.scale = glm::vec3(0.6f, 0.6f, 0.6f);
    world_->addComponent(cube4, transform4);

    Renderable renderable4;
    renderable4.meshPath = "cube";
    renderable4.texturePath = "default";
    renderable4.materialId = 3;  // Chrome material
    renderable4.isVisible = true;
    renderable4.renderLayer = 0;
    world_->addComponent(cube4, renderable4);

    // Object 5: Back cube
    EntityID cube5 = world_->createEntity();
    Transform transform5;
    transform5.position = glm::vec3(0.0f, 0.0f, -4.0f);
    transform5.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    transform5.scale = glm::vec3(1.5f, 0.5f, 1.5f);
    world_->addComponent(cube5, transform5);

    Renderable renderable5;
    renderable5.meshPath = "cube";
    renderable5.texturePath = "default";
    renderable5.materialId = 4;  // Emerald material
    renderable5.isVisible = true;
    renderable5.renderLayer = 0;
    world_->addComponent(cube5, renderable5);

    VKMON_DEBUG("Created 5 ECS test entities:");
    VKMON_DEBUG("  Entity " + std::to_string(cube1) + ": Center cube (Default material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube2) + ": Left cube (Gold material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube3) + ": Right cube (Ruby material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube4) + ": Top cube (Chrome material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube5) + ": Back cube (Emerald material)");

    // Future: Create camera entity to replace legacy Camera class
    // EntityID cameraEntity = world_->createEntity();
    // ... add Camera component

    VKMON_INFO("Test scene ready - 5 objects loaded");
}

void Application::updateECS(float deltaTime) {
    if (world_) {
        world_->update(deltaTime);
    }
}

void Application::handleCriticalError(const std::exception& error) {
    VKMON_ERROR("Critical application error: " + std::string(error.what()));
    logSystemState();
    
    // Attempt graceful shutdown
    running_ = false;
}

void Application::logSystemState() const {
    VKMON_INFO("=== Application System State ===");
    VKMON_INFO("Initialized: " + std::string(initialized_ ? "true" : "false"));
    VKMON_INFO("Running: " + std::string(running_ ? "true" : "false"));
    VKMON_INFO("Frame time: " + std::to_string(frameTime_) + "ms");
    VKMON_INFO("FPS: " + std::to_string(fps_));
    
    if (window_) {
        VKMON_INFO("Window: initialized");
    } else {
        VKMON_INFO("Window: null");
    }
    
    if (renderer_ && renderer_->isInitialized()) {
        VKMON_INFO("Renderer: initialized");
        VKMON_INFO("Renderer frame time: " + std::to_string(renderer_->getFrameTime()) + "ms");
    } else {
        VKMON_INFO("Renderer: " + std::string(renderer_ ? "not initialized" : "null"));
    }
    
    VKMON_INFO("=== End System State ===");
}

void Application::cleanup() {
    VKMON_INFO("Beginning Application cleanup...");

    // Cleanup in reverse order of initialization
    if (world_) {
        VKMON_DEBUG("Shutting down ECS World...");
        world_->shutdown();
        world_.reset();
        renderSystem_ = nullptr;  // Owned by World, already cleaned up
        cameraSystem_ = nullptr;  // Owned by World, already cleaned up
    }

    if (currentModel_) {
        VKMON_INFO("Cleaning up loaded model...");
        currentModel_.reset();
    }
    
    if (renderer_) {
        VKMON_INFO("Cleaning up renderer...");
        renderer_.reset();
    }
    
    if (materialSystem_) {
        materialSystem_.reset();
    }
    
    if (lightingSystem_) {
        lightingSystem_.reset();
    }
    
    if (modelLoader_) {
        if (modelLoader_) {
            modelLoader_->printLoadingSummary();
        }
        modelLoader_.reset();
    }
    
    if (assetManager_) {
        if (assetManager_) {
            assetManager_->printAssetSummary();
        }
        assetManager_.reset();
    }
    
    if (resourceManager_) {
        if (resourceManager_) {
            resourceManager_->printResourceSummary();
        }
        resourceManager_.reset();
    }
    
    if (inputHandler_) {
        inputHandler_.reset();
    }
    
    if (camera_) {
        camera_.reset();
    }
    
    if (window_) {
        window_.reset();
    }
    
    VKMON_INFO("Application cleanup completed");
}