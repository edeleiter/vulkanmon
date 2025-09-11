# VulkanMon New Architecture Design

## Overview

This document defines the complete architecture for VulkanMon Phase 5.2, transforming the monolithic `HelloTriangleApp` into a modular, extensible game engine architecture suitable for Pokemon-style 3D creature collector games.

## Design Principles

### Core Philosophy Alignment
- **"Simple is Powerful"**: Each class has one clear responsibility
- **"Test, Test, Test"**: Every class is unit testable in isolation
- **"Document Often"**: Comprehensive interface documentation

### Architectural Principles
1. **Single Responsibility**: Each class handles one concern
2. **Dependency Injection**: Clear dependency relationships
3. **RAII Resource Management**: All resources properly managed
4. **Interface Segregation**: Clean, minimal public APIs
5. **Open/Closed**: Extensible for future Pokemon-style features

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    VulkanMonApplication                     │
│                 (Application Coordinator)                  │
└─────────────────┬───────────────────────────────────────────┘
                  │
    ┌─────────────┼─────────────┐
    │             │             │
    ▼             ▼             ▼
┌─────────┐   ┌─────────┐   ┌─────────┐
│ Window  │   │ Vulkan  │   │  Input  │
│         │   │Renderer │   │ Handler │
└─────────┘   └─────────┘   └─────────┘
                  │             │
                  ▼             ▼
              ┌─────────┐   ┌─────────┐
              │  Scene  │   │ Camera  │
              │         │   │         │
              └─────────┘   └─────────┘
                  │
                  ▼
        ┌─────────────────────────┐
        │    Engine Systems       │
        │ ┌─────────────────────┐ │
        │ │ ResourceManager     │ │
        │ │ AssetManager        │ │
        │ │ ModelLoader         │ │
        │ │ LightingSystem      │ │
        │ │ MaterialSystem      │ │
        │ │ Logger              │ │
        │ └─────────────────────┘ │
        └─────────────────────────┘
```

## Class Definitions

### 1. VulkanMonApplication

**Responsibility**: Application lifecycle, system coordination, main loop management

```cpp
namespace VulkanMon {

class VulkanMonApplication {
public:
    VulkanMonApplication();
    ~VulkanMonApplication();
    
    // Application lifecycle
    void run();
    
    // Move-only semantics
    VulkanMonApplication(const VulkanMonApplication&) = delete;
    VulkanMonApplication& operator=(const VulkanMonApplication&) = delete;
    VulkanMonApplication(VulkanMonApplication&&) = default;
    VulkanMonApplication& operator=(VulkanMonApplication&&) = default;

private:
    // Core systems (in order of dependency)
    std::unique_ptr<Window> window_;
    std::unique_ptr<VulkanRenderer> renderer_;
    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<InputHandler> inputHandler_;
    
    // Application state
    bool running_ = false;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;
    float deltaTime_ = 0.0f;
    
    // Lifecycle methods
    void initialize();
    void mainLoop();
    void cleanup();
    
    // Setup phases
    void initializeSystems();
    void setupInputCallbacks();
    void loadInitialScene();
    
    // Main loop components
    void updateDeltaTime();
    void processInput();
    void update();
    void render();
    
    // Error handling
    void handleRenderError(const std::exception& e);
};

} // namespace VulkanMon
```

**Key Design Decisions**:
- Owns all major systems as unique_ptr for clear lifetime management
- Initialization order carefully planned (Window → Renderer → Scene → Camera → InputHandler)
- Main loop separated into clear phases: input → update → render
- Error handling isolated to prevent crashes

### 2. VulkanRenderer

**Responsibility**: All Vulkan operations, graphics pipeline, rendering commands

```cpp
namespace VulkanMon {

class VulkanRenderer {
public:
    VulkanRenderer(Window& window);
    ~VulkanRenderer();
    
    // Move-only semantics
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = default;
    VulkanRenderer& operator=(VulkanRenderer&&) = default;
    
    // Core rendering lifecycle
    void initialize();
    void render(const Scene& scene, const Camera& camera);
    void waitIdle();
    void handleWindowResize();
    
    // Pipeline management
    void reloadShaders();
    void recreateSwapchain();
    
    // Resource access (for other systems)
    std::shared_ptr<ResourceManager> getResourceManager() { return resourceManager_; }
    std::shared_ptr<LightingSystem> getLightingSystem() { return lightingSystem_; }
    std::shared_ptr<MaterialSystem> getMaterialSystem() { return materialSystem_; }
    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }

private:
    // Window reference (not owned)
    Window& window_;
    
    // Vulkan core objects
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    
    // Swapchain and presentation
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_;
    
    // Rendering pipeline
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkShaderModule vertShaderModule_ = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule_ = VK_NULL_HANDLE;
    
    // Command recording
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    
    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    std::vector<VkFence> imagesInFlight_;
    size_t currentFrame_ = 0;
    
    // Depth testing
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    
    // Descriptor management
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBuffersMemory_;
    
    // Engine systems
    std::shared_ptr<ResourceManager> resourceManager_;
    std::shared_ptr<AssetManager> assetManager_;
    std::shared_ptr<LightingSystem> lightingSystem_;
    std::shared_ptr<MaterialSystem> materialSystem_;
    
    // Initialization phases
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createDepthResources();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    void initializeEngineSystems();
    
    // Rendering methods
    void drawFrame(const Scene& scene, const Camera& camera);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, 
                           const Scene& scene, const Camera& camera);
    void updateUniformBuffer(uint32_t currentImage, const Camera& camera);
    
    // Resource management
    void cleanupSwapchain();
    void recreateSwapchainInternal();
    
    // Utility methods
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                VkImageTiling tiling, VkFormatFeatureFlags features);
};

} // namespace VulkanMon
```

**Key Design Decisions**:
- Contains ALL Vulkan state and operations
- Provides clean interface for rendering a Scene with a Camera
- Engine systems (ResourceManager, etc.) are owned and managed here
- Shader reloading and swapchain recreation are isolated operations
- Clear separation between initialization and runtime operations

### 3. Window

**Responsibility**: GLFW window management, surface creation, event routing

```cpp
namespace VulkanMon {

class Window {
public:
    // Callback type definitions
    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseCallback = std::function<void(double xpos, double ypos)>;
    using ResizeCallback = std::function<void(int width, int height)>;

    Window(uint32_t width, uint32_t height, const std::string& title);
    ~Window();
    
    // Move-only semantics
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = default;
    Window& operator=(Window&&) = default;
    
    // Core window lifecycle
    void initialize();
    void createSurface(VkInstance instance);
    bool shouldClose() const;
    void pollEvents();
    void cleanup();
    
    // Callback registration
    void setKeyCallback(KeyCallback callback);
    void setMouseCallback(MouseCallback callback);
    void setResizeCallback(ResizeCallback callback);
    
    // State queries
    GLFWwindow* getWindow() const { return window_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    VkExtent2D getExtent() const;
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    bool wasResized() const { return resized_; }
    void resetResizeFlag() { resized_ = false; }

private:
    // Window state
    GLFWwindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    uint32_t width_, height_;
    std::string title_;
    bool resized_ = false;
    
    // Callbacks
    KeyCallback keyCallback_;
    MouseCallback mouseCallback_;
    ResizeCallback resizeCallback_;
    
    // Static GLFW callback bridges
    static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfwMouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void glfwResizeCallback(GLFWwindow* window, int width, int height);
};

} // namespace VulkanMon
```

**Key Design Decisions**:
- Encapsulates all GLFW operations
- Provides type-safe callback registration
- Surface creation requires VkInstance (dependency on renderer)
- Window resize state management for swapchain recreation
- Clear separation between window and graphics concerns

### 4. InputHandler

**Responsibility**: User input processing, system control commands

```cpp
namespace VulkanMon {

class InputHandler {
public:
    // Callback types for actions InputHandler cannot perform directly
    using ShaderReloadCallback = std::function<void()>;

    InputHandler(std::shared_ptr<Camera> camera,
                std::shared_ptr<LightingSystem> lightingSystem,
                std::shared_ptr<MaterialSystem> materialSystem);
    
    // Input processing
    void processKeyInput(int key, int scancode, int action, int mods);
    void processMouseInput(double xpos, double ypos);
    void processContinuousInput(GLFWwindow* window, float deltaTime);
    
    // Callback registration
    void setShaderReloadCallback(ShaderReloadCallback callback);
    
    // State management
    void resetMousePosition();
    void setMouseSensitivity(float sensitivity);
    
    // Input configuration
    void setCameraSpeed(float speed);
    void setMouseLocked(bool locked);

private:
    // System references
    std::shared_ptr<Camera> camera_;
    std::shared_ptr<LightingSystem> lightingSystem_;
    std::shared_ptr<MaterialSystem> materialSystem_;
    
    // Input state
    bool firstMouse_ = true;
    float lastMouseX_ = 400.0f;
    float lastMouseY_ = 300.0f;
    float mouseSensitivity_ = 0.1f;
    float cameraSpeed_ = 2.5f;
    bool mouseLocked_ = true;
    
    // Callbacks
    ShaderReloadCallback shaderReloadCallback_;
    
    // Input processing helpers
    void handleCameraMovement(GLFWwindow* window, float deltaTime);
    void handleSystemControls(int key);
    void handleLightingControls(int key);
    void handleMaterialControls(int key);
    
    // Debug and feedback
    void logInputAction(const std::string& action);
};

} // namespace VulkanMon
```

**Key Design Decisions**:
- References systems it controls (Camera, LightingSystem, MaterialSystem)
- Cannot directly reload shaders (belongs to renderer), uses callback
- Handles both discrete (key press) and continuous (WASD) input
- Configuration methods for different input preferences
- Clear separation between input processing and action execution

### 5. Scene

**Responsibility**: World state, model management, rendering coordination

```cpp
namespace VulkanMon {

class Scene {
public:
    Scene(std::shared_ptr<ModelLoader> modelLoader);
    ~Scene() = default;
    
    // Move-only semantics  
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;
    
    // Scene management
    void initialize();
    void loadTestScene();
    void clear();
    
    // Model management
    size_t addModel(std::unique_ptr<Model> model, 
                   const glm::mat4& transform = glm::mat4(1.0f));
    void removeModel(size_t modelId);
    void updateModelTransform(size_t modelId, const glm::mat4& transform);
    
    // Rendering
    void render(VkCommandBuffer commandBuffer, 
               const glm::mat4& viewMatrix, 
               const glm::mat4& projMatrix) const;
    
    // Scene queries
    size_t getModelCount() const { return models_.size(); }
    size_t getTotalVertices() const;
    size_t getTotalTriangles() const;
    const Model* getModel(size_t modelId) const;
    const glm::mat4& getModelTransform(size_t modelId) const;
    
    // Scene state
    void setWorldTransform(const glm::mat4& transform) { worldTransform_ = transform; }
    const glm::mat4& getWorldTransform() const { return worldTransform_; }
    
    // Future Pokemon-style features
    void setEnvironmentLighting(const glm::vec3& ambient, const glm::vec3& directional);
    void addLight(const glm::vec3& position, const glm::vec3& color, float intensity);

private:
    // Model storage
    std::vector<std::unique_ptr<Model>> models_;
    std::vector<glm::mat4> modelTransforms_;
    std::vector<bool> modelActive_;  // For soft deletion
    
    // Scene state
    glm::mat4 worldTransform_ = glm::mat4(1.0f);
    
    // Systems
    std::shared_ptr<ModelLoader> modelLoader_;
    
    // Resource management
    void cleanupInactiveModels();
    
    // Rendering helpers
    void renderModel(VkCommandBuffer commandBuffer, 
                    const Model& model, 
                    const glm::mat4& modelMatrix,
                    const glm::mat4& viewMatrix,
                    const glm::mat4& projMatrix) const;
    
    // Validation
    bool isValidModelId(size_t modelId) const;
};

} // namespace VulkanMon
```

**Key Design Decisions**:
- Manages multiple models with individual transforms
- Supports soft deletion for efficient memory management
- Provides foundation for Pokemon-style world management
- Clear separation between scene state and rendering commands
- Extensible for future lighting and environment features

## System Integration Patterns

### 1. Dependency Injection Pattern

```cpp
// VulkanMonApplication coordinates system creation and injection
void VulkanMonApplication::initializeSystems() {
    // 1. Create independent systems first
    window_ = std::make_unique<Window>(800, 600, "VulkanMon");
    window_->initialize();
    
    // 2. Create systems that depend on window
    renderer_ = std::make_unique<VulkanRenderer>(*window_);
    renderer_->initialize();
    
    // 3. Create camera (independent)
    camera_ = std::make_unique<Camera>(
        glm::vec3(0.0f, 0.0f, 3.0f),  // position
        glm::vec3(0.0f, 1.0f, 0.0f),  // up
        -90.0f,                        // yaw
        0.0f                          // pitch
    );
    
    // 4. Create scene with model loader from renderer
    scene_ = std::make_unique<Scene>(renderer_->getModelLoader());
    scene_->initialize();
    scene_->loadTestScene();
    
    // 5. Create input handler with system references
    inputHandler_ = std::make_unique<InputHandler>(
        camera_,
        renderer_->getLightingSystem(),
        renderer_->getMaterialSystem()
    );
    
    // 6. Setup cross-system callbacks
    setupInputCallbacks();
}
```

### 2. Event Callback Pattern

```cpp
void VulkanMonApplication::setupInputCallbacks() {
    // Window callbacks route to input handler
    window_->setKeyCallback([this](int key, int scancode, int action, int mods) {
        inputHandler_->processKeyInput(key, scancode, action, mods);
    });
    
    window_->setMouseCallback([this](double xpos, double ypos) {
        inputHandler_->processMouseInput(xpos, ypos);
    });
    
    window_->setResizeCallback([this](int width, int height) {
        renderer_->handleWindowResize();
    });
    
    // Input handler callbacks route to appropriate systems
    inputHandler_->setShaderReloadCallback([this]() {
        renderer_->reloadShaders();
    });
}
```

### 3. Main Loop Pattern

```cpp
void VulkanMonApplication::mainLoop() {
    running_ = true;
    lastFrameTime_ = std::chrono::high_resolution_clock::now();
    
    while (running_ && !window_->shouldClose()) {
        // Timing
        updateDeltaTime();
        
        // Input processing  
        window_->pollEvents();
        inputHandler_->processContinuousInput(window_->getWindow(), deltaTime_);
        
        // Game logic update
        update();
        
        // Rendering
        try {
            renderer_->render(*scene_, *camera_);
        } catch (const std::exception& e) {
            handleRenderError(e);
        }
        
        // Window resize handling
        if (window_->wasResized()) {
            renderer_->handleWindowResize();
            window_->resetResizeFlag();
        }
    }
    
    renderer_->waitIdle();
}
```

## Testing Strategy

### Unit Testing Approach

Each class is designed for isolated testing:

```cpp
// Window testing (mock GLFW)
TEST_CASE("Window creates and initializes correctly") {
    Window window(800, 600, "Test");
    // Test window creation, callback registration, etc.
}

// InputHandler testing (mock systems)
TEST_CASE("InputHandler processes camera input correctly") {
    auto mockCamera = std::make_shared<MockCamera>();
    InputHandler handler(mockCamera, nullptr, nullptr);
    
    // Test key processing, mouse movement, etc.
}

// Scene testing (real models)
TEST_CASE("Scene manages models correctly") {
    auto mockLoader = std::make_shared<MockModelLoader>();
    Scene scene(mockLoader);
    
    // Test model addition, removal, transformation
}

// VulkanRenderer testing (integration tests)
TEST_CASE("VulkanRenderer initializes without errors") {
    // Requires actual Vulkan context for meaningful testing
}
```

### Integration Testing

```cpp
TEST_CASE("Complete application lifecycle") {
    VulkanMonApplication app;
    
    // Test initialization
    REQUIRE_NOTHROW(app.initialize());
    
    // Test single frame render
    app.renderSingleFrame();
    
    // Test cleanup
    REQUIRE_NOTHROW(app.cleanup());
}
```

## Performance Considerations

### Memory Management
- All Vulkan resources managed through ResourceManager RAII
- Scene uses soft deletion to avoid frequent allocations
- Uniform buffers allocated once, updated in-place
- Descriptor sets created once, reused throughout lifecycle

### Rendering Efficiency
- Command buffers recorded once per swapchain recreation
- Uniform buffer updates only when camera or scene changes
- Pipeline state changes minimized through good organization
- Descriptor set updates batched when possible

### Future Optimization Points
- Frustum culling in Scene::render()
- Level-of-detail system for distant models
- Instanced rendering for repeated objects (trees, grass, etc.)
- Multi-threaded command buffer recording

## Extension Points for Pokemon-Style Features

### World Management
```cpp
class World : public Scene {
    // Terrain rendering
    // Multiple biomes
    // Weather systems
    // Day/night cycle
};
```

### Creature System
```cpp
class Creature {
    // 3D model with animations
    // Battle stats
    // AI behavior
    // Particle effects
};
```

### Battle System
```cpp
class BattleRenderer : public VulkanRenderer {
    // Battle arena rendering
    // Attack animations
    // UI overlay
    // Effect systems
};
```

## Conclusion

This architecture provides a solid foundation for Pokemon-style game development while maintaining our core principles of simplicity, testability, and clear documentation. Each class has a single, well-defined responsibility and clean interfaces for interaction.

The modular design enables:
- **Team Development**: Multiple developers can work on different systems
- **Feature Extension**: Easy to add new Pokemon-style features
- **Performance Optimization**: Targeted optimization of specific systems
- **Maintenance**: Clear responsibility boundaries reduce bug surface area

**Ready to implement the future of VulkanMon!**