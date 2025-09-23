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
    physicsSystem_->initialize(); // Initialize with default gravity
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

    VKMON_INFO("Creating Pokemon Legends Arceus Scale Test Scene...");
    VKMON_INFO("Target: 125 creatures - 5x5x5 realistic Pokemon overworld test");

    // =========================================================================
    // REALISTIC POKEMON TEST - 3D Cube Formation (5x5x5 = 125 entities)
    // =========================================================================

    const int GRID_SIZE = 5;  // 5x5x5 = 125 creatures - realistic Pokemon overworld scale
    const float CREATURE_SPACING = 2.0f;  // 2.0f spacing for clear separation

    // Use only cubes for consistency during debugging
    const std::string creatureMesh = "cube.obj";
    const std::vector<uint32_t> creatureMaterials = {0, 1, 2, 3};  // Different materials for visual variety

    size_t totalCreatures = 0;

    VKMON_INFO("Creating " + std::to_string(GRID_SIZE * GRID_SIZE * GRID_SIZE) + " creatures in " + std::to_string(GRID_SIZE) + "x" + std::to_string(GRID_SIZE) + "x" + std::to_string(GRID_SIZE) + " cube formation");

    for (int x = 0; x < GRID_SIZE; x++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int z = 0; z < GRID_SIZE; z++) {
                // Entity creation (clean, no per-entity spam)

                EntityID creature = world_->createEntity();

                // Position in 3D cube formation starting from (0,0,0)
                Transform creatureTransform;
                creatureTransform.position = glm::vec3(
                    x * CREATURE_SPACING,  // X: 0, 2, 4, 6, 8, 10, 12, 14, 16, 18
                    y * CREATURE_SPACING,  // Y: 0, 2, 4, 6, 8, 10, 12, 14, 16, 18 (now 3D!)
                    z * CREATURE_SPACING   // Z: 0, 2, 4, 6, 8, 10, 12, 14, 16, 18
                );

                // Varied rotations for visual interest - each cube gets unique rotation
                float rotX = (x * 15.0f) + (y * 5.0f) + (z * 7.0f);   // X-axis rotation based on grid position
                float rotY = (x + y + z) * 12.0f;                     // Y-axis rotation creates spiral pattern
                float rotZ = (x - y + z) * 8.0f;                      // Z-axis rotation adds variety
                creatureTransform.setRotationEuler(rotX, rotY, rotZ);
                creatureTransform.scale = glm::vec3(0.5f);  // Scale down for clear separation
                world_->addComponent(creature, creatureTransform);

                // Material variety for visual distinction - now using 3D coordinates
                int materialType = (x + y + z) % creatureMaterials.size();

                Renderable creatureRenderable;
                creatureRenderable.meshPath = creatureMesh;
                creatureRenderable.texturePath = "default";
                creatureRenderable.materialId = creatureMaterials[materialType];
                creatureRenderable.isVisible = true;
                creatureRenderable.renderLayer = 0;
                creatureRenderable.lodDistance = 1000.0f;
                world_->addComponent(creature, creatureRenderable);

                // CreatureComponent for spatial detection testing
                CreatureComponent creatureComp;
                creatureComp.state = CreatureState::IDLE;
                creatureComp.detectionRadius = 8.0f + (x + y + z) * 0.1f;  // Varied detection radii using 3D coordinates
                creatureComp.type = static_cast<CreatureComponent::CreatureType>((x + y + z) % 3);
                world_->addComponent(creature, creatureComp);

                // SpatialComponent for spatial system testing
                SpatialComponent creatureSpatial;
                creatureSpatial.spatialLayers = LayerMask::Creatures;
                creatureSpatial.boundingRadius = 1.0f + (x + y + z) * 0.05f;
                creatureSpatial.behavior = SpatialBehavior::STATIC;
                creatureSpatial.setHomePosition(creatureTransform.position);
                world_->addComponent(creature, creatureSpatial);

                // Add physics components to a very small subset for controlled testing
                // Only make creatures dynamic if they're at specific positions (much fewer)
                bool isDynamic = (x == 2 && y == 2 && z == 2); // Only center cube starts with physics

                if (isDynamic) {
                    // Add dynamic physics - creature will fall due to gravity
                    RigidBodyComponent rigidBody;
                    rigidBody.isDynamic = true;
                    rigidBody.mass = 1.0f + (x + y + z) * 0.1f; // Varied mass
                    rigidBody.useGravity = true;
                    rigidBody.restitution = 0.3f; // Some bounciness
                    rigidBody.friction = 0.8f;
                    world_->addComponent(creature, rigidBody);

                    // Add collision detection
                    CollisionComponent collision = CollisionComponent::createCreature(0.5f, 1.0f);
                    world_->addComponent(creature, collision);

                    // Add creature-specific physics
                    CreaturePhysicsComponent creaturePhysics = CreaturePhysicsComponent::createLandCreature(3.0f, 1.0f);
                    world_->addComponent(creature, creaturePhysics);
                } else {
                    // Add static collision for non-dynamic creatures
                    CollisionComponent collision = CollisionComponent::createEnvironment(glm::vec3(0.5f, 1.0f, 0.5f));
                    world_->addComponent(creature, collision);
                }

                totalCreatures++;
            }
        }
    }

    VKMON_INFO("Scene setup complete: " + std::to_string(totalCreatures) + " creatures with spatial AI");

    // Create camera positioned to see 1000 creatures in 3D cube (cube spans 0 to 18 units in X, Y, and Z)
    EntityID cameraEntity = world_->createEntity();
    Transform cameraTransform;
    cameraTransform.position = glm::vec3(25.0f, 25.0f, 40.0f);  // Further back and elevated to see entire 3D cube
    cameraTransform.setRotationEuler(-20.0f, -15.0f, 0.0f); // Angled to see the 3D structure clearly
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

    // Scene setup complete - main application will log ready message
}
