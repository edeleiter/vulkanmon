# Camera Configuration Refactor Plan

## Problem Statement
Camera configuration constants are duplicated across multiple files with inconsistent values, causing rendering bugs and maintenance issues.

## Current Duplication Issues

### 1. Application.h (Lines 59-61)
```cpp
static constexpr float DEFAULT_CAMERA_FOV = 75.0f;     // Updated
static constexpr float DEFAULT_NEAR_PLANE = 0.1f;
static constexpr float DEFAULT_FAR_PLANE = 200.0f;     // Updated
```

### 2. VulkanRenderer.cpp (Lines 1907-1909)
```cpp
constexpr float CAMERA_FOV = 45.0f;      // OLD VALUE - INCONSISTENT!
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 10.0f;       // OLD VALUE - CAUSES CULLING BUG!
```

### 3. Components/Camera.h (Lines 13-15)
```cpp
float fov = 45.0f;          // Different default
float nearPlane = 0.1f;
float farPlane = 1000.0f;   // Different default
```

### 4. Application.h (Line 58)
```cpp
static constexpr float DEFAULT_CAMERA_SPEED = 2.5f;
```

## Solution: Unified Configuration System

### Phase 1: Create Configuration Infrastructure

#### Step 1.1: Create config directory
- Create `src/config/` directory
- Establish pattern for all engine configuration

#### Step 1.2: Create CameraConfig.h
```cpp
#pragma once

namespace VulkanMon::Config {
    /**
     * Unified camera configuration - Single source of truth
     * Used by: VulkanRenderer, Application, ECS Camera components
     */
    struct Camera {
        // Projection settings
        static constexpr float DEFAULT_FOV = 75.0f;          // Field of view in degrees
        static constexpr float DEFAULT_NEAR_PLANE = 0.1f;    // Near clipping plane
        static constexpr float DEFAULT_FAR_PLANE = 200.0f;   // Far clipping plane

        // Movement settings
        static constexpr float DEFAULT_SPEED = 2.5f;         // Movement speed

        // Display settings
        static constexpr float DEFAULT_ASPECT_RATIO = 16.0f / 9.0f;  // Default aspect ratio

        // Window settings (moved from Application.h for consistency)
        static constexpr int DEFAULT_WINDOW_WIDTH = 800;
        static constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    };
}
```

### Phase 2: Update All Camera-Related Files

#### Step 2.1: Update VulkanRenderer.cpp
**File**: `src/rendering/VulkanRenderer.cpp`
**Lines**: 1907-1909
**Action**: Replace hardcoded constants
```cpp
// Before:
constexpr float CAMERA_FOV = 45.0f;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 10.0f;

// After:
#include "../config/CameraConfig.h"
// Use: Config::Camera::DEFAULT_FOV, etc.
```

#### Step 2.2: Update Application.h
**File**: `src/core/Application.h`
**Lines**: 56-61
**Action**: Remove duplicate constants, include CameraConfig
```cpp
// Remove these lines:
static constexpr int DEFAULT_WINDOW_WIDTH = 800;
static constexpr int DEFAULT_WINDOW_HEIGHT = 600;
static constexpr float DEFAULT_CAMERA_SPEED = 2.5f;
static constexpr float DEFAULT_CAMERA_FOV = 75.0f;
static constexpr float DEFAULT_NEAR_PLANE = 0.1f;
static constexpr float DEFAULT_FAR_PLANE = 200.0f;

// Add:
#include "../config/CameraConfig.h"
// Use: Config::Camera::DEFAULT_* everywhere
```

#### Step 2.3: Update Components/Camera.h
**File**: `src/components/Camera.h`
**Lines**: 13-15
**Action**: Use config defaults
```cpp
// Before:
float fov = 45.0f;
float nearPlane = 0.1f;
float farPlane = 1000.0f;

// After:
#include "../config/CameraConfig.h"
float fov = Config::Camera::DEFAULT_FOV;
float nearPlane = Config::Camera::DEFAULT_NEAR_PLANE;
float farPlane = Config::Camera::DEFAULT_FAR_PLANE;
```

#### Step 2.4: Update ApplicationSetup.cpp
**File**: `src/core/ApplicationSetup.cpp`
**Lines**: 21, 32-36, 251-253
**Action**: Use config constants
```cpp
// Window creation:
window_ = std::make_unique<Window>(Config::Camera::DEFAULT_WINDOW_WIDTH,
                                  Config::Camera::DEFAULT_WINDOW_HEIGHT, "VulkanMon");

// Camera initialization:
cameraComponent.fov = Config::Camera::DEFAULT_FOV;
cameraComponent.nearPlane = Config::Camera::DEFAULT_NEAR_PLANE;
cameraComponent.farPlane = Config::Camera::DEFAULT_FAR_PLANE;
```

### Phase 3: Search and Fix Other Configuration Duplications

#### Step 3.1: Search for other duplicated constants
**Search patterns**:
- `constexpr.*=.*f;` (floating point constants)
- `static.*const.*=` (static constants)
- Hardcoded magic numbers in rendering code

#### Step 3.2: Potential areas to check
- Material system constants
- Lighting system defaults
- Rendering pipeline settings
- Performance thresholds
- Debug/logging settings

### Phase 4: Validation and Testing

#### Step 4.1: Build verification
- Ensure all files compile after changes
- No undefined reference errors
- All includes resolve correctly

#### Step 4.2: Runtime testing
- Camera FOV is 75° (wider view)
- Far plane is 200.0f (objects at Z=25 visible)
- Camera movement speed consistent
- Window size correct

#### Step 4.3: Regression testing
- ECS Inspector still shows entities
- Creature rendering still works
- No performance degradation

## Implementation Order

1. **Create config infrastructure** (Step 1.1-1.2)
2. **Fix critical rendering bug** (Step 2.1 - VulkanRenderer)
3. **Clean up Application.h** (Step 2.2)
4. **Update ECS Camera** (Step 2.3)
5. **Update ApplicationSetup** (Step 2.4)
6. **Build and test** (Step 4.1-4.2)
7. **Search for other duplications** (Step 3.1-3.2)
8. **Final validation** (Step 4.3)

## Success Criteria

✅ **Single source of truth**: All camera settings in CameraConfig.h
✅ **Consistent values**: No conflicting constants across files
✅ **Visible objects**: Creatures render with correct camera settings
✅ **Clean architecture**: Pattern established for future config needs
✅ **No regressions**: All existing functionality preserved

## Future Extensions

This pattern enables:
- `RenderConfig.h` for rendering settings
- `InputConfig.h` for control settings
- `PerformanceConfig.h` for optimization thresholds
- `DebugConfig.h` for logging and debug settings
- Runtime configuration loading from files