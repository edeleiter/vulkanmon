# VulkanMon Performance Optimization TODO

**Last Updated**: September 19, 2025
**Engine Status**: EXCELLENT (150+ FPS with 1000 creatures)
**Target**: Pokemon Legends: Arceus scale performance

## 🚨 CRITICAL PRIORITY (Blocking Production)

### 1. Fix Spatial System Connection Error
**Issue**: `World::connectSystems: SpatialSystem not initialized - cannot connect system dependencies`
**Impact**: Disables frustum culling, forces rendering all 1000 creatures (0% culled)
**Current Performance Loss**: ~2-3ms batching overhead per frame

**Implementation Tasks**:
- [ ] **1.1** Investigate SpatialSystem initialization in `ApplicationSetup.cpp:L138`
- [ ] **1.2** Ensure SpatialSystem is created before `World::connectSystems()` call
- [ ] **1.3** Fix system dependency injection order:
  ```cpp
  // Current broken order - investigate this sequence
  spatialSystem_ = std::make_shared<SpatialSystem>(world_.get());
  world_->connectSystems(); // SpatialSystem not available here
  ```
- [ ] **1.4** Verify RenderSystem can access SpatialSystem for frustum queries
- [ ] **1.5** Test frustum culling works with camera movement
- [ ] **1.6** Validate spatial queries return correct creature subsets

**Success Metrics**:
- [ ] Spatial system error eliminated from logs
- [ ] Frustum culling shows >0% culled creatures when camera moves
- [ ] Batching time reduces from 4.2ms to <2.0ms with proper culling

**Files to Modify**:
- `src/core/ApplicationSetup.cpp` - Fix initialization order
- `src/systems/RenderSystem.cpp` - Verify spatial integration
- `src/spatial/SpatialManager.cpp` - Check initialization flow

---

## 🔧 HIGH PRIORITY (Performance Gains)

### 2. Optimize Instance Buffer Updates
**Current**: 4.2ms batching time for 1000 creatures
**Target**: <2.0ms with spatial culling enabled
**Potential Gain**: 2.2ms per frame (35% total frame time reduction)

**Implementation Tasks**:
- [ ] **2.1** Profile instance buffer memory copy operations in `CreatureRenderSystem::buildInstanceBatches()`
- [ ] **2.2** Implement frustum culling to reduce creatures processed:
  ```cpp
  // Current: Process all 1000 creatures
  // Target: Process only ~200-400 visible creatures
  std::vector<EntityID> visibleCreatures = spatialSystem_->queryFrustum(cameraFrustum);
  ```
- [ ] **2.3** Add distance-based LOD culling for far creatures:
  ```cpp
  const float MAX_RENDER_DISTANCE = 150.0f; // Pokemon overworld scale
  if (distance > MAX_RENDER_DISTANCE) continue; // Skip instance creation
  ```
- [ ] **2.4** Optimize matrix calculations in instance data building
- [ ] **2.5** Consider double-buffering instance data for async updates
- [ ] **2.6** Profile GPU memory bandwidth usage during instance buffer uploads

**Performance Measurement**:
- [ ] Add detailed timing for `vkCmdUpdateBuffer()` calls
- [ ] Track instance buffer size vs. frame time correlation
- [ ] Monitor GPU memory bandwidth utilization

**Success Metrics**:
- [ ] Batching time: 4.2ms → <2.0ms
- [ ] Total frame time: 6.5ms → <4.5ms
- [ ] Maintain 200+ FPS with 1000 creatures

---

### 3. Fix Vulkan Memory Cleanup Error
**Issue**: `vkUnmapMemory: Invalid device [VUID-vkUnmapMemory-device-parameter]` on shutdown
**Impact**: Potential memory leaks, validation layer warnings

**Investigation Tasks**:
- [ ] **3.1** Identify which ResourceManager cleanup is calling `vkUnmapMemory` incorrectly
- [ ] **3.2** Check `VulkanRenderer::cleanup()` sequence in destructor
- [ ] **3.3** Verify all mapped memory is properly unmapped before device destruction
- [ ] **3.4** Review instance buffer cleanup in `CreatureRenderSystem`

**Implementation Tasks**:
- [ ] **3.5** Add proper memory mapping state tracking:
  ```cpp
  class ResourceManager {
      std::unordered_set<VkDeviceMemory> mappedMemory_;
      void trackMapping(VkDeviceMemory memory);
      void unmapIfMapped(VkDeviceMemory memory);
  };
  ```
- [ ] **3.6** Ensure cleanup order: unmap memory → destroy buffers → destroy device
- [ ] **3.7** Add debug logging for memory map/unmap operations
- [ ] **3.8** Test cleanup sequence under Vulkan validation layers

**Files to Investigate**:
- `src/core/ResourceManager.cpp` - Memory management patterns
- `src/rendering/VulkanRenderer.cpp:L139` - Cleanup sequence
- `src/systems/CreatureRenderSystem.cpp` - Instance buffer cleanup

---

## 📈 MEDIUM PRIORITY (Optimization)

### 4. Improve Spatial System Cache Performance
**Issue**: Cache hit rate 0%, cache size 0 entries in performance tests
**Impact**: Spatial queries slower than optimal for Pokemon-scale worlds

**Analysis Tasks**:
- [ ] **4.1** Review spatial cache implementation in `SpatialManager`
- [ ] **4.2** Identify why cache isn't being populated during queries
- [ ] **4.3** Analyze cache key generation for entity position queries

**Implementation Tasks**:
- [ ] **4.4** Implement spatial query result caching:
  ```cpp
  struct SpatialQueryCache {
      std::unordered_map<glm::vec3, std::vector<EntityID>> regionCache_;
      float cacheValidRadius_ = 5.0f; // Pokemon movement scale
      void cacheQuery(glm::vec3 center, float radius, std::vector<EntityID> results);
  };
  ```
- [ ] **4.5** Add cache invalidation on entity movement
- [ ] **4.6** Tune cache parameters for Pokemon world movement patterns
- [ ] **4.7** Add cache hit rate monitoring and reporting

**Performance Targets**:
- [ ] Cache hit rate: 0% → 80%+
- [ ] Spatial query time: Measure baseline → 50% reduction
- [ ] Cache memory usage: <1MB for typical Pokemon overworld

---

### 5. Add Detailed Performance Profiling
**Goal**: Granular visibility into frame time breakdown for Pokemon gameplay optimization

**Implementation Tasks**:
- [ ] **5.1** Implement hierarchical performance profiler:
  ```cpp
  class PerformanceProfiler {
      struct ProfileBlock {
          std::string name;
          float minTime, maxTime, avgTime;
          size_t callCount;
      };
      std::unordered_map<std::string, ProfileBlock> blocks_;
  };
  ```
- [ ] **5.2** Add profiling to all major systems:
  - ECS system updates (CreatureDetectionSystem, SpatialSystem)
  - Vulkan command buffer recording
  - Descriptor set binding operations
  - Texture/material switching overhead
- [ ] **5.3** Implement frame-by-frame profiling output:
  ```cpp
  // Every 300 frames (5 seconds at 60 FPS)
  if (frameCount % 300 == 0) {
      profiler.logDetailedStats();
  }
  ```
- [ ] **5.4** Add GPU timing queries for Vulkan operations
- [ ] **5.5** Create performance regression detection

**Profiling Categories**:
- [ ] **CPU Systems**: ECS updates, spatial queries, AI processing
- [ ] **Vulkan Pipeline**: Command recording, descriptor binding, draw calls
- [ ] **Memory Operations**: Buffer updates, texture uploads, resource allocation
- [ ] **Game Logic**: Creature AI, collision detection, animation updates

---

## 🎮 POKEMON-SPECIFIC OPTIMIZATIONS

### 6. Pokemon Legends: Arceus Scale Optimizations
**Goal**: Optimize for typical Pokemon overworld scenarios (50-100 creatures max)

**Implementation Tasks**:
- [ ] **6.1** Add Pokemon-specific LOD system:
  ```cpp
  enum class PokemonLOD {
      NEAR = 0,     // 0-30m: Full detail, animations
      MEDIUM = 1,   // 30-80m: Reduced detail, basic animations
      FAR = 2,      // 80-150m: Billboard, no animations
      CULLED = 3    // 150m+: Not rendered
  };
  ```
- [ ] **6.2** Implement Pokemon movement prediction for spatial cache:
  ```cpp
  // Cache regions ahead of player movement direction
  glm::vec3 playerVelocity = getPlayerVelocity();
  glm::vec3 predictedPosition = playerPos + (playerVelocity * 2.0f);
  spatialSystem_->preCacheRegion(predictedPosition, CACHE_RADIUS);
  ```
- [ ] **6.3** Add Pokemon-specific batching by species/size:
  ```cpp
  struct PokemonBatch {
      std::string speciesModel;  // "pikachu.obj", "charizard.obj"
      PokemonLOD lodLevel;
      std::vector<InstanceData> instances;
  };
  ```
- [ ] **6.4** Optimize for Pokemon interaction ranges (detection, battle triggers)
- [ ] **6.5** Add overworld streaming for large Pokemon world areas

**Pokemon Performance Targets**:
- [ ] 60+ FPS with 100 Pokemon + NPCs + environment
- [ ] <16ms frame time budget (60 FPS) with 40% headroom for effects
- [ ] Smooth camera movement during Pokemon battles/interactions
- [ ] Zero stuttering during Pokemon spawning/despawning

---

## 🔬 TESTING & VALIDATION

### 7. Performance Regression Testing
**Goal**: Ensure optimizations don't break existing functionality

**Test Cases**:
- [ ] **7.1** Stress test: 1000 creatures maintain 60+ FPS
- [ ] **7.2** Memory stability: No leaks during 30-minute runs
- [ ] **7.3** Spatial accuracy: Correct entity queries in all octree regions
- [ ] **7.4** Multi-material rendering: Different Pokemon species with unique materials
- [ ] **7.5** Interactive performance: Maintain FPS during camera movement/battles

**Automated Testing**:
- [ ] **7.6** Add performance benchmarks to unit test suite:
  ```cpp
  TEST_CASE("CreatureRenderSystem Performance", "[performance]") {
      // Test 1000 creatures @ 60 FPS for 10 seconds
      REQUIRE(averageFPS >= 60.0f);
      REQUIRE(maxFrameTime <= 16.67f); // 60 FPS budget
  }
  ```
- [ ] **7.7** Implement CI performance regression detection
- [ ] **7.8** Add memory usage validation tests

---

## 📋 IMPLEMENTATION PRIORITY ORDER

### Phase 1: Critical Fixes (Week 1)
1. **Fix Spatial System Connection** (Task 1.1-1.6)
2. **Fix Vulkan Memory Cleanup** (Task 3.1-3.8)

### Phase 2: Performance Optimization (Week 2)
3. **Optimize Instance Buffer Updates** (Task 2.1-2.6)
4. **Improve Spatial Cache** (Task 4.1-4.7)

### Phase 3: Pokemon Optimization (Week 3)
5. **Add Detailed Profiling** (Task 5.1-5.5)
6. **Pokemon-Specific Features** (Task 6.1-6.5)

### Phase 4: Testing & Polish (Week 4)
7. **Performance Testing Suite** (Task 7.1-7.8)

---

## 📊 SUCCESS METRICS

**Before Optimization** (Current):
- ✅ 150+ FPS with 1000 creatures
- ⚠️ 6.5ms frame time (4.2ms batching overhead)
- ❌ 0% frustum culling (spatial system broken)
- ❌ Memory cleanup errors on shutdown

**After Optimization** (Target):
- ✅ 200+ FPS with 1000 creatures
- ✅ <4.5ms frame time (2.0ms batching time)
- ✅ 30-70% frustum culling based on camera view
- ✅ Clean shutdown with no Vulkan validation errors
- ✅ 80%+ spatial cache hit rate
- ✅ Ready for Pokemon Legends: Arceus scale gameplay

**Pokemon Gameplay Ready Criteria**:
- [ ] 60+ FPS with 100 Pokemon + environment
- [ ] <1ms spatial query time for Pokemon detection
- [ ] <2ms total ECS update time for all Pokemon AI
- [ ] Zero frame drops during Pokemon battles/interactions
- [ ] Memory usage <500MB for typical Pokemon overworld scene

---

*This TODO represents the roadmap from "excellent performance" to "production-ready Pokemon game engine".*