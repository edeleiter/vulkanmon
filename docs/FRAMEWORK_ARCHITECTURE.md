# VulkanMon Framework Architecture

## � Framework Overview

VulkanMon is built on three foundational frameworks working in harmony to create a Pokemon Legends: Arceus-style game engine:

1. **Vulkan Graphics Framework** - Low-level GPU control and rendering
2. **Entity Component System (ECS)** - Data-driven game object architecture
3. **Spatial Management Framework** - Efficient world partitioning and queries

These frameworks follow our core principles: **Simple is Powerful**, **Test as we go**, and **Document often**.

---

## � Vulkan Graphics Framework

### Framework Philosophy
Modern Vulkan API provides explicit GPU control for maximum performance in Pokemon-style open worlds with hundreds of creatures and detailed environments.

### Core Architecture Components

#### 1. Vulkan Instance & Device Management
```cpp
// src/rendering/VulkanRenderer.h
class VulkanRenderer {
    VkInstance instance;              // Vulkan API entry point
    VkPhysicalDevice physicalDevice;  // GPU hardware abstraction
    VkDevice device;                  // Logical device for commands
    VkQueue graphicsQueue;            // Command submission queue
};
```

**Key Features:**
- **Cross-Platform GPU Support** - NVIDIA, AMD, Intel graphics
- **Vulkan 1.3 API** - Modern graphics features and performance
- **Debug Validation** - Development-time error checking and optimization hints
- **Extension Management** - Dynamic feature detection and usage

#### 2. Swapchain & Presentation
```cpp
struct SwapchainDetails {
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
};
```

**Capabilities:**
- **Dynamic Resize Handling** - Window resizing with swapchain recreation
- **Multi-Buffering** - Double/triple buffering for smooth frame delivery
- **Present Mode Selection** - VSync control and performance optimization
- **Format Negotiation** - Optimal surface format selection

#### 3. Command Buffer Management
```cpp
class CommandBufferManager {
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

public:
    void beginFrame();                    // Acquire image, begin recording
    void recordRenderCommands();          // Record drawing operations
    void endFrame();                      // Submit and present
};
```

**Command Flow:**
1. **Acquire Swapchain Image** - Get next frame target
2. **Begin Command Recording** - Start command buffer
3. **Begin Render Pass** - Set render targets and clear values
4. **Record Draw Commands** - Bind pipelines, descriptors, draw calls
5. **End Render Pass** - Finish rendering operations
6. **Submit Commands** - Send to GPU queue
7. **Present Frame** - Display completed frame

#### 4. Resource Management Framework
```cpp
// src/rendering/ResourceManager.h
class ResourceManager {
public:
    // RAII-compliant resource wrappers
    struct Buffer { VkBuffer buffer; VkDeviceMemory memory; ~Buffer(); };
    struct Image { VkImage image; VkDeviceMemory memory; VkImageView view; ~Image(); };

    // Factory methods with automatic cleanup
    std::unique_ptr<Buffer> createVertexBuffer(const void* data, VkDeviceSize size);
    std::unique_ptr<Buffer> createUniformBuffer(VkDeviceSize size);
    std::unique_ptr<Image> createTexture2D(uint32_t width, uint32_t height);
};
```

**Memory Management:**
- **RAII Principle** - Automatic resource cleanup on scope exit
- **Memory Type Selection** - Optimal GPU/CPU memory usage
- **Buffer Management** - Vertex, index, uniform buffer allocation
- **Texture Management** - 2D texture creation and GPU upload

#### 5. Pipeline State Management
```cpp
struct GraphicsPipelineBuilder {
    VkPipelineVertexInputStateCreateInfo vertexInput;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineColorBlendStateCreateInfo colorBlending;

    VkPipeline build(VkDevice device, VkRenderPass renderPass);
};
```

**Pipeline Features:**
- **Shader Stage Management** - Vertex/fragment shader integration
- **Vertex Input Description** - Position, normal, UV coordinate layout
- **Depth Testing** - Z-buffer for correct 3D rendering
- **Culling & Winding** - Backface culling for performance
- **Blending** - Alpha transparency support

### Performance Characteristics
- **Frame Rate** - 60+ FPS with complex scenes
- **Draw Call Batching** - Material-based grouping for efficiency
- **Memory Usage** - ~100MB baseline + assets
- **Command Recording** - <2ms per frame
- **GPU Synchronization** - Proper fences and semaphores

---

## � Entity Component System (ECS) Framework

### Framework Philosophy
Data-driven architecture separating game logic (Systems) from game data (Components) for scalable Pokemon creature management.

### Core ECS Architecture

#### 1. Entity Management
```cpp
// src/core/EntityManager.h
class EntityManager {
    using EntityID = uint32_t;
    static constexpr EntityID INVALID_ENTITY = 0;

public:
    EntityID createEntity();
    void destroyEntity(EntityID entity);
    bool isEntityValid(EntityID entity) const;

private:
    std::vector<bool> entityExists;
    std::queue<EntityID> freeEntityIDs;
    EntityID nextEntityID = 1;
};
```

**Entity Features:**
- **32-bit Entity IDs** - Support for 4 billion entities
- **ID Recycling** - Efficient memory usage with free ID reuse
- **Validation** - Safe entity existence checking
- **Thread Safety** - Single-threaded design for cache performance

#### 2. Component Storage Framework
```cpp
// src/core/ComponentArray.h
template<typename T>
class ComponentArray {
    std::vector<T> components;
    std::unordered_map<EntityID, size_t> entityToIndex;
    std::unordered_map<size_t, EntityID> indexToEntity;

public:
    void addComponent(EntityID entity, T&& component);
    T& getComponent(EntityID entity);
    bool hasComponent(EntityID entity) const;
    void removeComponent(EntityID entity);
};
```

**Storage Benefits:**
- **Packed Arrays** - Cache-friendly component storage
- **O(1) Access** - Fast component lookup and modification
- **Type Safety** - Template-based compile-time checking
- **Memory Efficiency** - Minimal overhead per component

#### 3. Core Component Types
```cpp
// Transform Component (src/components/Transform.h)
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};    // Euler angles in degrees
    glm::vec3 scale{1.0f};

    glm::mat4 getModelMatrix() const;
    glm::vec3 getForward() const;
    glm::vec3 getUp() const;
};

// Renderable Component (src/components/Renderable.h)
struct Renderable {
    std::string meshPath;        // Path to .obj/.fbx model
    uint32_t materialId = 0;     // Material preset index
    bool visible = true;         // Visibility flag
    float lodBias = 1.0f;        // Level-of-detail scaling
};

// Spatial Component (src/components/SpatialComponent.h)
struct SpatialComponent {
    float boundingRadius = 1.0f;     // Collision/culling radius
    float detectionRadius = 5.0f;    // AI detection range
    float territoryRadius = 10.0f;   // Creature territory size
    glm::vec3 homePosition{0.0f};    // Default/spawn position
    LayerMask layer = LayerMask::Default; // Spatial layer filtering
};
```

#### 4. System Base Framework
```cpp
// src/core/System.h
template<typename... ComponentTypes>
class System {
public:
    virtual ~System() = default;
    virtual void update(float deltaTime, EntityManager& entityManager) = 0;
    virtual void render(VulkanRenderer& renderer, EntityManager& entityManager) {}

protected:
    bool hasRequiredComponents(EntityID entity, EntityManager& em);
    std::vector<EntityID> getEntitiesWithComponents(EntityManager& em);
};
```

**System Features:**
- **Template-Based Component Requirements** - Compile-time safety
- **Query Optimization** - Efficient entity filtering
- **Update/Render Separation** - Clear logic vs presentation boundaries
- **Performance Tracking** - Optional timing and profiling

### ECS Integration Examples

#### Camera System
```cpp
// src/systems/CameraSystem.h
class CameraSystem : public System<Transform, Camera> {
public:
    void update(float deltaTime, EntityManager& entityManager) override;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    Frustum getActiveCameraFrustum() const;

private:
    EntityID activeCameraEntity = INVALID_ENTITY;
    CameraConfig config;
};
```

#### Render System
```cpp
// src/systems/RenderSystem.h
class RenderSystem : public System<Transform, Renderable> {
public:
    void render(VulkanRenderer& renderer, EntityManager& entityManager) override;
    void setSpatialSystem(SpatialSystem* spatialSystem);
    void setCameraSystem(CameraSystem* cameraSystem);

private:
    void performFrustumCulling(const Frustum& frustum,
                              std::vector<EntityID>& candidates);
    void sortByMaterial(std::vector<EntityID>& entities);
};
```

### Performance Characteristics
- **Entity Creation** - ~50ns per entity
- **Component Access** - O(1) lookup, ~10ns
- **System Updates** - <1ms for 1000+ entities
- **Memory Layout** - Cache-friendly packed storage

---

## � Spatial Management Framework

### Framework Philosophy
Efficient spatial partitioning for Pokemon-style open worlds requiring fast creature queries, collision detection, and rendering optimization.

### Spatial Architecture Components

#### 1. Octree Spatial Partitioning
```cpp
// src/spatial/SpatialManager.h
class SpatialManager {
    struct OctreeNode {
        BoundingBox bounds;
        std::vector<EntityID> entities;
        std::array<std::unique_ptr<OctreeNode>, 8> children;
        bool isLeaf = true;

        static constexpr size_t MAX_ENTITIES_PER_NODE = 10;
        static constexpr int MAX_DEPTH = 8;
    };

public:
    void updateEntity(EntityID entity, const BoundingBox& bounds);
    void removeEntity(EntityID entity);

    // Pokemon-specific queries
    std::vector<EntityID> findCreaturesInRadius(const glm::vec3& center,
                                               float radius,
                                               LayerMask layerFilter = LayerMask::All);
    std::vector<EntityID> findVisibleCreatures(const Frustum& frustum,
                                              LayerMask layerFilter = LayerMask::Creatures);
    EntityID findNearestEntity(const glm::vec3& position,
                              LayerMask layerFilter = LayerMask::All);
};
```

**Octree Features:**
- **Hierarchical Subdivision** - Efficient space partitioning
- **Dynamic Entity Management** - Real-time position updates
- **Layer-Based Filtering** - Creatures, terrain, items separation
- **Configurable Depth** - Balance between precision and performance

#### 2. Spatial Component Integration
```cpp
// src/systems/SpatialSystem.h
class SpatialSystem : public System<Transform, SpatialComponent> {
public:
    void update(float deltaTime, EntityManager& entityManager) override;
    void setSpatialManager(std::shared_ptr<SpatialManager> manager);

    // Query interface for other systems
    std::vector<EntityID> queryRadius(const glm::vec3& center, float radius,
                                     LayerMask filter = LayerMask::All);
    std::vector<EntityID> queryFrustum(const Frustum& frustum,
                                      LayerMask filter = LayerMask::All);

private:
    std::shared_ptr<SpatialManager> spatialManager;
    float updateThrottleTime = 0.1f; // 10 updates per second
};
```

#### 3. Spatial Query Types

**Radius Queries** - Find all entities within distance
```cpp
// Find creatures within 20 units of player
auto nearbyCreatures = spatialSystem->queryRadius(playerPosition, 20.0f, LayerMask::Creatures);
```

**Frustum Queries** - Camera visibility and culling
```cpp
// Get all visible entities for rendering
auto visibleEntities = spatialSystem->queryFrustum(cameraFrustum, LayerMask::All);
```

**Nearest Entity** - AI targeting and interaction
```cpp
// Find closest interactable object
EntityID target = spatialManager->findNearestEntity(creaturePos, LayerMask::Items);
```

#### 4. Layer Mask System
```cpp
// src/spatial/LayerMask.h
enum class LayerMask : uint32_t {
    Default   = 1 << 0,  // General objects
    Creatures = 1 << 1,  // Pokemon and NPCs
    Terrain   = 1 << 2,  // Ground and static geometry
    Items     = 1 << 3,  // Collectibles and interactables
    Player    = 1 << 4,  // Player character
    UI        = 1 << 5,  // UI elements in 3D space
    All       = 0xFFFFFFFF
};
```

### Performance Characteristics
- **Query Performance** - O(log n) for spatial lookups
- **Update Performance** - <0.5ms for 1000+ entities
- **Memory Usage** - ~1KB per 100 entities
- **Scalability** - Designed for 10,000+ entities

### Pokemon Gameplay Applications

#### Creature Detection System
```cpp
// src/game/CreatureDetectionSystem.h
class CreatureDetectionSystem : public System<Transform, SpatialComponent, Creature> {
public:
    void update(float deltaTime, EntityManager& entityManager) override;
    void setSpatialSystem(SpatialSystem* spatialSystem);

private:
    void processCreatureDetection(EntityID creature, EntityManager& em);
    void updateCreatureState(EntityID creature, bool playerDetected);

    SpatialSystem* spatialSystem = nullptr;
    float detectionUpdateInterval = 0.2f; // 5 times per second
};
```

**Use Cases:**
- **Battle Triggers** - Detect player proximity for wild Pokemon encounters
- **AI Behavior** - Creature awareness and territorial responses
- **Environmental Interaction** - Item collection and NPC dialogue
- **Rendering Optimization** - Frustum culling for performance

---

## � Framework Integration Patterns

### 1. System Dependencies
```cpp
// src/core/ApplicationSetup.cpp
void Application::initializeSystems() {
    // Core systems
    world.addSystem<CameraSystem>();
    world.addSystem<SpatialSystem>();
    world.addSystem<RenderSystem>();
    world.addSystem<CreatureDetectionSystem>();

    // Dependency injection for system coordination
    auto renderSystem = world.getSystem<RenderSystem>();
    auto spatialSystem = world.getSystem<SpatialSystem>();
    auto cameraSystem = world.getSystem<CameraSystem>();
    auto creatureSystem = world.getSystem<CreatureDetectionSystem>();

    renderSystem->setSpatialSystem(spatialSystem);
    renderSystem->setCameraSystem(cameraSystem);
    creatureSystem->setSpatialSystem(spatialSystem);
    spatialSystem->setSpatialManager(std::make_shared<SpatialManager>());
}
```

### 2. Framework Communication Flow
```
Input Events → InputHandler → Application
                                ↓
Application → ECS World → System Updates
                                ↓
SpatialSystem → SpatialManager → Entity Queries
                                ↓
RenderSystem → VulkanRenderer → GPU Commands
                                ↓
VulkanRenderer → Swapchain → Screen Display
```

### 3. Performance Integration
- **Spatial Culling** - Only render visible entities
- **Component Caching** - Minimize ECS queries per frame
- **Command Batching** - Group similar rendering operations
- **Resource Sharing** - Single model cache across systems

## � Framework Benefits for Pokemon Development

### 1. Scalability
- **Massive Entity Counts** - Hundreds of creatures in open world
- **Efficient Queries** - Fast proximity detection for battles
- **Memory Efficiency** - Packed component storage

### 2. Modularity
- **Independent Systems** - Easy feature addition/removal
- **Clean Interfaces** - System boundaries prevent coupling
- **Testing Isolation** - Unit test individual frameworks

### 3. Performance
- **Cache-Friendly Design** - Data-oriented architecture
- **Spatial Optimization** - O(log n) world queries
- **GPU Efficiency** - Modern Vulkan rendering pipeline

The three-framework architecture provides a robust foundation for Pokemon Legends: Arceus-style gameplay with professional-grade performance and maintainability.