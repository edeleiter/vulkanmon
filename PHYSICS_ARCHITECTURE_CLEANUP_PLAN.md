# Physics Architecture Cleanup Plan

## Problem Statement
VulkanMon currently has two physics systems (Jolt + legacy) with confused ownership models, manual sync complexity, and failing tests that don't reflect proper architecture. We need to clean this up to have a single, authoritative physics system.

## Goals
1. Single physics system (Jolt Physics only)
2. Clear data flow: Jolt Physics → ECS Transform (read-only)
3. Automatic synchronization every frame
4. Tests that verify actual functionality
5. Remove unnecessary complexity

## Architecture Vision
```
Game Logic → Jolt Physics Bodies ← (authoritative position/velocity)
                ↓ (automatic sync every frame)
            ECS Transform Components ← (read-only mirror for rendering/queries)
```

## Detailed Implementation Plan

### Step 1: Delete All Legacy Physics Code
**Why**: Maintaining two physics systems is wasteful and error-prone
**Files**: `src/systems/PhysicsSystem.cpp`, `src/systems/PhysicsSystem.h`

**1.1 Remove updateDynamicBodies method**
- This was custom physics integration with terminal velocity, damping
- Jolt Physics handles all of this internally
- Location: ~line 320-370 in PhysicsSystem.cpp

**1.2 Remove integrateRigidBodies method**
- Legacy fallback physics integration
- Location: ~line 312 in PhysicsSystem.cpp
- Remove call from main update() method line 118

**1.3 Remove applyGravity method**
- Jolt Physics handles gravity internally via world settings
- Location: ~line 408-421 in PhysicsSystem.cpp

**1.4 Remove terminal velocity and damping code**
- If needed for gameplay, implement via Jolt damping settings
- Remove from updateCreaturePhysics method

### Step 2: Simplify ECS Transform Sync
**Why**: ECS Transforms should be read-only mirrors of Jolt state
**Goal**: Automatic sync FROM Jolt TO ECS every frame

**2.1 Update syncTransformsFromJolt method**
- Currently selective sync, make it sync ALL physics entities every frame
- Remove any conditional logic based on "dirty" flags
- Location: ~line 1130 in PhysicsSystem.cpp

**2.2 Remove needsSync flag entirely**
- From RigidBodyComponent struct definition
- From all usage in PhysicsSystem.cpp
- From component creation in ApplicationSetup.cpp

**2.3 Remove updateTransforms method**
- Legacy ECS→Physics sync, we want Physics→ECS only
- Location: ~line 809 in PhysicsSystem.cpp

### Step 3: Clean Up Main Update Method
**Why**: Single code path is easier to maintain and debug
**Goal**: Only Jolt Physics, no fallbacks

**3.1 Remove legacy physics fallback**
- Remove else clause in main update() method ~line 114-130
- Remove collisionEnabled_ checks (always use Jolt)

**3.2 Simplify update flow**
```cpp
void PhysicsSystem::update(EntityManager& entityManager, float deltaTime) {
    // 1. Create Jolt bodies for new entities
    createJoltBodiesForNewEntities(entityManager);

    // 2. Step Jolt Physics simulation
    joltPhysics_->Update(deltaTimeSeconds, collisionSteps, tempAllocator_, jobSystem_);

    // 3. Sync all transforms from Jolt to ECS
    syncTransformsFromJolt(entityManager);
}
```

### Step 4: Remove/Rewrite Failing Tests
**Why**: Tests should verify actual functionality, not legacy features

**4.1 Delete needsSync test**
- Feature no longer exists in clean architecture
- File: `tests_cpp/physics/test_PhysicsSystem.cpp` ~line 448

**4.2 Delete terminal velocity test**
- Not needed for Pokemon gameplay
- If needed later, implement via Jolt damping
- File: `tests_cpp/physics/test_PhysicsSystem.cpp` ~line 539

**4.3 Rewrite deltaTime test**
- Test that Jolt Physics simulation responds to different time steps
- Verify positions change proportionally with time

### Step 5: Add New Tests for Clean Architecture
**Why**: Verify the new architecture works correctly

**5.1 Test automatic ECS Transform sync**
- Create Jolt body, step physics, verify ECS Transform updated
- Test multiple entities sync correctly

**5.2 Test Jolt Physics time stepping**
- Verify simulation advances correctly with different deltaTime values
- Test physics consistency

**5.3 Test entity lifecycle**
- Test that new entities get Jolt bodies created
- Test that removed entities get Jolt bodies cleaned up

## Success Criteria
1. ✅ Only Jolt Physics code remains (no legacy fallbacks)
2. ✅ ECS Transforms automatically sync from Jolt every frame
3. ✅ No manual sync flags or complex ownership logic
4. ✅ All physics tests pass and verify real functionality
5. ✅ Clean, maintainable codebase ready for Pokemon-scale gameplay

## Benefits
- **Simplified Architecture**: Single physics system, clear data flow
- **Better Performance**: No redundant physics calculations
- **Easier Maintenance**: One physics system to debug and optimize
- **Robust Foundation**: Jolt Physics is industry-proven for large-scale games
- **Pokemon Ready**: Efficient physics for hundreds of creatures

## Risks & Mitigations
- **Risk**: Breaking existing gameplay functionality
- **Mitigation**: Thorough testing at each step, keep git commits atomic
- **Risk**: Performance regression during cleanup
- **Mitigation**: Profile before/after, focus on correctness first