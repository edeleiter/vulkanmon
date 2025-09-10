# Step 32: Basic Directional Lighting System - Implementation Guide

## Overview

Transform our flat 3D textured cube into a properly lit 3D scene with sun/moon directional lighting. This is the first step of Phase 4 and will provide immediate dramatic visual improvement.

**Branch**: `lighting-system`  
**Estimated Time**: 2-3 hours  
**Visual Target**: Textured cube with clear light/shadow definition, controllable light direction

## Current State Analysis

### What We Have
- Complete 3D pipeline with textured cube
- Hot shader reloading system (R key)
- Interactive camera controls (WASD)
- Resource management and asset loading
- Professional logging system

### What We Need to Add
- DirectionalLight structure and management
- LightingSystem class for light state
- Updated shaders with Blinn-Phong lighting calculations
- Keyboard controls for light direction
- Light direction uniforms in UBO

## Implementation Plan

### Step 1: Create Lighting System Architecture

**New Files to Create:**
```cpp
// src/LightingSystem.h
#pragma once
#include <glm/glm.hpp>

namespace VulkanMon {
    struct DirectionalLight {
        glm::vec3 direction = glm::vec3(-0.5f, -1.0f, -0.3f);  // Default sun angle
        glm::vec3 color = glm::vec3(1.0f, 0.95f, 0.8f);       // Warm sunlight
        float intensity = 1.0f;
        
        // Normalized direction vector for shaders
        glm::vec3 getNormalizedDirection() const;
    };

    class LightingSystem {
    public:
        LightingSystem();
        
        // Light management
        void setDirectionalLight(const DirectionalLight& light);
        const DirectionalLight& getDirectionalLight() const;
        
        // Runtime controls
        void processInput(GLFWwindow* window, float deltaTime);
        void rotateSunAngle(float deltaRadians);
        
        // Uniform data for shaders
        struct LightingUniforms {
            glm::vec3 lightDirection;
            float pad1;
            glm::vec3 lightColor;
            float lightIntensity;
        };
        
        LightingUniforms getUniformData() const;
        
    private:
        DirectionalLight directionalLight_;
        float rotationSpeed_ = 1.0f; // radians per second
    };
}
```

**Implementation File:**
```cpp
// src/LightingSystem.cpp
#include "LightingSystem.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace VulkanMon {
    glm::vec3 DirectionalLight::getNormalizedDirection() const {
        return glm::normalize(direction);
    }
    
    LightingSystem::LightingSystem() {
        // Initialize with pleasant default sun lighting
        directionalLight_.direction = glm::vec3(-0.5f, -1.0f, -0.3f);
        directionalLight_.color = glm::vec3(1.0f, 0.95f, 0.8f);
        directionalLight_.intensity = 1.0f;
    }
    
    void LightingSystem::processInput(GLFWwindow* window, float deltaTime) {
        float rotationAmount = rotationSpeed_ * deltaTime;
        
        // Arrow keys control sun rotation
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            rotateSunAngle(-rotationAmount);
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            rotateSunAngle(rotationAmount);
        }
    }
    
    void LightingSystem::rotateSunAngle(float deltaRadians) {
        // Rotate around Y-axis (horizontal sun movement)
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), deltaRadians, glm::vec3(0, 1, 0));
        glm::vec4 rotatedDir = rotation * glm::vec4(directionalLight_.direction, 0.0f);
        directionalLight_.direction = glm::vec3(rotatedDir);
    }
    
    LightingSystem::LightingUniforms LightingSystem::getUniformData() const {
        LightingUniforms uniforms{};
        uniforms.lightDirection = directionalLight_.getNormalizedDirection();
        uniforms.lightColor = directionalLight_.color;
        uniforms.lightIntensity = directionalLight_.intensity;
        return uniforms;
    }
}
```

### Step 2: Update Uniform Buffer Object

**Modify main.cpp UBO structure:**
```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    
    // NEW: Lighting uniforms
    glm::vec3 lightDirection;
    float pad1;
    glm::vec3 lightColor;
    float lightIntensity;
};
```

### Step 3: Update Shaders for Lighting

**Update shaders/triangle.vert:**
```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    
    // Lighting uniforms
    vec3 lightDirection;
    float pad1;
    vec3 lightColor;
    float lightIntensity;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// NEW: Pass normal to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;

void main() {
    // Transform position
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // Pass data to fragment shader
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
    
    // Transform normal (assuming uniform scaling for now)
    fragNormal = mat3(ubo.model) * calculateNormal(inPosition);
}

// Calculate normal from position (for cube faces)
vec3 calculateNormal(vec3 position) {
    // Simple approach: use the dominant axis as normal
    vec3 absPos = abs(position);
    if (absPos.x >= absPos.y && absPos.x >= absPos.z) {
        return vec3(sign(position.x), 0, 0);
    } else if (absPos.y >= absPos.z) {
        return vec3(0, sign(position.y), 0);
    } else {
        return vec3(0, 0, sign(position.z));
    }
}
```

**Update shaders/triangle.frag:**
```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    
    // Lighting uniforms
    vec3 lightDirection;
    float pad1;
    vec3 lightColor;
    float lightIntensity;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture
    vec3 textureColor = texture(texSampler, fragTexCoord).rgb;
    
    // Normalize inputs
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-ubo.lightDirection);
    
    // Simple Lambertian diffuse lighting
    float diffuseFactor = max(dot(normal, lightDir), 0.0);
    
    // Ambient lighting to prevent complete darkness
    vec3 ambientLight = vec3(0.1, 0.1, 0.1);
    
    // Combine lighting
    vec3 finalColor = textureColor * (ambientLight + ubo.lightColor * ubo.lightIntensity * diffuseFactor);
    
    outColor = vec4(finalColor, 1.0);
}
```

### Step 4: Integration with Main Application

**Add to main.cpp:**
```cpp
#include "LightingSystem.h"

class HelloTriangleApp {
private:
    // Add lighting system
    std::unique_ptr<LightingSystem> lightingSystem_;
    
    // Update UBO with lighting data
    void updateUniformBuffer(uint32_t currentImage) {
        // ... existing MVP matrix code ...
        
        // NEW: Add lighting uniforms
        auto lightingUniforms = lightingSystem_->getUniformData();
        ubo.lightDirection = lightingUniforms.lightDirection;
        ubo.lightColor = lightingUniforms.lightColor;  
        ubo.lightIntensity = lightingUniforms.lightIntensity;
        
        // ... rest of UBO update ...
    }
    
    // Process lighting input
    void processInput() {
        camera_.processInput(window);
        lightingSystem_->processInput(window, deltaTime);
    }
};
```

## Testing Strategy

### Unit Tests to Add
```cpp
// tests_cpp/test_LightingSystem.cpp
TEST_CASE("DirectionalLight Math", "[Lighting][Math]") {
    DirectionalLight light;
    light.direction = glm::vec3(1.0f, -1.0f, 0.0f);
    
    glm::vec3 normalized = light.getNormalizedDirection();
    REQUIRE(glm::length(normalized) == Approx(1.0f));
}

TEST_CASE("LightingSystem Uniform Generation", "[Lighting][Uniforms]") {
    LightingSystem lighting;
    auto uniforms = lighting.getUniformData();
    
    // Validate uniform structure
    REQUIRE(glm::length(uniforms.lightDirection) == Approx(1.0f));
    REQUIRE(uniforms.lightIntensity > 0.0f);
}
```

### Manual Testing Checklist
- [ ] Application compiles and runs without errors
- [ ] Cube shows clear lighting with bright and dark sides
- [ ] Left/Right arrow keys rotate sun direction
- [ ] Hot shader reloading (R key) works with new lighting shaders
- [ ] Performance maintains 60+ FPS
- [ ] Lighting appears natural and realistic

## Success Criteria

**Visual Results:**
- Textured cube has clear bright and dark sides based on light direction
- Light direction controllable with arrow keys
- Natural-looking diffuse lighting with subtle ambient lighting

**Technical Results:**  
- No performance regression (maintains 60+ FPS)
- Hot shader reloading works with new lighting shaders
- Code follows existing architecture patterns
- Proper integration with logging system

**Development Experience:**
- Fast iteration with hot reloading for lighting experiments
- Clear keyboard controls for testing different lighting angles
- Foundation established for next lighting steps (point lights, shadows)

## Next Steps After Step 32

Once directional lighting is working:
1. **Step 33**: Add point lights with attenuation
2. **Step 34**: Add spot lights with cone controls
3. **Step 35**: Begin PBR material system

The directional lighting foundation will make all subsequent lighting features much easier to implement and test.

---

## ✅ IMPLEMENTATION COMPLETE

**Status**: ✅ **COMPLETED**  
**Date**: September 10, 2025  
**Branch**: `core-engine-systems`

### What Was Actually Implemented

The Step 32 implementation was successfully completed with some architectural improvements over the original plan:

#### 1. Enhanced Lighting System Architecture
- **LightingSystem.h/cpp**: Complete RAII-managed lighting system
- **DirectionalLight + LightingData structures**: GPU-aligned with proper padding
- **Vulkan descriptor set management**: Full UBO binding and uniform management
- **ResourceManager integration**: Proper RAII buffer management with ManagedBuffer

#### 2. Interactive Controls Implemented
- **1 Key**: Bright daylight preset (intensity 1.5, white light)
- **2 Key**: Golden hour preset (intensity 1.2, warm golden light)  
- **3 Key**: Moonlight preset (intensity 0.5, cool blue light)
- **4 Key**: Dramatic side lighting preset (intensity 1.8, warm light)
- **L Key**: Toggle between different ambient lighting levels

#### 3. Shader System Updates
- **triangle.vert**: Added normal calculation and lighting uniform binding
- **triangle.frag**: Complete directional + ambient lighting with proper Lambertian diffuse
- **Descriptor Set Layout**: Updated to include lighting uniforms at binding slot 2
- **SPIR-V compilation**: Integrated with existing hot-reload system (R key)

#### 4. Technical Architecture Improvements
- **RAII Resource Management**: Proper integration with ResourceManager system
- **GPU Memory Alignment**: Correct vec3 + float padding for uniform buffer layout
- **Error Handling**: Complete validation and error reporting
- **Performance**: Maintains 60+ FPS with no regressions

#### 5. Build System Integration
- **CMakeLists.txt**: Updated with LightingSystem.cpp
- **Namespace Organization**: Proper VulkanMon:: namespace usage
- **Header Dependencies**: Clean forward declarations and minimal includes

### Key Architectural Decisions

1. **Improved Over Original Plan**: Used ResourceManager's ManagedBuffer system instead of manual Vulkan resource management
2. **Enhanced Controls**: Implemented preset-based lighting system instead of just arrow key controls
3. **Better GPU Layout**: Used proper LightingData structure with explicit padding for GPU alignment
4. **Descriptor Set Integration**: Full Vulkan descriptor set management with proper binding slots

### Testing Results

- ✅ **Build Success**: Compiles cleanly without warnings
- ✅ **Runtime Stability**: No crashes or validation layer errors
- ✅ **Performance**: Maintains 60+ FPS
- ✅ **Interactive Controls**: All lighting presets work correctly
- ✅ **Hot Reloading**: Shader recompilation works with new lighting system
- ✅ **Visual Quality**: Clear directional lighting with ambient contribution

### Files Modified/Created

**New Files:**
- `src/LightingSystem.h` - Complete lighting system interface
- `src/LightingSystem.cpp` - Full Vulkan lighting implementation

**Modified Files:**
- `src/ResourceManager.h` - Added getDevice() method
- `shaders/triangle.vert` - Added lighting uniforms and normal calculation
- `shaders/triangle.frag` - Added directional + ambient lighting calculations  
- `src/main.cpp` - Integrated LightingSystem with descriptor sets and controls
- `CMakeLists.txt` - Added LightingSystem.cpp to build

### Next Steps Ready
With Step 32 complete, the foundation is now ready for:
1. **Step 33**: Point lights with attenuation
2. **Step 34**: Spot lights with cone controls  
3. **Step 35**: PBR material system

---

*This implementation follows VulkanMon's philosophy: "Simple is Powerful" - one lighting type at a time, building on our solid foundation.*