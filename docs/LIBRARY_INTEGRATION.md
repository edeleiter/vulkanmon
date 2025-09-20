# VulkanMon Library Integration Guide

## ¦ Library Management Philosophy

VulkanMon uses **vcpkg** as the primary package manager, ensuring consistent, cross-platform library integration for Pokemon-style game development. All external dependencies are managed through vcpkg manifests for reproducible builds.

### Core Integration Principles
1. **vcpkg-First** - All libraries integrated through vcpkg when possible
2. **Minimal Dependencies** - Only include libraries that provide substantial value
3. **Cross-Platform** - Libraries must support Windows, Linux, and macOS
4. **Performance-Oriented** - Libraries chosen for game engine performance requirements

---

## § vcpkg Package Management

### vcpkg Configuration
**File**: `vcpkg.json` (Project Root)

```json
{
  "name": "vulkanmon",
  "version": "1.0.0",
  "description": "Pokemon-style 3D game engine with Vulkan graphics",
  "dependencies": [
    "vulkan",
    "glfw3",
    "glm",
    "assimp",
    "stb",
    "imgui[vulkan-binding,glfw-binding]",
    "catch2"
  ],
  "builtin-baseline": "2024.08.23",
  "overrides": [
    {
      "name": "vulkan",
      "version": "1.3.268.0"
    }
  ]
}
```

### vcpkg Integration Workflow

#### 1. Initial Setup
```bash
# Initialize vcpkg submodule (already included in repo)
git submodule update --init vcpkg

# Bootstrap vcpkg for current platform
./vcpkg/bootstrap-vcpkg.sh    # Linux/macOS
./vcpkg/bootstrap-vcpkg.bat   # Windows

# Install all dependencies
./vcpkg/vcpkg install
```

#### 2. CMake Integration
```cmake
# CMakeLists.txt (Root)
cmake_minimum_required(VERSION 3.20)
project(vulkanmon)

# vcpkg toolchain integration
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
    CACHE STRING "Vcpkg toolchain file")

# Find all vcpkg packages
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# Link libraries to main executable
target_link_libraries(vulkanmon PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    assimp::assimp
    imgui::imgui
)
```

#### 3. Dependency Updates
```bash
# Update vcpkg to latest baseline
cd vcpkg
git pull origin master
cd ..

# Update vcpkg.json baseline
# Edit vcpkg.json -> "builtin-baseline": "new-commit-hash"

# Reinstall dependencies with new versions
./vcpkg/vcpkg install --recurse
```

---

## ¨ Graphics Libraries Integration

### 1. Vulkan SDK
**Purpose**: Low-level GPU API for maximum rendering performance
**Version**: 1.3.268.0+
**Integration**: Direct API usage with validation layers

#### Vulkan Setup
```cpp
// src/rendering/VulkanRenderer.h
#include <vulkan/vulkan.h>

class VulkanRenderer {
private:
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

public:
    bool initializeVulkan(GLFWwindow* window);
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
};
```

#### Vulkan Extension Management
```cpp
// Required instance extensions
std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    #ifdef DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    return extensions;
}

// Device extensions
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
};
```

### 2. GLFW3 (Window Management)
**Purpose**: Cross-platform window creation and input handling
**Version**: 3.4+
**Integration**: Window lifecycle and event callbacks

#### GLFW Integration
```cpp
// src/core/Window.h
#include <GLFW/glfw3.h>

class Window {
    GLFWwindow* window = nullptr;
    int width = 1920, height = 1080;

public:
    bool initialize();
    void cleanup();
    bool shouldClose() const;
    void pollEvents();
    GLFWwindow* getWindow() const { return window; }

    // Callback registration
    void setKeyCallback(GLFWkeyfun callback);
    void setMouseCallback(GLFWcursorposfun callback);
    void setResizeCallback(GLFWframebuffersizefun callback);
};

// Window initialization
bool Window::initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan, not OpenGL
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, "VulkanMon", nullptr, nullptr);
    if (!window) {
        Logger::error("Failed to create GLFW window");
        return false;
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window, this);
    return true;
}
```

#### Input Event Integration
```cpp
// Input callback setup
void setupInputCallbacks(GLFWwindow* window, InputHandler* handler) {
    glfwSetWindowUserPointer(window, handler);

    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(w));
        handler->onKeyPress(key, action, mods);
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) {
        auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(w));
        handler->onMouseMove(xpos, ypos);
    });
}
```

### 3. GLM (Mathematics)
**Purpose**: OpenGL-compatible mathematics for 3D graphics
**Version**: 1.0.1+
**Integration**: Vector/matrix operations throughout codebase

#### GLM Usage Patterns
```cpp
// src/core/Transform.h
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};  // Euler angles in degrees
    glm::vec3 scale{1.0f};

    glm::mat4 getModelMatrix() const {
        glm::mat4 translate = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotateX = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1, 0, 0));
        glm::mat4 rotateY = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0, 1, 0));
        glm::mat4 rotateZ = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0, 0, 1));
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

        return translate * rotateY * rotateX * rotateZ * scaleMatrix;
    }

    glm::vec3 getForward() const {
        glm::mat4 rotMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0, 1, 0));
        return glm::normalize(glm::vec3(rotMatrix * glm::vec4(0, 0, -1, 0)));
    }
};
```

#### Camera Mathematics
```cpp
// src/systems/CameraSystem.cpp
glm::mat4 CameraSystem::getViewMatrix() const {
    if (activeCameraEntity == INVALID_ENTITY) return glm::mat4(1.0f);

    const auto& transform = entityManager->getComponent<Transform>(activeCameraEntity);
    const auto& camera = entityManager->getComponent<Camera>(activeCameraEntity);

    glm::vec3 eye = transform.position;
    glm::vec3 center = eye + transform.getForward();
    glm::vec3 up = transform.getUp();

    return glm::lookAt(eye, center, up);
}

glm::mat4 CameraSystem::getProjectionMatrix() const {
    return glm::perspective(
        glm::radians(config.fov),
        config.aspectRatio,
        config.nearPlane,
        config.farPlane
    );
}
```

---

## — Asset Loading Libraries

### 1. Assimp (3D Model Loading)
**Purpose**: Universal 3D model format support (40+ formats)
**Version**: 5.4.0+
**Integration**: ModelLoader system for .obj, .fbx, .gltf, etc.

#### Assimp Integration
```cpp
// src/rendering/ModelLoader.h
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class ModelLoader {
    Assimp::Importer importer;
    std::unordered_map<std::string, std::shared_ptr<Model>> modelCache;

public:
    std::shared_ptr<Model> loadModel(const std::string& path);

private:
    Model processScene(const aiScene* scene, const std::string& directory);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type);
};

// Model loading implementation
std::shared_ptr<Model> ModelLoader::loadModel(const std::string& path) {
    // Check cache first
    auto it = modelCache.find(path);
    if (it != modelCache.end()) {
        return it->second;
    }

    // Load with Assimp
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |           // Convert to triangles
        aiProcess_FlipUVs |              // Flip UV coordinates
        aiProcess_GenNormals |           // Generate normals if missing
        aiProcess_CalcTangentSpace       // Calculate tangent space for normal mapping
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::error("Assimp error: " + std::string(importer.GetErrorString()));
        return nullptr;
    }

    auto model = std::make_shared<Model>();
    *model = processScene(scene, path.substr(0, path.find_last_of('/')));

    // Cache for future use
    modelCache[path] = model;
    return model;
}
```

#### Supported Model Formats
- **Common Formats**: .obj, .fbx, .gltf/.glb, .dae, .3ds
- **Industry Formats**: .blend (Blender), .max (3ds Max), .maya
- **Game Formats**: .md2/.md3 (Quake), .x (DirectX)
- **CAD Formats**: .ply, .stl, .off

### 2. STB Libraries (Image Loading)
**Purpose**: Lightweight image loading for textures
**Version**: Latest via vcpkg
**Integration**: AssetManager for PNG, JPG, TGA texture loading

#### STB Integration
```cpp
// src/rendering/AssetManager.h
#include <stb_image.h>

class AssetManager {
public:
    struct LoadedTexture {
        int width, height, channels;
        std::unique_ptr<unsigned char[]> data;
        std::string debugName;

        LoadedTexture(int w, int h, int c, unsigned char* pixels, const std::string& name)
            : width(w), height(h), channels(c), debugName(name) {
            size_t dataSize = w * h * c;
            data = std::make_unique<unsigned char[]>(dataSize);
            std::memcpy(data.get(), pixels, dataSize);
        }
    };

    LoadedTexture loadTexture(const std::string& path);

private:
    std::unordered_map<std::string, LoadedTexture> textureCache;
};

// Texture loading implementation
LoadedTexture AssetManager::loadTexture(const std::string& path) {
    // Check cache
    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
        return it->second;
    }

    // Load with STB
    stbi_set_flip_vertically_on_load(true); // OpenGL convention

    int width, height, channels;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        Logger::error("Failed to load texture: " + path);
        Logger::error("STB error: " + std::string(stbi_failure_reason()));
        return {0, 0, 0, nullptr, "error"};
    }

    // Force RGBA format for Vulkan compatibility
    LoadedTexture texture(width, height, 4, pixels, path);
    stbi_image_free(pixels);

    // Cache for future use
    textureCache[path] = texture;
    return texture;
}
```

#### Supported Image Formats
- **PNG** - High quality with alpha support
- **JPG/JPEG** - Compressed photos and textures
- **TGA** - Uncompressed with alpha channel
- **BMP** - Windows bitmap format
- **GIF** - Animated images (first frame only)
- **HDR** - High dynamic range images

---

## ¥ User Interface Libraries

### ImGui Integration (Debug Interface)
**Purpose**: Immediate mode GUI for development tools and ECS inspector
**Version**: 1.90.0+ with Vulkan and GLFW bindings
**Integration**: ECS Inspector and debug overlays

#### ImGui Setup
```cpp
// src/core/Application.cpp - ImGui initialization
void Application::initializeImGui() {
    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();

    // Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(window->getWindow(), true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = renderer->getInstance();
    init_info.PhysicalDevice = renderer->getPhysicalDevice();
    init_info.Device = renderer->getDevice();
    init_info.QueueFamily = renderer->getGraphicsQueueFamily();
    init_info.Queue = renderer->getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = renderer->getImGuiDescriptorPool();
    init_info.Allocator = nullptr;
    init_info.MinImageCount = 2;
    init_info.ImageCount = renderer->getSwapchainImageCount();
    init_info.CheckVkResultFn = checkVkResult;

    ImGui_ImplVulkan_Init(&init_info, renderer->getRenderPass());

    // Upload ImGui fonts to GPU
    renderer->uploadImGuiFonts();
}
```

#### ECS Inspector Implementation
```cpp
// src/debug/ECSInspector.cpp
void ECSInspector::render(EntityManager& entityManager) {
    if (!showInspector) return;

    ImGui::Begin("ECS Inspector", &showInspector);

    // Entity list
    if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char filter[256] = "";
        ImGui::InputText("Filter", filter, sizeof(filter));

        for (EntityID entity = 1; entity <= entityManager.getMaxEntityID(); ++entity) {
            if (!entityManager.isEntityValid(entity)) continue;

            std::string entityName = "Entity " + std::to_string(entity);
            if (strlen(filter) > 0 && entityName.find(filter) == std::string::npos) {
                continue;
            }

            if (ImGui::Selectable(entityName.c_str(), selectedEntity == entity)) {
                selectedEntity = entity;
            }
        }
    }

    // Component inspector for selected entity
    if (selectedEntity != INVALID_ENTITY) {
        renderComponentInspector(selectedEntity, entityManager);
    }

    ImGui::End();
}

void ECSInspector::renderComponentInspector(EntityID entity, EntityManager& em) {
    ImGui::Separator();
    ImGui::Text("Components for Entity %u", entity);

    // Transform component
    if (em.hasComponent<Transform>(entity)) {
        if (ImGui::CollapsingHeader("Transform")) {
            auto& transform = em.getComponent<Transform>(entity);

            ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
            ImGui::DragFloat3("Rotation", &transform.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.01f, 10.0f);
        }
    }

    // Renderable component
    if (em.hasComponent<Renderable>(entity)) {
        if (ImGui::CollapsingHeader("Renderable")) {
            auto& renderable = em.getComponent<Renderable>(entity);

            ImGui::Text("Mesh: %s", renderable.meshPath.c_str());
            ImGui::SliderInt("Material ID", reinterpret_cast<int*>(&renderable.materialId), 0, 4);
            ImGui::Checkbox("Visible", &renderable.visible);
            ImGui::SliderFloat("LOD Bias", &renderable.lodBias, 0.1f, 2.0f);
        }
    }
}
```

#### ImGui Performance Features
- **Immediate Mode** - No retained state, rebuild every frame
- **Vulkan Integration** - Native Vulkan rendering backend
- **Memory Efficient** - Minimal overhead for debug builds
- **Hot Reload** - Live entity editing without restart

---

## ª Testing Libraries

### Catch2 Testing Framework
**Purpose**: Modern C++ unit testing framework
**Version**: 3.5.0+
**Integration**: Comprehensive test suite for all engine components

#### Catch2 Setup
```cpp
// tests_cpp/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

// Custom configuration for Pokemon testing
struct PokemonTestConfig {
    static constexpr size_t MAX_ENTITIES = 10000;
    static constexpr float PERFORMANCE_THRESHOLD_MS = 5.0f;
    static constexpr float SPATIAL_QUERY_THRESHOLD_MS = 0.5f;
};
```

#### Test Organization
```cpp
// tests_cpp/test_Logger.cpp
#include <catch2/catch_test_macros.hpp>
#include "src/Logger.h"

TEST_CASE("Logger thread safety", "[Logger][ThreadSafety]") {
    Logger logger;
    std::atomic<int> completedThreads{0};

    SECTION("Concurrent logging from multiple threads") {
        std::vector<std::thread> threads;

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&logger, &completedThreads, i]() {
                for (int j = 0; j < 100; ++j) {
                    logger.info("Thread " + std::to_string(i) + " message " + std::to_string(j));
                }
                completedThreads++;
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(completedThreads == 10);
    }
}
```

#### Performance Benchmarking
```cpp
// tests_cpp/bench_SpatialSystem.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Spatial system performance", "[SpatialSystem][Performance]") {
    SpatialManager spatialManager;

    BENCHMARK("Insert 1000 entities") {
        for (int i = 0; i < 1000; ++i) {
            EntityID entity = static_cast<EntityID>(i + 1);
            glm::vec3 pos(i % 100, 0, i / 100);
            BoundingBox bounds{pos - glm::vec3(0.5f), pos + glm::vec3(0.5f)};
            spatialManager.updateEntity(entity, bounds);
        }
    };

    BENCHMARK("Radius query with 1000 entities") {
        auto results = spatialManager.findCreaturesInRadius(glm::vec3(50, 0, 50), 10.0f);
        return results.size();
    };
}
```

---

## § Build System Integration

### CMake Library Linking
```cmake
# CMakeLists.txt - Main executable
add_executable(vulkanmon
    src/main.cpp
    src/core/Application.cpp
    src/rendering/VulkanRenderer.cpp
    # ... other source files
)

# Link all vcpkg libraries
target_link_libraries(vulkanmon PRIVATE
    # Graphics
    Vulkan::Vulkan
    glfw
    glm::glm

    # Asset loading
    assimp::assimp
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/lib/stb_image.lib

    # UI
    imgui::imgui

    # Platform-specific libraries
    $<$<PLATFORM_ID:Windows>:>
    $<$<PLATFORM_ID:Linux>:dl pthread>
)

# Preprocessor definitions
target_compile_definitions(vulkanmon PRIVATE
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE  # Vulkan depth range [0,1]
    STB_IMAGE_IMPLEMENTATION
    $<$<CONFIG:Debug>:DEBUG>
)

# Include directories
target_include_directories(vulkanmon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/include
)
```

### Cross-Platform Compatibility
```cmake
# Platform-specific configurations
if(WIN32)
    # Windows-specific settings
    target_compile_definitions(vulkanmon PRIVATE WIN32_LEAN_AND_MEAN)
    set_property(TARGET vulkanmon PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

if(UNIX AND NOT APPLE)
    # Linux-specific settings
    target_link_libraries(vulkanmon PRIVATE dl pthread)
endif()

if(APPLE)
    # macOS-specific settings (future)
    find_library(MOLTEN_VK NAMES MoltenVK PATHS /usr/local/lib)
    target_link_libraries(vulkanmon PRIVATE ${MOLTEN_VK})
endif()
```

---

## Š Library Performance Impact

### Memory Usage by Library
- **Vulkan Runtime**: ~30MB (driver dependent)
- **GLFW**: ~2MB (window management)
- **GLM**: Header-only (zero runtime cost)
- **Assimp**: ~15MB (model loading)
- **STB**: ~1MB (image loading)
- **ImGui**: ~5MB (debug interface)
- **Catch2**: Test-only (zero runtime cost)

### Performance Characteristics
- **Vulkan**: Direct GPU control, minimal CPU overhead
- **GLFW**: <1ms event processing per frame
- **GLM**: Compile-time optimization, zero-cost abstractions
- **Assimp**: One-time loading cost, cached models
- **STB**: Fast image decoding, minimal allocation
- **ImGui**: ~0.5ms rendering overhead in debug builds

### Build Time Impact
- **Total Dependencies**: ~15 minutes first build
- **Incremental Builds**: <30 seconds for code changes
- **vcpkg Caching**: Subsequent builds use pre-built libraries
- **Cross-Platform**: Identical build process Windows/Linux

## ¯ Library Selection Rationale

### Why These Libraries?
1. **Vulkan** - Industry standard for high-performance graphics
2. **GLFW** - Lightweight, cross-platform window management
3. **GLM** - OpenGL-compatible math, header-only efficiency
4. **Assimp** - Universal model format support for Pokemon assets
5. **STB** - Minimal, reliable image loading
6. **ImGui** - Industry-standard debug interface (Unity/Unreal style)
7. **Catch2** - Modern C++ testing with excellent Pokemon scenario support

### Alternative Libraries Considered
- **SDL2** vs **GLFW** - GLFW chosen for Vulkan focus and simplicity
- **DirectXMath** vs **GLM** - GLM chosen for cross-platform compatibility
- **FBX SDK** vs **Assimp** - Assimp chosen for open-source and format variety
- **SOIL** vs **STB** - STB chosen for header-only convenience
- **Dear ImGui** vs **CEGUI** - ImGui chosen for immediate mode and Vulkan support
- **Google Test** vs **Catch2** - Catch2 chosen for header-only and BDD style

The library integration provides a solid foundation for Pokemon Legends: Arceus-style development with professional build system and cross-platform support.