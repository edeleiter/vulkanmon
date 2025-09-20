# Performance Regression Testing Plan

## 🎯 **OBJECTIVE**
Create automated performance tests to catch regressions like the 22ms CreatureDetectionSystem bottleneck we just discovered.

## 🧪 **PERFORMANCE TEST CATEGORIES**

### **1. Frame Time Benchmarks**
```cpp
// tests_cpp/performance/test_FrameTimeBenchmarks.cpp
TEST_CASE("Frame Time Performance Regression Tests", "[performance][benchmark]") {
    SECTION("529 Creature Scene - Target: <16.67ms total frame time") {
        // Setup scene with 529 creatures
        // Measure actual frame times over 300 frames
        // REQUIRE(averageFrameTime < 16.67f);
    }

    SECTION("CreatureDetectionSystem - Target: <5ms") {
        // Isolate CreatureDetectionSystem timing
        // REQUIRE(detectionSystemTime < 5.0f);
    }

    SECTION("Spatial Query Performance - Target: <10 queries per frame") {
        // Count spatial queries per frame
        // REQUIRE(queriesPerFrame < 10);
    }
}
```

### **2. Memory Allocation Tracking**
```cpp
TEST_CASE("Memory Performance Regression Tests", "[performance][memory]") {
    SECTION("Hot Path Allocations - Target: 0 allocations") {
        // Track allocations during frame processing
        // REQUIRE(hotPathAllocations == 0);
    }
}
```

### **3. System-Level Performance**
```cpp
TEST_CASE("ECS System Performance", "[performance][ecs]") {
    SECTION("Individual System Timing") {
        // Measure each ECS system update time
        // RenderSystem, SpatialSystem, CreatureDetection, etc.
    }
}
```

### **4. GPU Performance Validation**
```cpp
TEST_CASE("Render Performance", "[performance][gpu]") {
    SECTION("Instanced Rendering Efficiency") {
        // Verify GPU instancing is working
        // Check draw call counts vs entity counts
    }
}
```

## 🚨 **CRITICAL PERFORMANCE THRESHOLDS**

### **Frame Time Targets** (for 529 creatures):
- **Total frame time**: <16.67ms (60 FPS)
- **CreatureDetectionSystem**: <5ms
- **Spatial queries**: <10 per frame
- **Memory allocations**: 0 in hot paths

### **Scaling Targets** (for 1000+ creatures):
- **Total frame time**: <16.67ms (60 FPS)
- **CreatureDetectionSystem**: <8ms
- **Spatial queries**: <20 per frame

## 🔧 **IMPLEMENTATION STRATEGY**

### **Phase 1: Basic Performance Tests**
1. **Frame timing infrastructure** - High-resolution timers
2. **System timing breakdown** - Per-ECS system measurement
3. **Automated thresholds** - Fail tests if performance regresses

### **Phase 2: Advanced Performance Monitoring**
1. **Memory allocation tracking** - Catch dynamic allocations
2. **Mutex contention detection** - Identify locking bottlenecks
3. **Cache performance** - Hit rates and query efficiency

### **Phase 3: CI Integration**
1. **Automated performance runs** - Part of PR validation
2. **Performance regression reports** - Clear failure diagnostics
3. **Benchmark comparison** - Before/after performance data

## 📊 **MEASUREMENT INFRASTRUCTURE**

### **Performance Profiler Class**
```cpp
class PerformanceProfiler {
    struct SystemMetrics {
        float updateTimeMs;
        size_t memoryAllocations;
        size_t mutexContentions;
    };

public:
    void startFrame();
    void endFrame();
    void recordSystemTime(const std::string& systemName, float timeMs);
    SystemMetrics getSystemMetrics(const std::string& systemName);
    bool validatePerformanceThresholds();
};
```

### **Automated Test Integration**
```cpp
// In each performance test:
PerformanceProfiler profiler;
// ... run test scenario ...
REQUIRE(profiler.validatePerformanceThresholds());
```

## 🎯 **SUCCESS CRITERIA**

### **Immediate Goals**:
- [ ] Detect 22ms CreatureDetectionSystem regression automatically
- [ ] Catch spatial query explosion (529 individual queries)
- [ ] Validate batched query improvements

### **Long-term Goals**:
- [ ] Prevent any frame time regression >20%
- [ ] Catch memory allocation spikes in hot paths
- [ ] Monitor scaling performance (100 → 1000 creatures)

---

*This performance testing plan ensures we never again miss critical performance regressions like the spatial query storm that caused 22ms frame spikes.*