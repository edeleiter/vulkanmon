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
        connectDeferredSystems();   // Connect systems that need renderer resources
        initializeInputSystem();
        createProjectileTestScene(); // Create clean projectile test scene

        // Perform GPU warm-up to eliminate first-frame delay
        if (renderer_ && renderer_->performGPUWarmup()) {
            VKMON_INFO("GPU warm-up completed - first frame delay should be eliminated");
        } else {
            VKMON_WARNING("GPU warm-up failed - first frame may still have delay");
        }

        initialized_ = true;
        VKMON_INFO("VulkanMon ready!");

        // Show window now that all systems are initialized and ready to render
        if (window_) {
            window_->show();
        }

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

    try {
        // Initialize timing AFTER all initialization is complete
        // This prevents first frame from including initialization time
        lastFrameTime_ = std::chrono::high_resolution_clock::now();

        VKMON_INFO("About to enter main loop - checking window shouldClose()...");
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
    try {
        auto frameStart = std::chrono::high_resolution_clock::now();

        updateFrameTiming();
        auto timingEnd = std::chrono::high_resolution_clock::now();

        // Poll window events
        window_->pollEvents();
        auto eventsEnd = std::chrono::high_resolution_clock::now();

        processInput(frameTime_);
        auto inputEnd = std::chrono::high_resolution_clock::now();

        updateSystems(frameTime_);
        auto systemsEnd = std::chrono::high_resolution_clock::now();

        updateECS(frameTime_);
        auto ecsEnd = std::chrono::high_resolution_clock::now();

        updateImGui(frameTime_);
        auto imguiEnd = std::chrono::high_resolution_clock::now();

        render(frameTime_);
        auto renderEnd = std::chrono::high_resolution_clock::now();

        // Log performance breakdown when frame is slow (>100ms)
        if (frameTime_ > 100.0f) {
            auto timingMs = std::chrono::duration<float, std::milli>(timingEnd - frameStart).count();
            auto eventsMs = std::chrono::duration<float, std::milli>(eventsEnd - timingEnd).count();
            auto inputMs = std::chrono::duration<float, std::milli>(inputEnd - eventsEnd).count();
            auto systemsMs = std::chrono::duration<float, std::milli>(systemsEnd - inputEnd).count();
            auto ecsMs = std::chrono::duration<float, std::milli>(ecsEnd - systemsEnd).count();
            auto imguiMs = std::chrono::duration<float, std::milli>(imguiEnd - ecsEnd).count();
            auto renderMs = std::chrono::duration<float, std::milli>(renderEnd - imguiEnd).count();

            VKMON_WARNING("SLOW FRAME (" + std::to_string(frameTime_) + "ms total): " +
                         "timing=" + std::to_string(timingMs) + "ms, " +
                         "events=" + std::to_string(eventsMs) + "ms, " +
                         "input=" + std::to_string(inputMs) + "ms, " +
                         "systems=" + std::to_string(systemsMs) + "ms, " +
                         "ecs=" + std::to_string(ecsMs) + "ms, " +
                         "imgui=" + std::to_string(imguiMs) + "ms, " +
                         "render=" + std::to_string(renderMs) + "ms");
        }

    } catch (const std::exception& e) {
        VKMON_ERROR("processFrame error: " + std::string(e.what()));
        throw;
    }
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
        physicsSystem_->update(deltaTime, world_->getEntityManager());
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

        // Entity rotation can be controlled per-entity through Transform components
        // Global rotation animation was removed in favor of individual entity control

        // CRITICAL FIX: Convert deltaTime from milliseconds to seconds for ECS systems
        // frameTime_ is in milliseconds, but World expects seconds
        float deltaTimeSeconds = deltaTime / 1000.0f;
        world_->update(deltaTimeSeconds);
    }
}

void Application::makeRandomCubeFall() {
    if (!world_ || !physicsSystem_) {
        return;
    }

    // Enhanced Dynamic Falling Cube Spawner for Physics Stress Testing
    auto& entityManager = world_->getEntityManager();

    // Clean up cubes that have fallen below ground
    cleanupFallenCubes();

    // Check if we've reached maximum cube limit (prevent memory issues)
    if (fallingCubes_.size() >= MAX_FALLING_CUBES) {
        return;
    }

    // Random number generation for varied trajectories
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> xPos(-15.0f, 15.0f);  // Random X position
    std::uniform_real_distribution<float> zPos(-15.0f, 15.0f);  // Random Z position
    std::uniform_real_distribution<float> velocity(-2.0f, 2.0f); // Random initial velocity

    // Create new falling cube entity
    EntityID newCube = entityManager.createEntity();

    // Add Transform component (spawn from sky)
    Transform transform;
    transform.position = glm::vec3(xPos(gen), 20.0f, zPos(gen)); // Spawn at Y=20 (sky)
    transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // No rotation
    transform.scale = glm::vec3(0.8f, 0.8f, 0.8f); // Slightly smaller than scene cubes
    entityManager.addComponent(newCube, transform);

    // Add Renderable component (visible cube)
    Renderable renderable;
    renderable.meshPath = "cube.obj";
    renderable.materialId = 4; // Use Chrome material for distinction
    renderable.isVisible = true;
    entityManager.addComponent(newCube, renderable);

    // Add RigidBody component (physics properties)
    RigidBodyComponent rigidBody;
    rigidBody.isDynamic = true;
    rigidBody.mass = 2.0f; // Heavier than scene cubes for impact
    rigidBody.useGravity = true;
    rigidBody.velocity = glm::vec3(velocity(gen), 0.0f, velocity(gen)); // Random horizontal velocity
    rigidBody.restitution = 0.7f; // Good bounce for visual feedback
    rigidBody.friction = 0.5f; // Moderate friction
    entityManager.addComponent(newCube, rigidBody);

    // Add Collision component (physical interaction)
    CollisionComponent collision = CollisionComponent::createEnvironment(glm::vec3(0.8f, 0.8f, 0.8f));
    collision.layer = LayerMask::Environment; // Collides with creatures and other cubes
    entityManager.addComponent(newCube, collision);

    // Add Spatial component for spatial partitioning
    SpatialComponent spatial;
    spatial.boundingRadius = 0.8f;
    spatial.spatialLayers = LayerMask::Environment;
    entityManager.addComponent(newCube, spatial);

    // Track this cube for cleanup
    fallingCubes_.push_back(newCube);

    VKMON_INFO("Spawned dynamic falling cube #" + std::to_string(fallingCubes_.size()) +
               " at (" + std::to_string(transform.position.x) + ", " +
               std::to_string(transform.position.y) + ", " +
               std::to_string(transform.position.z) + ") with velocity (" +
               std::to_string(rigidBody.velocity.x) + ", " +
               std::to_string(rigidBody.velocity.z) + ")");
}

void Application::cleanupFallenCubes() {
    if (!world_) return;

    auto& entityManager = world_->getEntityManager();
    auto it = fallingCubes_.begin();

    while (it != fallingCubes_.end()) {
        EntityID cube = *it;

        // Check if cube still exists and has fallen below cleanup threshold
        if (entityManager.hasComponent<Transform>(cube)) {
            const auto& transform = entityManager.getComponent<Transform>(cube);

            // Clean up cubes that have fallen below Y = -10 (well below ground at Y = -4)
            if (transform.position.y < -10.0f) {
                VKMON_INFO("Cleaning up fallen cube at Y=" + std::to_string(transform.position.y));
                entityManager.destroyEntity(cube);
                it = fallingCubes_.erase(it);
                continue;
            }
        } else {
            // Entity no longer exists, remove from tracking
            it = fallingCubes_.erase(it);
            continue;
        }

        ++it;
    }
}

// =============================================================================
// PHYSICS TESTING AND VALIDATION METHODS
// =============================================================================

void Application::testRaycastSystem() {
    VKMON_INFO("=== PHYSICS TEST: Raycast System Validation ===");

    if (!physicsSystem_) {
        VKMON_ERROR("Physics system not available for raycast testing");
        return;
    }

    // Test 1: Raycast downward from sky to detect ground
    glm::vec3 skyOrigin(0.0f, 15.0f, 0.0f);  // High above scene
    glm::vec3 downDirection(0.0f, -1.0f, 0.0f);  // Straight down

    VKMON_INFO("Test 1: Raycast from sky (0, 15, 0) downward to detect ground");
    auto groundHit = physicsSystem_->raycast(skyOrigin, downDirection, 25.0f);

    if (groundHit.hit) {
        VKMON_INFO("✅ RAYCAST SUCCESS: Hit entity " + std::to_string(groundHit.entity) +
                  " at distance " + std::to_string(groundHit.distance) +
                  " (point: " + std::to_string(groundHit.point.x) + ", " +
                  std::to_string(groundHit.point.y) + ", " + std::to_string(groundHit.point.z) + ")");
    } else {
        VKMON_WARNING("❌ RAYCAST FAILED: No ground detected from sky position");
    }

    // Test 2: Raycast horizontally through cube formation
    glm::vec3 sideOrigin(-20.0f, 2.0f, 0.0f);  // Side of scene at cube level
    glm::vec3 rightDirection(1.0f, 0.0f, 0.0f);  // Straight right

    VKMON_INFO("Test 2: Raycast horizontally through cube formation");
    auto cubeHit = physicsSystem_->raycast(sideOrigin, rightDirection, 40.0f);

    if (cubeHit.hit) {
        VKMON_INFO("✅ RAYCAST SUCCESS: Hit cube entity " + std::to_string(cubeHit.entity) +
                  " at distance " + std::to_string(cubeHit.distance));
    } else {
        VKMON_WARNING("❌ RAYCAST FAILED: No cubes detected in horizontal raycast");
    }

    // Test 3: Raycast with no expected hits
    glm::vec3 emptyOrigin(0.0f, 25.0f, 0.0f);  // High above scene
    glm::vec3 upDirection(0.0f, 1.0f, 0.0f);  // Straight up into empty space

    VKMON_INFO("Test 3: Raycast upward into empty space (should miss)");
    auto missHit = physicsSystem_->raycast(emptyOrigin, upDirection, 10.0f);

    if (!missHit.hit) {
        VKMON_INFO("✅ RAYCAST SUCCESS: Correctly detected no hits in empty space");
    } else {
        VKMON_WARNING("❌ RAYCAST UNEXPECTED: Hit entity " + std::to_string(missHit.entity) + " in empty space");
    }

    VKMON_INFO("=== Raycast System Test Complete ===");
}

void Application::testSphereOverlapQueries() {
    VKMON_INFO("=== PHYSICS TEST: Sphere Overlap Queries Validation ===");

    if (!physicsSystem_) {
        VKMON_ERROR("Physics system not available for sphere overlap testing");
        return;
    }

    // Test 1: Large sphere at center to detect multiple cubes
    glm::vec3 centerPosition(0.0f, 2.0f, 0.0f);  // Center of cube formation
    float largeRadius = 5.0f;

    VKMON_INFO("Test 1: Large sphere overlap at center (radius 5.0) to detect multiple cubes");
    auto centerOverlaps = physicsSystem_->overlapSphere(centerPosition, largeRadius);

    VKMON_INFO("✅ SPHERE OVERLAP: Found " + std::to_string(centerOverlaps.size()) + " entities in center sphere");
    if (centerOverlaps.size() > 0) {
        VKMON_INFO("First few detected entities: ");
        for (size_t i = 0; i < std::min(centerOverlaps.size(), size_t(5)); ++i) {
            VKMON_INFO("  - Entity " + std::to_string(centerOverlaps[i]));
        }
    }

    // Test 2: Small sphere at specific cube location
    glm::vec3 cubePosition(2.0f, 2.0f, 2.0f);  // Should be near a cube
    float smallRadius = 1.5f;

    VKMON_INFO("Test 2: Small sphere overlap at cube location (2,2,2) radius 1.5");
    auto cubeOverlaps = physicsSystem_->overlapSphere(cubePosition, smallRadius);

    VKMON_INFO("✅ SPHERE OVERLAP: Found " + std::to_string(cubeOverlaps.size()) + " entities near cube");

    // Test 3: Sphere in empty space (should find nothing)
    glm::vec3 emptyPosition(50.0f, 50.0f, 50.0f);  // Far from any objects
    float mediumRadius = 3.0f;

    VKMON_INFO("Test 3: Sphere overlap in empty space (50,50,50) radius 3.0 (should be empty)");
    auto emptyOverlaps = physicsSystem_->overlapSphere(emptyPosition, mediumRadius);

    if (emptyOverlaps.empty()) {
        VKMON_INFO("✅ SPHERE OVERLAP SUCCESS: Correctly detected no entities in empty space");
    } else {
        VKMON_WARNING("❌ SPHERE OVERLAP UNEXPECTED: Found " + std::to_string(emptyOverlaps.size()) + " entities in empty space");
    }

    // Test 4: Ground detection sphere
    glm::vec3 groundPosition(0.0f, -3.0f, 0.0f);  // At ground level
    float groundRadius = 2.0f;

    VKMON_INFO("Test 4: Sphere overlap at ground level to detect floor");
    auto groundOverlaps = physicsSystem_->overlapSphere(groundPosition, groundRadius);

    VKMON_INFO("✅ SPHERE OVERLAP: Found " + std::to_string(groundOverlaps.size()) + " entities at ground level");

    VKMON_INFO("=== Sphere Overlap Queries Test Complete ===");
}

void Application::measureSpatialCollisionPerformance() {
    VKMON_INFO("=== PHYSICS TEST: Spatial Collision Performance Measurement ===");

    if (!physicsSystem_) {
        VKMON_ERROR("Physics system not available for performance testing");
        return;
    }

    // Get current physics statistics
    auto stats = physicsSystem_->getStats();

    VKMON_INFO("Current Physics Performance Metrics:");
    VKMON_INFO("  Active Rigid Bodies: " + std::to_string(stats.activeRigidBodies));
    VKMON_INFO("  Collision Checks: " + std::to_string(stats.collisionChecks));
    VKMON_INFO("  Collision Hits: " + std::to_string(stats.collisionHits));
    VKMON_INFO("  Update Time: " + std::to_string(stats.updateTime) + "ms");
    VKMON_INFO("  Average Velocity: " + std::to_string(stats.averageVelocity));

    // Performance validation thresholds
    const float TARGET_UPDATE_TIME = 5.0f;  // < 5ms per frame
    const int EXPECTED_ENTITIES = 512;      // 8x8x8 cube formation

    VKMON_INFO("Performance Validation:");

    // Check update time performance
    if (stats.updateTime < TARGET_UPDATE_TIME) {
        VKMON_INFO("✅ PERFORMANCE SUCCESS: Physics update time " + std::to_string(stats.updateTime) +
                  "ms is under " + std::to_string(TARGET_UPDATE_TIME) + "ms target");
    } else {
        VKMON_WARNING("❌ PERFORMANCE ISSUE: Physics update time " + std::to_string(stats.updateTime) +
                     "ms exceeds " + std::to_string(TARGET_UPDATE_TIME) + "ms target");
    }

    // Check entity count
    if (stats.activeRigidBodies >= EXPECTED_ENTITIES) {
        VKMON_INFO("✅ SCALE SUCCESS: Managing " + std::to_string(stats.activeRigidBodies) +
                  " rigid bodies (target: " + std::to_string(EXPECTED_ENTITIES) + "+)");
    } else {
        VKMON_INFO("📊 SCALE INFO: Currently managing " + std::to_string(stats.activeRigidBodies) +
                  " rigid bodies");
    }

    // Calculate collision check efficiency
    if (stats.activeRigidBodies > 0) {
        // For spatial optimization, collision checks should be much less than O(n²)
        int theoreticalBruteForce = (stats.activeRigidBodies * (stats.activeRigidBodies - 1)) / 2;
        float efficiency = (stats.collisionChecks > 0) ?
                          (float(theoreticalBruteForce) / float(stats.collisionChecks)) : 0.0f;

        VKMON_INFO("Collision Detection Efficiency:");
        VKMON_INFO("  Brute Force Would Be: " + std::to_string(theoreticalBruteForce) + " checks");
        VKMON_INFO("  Spatial Optimized: " + std::to_string(stats.collisionChecks) + " checks");
        VKMON_INFO("  Efficiency Gain: " + std::to_string(efficiency) + "x improvement");

        if (efficiency > 10.0f) {
            VKMON_INFO("✅ SPATIAL SUCCESS: Achieving " + std::to_string(efficiency) + "x efficiency gain");
        } else if (stats.collisionChecks == 0) {
            VKMON_INFO("📊 SPATIAL INFO: No collisions detected this frame");
        } else {
            VKMON_WARNING("❌ SPATIAL ISSUE: Low efficiency gain of " + std::to_string(efficiency) + "x");
        }
    }

    VKMON_INFO("=== Spatial Collision Performance Test Complete ===");
}

void Application::runPhysicsValidationTests() {
    VKMON_INFO("🚀 === COMPREHENSIVE PHYSICS SYSTEM VALIDATION ===");
    VKMON_INFO("Running all critical blocker validation tests...");

    // Test all three critical blockers in sequence
    testRaycastSystem();
    VKMON_INFO("");  // Spacing between tests

    testSphereOverlapQueries();
    VKMON_INFO("");  // Spacing between tests

    measureSpatialCollisionPerformance();
    VKMON_INFO("");  // Final summary

    VKMON_INFO("🎯 === Physics Validation Complete ===");
    VKMON_INFO("All critical blockers tested. Check logs above for detailed results.");
    VKMON_INFO("Key Controls:");
    VKMON_INFO("  7 = Test Raycast System");
    VKMON_INFO("  8 = Test Sphere Overlap");
    VKMON_INFO("  9 = Measure Performance");
    VKMON_INFO("  0 = Run All Tests");
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