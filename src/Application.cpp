#include "Application.h"
#include "Utils.h"
#include <iostream>
#include <stdexcept>

using namespace VulkanMon;

Application::Application() {
    VKMON_INFO("VulkanMon Application created");
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
        VKMON_INFO("VulkanMon - Initializing Application Systems");
        
        // Initialize systems in dependency order
        initializeLogger();
        initializeWindow();
        initializeCamera();
        initializeCoreEngineSystems();
        initializeRenderer();
        initializeInputSystem();
        loadTestAssets();
        
        initialized_ = true;
        VKMON_INFO("Application initialization completed successfully");
        
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
    
    VKMON_INFO("Starting main application loop");
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
    VKMON_INFO("Main application loop ended");
    
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
    VKMON_INFO("Window system initialized");
}

void Application::initializeCamera() {
    camera_ = std::make_shared<Camera>();
    VKMON_INFO("Camera system initialized");
}

void Application::initializeCoreEngineSystems() {
    VKMON_INFO("Initializing core engine systems...");
    
    // These systems need Vulkan device, so we'll initialize them after renderer
    // For now, just log that we're preparing for them
    VKMON_INFO("Core systems ready for initialization");
}

void Application::initializeRenderer() {
    VKMON_INFO("Initializing Vulkan renderer...");
    
    // Initialize ResourceManager first (needs device from renderer)
    // This is a chicken-and-egg problem we'll solve by creating a VulkanContext class
    // For now, we'll handle this in the renderer initialization
    
    // Create placeholder systems that renderer will initialize
    resourceManager_ = nullptr;  // Will be created by renderer
    assetManager_ = nullptr;     // Will be created after resourceManager
    modelLoader_ = nullptr;      // Will be created after assetManager
    lightingSystem_ = nullptr;   // Will be created after resourceManager
    materialSystem_ = nullptr;   // Will be created after resourceManager
    
    // Create renderer with Window and Camera (the only dependencies we have ready)
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
    VKMON_INFO("Renderer initialized successfully");
    
    // Now get the core systems from renderer (this is temporary until we create VulkanContext)
    // We'll need to refactor this when we extract VulkanContext
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
        renderer_->renderFrame(deltaTime);
    }
}

void Application::handleShaderReload() {
    VKMON_INFO("[HOT RELOAD] R key pressed - reloading shaders...");
    
    try {
        if (renderer_) {
            renderer_->reloadShaders();
            VKMON_INFO("[SUCCESS] Hot reload complete - new shaders active!");
        }
    } catch (const std::exception& e) {
        VKMON_ERROR("[ERROR] Hot reload failed: " + std::string(e.what()));
        VKMON_WARNING("[WARN] Application continuing with previous shaders");
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
    // TODO: This will be implemented once we have proper system coordination
    // For now, just log the action
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
    materialState_.currentPreset = (materialState_.currentPreset + 1) % 5;
    
    const std::vector<std::string> presetNames = {
        "Default (Warm Brown)",
        "Metallic Gold", 
        "Ruby Red",
        "Chrome",
        "Emerald Green"
    };
    
    VKMON_INFO("[MATERIAL] Preset: " + presetNames[materialState_.currentPreset]);
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