#include "Application.h"
#include "../utils/Utils.h"
#include "../utils/Logger.h"
#include "../game/CreatureDetectionSystem.h"
#include "../components/RigidBodyComponent.h"
#include "../components/CollisionComponent.h"
#include <iostream>
#include <stdexcept>
#include <random>

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

        // Log actual FPS every 5 seconds instead of every second
        static int logCounter = 0;
        if (++logCounter >= 5) {
            VKMON_INFO("Application Performance: " + std::to_string(static_cast<int>(fps_)) + " FPS, " +
                       std::to_string(frameTime_) + "ms frame time");
            logCounter = 0;
        }

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

    // Update physics simulation
    if (physicsSystem_ && world_) {
        physicsSystem_->update(world_->getEntityManager(), deltaTime);
    }

    // Future: Update game logic, animations, other systems, etc.
}

void Application::updateECS(float deltaTime) {
    if (world_) {
        // Random falling cube demonstration - one cube falls every 5 seconds
        static float fallingCubeTimer = 0.0f;
        fallingCubeTimer += deltaTime;

        if (fallingCubeTimer >= 5000.0f) { // 5 seconds in milliseconds
            fallingCubeTimer = 0.0f;
            makeRandomCubeFall();
        }

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

        // Debug: Occasional grid verification (reduced frequency)
        static int debugFrameCounter = 0;
        if (debugFrameCounter % 1800 == 0) { // Every 30 seconds at 60fps
            int entityCount = 0;
            for (EntityID entity : entities) {
                if (world_->hasComponent<CreatureComponent>(entity) && entityCount < 5) {
                    auto& transform = world_->getComponent<Transform>(entity);
                    VKMON_DEBUG("Entity " + std::to_string(entityCount) + " pos: (" +
                               std::to_string(transform.position.x) + "," +
                               std::to_string(transform.position.y) + "," +
                               std::to_string(transform.position.z) + ")");
                    entityCount++;
                }
            }
            VKMON_DEBUG("Total entities with CreatureComponent: " + std::to_string(entities.size()));
        }
        debugFrameCounter++;

        // TODO: Add back rotation animation as needed for specific entities
        // Rotation animation disabled to see clean 10x10x10 cube formation

        // PERFORMANCE TEST: Only essential systems enabled (CameraSystem + RenderSystem)
        world_->update(deltaTime);
    }
}

void Application::makeRandomCubeFall() {
    if (!world_ || !physicsSystem_) {
        return;
    }

    // Get all entities with creature components (our cubes)
    auto& entityManager = world_->getEntityManager();
    const auto& creatureEntities = entityManager.getEntitiesWithComponent<CreatureComponent>();

    if (creatureEntities.empty()) {
        return;
    }

    // Select a random creature entity
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, creatureEntities.size() - 1);

    EntityID randomEntity = creatureEntities[dis(gen)];

    // Check if this entity already has physics components
    if (entityManager.hasComponent<RigidBodyComponent>(randomEntity)) {
        // Reset position to make it fall again
        auto& transform = entityManager.getComponent<Transform>(randomEntity);
        auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(randomEntity);

        // Reset to original position but higher up
        transform.position.y += 10.0f; // Add 10 units height
        rigidBody.velocity = glm::vec3(0.0f, 0.0f, 0.0f); // Reset velocity
        rigidBody.needsSync = true;

        VKMON_INFO("Making existing physics cube fall again from higher position");
    } else {
        // Add physics components to make it fall
        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.mass = 1.0f;
        rigidBody.useGravity = true;
        rigidBody.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rigidBody.restitution = 0.5f; // Add some bounce
        rigidBody.friction = 0.7f;
        rigidBody.needsSync = true;
        entityManager.addComponent(randomEntity, rigidBody);

        // Add collision detection only if it doesn't already exist
        if (!entityManager.hasComponent<CollisionComponent>(randomEntity)) {
            CollisionComponent collision = CollisionComponent::createCreature(0.5f, 1.0f);
            entityManager.addComponent(randomEntity, collision);
        }

        // Get entity position for logging
        auto& transform = entityManager.getComponent<Transform>(randomEntity);
        VKMON_INFO("Making NEW cube fall with gravity at position (" +
                   std::to_string(transform.position.x) + ", " +
                   std::to_string(transform.position.y) + ", " +
                   std::to_string(transform.position.z) + ")");
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

    // Update Vulkan viewport and swapchain
    if (renderer_) {
        renderer_->handleWindowResize(width, height);
    }

    // Update camera aspect ratio
    if (cameraSystem_ && world_) {
        cameraSystem_->handleWindowResize(width, height, world_->getEntityManager());

        // VIEWPORT FIX: Immediately update projection matrix after camera resize
        // This ensures the GPU gets the correct projection matrix right away,
        // instead of waiting for the next frame update cycle
        updateCameraMatrices();
    }
}

void Application::render(float deltaTime) {
    if (renderer_ && renderer_->isInitialized()) {
        // Update camera matrices from ECS before rendering
        updateCameraMatrices();

        // VulkanRenderer will call back to ECS during renderFrame()
        renderer_->renderFrame(deltaTime);
    }
}

// =============================================================================
// UNIFIED CAMERA INTERFACE - Bridge ECS camera to VulkanRenderer
// =============================================================================

/**
 * Updates VulkanRenderer with camera data from active ECS camera entity
 *
 * Unified Camera Architecture Data Flow:
 * 1. CameraSystem finds active camera entity with Camera component
 * 2. CameraSystem calculates view/projection matrices from Transform + Camera components
 * 3. CameraSystem extracts camera position from Transform component
 * 4. Application bridges all camera data to VulkanRenderer via clean interface
 * 5. VulkanRenderer uses unified camera data for both rendering and lighting calculations
 *
 * This design ensures:
 * - Single source of truth: All camera data comes from ECS entities
 * - Clean interfaces: No EntityManager dependencies outside ECS systems
 * - Consistent data: Same camera used for rendering, spatial culling, and lighting
 */
void Application::updateCameraMatrices() {
    if (!cameraSystem_ || !renderer_) {
        VKMON_WARNING("Application::updateCameraMatrices: Missing cameraSystem or renderer");
        return;
    }

    // Get EntityManager reference for camera access
    auto& entityManager = world_->getEntityManager();

    // Check if ECS camera is available
    if (cameraSystem_->hasActiveCamera(entityManager)) {
        // Get matrices from ECS camera system
        glm::mat4 viewMatrix = cameraSystem_->getActiveViewMatrix(entityManager);
        glm::mat4 projectionMatrix = cameraSystem_->getActiveProjectionMatrix(entityManager);
        glm::vec3 cameraPosition = cameraSystem_->getActiveCameraPosition(entityManager);

        // Update VulkanRenderer with ECS camera data
        renderer_->setViewMatrix(viewMatrix);
        renderer_->setProjectionMatrix(projectionMatrix);
        renderer_->setCameraPosition(cameraPosition);

        // ECS camera successfully integrated with VulkanRenderer
    } else {
        VKMON_WARNING("Application: No active ECS camera - VulkanRenderer will use fallback matrices");
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

    // CRITICAL: Clear system shared_ptrs BEFORE destroying renderer
    // This ensures systems are destroyed while Vulkan device is still valid
    VKMON_INFO("Clearing system references before renderer cleanup...");
    materialSystem_.reset();
    lightingSystem_.reset();
    modelLoader_.reset();
    assetManager_.reset();
    resourceManager_.reset();

    // Now safe to destroy renderer and Vulkan device
    if (renderer_) {
        VKMON_INFO("Cleaning up renderer...");
        renderer_.reset();
    }

    // Old camera_ cleanup removed - ECS camera entities managed by World

    if (window_) {
        // Window cleanup is automatic (RAII)
        window_.reset();
    }

    VKMON_INFO("Application cleanup completed");
}


// Include the separated implementation files
#include "ApplicationSetup.cpp"
#include "ApplicationInputs.cpp"