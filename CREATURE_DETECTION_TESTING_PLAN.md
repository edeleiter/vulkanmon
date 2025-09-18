# Creature Detection System Testing Plan

## Overview

Comprehensive testing plan for the newly uncommented and fixed CreatureDetectionSystem to ensure spatial query integration is working correctly for Pokemon-style creature AI.

## Current Status

### Recently Fixed Issues (Session Complete)
- ✅ Fixed hardcoded `timeSinceLastQuery` in SpatialSystem.h (now uses actual deltaTime)
- ✅ Fixed zero position detection using proper `isInitialized` flag in SpatialComponent
- ✅ Fixed transform rotation type mismatches in ApplicationSetup.cpp using `setRotationEuler()`
- ✅ Uncommented spatial query integration in CreatureDetectionSystem.h
- ✅ Resolved build issues by adding missing `#include "../systems/SpatialSystem.h"`
- ✅ Confirmed project builds and runs without errors

### System Integration Status
- **SpatialSystem**: ✅ Functional with octree-based queries
- **CreatureDetectionSystem**: ✅ Now has active spatial query integration
- **Application**: ✅ Creates 36 test creatures in stress test grid
- **Build System**: ✅ No compilation errors

## Detailed Testing Plan

### Phase 1: Basic Functionality Verification (30 minutes)

#### 1.1 Visual Confirmation of System Integration
**File**: `src/core/Application.cpp` or check initialization logs

**Actions**:
1. **Launch Application**: Run `Debug/vulkanmon.exe` from build directory
2. **Check Console Output**: Look for these initialization messages:
   ```
   [INFO ] SpatialSystem initialized with world bounds
   [INFO ] CreatureDetectionSystem initialized
   [INFO ] GPU Instancing Stress Test: Created 36 creatures
   ```
3. **Verify Scene**: Should see 6x6 grid of creatures (cubes, spheres, pyramids, planes)
4. **Camera Movement**: Use WASD + mouse to navigate around the creature grid
5. **Performance Check**: Confirm 60+ FPS with creature detection running

**Success Criteria**:
- All systems initialize without errors
- 36 creatures visible in organized grid pattern
- Smooth camera movement and rendering performance
- No console error messages

#### 1.2 CreatureDetectionSystem Performance Logging
**File**: `src/game/CreatureDetectionSystem.h:113-116` (performance logging every 5 seconds)

**Actions**:
1. **Monitor Console**: Wait for performance reports (every ~5 seconds at 60 FPS)
2. **Look for Log Pattern**:
   ```
   [INFO ] CreatureDetectionSystem Performance:
   [INFO ]   Creatures processed: 36
   [INFO ]   Detections triggered: X
   [INFO ]   State changes: Y
   [INFO ]   Update time: Z.XXms
   ```

**Success Criteria**:
- `Creatures processed: 36` (matches expected creature count)
- `Detections triggered: > 0` (spatial queries finding entities)
- `State changes: > 0` (creatures changing behavior states)
- `Update time: < 5ms` (performance is acceptable)

**Failure Indicators**:
- `Creatures processed: 0` → ECS integration problem
- `Detections triggered: 0` → Spatial queries not working
- `Update time: > 10ms` → Performance regression

### Phase 2: Spatial Query Validation (45 minutes)

#### 2.1 Layer Mask Integration Testing
**File**: `src/game/CreatureDetectionSystem.h:141-144` (spatial query with layer mask)

**Background**: Creatures query for `LayerMask::Player | LayerMask::Creatures`

**Actions**:
1. **Check Current Layer Assignments**:
   - Find where creatures get their `SpatialComponent.spatialLayers`
   - Expected location: `src/core/ApplicationSetup.cpp` in stress test creation
   - Look for: `spatialComponent.spatialLayers = LayerMask::Creatures;`

2. **Verify Layer Assignment**:
   ```cpp
   // Expected code pattern in ApplicationSetup.cpp:
   SpatialComponent spatialComponent(1.5f, SpatialBehavior::DYNAMIC, LayerMask::Creatures);
   world_->addComponent(creature, spatialComponent);
   ```

3. **Test Detection Range**:
   - Move camera close to creatures (within 15.0f units - default detection radius)
   - Check if `Detections triggered` count increases in performance logs
   - Move camera far away (> 25.0f units) and verify detections decrease

**Success Criteria**:
- Creatures have proper layer assignments (`LayerMask::Creatures`)
- Detection count correlates with camera proximity to creatures
- No "SpatialSystem: No spatial system available" warnings

**Debugging Steps if Failing**:
1. **Check ApplicationSetup.cpp**: Verify `spatialLayers` assignment during creature creation
2. **Check LayerMask Values**: Ensure `LayerMask::Creatures` is properly defined
3. **Add Debug Logging**: Temporarily add `VKMON_DEBUG` to spatial query results

#### 2.2 CreatureComponent Integration Verification
**Files**:
- `src/game/CreatureDetectionSystem.h:97-98` (component checking)
- Expected: `src/core/ApplicationSetup.cpp` (CreatureComponent creation)

**Actions**:
1. **Find CreatureComponent Creation**: Search ApplicationSetup.cpp for CreatureComponent usage
   - If missing: Need to add `CreatureComponent` to stress test entities
   - Expected pattern: `world_->addComponent(creature, CreatureComponent{});`

2. **Component Requirements Check**:
   ```cpp
   // Each creature entity should have all three components:
   // - Transform ✅ (already exists)
   // - SpatialComponent ✅ (already exists)
   // - CreatureComponent ❓ (needs verification)
   ```

3. **ECS Query Validation**:
   - CreatureDetectionSystem requires entities with all three components
   - Line 97-98: `entityManager.hasComponent<SpatialComponent>(entity) && entityManager.hasComponent<CreatureComponent>(entity)`

**Success Criteria**:
- All 36 creatures have CreatureComponent attached
- `Creatures processed: 36` in performance logs
- No component-missing related errors

**Fix if Needed**:
```cpp
// Add to ApplicationSetup.cpp stress test creation:
CreatureComponent creatureComponent(CreatureComponent::CreatureType::PEACEFUL, 15.0f);
world_->addComponent(creature, creatureComponent);
```

#### 2.3 State Machine Behavior Testing
**File**: `src/game/CreatureDetectionSystem.h:234-273` (state transitions)

**Actions**:
1. **Enable Debug Logging**: Uncomment debug verbose logging
   ```cpp
   // In CreatureDetectionSystem.h, change:
   #ifdef DEBUG_VERBOSE  // Comment this out temporarily
   VKMON_DEBUG("Creature state changed...");
   #endif               // And this
   ```

2. **Observe State Changes**:
   - Let application run for 30+ seconds
   - Should see random IDLE ↔ WANDERING transitions (1% chance per frame)
   - Look for state change debug messages in console

3. **Test Detection-Based State Changes**:
   - If player layer entities exist, should see ALERT/FLEEING states
   - Currently may not trigger without proper player entities

**Success Criteria**:
- `State changes: > 0` in performance reports
- Visible state transition debug messages (if enabled)
- No infinite loops or stuck states

### Phase 3: Player Interaction Testing (30 minutes)

#### 3.1 Player Entity Setup
**Issue**: Current stress test may not have player entities for creatures to detect

**Actions**:
1. **Check for Player Entities**: Search ApplicationSetup.cpp for player creation
2. **Add Test Player Entity** (if missing):
   ```cpp
   // Add to ApplicationSetup.cpp after creature creation:
   EntityID playerEntity = world_->createEntity();

   Transform playerTransform;
   playerTransform.position = glm::vec3(0.0f, 0.0f, 0.0f); // Center of grid
   playerTransform.setRotationEuler(0.0f, 0.0f, 0.0f);
   world_->addComponent(playerEntity, playerTransform);

   SpatialComponent playerSpatial(2.0f, SpatialBehavior::DYNAMIC, LayerMask::Player);
   world_->addComponent(playerEntity, playerSpatial);

   VKMON_INFO("Test player entity created for creature detection testing");
   ```

3. **Verify Player Detection**:
   - Place player entity within creature detection radius (15.0f default)
   - Should trigger creature state changes to ALERT/FLEEING/AGGRESSIVE
   - Monitor performance logs for increased `Detections triggered`

#### 3.2 Detection Radius Validation
**File**: `src/game/CreatureDetectionSystem.h:31-33` (detection parameters)

**Actions**:
1. **Test Detection Ranges**:
   - `detectionRadius = 15.0f` - basic creature awareness
   - `alertRadius = 25.0f` - creature becomes alert
   - `fleeRadius = 8.0f` - creature starts fleeing

2. **Distance Calculation Fix**:
   - **Current Bug**: Line 168 has `glm::distance(transform.position, transform.position)`
   - **Fix Needed**: Get actual player position for proper distance calculation
   - **Test**: Verify creatures react at correct distances

3. **Behavior Type Testing**:
   ```cpp
   // Test each creature type behavior:
   // PEACEFUL - should flee when player gets close
   // NEUTRAL - should become alert but not flee
   // AGGRESSIVE - should approach player
   ```

### Phase 4: Performance and Optimization Testing (20 minutes)

#### 4.1 Spatial Query Performance
**Files**:
- `src/game/CreatureDetectionSystem.h:135` (query throttling)
- `src/components/SpatialComponent.h:60-70` (throttling logic)

**Actions**:
1. **Monitor Query Frequency**:
   - Default: `detectionCheckInterval = 0.2f` (5 Hz per creature)
   - 36 creatures = ~180 spatial queries per second maximum
   - Actual rate should be lower due to throttling

2. **Performance Metrics**:
   - CreatureDetectionSystem update time should be < 5ms
   - SpatialSystem performance stats (logged every 30 seconds)
   - Look for: average query time, cache hit rate

3. **Stress Testing**:
   - All 36 creatures running detection simultaneously
   - No frame rate drops below 60 FPS
   - Memory usage stable (no leaks from spatial queries)

#### 4.2 Cache Hit Rate Analysis
**File**: `src/systems/SpatialSystem.h:172-173` (cache performance logging)

**Actions**:
1. **Wait for Spatial Performance Report** (every 30 seconds):
   ```
   [INFO ] SpatialSystem Performance Report
   [INFO ]   Cache hit rate: X.X%
   [INFO ]   Average query time: X.XXms
   ```

2. **Expected Performance**:
   - Cache hit rate: > 80% (creatures in same area query similar regions)
   - Average query time: < 0.1ms (sub-millisecond spatial queries)
   - Total queries: correlates with creature detection frequency

**Success Criteria**:
- Cache hit rate > 70% (indicates good spatial locality)
- Query time < 1ms (octree performing efficiently)
- No performance degradation over time

### Phase 5: Integration and Edge Case Testing (15 minutes)

#### 5.1 ECS Integration Validation
**Actions**:
1. **Component Lifecycle**: Verify creatures maintain all required components
2. **Entity Creation/Deletion**: Test system handles entity changes gracefully
3. **System Dependencies**: Confirm CreatureDetectionSystem works with SpatialSystem

#### 5.2 Edge Case Scenarios
**Test Cases**:
1. **No Player Entities**: System should handle gracefully (no crashes)
2. **Creatures at World Bounds**: Ensure no out-of-bounds spatial queries
3. **High Detection Frequency**: Temporarily set `detectionCheckInterval = 0.01f` for stress test
4. **Zero Detection Radius**: Test with `detectionRadius = 0.0f` (should disable detection)

### Phase 6: Documentation and Next Steps (10 minutes)

#### 6.1 Results Documentation
**Create**: `CREATURE_DETECTION_TEST_RESULTS.md`

**Include**:
- Performance metrics (frame times, query counts, cache hit rates)
- Behavior observations (state transitions, detection accuracy)
- Issues found and fixes needed
- Configuration recommendations

#### 6.2 Known Issues to Address
**Expected Findings**:
1. **Distance Calculation Bug**: Line 168 needs proper player position lookup
2. **Missing CreatureComponents**: May need to add to stress test entities
3. **Player Entity Setup**: Might need test player for meaningful interactions
4. **Detection Throttling**: May need tuning for optimal performance/responsiveness

## Success Metrics Summary

### Functional Requirements
- ✅ 36 creatures processed by detection system
- ✅ Spatial queries returning valid entity lists
- ✅ State transitions occurring (IDLE ↔ WANDERING minimum)
- ✅ No system crashes or errors

### Performance Requirements
- ✅ CreatureDetectionSystem update time < 5ms
- ✅ Spatial query average time < 1ms
- ✅ Cache hit rate > 70%
- ✅ Frame rate maintained at 60+ FPS

### Integration Requirements
- ✅ All three component types present on creatures
- ✅ SpatialSystem and CreatureDetectionSystem communication working
- ✅ Layer mask filtering functioning correctly
- ✅ ECS World managing entities properly

## Next Session Priorities

1. **High Priority**: Fix distance calculation bug for accurate creature reactions
2. **High Priority**: Add proper player entities for meaningful detection testing
3. **Medium Priority**: Tune detection parameters for Pokemon-style gameplay feel
4. **Medium Priority**: Implement creature movement based on state changes
5. **Low Priority**: Add visual debug rendering for detection radius/states

## Files to Monitor During Testing

### Key Log Sources
- `CreatureDetectionSystem` performance reports (every 5 seconds)
- `SpatialSystem` performance reports (every 30 seconds)
- Entity creation logs during startup
- Any ERROR/WARNING messages

### Key Source Files
- `src/game/CreatureDetectionSystem.h` - Main system logic
- `src/core/ApplicationSetup.cpp` - Entity creation and configuration
- `src/systems/SpatialSystem.h` - Spatial query implementation
- `src/components/SpatialComponent.h` - Entity spatial properties

---

**Note for Next Session**: This testing plan provides a complete roadmap to validate the creature detection system. Follow the phases sequentially, document findings, and prioritize fixes based on impact on Pokemon-style gameplay experience.