# Physics System Development - Next Session Plan

## Current Status Summary

### ✅ COMPLETED THIS SESSION:
- **Core PhysicsSystem Integration**: Successfully integrated PhysicsSystem with ECS architecture
- **Ground Collision Detection**: Implemented realistic ground collision with bouncing, restitution, and friction
- **Gravity System**: Implemented realistic falling physics with terminal velocity (30 m/s)
- **Transform Integration**: Added needsSync flag system for efficient transform updates
- **Falling Cube Demo**: Random cube falls every 5 seconds with proper physics response
- **Test Coverage Added**: Comprehensive physics unit tests (though some tests need debugging)

### 🚧 IMMEDIATE PRIORITY: Fix Failing Physics Tests

**Problem**: 3 physics tests are failing due to ground collision logic issues:
1. **Ground bounce test**: Entity velocity shows -4.99f instead of positive bounce
2. **Velocity stopping test**: Slow velocity shows -0.05f instead of 0.0f
3. **needsSync control test**: Transform still updates even with needsSync=false

**Root Cause Analysis Needed**:
- Ground collision detection may not be triggering properly
- Physics update order may be affecting collision response
- Gravity application might override collision response

**Debug Steps for Next Session**:
1. Add detailed logging to `PhysicsSystem::updateGroundDetection()`
2. Verify collision detection conditions are met (`bottomY <= GROUND_Y`)
3. Check if `useGravity=false` is being respected in tests
4. Trace physics update call sequence: gravity → collision → transform

**Files to Investigate**:
- `src/systems/PhysicsSystem.cpp:292-354` (updateGroundDetection method)
- `tests_cpp/physics/test_PhysicsSystem.cpp:309-385` (failing test cases)

## NEXT DEVELOPMENT PHASE: Collision Layers & Filtering (Phase 7.2)

### 1. **Fix Physics Tests** (HIGH PRIORITY)
**Estimated Time**: 30-60 minutes
**Blockers**: Tests must pass before proceeding with collision system

**Deliverables**:
- All physics tests passing (100% pass rate restoration)
- Ground collision working reliably in both production and test environments
- Clear test documentation explaining expected behavior

### 2. **Collision Layers and Filtering System** (CORE FEATURE)
**Estimated Time**: 2-3 hours
**Pokemon Use Case**: Creatures don't collide with other creatures, but do collide with environment/player

**Implementation Tasks**:
- **LayerMask Integration**: Use existing LayerMask system (Creatures, Player, Environment, etc.)
- **Collision Matrix**: Implement shouldLayersCollide() logic in PhysicsSystem
- **Component Updates**: Add collision layer assignment to CollisionComponent
- **Test Coverage**: Creature-creature pass-through, creature-environment collision

**Technical Specifications**:
```cpp
// Example collision matrix setup
physicsSystem.setCollisionMatrix(LayerMask::Creatures, LayerMask::Creatures, false); // No creature-creature
physicsSystem.setCollisionMatrix(LayerMask::Creatures, LayerMask::Environment, true); // Creature-environment
physicsSystem.setCollisionMatrix(LayerMask::Player, LayerMask::Environment, true); // Player-environment
```

**Files to Create/Modify**:
- `src/systems/PhysicsSystem.h/.cpp` - Add collision filtering methods
- `src/components/CollisionComponent.h` - Add layer assignment properties
- `tests_cpp/physics/test_PhysicsCollisionLayers.cpp` - Layer filtering tests

### 3. **Terrain Mesh Collision System** (ADVANCED FEATURE)
**Estimated Time**: 3-4 hours
**Pokemon Use Case**: Creatures walk on uneven terrain, hills, slopes

**Implementation Approach**:
- **Heightfield Collision**: Support for terrain mesh collision detection
- **Ray Casting**: Implement terrain height queries for creature placement
- **Slope Response**: Creatures slide down steep slopes, walk on gentle slopes
- **Performance**: Spatial partitioning for large terrain meshes

**Technical Components**:
- Mesh collision shape support in CollisionComponent
- Heightfield queries in PhysicsSystem
- Integration with SpatialSystem for efficient terrain queries

### 4. **Enhanced Collision Response System** (POKEMON-SPECIFIC)
**Estimated Time**: 2-3 hours
**Pokemon Use Cases**: Pokeballs bounce off creatures, creatures have different physics materials

**Response Types**:
- **Bounce**: Pokeballs, spherical objects with high restitution
- **Slide**: Creatures on slopes, low-friction surfaces
- **Stop**: Heavy objects, high-friction interactions
- **Pass-Through**: Creature-creature interactions, trigger zones

**Material Properties**:
```cpp
// Pokemon-specific physics materials
CreaturePhysicsMaterial lightCreature = {restitution: 0.2f, friction: 0.8f};
CreaturePhysicsMaterial heavyCreature = {restitution: 0.1f, friction: 0.9f};
PokeballMaterial pokeball = {restitution: 0.7f, friction: 0.3f};
```

### 5. **Projectile Physics for Capture Mechanics** (GAMEPLAY FEATURE)
**Estimated Time**: 2-3 hours
**Pokemon Use Case**: Pokeball throwing with realistic trajectory and creature collision

**Physics Requirements**:
- **Ballistic Trajectory**: Arc-based projectile motion with gravity
- **Collision Detection**: Pokeball-creature collision triggers capture attempt
- **Bouncing Behavior**: Pokeballs bounce off environment but stop on creature hit
- **Visual Feedback**: Trajectory prediction for player aiming

**Integration Points**:
- InputHandler captures throw input and power
- PhysicsSystem handles projectile simulation
- CreatureDetectionSystem handles capture logic

### 6. **Spatial-Physics Integration** (PERFORMANCE OPTIMIZATION)
**Estimated Time**: 1-2 hours
**Benefits**: Efficient collision detection for hundreds of creatures

**Optimization Strategy**:
- **Broad Phase**: Use SpatialSystem octree for collision candidate detection
- **Narrow Phase**: PhysicsSystem performs detailed collision detection only on candidates
- **Culling**: Skip physics updates for creatures outside player vicinity
- **LOD Physics**: Simplified physics for distant creatures

**Performance Targets**:
- 100+ creatures with full physics simulation
- <5ms total physics update time at 60 FPS
- Seamless integration with existing spatial queries

### 7. **Physics Debug Visualization** (DEVELOPMENT TOOL)
**Estimated Time**: 1-2 hours
**Development Value**: Essential for debugging collision issues and performance

**Visualization Features**:
- **Collision Shapes**: Wireframe display of collision boundaries
- **Velocity Vectors**: Real-time velocity visualization
- **Contact Points**: Ground collision and object collision points
- **Performance Overlay**: Physics timing and entity count

**Integration**:
- Add to existing ECS Inspector interface
- Toggle-able debug visualization (F3 key)
- Integration with ImGui for parameter tweaking

## TESTING STRATEGY

### Unit Test Coverage
- **Ground Collision**: ✅ Added (needs debugging)
- **Collision Layers**: ❌ Missing
- **Projectile Physics**: ❌ Missing
- **Performance Tests**: ❌ Missing

### Integration Testing
- **ECS-Physics Integration**: ✅ Working
- **Spatial-Physics Integration**: ❌ Planned
- **Visual-Physics Sync**: ✅ Working (falling cubes demo)

### Performance Benchmarks
- **Target**: 200+ entities at 60 FPS
- **Current**: ~125 entities (pre-physics baseline)
- **Optimization**: Spatial culling, collision filtering

## ARCHITECTURAL DECISIONS

### Physics Engine Choice
- **Current**: Custom implementation with potential Jolt Physics integration
- **Rationale**: Pokemon-style games need specialized creature interaction physics
- **Future**: Evaluate Jolt Physics for complex terrain collision

### ECS Integration Pattern
- **SystemBase Interface**: ✅ PhysicsSystem implements standard ECS interface
- **Component Design**: ✅ RigidBodyComponent, CollisionComponent, CreaturePhysicsComponent
- **Dependency Injection**: ✅ PhysicsSystem integrates with SpatialSystem

### Performance Philosophy
- **Simplicity First**: Start with O(n²) collision detection, optimize with spatial partitioning
- **Pokemon-Specific**: Optimize for 100-200 creature scenarios, not generic physics simulation
- **Incremental**: Add complexity only when performance targets require it

## FILES MODIFIED THIS SESSION

### Core Implementation
- `src/core/Application.h` - Added PhysicsSystem integration
- `src/core/ApplicationSetup.cpp` - PhysicsSystem initialization and entity setup
- `src/core/Application.cpp` - Physics update loop and falling cube demo
- `src/systems/PhysicsSystem.h/.cpp` - Complete physics system implementation

### Test Coverage
- `tests_cpp/physics/test_PhysicsSystem.cpp` - Comprehensive physics tests (debugging needed)

### Component Integration
- `src/systems/PhysicsSystem.cpp` - SystemBase inheritance and ECS integration
- Ground collision detection with bouncing physics
- Transform synchronization with needsSync flag system

## NEXT SESSION KICKOFF

**Start Here**: Fix the 3 failing physics tests by debugging ground collision logic
**Goal**: 100% test pass rate before proceeding to collision layers
**Success Metric**: All physics tests green, falling cube demo working correctly

**Command to Run**:
```bash
cd build/tests_cpp && Debug/vulkanmon_tests.exe "[Physics]"
```

**Expected Outcome**: 17/17 tests passing, 0 failures

This comprehensive plan ensures seamless continuation of physics system development with clear priorities, technical specifications, and success criteria for each phase.