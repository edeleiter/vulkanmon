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
    // Position camera to view the 8x8 creature grid (spans -10.5 to +10.5)
    glm::vec3 cameraPos(0.0f, 15.0f, 25.0f);   // Elevated and back from grid
    glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);  // Look at grid center
    glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);      // Standard up vector

    camera_ = std::make_shared<::Camera>(cameraPos, cameraTarget, cameraUp);
    VKMON_INFO("Camera positioned to view creature grid at " +
               std::to_string(cameraPos.x) + ", " +
               std::to_string(cameraPos.y) + ", " +
               std::to_string(cameraPos.z));
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

    VKMON_INFO("Creating GPU Instancing Stress Test Scene...");
    VKMON_INFO("Target: 64+ creatures with multiple mesh types for batching validation");

    // =========================================================================
    // GPU INSTANCING STRESS TEST - Pokemon Legends: Arceus Scale Testing
    // =========================================================================

    const int CREATURE_GRID_SIZE = 8;  // 8x8 = 64 creatures
    const float CREATURE_SPACING = 3.0f;
    const float GRID_OFFSET = -(CREATURE_GRID_SIZE - 1) * CREATURE_SPACING * 0.5f;

    // Create 4 different creature types for batching efficiency testing
    const std::vector<std::string> creatureMeshes = {"cube.obj", "sphere.obj", "pyramid.obj", "plane.obj"};
    const std::vector<uint32_t> creatureMaterials = {0, 1, 2, 3};  // Different materials per type

    size_t totalCreatures = 0;

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
            // Add some variation to rotations and scales
            creatureTransform.rotation = glm::vec3(0.0f, (x + z) * 30.0f, 0.0f);
            creatureTransform.scale = glm::vec3(0.6f + (x + z) * 0.05f);
            world_->addComponent(creature, creatureTransform);

            // Creature type based on position (creates natural batching)
            int creatureType = (x + z) % creatureMeshes.size();

            Renderable creatureRenderable;
            creatureRenderable.meshPath = creatureMeshes[creatureType];
            creatureRenderable.texturePath = "default";
            creatureRenderable.materialId = creatureMaterials[creatureType];
            creatureRenderable.isVisible = true;
            creatureRenderable.renderLayer = 0;
            world_->addComponent(creature, creatureRenderable);

            // CreatureComponent for GPU instancing system
            CreatureComponent creatureComp;
            creatureComp.state = CreatureState::IDLE;
            creatureComp.detectionRadius = 5.0f;
            creatureComp.type = CreatureComponent::CreatureType::PEACEFUL;
            world_->addComponent(creature, creatureComp);

            // SpatialComponent for culling
            SpatialComponent creatureSpatial;
            creatureSpatial.spatialLayers = LayerMask::Creatures;
            creatureSpatial.boundingRadius = 0.8f;
            creatureSpatial.behavior = SpatialBehavior::STATIC;  // Static for stress test
            world_->addComponent(creature, creatureSpatial);

            totalCreatures++;
        }
    }

    VKMON_INFO("GPU Instancing Stress Test: Created " + std::to_string(totalCreatures) + " creatures");
    VKMON_INFO("Creature Types: " + std::to_string(creatureMeshes.size()) + " different meshes");
    VKMON_INFO("Expected Batches: " + std::to_string(creatureMeshes.size()) + " (one per mesh type)");
    VKMON_INFO("Expected Efficiency: " + std::to_string(totalCreatures / creatureMeshes.size()) + " creatures per draw call");

    // Create a simple camera for navigation (optional reference object)
    EntityID cameraEntity = world_->createEntity();
    Transform cameraTransform;
    cameraTransform.position = glm::vec3(0.0f, 8.0f, 15.0f);  // Elevated view of the grid center
    cameraTransform.rotation = glm::vec3(-25.0f, 0.0f, 0.0f); // Look down at the grid
    cameraTransform.scale = glm::vec3(1.0f);
    world_->addComponent(cameraEntity, cameraTransform);

    Camera cameraComponent;
    cameraComponent.fov = 45.0f;
    cameraComponent.nearPlane = 0.1f;
    cameraComponent.farPlane = 100.0f;
    cameraComponent.isActive = true;
    world_->addComponent(cameraEntity, cameraComponent);

    SpatialComponent cameraSpatial;
    cameraSpatial.spatialLayers = LayerMask::Camera;
    cameraSpatial.boundingRadius = 0.1f;
    cameraSpatial.behavior = SpatialBehavior::DYNAMIC;
    world_->addComponent(cameraEntity, cameraSpatial);

    VKMON_INFO("GPU Instancing Stress Test Scene Complete!");
    VKMON_INFO("Performance Target: 60+ FPS with " + std::to_string(totalCreatures) + " creatures");
    VKMON_INFO("Batch Efficiency Target: 16 creatures per draw call");
    VKMON_INFO("Ready for Pokemon Legends: Arceus scale testing!");
}
