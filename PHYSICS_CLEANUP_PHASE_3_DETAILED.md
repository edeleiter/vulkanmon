# Physics Cleanup Phase 3: Engine Optimization & Advanced Features

## 🎯 **MISSION OBJECTIVE**
Transform VulkanMon's physics system into a professional-grade game engine with advanced Jolt Physics features, performance optimization, and robust error handling.

## 📊 **SUCCESS CRITERIA**
- ✅ Professional thread and memory management
- ✅ Robust collision layer system for any game architecture
- ✅ Performance optimization for massive entity counts (200+)
- ✅ Comprehensive error handling and diagnostics
- ✅ Advanced Jolt Physics foundation features
- ✅ Configurable engine parameters for different deployment targets

---

## 📋 **DETAILED IMPLEMENTATION TASKS**

### **Task 3.1: Thread Configuration & Management**
**Files**: `src/systems/PhysicsSystem.h/.cpp`
**Estimated Time**: 20 minutes

#### **Add Thread Management to PhysicsSystem.h**:

```cpp
// =============================================================================
// THREAD CONFIGURATION & MANAGEMENT
// =============================================================================

// Thread configuration options
struct ThreadConfig {
    int numThreads = -1;                    // -1 = auto-detect, 0 = single-threaded
    int tempAllocatorSize = 10 * 1024 * 1024; // 10MB default for temp allocations
    bool enableProfiling = false;           // Enable Jolt profiling integration
    std::string profileOutputPath = "";     // Where to write profiling data
};

// Configure physics threading
void configureThreading(const ThreadConfig& config);
ThreadConfig getCurrentThreadConfig() const;

// Thread performance monitoring
struct ThreadMetrics {
    int activeThreads = 0;
    float threadUtilization = 0.0f;        // 0.0-1.0
    float averageJobTime = 0.0f;           // ms
    int jobsQueued = 0;
    int jobsCompleted = 0;
};
ThreadMetrics getThreadMetrics() const;

private:
    ThreadConfig threadConfig_;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem_;
    mutable ThreadMetrics threadMetrics_;
```

#### **Implementation in PhysicsSystem.cpp**:

```cpp
void PhysicsSystem::configureThreading(const ThreadConfig& config) {
    if (initialized_) {
        VKMON_WARNING("Cannot reconfigure threading while physics system is running");
        return;
    }

    threadConfig_ = config;

    // Determine thread count
    int threads = config.numThreads;
    if (threads <= 0) {
        threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    }

    threadCount_ = static_cast<uint32_t>(threads);

    VKMON_INFO("Physics threading configured: " + std::to_string(threads) + " threads, " +
               std::to_string(config.tempAllocatorSize / 1024 / 1024) + "MB temp allocator");

    // Create job system with configured parameters
    jobSystem_ = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        static_cast<uint32_t>(threads - 1) // Main thread + worker threads
    );
}

PhysicsSystem::ThreadConfig PhysicsSystem::getCurrentThreadConfig() const {
    return threadConfig_;
}

PhysicsSystem::ThreadMetrics PhysicsSystem::getThreadMetrics() const {
    ThreadMetrics metrics = threadMetrics_;

    if (jobSystem_) {
        // Update metrics from job system (simplified - Jolt doesn't expose all internals)
        metrics.activeThreads = static_cast<int>(threadCount_);

        // Estimate utilization based on update timing
        float targetFrameTime = 16.67f; // 60 FPS
        metrics.threadUtilization = std::min(1.0f, stats_.updateTime / targetFrameTime);

        // Estimate job metrics (Jolt doesn't expose these directly)
        metrics.averageJobTime = stats_.updateTime / std::max(1, metrics.activeThreads);
        metrics.jobsQueued = stats_.activeRigidBodies; // Approximate
        metrics.jobsCompleted = metrics.jobsQueued; // Assume all complete
    }

    return metrics;
}
```

#### **Validation Steps:**
- [ ] Thread configuration works for different core counts
- [ ] Performance scales appropriately with thread count
- [ ] Thread metrics provide useful insights
- [ ] System handles single-threaded mode gracefully

---

### **Task 3.2: Memory Management & Optimization**
**Files**: `src/systems/PhysicsSystem.h/.cpp`
**Estimated Time**: 25 minutes

#### **Add Memory Management to PhysicsSystem.h**:

```cpp
// =============================================================================
// MEMORY MANAGEMENT & OPTIMIZATION
// =============================================================================

// Memory configuration
struct MemoryConfig {
    size_t maxBodies = 10000;              // Maximum number of physics bodies
    size_t maxBodyPairs = 65536;           // Maximum collision pairs
    size_t maxContactConstraints = 20480;   // Maximum contact constraints
    size_t tempAllocatorSize = 10 * 1024 * 1024; // Temp allocator size
    bool enableMemoryProfiling = false;    // Track memory allocations
};

void configureMemory(const MemoryConfig& config);
MemoryConfig getCurrentMemoryConfig() const;

// Memory usage tracking
struct MemoryMetrics {
    size_t totalAllocated = 0;             // Bytes allocated by Jolt
    size_t tempAllocatorUsed = 0;          // Temp allocator usage
    size_t bodyMemoryUsed = 0;             // Memory used by physics bodies
    size_t shapeMemoryUsed = 0;            // Memory used by collision shapes
    int allocationsPerFrame = 0;           // Allocations in last frame
    int deallocationsPerFrame = 0;         // Deallocations in last frame
};
MemoryMetrics getMemoryMetrics() const;

// Efficient body lifecycle management
void preallocateBodies(size_t count);
void cleanupOrphanedBodies();
void compactMemory();                      // Defragment physics memory

private:
    MemoryConfig memoryConfig_;
    mutable MemoryMetrics memoryMetrics_;
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator_;

    // Body pool management
    std::vector<JPH::BodyID> bodyPool_;    // Pre-allocated body IDs
    std::queue<JPH::BodyID> availableBodies_;
```

#### **Implementation in PhysicsSystem.cpp**:

```cpp
void PhysicsSystem::configureMemory(const MemoryConfig& config) {
    if (initialized_) {
        VKMON_WARNING("Cannot reconfigure memory while physics system is running");
        return;
    }

    memoryConfig_ = config;

    // Create temp allocator with configured size
    tempAllocator_ = std::make_unique<JPH::TempAllocatorImpl>(config.tempAllocatorSize);

    VKMON_INFO("Physics memory configured: " +
               std::to_string(config.maxBodies) + " max bodies, " +
               std::to_string(config.tempAllocatorSize / 1024 / 1024) + "MB temp allocator");
}

void PhysicsSystem::preallocateBodies(size_t count) {
    if (!joltPhysics_) return;

    bodyPool_.reserve(count);

    const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

    // Pre-create body IDs for efficient allocation
    for (size_t i = 0; i < count; ++i) {
        // Create a minimal body that we can reuse
        JPH::BodyCreationSettings bodySettings;
        bodySettings.SetShape(new JPH::SphereShape(0.5f));
        bodySettings.mPosition = JPH::Vec3::sZero();
        bodySettings.mMotionType = JPH::EMotionType::Dynamic;
        bodySettings.mObjectLayer = ObjectLayers::ENVIRONMENT;

        JPH::Body* body = bodyInterface.CreateBody(bodySettings);
        if (body) {
            JPH::BodyID bodyID = body->GetID();
            bodyPool_.push_back(bodyID);
            availableBodies_.push(bodyID);

            // Remove from simulation but keep allocated
            joltPhysics_->GetBodyInterface().RemoveBody(bodyID);
        }
    }

    VKMON_INFO("Pre-allocated " + std::to_string(bodyPool_.size()) + " physics bodies for efficient reuse");
}

void PhysicsSystem::cleanupOrphanedBodies() {
    if (!joltPhysics_) return;

    const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
    std::vector<JPH::BodyID> orphanedBodies;

    // Find bodies in Jolt that don't have corresponding entities
    for (const auto& [entityID, bodyID] : entityToBodyMap_) {
        if (!bodyInterface.IsAdded(bodyID)) {
            orphanedBodies.push_back(bodyID);
        }
    }

    // Clean up orphaned bodies
    for (JPH::BodyID bodyID : orphanedBodies) {
        auto it = bodyToEntityMap_.find(bodyID);
        if (it != bodyToEntityMap_.end()) {
            EntityID entityID = it->second;

            bodyToEntityMap_.erase(bodyID);
            entityToBodyMap_.erase(entityID);

            // Return to pool if it was pre-allocated
            auto poolIt = std::find(bodyPool_.begin(), bodyPool_.end(), bodyID);
            if (poolIt != bodyPool_.end()) {
                availableBodies_.push(bodyID);
            } else {
                bodyInterface.DestroyBody(bodyID);
            }
        }
    }

    if (!orphanedBodies.empty()) {
        VKMON_DEBUG("Cleaned up " + std::to_string(orphanedBodies.size()) + " orphaned physics bodies");
    }
}

void PhysicsSystem::compactMemory() {
    if (!joltPhysics_) return;

    // Force Jolt to compact its internal memory structures
    // Note: Jolt doesn't expose direct compaction, but we can trigger cleanup
    cleanupOrphanedBodies();

    // Update memory metrics after compaction
    updateMemoryMetrics();

    VKMON_DEBUG("Physics memory compaction completed");
}

PhysicsSystem::MemoryMetrics PhysicsSystem::getMemoryMetrics() const {
    MemoryMetrics metrics = memoryMetrics_;

    if (joltPhysics_) {
        // Estimate memory usage (Jolt doesn't expose detailed memory stats)
        metrics.bodyMemoryUsed = entityToBodyMap_.size() * sizeof(JPH::Body);
        metrics.totalAllocated = metrics.bodyMemoryUsed + (stats_.activeConstraints * 256); // Rough estimate

        if (tempAllocator_) {
            metrics.tempAllocatorUsed = tempAllocator_->GetSize() - tempAllocator_->GetFreeSize();
        }
    }

    return metrics;
}
```

#### **Validation Steps:**
- [ ] Memory preallocation improves body creation performance
- [ ] Orphaned body cleanup prevents memory leaks
- [ ] Memory metrics track allocations accurately
- [ ] Large entity counts don't cause memory fragmentation

---

### **Task 3.3: Enhanced Layer System**
**Files**: `src/systems/PhysicsSystem.h/.cpp`
**Estimated Time**: 25 minutes

#### **Add Advanced Layer Management**:

```cpp
// =============================================================================
// ENHANCED COLLISION LAYER SYSTEM
// =============================================================================

// Runtime layer configuration
struct LayerConfiguration {
    std::string name;
    uint32_t layerMask;
    uint32_t collidesWith;
    float priority = 1.0f;                 // Higher priority = processed first
    bool enableBroadPhase = true;          // Participate in broad phase
    bool enableNarrowPhase = true;         // Participate in narrow phase
};

// Layer management
void configureLayers(const std::vector<LayerConfiguration>& configs);
void setLayerCollision(uint32_t layer1, uint32_t layer2, bool canCollide);
bool getLayerCollision(uint32_t layer1, uint32_t layer2) const;

// Advanced collision filtering
void setLayerPriority(uint32_t layerMask, float priority);
float getLayerPriority(uint32_t layerMask) const;

// Runtime layer queries
std::vector<LayerConfiguration> getActiveLayerConfigurations() const;
uint32_t getEntitiesInLayer(uint32_t layerMask) const;

// Collision matrix management
struct CollisionMatrix {
    uint32_t layerPairs[32][32];           // Full collision matrix
    float priorities[32];                  // Layer priorities
    std::string names[32];                 // Layer names for debugging
};
CollisionMatrix getCollisionMatrix() const;
void setCollisionMatrix(const CollisionMatrix& matrix);

private:
    std::vector<LayerConfiguration> layerConfigurations_;
    std::unordered_map<uint32_t, float> layerPriorities_;
    mutable CollisionMatrix collisionMatrix_;
```

#### **Implementation in PhysicsSystem.cpp**:

```cpp
void PhysicsSystem::configureLayers(const std::vector<LayerConfiguration>& configs) {
    layerConfigurations_ = configs;

    // Update collision matrix based on configurations
    for (size_t i = 0; i < configs.size() && i < 32; ++i) {
        const auto& config = configs[i];

        layerPriorities_[config.layerMask] = config.priority;

        // Set up collision relationships
        for (size_t j = 0; j < configs.size() && j < 32; ++j) {
            const auto& otherConfig = configs[j];

            bool canCollide = (config.collidesWith & otherConfig.layerMask) != 0;
            setLayerCollision(config.layerMask, otherConfig.layerMask, canCollide);
        }

        VKMON_DEBUG("Configured layer '" + config.name +
                   "' (mask: " + std::to_string(config.layerMask) +
                   ", priority: " + std::to_string(config.priority) + ")");
    }

    // Update Jolt Physics layer configuration
    if (initialized_) {
        setupJoltLayers();
    }
}

void PhysicsSystem::setLayerCollision(uint32_t layer1, uint32_t layer2, bool canCollide) {
    // Find layer indices
    int idx1 = -1, idx2 = -1;
    for (int i = 0; i < 32; ++i) {
        if ((1u << i) == layer1) idx1 = i;
        if ((1u << i) == layer2) idx2 = i;
    }

    if (idx1 >= 0 && idx1 < 32 && idx2 >= 0 && idx2 < 32) {
        collisionMatrix_.layerPairs[idx1][idx2] = canCollide ? 1 : 0;
        collisionMatrix_.layerPairs[idx2][idx1] = canCollide ? 1 : 0; // Symmetric
    }
}

bool PhysicsSystem::getLayerCollision(uint32_t layer1, uint32_t layer2) const {
    int idx1 = -1, idx2 = -1;
    for (int i = 0; i < 32; ++i) {
        if ((1u << i) == layer1) idx1 = i;
        if ((1u << i) == layer2) idx2 = i;
    }

    if (idx1 >= 0 && idx1 < 32 && idx2 >= 0 && idx2 < 32) {
        return collisionMatrix_.layerPairs[idx1][idx2] != 0;
    }

    return false; // Default to no collision
}

void PhysicsSystem::setLayerPriority(uint32_t layerMask, float priority) {
    layerPriorities_[layerMask] = priority;

    // Update collision matrix
    for (int i = 0; i < 32; ++i) {
        if ((1u << i) == layerMask) {
            collisionMatrix_.priorities[i] = priority;
            break;
        }
    }
}

float PhysicsSystem::getLayerPriority(uint32_t layerMask) const {
    auto it = layerPriorities_.find(layerMask);
    return (it != layerPriorities_.end()) ? it->second : 1.0f;
}

uint32_t PhysicsSystem::getEntitiesInLayer(uint32_t layerMask) const {
    uint32_t count = 0;

    if (joltPhysics_) {
        const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

        for (const auto& [entityID, bodyID] : entityToBodyMap_) {
            JPH::ObjectLayer objectLayer = bodyInterface.GetObjectLayer(bodyID);
            uint32_t entityLayerMask = mapObjectLayerToLayerMask(objectLayer);

            if ((entityLayerMask & layerMask) != 0) {
                count++;
            }
        }
    }

    return count;
}

PhysicsSystem::CollisionMatrix PhysicsSystem::getCollisionMatrix() const {
    return collisionMatrix_;
}

void PhysicsSystem::setCollisionMatrix(const CollisionMatrix& matrix) {
    collisionMatrix_ = matrix;

    // Update layer priorities
    for (int i = 0; i < 32; ++i) {
        if (collisionMatrix_.priorities[i] > 0.0f) {
            layerPriorities_[1u << i] = collisionMatrix_.priorities[i];
        }
    }

    // Reconfigure Jolt layers
    if (initialized_) {
        setupJoltLayers();
    }
}
```

#### **Validation Steps:**
- [ ] Layer configuration works for complex game architectures
- [ ] Runtime collision matrix changes take effect immediately
- [ ] Layer priorities affect collision processing order
- [ ] Entity counting by layer is accurate

---

### **Task 3.4: Performance Optimization & LOD System**
**Files**: `src/systems/PhysicsSystem.h/.cpp`
**Estimated Time**: 20 minutes

#### **Add Performance Optimization System**:

```cpp
// =============================================================================
// PERFORMANCE OPTIMIZATION & LOD SYSTEM
// =============================================================================

// Level of detail for physics simulation
enum class PhysicsLOD : uint8_t {
    Full = 0,       // Complete physics simulation
    Simplified = 1, // Reduced collision checks, simplified forces
    Kinematic = 2,  // Position-only updates, no collision response
    Dormant = 3     // Minimal updates, distance-based activation
};

// LOD configuration
struct LODConfig {
    float fullSimulationRadius = 20.0f;    // Full physics within this radius
    float simplifiedRadius = 50.0f;        // Simplified physics beyond this
    float kinematicRadius = 100.0f;        // Kinematic only beyond this
    float dormantRadius = 200.0f;          // Dormant beyond this

    bool enableLOD = true;
    bool adaptiveQuality = true;            // Adjust LOD based on performance
    float targetFrameTime = 16.67f;        // Target frame time in ms
};

void configureLOD(const LODConfig& config);
LODConfig getLODConfig() const;

// LOD management
void updateEntityLOD(EntityManager& entityManager);
PhysicsLOD calculateLOD(EntityID entity, const glm::vec3& referencePosition,
                       EntityManager& entityManager) const;

// Batch processing for performance
void processBatchedUpdates(EntityManager& entityManager, float deltaTime);
void optimizePerformance(EntityManager& entityManager);

// Performance monitoring and adaptation
struct PerformanceMetrics {
    float averageFrameTime = 16.67f;
    float physicsLoadPercent = 0.0f;
    int entitiesProcessed = 0;
    int entitiesSkipped = 0;
    bool adaptiveOptimization = true;

    // LOD statistics
    int fullLODEntities = 0;
    int simplifiedLODEntities = 0;
    int kinematicLODEntities = 0;
    int dormantLODEntities = 0;
};

PerformanceMetrics getPerformanceMetrics() const;
void updatePerformanceMetrics();

private:
    LODConfig lodConfig_;
    mutable PerformanceMetrics performanceMetrics_;
    std::unordered_map<EntityID, PhysicsLOD> entityLODCache_;
```

#### **Implementation in PhysicsSystem.cpp**:

```cpp
void PhysicsSystem::configureLOD(const LODConfig& config) {
    lodConfig_ = config;

    VKMON_INFO("Physics LOD configured: " +
               std::to_string(config.fullSimulationRadius) + "m full, " +
               std::to_string(config.simplifiedRadius) + "m simplified, " +
               std::to_string(config.kinematicRadius) + "m kinematic");
}

void PhysicsSystem::updateEntityLOD(EntityManager& entityManager) {
    if (!lodConfig_.enableLOD) return;

    // Find reference position (camera or player)
    glm::vec3 referencePos(0.0f);
    // TODO: Get from camera system when available

    const auto& physicsEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();

    performanceMetrics_.fullLODEntities = 0;
    performanceMetrics_.simplifiedLODEntities = 0;
    performanceMetrics_.kinematicLODEntities = 0;
    performanceMetrics_.dormantLODEntities = 0;

    for (EntityID entity : physicsEntities) {
        PhysicsLOD lod = calculateLOD(entity, referencePos, entityManager);
        entityLODCache_[entity] = lod;

        // Update entity based on LOD
        if (entityManager.hasComponent<RigidBodyComponent>(entity)) {
            auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);

            switch (lod) {
                case PhysicsLOD::Full:
                    performanceMetrics_.fullLODEntities++;
                    // No changes needed - full simulation
                    break;

                case PhysicsLOD::Simplified:
                    performanceMetrics_.simplifiedLODEntities++;
                    // Reduce simulation frequency or accuracy
                    break;

                case PhysicsLOD::Kinematic:
                    performanceMetrics_.kinematicLODEntities++;
                    rigidBody.isKinematic = true;
                    break;

                case PhysicsLOD::Dormant:
                    performanceMetrics_.dormantLODEntities++;
                    rigidBody.isDynamic = false;
                    break;
            }
        }
    }
}

PhysicsSystem::PhysicsLOD PhysicsSystem::calculateLOD(EntityID entity,
                                                     const glm::vec3& referencePosition,
                                                     EntityManager& entityManager) const {
    if (!entityManager.hasComponent<Transform>(entity)) {
        return PhysicsLOD::Dormant;
    }

    auto& transform = entityManager.getComponent<Transform>(entity);
    float distance = glm::distance(transform.position, referencePosition);

    // Distance-based LOD
    if (distance <= lodConfig_.fullSimulationRadius) {
        return PhysicsLOD::Full;
    } else if (distance <= lodConfig_.simplifiedRadius) {
        return PhysicsLOD::Simplified;
    } else if (distance <= lodConfig_.kinematicRadius) {
        return PhysicsLOD::Kinematic;
    } else {
        return PhysicsLOD::Dormant;
    }
}

void PhysicsSystem::processBatchedUpdates(EntityManager& entityManager, float deltaTime) {
    // Group entities by LOD for batch processing
    std::vector<EntityID> fullLODEntities;
    std::vector<EntityID> simplifiedLODEntities;

    for (const auto& [entity, lod] : entityLODCache_) {
        switch (lod) {
            case PhysicsLOD::Full:
                fullLODEntities.push_back(entity);
                break;
            case PhysicsLOD::Simplified:
                simplifiedLODEntities.push_back(entity);
                break;
            default:
                break; // Skip kinematic and dormant for batch processing
        }
    }

    // Process full LOD entities first (highest priority)
    performanceMetrics_.entitiesProcessed = static_cast<int>(fullLODEntities.size() + simplifiedLODEntities.size());
    performanceMetrics_.entitiesSkipped = static_cast<int>(entityLODCache_.size() - performanceMetrics_.entitiesProcessed);
}

void PhysicsSystem::updatePerformanceMetrics() {
    // Update performance tracking with exponential moving average
    auto currentTime = std::chrono::high_resolution_clock::now();
    static auto lastUpdate = currentTime;
    auto frameDuration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastUpdate);
    float frameTime = frameDuration.count() / 1000.0f; // Convert to milliseconds

    performanceMetrics_.averageFrameTime = performanceMetrics_.averageFrameTime * 0.9f + frameTime * 0.1f;
    performanceMetrics_.physicsLoadPercent = (stats_.updateTime / performanceMetrics_.averageFrameTime) * 100.0f;

    lastUpdate = currentTime;

    // Adaptive optimization
    if (lodConfig_.adaptiveQuality && performanceMetrics_.physicsLoadPercent > 80.0f) {
        // Reduce LOD distances to improve performance
        lodConfig_.fullSimulationRadius *= 0.95f;
        lodConfig_.simplifiedRadius *= 0.95f;

        VKMON_WARNING("Physics performance critical - reducing LOD distances");
    } else if (performanceMetrics_.physicsLoadPercent < 40.0f) {
        // Increase LOD distances for better quality
        lodConfig_.fullSimulationRadius *= 1.02f;
        lodConfig_.simplifiedRadius *= 1.02f;
    }
}

PhysicsSystem::PerformanceMetrics PhysicsSystem::getPerformanceMetrics() const {
    return performanceMetrics_;
}
```

#### **Validation Steps:**
- [ ] LOD system correctly reduces simulation complexity at distance
- [ ] Performance monitoring accurately tracks physics load
- [ ] Adaptive optimization maintains target frame rates
- [ ] Batch processing improves performance with large entity counts

---

### **Task 3.5: Professional Error Handling**
**Files**: `src/systems/PhysicsSystem.h/.cpp`
**Estimated Time**: 15 minutes

#### **Add Comprehensive Error Handling**:

```cpp
// =============================================================================
// PROFESSIONAL ERROR HANDLING & DIAGNOSTICS
// =============================================================================

// Error categories
enum class PhysicsErrorLevel : uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2,
    Critical = 3
};

struct PhysicsError {
    PhysicsErrorLevel level;
    std::string message;
    std::string context;
    std::chrono::steady_clock::time_point timestamp;
    EntityID relatedEntity = 0;
};

// Error reporting and handling
void reportError(PhysicsErrorLevel level, const std::string& message,
                const std::string& context = "", EntityID entity = 0);
std::vector<PhysicsError> getRecentErrors(size_t maxCount = 10) const;
void clearErrorHistory();

// Graceful degradation
bool enableGracefulDegradation = true;
void handleCriticalError(const std::string& error);
void attemptRecovery();

// Debug visualization hooks
struct DebugVisualization {
    bool showCollisionShapes = false;
    bool showConstraints = false;
    bool showContactPoints = false;
    bool showVelocities = false;
    bool showAABBs = false;
};
void setDebugVisualization(const DebugVisualization& viz);
DebugVisualization getDebugVisualization() const;

private:
    std::deque<PhysicsError> errorHistory_;
    mutable std::mutex errorMutex_;
    DebugVisualization debugViz_;
```

#### **Implementation in PhysicsSystem.cpp**:

```cpp
void PhysicsSystem::reportError(PhysicsErrorLevel level, const std::string& message,
                               const std::string& context, EntityID entity) {
    std::lock_guard<std::mutex> lock(errorMutex_);

    PhysicsError error;
    error.level = level;
    error.message = message;
    error.context = context;
    error.timestamp = std::chrono::steady_clock::now();
    error.relatedEntity = entity;

    errorHistory_.push_back(error);

    // Keep only recent errors (last 100)
    while (errorHistory_.size() > 100) {
        errorHistory_.pop_front();
    }

    // Log to engine logger based on severity
    std::string fullMessage = "PhysicsSystem";
    if (!context.empty()) {
        fullMessage += " (" + context + ")";
    }
    fullMessage += ": " + message;
    if (entity != 0) {
        fullMessage += " [Entity: " + std::to_string(entity) + "]";
    }

    switch (level) {
        case PhysicsErrorLevel::Info:
            VKMON_INFO(fullMessage);
            break;
        case PhysicsErrorLevel::Warning:
            VKMON_WARNING(fullMessage);
            break;
        case PhysicsErrorLevel::Error:
            VKMON_ERROR(fullMessage);
            break;
        case PhysicsErrorLevel::Critical:
            VKMON_ERROR("CRITICAL: " + fullMessage);
            if (enableGracefulDegradation) {
                handleCriticalError(message);
            }
            break;
    }
}

std::vector<PhysicsSystem::PhysicsError> PhysicsSystem::getRecentErrors(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(errorMutex_);

    std::vector<PhysicsError> result;

    size_t startIndex = errorHistory_.size() > maxCount ? errorHistory_.size() - maxCount : 0;
    for (size_t i = startIndex; i < errorHistory_.size(); ++i) {
        result.push_back(errorHistory_[i]);
    }

    return result;
}

void PhysicsSystem::handleCriticalError(const std::string& error) {
    VKMON_ERROR("Attempting physics system recovery due to critical error: " + error);

    // Attempt graceful recovery
    try {
        // Save current state
        size_t entityCount = entityToBodyMap_.size();

        // Clear all physics bodies
        if (joltPhysics_) {
            for (const auto& [entityID, bodyID] : entityToBodyMap_) {
                joltPhysics_->GetBodyInterface().RemoveBody(bodyID);
                joltPhysics_->GetBodyInterface().DestroyBody(bodyID);
            }
        }

        entityToBodyMap_.clear();
        bodyToEntityMap_.clear();

        reportError(PhysicsErrorLevel::Warning,
                   "Cleared " + std::to_string(entityCount) + " physics bodies during recovery",
                   "CriticalErrorRecovery");

        // Reset statistics
        stats_ = PhysicsStats{};

    } catch (const std::exception& e) {
        reportError(PhysicsErrorLevel::Critical,
                   "Recovery failed: " + std::string(e.what()),
                   "RecoveryFailure");
    }
}

void PhysicsSystem::attemptRecovery() {
    if (!initialized_ || !joltPhysics_) {
        reportError(PhysicsErrorLevel::Error, "Cannot attempt recovery - system not initialized");
        return;
    }

    try {
        // Validate all body mappings
        std::vector<EntityID> invalidEntities;
        const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

        for (const auto& [entityID, bodyID] : entityToBodyMap_) {
            if (!bodyInterface.IsValidBodyPointer(joltPhysics_->GetBodyInterface().GetBody(bodyID))) {
                invalidEntities.push_back(entityID);
            }
        }

        // Clean up invalid mappings
        for (EntityID entity : invalidEntities) {
            auto it = entityToBodyMap_.find(entity);
            if (it != entityToBodyMap_.end()) {
                bodyToEntityMap_.erase(it->second);
                entityToBodyMap_.erase(it);
            }
        }

        if (!invalidEntities.empty()) {
            reportError(PhysicsErrorLevel::Warning,
                       "Cleaned up " + std::to_string(invalidEntities.size()) + " invalid body mappings",
                       "RecoveryCleanup");
        }

    } catch (const std::exception& e) {
        reportError(PhysicsErrorLevel::Error, "Recovery attempt failed: " + std::string(e.what()));
    }
}

void PhysicsSystem::setDebugVisualization(const DebugVisualization& viz) {
    debugViz_ = viz;

    // Configure Jolt debug rendering if available
    if (joltPhysics_) {
        // Note: In a full implementation, this would integrate with the rendering system
        // For now, just log the debug settings
        VKMON_DEBUG("Debug visualization updated - Shapes: " + std::to_string(viz.showCollisionShapes) +
                   ", Constraints: " + std::to_string(viz.showConstraints) +
                   ", Contacts: " + std::to_string(viz.showContactPoints));
    }
}

PhysicsSystem::DebugVisualization PhysicsSystem::getDebugVisualization() const {
    return debugViz_;
}
```

#### **Validation Steps:**
- [ ] Error reporting works for all severity levels
- [ ] Critical error recovery prevents system crashes
- [ ] Error history provides useful debugging information
- [ ] Debug visualization hooks integrate with rendering system

---

### **Task 3.6: Advanced Jolt Physics Features**
**Files**: `src/systems/PhysicsSystem.h/.cpp`
**Estimated Time**: 25 minutes

#### **Add Advanced Jolt Features**:

```cpp
// =============================================================================
// ADVANCED JOLT PHYSICS FEATURES
// =============================================================================

// Character controller foundation (generic, not game-specific)
class GenericCharacterController {
public:
    GenericCharacterController(EntityID entity, const CollisionComponent& collision,
                              PhysicsSystem* physicsSystem);
    ~GenericCharacterController();

    void updateMovement(const glm::vec3& inputDirection, float deltaTime);
    bool isGrounded() const;
    void jump(float jumpVelocity);

    glm::vec3 getVelocity() const;
    void setVelocity(const glm::vec3& velocity);

private:
    EntityID entityID_;
    PhysicsSystem* physicsSystem_;
    JPH::Ref<JPH::CharacterVirtual> character_;
};

std::unique_ptr<GenericCharacterController> createCharacterController(EntityID entity,
                                                                     EntityManager& entityManager);

// Compound shape system for complex collision volumes
JPH::ShapeRefC createCompoundShape(const std::vector<CollisionComponent>& shapes);
void addShapeToEntity(EntityID entity, const CollisionComponent& additionalShape,
                     const glm::vec3& offset, EntityManager& entityManager);

// Constraint system for joints and attachments
enum class ConstraintType : uint8_t {
    Fixed,          // Rigid connection
    Point,          // Ball joint
    Hinge,          // Rotating joint
    Distance,       // Maintains distance
    Spring          // Spring connection
};

struct PhysicsConstraint {
    EntityID entityA;
    EntityID entityB;
    ConstraintType type;
    glm::vec3 offsetA{0.0f};
    glm::vec3 offsetB{0.0f};

    // Constraint parameters
    float minDistance = 0.0f;
    float maxDistance = 10.0f;
    float springConstant = 100.0f;
    float damping = 10.0f;

    JPH::Ref<JPH::TwoBodyConstraint> joltConstraint;
};

void createConstraint(const PhysicsConstraint& constraint, EntityManager& entityManager);
void removeConstraint(EntityID entityA, EntityID entityB);
std::vector<PhysicsConstraint> getActiveConstraints() const;

// Trigger volume system for zone detection
struct TriggerZone {
    EntityID entity;
    float radius;
    uint32_t triggerLayers;
    std::function<void(EntityID)> onEnterCallback;
    std::function<void(EntityID)> onExitCallback;
    std::set<EntityID> entitiesInside;
};

void createTriggerZone(const TriggerZone& zone, EntityManager& entityManager);
void removeTriggerZone(EntityID entity);
void updateTriggerZones(EntityManager& entityManager);

private:
    std::vector<std::unique_ptr<GenericCharacterController>> characterControllers_;
    std::vector<PhysicsConstraint> activeConstraints_;
    std::vector<TriggerZone> triggerZones_;
```

#### **Simplified Implementation (Core Features)**:

```cpp
// Character Controller Implementation
PhysicsSystem::GenericCharacterController::GenericCharacterController(
    EntityID entity, const CollisionComponent& collision, PhysicsSystem* physicsSystem)
    : entityID_(entity), physicsSystem_(physicsSystem) {

    // Create character shape
    JPH::Ref<JPH::Shape> characterShape;
    if (collision.shapeType == CollisionComponent::ShapeType::Capsule) {
        characterShape = new JPH::CapsuleShape(collision.dimensions.y * 0.5f, collision.dimensions.x);
    } else {
        characterShape = new JPH::CapsuleShape(1.0f, 0.5f); // Default capsule
    }

    // Character settings
    JPH::CharacterVirtualSettings settings;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings.mMaxStrength = 100.0f;
    settings.mShape = characterShape;
    settings.mCharacterPadding = 0.02f;

    // Create character
    character_ = new JPH::CharacterVirtual(&settings, JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
                                          physicsSystem_->joltPhysics_.get());
}

void PhysicsSystem::GenericCharacterController::updateMovement(const glm::vec3& inputDirection,
                                                              float deltaTime) {
    if (!character_) return;

    // Convert input to Jolt coordinates
    JPH::Vec3 joltInput(inputDirection.x, inputDirection.y, inputDirection.z);

    // Update character with input
    character_->SetLinearVelocity(joltInput * 5.0f); // 5 m/s movement speed
    character_->Update(deltaTime, JPH::Vec3(0, -9.81f, 0), // gravity
                      *physicsSystem_->joltPhysics_);
}

bool PhysicsSystem::GenericCharacterController::isGrounded() const {
    return character_ ? character_->IsSupported() : false;
}

// Constraint System Implementation
void PhysicsSystem::createConstraint(const PhysicsConstraint& constraint,
                                    EntityManager& entityManager) {
    auto bodyA_it = entityToBodyMap_.find(constraint.entityA);
    auto bodyB_it = entityToBodyMap_.find(constraint.entityB);

    if (bodyA_it == entityToBodyMap_.end() || bodyB_it == entityToBodyMap_.end()) {
        reportError(PhysicsErrorLevel::Error, "Cannot create constraint - entities don't have physics bodies");
        return;
    }

    JPH::BodyID bodyA = bodyA_it->second;
    JPH::BodyID bodyB = bodyB_it->second;

    JPH::Ref<JPH::TwoBodyConstraint> joltConstraint;

    switch (constraint.type) {
        case ConstraintType::Fixed: {
            JPH::FixedConstraintSettings settings;
            joltConstraint = settings.Create(*joltPhysics_->GetBodyInterface().GetBody(bodyA),
                                           *joltPhysics_->GetBodyInterface().GetBody(bodyB));
            break;
        }
        case ConstraintType::Distance: {
            JPH::DistanceConstraintSettings settings;
            settings.mMinDistance = constraint.minDistance;
            settings.mMaxDistance = constraint.maxDistance;
            joltConstraint = settings.Create(*joltPhysics_->GetBodyInterface().GetBody(bodyA),
                                           *joltPhysics_->GetBodyInterface().GetBody(bodyB));
            break;
        }
        default:
            reportError(PhysicsErrorLevel::Warning, "Constraint type not yet implemented");
            return;
    }

    if (joltConstraint) {
        joltPhysics_->AddConstraint(joltConstraint);

        PhysicsConstraint newConstraint = constraint;
        newConstraint.joltConstraint = joltConstraint;
        activeConstraints_.push_back(newConstraint);

        reportError(PhysicsErrorLevel::Info, "Created constraint between entities " +
                   std::to_string(constraint.entityA) + " and " + std::to_string(constraint.entityB));
    }
}

// Trigger Zone Implementation
void PhysicsSystem::updateTriggerZones(EntityManager& entityManager) {
    if (!spatialSystem_) return;

    for (auto& zone : triggerZones_) {
        if (!entityManager.hasComponent<Transform>(zone.entity)) continue;

        auto& transform = entityManager.getComponent<Transform>(zone.entity);

        // Find entities in trigger radius
        auto entitiesInRange = spatialSystem_->queryRadius(transform.position, zone.radius, zone.triggerLayers);
        std::set<EntityID> currentEntities(entitiesInRange.begin(), entitiesInRange.end());

        // Check for entities entering
        for (EntityID entity : currentEntities) {
            if (zone.entitiesInside.find(entity) == zone.entitiesInside.end()) {
                zone.entitiesInside.insert(entity);
                if (zone.onEnterCallback) {
                    zone.onEnterCallback(entity);
                }
            }
        }

        // Check for entities leaving
        auto it = zone.entitiesInside.begin();
        while (it != zone.entitiesInside.end()) {
            if (currentEntities.find(*it) == currentEntities.end()) {
                EntityID leavingEntity = *it;
                if (zone.onExitCallback) {
                    zone.onExitCallback(leavingEntity);
                }
                it = zone.entitiesInside.erase(it);
            } else {
                ++it;
            }
        }
    }
}
```

#### **Validation Steps:**
- [ ] Character controllers provide smooth movement
- [ ] Constraint system connects entities correctly
- [ ] Compound shapes enable complex collision volumes
- [ ] Trigger zones detect enter/exit events reliably

---

## 🧪 **COMPREHENSIVE TESTING STRATEGY**

### **Engine Feature Testing (Estimated Time: 20 minutes)**
- [ ] **Thread Configuration**: Verify threading works for 1-16 cores
- [ ] **Memory Management**: Verify no leaks with 1000+ entity creation/destruction
- [ ] **Layer System**: Verify complex collision filtering scenarios
- [ ] **LOD System**: Verify performance scaling with distance

### **Performance Validation**:
- [ ] **Large Scale**: Verify 200+ entities maintain 60+ FPS
- [ ] **Error Handling**: Verify graceful degradation under stress
- [ ] **Advanced Features**: Verify constraints and controllers work correctly

---

## 🎯 **PHASE 3 COMPLETION CRITERIA**

### **Professional Engine Requirements:**
- ✅ **Thread Management**: Configurable for deployment targets
- ✅ **Memory Optimization**: Efficient lifecycle and cleanup
- ✅ **Layer System**: Supports any game collision architecture
- ✅ **Performance LOD**: Maintains target frame rates automatically
- ✅ **Error Handling**: Professional diagnostics and recovery

### **Advanced Physics Requirements:**
- ✅ **Character Controllers**: Generic foundation for any movement
- ✅ **Constraint System**: Joints and connections work reliably
- ✅ **Trigger Zones**: Zone detection for gameplay events
- ✅ **Compound Shapes**: Complex collision volumes supported

### **Quality Assurance Requirements:**
- ✅ **Error Reporting**: Comprehensive logging and diagnostics
- ✅ **Performance Monitoring**: Real-time metrics and adaptation
- ✅ **Test Coverage**: All new systems covered by validation
- ✅ **Documentation**: Professional API documentation

**Phase 3 SUCCESS = Professional game engine physics system!** 🌟🚀

### **Post-Phase 3 VulkanMon Status:**
With Phase 3 completion, VulkanMon will have achieved **professional game engine status** with physics capabilities suitable for any game architecture - from Pokemon-style adventures to first-person shooters, racing games, and complex simulations. The engine provides robust foundations while leaving game-specific logic to the developers who use it.