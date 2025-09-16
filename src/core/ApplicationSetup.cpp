#include "Application.h"
#include "../utils/Logger.h"
#include "../systems/SpatialSystem.h"
#include "../systems/CreatureRenderSystem.h"
#include "../components/SpatialComponent.h"
#include "../spatial/WorldConfig.h"
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
    assetManager_ = nullptr;     // Will be created by renderer
    modelLoader_ = nullptr;      // Will be created by renderer
    lightingSystem_ = nullptr;   // Will be created by renderer
    materialSystem_ = nullptr;   // Will be created by renderer

    // Create VulkanRenderer with shared dependencies
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

    // Get initialized systems from renderer
    lightingSystem_ = renderer_->getLightingSystem();
    materialSystem_ = renderer_->getMaterialSystem();
    assetManager_ = renderer_->getAssetManager();
}

void Application::initializeInputSystem() {
    // Create InputHandler with camera and window references
    inputHandler_ = std::make_unique<InputHandler>(camera_, window_);

    // Connect input callbacks to window system
    window_->setKeyCallback([this](int key, int scancode, int action, int mods) {
        inputHandler_->processKeyInput(key, scancode, action, mods);
    });

    window_->setMouseCallback([this](double xpos, double ypos) {
        inputHandler_->processMouseInput(xpos, ypos);
    });

    VKMON_INFO("Input callbacks registered with Window system");

    // Set up system control callbacks
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

    // Connect RenderSystem with SpatialSystem for spatial frustum culling
    if (renderSystem_ && spatialSystem_) {
        renderSystem_->setSpatialSystem(spatialSystem_);
        VKMON_INFO("RenderSystem connected to SpatialSystem for frustum culling");
    }

    // Connect CreatureRenderSystem with SpatialSystem for massive creature culling
    if (creatureRenderSystem_ && spatialSystem_) {
        creatureRenderSystem_->setSpatialSystem(spatialSystem_);
        VKMON_INFO("CreatureRenderSystem connected to SpatialSystem for creature culling");
    }

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
    renderable1.meshPath = "cube.obj";  // Original cube - ModelLoader adds assets/models/ prefix
    renderable1.texturePath = "default";
    renderable1.materialId = 0;  // Default material
    renderable1.isVisible = true;
    renderable1.renderLayer = 0;
    world_->addComponent(cube1, renderable1);
    // Add spatial component for spatial system integration
    SpatialComponent spatial1;
    spatial1.boundingRadius = 1.0f;  // Default cube radius
    spatial1.behavior = SpatialBehavior::STATIC;  // Static test objects
    spatial1.spatialLayers = LayerMask::Terrain;  // Terrain object
    spatial1.needsSpatialUpdate = true;  // Initial registration needed
    world_->addComponent(cube1, spatial1);

    // Object 2: Left sphere with different material
    EntityID cube2 = world_->createEntity();
    Transform transform2;
    transform2.position = glm::vec3(-3.0f, 0.0f, 0.0f);
    transform2.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    transform2.scale = glm::vec3(0.8f, 0.8f, 0.8f);
    world_->addComponent(cube2, transform2);

    Renderable renderable2;
    renderable2.meshPath = "sphere.obj";  // Sphere shape
    renderable2.texturePath = "default";
    renderable2.materialId = 1;  // Gold material
    renderable2.isVisible = true;
    renderable2.renderLayer = 0;
    world_->addComponent(cube2, renderable2);
    // Add spatial component
    SpatialComponent spatial2;
    spatial2.boundingRadius = 0.8f;  // Sphere radius
    spatial2.behavior = SpatialBehavior::STATIC;
    spatial2.spatialLayers = LayerMask::Creatures;  // Creature-like object
    spatial2.needsSpatialUpdate = true;
    world_->addComponent(cube2, spatial2);

    // Object 3: Right pyramid with different material
    EntityID cube3 = world_->createEntity();
    Transform transform3;
    transform3.position = glm::vec3(3.0f, 0.0f, 0.0f);
    transform3.rotation = glm::vec3(0.0f, -45.0f, 0.0f);
    transform3.scale = glm::vec3(1.2f, 1.2f, 1.2f);
    world_->addComponent(cube3, transform3);

    Renderable renderable3;
    renderable3.meshPath = "pyramid.obj";  // Pyramid shape
    renderable3.texturePath = "default";
    renderable3.materialId = 2;  // Ruby material
    renderable3.isVisible = true;
    renderable3.renderLayer = 0;
    world_->addComponent(cube3, renderable3);
    SpatialComponent spatial3;
    spatial3.boundingRadius = 1.2f;  // Pyramid radius
    spatial3.behavior = SpatialBehavior::STATIC;
    spatial3.spatialLayers = LayerMask::Buildings;  // Structure object
    spatial3.needsSpatialUpdate = true;
    world_->addComponent(cube3, spatial3);

    // Object 4: Above center cube
    EntityID cube4 = world_->createEntity();
    Transform transform4;
    transform4.position = glm::vec3(0.0f, 2.5f, 0.0f);
    transform4.rotation = glm::vec3(45.0f, 45.0f, 45.0f);
    transform4.scale = glm::vec3(0.6f, 0.6f, 0.6f);
    world_->addComponent(cube4, transform4);

    Renderable renderable4;
    renderable4.meshPath = "sphere.obj";  // Another sphere
    renderable4.texturePath = "default";
    renderable4.materialId = 3;  // Chrome material
    renderable4.isVisible = true;
    renderable4.renderLayer = 0;
    world_->addComponent(cube4, renderable4);
    SpatialComponent spatial4;
    spatial4.boundingRadius = 0.6f;  // Small sphere radius
    spatial4.behavior = SpatialBehavior::STATIC;
    spatial4.spatialLayers = LayerMask::Items;  // Collectible item
    spatial4.needsSpatialUpdate = true;
    world_->addComponent(cube4, spatial4);

    // Object 5: Back cube
    EntityID cube5 = world_->createEntity();
    Transform transform5;
    transform5.position = glm::vec3(0.0f, 0.0f, -4.0f);
    transform5.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    transform5.scale = glm::vec3(1.5f, 0.5f, 1.5f);
    world_->addComponent(cube5, transform5);

    Renderable renderable5;
    renderable5.meshPath = "plane.obj";  // Ground plane
    renderable5.texturePath = "default";
    renderable5.materialId = 4;  // Emerald material
    renderable5.isVisible = true;
    renderable5.renderLayer = 0;
    world_->addComponent(cube5, renderable5);
    SpatialComponent spatial5;
    spatial5.boundingRadius = 1.5f;  // Ground plane radius
    spatial5.behavior = SpatialBehavior::STATIC;
    spatial5.spatialLayers = LayerMask::Grass;  // Environmental ground
    spatial5.needsSpatialUpdate = true;
    world_->addComponent(cube5, spatial5);

    VKMON_DEBUG("Created 5 ECS test entities with diverse shapes:");
    VKMON_DEBUG("  Entity " + std::to_string(cube1) + ": Center cube (Default material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube2) + ": Left sphere (Gold material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube3) + ": Right pyramid (Ruby material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube4) + ": Top sphere (Chrome material)");
    VKMON_DEBUG("  Entity " + std::to_string(cube5) + ": Ground plane (Emerald material)");

    // Create camera entity for spatial-render integration
    EntityID cameraEntity = world_->createEntity();

    // Camera transform (positioned to view the test scene)
    Transform cameraTransform;
    cameraTransform.position = glm::vec3(0.0f, 0.0f, 5.0f);  // Back from center
    cameraTransform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);  // Looking forward
    cameraTransform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    world_->addComponent(cameraEntity, cameraTransform);

    // Camera component with reasonable perspective settings
    Camera cameraComponent;
    cameraComponent.fov = 45.0f;           // Standard FOV
    cameraComponent.nearPlane = 0.1f;      // Close near plane
    cameraComponent.farPlane = 1000.0f;    // Far viewing distance
    cameraComponent.priority = 100;        // High priority (active camera)
    cameraComponent.isActive = true;       // Enable this camera
    world_->addComponent(cameraEntity, cameraComponent);

    VKMON_INFO("Created ECS camera entity " + std::to_string(cameraEntity) + " for spatial culling");

    // =============================================================================
    // PHASE 7.1 TEST: Create 5 identical creatures for instanced rendering test
    // =============================================================================

    VKMON_INFO("Creating Phase 7.1 creature test entities...");

    for (int i = 0; i < 5; ++i) {
        EntityID creature = world_->createEntity();

        // Transform - arrange in a line for easy testing
        Transform creatureTransform;
        creatureTransform.position = glm::vec3(
            -8.0f + i * 4.0f,  // X: spread from -8 to +8
            -1.0f,             // Y: slightly below center objects
            0.0f               // Z: same depth as center objects
        );
        creatureTransform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        creatureTransform.scale = glm::vec3(0.5f, 0.5f, 0.5f);  // Smaller than main objects
        world_->addComponent(creature, creatureTransform);

        // Renderable - all use the same mesh and material for instancing
        Renderable creatureRenderable;
        creatureRenderable.meshPath = "sphere.obj";    // Same mesh for all creatures
        creatureRenderable.texturePath = "default";
        creatureRenderable.materialId = 1;             // Gold material for all creatures
        creatureRenderable.isVisible = true;
        creatureRenderable.renderLayer = 0;
        world_->addComponent(creature, creatureRenderable);

        // CreatureComponent - makes them creatures that CreatureRenderSystem will handle
        CreatureComponent creatureComp;
        creatureComp.state = CreatureState::IDLE;
        creatureComp.detectionRadius = 10.0f;
        creatureComp.type = CreatureComponent::CreatureType::PEACEFUL;
        world_->addComponent(creature, creatureComp);

        // SpatialComponent - for spatial system integration
        SpatialComponent creatureSpatial;
        creatureSpatial.spatialLayers = LayerMask::Creatures;
        creatureSpatial.boundingRadius = 0.5f;  // Match the scale
        world_->addComponent(creature, creatureSpatial);

        VKMON_DEBUG("Created creature " + std::to_string(creature) +
                   " at position (" + std::to_string(creatureTransform.position.x) +
                   ", " + std::to_string(creatureTransform.position.y) +
                   ", " + std::to_string(creatureTransform.position.z) + ")");
    }

    VKMON_INFO("Phase 7.1: Created 5 identical creatures for instanced rendering test");
    VKMON_INFO("Test scene ready - 5 objects + 5 creatures loaded");
}

void Application::loadTestAssets() {
    VKMON_INFO("Loading test assets...");

    // Future: Load additional textures, models, sounds, etc.
    // For now, just placeholder

    VKMON_INFO("Test assets loaded");
}