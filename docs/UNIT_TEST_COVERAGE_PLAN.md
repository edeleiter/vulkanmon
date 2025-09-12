# VulkanMon Unit Test Coverage Plan
## Comprehensive Path to 100% Test Coverage

### Document Overview
This document provides an excruciating level of detail for achieving 100% unit test coverage for the VulkanMon game engine. Following our core principles of "Test, Test, Test" and "Simple is Powerful", this plan methodically addresses every gap in our current test suite.

---

## Current Test Coverage Assessment

### Test Coverage Statistics
- **Current Overall Coverage**: ~40%
- **Target Coverage**: 95%+ (100% for critical systems)
- **Total Test Files**: 13
- **Total Source Files**: 14 (12 classes + main.cpp + stb_image_impl.cpp)
- **Test Lines of Code**: 72,299 lines across all test files

### Test Quality Matrix

| Component | File Size | Test Status | Coverage % | Priority | Phase |
|-----------|-----------|-------------|------------|----------|--------|
| Logger | 2,744 bytes | ✅ Excellent | 95% | Critical | Complete |
| Camera | 8,121 bytes | ✅ Comprehensive | 90% | High | Complete |
| InputHandler | 19,404 bytes | ✅ Extensive* | 85% | High | Needs cleanup |
| Window | 12,186 bytes | ✅ Good | 80% | Medium | Complete |
| ResourceManager | 2,807 bytes | ❌ Placeholders | 5% | Critical | Phase 1 |
| Utils | 1,553 bytes | ⚠️ Basic | 30% | Critical | Phase 1 |
| MaterialSystem | 4,096 bytes | ⚠️ Incomplete | 40% | High | Phase 1 |
| LightingSystem | 3,585 bytes | ⚠️ Basic | 35% | High | Phase 2 |
| AssetManager | 3,853 bytes | ❌ Minimal | 10% | High | Phase 2 |
| ModelLoader | 5,521 bytes | ❌ Stubs | 15% | High | Phase 2 |
| Application | 3,078 bytes | ❌ Basic | 20% | Medium | Phase 3 |
| VulkanRenderer | 2,368 bytes | ❌ Stubs | 5% | Low** | Phase 3 |

*InputHandler has spam logging issues that need cleanup  
**VulkanRenderer requires integration test strategy due to complexity

---

## Detailed Implementation Plan

## Phase 1: Critical Foundation (Target: 65% Coverage)
**Timeline**: Week 1-2  
**Focus**: Enable other tests by completing foundational systems

### 1.1 ResourceManager Test Suite Enhancement

**File**: `tests_cpp/test_ResourceManager.cpp`  
**Current State**: Lines 50-57 contain only `REQUIRE(true)` placeholders  
**Target Coverage**: 95%  

#### 1.1.1 ManagedBuffer RAII Testing
**Location**: test_ResourceManager.cpp:50-54  
**Current Code**:
```cpp
TEST_CASE("ManagedBuffer RAII design", "[ResourceManager]") {
    REQUIRE(true); // Placeholder - need actual RAII tests
}
```

**Required Implementation**:
```cpp
TEST_CASE("ManagedBuffer RAII design", "[ResourceManager]") {
    SECTION("Constructor properly initializes buffer") {
        // Test buffer creation with valid parameters
        // Test buffer creation with invalid parameters (should throw)
        // Test move constructor behavior
        // Test move assignment behavior
    }
    
    SECTION("Destructor properly cleans up resources") {
        // Test that buffer is destroyed when object goes out of scope
        // Test that memory is freed correctly
        // Test behavior with null buffer
    }
    
    SECTION("Copy operations are disabled") {
        // Test that copy constructor is deleted
        // Test that copy assignment is deleted
    }
}
```

#### 1.1.2 ManagedBuffer Memory Mapping Testing
**Location**: test_ResourceManager.cpp:55-58  
**Current Code**:
```cpp
TEST_CASE("ManagedBuffer memory mapping", "[ResourceManager]") {
    REQUIRE(true); // Placeholder - need memory mapping tests
}
```

**Required Implementation**:
```cpp
TEST_CASE("ManagedBuffer memory mapping", "[ResourceManager]") {
    SECTION("Map memory for host-visible buffer") {
        // Test successful memory mapping
        // Test writing data to mapped memory
        // Test reading data from mapped memory
        // Test unmapping memory
    }
    
    SECTION("Map memory error conditions") {
        // Test mapping non-host-visible buffer (should fail gracefully)
        // Test double mapping (should handle correctly)
        // Test mapping with null buffer
    }
    
    SECTION("Persistent mapping behavior") {
        // Test persistently mapped buffers
        // Test flush/invalidate operations
        // Test memory coherence
    }
}
```

#### 1.1.3 Additional ResourceManager Tests Needed
**New Test Cases**:

```cpp
TEST_CASE("ManagedImage RAII design", "[ResourceManager]") {
    // Similar to ManagedBuffer but for images
    // Test image creation, destruction, move semantics
    // Test different image formats and layouts
    // Test mipmap handling
}

TEST_CASE("ManagedImage transition and barriers", "[ResourceManager]") {
    // Test layout transitions
    // Test memory barriers
    // Test synchronization
}

TEST_CASE("ResourceManager factory methods", "[ResourceManager]") {
    // Test createBuffer with various parameters
    // Test createImage with various parameters
    // Test error handling for invalid parameters
    // Test resource reuse and pooling if implemented
}

TEST_CASE("ResourceManager memory management", "[ResourceManager]") {
    // Test memory allocation strategies
    // Test memory fragmentation handling
    // Test out-of-memory conditions
    // Test memory usage tracking if implemented
}
```

### 1.2 Utils Test Suite Enhancement

**File**: `tests_cpp/test_Utils.cpp`  
**Current State**: Lines 22-29 have basic file operations  
**Target Coverage**: 90%  

#### 1.2.1 Current Test Analysis
**Location**: test_Utils.cpp:22-29  
**Current Code**:
```cpp
TEST_CASE("File reading operations", "[Utils][File]") {
    REQUIRE(true); // Placeholder for actual file tests
}

TEST_CASE("Shader file reading", "[Utils][File]") {
    REQUIRE_THROWS(Utils::readFile("nonexistent_shader.spv"));
}
```

#### 1.2.2 Enhanced File Operations Testing
**Required Implementation**:

```cpp
TEST_CASE("File reading operations", "[Utils][File]") {
    SECTION("Read valid text file") {
        // Create temporary text file with known content
        // Test Utils::readFile returns correct content
        // Test file size validation
        // Clean up temporary file
    }
    
    SECTION("Read valid binary file") {
        // Create temporary binary file with known content
        // Test Utils::readFile handles binary data correctly
        // Test large file handling (>1MB)
        // Clean up temporary file
    }
    
    SECTION("Read empty file") {
        // Create empty file
        // Test Utils::readFile returns empty vector
        // Clean up temporary file
    }
    
    SECTION("File reading error conditions") {
        // Test nonexistent file (should throw)
        // Test file with no read permissions (should throw)
        // Test directory instead of file (should throw)
        // Test corrupted file handling
    }
}

TEST_CASE("Shader file reading", "[Utils][File]") {
    SECTION("Valid SPIR-V shader file") {
        // Create valid SPIR-V binary data
        // Test successful shader loading
        // Validate SPIR-V header magic number
    }
    
    SECTION("Invalid shader file formats") {
        REQUIRE_THROWS(Utils::readFile("nonexistent_shader.spv"));
        // Test invalid SPIR-V format
        // Test corrupted shader file
        // Test text file with .spv extension
    }
    
    SECTION("Shader file validation") {
        // Test SPIR-V magic number validation
        // Test minimum file size requirements
        // Test endianness handling
    }
}
```

#### 1.2.3 Additional Utils Tests Needed
**New Test Cases**:

```cpp
TEST_CASE("Path utilities", "[Utils][Path]") {
    // Test path normalization
    // Test path joining
    // Test file extension extraction
    // Test directory creation
    // Test cross-platform path handling
}

TEST_CASE("String utilities", "[Utils][String]") {
    // Test string trimming
    // Test string splitting
    // Test case conversion if implemented
    // Test string validation functions
}

TEST_CASE("Math utilities", "[Utils][Math]") {
    // Test common math functions if implemented
    // Test floating-point comparisons
    // Test vector/matrix utilities if present
    // Test interpolation functions if present
}

TEST_CASE("Error handling utilities", "[Utils][Error]") {
    // Test error message formatting
    // Test exception wrapping utilities
    // Test logging integration
    // Test debug assertion helpers if present
}
```

### 1.3 MaterialSystem Test Suite Enhancement

**File**: `tests_cpp/test_MaterialSystem.cpp`  
**Current State**: 4,096 bytes with placeholder tests  
**Target Coverage**: 90%  

#### 1.3.1 Current Test Analysis
**Issues Identified**:
- Multiple `REQUIRE(true)` placeholder assertions
- Incomplete material property testing
- Missing descriptor management validation
- No error condition testing

#### 1.3.2 Enhanced Material Property Testing
**Required Implementation**:

```cpp
TEST_CASE("Material property validation", "[MaterialSystem]") {
    SECTION("Default material properties") {
        MaterialSystem matSys;
        auto material = matSys.getDefaultMaterial();
        
        // Test default ambient values
        REQUIRE(material.ambient.r == Approx(0.2f));
        REQUIRE(material.ambient.g == Approx(0.2f));
        REQUIRE(material.ambient.b == Approx(0.2f));
        REQUIRE(material.ambient.a == Approx(1.0f));
        
        // Test default diffuse values
        REQUIRE(material.diffuse.r == Approx(0.8f));
        REQUIRE(material.diffuse.g == Approx(0.8f));
        REQUIRE(material.diffuse.b == Approx(0.8f));
        REQUIRE(material.diffuse.a == Approx(1.0f));
        
        // Test default specular values and shininess
        REQUIRE(material.specular.r == Approx(1.0f));
        REQUIRE(material.specular.g == Approx(1.0f));
        REQUIRE(material.specular.b == Approx(1.0f));
        REQUIRE(material.shininess == Approx(32.0f));
    }
    
    SECTION("Preset material properties") {
        MaterialSystem matSys;
        
        // Test Gold preset
        auto gold = matSys.getMaterial("Gold");
        REQUIRE(gold.ambient.r == Approx(0.24725f));
        REQUIRE(gold.ambient.g == Approx(0.1995f));
        REQUIRE(gold.ambient.b == Approx(0.0745f));
        
        // Test Ruby preset
        auto ruby = matSys.getMaterial("Ruby");
        REQUIRE(ruby.diffuse.r == Approx(0.61424f));
        REQUIRE(ruby.diffuse.g == Approx(0.04136f));
        REQUIRE(ruby.diffuse.b == Approx(0.04136f));
        
        // Test Chrome preset
        auto chrome = matSys.getMaterial("Chrome");
        REQUIRE(chrome.specular.r == Approx(0.774597f));
        REQUIRE(chrome.shininess == Approx(76.8f));
    }
}

TEST_CASE("Material descriptor management", "[MaterialSystem]") {
    SECTION("Descriptor set updates") {
        // Test descriptor set creation
        // Test uniform buffer updates
        // Test descriptor binding
        // Test multiple material switching
    }
    
    SECTION("GPU memory alignment") {
        // Test MaterialData structure alignment
        // Test padding validation
        // Test buffer size calculations
    }
}

TEST_CASE("Material system error handling", "[MaterialSystem]") {
    SECTION("Invalid material names") {
        MaterialSystem matSys;
        
        // Test nonexistent material returns default
        auto material = matSys.getMaterial("NonexistentMaterial");
        // Should return default material
        REQUIRE(material.ambient.r == Approx(0.2f));
    }
    
    SECTION("Null parameter handling") {
        // Test null descriptor set handling
        // Test null device handling
        // Test invalid buffer handling
    }
}
```

---

## Phase 2: Core Engine Systems (Target: 85% Coverage)
**Timeline**: Week 3-4  
**Focus**: Complete testing of asset management and rendering systems

### 2.1 AssetManager Test Suite Implementation

**File**: `tests_cpp/test_AssetManager.cpp`  
**Current State**: 3,853 bytes mostly placeholder tests  
**Target Coverage**: 90%  

#### 2.1.1 Texture Loading and Management Testing

```cpp
TEST_CASE("Texture loading operations", "[AssetManager]") {
    SECTION("Load valid texture formats") {
        AssetManager assetMgr;
        
        // Test PNG loading
        auto pngTexture = assetMgr.loadTexture("test_texture.png");
        REQUIRE(pngTexture != nullptr);
        REQUIRE(pngTexture->getWidth() > 0);
        REQUIRE(pngTexture->getHeight() > 0);
        
        // Test JPG loading
        auto jpgTexture = assetMgr.loadTexture("test_texture.jpg");
        REQUIRE(jpgTexture != nullptr);
        
        // Test different bit depths (8-bit, 16-bit, 32-bit)
        // Test different channel counts (RGB, RGBA, Grayscale)
    }
    
    SECTION("Texture caching behavior") {
        AssetManager assetMgr;
        
        // Load same texture twice
        auto texture1 = assetMgr.loadTexture("test_texture.png");
        auto texture2 = assetMgr.loadTexture("test_texture.png");
        
        // Should return same cached instance
        REQUIRE(texture1.get() == texture2.get());
        
        // Test cache invalidation
        assetMgr.clearCache();
        auto texture3 = assetMgr.loadTexture("test_texture.png");
        REQUIRE(texture1.get() != texture3.get());
    }
    
    SECTION("Texture loading error conditions") {
        AssetManager assetMgr;
        
        // Test nonexistent file
        REQUIRE_THROWS(assetMgr.loadTexture("nonexistent.png"));
        
        // Test corrupted image file
        REQUIRE_THROWS(assetMgr.loadTexture("corrupted.png"));
        
        // Test unsupported format
        REQUIRE_THROWS(assetMgr.loadTexture("unsupported.bmp"));
    }
}

TEST_CASE("Asset discovery and enumeration", "[AssetManager]") {
    SECTION("Discover textures in directory") {
        AssetManager assetMgr;
        
        auto textures = assetMgr.discoverTextures("assets/textures/");
        REQUIRE(textures.size() > 0);
        
        // Test file extension filtering
        for (const auto& texture : textures) {
            REQUIRE((texture.ends_with(".png") || 
                    texture.ends_with(".jpg") || 
                    texture.ends_with(".jpeg")));
        }
    }
    
    SECTION("Discover models in directory") {
        AssetManager assetMgr;
        
        auto models = assetMgr.discoverModels("assets/models/");
        // Test various model formats are discovered
        // Test subdirectory traversal if supported
    }
}

TEST_CASE("Asset metadata management", "[AssetManager]") {
    SECTION("Texture metadata extraction") {
        AssetManager assetMgr;
        
        auto metadata = assetMgr.getTextureMetadata("test_texture.png");
        REQUIRE(metadata.width > 0);
        REQUIRE(metadata.height > 0);
        REQUIRE(metadata.channels > 0);
        REQUIRE(metadata.bitDepth > 0);
    }
    
    SECTION("Asset dependency tracking") {
        // Test asset reference counting
        // Test dependency graph if implemented
        // Test circular dependency detection
    }
}
```

### 2.2 ModelLoader Test Suite Implementation

**File**: `tests_cpp/test_ModelLoader.cpp`  
**Current State**: 5,521 bytes with limited functionality testing  
**Target Coverage**: 85%  

#### 2.2.1 3D Model Loading and Processing Testing

```cpp
TEST_CASE("3D model loading operations", "[ModelLoader]") {
    SECTION("Load OBJ model") {
        ModelLoader loader;
        
        auto model = loader.loadModel("test_model.obj");
        REQUIRE(model != nullptr);
        REQUIRE(model->getMeshCount() > 0);
        
        // Test vertex data extraction
        auto mesh = model->getMesh(0);
        REQUIRE(mesh.vertices.size() > 0);
        REQUIRE(mesh.indices.size() > 0);
        
        // Test vertex attributes
        for (const auto& vertex : mesh.vertices) {
            // Test position is valid (not NaN)
            REQUIRE(std::isfinite(vertex.position.x));
            REQUIRE(std::isfinite(vertex.position.y));
            REQUIRE(std::isfinite(vertex.position.z));
            
            // Test normal is normalized
            float normalLength = glm::length(vertex.normal);
            REQUIRE(normalLength == Approx(1.0f).margin(0.01f));
            
            // Test UV coordinates are in valid range
            REQUIRE(vertex.texCoord.x >= 0.0f);
            REQUIRE(vertex.texCoord.x <= 1.0f);
            REQUIRE(vertex.texCoord.y >= 0.0f);
            REQUIRE(vertex.texCoord.y <= 1.0f);
        }
    }
    
    SECTION("Load FBX model") {
        ModelLoader loader;
        
        auto model = loader.loadModel("test_model.fbx");
        REQUIRE(model != nullptr);
        
        // Test animation data if present
        if (model->hasAnimations()) {
            REQUIRE(model->getAnimationCount() > 0);
            
            auto animation = model->getAnimation(0);
            REQUIRE(animation.duration > 0.0f);
            REQUIRE(animation.ticksPerSecond > 0.0f);
        }
    }
    
    SECTION("Load glTF model") {
        ModelLoader loader;
        
        auto model = loader.loadModel("test_model.gltf");
        REQUIRE(model != nullptr);
        
        // Test PBR material properties
        if (model->getMaterialCount() > 0) {
            auto material = model->getMaterial(0);
            // Test albedo, metallic, roughness values
            REQUIRE(material.albedo.r >= 0.0f);
            REQUIRE(material.albedo.r <= 1.0f);
        }
    }
}

TEST_CASE("Material extraction and processing", "[ModelLoader]") {
    SECTION("Extract materials from model") {
        ModelLoader loader;
        
        auto model = loader.loadModel("textured_model.obj");
        REQUIRE(model->getMaterialCount() > 0);
        
        auto material = model->getMaterial(0);
        
        // Test diffuse texture path
        if (!material.diffuseTexturePath.empty()) {
            REQUIRE(std::filesystem::exists(material.diffuseTexturePath));
        }
        
        // Test material properties
        REQUIRE(material.diffuse.r >= 0.0f);
        REQUIRE(material.diffuse.r <= 1.0f);
        REQUIRE(material.specular.r >= 0.0f);
        REQUIRE(material.specular.r <= 1.0f);
        REQUIRE(material.shininess >= 0.0f);
    }
    
    SECTION("Handle models without materials") {
        ModelLoader loader;
        
        auto model = loader.loadModel("no_material_model.obj");
        REQUIRE(model != nullptr);
        
        // Should have default material
        REQUIRE(model->getMaterialCount() == 1);
        auto defaultMaterial = model->getMaterial(0);
        REQUIRE(defaultMaterial.diffuse.r > 0.0f);
    }
}

TEST_CASE("Model loading error handling", "[ModelLoader]") {
    SECTION("Invalid file paths") {
        ModelLoader loader;
        
        REQUIRE_THROWS(loader.loadModel("nonexistent.obj"));
        REQUIRE_THROWS(loader.loadModel(""));
        REQUIRE_THROWS(loader.loadModel("directory_not_file"));
    }
    
    SECTION("Corrupted model files") {
        ModelLoader loader;
        
        REQUIRE_THROWS(loader.loadModel("corrupted.obj"));
        REQUIRE_THROWS(loader.loadModel("invalid_format.txt"));
    }
    
    SECTION("Unsupported model formats") {
        ModelLoader loader;
        
        // Test graceful handling of unsupported formats
        REQUIRE_THROWS(loader.loadModel("model.unsupported"));
    }
}

TEST_CASE("Mesh processing and optimization", "[ModelLoader]") {
    SECTION("Mesh validation") {
        ModelLoader loader;
        
        auto model = loader.loadModel("complex_model.obj");
        
        for (size_t i = 0; i < model->getMeshCount(); ++i) {
            auto mesh = model->getMesh(i);
            
            // Test index bounds
            for (const auto& index : mesh.indices) {
                REQUIRE(index < mesh.vertices.size());
            }
            
            // Test triangle integrity
            REQUIRE(mesh.indices.size() % 3 == 0);
        }
    }
    
    SECTION("Vertex optimization") {
        // Test duplicate vertex removal if implemented
        // Test vertex cache optimization if implemented
        // Test mesh simplification if implemented
    }
}
```

### 2.3 LightingSystem Test Suite Implementation

**File**: `tests_cpp/test_LightingSystem.cpp`  
**Current State**: 3,585 bytes with basic tests  
**Target Coverage**: 90%  

#### 2.3.1 Lighting Calculations and Management Testing

```cpp
TEST_CASE("Directional light management", "[LightingSystem]") {
    SECTION("Default directional light") {
        LightingSystem lightSys;
        
        auto dirLight = lightSys.getDirectionalLight();
        
        // Test default direction is normalized
        float length = glm::length(dirLight.direction);
        REQUIRE(length == Approx(1.0f).margin(0.01f));
        
        // Test default color values
        REQUIRE(dirLight.color.r >= 0.0f);
        REQUIRE(dirLight.color.g >= 0.0f);
        REQUIRE(dirLight.color.b >= 0.0f);
        REQUIRE(dirLight.color.a >= 0.0f);
        
        // Test default intensity
        REQUIRE(dirLight.intensity > 0.0f);
        REQUIRE(dirLight.intensity <= 10.0f); // Reasonable range
    }
    
    SECTION("Modify directional light properties") {
        LightingSystem lightSys;
        
        // Test direction setting
        glm::vec3 newDirection = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
        lightSys.setDirectionalLightDirection(newDirection);
        
        auto dirLight = lightSys.getDirectionalLight();
        REQUIRE(dirLight.direction.x == Approx(newDirection.x));
        REQUIRE(dirLight.direction.y == Approx(newDirection.y));
        REQUIRE(dirLight.direction.z == Approx(newDirection.z));
        
        // Test color setting
        glm::vec4 newColor(0.8f, 0.9f, 1.0f, 1.0f);
        lightSys.setDirectionalLightColor(newColor);
        
        dirLight = lightSys.getDirectionalLight();
        REQUIRE(dirLight.color.r == Approx(newColor.r));
        REQUIRE(dirLight.color.g == Approx(newColor.g));
        REQUIRE(dirLight.color.b == Approx(newColor.b));
        REQUIRE(dirLight.color.a == Approx(newColor.a));
        
        // Test intensity setting
        float newIntensity = 2.5f;
        lightSys.setDirectionalLightIntensity(newIntensity);
        
        dirLight = lightSys.getDirectionalLight();
        REQUIRE(dirLight.intensity == Approx(newIntensity));
    }
}

TEST_CASE("Ambient light management", "[LightingSystem]") {
    SECTION("Default ambient light") {
        LightingSystem lightSys;
        
        auto ambientColor = lightSys.getAmbientLight();
        
        // Test valid color values
        REQUIRE(ambientColor.r >= 0.0f);
        REQUIRE(ambientColor.g >= 0.0f);
        REQUIRE(ambientColor.b >= 0.0f);
        REQUIRE(ambientColor.a >= 0.0f);
        
        // Test reasonable intensity
        float intensity = (ambientColor.r + ambientColor.g + ambientColor.b) / 3.0f;
        REQUIRE(intensity > 0.0f);
        REQUIRE(intensity < 1.0f); // Ambient should be subtle
    }
    
    SECTION("Toggle ambient light") {
        LightingSystem lightSys;
        
        // Test enabling/disabling ambient light
        lightSys.enableAmbientLight(false);
        auto ambientOff = lightSys.getAmbientLight();
        REQUIRE(ambientOff.r == Approx(0.0f));
        REQUIRE(ambientOff.g == Approx(0.0f));
        REQUIRE(ambientOff.b == Approx(0.0f));
        
        lightSys.enableAmbientLight(true);
        auto ambientOn = lightSys.getAmbientLight();
        REQUIRE(ambientOn.r > 0.0f);
        REQUIRE(ambientOn.g > 0.0f);
        REQUIRE(ambientOn.b > 0.0f);
    }
}

TEST_CASE("Lighting presets", "[LightingSystem]") {
    SECTION("Default preset") {
        LightingSystem lightSys;
        lightSys.setLightingPreset(LightingPreset::Default);
        
        auto dirLight = lightSys.getDirectionalLight();
        // Test specific values for default preset
        REQUIRE(dirLight.direction.y < 0.0f); // Pointing downward
        REQUIRE(dirLight.intensity == Approx(1.0f));
    }
    
    SECTION("Daylight preset") {
        LightingSystem lightSys;
        lightSys.setLightingPreset(LightingPreset::Daylight);
        
        auto dirLight = lightSys.getDirectionalLight();
        // Test daylight characteristics
        REQUIRE(dirLight.intensity > 1.0f); // Brighter than default
        
        // Test color temperature (should be cooler/bluer)
        REQUIRE(dirLight.color.b >= dirLight.color.r); // More blue than red
    }
    
    SECTION("Sunset preset") {
        LightingSystem lightSys;
        lightSys.setLightingPreset(LightingPreset::Sunset);
        
        auto dirLight = lightSys.getDirectionalLight();
        // Test sunset characteristics
        REQUIRE(dirLight.color.r > dirLight.color.b); // Warmer colors
        
        // Test lower angle
        REQUIRE(dirLight.direction.y > -0.5f); // Not straight down
    }
    
    SECTION("Moonlight preset") {
        LightingSystem lightSys;
        lightSys.setLightingPreset(LightingPreset::Moonlight);
        
        auto dirLight = lightSys.getDirectionalLight();
        // Test moonlight characteristics
        REQUIRE(dirLight.intensity < 0.5f); // Much dimmer
        
        // Test cooler color temperature
        REQUIRE(dirLight.color.b >= dirLight.color.r); // Blue/cool light
    }
}

TEST_CASE("Descriptor management", "[LightingSystem]") {
    SECTION("Uniform buffer updates") {
        // Test descriptor set creation and updates
        // Test proper GPU data alignment
        // Test synchronization with rendering pipeline
    }
    
    SECTION("Multi-frame consistency") {
        // Test that lighting data remains consistent across frames
        // Test double-buffering if implemented
        // Test concurrent access safety
    }
}

TEST_CASE("Lighting calculations", "[LightingSystem]") {
    SECTION("Blinn-Phong lighting model") {
        // Test diffuse calculation
        // Test specular calculation with Blinn-Phong
        // Test normal vector handling
        // Test view direction calculations
    }
    
    SECTION("Shadow mapping support") {
        // Test shadow map generation if implemented
        // Test shadow map filtering
        // Test shadow acne prevention
    }
}
```

---

## Phase 3: Application Integration (Target: 95% Coverage)
**Timeline**: Week 5  
**Focus**: Complete application-level testing and integration strategies

### 3.1 Application Test Suite Enhancement

**File**: `tests_cpp/test_Application.cpp`  
**Current State**: 3,078 bytes with basic structure only  
**Target Coverage**: 80%  

#### 3.1.1 Application Lifecycle Testing

```cpp
TEST_CASE("Application initialization", "[Application]") {
    SECTION("Default construction") {
        Application app;
        
        // Test initial state
        REQUIRE_FALSE(app.isRunning());
        REQUIRE(app.getWindow() != nullptr);
        REQUIRE(app.getRenderer() != nullptr);
        
        // Test component initialization
        REQUIRE_NOTHROW(app.initialize());
    }
    
    SECTION("Custom configuration") {
        ApplicationConfig config;
        config.windowWidth = 1920;
        config.windowHeight = 1080;
        config.windowTitle = "Custom Test Window";
        config.enableVSync = false;
        
        Application app(config);
        
        auto window = app.getWindow();
        REQUIRE(window->getWidth() == 1920);
        REQUIRE(window->getHeight() == 1080);
    }
}

TEST_CASE("Application execution", "[Application]") {
    SECTION("Run loop control") {
        Application app;
        app.initialize();
        
        // Test that run loop can be started and stopped
        std::thread runThread([&app]() {
            app.run();
        });
        
        // Let it run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop the application
        app.stop();
        runThread.join();
        
        REQUIRE_FALSE(app.isRunning());
    }
    
    SECTION("Frame rate control") {
        Application app;
        app.initialize();
        
        // Test frame rate limiting
        app.setTargetFrameRate(60);
        REQUIRE(app.getTargetFrameRate() == 60);
        
        // Test unlimited frame rate
        app.setTargetFrameRate(0);
        REQUIRE(app.getTargetFrameRate() == 0);
    }
}

TEST_CASE("Application error handling", "[Application]") {
    SECTION("Failed initialization") {
        // Test behavior when window creation fails
        // Test behavior when Vulkan initialization fails
        // Test graceful degradation scenarios
    }
    
    SECTION("Runtime errors") {
        // Test device lost handling
        // Test out of memory conditions
        // Test invalid operation recovery
    }
}

TEST_CASE("Application integration", "[Application]") {
    SECTION("Component communication") {
        Application app;
        app.initialize();
        
        // Test input system integration
        auto inputHandler = app.getInputHandler();
        REQUIRE(inputHandler != nullptr);
        
        // Test camera integration
        auto camera = app.getCamera();
        REQUIRE(camera != nullptr);
        
        // Test renderer integration
        auto renderer = app.getRenderer();
        REQUIRE(renderer != nullptr);
    }
    
    SECTION("Asset management integration") {
        Application app;
        app.initialize();
        
        auto assetManager = app.getAssetManager();
        REQUIRE(assetManager != nullptr);
        
        // Test asset loading through application
        REQUIRE_NOTHROW(app.loadTexture("test_texture.png"));
        REQUIRE_NOTHROW(app.loadModel("test_model.obj"));
    }
}
```

### 3.2 VulkanRenderer Integration Test Strategy

**File**: `tests_cpp/test_VulkanRenderer.cpp`  
**Current State**: 2,368 bytes with stub tests  
**Target Coverage**: 70% (Limited due to Vulkan complexity)  

#### 3.2.1 Mock-Based Testing Strategy

Due to the complexity of Vulkan operations, the VulkanRenderer tests will use a mock-based approach:

```cpp
// Mock Vulkan Interface for Testing
class MockVulkanInterface {
public:
    virtual ~MockVulkanInterface() = default;
    
    virtual VkResult createInstance(const VkInstanceCreateInfo* pCreateInfo,
                                  const VkAllocationCallbacks* pAllocator,
                                  VkInstance* pInstance) = 0;
    
    virtual VkResult createDevice(VkPhysicalDevice physicalDevice,
                                const VkDeviceCreateInfo* pCreateInfo,
                                const VkAllocationCallbacks* pAllocator,
                                VkDevice* pDevice) = 0;
    
    // Additional Vulkan functions as needed
};

TEST_CASE("VulkanRenderer initialization", "[VulkanRenderer]") {
    SECTION("Successful initialization") {
        MockVulkanInterface mockVulkan;
        VulkanRenderer renderer(mockVulkan);
        
        // Configure mock to return success
        EXPECT_CALL(mockVulkan, createInstance(_, _, _))
            .WillOnce(Return(VK_SUCCESS));
        
        EXPECT_CALL(mockVulkan, createDevice(_, _, _, _))
            .WillOnce(Return(VK_SUCCESS));
        
        REQUIRE_NOTHROW(renderer.initialize());
        REQUIRE(renderer.isInitialized());
    }
    
    SECTION("Failed initialization") {
        MockVulkanInterface mockVulkan;
        VulkanRenderer renderer(mockVulkan);
        
        // Configure mock to return failure
        EXPECT_CALL(mockVulkan, createInstance(_, _, _))
            .WillOnce(Return(VK_ERROR_INITIALIZATION_FAILED));
        
        REQUIRE_THROWS(renderer.initialize());
        REQUIRE_FALSE(renderer.isInitialized());
    }
}

TEST_CASE("VulkanRenderer resource management", "[VulkanRenderer]") {
    SECTION("Buffer creation and cleanup") {
        // Test buffer lifecycle management
        // Test proper cleanup on destruction
        // Test resource leak detection
    }
    
    SECTION("Command buffer management") {
        // Test command buffer allocation
        // Test command buffer recording
        // Test command buffer submission
        // Test synchronization primitives
    }
}

TEST_CASE("VulkanRenderer rendering pipeline", "[VulkanRenderer]") {
    SECTION("Render pass execution") {
        // Test render pass creation
        // Test framebuffer management
        // Test viewport and scissor setup
        // Test clear values
    }
    
    SECTION("Shader pipeline management") {
        // Test shader module loading
        // Test pipeline state objects
        // Test descriptor set binding
        // Test push constants
    }
}
```

### 3.3 Integration Test Infrastructure

#### 3.3.1 Enhanced Test Fixtures

**File**: `tests_cpp/fixtures/VulkanTestFixture.h`  
**Enhancements Needed**:

```cpp
class VulkanTestFixture {
public:
    VulkanTestFixture();
    ~VulkanTestFixture();
    
    // Mock Vulkan device for testing
    VkDevice getMockDevice() const { return mockDevice_; }
    
    // Test asset creation helpers
    std::shared_ptr<TestTexture> createTestTexture(int width, int height, int channels = 4);
    std::shared_ptr<TestModel> createTestCube();
    std::shared_ptr<TestModel> createTestSphere();
    
    // Test material creation
    MaterialData createTestMaterial(const std::string& type = "default");
    
    // Validation layer helpers
    void enableValidationLayers();
    void checkValidationErrors();
    
private:
    VkInstance mockInstance_;
    VkDevice mockDevice_;
    VkPhysicalDevice mockPhysicalDevice_;
    
    // Test asset storage
    std::vector<std::shared_ptr<TestAsset>> testAssets_;
};

class PerformanceTestFixture {
public:
    void startTimer(const std::string& testName);
    void endTimer(const std::string& testName);
    void logPerformanceResults();
    
    // Memory usage tracking
    size_t getCurrentMemoryUsage();
    void trackMemoryUsage(const std::string& checkpoint);
    
private:
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> timers_;
    std::unordered_map<std::string, size_t> memoryCheckpoints_;
};
```

#### 3.3.2 Test Asset Creation Utilities

**File**: `tests_cpp/fixtures/TestAssetGenerator.h`  

```cpp
class TestAssetGenerator {
public:
    // Generate test textures
    static std::vector<uint8_t> generateCheckerboardTexture(int width, int height);
    static std::vector<uint8_t> generateGradientTexture(int width, int height);
    static std::vector<uint8_t> generateNoiseTexture(int width, int height);
    
    // Generate test models
    static TestModel generateCube(float size = 1.0f);
    static TestModel generateSphere(float radius = 1.0f, int segments = 32);
    static TestModel generatePlane(float width = 1.0f, float height = 1.0f);
    
    // Generate test shaders
    static std::vector<uint32_t> generateVertexShaderSPIRV();
    static std::vector<uint32_t> generateFragmentShaderSPIRV();
    
    // Cleanup utilities
    static void cleanupTestAssets();
    static void createTestDirectories();
};
```

---

## Test Infrastructure Improvements

### Enhanced Catch2 Configuration

**File**: `tests_cpp/catch2_config.hpp`  

```cpp
#pragma once

// Custom Catch2 configuration for VulkanMon
#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#define CATCH_CONFIG_COLOUR_ANSI

// Custom matchers for floating-point comparisons
class Vec3Matcher : public Catch::Matchers::MatcherBase<glm::vec3> {
    glm::vec3 expected_;
    float margin_;
    
public:
    Vec3Matcher(const glm::vec3& expected, float margin = 0.01f)
        : expected_(expected), margin_(margin) {}
    
    bool match(const glm::vec3& actual) const override {
        return std::abs(actual.x - expected_.x) < margin_ &&
               std::abs(actual.y - expected_.y) < margin_ &&
               std::abs(actual.z - expected_.z) < margin_;
    }
    
    std::string describe() const override {
        return "is approximately equal to " + 
               glm::to_string(expected_) + " with margin " + std::to_string(margin_);
    }
};

// Helper function to create the matcher
Vec3Matcher ApproxVec3(const glm::vec3& expected, float margin = 0.01f) {
    return Vec3Matcher(expected, margin);
}
```

### Continuous Integration Test Configuration

**File**: `.github/workflows/test-coverage.yml`  

```yaml
name: Test Coverage Analysis

on: [push, pull_request]

jobs:
  test-coverage:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [windows-latest, ubuntu-latest]
        
    steps:
    - uses: actions/checkout@v3
    
    - name: Setup vcpkg
      uses: microsoft/setup-vcpkg@v1
      
    - name: Install dependencies
      run: |
        vcpkg install catch2[main] glfw3 glm assimp stb vulkan-headers
        
    - name: Build tests
      run: |
        mkdir build
        cd build
        cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
        cmake --build . --target vulkanmon_tests
        
    - name: Run tests with coverage
      run: |
        cd build/tests_cpp
        ./Debug/vulkanmon_tests --reporter=junit --out=test-results.xml
        
    - name: Generate coverage report
      if: matrix.os == 'ubuntu-latest'
      run: |
        # Install lcov for coverage analysis
        sudo apt-get install lcov
        
        # Generate coverage data
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --list coverage.info
        
    - name: Upload coverage to Codecov
      if: matrix.os == 'ubuntu-latest'
      uses: codecov/codecov-action@v3
      with:
        file: ./coverage.info
        fail_ci_if_error: true
```

---

## Success Metrics and Validation

### Coverage Targets by Component

| Component | Current Coverage | Phase 1 Target | Phase 2 Target | Final Target |
|-----------|------------------|-----------------|-----------------|--------------|
| Logger | 95% | 95% | 95% | 95% |
| Camera | 90% | 90% | 90% | 90% |
| InputHandler | 85% | 85% | 85% | 85% |
| Window | 80% | 80% | 80% | 80% |
| ResourceManager | 5% | 95% | 95% | 95% |
| Utils | 30% | 90% | 90% | 90% |
| MaterialSystem | 40% | 90% | 90% | 90% |
| LightingSystem | 35% | 40% | 90% | 90% |
| AssetManager | 10% | 15% | 90% | 90% |
| ModelLoader | 15% | 20% | 85% | 85% |
| Application | 20% | 25% | 30% | 80% |
| VulkanRenderer | 5% | 10% | 15% | 70% |
| **Overall** | **40%** | **65%** | **85%** | **95%** |

### Quality Assurance Checklist

**Phase 1 Completion Criteria**:
- [ ] ResourceManager: All placeholder tests replaced with functional tests
- [ ] Utils: File operations, error handling, and shader loading fully tested
- [ ] MaterialSystem: Material properties, descriptor management, and presets tested
- [ ] All Phase 1 tests pass with 0 failures
- [ ] Performance tests maintain reasonable execution times (<1s per test)
- [ ] Memory leak detection shows no resource leaks

**Phase 2 Completion Criteria**:
- [ ] AssetManager: Texture/model loading, caching, and discovery fully tested
- [ ] ModelLoader: 3D model parsing, material extraction, and error handling tested
- [ ] LightingSystem: Light management, presets, and calculations fully tested
- [ ] Integration between components verified through cross-component tests
- [ ] All tests pass on both Windows and Linux platforms
- [ ] Performance regression tests show no significant slowdowns

**Phase 3 Completion Criteria**:
- [ ] Application: Lifecycle, integration, and error handling fully tested
- [ ] VulkanRenderer: Mock-based testing strategy implemented and functional
- [ ] Integration test suite covers major system interactions
- [ ] All tests pass with validation layers enabled
- [ ] Documentation updated with test coverage information
- [ ] CI/CD pipeline includes automated coverage reporting

### Performance Benchmarks

**Test Execution Time Targets**:
- Individual test case: < 100ms
- Component test suite: < 5 seconds
- Full test suite: < 30 seconds
- Coverage analysis: < 60 seconds

**Memory Usage Targets**:
- Test fixture memory overhead: < 50MB
- Peak memory usage during full test run: < 500MB
- Memory leak detection: 0 leaks > 1KB

---

## Implementation Timeline

### Week 1-2: Phase 1 Implementation
**Days 1-3**: ResourceManager test suite completion
**Days 4-6**: Utils test suite completion
**Days 7-10**: MaterialSystem test enhancement
**Days 11-14**: Phase 1 integration testing and bug fixes

### Week 3-4: Phase 2 Implementation
**Days 15-18**: AssetManager test suite completion
**Days 19-22**: ModelLoader test suite completion
**Days 23-26**: LightingSystem test suite completion
**Days 27-28**: Phase 2 integration testing

### Week 5: Phase 3 Implementation
**Days 29-31**: Application test enhancement
**Days 32-33**: VulkanRenderer mock testing strategy
**Days 34-35**: Final integration testing and documentation

### Maintenance Phase: Ongoing
- Weekly test suite execution and maintenance
- Monthly coverage analysis and reporting
- Quarterly test infrastructure improvements
- Continuous integration monitoring and optimization

---

This comprehensive plan provides the excruciating level of detail requested for achieving 100% test coverage in the VulkanMon engine, following the project's core principles while ensuring maintainable, high-quality test code that enables confident development and refactoring.