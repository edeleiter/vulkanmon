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

    // Connect mouse button callback for projectile spawning
    window_->setMouseButtonCallback([this](int button, int action, int mods) {
        if (inputHandler_) {
            inputHandler_->processMouseButtonInput(button, action, mods);
        }
    });

    // Connect projectile spawn callback from InputHandler to ProjectileSystem
    if (projectileSystem_) {
        inputHandler_->setProjectileSpawnCallback([this](double mouseX, double mouseY) {
            if (projectileSystem_ && window_) {
                uint32_t width = window_->getWidth();
                uint32_t height = window_->getHeight();
                projectileSystem_->spawnProjectileFromMouse(
                    static_cast<float>(mouseX),
                    static_cast<float>(mouseY),
                    static_cast<float>(width),
                    static_cast<float>(height),
                    world_->getEntityManager(),
                    ProjectileComponent::Type::PHYSICS_OBJECT
                );
            }
        });
        VKMON_INFO("Projectile spawn callback connected: Mouse clicks will spawn projectiles");
    }

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

    // Add projectile system for mouse-click projectile spawning
    // Note: MaterialSystem will be connected after renderer systems are available
    projectileSystem_ = world_->addSystem<ProjectileSystem>(cameraSystem_, nullptr);
    projectileSystem_->setPhysicsSystem(physicsSystem_);
    VKMON_INFO("ProjectileSystem added to World and connected to physics/camera (MaterialSystem deferred)");

    // MaterialSystem connection will be done after renderer initialization

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

void Application::connectDeferredSystems() {
    VKMON_DEBUG("Connecting deferred systems with renderer resources...");

    // Connect ProjectileSystem to MaterialSystem
    if (projectileSystem_ && materialSystem_) {
        projectileSystem_->setMaterialSystem(materialSystem_.get());
        VKMON_INFO("ProjectileSystem: MaterialSystem connected");
    } else if (projectileSystem_) {
        VKMON_WARNING("ProjectileSystem: MaterialSystem not available for connection");
    }

    VKMON_INFO("Deferred system connections completed");
}

void Application::createProjectileTestScene() {
    if (!world_) {
        VKMON_WARNING("Cannot create projectile test scene: ECS World not initialized");
        return;
    }

    VKMON_INFO("Creating Clean Projectile Test Scene...");
    VKMON_INFO("Focus: Mouse click projectile spawning and physics validation");

    // =========================================================================
    // CLEAN PROJECTILE TEST SCENE
    // =========================================================================

    size_t totalEntities = 0;

    // =========================================================================
    // GROUND PLANE - Simple static collision surface
    // =========================================================================
    VKMON_INFO("Creating simple ground plane...");

    EntityID groundEntity = world_->createEntity();
    Transform groundTransform;
    groundTransform.position = glm::vec3(0.0f, -2.0f, 0.0f);
    groundTransform.setRotationEuler(0.0f, 0.0f, 0.0f);
    groundTransform.scale = glm::vec3(20.0f, 1.0f, 20.0f);
    world_->addComponent(groundEntity, groundTransform);

    Renderable groundRenderable;
    groundRenderable.meshPath = "plane.obj";
    groundRenderable.texturePath = "default";
    groundRenderable.materialId = 0; // Default material
    groundRenderable.isVisible = true;
    world_->addComponent(groundEntity, groundRenderable);

    SpatialComponent groundSpatial;
    groundSpatial.spatialLayers = LayerMask::Environment;
    groundSpatial.boundingRadius = 15.0f;
    groundSpatial.behavior = SpatialBehavior::STATIC;
    world_->addComponent(groundEntity, groundSpatial);

    RigidBodyComponent groundRigidBody = RigidBodyComponent::staticBody();
    world_->addComponent(groundEntity, groundRigidBody);

    CollisionComponent groundCollision = CollisionComponent::box(glm::vec3(20.0f, 1.0f, 20.0f), LayerMask::Environment);
    groundCollision.isStatic = true;
    world_->addComponent(groundEntity, groundCollision);

    totalEntities++;
    VKMON_INFO("Ground plane created at Y=-2.0");

    // =========================================================================
    // TARGET CUBES - Simple targets for projectile testing
    // =========================================================================
    VKMON_INFO("Creating target cubes for projectile testing...");

    // Create 3 target cubes at different distances
    std::vector<glm::vec3> targetPositions = {
        glm::vec3(-5.0f, 2.0f, -10.0f),  // Left target
        glm::vec3(0.0f, 2.0f, -15.0f),   // Center target
        glm::vec3(5.0f, 2.0f, -10.0f)    // Right target
    };

    for (size_t i = 0; i < targetPositions.size(); i++) {
        EntityID targetEntity = world_->createEntity();

        Transform targetTransform;
        targetTransform.position = targetPositions[i];
        targetTransform.setRotationEuler(0.0f, 0.0f, 0.0f);
        targetTransform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
        world_->addComponent(targetEntity, targetTransform);

        Renderable targetRenderable;
        targetRenderable.meshPath = "cube.obj";
        targetRenderable.texturePath = "default";
        targetRenderable.materialId = 1 + static_cast<uint32_t>(i); // Different materials
        targetRenderable.isVisible = true;
        world_->addComponent(targetEntity, targetRenderable);

        SpatialComponent targetSpatial;
        targetSpatial.spatialLayers = LayerMask::Environment;
        targetSpatial.boundingRadius = 1.0f;
        targetSpatial.behavior = SpatialBehavior::STATIC;  // STATIC - targets don't move!
        world_->addComponent(targetEntity, targetSpatial);

        // NO PHYSICS - these are static visual targets, not dynamic physics objects
        // RigidBodyComponent targetRigidBody = RigidBodyComponent::dynamic(1.0f);
        // world_->addComponent(targetEntity, targetRigidBody);
        // CollisionComponent targetCollision = CollisionComponent::box(glm::vec3(1.0f, 1.0f, 1.0f), LayerMask::Environment);
        // world_->addComponent(targetEntity, targetCollision);

        totalEntities++;
    }

    VKMON_INFO("Created 3 target cubes for projectile testing");
    // =========================================================================
    // CAMERA SETUP - Positioned for projectile testing
    // =========================================================================
    VKMON_INFO("Setting up camera for projectile testing...");

    EntityID cameraEntity = world_->createEntity();

    Transform cameraTransform;
    cameraTransform.position = glm::vec3(0.0f, 5.0f, 10.0f);  // Good position to see targets
    cameraTransform.setRotationEuler(-15.0f, 0.0f, 0.0f);      // Look down slightly
    world_->addComponent(cameraEntity, cameraTransform);

    Camera camera;
    camera.fov = 75.0f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 1000.0f;
    camera.priority = 1;
    camera.isActive = true;
    world_->addComponent(cameraEntity, camera);

    SpatialComponent cameraSpatial;
    cameraSpatial.spatialLayers = LayerMask::Camera;
    cameraSpatial.boundingRadius = 1.0f;
    cameraSpatial.behavior = SpatialBehavior::DYNAMIC;
    world_->addComponent(cameraEntity, cameraSpatial);

    totalEntities++;

    VKMON_INFO("Clean Projectile Test Scene Layout:");
    VKMON_INFO("  - Ground plane at Y=-2.0");
    VKMON_INFO("  - 3 target cubes positioned at different distances");
    VKMON_INFO("  - Camera at (0,5,10) looking at targets");
    VKMON_INFO("  - Total entities: " + std::to_string(totalEntities));
    VKMON_INFO("  - Ready for mouse click projectile testing");

    // =========================================================================
    // ENHANCED LIGHTING SETUP - Ensure scene is visible
    // =========================================================================
    VKMON_INFO("Setting up enhanced lighting for projectile test scene...");

    if (lightingSystem_) {
        // Set bright directional light
        lightingSystem_->setDirectionalLight(
            glm::vec3(-0.3f, -1.0f, -0.4f),  // Direction (slightly angled)
            2.0f,                             // High intensity
            glm::vec3(1.0f, 1.0f, 1.0f)      // White light
        );

        // Set bright ambient lighting
        lightingSystem_->setAmbientLight(
            glm::vec3(0.4f, 0.4f, 0.5f),     // Slightly blue ambient
            0.6f                              // High ambient intensity
        );

        VKMON_INFO("Enhanced lighting configured: Directional(2.0x) + Ambient(0.6x)");
    } else {
        VKMON_WARNING("LightingSystem not available - using default lighting");
    }

    // Scene setup complete - physics bodies will be created automatically on first update
    preloadSceneAssets();

    VKMON_INFO("createProjectileTestScene() completing successfully");
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
    size_t successCount = 0;
    size_t failureCount = 0;
    double totalLoadTime = 0.0;

    for (const std::string& meshPath : uniqueMeshPaths) {
        VKMON_INFO("Preloading model: " + meshPath);

        auto start = std::chrono::high_resolution_clock::now();

        if (renderer_->preloadModel(meshPath)) {
            auto end = std::chrono::high_resolution_clock::now();
            double loadTime = std::chrono::duration<double, std::milli>(end - start).count();
            totalLoadTime += loadTime;

            VKMON_INFO("Model preloaded successfully in " + std::to_string(loadTime) + "ms: " + meshPath);
            successCount++;
        } else {
            VKMON_ERROR("Failed to preload model: " + meshPath);
            failureCount++;
        }
    }

    VKMON_INFO("Scene asset preloading complete: " +
               std::to_string(successCount) + " models loaded successfully, " +
               std::to_string(failureCount) + " failures, " +
               std::to_string(totalLoadTime) + "ms total");
}
