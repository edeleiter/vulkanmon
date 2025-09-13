# Class Extraction Strategy: HelloTriangleApp → Modular Architecture

## Overview

This document provides the detailed, step-by-step strategy for extracting functionality from the 1,634-line `HelloTriangleApp` class into focused, single-responsibility classes following our "Simple is Powerful" philosophy.

## Extraction Methodology

### Core Principles
1. **Safety First**: Each extraction maintains identical functionality
2. **Incremental Progress**: Working system at each step
3. **Comprehensive Testing**: Validate before and after each change
4. **Clean Interfaces**: Clear contracts between classes

### Validation Protocol
For each extraction step:
1. **Before**: Screenshot + performance baseline
2. **During**: Incremental testing at each substep  
3. **After**: Full functionality validation + unit tests

## Phase 1: Window Class Extraction

### Target: Simplest, Most Isolated Functionality

#### Current Code Location (src/main.cpp)
```cpp
// Lines 179-190: Window initialization
void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "VulkanMon", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

// Lines 256-262: Surface creation
void createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

// Member variables to extract:
GLFWwindow* window;
VkSurfaceKHR surface;  // Note: Surface creation needs VkInstance
```

#### Step 1.1: Create Window Class Header
**File**: `src/Window.h`
```cpp
#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <string>
#include <functional>

namespace VulkanMon {

/**
 * Window Management System
 * 
 * Handles GLFW window creation, surface management, and event callbacks.
 * Follows "Simple is Powerful" - focused solely on window lifecycle.
 */
class Window {
public:
    // Callback type definitions for clean interface
    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseCallback = std::function<void(double xpos, double ypos)>;
    using ResizeCallback = std::function<void(int width, int height)>;

    Window(uint32_t width, uint32_t height, const std::string& title);
    ~Window();
    
    // Move-only semantics (RAII)
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
    
    // Getters (const interface)
    GLFWwindow* getWindow() const { return window_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    VkExtent2D getExtent() const { return {width_, height_}; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    
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
    
    // Static GLFW callbacks (bridge to member functions)
    static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfwMouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void glfwResizeCallback(GLFWwindow* window, int width, int height);
};

} // namespace VulkanMon
```

#### Step 1.2: Create Window Class Implementation
**File**: `src/Window.cpp`
```cpp
#include "Window.h"
#include "Logger.h"
#include <stdexcept>

namespace VulkanMon {

Window::Window(uint32_t width, uint32_t height, const std::string& title)
    : width_(width), height_(height), title_(title) {
    VKMON_DEBUG("Window created: " + std::to_string(width) + "x" + std::to_string(height));
}

Window::~Window() {
    cleanup();
}

void Window::initialize() {
    VKMON_INFO("Initializing GLFW window system...");
    
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    
    // Configure for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    // Create window
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    
    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);
    
    // Register static callbacks
    glfwSetKeyCallback(window_, glfwKeyCallback);
    glfwSetCursorPosCallback(window_, glfwMouseCallback);
    glfwSetFramebufferSizeCallback(window_, glfwResizeCallback);
    
    // Configure mouse input for 3D camera
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    VKMON_INFO("GLFW window initialized successfully");
}

void Window::createSurface(VkInstance instance) {
    VKMON_INFO("Creating Vulkan surface...");
    
    if (glfwCreateWindowSurface(instance, window_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    
    VKMON_INFO("Vulkan surface created successfully");
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::cleanup() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    VKMON_DEBUG("Window cleanup complete");
}

// Callback registration
void Window::setKeyCallback(KeyCallback callback) {
    keyCallback_ = callback;
}

void Window::setMouseCallback(MouseCallback callback) {
    mouseCallback_ = callback;
}

void Window::setResizeCallback(ResizeCallback callback) {
    resizeCallback_ = callback;
}

// Static GLFW callback bridges
void Window::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowObj && windowObj->keyCallback_) {
        windowObj->keyCallback_(key, scancode, action, mods);
    }
}

void Window::glfwMouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowObj && windowObj->mouseCallback_) {
        windowObj->mouseCallback_(xpos, ypos);
    }
}

void Window::glfwResizeCallback(GLFWwindow* window, int width, int height) {
    Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowObj) {
        windowObj->resized_ = true;
        if (windowObj->resizeCallback_) {
            windowObj->resizeCallback_(width, height);
        }
    }
}

} // namespace VulkanMon
```

#### Step 1.3: Update CMakeLists.txt
```cmake
# Add new source files
set(SOURCES
    src/main.cpp
    src/Window.cpp  # NEW
    src/Logger.cpp
    # ... existing files
)
```

#### Step 1.4: Integrate Window Class into HelloTriangleApp
```cpp
// In HelloTriangleApp class, replace:
GLFWwindow* window;

// With:
std::unique_ptr<Window> window_;

// Update initWindow() method:
void initWindow() {
    window_ = std::make_unique<Window>(WINDOW_WIDTH, WINDOW_HEIGHT, "VulkanMon");
    window_->initialize();
    
    // Register callbacks  
    window_->setKeyCallback([this](int key, int scancode, int action, int mods) {
        keyCallback(window_->getWindow(), key, scancode, action, mods);
    });
    
    window_->setMouseCallback([this](double xpos, double ypos) {
        mouseCallback(window_->getWindow(), xpos, ypos);  
    });
    
    window_->setResizeCallback([this](int width, int height) {
        framebufferResized = true;
    });
}

// Update createSurface() method:
void createSurface() {
    window_->createSurface(instance);
}

// Update main loop condition:
while (!window_->shouldClose()) {
    window_->pollEvents();
    // ... rest of loop
}

// Update all window references:
// OLD: window
// NEW: window_->getWindow()
```

#### Step 1.5: Validation Checklist
- [ ] Application starts and displays window
- [ ] Window resizing works correctly
- [ ] Keyboard input still functions
- [ ] Mouse input still functions  
- [ ] Application closes properly
- [ ] Memory leaks check (Valgrind/AddressSanitizer)

## Phase 2: InputHandler Class Extraction

### Target: Input Processing and System Control

#### Current Code Locations
```cpp
// Lines 63-109: Key callback
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Material cycling, lighting controls, hot reload
}

// Lines 110-121: Mouse callback  
static void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    // Camera movement
}

// Member variables to extract:
bool firstMouse = true;
float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;
```

#### Step 2.1: Create InputHandler Class Header
**File**: `src/InputHandler.h`
```cpp
#pragma once

#include "Camera.h"
#include "LightingSystem.h"
#include "MaterialSystem.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <functional>

namespace VulkanMon {

/**
 * Input Processing and System Control
 * 
 * Handles all user input including:
 * - Camera movement (WASD + mouse)
 * - Material system controls (M, 5, 6 keys)
 * - Lighting system controls (1, 2, 3, 4, L keys)
 * - Hot reloading (R key)
 * 
 * Follows "Simple is Powerful" - focused input processing only.
 */
class InputHandler {
public:
    // Callback for shader reloading (InputHandler can't reload shaders directly)
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
    
    // Input state management
    void resetMousePosition();
    
private:
    // System references
    std::shared_ptr<Camera> camera_;
    std::shared_ptr<LightingSystem> lightingSystem_;
    std::shared_ptr<MaterialSystem> materialSystem_;
    
    // Mouse state
    bool firstMouse_ = true;
    float lastMouseX_;
    float lastMouseY_;
    
    // Callbacks
    ShaderReloadCallback shaderReloadCallback_;
    
    // Input processing helpers
    void handleCameraInput(GLFWwindow* window, float deltaTime);
    void handleMaterialControls(int key);
    void handleLightingControls(int key);
    void handleSystemControls(int key);
};

} // namespace VulkanMon
```

#### Step 2.2: Create InputHandler Implementation
**File**: `src/InputHandler.cpp`
```cpp
#include "InputHandler.h"
#include "Logger.h"
#include <iostream>

namespace VulkanMon {

InputHandler::InputHandler(std::shared_ptr<Camera> camera,
                          std::shared_ptr<LightingSystem> lightingSystem,
                          std::shared_ptr<MaterialSystem> materialSystem)
    : camera_(camera)
    , lightingSystem_(lightingSystem)
    , materialSystem_(materialSystem)
    , lastMouseX_(400.0f)  // Default to center
    , lastMouseY_(300.0f) {
    
    VKMON_DEBUG("InputHandler initialized");
}

void InputHandler::processKeyInput(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            // System controls
            case GLFW_KEY_R:
                VKMON_INFO("[HOT RELOAD] R key pressed - reloading shaders...");
                if (shaderReloadCallback_) {
                    shaderReloadCallback_();
                }
                break;
                
            // Lighting controls
            case GLFW_KEY_1:
            case GLFW_KEY_2:
            case GLFW_KEY_3:
            case GLFW_KEY_4:
            case GLFW_KEY_L:
                handleLightingControls(key);
                break;
                
            // Material controls
            case GLFW_KEY_M:
            case GLFW_KEY_5:
            case GLFW_KEY_6:
                handleMaterialControls(key);
                break;
                
            default:
                // Unhandled key
                break;
        }
    }
}

void InputHandler::processMouseInput(double xpos, double ypos) {
    if (firstMouse_) {
        lastMouseX_ = static_cast<float>(xpos);
        lastMouseY_ = static_cast<float>(ypos);
        firstMouse_ = false;
    }

    float xoffset = static_cast<float>(xpos) - lastMouseX_;
    float yoffset = lastMouseY_ - static_cast<float>(ypos); // Reversed: y-coordinates go from bottom to top
    
    lastMouseX_ = static_cast<float>(xpos);
    lastMouseY_ = static_cast<float>(ypos);

    if (camera_) {
        camera_->processMouseMovement(xoffset, yoffset);
    }
}

void InputHandler::processContinuousInput(GLFWwindow* window, float deltaTime) {
    handleCameraInput(window, deltaTime);
}

void InputHandler::setShaderReloadCallback(ShaderReloadCallback callback) {
    shaderReloadCallback_ = callback;
}

void InputHandler::resetMousePosition() {
    firstMouse_ = true;
}

// Private helper methods
void InputHandler::handleCameraInput(GLFWwindow* window, float deltaTime) {
    if (!camera_) return;
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera_->processKeyboard(Camera::Movement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera_->processKeyboard(Camera::Movement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera_->processKeyboard(Camera::Movement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera_->processKeyboard(Camera::Movement::RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera_->processKeyboard(Camera::Movement::UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera_->processKeyboard(Camera::Movement::DOWN, deltaTime);
}

void InputHandler::handleMaterialControls(int key) {
    switch (key) {
        case GLFW_KEY_M:
            if (materialSystem_) {
                materialSystem_->cycleMaterial();
                VKMON_INFO("Material cycled");
            }
            break;
            
        case GLFW_KEY_5:
            if (materialSystem_) {
                materialSystem_->adjustShininess(-10.0f);
                VKMON_INFO("Shininess decreased");
            }
            break;
            
        case GLFW_KEY_6:
            if (materialSystem_) {
                materialSystem_->adjustShininess(10.0f);
                VKMON_INFO("Shininess increased");
            }
            break;
    }
}

void InputHandler::handleLightingControls(int key) {
    switch (key) {
        case GLFW_KEY_1:
            if (lightingSystem_) {
                lightingSystem_->adjustDirectionalLightIntensity(0.1f);
                VKMON_INFO("Light intensity increased");
            }
            break;
            
        case GLFW_KEY_2:
            if (lightingSystem_) {
                lightingSystem_->adjustDirectionalLightIntensity(-0.1f);
                VKMON_INFO("Light intensity decreased");
            }
            break;
            
        case GLFW_KEY_3:
            if (lightingSystem_) {
                lightingSystem_->cycleLightingPreset();
                VKMON_INFO("Lighting preset cycled");
            }
            break;
            
        case GLFW_KEY_4:
            if (lightingSystem_) {
                lightingSystem_->toggleAmbientLight();
                VKMON_INFO("Ambient light toggled");
            }
            break;
            
        case GLFW_KEY_L:
            if (lightingSystem_) {
                lightingSystem_->printDebugInfo();
            }
            break;
    }
}

} // namespace VulkanMon
```

## Phase 3: Scene Class Extraction

### Target: Model and World Management

#### Current Code Locations
```cpp
// Lines 1387-1396: Model loading
void loadTestModel() {
    currentModel = modelLoader->loadModel("test_cube.obj");
    createDefaultMaterial();
}

// Lines 1399-1410: Material creation
void createDefaultMaterial() {
    MaterialData defaultMaterial;
    // ... material setup
}

// Member variables to extract:
std::unique_ptr<Model> currentModel;
uint32_t currentMaterialId = 0;
```

#### Scene Class Implementation Strategy
**File**: `src/Scene.h` & `src/Scene.cpp`
```cpp
class Scene {
private:
    std::vector<std::unique_ptr<Model>> models_;
    std::vector<glm::mat4> transforms_;
    std::shared_ptr<ModelLoader> modelLoader_;
    
public:
    void loadTestScene();
    void addModel(std::unique_ptr<Model> model, const glm::mat4& transform = glm::mat4(1.0f));
    void render(VkCommandBuffer commandBuffer, const glm::mat4& view, const glm::mat4& proj);
};
```

## Phase 4: VulkanRenderer Class Extraction

### Target: All Vulkan Rendering State

This is the most complex extraction, involving:
- 40+ Vulkan objects
- Graphics pipeline creation
- Command buffer recording
- Descriptor set management
- Synchronization objects

#### Strategy: Incremental Migration
1. Create VulkanRenderer class shell
2. Move Vulkan instance creation
3. Move graphics pipeline creation
4. Move rendering loop
5. Move resource cleanup

## Phase 5: VulkanMonApplication Integration

### Target: Application Coordination

Final class that ties everything together:
```cpp
class VulkanMonApplication {
private:
    std::unique_ptr<Window> window_;
    std::unique_ptr<VulkanRenderer> renderer_;
    std::unique_ptr<InputHandler> inputHandler_;
    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Camera> camera_;
    
public:
    void run();
    void initialize();
    void mainLoop();
    void cleanup();
};
```

## Validation Protocol

### After Each Phase
1. **Build Verification**: Code compiles without warnings
2. **Functional Testing**: All features work identically
3. **Performance Testing**: FPS within 5% of baseline
4. **Memory Testing**: No memory leaks detected
5. **Visual Testing**: Screenshot comparison matches
6. **Unit Testing**: New classes have test coverage

### Rollback Strategy
Each phase creates a git branch:
- `phase-5-2-1-window-extraction`
- `phase-5-2-2-input-extraction`
- `phase-5-2-3-scene-extraction`
- `phase-5-2-4-renderer-extraction`
- `phase-5-2-5-application-integration`

If any phase fails validation, rollback to previous working state.

## Success Criteria

### Quantitative Goals
- main.cpp reduced from 1,634 to ~20 lines
- No single class exceeds 500 lines
- All features work identically to original
- Performance within 5% of baseline
- 100% test coverage on new classes

### Qualitative Goals
- Code is easier to understand
- Each class has single, clear responsibility
- New features can be added without touching multiple classes
- Team members can work on different systems independently

**Ready to transform HelloTriangleApp into VulkanMonApplication!**