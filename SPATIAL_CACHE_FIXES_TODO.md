# Spatial Cache Critical Fixes TODO

**Created**: September 19, 2025
**Priority**: HIGH - Fix architectural flaws in spatial cache system
**Current Status**: Proof-of-concept working but not production-ready

## 🚨 CRITICAL PRIORITY (Production Blockers)

### 1. Fix Broken "Lock-Free" Architecture
**Issue**: Claims lock-free but uses single buffer with atomic pointers to same location
**Impact**: Concurrency unsafe, misleading design, potential data races

**Implementation Tasks**:
- [ ] **1.1** Choose concurrency model: single-threaded OR proper lock-free
  - Option A: Single-threaded with explicit threading model documentation
  - Option B: True lock-free with proper double-buffering and atomic swaps
- [ ] **1.2** If single-threaded: Remove atomic operations, simplify to `std::unique_ptr`
- [ ] **1.3** If lock-free: Implement proper buffer swapping mechanism:
  ```cpp
  void swapBuffers() {
      auto* oldRead = readCache_.load();
      auto* oldWrite = writeCache_.load();
      readCache_.store(oldWrite);   // Readers now see previous writes
      writeCache_.store(oldRead);   // Writers use previous read buffer
  }
  ```
- [ ] **1.4** Add documentation explaining chosen concurrency model
- [ ] **1.5** Add thread safety assertions or checks
- [ ] **1.6** Test under multi-threaded load if lock-free chosen

**Files to Modify**:
- `src/spatial/SpatialCache.h` - Architecture redesign
- `src/spatial/SpatialCache.cpp` - Implementation updates
- Documentation explaining threading model

---

### 2. Replace Nuclear Cache Invalidation
**Issue**: `queryCache_->clear()` destroys entire cache when one entity moves
**Impact**: Cache thrashing in real Pokemon scenarios with moving creatures

**Analysis Tasks**:
- [ ] **2.1** Profile current invalidation frequency in realistic Pokemon scenario
- [ ] **2.2** Measure cache hit rate with 20% entities moving vs 100% static

**Implementation Tasks**:
- [ ] **2.3** Design spatial region-based invalidation:
  ```cpp
  struct CacheRegion {
      BoundingBox bounds;
      std::unordered_set<size_t> affectedHashes;
  };
  void invalidateRegion(const BoundingBox& region);
  ```
- [ ] **2.4** Implement region-to-hash mapping for frustum queries
- [ ] **2.5** Implement region-to-hash mapping for radius queries
- [ ] **2.6** Add entity position tracking for invalidation:
  ```cpp
  void trackEntityMovement(EntityID entity, glm::vec3 oldPos, glm::vec3 newPos) {
      BoundingBox affectedRegion = calculateAffectedRegion(oldPos, newPos);
      invalidateRegion(affectedRegion);
  }
  ```
- [ ] **2.7** Fallback to global clear only when region tracking fails
- [ ] **2.8** Add metrics for regional vs global invalidations

**Success Metrics**:
- [ ] Cache hit rate >80% with 20% entities moving
- [ ] <10% global cache clears in typical Pokemon gameplay
- [ ] Regional invalidation covers <25% of cache entries per movement

---

### 3. Fix Floating-Point Quantization Issues
**Issue**: Hardcoded 1000.0f quantization causes precision loss and hash collisions
**Impact**: Similar frustums hash differently, truly different frustums hash same

**Investigation Tasks**:
- [ ] **3.1** Test hash collision rate with current quantization
- [ ] **3.2** Measure frustum precision requirements for typical camera movement
- [ ] **3.3** Compare hash stability vs precision for different quantization levels

**Implementation Tasks**:
- [ ] **3.4** Replace quantization with epsilon-based comparison:
  ```cpp
  bool frustumsEqual(const Frustum& a, const Frustum& b, float epsilon = 1e-5f) {
      for (int i = 0; i < 6; ++i) {
          if (glm::distance(a.planes[i], b.planes[i]) > epsilon) return false;
      }
      return true;
  }
  ```
- [ ] **3.5** Implement cache lookup with epsilon tolerance:
  - Hash to bucket, then epsilon comparison within bucket
  - Or use spatial hash with controllable precision
- [ ] **3.6** Add configurable epsilon based on use case:
  - Static camera: 1e-6f (very precise)
  - Moving camera: 1e-4f (more tolerant)
  - Cinematic camera: 1e-3f (very tolerant)
- [ ] **3.7** Profile hash collision rate and cache effectiveness
- [ ] **3.8** Add hash distribution analysis tools

**Alternative Approach**:
- [ ] **3.9** Consider cache key redesign: camera position + direction + FOV instead of frustum planes

---

### 4. Add Comprehensive Error Handling
**Issue**: No error handling for cache operations, memory pressure, or edge cases
**Impact**: Silent failures, memory leaks, unpredictable behavior

**Implementation Tasks**:
- [ ] **4.1** Add cache size limits and enforcement:
  ```cpp
  static constexpr size_t MAX_CACHE_ENTRIES = 10000;
  static constexpr size_t MAX_MEMORY_MB = 100;
  bool hasCapacity() const;
  void enforceMemoryLimits();
  ```
- [ ] **4.2** Add cache operation result types:
  ```cpp
  enum class CacheResult { Hit, Miss, Error, MemoryFull };
  CacheResult tryGetFrustumQuery(...);
  ```
- [ ] **4.3** Handle memory allocation failures gracefully
- [ ] **4.4** Add cache corruption detection and recovery
- [ ] **4.5** Implement cache health monitoring:
  ```cpp
  struct CacheHealth {
      bool memoryHealthy;
      bool hashDistributionHealthy;
      float averageChainLength;
      size_t totalCollisions;
  };
  ```
- [ ] **4.6** Add automatic cache reset when corruption detected
- [ ] **4.7** Log cache errors appropriately (not spam, but not silent)

---

## 🔧 HIGH PRIORITY (Performance & Reliability)

### 5. Implement Proper Cache Eviction Strategy
**Issue**: Current TTL-only eviction may not handle memory pressure well
**Impact**: Unbounded memory growth, poor cache locality

**Implementation Tasks**:
- [ ] **5.1** Implement LRU eviction with TTL:
  ```cpp
  struct CacheEntry {
      float lastAccessTime;
      float creationTime;
      size_t accessCount;
      std::vector<EntityID> results;
  };
  ```
- [ ] **5.2** Add access time tracking for entries
- [ ] **5.3** Implement memory-pressure-triggered eviction
- [ ] **5.4** Add cache warming strategies for predictable access patterns
- [ ] **5.5** Tune eviction parameters based on Pokemon gameplay patterns

### 6. Add Production-Grade Metrics and Monitoring
**Issue**: Only basic hit rate available, no detailed performance analysis
**Impact**: Cannot optimize or debug cache issues in production

**Implementation Tasks**:
- [ ] **6.1** Add detailed cache metrics:
  ```cpp
  struct DetailedCacheMetrics {
      float hitRate;
      float missRate;
      size_t totalQueries;
      float averageQueryTime;
      size_t cacheSize;
      size_t memoryUsage;
      float evictionRate;
      std::map<std::string, size_t> queryTypeBreakdown;
  };
  ```
- [ ] **6.2** Add per-query-type performance tracking
- [ ] **6.3** Implement cache heatmap for spatial access patterns
- [ ] **6.4** Add cache effectiveness alerts (hit rate drops, memory spikes)
- [ ] **6.5** Create cache performance regression tests

### 7. Optimize Hash Function Performance
**Issue**: Current hashing may be expensive for high-frequency queries
**Impact**: Cache overhead negating cache benefits

**Analysis Tasks**:
- [ ] **7.1** Profile hash function performance vs cache lookup time
- [ ] **7.2** Compare hash collision rates with different hash functions

**Implementation Tasks**:
- [ ] **7.3** Optimize frustum hash calculation:
  - Pre-compute hash components
  - Use SIMD for plane processing if beneficial
  - Cache hash values when possible
- [ ] **7.4** Consider alternative hash functions (xxHash, FNV-1a)
- [ ] **7.5** Add hash function benchmarking tools

---

## 📈 MEDIUM PRIORITY (Enhancements)

### 8. Add Pokemon-Specific Cache Optimizations
**Issue**: Current cache is generic, not optimized for Pokemon gameplay patterns
**Impact**: Missing Pokemon-specific optimization opportunities

**Implementation Tasks**:
- [ ] **8.1** Add creature movement prediction caching:
  ```cpp
  void preCacheCreatureMovement(EntityID creature, glm::vec3 direction, float speed);
  ```
- [ ] **8.2** Implement battle-zone cache pinning (never evict battle area)
- [ ] **8.3** Add player-centered cache prioritization
- [ ] **8.4** Implement grass/environmental system cache clustering
- [ ] **8.5** Add cache warming for area transitions

### 9. Implement Cache Persistence and Sharing
**Issue**: Cache starts empty every session, no sharing between systems
**Impact**: Cold start performance, missed optimization opportunities

**Implementation Tasks**:
- [ ] **9.1** Add cache warming from previous session data
- [ ] **9.2** Implement cross-system cache sharing (render + AI + physics)
- [ ] **9.3** Add cache export/import for level streaming
- [ ] **9.4** Implement cache compression for memory efficiency

### 10. Add Advanced Debugging and Analysis Tools
**Issue**: Limited tools for understanding cache behavior and optimizing
**Impact**: Difficult to tune and optimize cache performance

**Implementation Tasks**:
- [ ] **10.1** Add cache visualization tools:
  ```cpp
  void exportCacheHeatmap(const std::string& filename);
  void visualizeSpatialAccess();
  ```
- [ ] **10.2** Implement cache replay system for debugging
- [ ] **10.3** Add cache diff tools for comparing configurations
- [ ] **10.4** Create automated cache tuning recommendations

---

## 🧪 TESTING & VALIDATION

### 11. Comprehensive Cache Testing Suite
**Implementation Tasks**:
- [ ] **11.1** Unit tests for all cache operations
- [ ] **11.2** Thread safety tests (if lock-free chosen)
- [ ] **11.3** Memory pressure tests
- [ ] **11.4** Cache corruption and recovery tests
- [ ] **11.5** Performance regression tests
- [ ] **11.6** Pokemon gameplay scenario tests:
  - Overworld exploration with moving creatures
  - Battle transitions with camera changes
  - Dense creature areas (tall grass)
  - Fast travel / area loading

### 12. Production Readiness Validation
**Validation Tasks**:
- [ ] **12.1** Sustained high-load testing (1000+ entities, 1+ hour)
- [ ] **12.2** Memory leak detection
- [ ] **12.3** Performance profiling under various Pokemon scenarios
- [ ] **12.4** Cache effectiveness measurement in realistic gameplay
- [ ] **12.5** Stress testing with deliberately adversarial access patterns

---

## 📋 IMPLEMENTATION PHASES

### **Phase 1: Critical Architecture Fixes (Week 1)**
- Fix lock-free architecture (Tasks 1.1-1.6)
- Replace nuclear invalidation (Tasks 2.1-2.8)
- Add basic error handling (Tasks 4.1-4.4)

### **Phase 2: Precision and Performance (Week 2)**
- Fix floating-point issues (Tasks 3.1-3.9)
- Implement proper eviction (Tasks 5.1-5.5)
- Add production metrics (Tasks 6.1-6.5)

### **Phase 3: Pokemon Optimization (Week 3)**
- Pokemon-specific optimizations (Tasks 8.1-8.5)
- Advanced debugging tools (Tasks 10.1-10.4)
- Hash function optimization (Tasks 7.1-7.5)

### **Phase 4: Production Polish (Week 4)**
- Comprehensive testing (Tasks 11.1-11.6)
- Production validation (Tasks 12.1-12.5)
- Cache persistence features (Tasks 9.1-9.4)

---

## 🎯 SUCCESS CRITERIA

**Phase 1 Success:**
- [ ] No data races or concurrency issues
- [ ] Cache hit rate >70% with 20% moving entities
- [ ] No silent failures or memory leaks

**Phase 2 Success:**
- [ ] Cache hit rate >85% with realistic Pokemon movement
- [ ] Hash collision rate <5%
- [ ] Memory usage stable under sustained load

**Final Success:**
- [ ] Cache hit rate >90% in typical Pokemon gameplay
- [ ] Zero cache-related crashes or data corruption
- [ ] Cache overhead <10% of total spatial query time
- [ ] Memory usage <200MB for typical Pokemon overworld

---

*This TODO represents the roadmap from "proof-of-concept cache" to "production-grade spatial caching system worthy of a Pokemon-scale game engine."*