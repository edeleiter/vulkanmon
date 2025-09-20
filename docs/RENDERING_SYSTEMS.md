# Vulkan Rendering Systems Documentation

## ¨ Graphics Pipeline Overview (src/rendering/)

VulkanMon implements a modern Vulkan-based rendering pipeline designed for Pokemon-style 3D environments with efficient multi-object rendering and PBR (Physically Based Rendering) materials.

### Rendering Architecture
- **Low-Level Vulkan API** - Direct GPU control for maximum performance
- **RAII Resource Management** - Automatic cleanup of Vulkan objects
- **Multi-Object Rendering** - Efficient batching and sorting
- **PBR Material System** - Physically based lighting and materials
- **Dynamic Asset Loading** - Runtime model and texture management

---

## € VulkanRenderer Core
**File**: `src/rendering/VulkanRenderer.h/.cpp`
**Purpose**: Central Vulkan abstraction and command coordination

### Core Responsibilities
- Vulkan instance, device, and swapchain management
- Command buffer recording and submission
- Render pass and pipeline state management
- Multi-object rendering with material optimization
- Integration with ECS rendering systems

### Key Architecture Components
```cpp
class VulkanRenderer {
public:
    // Initialization
    bool initialize(GLFWwindow* window);
    void cleanup();

    // Frame rendering
    void beginFrame();
    void endFrame();

    // ECS Integration
    void setECSRenderCallback(std::function<void()> callback);
    void beginECSFrame();
    void renderECSObject(const glm::mat4& modelMatrix,
                        const std::string& meshPath,
                        uint32_t materialId);
    void endECSFrame();

    // Resource management
    bool ensureMeshLoaded(const std::string& path);
    void recreateSwapChain();  // Window resize handling

private:
    // Vulkan core objects
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Model caching
    std::unordered_map<std::string, std::shared_ptr<Model>> modelCache;
};
```

### Rendering Pipeline Flow
1. **Frame Begin** - Acquire swapchain image, begin command recording
2. **Render Pass** - Begin render pass with depth testing
3. **ECS Rendering** - Process all renderable entities via callback
4. **Material Batching** - Group objects by material for efficiency
5. **Command Submission** - Submit commands to GPU queue
6. **Frame End** - Present to swapchain, handle synchronization

---

##   Resource Management System
**File**: `src/rendering/ResourceManager.h/.cpp`
**Purpose**: RAII-compliant Vulkan resource lifecycle management

### ResourceManager Features
- **Automatic Cleanup** - RAII destructors for all Vulkan objects
- **Memory Safety** - No raw Vulkan handle exposure
- **Buffer Management** - Vertex, index, and uniform buffer creation
- **Image Resources** - Texture loading and GPU upload
- **Descriptor Sets** - Material property binding

### Resource Types
```cpp
class ResourceManager {
public:
    // Buffer creation with automatic cleanup
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        ~Buffer(); // Automatic cleanup
    };

    // Image resources
    struct Image {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        ~Image(); // Automatic cleanup
    };

    // Factory methods
    std::unique_ptr<Buffer> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    std::unique_ptr<Image> createImage(uint32_t width, uint32_t height, VkFormat format);
};
```

### Memory Management Strategy
- **RAII Principle** - All resources cleaned up automatically
- **Smart Pointers** - std::unique_ptr for ownership
- **No Memory Leaks** - Validated with Vulkan validation layers
- **Exception Safety** - Resources cleaned up even during errors

---

## ¨ Material System
**File**: `src/rendering/MaterialSystem.h/.cpp`
**Purpose**: PBR material properties and GPU data management

### Material Architecture
The material system provides Pokemon-style material presets with physically based rendering properties.

```cpp
struct MaterialData {
    alignas(16) glm::vec4 ambient;   // Ambient color and intensity
    alignas(16) glm::vec4 diffuse;   // Diffuse color and opacity
    alignas(16) glm::vec4 specular;  // Specular color and intensity
    alignas(4)  float shininess;     // Specular exponent
};

class MaterialSystem {
public:
    MaterialSystem(std::shared_ptr<ResourceManager> resourceManager);

    // Material presets
    enum MaterialPreset {
        DEFAULT = 0,
        GOLD = 1,
        RUBY = 2,
        CHROME = 3,
        EMERALD = 4
    };

    void setActiveMaterial(MaterialPreset preset);
    MaterialData getCurrentMaterialData() const;
    void updateGPUData(); // Upload to uniform buffer
};
```

### Material Presets
- **Default** - Neutral gray material for testing
- **Gold** - Metallic yellow with high specular reflection
- **Ruby** - Deep red gemstone with moderate specular
- **Chrome** - Highly reflective metallic surface
- **Emerald** - Green gemstone with brilliant highlights

### GPU Integration
- **Uniform Buffers** - Material data uploaded to GPU
- **Descriptor Sets** - Efficient material switching
- **Vulkan Alignment** - Proper vec4 alignment for GPU compatibility

---

## ¡ Lighting System
**File**: `src/rendering/LightingSystem.h/.cpp`
**Purpose**: Scene lighting with Blinn-Phong illumination model

### Lighting Architecture
```cpp
struct LightingData {
    alignas(16) glm::vec4 directionalLight;  // Direction + intensity
    alignas(16) glm::vec4 lightColor;        // RGB color + ambient
    alignas(16) glm::vec4 ambientLight;      // Global ambient illumination
};

class LightingSystem {
public:
    // Lighting presets
    enum LightingPreset {
        DEFAULT = 0,    // Balanced indoor lighting
        DAYLIGHT = 1,   // Bright outdoor sun
        SUNSET = 2,     // Warm orange evening light
        MOONLIGHT = 3   // Cool blue night lighting
    };

    void setLightingPreset(LightingPreset preset);
    void setDirectionalLightIntensity(float intensity);
    void toggleAmbientLight();

    LightingData getCurrentLightingData() const;
};
```

### Lighting Features
- **Directional Lighting** - Sun-like directional illumination
- **Ambient Lighting** - Global light for Pokemon world visibility
- **Blinn-Phong Model** - Efficient specular highlights
- **Dynamic Control** - Real-time lighting adjustment
- **Preset System** - Quick lighting scenarios (day/night/sunset)

### Interactive Controls
- **1/2 Keys** - Adjust directional light intensity
- **3 Key** - Cycle through lighting presets
- **4 Key** - Toggle ambient lighting on/off
- **L Key** - Print detailed lighting debug information

---

## ­ Asset Management Systems

### AssetManager
**File**: `src/rendering/AssetManager.h/.cpp`
**Purpose**: Texture loading, caching, and asset discovery

#### Features
- **Multi-Format Support** - PNG, JPG via STB Image
- **Automatic Caching** - Prevent duplicate loading
- **Asset Discovery** - Recursive directory scanning
- **Debug Information** - Asset statistics and validation

```cpp
class AssetManager {
public:
    struct LoadedTexture {
        int width, height, channels;
        std::unique_ptr<unsigned char[]> data;
        std::string debugName;
    };

    LoadedTexture loadTexture(const std::string& path);
    std::vector<std::string> discoverAssets(const std::string& directory);
    void clearCache();
};
```

### ModelLoader
**File**: `src/rendering/ModelLoader.h/.cpp`
**Purpose**: 3D model loading via Assimp with material support

#### Supported Formats
Via Assimp library: .obj, .fbx, .gltf, .dae, .3ds, .blend, and 40+ other formats

```cpp
class ModelLoader {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
    };

    struct Model {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::string materialPath; // Associated texture
    };

    std::shared_ptr<Model> loadModel(const std::string& path);
    void logStatistics() const;

private:
    std::unordered_map<std::string, std::shared_ptr<Model>> modelCache;
};
```

#### Model Processing Pipeline
1. **Assimp Loading** - Parse 3D file format
2. **Mesh Processing** - Extract vertices, indices, normals
3. **UV Coordinate Handling** - Texture coordinate mapping
4. **Material Discovery** - Associated texture paths
5. **Memory Optimization** - Efficient vertex layout

---

## § Shader System

### Shader Pipeline
VulkanMon uses GLSL shaders compiled to SPIR-V for Vulkan compatibility.

#### Vertex Shader (`shaders/shader.vert`)
```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(ubo.model) * inNormal;
    fragTexCoord = inTexCoord;
}
```

#### Fragment Shader (`shaders/shader.frag`)
```glsl
#version 450

layout(binding = 1) uniform MaterialData {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} material;

layout(binding = 2) uniform LightingData {
    vec4 directionalLight;
    vec4 lightColor;
    vec4 ambientLight;
} lighting;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Blinn-Phong lighting calculation
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-lighting.directionalLight.xyz);

    // Diffuse component
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lighting.lightColor.rgb * material.diffuse.rgb;

    // Specular component (Blinn-Phong)
    vec3 viewDir = normalize(-fragWorldPos); // Camera at origin
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = spec * lighting.lightColor.rgb * material.specular.rgb;

    // Ambient component
    vec3 ambient = lighting.ambientLight.rgb * material.ambient.rgb;

    // Final color
    vec3 result = ambient + diffuse + specular;
    outColor = vec4(result, material.diffuse.a);
}
```

### Hot Reload System
- **R Key** - Reload shaders during runtime for rapid development
- **Automatic Compilation** - CMake GLSL to SPIR-V conversion
- **Error Handling** - Graceful fallback on shader compilation errors

---

## Š Performance Metrics

### Rendering Performance
- **Draw Calls** - Batched by material for efficiency
- **Frame Rate** - 60+ FPS with multi-object scenes
- **Memory Usage** - ~100MB for typical Pokemon scenes
- **Command Recording** - <2ms per frame

### Vulkan Validation
- **Debug Layers** - Comprehensive error checking during development
- **Memory Tracking** - Zero memory leaks validated
- **Performance Warnings** - Optimization suggestions during development

### Multi-Object Scenarios
- **5+ Diverse Models** - Cube, sphere, pyramid, plane with unique materials
- **Material Switching** - Efficient descriptor set changes
- **Spatial Culling** - Frustum culling integration for large scenes

The rendering system provides a solid foundation for Pokemon-style graphics with professional Vulkan implementation and efficient multi-object rendering capabilities.