# Jolt Physics Crash Investigation - Detailed Session State

## Current Status: CRITICAL CRASH DURING INITIALIZATION

**Last Updated:** September 23, 2025, Session End
**Current Branch:** physics
**Issue:** Application crashes during Jolt Physics body creation in initialization phase

## Problem Summary

VulkanMon crashes during application initialization, specifically after creating Jolt Physics bodies but before completing the `createJoltBodiesForAllEntities()` function. The crash occurs BEFORE the main loop starts.

### Symptoms
- ✅ Application initializes successfully up to Jolt Physics body creation
- ✅ All 27 Jolt physics bodies are created successfully (confirmed by logs)
- ❌ Application crashes after the last body creation log message
- ❌ Never reaches "VulkanMon ready! Starting main loop..." message
- ❌ Never reaches the main application loop where physics updates would occur

### What Works
- Jolt Physics system initialization (15 threads, collision layers)
- All 27 individual body creations complete successfully
- Body creation logging shows successful completion for entities 1-27
- No errors during individual `createJoltBody()` calls

### What Crashes
- Something after the body creation loop but before function completion
- Crash occurs in `createJoltBodiesForNewEntities()` after the main loop
- Never executes "Created X new Jolt physics bodies" completion message

## Current Investigation State

### Files Under Investigation

**Primary Crash Location:**
- `src/systems/PhysicsSystem.cpp:1113-1159` - `createJoltBodiesForNewEntities()` function
- Crash occurs between lines 1150-1158 (after main loop, before completion)

**Modified Files for Debugging:**
1. `src/systems/PhysicsSystem.cpp` - Added extensive logging to pinpoint crash
2. `src/core/ApplicationSetup.cpp` - Added post-body-creation logging
3. `src/core/Application.cpp` - Added granular processFrame() logging (for later testing)

### Exact Crash Location Analysis

**Last Successful Log Message:**
```
[22:02:19.806] [INFO ] PhysicsSystem: Created Jolt body for entity 27 (Dynamic: true, Shape: 2)
```

**Missing Expected Messages:**
- "PhysicsSystem: Loop completed, created 27 bodies" (DEBUG level)
- "PhysicsSystem: Created 27 new Jolt physics bodies" (INFO level)
- "All Jolt physics bodies created successfully - returning to main initialization" (INFO level)
- "createTestScene() completing successfully" (INFO level)
- "VulkanMon ready!" (INFO level)

**Conclusion:** Crash happens in `PhysicsSystem.cpp` lines 1152-1158, specifically after the for-loop completes but before the completion messages.

### Code Analysis - PhysicsSystem.cpp:1113-1159

```cpp
void PhysicsSystem::createJoltBodiesForNewEntities(EntityManager& entityManager) {
    if (!joltPhysics_) return;

    const auto& rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();
    int newBodies = 0;

    for (EntityID entity : rigidBodyEntities) {  // ✅ This loop completes successfully
        // ... body creation logic
        newBodies++;
        VKMON_INFO("PhysicsSystem: Created Jolt body for entity " + std::to_string(entity) +
                   " (Dynamic: " + (rigidBody.isDynamic ? "true" : "false") +
                   ", Shape: " + std::to_string(static_cast<int>(collision.shapeType)) + ")");
    } // ✅ Loop exits successfully with newBodies = 27

    // 🔥 CRASH OCCURS SOMEWHERE IN THIS SECTION:
    VKMON_DEBUG("PhysicsSystem: Loop completed, created " + std::to_string(newBodies) + " bodies"); // ❌ NEVER EXECUTED

    if (newBodies > 0) { // ❌ NEVER EXECUTED
        VKMON_INFO("PhysicsSystem: Created " + std::to_string(newBodies) + " new Jolt physics bodies");
    }

    VKMON_DEBUG("PhysicsSystem: createJoltBodiesForNewEntities finishing"); // ❌ NEVER EXECUTED
}
```

### Hypothesis: Memory Corruption in Body Creation

The crash likely occurs due to:
1. **Stack corruption** during `createJoltBody()` that manifests later
2. **Hash map corruption** in `bodyToEntityMap_[bodyID] = entity` assignment
3. **Jolt Physics internal state** corruption after 27th body addition

### Current Test in Progress

**Active Debugging Change:**
- Temporarily disabled `bodyToEntityMap_[bodyID] = entity;` storage in `PhysicsSystem.cpp:947`
- Testing if the reverse mapping (BodyID -> EntityID) causes the crash
- Build was interrupted but partially completed

**File State:**
```cpp
// Store bidirectional mapping
VKMON_DEBUG("PhysicsSystem: Storing body mapping for entity " + std::to_string(entity));
entityToBodyMap_[entity] = bodyID;  // ✅ Keep this
// TEMPORARY: Skip reverse mapping to test if it causes crash
// bodyToEntityMap_[bodyID] = entity;  // ❌ DISABLED FOR TESTING
VKMON_DEBUG("PhysicsSystem: Body mapping stored successfully for entity " + std::to_string(entity));
```

## Next Session Action Plan

### Immediate Next Steps (Priority Order)

1. **Complete Current Test**
   ```bash
   cd build
   cmake --build .
   timeout 8s Debug/vulkanmon.exe 2>&1
   ```
   - Test if disabling `bodyToEntityMap_` storage fixes the crash
   - If successful, investigate proper JPH::BodyID hashing

2. **If Reverse Mapping Is The Issue:**
   - Research JPH::BodyID hash function requirements
   - Implement proper hash specialization for `std::unordered_map<JPH::BodyID, EntityID>`
   - Alternative: Use `std::map<JPH::BodyID, EntityID>` instead

3. **If Reverse Mapping Is NOT The Issue:**
   - Re-enable `bodyToEntityMap_[bodyID] = entity;`
   - Add logging between each line in the completion section:
     ```cpp
     VKMON_INFO("DEBUG: About to check newBodies > 0");
     if (newBodies > 0) {
         VKMON_INFO("DEBUG: About to log completion message");
         VKMON_INFO("PhysicsSystem: Created " + std::to_string(newBodies) + " new Jolt physics bodies");
         VKMON_INFO("DEBUG: Completion message logged");
     }
     VKMON_INFO("DEBUG: About to finish function");
     ```

4. **Alternative Investigation Paths:**
   - Test with fewer entities (create only 5 instead of 27)
   - Test with static-only bodies (disable dynamic body creation)
   - Add try-catch around the entire `createJoltBodiesForNewEntities()` function

### Key Files to Monitor

**Critical Files:**
- `src/systems/PhysicsSystem.cpp:1113-1159` - Primary crash location
- `src/systems/PhysicsSystem.cpp:889-953` - Individual body creation
- `src/systems/PhysicsSystem.h:333-334` - Hash map declarations

**Modified Files (Revert After Debug):**
- `src/core/ApplicationSetup.cpp:426,430` - Added debug messages
- `src/systems/PhysicsSystem.cpp:99-108` - Added detailed update logging
- `src/systems/PhysicsSystem.cpp:939-951` - Added detailed body creation logging
- `src/systems/PhysicsSystem.cpp:1152-1158` - Added completion logging

### Expected Success Criteria

**Crash Fixed When:**
- Application completes initialization and shows "VulkanMon ready!" message
- Application enters main loop without crashing
- Can test actual Jolt Physics simulation updates

**Success Metrics:**
```
[INFO ] PhysicsSystem: Created 27 new Jolt physics bodies
[INFO ] All Jolt physics bodies created successfully - returning to main initialization
[INFO ] createTestScene() completing successfully
[INFO ] VulkanMon ready!
[INFO ] VulkanMon ready! Starting main loop...
```

## Technical Context

### Environment
- **Branch:** physics
- **Platform:** Windows with MSVC, NVIDIA GeForce RTX 5070 Ti
- **Build System:** CMake + vcpkg
- **Jolt Physics:** 15 threads, properly initialized
- **Entity Count:** 27 dynamic bodies with collision components

### Integration Status
- ✅ Jolt Physics system initialization complete
- ✅ ECS integration functional
- ✅ Individual body creation working
- ❌ Body creation completion crashes
- ❌ Physics simulation loop untested (never reached)

### Test Data
- 27 entities in 3x3x3 cube formation
- All entities have RigidBodyComponent (dynamic, mass=1.0, gravity enabled)
- All entities have CollisionComponent (Box shape, layer=Creatures)
- All entities have Transform component

This documentation should allow seamless continuation of the debugging session by immediately understanding the exact crash location and current test state.