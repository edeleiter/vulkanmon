# InputHandler ECS Integration Plan

## Mission: Replace Old Camera WASD Controls with ECS Camera Entity Movement

**Current State**: InputHandler disabled during Phase 4 camera unification
**Goal**: Full WASD movement integration with ECS camera entities
**Priority**: Medium (camera unification complete, this enhances UX)

## Problem Analysis

### Current InputHandler Dependencies
```cpp
// InputHandler.h - BROKEN DEPENDENCIES
#include "Camera.h"  // ❌ File deleted
InputHandler(std::shared_ptr<Camera> camera, std::shared_ptr<Window> window);  // ❌ Constructor broken
std::shared_ptr<Camera> camera_;  // ❌ Member variable broken

// InputHandler.cpp - EXPECTED METHODS (need investigation)
camera_->moveForward(deltaTime);   // ❌ Method doesn't exist in ECS Camera
camera_->moveBackward(deltaTime);  // ❌ Method doesn't exist in ECS Camera
camera_->strafeLeft(deltaTime);    // ❌ Method doesn't exist in ECS Camera
camera_->strafeRight(deltaTime);   // ❌ Method doesn't exist in ECS Camera
camera_->rotate(deltaX, deltaY);   // ❌ Method doesn't exist in ECS Camera
```

### Required ECS Integration
```cpp
// NEW ARCHITECTURE NEEDED
EntityManager* entityManager_;           // Access to ECS entities
CameraSystem* cameraSystem_;            // Access to active camera entity
EntityID activeCameraEntity_;           // Current camera being controlled

// WASD movement should modify:
Transform& cameraTransform = entityManager_->getComponent<Transform>(activeCameraEntity_);
cameraTransform.position += movement;    // Update ECS Transform component
cameraTransform.rotation += rotation;   // Update ECS Transform rotation
```

## Implementation Plan

### Phase 1: InputHandler Constructor Refactoring
**File**: `src/core/InputHandler.h`
```cpp
// OLD CONSTRUCTOR (broken)
InputHandler(std::shared_ptr<Camera> camera, std::shared_ptr<Window> window);

// NEW CONSTRUCTOR (ECS-compatible)
InputHandler(
    std::shared_ptr<Window> window,
    std::shared_ptr<CameraSystem> cameraSystem,
    EntityManager* entityManager  // Raw pointer - lifetime managed by World
);
```

**File**: `src/core/InputHandler.cpp`
```cpp
// NEW MEMBER VARIABLES
std::shared_ptr<CameraSystem> cameraSystem_;
EntityManager* entityManager_;

// NEW CONSTRUCTOR IMPLEMENTATION
InputHandler::InputHandler(
    std::shared_ptr<Window> window,
    std::shared_ptr<CameraSystem> cameraSystem,
    EntityManager* entityManager
) : window_(window), cameraSystem_(cameraSystem), entityManager_(entityManager) {
    // Constructor body
}
```

### Phase 2: Movement Method Implementation
**File**: `src/core/InputHandler.cpp`

#### Step 2.1: Replace Camera Movement Methods
```cpp
// OLD METHODS (investigate actual method names first!)
void InputHandler::processInput(float deltaTime) {
    // ❌ BROKEN - Replace these calls
    if (/* W key */) camera_->moveForward(deltaTime);
    if (/* S key */) camera_->moveBackward(deltaTime);
    if (/* A key */) camera_->strafeLeft(deltaTime);
    if (/* D key */) camera_->strafeRight(deltaTime);
}

// NEW METHODS (ECS Transform-based)
void InputHandler::processInput(float deltaTime) {
    EntityID activeCameraEntity = cameraSystem_->getActiveCameraEntity();
    if (activeCameraEntity == INVALID_ENTITY) return;

    Transform& cameraTransform = entityManager_->getComponent<Transform>(activeCameraEntity);

    // WASD movement using Transform component
    if (/* W key */) {
        glm::vec3 forward = cameraTransform.getForward();
        cameraTransform.position += forward * speed * deltaTime;
    }
    if (/* S key */) {
        glm::vec3 forward = cameraTransform.getForward();
        cameraTransform.position -= forward * speed * deltaTime;
    }
    if (/* A key */) {
        glm::vec3 right = cameraTransform.getRight();
        cameraTransform.position -= right * speed * deltaTime;
    }
    if (/* D key */) {
        glm::vec3 right = cameraTransform.getRight();
        cameraTransform.position += right * speed * deltaTime;
    }
    if (/* Q key */) {
        cameraTransform.position.y += speed * deltaTime;  // Move up
    }
    if (/* E key */) {
        cameraTransform.position.y -= speed * deltaTime;  // Move down
    }
}
```

#### Step 2.2: Mouse Look Implementation
```cpp
// OLD MOUSE LOOK (investigate actual method)
void InputHandler::processMouseInput(double xpos, double ypos) {
    // ❌ BROKEN
    camera_->rotate(deltaX, deltaY);
}

// NEW MOUSE LOOK (ECS Transform-based)
void InputHandler::processMouseInput(double xpos, double ypos) {
    EntityID activeCameraEntity = cameraSystem_->getActiveCameraEntity();
    if (activeCameraEntity == INVALID_ENTITY) return;

    Transform& cameraTransform = entityManager_->getComponent<Transform>(activeCameraEntity);

    // Calculate mouse delta
    float deltaX = static_cast<float>(xpos - lastMouseX_);
    float deltaY = static_cast<float>(lastMouseY_ - ypos);  // Reversed Y

    // Apply mouse sensitivity
    deltaX *= mouseSensitivity_;
    deltaY *= mouseSensitivity_;

    // Update rotation (Euler angles in Transform)
    cameraTransform.rotation.y += deltaX;  // Yaw
    cameraTransform.rotation.x += deltaY;  // Pitch

    // Clamp pitch to prevent over-rotation
    cameraTransform.rotation.x = glm::clamp(cameraTransform.rotation.x, -89.0f, 89.0f);

    // Store current mouse position
    lastMouseX_ = xpos;
    lastMouseY_ = ypos;
}
```

### Phase 3: Application Integration
**File**: `src/core/ApplicationSetup.cpp`

#### Step 3.1: Update InputHandler Creation
```cpp
// OLD CREATION (currently commented out)
// inputHandler_ = std::make_unique<InputHandler>(camera_, window_);

// NEW CREATION (ECS-compatible)
inputHandler_ = std::make_unique<InputHandler>(
    window_,
    cameraSystem_,          // Already available from ECS initialization
    &world_->getEntityManager()  // Raw pointer to EntityManager
);
```

#### Step 3.2: Restore Input Callbacks
```cpp
// Currently commented out - restore these:
window_->setKeyCallback([this](int key, int scancode, int action, int mods) {
    if (inputHandler_) {  // Safety check
        inputHandler_->processKeyInput(key, scancode, action, mods);
    }
});

window_->setMouseCallback([this](double xpos, double ypos) {
    if (inputHandler_) {  // Safety check
        inputHandler_->processMouseInput(xpos, ypos);
    }
});

// System control callbacks
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
```

### Phase 4: Configuration Integration
**File**: `src/core/InputHandler.h`
```cpp
// Add camera speed from unified config
#include "../config/CameraConfig.h"

// Use unified camera speed
private:
    float cameraSpeed_ = Config::Camera::DEFAULT_SPEED;  // 2.5f from CameraConfig
    float mouseSensitivity_ = 0.1f;  // Could also move to CameraConfig
```

### Phase 5: Build System Cleanup
**File**: `CMakeLists.txt` (investigate current structure)

#### Step 5.1: Remove Old Camera References
```cmake
# Remove these lines (investigate exact file):
src/core/Camera.cpp           # ❌ File deleted
tests_cpp/test_Camera.cpp     # ❌ File deleted
```

#### Step 5.2: Verify InputHandler Compilation
```bash
# Test build after InputHandler integration
cd build && cmake --build .
```

### Phase 6: Testing and Validation
**File**: Manual testing required

#### Step 6.1: WASD Movement Testing
- [ ] **W/S keys**: Camera moves forward/backward relative to view direction
- [ ] **A/D keys**: Camera strafes left/right relative to view direction
- [ ] **Q/E keys**: Camera moves up/down in world coordinates
- [ ] **Movement speed**: Consistent with `Config::Camera::DEFAULT_SPEED`
- [ ] **Smooth movement**: No jitter or lag during movement

#### Step 6.2: Mouse Look Testing
- [ ] **Mouse horizontal**: Camera yaw (left/right rotation) works smoothly
- [ ] **Mouse vertical**: Camera pitch (up/down rotation) works smoothly
- [ ] **Pitch clamping**: Cannot over-rotate vertically (±89° limit)
- [ ] **Mouse sensitivity**: Reasonable response to mouse movement
- [ ] **No drift**: Camera doesn't move when mouse is stationary

#### Step 6.3: System Integration Testing
- [ ] **Unified matrices**: Camera movement updates both spatial culling AND rendering
- [ ] **Performance**: 60+ FPS maintained during camera movement
- [ ] **ECS consistency**: Transform changes reflected in CameraSystem matrices
- [ ] **Inspector integration**: ECS Inspector shows updated Transform values

#### Step 6.4: Control Testing
- [ ] **R key**: Shader reload still works
- [ ] **Material controls**: Material switching still works
- [ ] **Lighting controls**: Lighting changes still work
- [ ] **I key**: Inspector toggle still works

## Success Criteria

### ✅ Technical Success
- [ ] **WASD movement**: Smooth camera movement in all directions
- [ ] **Mouse look**: Responsive camera rotation without drift
- [ ] **ECS integration**: Camera movement modifies Transform components
- [ ] **Matrix consistency**: Same camera data used for culling and rendering
- [ ] **Performance**: 60+ FPS during active camera movement
- [ ] **No regressions**: All existing controls continue working

### ✅ Architecture Success
- [ ] **Clean dependencies**: InputHandler uses ECS systems, not old Camera class
- [ ] **Configuration consistency**: Uses unified `Config::Camera` constants
- [ ] **Future-ready**: Easy to add multiple camera switching, camera constraints
- [ ] **Well-documented**: Clear code comments explaining ECS Transform usage

## Risk Mitigation

### Potential Issues
1. **Mouse capture**: Ensure mouse is properly captured for look controls
2. **Delta time accuracy**: Ensure smooth movement regardless of framerate
3. **Transform update timing**: Ensure Transform updates happen before matrix calculation
4. **Active camera changes**: Handle gracefully when active camera entity changes

### Rollback Plan
If integration fails:
1. Restore InputHandler to disabled state (current Phase 4 state)
2. Camera unification remains functional without movement controls
3. Document issues for future resolution
4. Consider alternative approaches (e.g., different movement implementation)

## Investigation Required

### Unknown Implementation Details
1. **Current InputHandler methods**: Need to examine actual method names in InputHandler.cpp
2. **Key mapping**: Need to identify current WASD key codes and mapping
3. **Mouse capture**: Need to understand current mouse capture implementation
4. **CMakeLists structure**: Need to identify exact lines to remove Camera references
5. **Transform methods**: Verify `getForward()`, `getRight()` methods exist in Transform component

### Files to Examine Before Implementation
- [ ] `src/core/InputHandler.cpp` - Current implementation details
- [ ] `src/core/InputHandler.h` - Full method signatures
- [ ] `src/components/Transform.h` - Available methods for movement
- [ ] `CMakeLists.txt` - Current Camera.cpp references
- [ ] `tests_cpp/test_InputHandler.cpp` - Current test expectations

---

**Estimated Implementation Time**: 4-6 hours
**Priority**: Medium (enhances UX, but camera unification already complete)
**Dependencies**: Completed camera unification (Phase 4 complete)