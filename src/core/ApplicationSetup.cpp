#include "Application.h"
#include "../utils/Logger.h"
#include "../systems/SpatialSystem.h"
#include "../systems/CreatureRenderSystem.h"
#include "../components/SpatialComponent.h"
#include "../components/Camera.h"
#include "../components/RigidBodyComponent.h"
#include "../components/CollisionComponent.h"
#include "../components/CreaturePhysicsComponent.h"
#include "../spatial/WorldConfig.h"
#include "../config/CameraConfig.h"
#include <stdexcept>
#include <set>
#include <chrono>

using namespace VulkanMon;

// =============================================================================
// System Initialization Methods
// =============================================================================

void Application::initializeLogger() {
    Logger::getInstance().enableConsoleOutput(true);
    Logger::getInstance().setLogLevel(LogLevel::INFO_LEVEL);
}

void Application::initializeWindow() {
    window_ = std::make_unique<Window>(Config::Camera::DEFAULT_WINDOW_WIDTH, Config::Camera::DEFAULT_WINDOW_HEIGHT, "VulkanMon");
    window_->initialize();
    // Window logs its own initialization success
}

void Application::initializeCamera() {
    // Old Camera class removed - ECS camera entities now handle camera positioning
    // Camera is created as ECS entity in createTestScene() method
    VKMON_INFO("Legacy camera initialization removed - ECS camera system active");
}

void Application::initializeCoreEngineSystems() {
    // Core systems will be initialized after renderer is ready
}

void Application::initializeRenderer() {
    // Create placeholder systems that renderer will initialize
    resourceManager_ = nullptr;  // Will be created by renderer
    assetManager_ = nullptr;     // Will be created by renderer
    modelLoader_ = nullptr;      // Will be created by renderer
    lightingSystem_ = nullptr;   // Will be created by renderer
    materialSystem_ = nullptr;   // Will be created by renderer

    // Create VulkanRenderer with shared dependencies
    renderer_ = std::make_shared<VulkanRenderer>(
        window_,
        resourceManager_,
        assetManager_,
        modelLoader_,
        lightingSystem_,
        materialSystem_
    );

    renderer_->initialize();

    // Get initialized systems from renderer
    lightingSystem_ = renderer_->getLightingSystem();
    materialSystem_ = renderer_->getMaterialSystem();
    assetManager_ = renderer_->getAssetManager();
}

void Application::initializeInputSystem() {
    // Create InputHandler with ECS camera system
    if (!cameraSystem_ || !world_) {
        VKMON_ERROR("Cannot initialize InputHandler: ECS camera system not available");
        return;
    }

    inputHandler_ = std::make_shared<InputHandler>(window_, cameraSystem_, world_.get());
    VKMON_INFO("InputHandler initialized with ECS camera system");

    // System control callbacks
    inputHandler_->setShaderReloadCallback([this]() {
        handleShaderReload();
    });

    inputHandler_->setLightingControlCallback([this](int key) {
        handleLightingControl(key);
    });

    inputHandler_->setMaterialControlCallback([this](int key) {
        handleMaterialControl(key);
    });

    inputHandler_->setInspectorToggleCallback([this]() {
        toggleInspector();
    });

    // Connect window input callbacks for WASD + mouse controls
    window_->setKeyCallback([this](int key, int scancode, int action, int mods) {
        if (inputHandler_) {
            inputHandler_->processKeyInput(key, scancode, action, mods);
        }
    });

    window_->setMouseCallback([this](double xpos, double ypos) {
        if (inputHandler_) {
            inputHandler_->processMouseInput(xpos, ypos);
        }
    });

    // Connect window resize callback
    window_->setResizeCallback([this](int width, int height) {
        handleWindowResize(width, height);
    });

    VKMON_INFO("System callbacks connected");
    VKMON_INFO("Input system initialized");
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

    // Add creature render system for massive creature rendering (Phase 7.1)
    creatureRenderSystem_ = world_->addSystem<CreatureRenderSystem>(cameraSystem_);

    // PERFORMANCE OPTIMIZATION: Disable frustum culling for stress test scenario
    // In 10x10x10 cube test, all 1000 creatures are visible, so spatial queries add overhead without benefit
    creatureRenderSystem_->setEnableFrustumCulling(false);
    VKMON_INFO("CreatureRenderSystem: Frustum culling DISABLED for stress test performance");

    // Add spatial system for Pokemon-style spatial queries and management
    WorldConfig worldConfig = WorldConfig::createTestWorld();
    VKMON_INFO("Using world config: " + worldConfig.name +
               " bounds (" + std::to_string(worldConfig.minBounds.x) + "," +
                           std::to_string(worldConfig.minBounds.y) + "," +
                           std::to_string(worldConfig.minBounds.z) + ") to (" +
                           std::to_string(worldConfig.maxBounds.x) + "," +
                           std::to_string(worldConfig.maxBounds.y) + "," +
                           std::to_string(worldConfig.maxBounds.z) + ")");
    // Add spatial system for Pokemon-style spatial queries and octree partitioning
    spatialSystem_ = world_->addSystem<SpatialSystem>(worldConfig.getBoundingBox());

    // Add physics system for realistic physics simulation
    physicsSystem_ = world_->addSystem<PhysicsSystem>();
    physicsSystem_->initialize(world_->getEntityManager()); // Initialize with default gravity
    VKMON_INFO("PhysicsSystem added to World and initialized");

    // Add CreatureDetectionSystem for AI behavior and spatial detection
    // creatureDetectionSystem_ = world_->addSystem<CreatureDetectionSystem>();

    // Use centralized system connection with validation
    world_->connectSystems();
    VKMON_INFO("System dependency connection completed via World::connectSystems()");

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

    // Initialize ECS Inspector
    ecsInspector_ = std::make_unique<Debug::ECSInspector>(world_.get());
    VKMON_DEBUG("ECS Inspector initialized successfully");

    VKMON_INFO("ECS systems initialized successfully");
}

void Application::createTestScene() {
    if (!world_) {
        VKMON_WARNING("Cannot create test scene: ECS World not initialized");
        return;
    }

    VKMON_INFO("Creating Jolt Physics Demonstration Scene...");
    VKMON_INFO("Showcasing: Different shapes, materials, physics behaviors, and spatial queries");

    // =========================================================================
    // JOLT PHYSICS DEMONSTRATION SCENE
    // =========================================================================

    size_t totalEntities = 0;

    // =========================================================================
    // GROUND PLANE - CREATE FIRST so it's ready when objects start falling
    // =========================================================================
    VKMON_INFO("Creating ground plane collision first...");

    EntityID groundEntity = world_->createEntity();

    // Create visual ground plane
    Transform groundTransform;

    // Position ground plane RIGHT AT ORIGIN for obvious collision test
    groundTransform.position = glm::vec3(0.0f, 0.0f, 0.0f); // Ground at origin level - impossible to miss!
    groundTransform.setRotationEuler(0.0f, 0.0f, 0.0f);
    groundTransform.scale = glm::vec3(50.0f, 2.0f, 50.0f); // MASSIVE thick ground - impossible to fall through
    world_->addComponent(groundEntity, groundTransform);

    // Make ground plane visible
    Renderable groundRenderable;
    groundRenderable.meshPath = "plane.obj"; // Use plane mesh
    groundRenderable.texturePath = "default";
    groundRenderable.materialId = 3; // Chrome for visibility
    groundRenderable.isVisible = true;
    groundRenderable.renderLayer = 0;
    groundRenderable.lodDistance = 1000.0f;
    world_->addComponent(groundEntity, groundRenderable);

    // Add spatial component for ground
    SpatialComponent groundSpatial;
    groundSpatial.spatialLayers = LayerMask::Environment;
    groundSpatial.boundingRadius = 15.0f;
    groundSpatial.behavior = SpatialBehavior::STATIC;
    world_->addComponent(groundEntity, groundSpatial);

    // CRITICAL: Add physics components FIRST
    RigidBodyComponent groundRigidBody = RigidBodyComponent::staticBody();
    world_->addComponent(groundEntity, groundRigidBody);

    // MASSIVE box collision that matches the visual - impossible to miss
    CollisionComponent groundCollision = CollisionComponent::box(glm::vec3(50.0f, 2.0f, 50.0f), LayerMask::Environment);
    groundCollision.isStatic = true;
    world_->addComponent(groundEntity, groundCollision);

    VKMON_INFO("MASSIVE ground plane created at Y=0.0 - 50x2x50 box collision (impossible to miss!)");
    totalEntities++;

    // =========================================================================
    // SECTION 1: DIFFERENT COLLISION SHAPES DEMO
    // =========================================================================
    VKMON_INFO("Section 1: Creating different collision shapes falling from height...");

    // Row 1: Spheres - Different masses and materials
    for (int i = 0; i < 5; ++i) {
        EntityID sphereEntity = world_->createEntity();

        Transform sphereTransform;
        sphereTransform.position = glm::vec3(-8.0f + i * 2.0f, 15.0f, -8.0f);
        sphereTransform.scale = glm::vec3(0.6f);
        world_->addComponent(sphereEntity, sphereTransform);

        // Varied physics properties
        RigidBodyComponent sphereRigidBody = RigidBodyComponent::dynamic(1.0f + i * 0.5f); // Mass: 1.0, 1.5, 2.0, 2.5, 3.0
        sphereRigidBody.restitution = 0.1f + i * 0.2f; // Bouncy progression: 0.1 to 0.9
        sphereRigidBody.friction = 0.3f + i * 0.15f;   // Friction progression
        world_->addComponent(sphereEntity, sphereRigidBody);

        CollisionComponent sphereCollision = CollisionComponent::sphere(0.6f, LayerMask::Creatures);
        world_->addComponent(sphereEntity, sphereCollision);

        // Visual appearance
        Renderable sphereRenderable;
        sphereRenderable.meshPath = "sphere.obj";
        sphereRenderable.materialId = i % 4; // Cycle through materials
        sphereRenderable.isVisible = true;
        world_->addComponent(sphereEntity, sphereRenderable);

        // Spatial tracking
        SpatialComponent sphereSpatial(0.8f, SpatialBehavior::DYNAMIC, LayerMask::Creatures);
        world_->addComponent(sphereEntity, sphereSpatial);

        totalEntities++;
    }

    // Row 2: Boxes - Different sizes and orientations
    for (int i = 0; i < 4; ++i) {
        EntityID boxEntity = world_->createEntity();

        Transform boxTransform;
        boxTransform.position = glm::vec3(-6.0f + i * 3.0f, 12.0f, -4.0f);
        boxTransform.setRotationEuler(i * 15.0f, i * 25.0f, i * 10.0f); // Varied rotations
        boxTransform.scale = glm::vec3(0.8f + i * 0.2f); // Different sizes
        world_->addComponent(boxEntity, boxTransform);

        RigidBodyComponent boxRigidBody = RigidBodyComponent::dynamic(2.0f);
        boxRigidBody.restitution = 0.2f;
        boxRigidBody.friction = 0.8f;
        world_->addComponent(boxEntity, boxRigidBody);

        CollisionComponent boxCollision = CollisionComponent::box(glm::vec3(0.8f + i * 0.2f), LayerMask::Creatures);
        world_->addComponent(boxEntity, boxCollision);

        Renderable boxRenderable;
        boxRenderable.meshPath = "cube.obj";
        boxRenderable.materialId = (i + 1) % 4;
        boxRenderable.isVisible = true;
        world_->addComponent(boxEntity, boxRenderable);

        SpatialComponent boxSpatial(1.0f + i * 0.2f, SpatialBehavior::DYNAMIC, LayerMask::Creatures);
        world_->addComponent(boxEntity, boxSpatial);

        totalEntities++;
    }

    // Row 3: Capsules - Character-like physics
    for (int i = 0; i < 3; ++i) {
        EntityID capsuleEntity = world_->createEntity();

        Transform capsuleTransform;
        capsuleTransform.position = glm::vec3(-4.0f + i * 4.0f, 18.0f, 0.0f);
        capsuleTransform.scale = glm::vec3(0.5f, 1.2f, 0.5f); // Tall and narrow
        world_->addComponent(capsuleEntity, capsuleTransform);

        // Character-like physics
        RigidBodyComponent capsuleRigidBody = RigidBodyComponent::dynamic(1.5f);
        capsuleRigidBody.restitution = 0.1f; // Low bounce (character-like)
        capsuleRigidBody.friction = 0.9f;    // High friction (good grip)
        capsuleRigidBody.linearDamping = 0.3f; // Natural slowdown
        world_->addComponent(capsuleEntity, capsuleRigidBody);

        CollisionComponent capsuleCollision = CollisionComponent::capsule(0.5f, 2.4f, LayerMask::Creatures);
        world_->addComponent(capsuleEntity, capsuleCollision);

        Renderable capsuleRenderable;
        capsuleRenderable.meshPath = "sphere.obj"; // Use sphere as capsule approximation
        capsuleRenderable.materialId = (i + 2) % 4;
        capsuleRenderable.isVisible = true;
        world_->addComponent(capsuleEntity, capsuleRenderable);

        // Add creature physics for character behavior
        CreaturePhysicsComponent creaturePhysics = CreaturePhysicsComponent::createLandCreature(4.0f, 1.5f);
        world_->addComponent(capsuleEntity, creaturePhysics);

        SpatialComponent capsuleSpatial(0.8f, SpatialBehavior::DYNAMIC, LayerMask::Creatures);
        world_->addComponent(capsuleEntity, capsuleSpatial);

        totalEntities++;
    }

    // =========================================================================
    // SECTION 2: STATIC ENVIRONMENT - Obstacles and Platforms
    // =========================================================================
    VKMON_INFO("Section 2: Creating static environment obstacles...");

    // Static platform 1
    EntityID platform1 = world_->createEntity();
    Transform platform1Transform;
    platform1Transform.position = glm::vec3(-5.0f, 8.0f, -2.0f);
    platform1Transform.scale = glm::vec3(3.0f, 0.5f, 2.0f);
    world_->addComponent(platform1, platform1Transform);

    RigidBodyComponent platform1RigidBody = RigidBodyComponent::staticBody();
    world_->addComponent(platform1, platform1RigidBody);

    CollisionComponent platform1Collision = CollisionComponent::box(glm::vec3(3.0f, 0.5f, 2.0f), LayerMask::Environment);
    world_->addComponent(platform1, platform1Collision);

    Renderable platform1Renderable;
    platform1Renderable.meshPath = "cube.obj";
    platform1Renderable.materialId = 3; // Chrome material
    platform1Renderable.isVisible = true;
    world_->addComponent(platform1, platform1Renderable);

    // Static platform 2
    EntityID platform2 = world_->createEntity();
    Transform platform2Transform;
    platform2Transform.position = glm::vec3(5.0f, 6.0f, 2.0f);
    platform2Transform.scale = glm::vec3(2.5f, 0.5f, 3.0f);
    world_->addComponent(platform2, platform2Transform);

    RigidBodyComponent platform2RigidBody = RigidBodyComponent::staticBody();
    world_->addComponent(platform2, platform2RigidBody);

    CollisionComponent platform2Collision = CollisionComponent::box(glm::vec3(2.5f, 0.5f, 3.0f), LayerMask::Environment);
    world_->addComponent(platform2, platform2Collision);

    Renderable platform2Renderable;
    platform2Renderable.meshPath = "cube.obj";
    platform2Renderable.materialId = 2; // Ruby material
    platform2Renderable.isVisible = true;
    world_->addComponent(platform2, platform2Renderable);

    totalEntities += 2;

    // =========================================================================
    // SECTION 3: PHYSICS MATERIAL SHOWCASE
    // =========================================================================
    VKMON_INFO("Section 3: Creating physics material demonstration...");

    // Bouncy balls with different restitution
    std::vector<float> bounciness = {0.1f, 0.3f, 0.6f, 0.9f}; // Low to high bounce
    for (int i = 0; i < 4; ++i) {
        EntityID bounceEntity = world_->createEntity();

        Transform bounceTransform;
        bounceTransform.position = glm::vec3(6.0f + i * 1.5f, 20.0f, -6.0f);
        bounceTransform.scale = glm::vec3(0.4f);
        world_->addComponent(bounceEntity, bounceTransform);

        RigidBodyComponent bounceRigidBody = RigidBodyComponent::dynamic(0.8f);
        bounceRigidBody.restitution = bounciness[i];
        bounceRigidBody.friction = 0.4f;
        world_->addComponent(bounceEntity, bounceRigidBody);

        CollisionComponent bounceCollision = CollisionComponent::sphere(0.4f, LayerMask::Items);
        world_->addComponent(bounceEntity, bounceCollision);

        Renderable bounceRenderable;
        bounceRenderable.meshPath = "sphere.obj";
        bounceRenderable.materialId = i; // Different colors for different bounce
        bounceRenderable.isVisible = true;
        world_->addComponent(bounceEntity, bounceRenderable);

        SpatialComponent bounceSpatial(0.6f, SpatialBehavior::DYNAMIC, LayerMask::Items);
        world_->addComponent(bounceEntity, bounceSpatial);

        totalEntities++;
    }

    VKMON_INFO("Section 1-3 Complete: " + std::to_string(totalEntities) + " physics demonstration entities created");
    VKMON_INFO("Physics Demo Scene Layout:");
    VKMON_INFO("  - Row 1: 5 spheres falling from height 15 (different masses & bounce)");
    VKMON_INFO("  - Row 2: 4 boxes falling from height 12 (different sizes & orientations)");
    VKMON_INFO("  - Row 3: 3 capsules falling from height 18 (character-like physics)");
    VKMON_INFO("  - Static platforms at heights 8 and 6 to catch falling objects");
    VKMON_INFO("  - 4 bouncy balls from height 20 (bounce test: 0.1, 0.3, 0.6, 0.9)");
    VKMON_INFO("  - Ground plane at Y=-4 to catch everything");
    VKMON_INFO("  - Camera at (0,10,20) looking down at the action");

    // =========================================================================
    // SECTION 4: SPATIAL QUERY DEMONSTRATION
    // =========================================================================
    // Additional target objects for spatial query testing
    for (int i = 0; i < 3; ++i) {
        EntityID targetEntity = world_->createEntity();

        Transform targetTransform;
        targetTransform.position = glm::vec3(-2.0f + i * 4.0f, 3.0f, 4.0f);
        targetTransform.scale = glm::vec3(0.7f);
        world_->addComponent(targetEntity, targetTransform);

        RigidBodyComponent targetRigidBody = RigidBodyComponent::kinematic(); // Controlled movement for queries
        world_->addComponent(targetEntity, targetRigidBody);

        CollisionComponent targetCollision = CollisionComponent::sphere(0.7f, LayerMask::Items);
        world_->addComponent(targetEntity, targetCollision);

        Renderable targetRenderable;
        targetRenderable.meshPath = "sphere.obj";
        targetRenderable.materialId = 1; // Gold material for visibility
        targetRenderable.isVisible = true;
        world_->addComponent(targetEntity, targetRenderable);

        // These will be used for spatial query demonstrations
        SpatialComponent targetSpatial(0.9f, SpatialBehavior::DYNAMIC, LayerMask::Items);
        world_->addComponent(targetEntity, targetSpatial);

        totalEntities++;
    }

    VKMON_INFO("Section 4: Spatial query targets created");

    // =========================================================================
    // CAMERA SETUP - Positioned to see physics demonstration clearly
    // =========================================================================
    EntityID cameraEntity = world_->createEntity();
    Transform cameraTransform;
    cameraTransform.position = glm::vec3(0.0f, 10.0f, 20.0f);  // Closer view of physics demo
    cameraTransform.setRotationEuler(-20.0f, 0.0f, 0.0f); // Angled down to see falling objects and ground
    cameraTransform.scale = glm::vec3(1.0f);
    world_->addComponent(cameraEntity, cameraTransform);

    Camera cameraComponent;
    cameraComponent.fov = 45.0f;  // Much narrower FOV for less distortion - standard value
    cameraComponent.nearPlane = Config::Camera::DEFAULT_NEAR_PLANE;
    cameraComponent.farPlane = Config::Camera::DEFAULT_FAR_PLANE;
    cameraComponent.aspectRatio = 16.0f / 9.0f;
    cameraComponent.isActive = true;
    cameraComponent.updateProjectionMatrix();
    world_->addComponent(cameraEntity, cameraComponent);

    SpatialComponent cameraSpatial;
    cameraSpatial.spatialLayers = LayerMask::Camera;
    cameraSpatial.boundingRadius = 0.1f;
    cameraSpatial.behavior = SpatialBehavior::DYNAMIC;
    world_->addComponent(cameraEntity, cameraSpatial);

    // Create test player entity for creature detection testing
    EntityID playerEntity = world_->createEntity();

    // Add Transform component positioned at center of creature grid
    Transform playerTransform;
    playerTransform.position = glm::vec3(0.0f, 1.0f, 0.0f); // Center of 6x6 grid, slightly elevated
    playerTransform.setRotationEuler(0.0f, 0.0f, 0.0f);
    playerTransform.scale = glm::vec3(1.0f);
    world_->addComponent(playerEntity, playerTransform);

    // Add SpatialComponent with Player layer
    SpatialComponent playerSpatial(2.0f, SpatialBehavior::DYNAMIC, LayerMask::Player);
    world_->addComponent(playerEntity, playerSpatial);

    VKMON_INFO("Physics demonstration scene setup complete: " + std::to_string(totalEntities) + " entities created");

    // Scene setup complete - physics bodies will be created automatically on first update

    // Preload all models before declaring scene ready
    preloadSceneAssets();

    VKMON_INFO("createTestScene() completing successfully");
    // Scene setup complete - main application will log ready message
}

void Application::preloadSceneAssets() {
    if (!world_ || !renderer_) {
        VKMON_WARNING("Cannot preload scene assets: World or Renderer not initialized");
        return;
    }

    VKMON_INFO("Preloading all scene assets during initialization...");

    // Get all entities with Renderable components
    auto entities = world_->getEntityManager().getEntitiesWithComponent<Renderable>();

    if (entities.empty()) {
        VKMON_INFO("No renderable entities found - no assets to preload");
        return;
    }

    // Collect unique mesh paths to avoid duplicate loading
    std::set<std::string> uniqueMeshPaths;
    int totalRenderables = 0;

    for (EntityID entity : entities) {
        auto& renderable = world_->getComponent<Renderable>(entity);
        uniqueMeshPaths.insert(renderable.meshPath);
        totalRenderables++;
    }

    VKMON_INFO("Found " + std::to_string(totalRenderables) + " renderable entities using " +
               std::to_string(uniqueMeshPaths.size()) + " unique models");

    // Preload each unique model
    int successCount = 0;
    int failureCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    for (const std::string& meshPath : uniqueMeshPaths) {
        if (renderer_->preloadModel(meshPath)) {
            successCount++;
        } else {
            failureCount++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    VKMON_INFO("Scene asset preloading complete: " +
               std::to_string(successCount) + " models loaded successfully, " +
               std::to_string(failureCount) + " failures, " +
               std::to_string(totalTimeMs) + "ms total");

    // Also preload physics bodies to prevent first-frame delay
    if (physicsSystem_) {
        physicsSystem_->preloadPhysicsBodies(world_->getEntityManager());
    }
}
