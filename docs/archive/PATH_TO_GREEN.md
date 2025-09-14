# Path to Green - Bug Fixes for VulkanMon

This document outlines the debugging and fix paths for two critical issues discovered after the Application.cpp refactoring.

## 🚨 Issue #1: Lighting System Not Working

**Problem**: Lighting controls (1,2,3,4,L keys) show warning: `[LIGHTING] LightingSystem not available`

**Root Cause**: The `lightingSystem_` member in Application is `nullptr` because it's not being properly retrieved from the VulkanRenderer after initialization.

### 🛤️ Path to Green #1: Fix Lighting System Access

#### **Step 1: Check VulkanRenderer API**
- Examine `VulkanRenderer.h` for getter methods like `getLightingSystem()` or `getSystem<LightingSystem>()`
- Look for public access to the `lightingSystem_` member

#### **Step 2: Update ApplicationSetup.cpp**
After `renderer_->initialize()` in `initializeRenderer()`, add:
```cpp
// Get initialized systems from renderer
lightingSystem_ = renderer_->getLightingSystem();  // or similar method
materialSystem_ = renderer_->getMaterialSystem();
assetManager_ = renderer_->getAssetManager();
// etc.
```

#### **Step 3: Alternative - Direct Reference Pattern**
If no getters exist, modify VulkanRenderer constructor to accept references instead of shared_ptr copies:
- Pass `lightingSystem_` as a reference parameter
- VulkanRenderer updates the Application's member directly during initialization

#### **Step 4: Verify Integration**
- Build and test lighting controls (1,2,3,4,L keys)
- Confirm lighting changes are visually applied to the rendered cubes
- Expected behaviors:
  - 1/2: Light intensity changes visible
  - 3: Preset cycles (Default→Daylight→Sunset→Moonlight) with color changes
  - 4: Ambient toggle creates brightness difference
  - L: Debug info shows real lighting values, not "not available"

---

## 🚨 Issue #2: Cubes Not Rotating

**Problem**: The 5 ECS test cubes are static - no rotation animation visible.

**Root Cause Analysis Needed**: The cube rotation could be missing from several places:

### 🛤️ Path to Green #2: Restore Cube Rotation

#### **Step 1: Locate Original Rotation Logic**
Search the original working codebase for rotation:
```bash
# Check where rotation was implemented:
grep -r "rotation" src/
grep -r "glm::rotate" src/
grep -r "time.*rotation\|rotation.*time" src/
```

#### **Step 2: Check Common Rotation Locations**
1. **ECS Systems**: Look in `src/systems/RenderSystem.cpp` for frame-based rotation updates
2. **Main Loop**: Check if rotation updates were in the main application loop
3. **VulkanRenderer**: Verify if rotation happens during rendering
4. **Transform Component**: Check if Transform has auto-rotation logic

#### **Step 3: Most Likely Fix Locations**
Based on typical ECS architecture:

**Option A - RenderSystem Update**:
In `src/systems/RenderSystem.cpp`, add rotation logic to `update()` method:
```cpp
void RenderSystem::update(float deltaTime) {
    // Update all entities with Transform components
    for (auto& entity : entities_) {
        auto* transform = world_->getComponent<Transform>(entity);
        if (transform) {
            // Add rotation based on time
            transform->rotation.y += deltaTime * 45.0f; // 45 degrees per second
        }
    }
}
```

**Option B - Application Update**:
In `Application::updateECS()`, add rotation before calling `world_->update()`:
```cpp
void Application::updateECS(float deltaTime) {
    if (world_) {
        // Rotate test cubes
        // Add rotation logic here
        world_->update(deltaTime);
    }
}
```

**Option C - VulkanRenderer Animation**:
Check if rotation should happen in the vertex shader or during matrix calculations in VulkanRenderer.

#### **Step 4: Verify Animation**
- Build and run application
- Confirm all 5 cubes are rotating around their Y-axis
- Verify rotation speeds are reasonable (not too fast/slow)
- Check that ECS entity positions remain correct while rotating

---

## 🔧 Implementation Priority

### **High Priority (Lighting)**
1. Fix lightingSystem_ access from renderer
2. Test all lighting controls work visually
3. This affects immediate user experience with lighting controls

### **Medium Priority (Rotation)**
1. Locate and restore rotation logic
2. Ensure smooth animation performance
3. This affects the visual appeal but doesn't break functionality

---

## 📋 Debugging Commands

### Test Lighting Fix:
```bash
cd D:/ws/vulkanmon/build
Debug/vulkanmon.exe
# Press 1,2,3,4,L keys and verify console shows actual values, not "not available"
```

### Test Rotation Fix:
```bash
cd D:/ws/vulkanmon/build
Debug/vulkanmon.exe
# Visually confirm all 5 cubes rotate smoothly
```

### Run Tests:
```bash
cd D:/ws/vulkanmon/build/tests_cpp
Debug/vulkanmon_tests.exe
# Should maintain 1290 passing assertions
```

---

## 📁 Key Files to Examine

**For Lighting Issue**:
- `src/rendering/VulkanRenderer.h` - Check for getter methods
- `src/core/ApplicationSetup.cpp` - Fix system retrieval after renderer init
- `src/core/ApplicationInputs.cpp` - Verify lighting methods use lightingSystem_

**For Rotation Issue**:
- `src/systems/RenderSystem.cpp` - Most likely location for ECS rotation updates
- `src/core/Application.cpp` - Check updateECS() method
- `src/components/Transform.h` - Verify rotation data structure
- `src/core/Application_backup.cpp` - Reference for original working rotation

## ✅ Success Criteria

**Lighting Fixed**: All lighting controls produce visible changes + console shows actual values
**Rotation Fixed**: All 5 cubes rotate smoothly around Y-axis
**Tests Pass**: All 1290 unit test assertions continue passing

Both fixes should restore the functionality that existed before the Application.cpp refactoring.