# VulkanMon Phase 2.5 - Developer Experience Features

## Overview

Phase 2.5 adds interactive development features to accelerate workflow and make the 3D foundation more engaging. These features transform our static 3D renderer into an interactive development environment.

## Feature 1: WASD Camera Movement

### Current State
- Fixed camera position at `(2.0f, 2.0f, 2.0f)` looking at origin
- View matrix calculated once in `updateUniformBuffer()`
- Static viewing angle provides consistent perspective

### Implementation Plan
```cpp
// Add camera state variables to class
glm::vec3 cameraPos = glm::vec3(2.0f, 2.0f, 2.0f);
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 0.0f, 1.0f);
float cameraSpeed = 2.5f; // units per second

// In mainLoop(), add key polling
void handleCameraInput() {
    float deltaTime = 0.016f; // Assume 60fps for now
    float velocity = cameraSpeed * deltaTime;
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += velocity * normalize(cameraTarget - cameraPos);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)  
        cameraPos -= velocity * normalize(cameraTarget - cameraPos);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= velocity * normalize(cross(cameraTarget - cameraPos, cameraUp));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += velocity * normalize(cross(cameraTarget - cameraPos, cameraUp));
}

// In updateUniformBuffer(), use dynamic camera position
ubo.view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
```

### Benefits
- Interactive 3D exploration of the cube
- Foundation for Phase 3 scene navigation
- Immediate visual engagement and testing
- Understanding of 3D spatial relationships

## Feature 2: Hot Shader Reloading

### Current State  
- Shaders compiled once at startup via `createShaderModules()`
- Changes require full application restart
- Development iteration cycle: edit → compile → restart → test

### Implementation Plan
```cpp
// Add key callback setup in initWindow()
glfwSetKeyCallback(window, keyCallback);

// Static key callback function
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    HelloTriangleApp* app = reinterpret_cast<HelloTriangleApp*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        app->reloadShaders();
    }
}

// New shader reloading function
void reloadShaders() {
    std::cout << "Recompiling shaders..." << std::endl;
    
    // 1. Recompile shader files
    bool success = recompileShaderFiles();
    if (!success) {
        std::cout << "Shader compilation failed, keeping current shaders." << std::endl;
        return;
    }
    
    // 2. Wait for device to be idle
    vkDeviceWaitIdle(device);
    
    // 3. Destroy old pipeline and shader modules
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    
    // 4. Recreate shader modules and pipeline
    createShaderModules();
    createGraphicsPipeline();
    
    std::cout << "Shaders reloaded successfully!" << std::endl;
}

bool recompileShaderFiles() {
    // Execute glslc commands
    int vertResult = system("cd shaders && glslc triangle.vert -o vert.spv");
    int fragResult = system("cd shaders && glslc triangle.frag -o frag.spv");
    
    return (vertResult == 0 && fragResult == 0);
}
```

### Benefits  
- Instant shader iteration without restart
- Rapid visual feedback for shader experiments  
- Massive development velocity boost
- Foundation for Phase 3 material/lighting work

## Technical Considerations

### Performance
- Camera movement: Minimal impact, just matrix recalculation
- Hot reloading: Brief pause during pipeline recreation (~100ms)
- Both features maintain 60fps target

### Error Handling
- Shader compilation failure: Keep existing pipeline, show console message
- Invalid camera positions: Clamp to reasonable bounds
- GLFW callback safety: Store app instance pointer

### Testing Strategy
- Unit tests for camera movement math
- Integration tests for shader recompilation
- Manual testing for user experience
- Performance regression testing

### Future Extensions
- Mouse look camera rotation
- Multiple shader reloading (compute, geometry shaders)  
- Camera presets and bookmarking
- Shader error overlay display

## Success Criteria

✅ **WASD Camera Movement**
- Smooth 60fps camera movement
- Intuitive WASD controls (W=forward, S=back, A=left, D=right)
- Camera stays focused on cube during movement
- No visual artifacts or jitter

✅ **Hot Shader Reloading**  
- 'R' key triggers successful recompilation
- Failed compilation preserves existing shaders
- Console feedback on success/failure
- No memory leaks from repeated reloading

## Timeline
- **Setup & Documentation**: 10 minutes ✅
- **WASD Implementation**: 20-30 minutes 
- **Hot Reloading Implementation**: 30-45 minutes
- **Testing & Polish**: 15 minutes
- **Total Estimated**: ~90 minutes

---

*Phase 2.5 transforms our foundation into an interactive development powerhouse!*