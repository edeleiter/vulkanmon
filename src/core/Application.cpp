#include "Application.h"
#include "../utils/Utils.h"
#include "../utils/Logger.h"
#include <iostream>
#include <stdexcept>

// ImGui includes for debug interface
#include <imgui.h>

using namespace VulkanMon;

// =============================================================================
// Core Application Lifecycle Methods
// =============================================================================

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

    running_ = false;
    VKMON_INFO("Application shutdown requested");

    // Additional shutdown logic can be added here
}

// =============================================================================
// Main Loop Methods
// =============================================================================

void Application::processFrame() {
    updateFrameTiming();

    // Poll window events
    window_->pollEvents();

    processInput(frameTime_);
    updateSystems(frameTime_);
    updateECS(frameTime_);
    updateImGui(frameTime_);
    render(frameTime_);
}

void Application::updateFrameTiming() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::milli>(currentTime - lastFrameTime_);
    frameTime_ = duration.count();
    lastFrameTime_ = currentTime;

    // Calculate FPS (update every ~60 frames to avoid flicker)
    static int frameCount = 0;
    static float fpsAccumulator = 0.0f;

    frameCount++;
    fpsAccumulator += frameTime_;

    if (frameCount >= 60) {
        fps_ = 60000.0f / fpsAccumulator;  // 60 frames * 1000ms per second / accumulated time
        frameCount = 0;
        fpsAccumulator = 0.0f;
    }
}

void Application::processInput(float deltaTime) {
    // Process continuous input (WASD movement, held keys)
    if (inputHandler_) {
        inputHandler_->processContinuousInput(window_->getWindow(), deltaTime);
    }
}

void Application::updateSystems(float deltaTime) {
    // Update various engine systems each frame
    // Future: Update game logic, animations, physics, etc.
}

void Application::updateECS(float deltaTime) {
    if (world_) {
        // Update cube rotations for animation
        static float rotationAngle = 0.0f;
        const auto& entities = world_->getEntityManager().getEntitiesWithComponent<Transform>();

        // Rotate around Y-axis at 45 degrees per second
        float rotationSpeed = 45.0f; // degrees per second
        rotationAngle += rotationSpeed * deltaTime / 1000.0f;

        // Keep angle in reasonable range
        if (rotationAngle > 360.0f) {
            rotationAngle -= 360.0f;
        }

        for (EntityID entity : entities) {
            auto& transform = world_->getComponent<Transform>(entity);
            // Apply rotation to all test cubes
            transform.setRotationEuler(0.0f, rotationAngle, 0.0f);
        }

        world_->update(deltaTime);
    }
}

void Application::updateImGui(float deltaTime) {
    if (renderer_ && renderer_->isImGuiEnabled()) {
        renderer_->beginImGuiFrame();

        // Phase 6.3 Complete: Clean ImGui interface - only ECS Inspector when toggled

        // Render ECS Inspector (only if enabled)
        if (ecsInspector_ && inspectorEnabled_) {
            ecsInspector_->render(frameTime_);
        }

        renderer_->endImGuiFrame();
    }
}

void Application::toggleInspector() {
    inspectorEnabled_ = !inspectorEnabled_;
    VKMON_INFO(std::string("ECS Inspector ") + (inspectorEnabled_ ? "enabled" : "disabled"));
}

void Application::handleWindowResize(int width, int height) {
    VKMON_INFO("Window resize event: " + std::to_string(width) + "x" + std::to_string(height));

    if (renderer_) {
        renderer_->handleWindowResize(width, height);
    }
}

void Application::render(float deltaTime) {
    if (renderer_ && renderer_->isInitialized()) {
        // VulkanRenderer will call back to ECS during renderFrame()
        renderer_->renderFrame(deltaTime);
    }
}

// =============================================================================
// Cleanup Methods
// =============================================================================

void Application::cleanup() {
    VKMON_INFO("Beginning Application cleanup...");

    // Cleanup in reverse order of initialization
    if (ecsInspector_) {
        VKMON_DEBUG("Cleaning up ECS Inspector...");
        ecsInspector_.reset();
    }

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

    if (inputHandler_) {
        // InputHandler cleanup is automatic (RAII)
        inputHandler_.reset();
    }

    // Note: Systems cleanup order is important
    // MaterialSystem and LightingSystem are owned by VulkanRenderer
    if (renderer_) {
        VKMON_INFO("Cleaning up renderer...");
        renderer_.reset();  // This will cleanup owned systems
    }

    // These were references to renderer-owned systems, now null
    materialSystem_.reset();
    lightingSystem_.reset();
    modelLoader_.reset();
    assetManager_.reset();
    resourceManager_.reset();

    if (camera_) {
        // Camera cleanup is automatic (RAII)
        camera_.reset();
    }

    if (window_) {
        // Window cleanup is automatic (RAII)
        window_.reset();
    }

    VKMON_INFO("Application cleanup completed");
}

// Include the separated implementation files
#include "ApplicationSetup.cpp"
#include "ApplicationInputs.cpp"