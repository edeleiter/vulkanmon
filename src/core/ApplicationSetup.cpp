#include "Application.h"
#include "../utils/Logger.h"
#include "../systems/SpatialSystem.h"
#include "../systems/CreatureRenderSystem.h"
#include "../components/SpatialComponent.h"
#include "../components/Camera.h"
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

    // Add spatial system for Pokemon-style spatial queries and management
    WorldConfig worldConfig = WorldConfig::createTestWorld();
    VKMON_INFO("Using world config: " + worldConfig.name +
               " bounds (" + std::to_string(worldConfig.minBounds.x) + "," +
                           std::to_string(worldConfig.minBounds.y) + "," +
                           std::to_string(worldConfig.minBounds.z) + ") to (" +
                           std::to_string(worldConfig.maxBounds.x) + "," +
                           std::to_string(worldConfig.maxBounds.y) + "," +
                           std::to_string(worldConfig.maxBounds.z) + ")");
    spatialSystem_ = world_->addSystem<SpatialSystem>(worldConfig.getBoundingBox());

    // Add CreatureDetectionSystem for AI behavior and spatial detection
    creatureDetectionSystem_ = world_->addSystem<CreatureDetectionSystem>();

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

    VKMON_INFO("Creating Spatial System Stress Test Scene...");
    VKMON_INFO("Target: 200+ creatures for thread-safe spatial system validation");

    // =========================================================================
    // SPATIAL STRESS TEST - Thread-Safe Cache System Validation
    // =========================================================================

    const int CREATURE_GRID_SIZE = 16;  // 16x16 = 256 creatures
    const float CREATURE_SPACING = 3.0f;  // Proper spacing to prevent overlaps
    const float GRID_OFFSET = -(CREATURE_GRID_SIZE - 1) * CREATURE_SPACING * 0.5f;

    // Use only cubes for maximum stress testing and consistency
    const std::string creatureMesh = "cube.obj";  // All creatures use cubes
    const std::vector<uint32_t> creatureMaterials = {0, 1, 2, 3};  // Different materials for visual variety

    size_t totalCreatures = 0;

    VKMON_INFO("Starting creature creation: " + std::to_string(CREATURE_GRID_SIZE) + "x" + std::to_string(CREATURE_GRID_SIZE) + " grid");
    VKMON_INFO("Grid spans: " + std::to_string(GRID_OFFSET) + " to " + std::to_string(-GRID_OFFSET) + " units");

    for (int x = 0; x < CREATURE_GRID_SIZE; x++) {
        for (int z = 0; z < CREATURE_GRID_SIZE; z++) {
            EntityID creature = world_->createEntity();

            // Position in grid formation
            Transform creatureTransform;
            creatureTransform.position = glm::vec3(
                GRID_OFFSET + x * CREATURE_SPACING,  // X: grid spread
                0.0f,                                // Y: ground level
                GRID_OFFSET + z * CREATURE_SPACING   // Z: grid spread
            );
            // Uniform scale and rotation for consistency
            creatureTransform.setRotationEuler(0.0f, 0.0f, 0.0f);
            creatureTransform.scale = glm::vec3(1.0f);  // Standard 1-unit cubes
            world_->addComponent(creature, creatureTransform);

            // Material variety for visual distinction while using single mesh
            int materialType = (x + z) % creatureMaterials.size();

            Renderable creatureRenderable;
            creatureRenderable.meshPath = creatureMesh;  // All cubes for consistency
            creatureRenderable.texturePath = "default";
            creatureRenderable.materialId = creatureMaterials[materialType];
            creatureRenderable.isVisible = true;
            creatureRenderable.renderLayer = 0;
            creatureRenderable.lodDistance = 1000.0f;  // Ensure all creatures are always visible for stress test
            world_->addComponent(creature, creatureRenderable);

            // CreatureComponent for spatial detection testing
            CreatureComponent creatureComp;
            creatureComp.state = CreatureState::WANDERING;  // Active behavior for spatial queries
            creatureComp.detectionRadius = 8.0f + (x + z) * 0.5f;  // Varied detection radii
            creatureComp.type = static_cast<CreatureComponent::CreatureType>((x + z) % 3);  // Mix of types
            world_->addComponent(creature, creatureComp);

            // SpatialComponent for thread-safe spatial system testing
            SpatialComponent creatureSpatial;
            creatureSpatial.spatialLayers = LayerMask::Creatures;
            creatureSpatial.boundingRadius = 1.0f + (x + z) * 0.1f;  // Varied sizes
            creatureSpatial.behavior = SpatialBehavior::DYNAMIC;  // Dynamic for spatial update testing
            world_->addComponent(creature, creatureSpatial);

            totalCreatures++;
        }
    }

    VKMON_INFO("Spatial Stress Test: Created " + std::to_string(totalCreatures) + " creatures");
    VKMON_INFO("Thread-Safe Validation: " + std::to_string(totalCreatures) + " entities with varied detection radii");
    VKMON_INFO("Expected Spatial Queries: ~" + std::to_string(totalCreatures / 10) + " per frame");
    VKMON_INFO("Performance Target: <2ms spatial system overhead per frame");

    // Create camera positioned to see all 256 creatures (grid spans -22.5 to +22.5 = 45 units)
    EntityID cameraEntity = world_->createEntity();
    Transform cameraTransform;
    cameraTransform.position = glm::vec3(0.0f, 35.0f, 50.0f);  // Closer to see 45-unit grid clearly
    cameraTransform.setRotationEuler(-30.0f, 0.0f, 0.0f); // Look down at the grid
    cameraTransform.scale = glm::vec3(1.0f);
    world_->addComponent(cameraEntity, cameraTransform);

    Camera cameraComponent;
    cameraComponent.fov = 75.0f;  // Standard FOV should work for closer camera
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

    VKMON_INFO("Test player entity created at center for creature detection testing");

    VKMON_INFO("Spatial Stress Test Scene Complete!");
    VKMON_INFO("Thread-Safe System Test: " + std::to_string(totalCreatures) + " creatures with active AI");
    VKMON_INFO("Spatial Query Load: ~" + std::to_string(totalCreatures / 10) + " radius queries per frame");
    VKMON_INFO("Ready to validate thread-safe spatial cache performance!");
}
