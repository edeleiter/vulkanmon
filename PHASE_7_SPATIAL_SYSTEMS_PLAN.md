# Phase 7: Integrated Spatial Systems + Creature Framework

## Priority: HIGH - Critical for Open World Pokemon
Based on analysis of current RenderSystem, we need robust spatial systems before spawning hundreds of creatures across large worlds.

---

## Current Spatial System Status

### ✅ What's Working
- Basic render command sorting (layer/material/distance)
- Simple distance-based culling (`maxRenderDistance = 1000.0f`)
- ECS integration with efficient component iteration

### ❌ Critical Missing Systems
- **Spatial Partitioning**: Currently iterating ALL entities every frame
- **Proper Frustum Culling**: Only distance-based, not view frustum
- **LOD System**: Framework exists but not implemented
- **Scene Graph**: No hierarchical transforms for complex scenes

---

## Integrated Implementation Plan

### **Phase 7.1: Spatial Partitioning Foundation (Week 1)**

#### Task 1: Octree Spatial Manager
**File**: `src/spatial/SpatialManager.h`

```cpp
#pragma once

#include "../core/Entity.h"
#include "../components/Transform.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>
#include <memory>

namespace VulkanMon {

struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;

    BoundingBox() : min(FLT_MAX), max(-FLT_MAX) {}
    BoundingBox(const glm::vec3& center, float radius)
        : min(center - radius), max(center + radius) {}

    bool contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool intersects(const BoundingBox& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    glm::vec3 getCenter() const { return (min + max) * 0.5f; }
    glm::vec3 getSize() const { return max - min; }
};

struct Frustum {
    glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far

    void updateFromMatrix(const glm::mat4& viewProjectionMatrix);
    bool intersects(const BoundingBox& box) const;
    bool contains(const glm::vec3& point) const;
};

class OctreeNode {
public:
    static constexpr int MAX_ENTITIES_PER_NODE = 16;
    static constexpr int MAX_DEPTH = 8;

private:
    BoundingBox bounds_;
    std::vector<EntityID> entities_;
    std::unique_ptr<OctreeNode> children_[8];
    int depth_;
    bool is_leaf_;

public:
    OctreeNode(const BoundingBox& bounds, int depth = 0)
        : bounds_(bounds), depth_(depth), is_leaf_(true) {}

    void insert(EntityID entity, const glm::vec3& position);
    void remove(EntityID entity, const glm::vec3& position);
    void update(EntityID entity, const glm::vec3& oldPos, const glm::vec3& newPos);

    void query(const BoundingBox& region, std::vector<EntityID>& results) const;
    void query(const Frustum& frustum, std::vector<EntityID>& results) const;
    void queryRadius(const glm::vec3& center, float radius, std::vector<EntityID>& results) const;

    void clear();
    void subdivide();

    const BoundingBox& getBounds() const { return bounds_; }
    size_t getEntityCount() const;
    void getStatistics(int& nodeCount, int& maxDepth, int& totalEntities) const;

private:
    int getChildIndex(const glm::vec3& position) const;
    BoundingBox getChildBounds(int childIndex) const;
};

class SpatialManager {
private:
    std::unique_ptr<OctreeNode> octree_;
    BoundingBox worldBounds_;

    // Entity tracking for updates
    std::unordered_map<EntityID, glm::vec3> entityPositions_;

public:
    SpatialManager(const BoundingBox& worldBounds);
    ~SpatialManager() = default;

    // Entity management
    void addEntity(EntityID entity, const glm::vec3& position);
    void removeEntity(EntityID entity);
    void updateEntity(EntityID entity, const glm::vec3& newPosition);

    // Spatial queries
    std::vector<EntityID> queryRegion(const BoundingBox& region) const;
    std::vector<EntityID> queryFrustum(const Frustum& frustum) const;
    std::vector<EntityID> queryRadius(const glm::vec3& center, float radius) const;

    // Nearest neighbor queries (for creature AI)
    std::vector<EntityID> findNearestEntities(const glm::vec3& position, int count, float maxDistance = FLT_MAX) const;
    EntityID findNearestEntity(const glm::vec3& position, float maxDistance = FLT_MAX) const;

    // Performance and debugging
    void getStatistics(int& nodeCount, int& maxDepth, int& totalEntities) const;
    void clear();

    const BoundingBox& getWorldBounds() const { return worldBounds_; }
};

} // namespace VulkanMon
```

#### Task 2: Enhanced RenderSystem with Spatial Culling
**File**: `src/systems/SpatialRenderSystem.h`

```cpp
#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"
#include "../spatial/SpatialManager.h"
#include "../rendering/VulkanRenderer.h"
#include "CameraSystem.h"
#include <vector>

namespace VulkanMon {

class SpatialRenderSystem : public System<Transform, Renderable> {
public:
    struct LODLevel {
        float distance;      // Distance threshold for this LOD
        float quality;       // Quality multiplier (1.0 = full, 0.5 = half detail)
        std::string meshSuffix; // "_lod0", "_lod1", "_lod2"
    };

    struct RenderStats {
        size_t entitiesConsidered = 0;
        size_t frustumCulled = 0;
        size_t distanceCulled = 0;
        size_t lodReduced = 0;
        size_t rendered = 0;
        float cullingTimeMs = 0.0f;
        float renderTimeMs = 0.0f;
    };

private:
    SpatialManager* spatialManager_;
    CameraSystem* cameraSystem_;

    // LOD configuration
    std::vector<LODLevel> lodLevels_;
    float maxRenderDistance_ = 500.0f;  // Reduced for Pokemon world scale

    // Performance tracking
    RenderStats frameStats_;

    // Render command structure
    struct RenderCommand {
        EntityID entity;
        glm::mat4 modelMatrix;
        std::string meshPath;
        uint32_t materialId;
        float distanceToCamera;
        int lodLevel;
        uint32_t renderLayer;

        bool operator<(const RenderCommand& other) const {
            // Sort by layer, then material, then distance
            if (renderLayer != other.renderLayer) return renderLayer < other.renderLayer;
            if (materialId != other.materialId) return materialId < other.materialId;
            return distanceToCamera < other.distanceToCamera;
        }
    };

public:
    SpatialRenderSystem(SpatialManager* spatial, CameraSystem* camera);

    void update(float deltaTime, EntityManager& entityManager) override;
    void render(VulkanRenderer& renderer, EntityManager& entityManager) override;

    // Configuration
    void setMaxRenderDistance(float distance) { maxRenderDistance_ = distance; }
    void addLODLevel(float distance, float quality, const std::string& suffix);
    void setupDefaultLODLevels();

    // Statistics
    const RenderStats& getLastFrameStats() const { return frameStats_; }

private:
    void performSpatialCulling(const glm::vec3& cameraPos, const Frustum& frustum,
                              std::vector<EntityID>& visibleEntities);

    int calculateLODLevel(float distance) const;
    std::string getLODMeshPath(const std::string& basePath, int lodLevel) const;

    void collectRenderCommands(const std::vector<EntityID>& entities,
                              const glm::vec3& cameraPos,
                              EntityManager& entityManager,
                              std::vector<RenderCommand>& commands);
};

} // namespace VulkanMon
```

### **Phase 7.2: Creature-Optimized Systems (Week 2)**

#### Task 3: Creature Spatial Component
**File**: `src/components/SpatialComponent.h`

```cpp
#pragma once

#include "../core/Entity.h"
#include "../spatial/SpatialManager.h"
#include <glm/glm.hpp>

namespace VulkanMon {

enum class SpatialBehavior : uint8_t {
    STATIC = 0,     // Never moves (trees, rocks)
    DYNAMIC = 1,    // Moves frequently (creatures, player)
    OCCASIONAL = 2  // Moves rarely (NPCs, some creatures)
};

struct SpatialComponent {
    VKMON_COMPONENT(SpatialComponent)

    // Spatial properties
    float boundingRadius = 1.0f;        // For sphere-based culling
    BoundingBox localBounds;             // Local space bounding box
    SpatialBehavior behavior = SpatialBehavior::DYNAMIC;

    // Spatial optimization flags
    bool needsSpatialUpdate = true;      // Entity moved, needs octree update
    bool isVisible = true;               // Last frame visibility result
    float lastCullingDistance = 0.0f;    // Distance when last culled

    // Creature-specific spatial data
    float detectionRadius = 15.0f;       // How far creature can detect others
    float territoryRadius = 25.0f;       // Territory size for territorial creatures
    glm::vec3 homePosition = glm::vec3(0.0f); // Original spawn position

    // Performance tracking
    uint32_t spatialQueryCount = 0;      // How many queries this entity made
    float timeSinceLastQuery = 0.0f;     // Throttle expensive queries

    // Methods
    BoundingBox getWorldBounds(const glm::vec3& worldPosition) const {
        return BoundingBox(worldPosition - boundingRadius, worldPosition + boundingRadius);
    }

    bool isNearHome(const glm::vec3& currentPos, float threshold = 5.0f) const {
        return glm::distance(currentPos, homePosition) <= threshold;
    }

    bool shouldThrottleQueries(float deltaTime, float maxQueryFrequency = 10.0f) {
        timeSinceLastQuery += deltaTime;
        float minInterval = 1.0f / maxQueryFrequency;

        if (timeSinceLastQuery >= minInterval) {
            timeSinceLastQuery = 0.0f;
            return false; // Don't throttle
        }
        return true; // Throttle this query
    }
};

} // namespace VulkanMon
```

#### Task 4: Creature Behavior System with Spatial Awareness
**File**: `src/systems/CreatureBehaviorSystem.h`

```cpp
#pragma once

#include "../core/SystemImpl.h"
#include "../components/CreatureComponent.h"
#include "../components/SpatialComponent.h"
#include "../components/Transform.h"
#include "../spatial/SpatialManager.h"
#include "../utils/Logger.h"

namespace VulkanMon {

class CreatureBehaviorSystem : public System<CreatureComponent, Transform, SpatialComponent> {
private:
    SpatialManager* spatialManager_;
    EntityID playerEntity_ = INVALID_ENTITY;

    // Behavior performance tracking
    struct BehaviorStats {
        size_t creaturesUpdated = 0;
        size_t spatialQueriesPerformed = 0;
        float averageNearbyCreatures = 0.0f;
        float updateTimeMs = 0.0f;
    } frameStats_;

public:
    CreatureBehaviorSystem(SpatialManager* spatial) : spatialManager_(spatial) {}

    void setPlayerEntity(EntityID player) { playerEntity_ = player; }

    void update(float deltaTime, EntityManager& entityManager) override;
    void render(VulkanRenderer& renderer, EntityManager& entityManager) override {} // No rendering

    const BehaviorStats& getLastFrameStats() const { return frameStats_; }

private:
    // Core behavior functions
    void updateCreatureBehavior(EntityID entity, CreatureComponent& creature,
                               Transform& transform, SpatialComponent& spatial,
                               float deltaTime, EntityManager& entityManager);

    // Specific behavior implementations
    void updateWildBehavior(EntityID entity, CreatureComponent& creature,
                           Transform& transform, SpatialComponent& spatial,
                           float deltaTime, EntityManager& entityManager);

    void updateAlphaBehavior(EntityID entity, CreatureComponent& creature,
                            Transform& transform, SpatialComponent& spatial,
                            float deltaTime, EntityManager& entityManager);

    void updateHerdBehavior(EntityID entity, CreatureComponent& creature,
                           Transform& transform, SpatialComponent& spatial,
                           float deltaTime, EntityManager& entityManager);

    // Spatial query helpers
    std::vector<EntityID> findNearbyCreatures(const glm::vec3& position, float radius);
    EntityID findNearestCreature(const glm::vec3& position, float maxDistance = 50.0f);
    glm::vec3 getPlayerPosition(EntityManager& entityManager);
    float getDistanceToPlayer(const glm::vec3& creaturePos, EntityManager& entityManager);

    // Movement helpers
    void moveCreature(Transform& transform, const glm::vec3& direction,
                     float speed, float deltaTime);
    void faceDirection(Transform& transform, const glm::vec3& direction);
    glm::vec3 calculateFleeDirection(const glm::vec3& creaturePos, const glm::vec3& threatPos);
    glm::vec3 generateWanderTarget(const glm::vec3& currentPos, const glm::vec3& homePos,
                                  float wanderRadius);

    // Animation and state helpers
    void setAnimationState(CreatureComponent& creature, CreatureAnimState newState);
    void updateStateTimer(CreatureComponent& creature, float deltaTime);
};

} // namespace VulkanMon
```

### **Phase 7.3: Integration & Performance (Week 3)**

#### Task 5: Spatial System Integration
1. **Modify World class** to include SpatialManager
2. **Update Application.cpp** to initialize spatial systems
3. **Integrate with ECS Inspector** for spatial debugging
4. **Performance profiling** with 100+ creatures

#### Task 6: Pokemon-Specific Optimizations
```cpp
// Creature spawning with spatial awareness
class CreatureSpawnSystem : public System<> {
private:
    SpatialManager* spatialManager_;

    struct SpawnZone {
        BoundingBox bounds;
        std::vector<uint32_t> allowedSpecies;
        float spawnDensity;     // Creatures per square unit
        float currentDensity;   // Current creature count
    };

    std::vector<SpawnZone> spawnZones_;

public:
    void spawnCreaturesInZone(const SpawnZone& zone, EntityManager& entityManager) {
        // Use spatial queries to check current density
        auto existingCreatures = spatialManager_->queryRegion(zone.bounds);

        if (existingCreatures.size() < zone.spawnDensity * zone.bounds.getSize().x * zone.bounds.getSize().z) {
            // Find safe spawn location
            glm::vec3 spawnPos = findSafeSpawnLocation(zone.bounds);

            // Spawn creature with spatial component
            EntityID creature = spawnCreature(chooseRandomSpecies(zone), spawnPos, entityManager);

            // Add to spatial manager
            spatialManager_->addEntity(creature, spawnPos);
        }
    }

private:
    glm::vec3 findSafeSpawnLocation(const BoundingBox& zone) {
        // Use spatial queries to avoid spawning creatures on top of each other
        for (int attempts = 0; attempts < 10; ++attempts) {
            glm::vec3 candidate = randomPointInBox(zone);

            auto nearby = spatialManager_->queryRadius(candidate, 3.0f); // 3m minimum distance
            if (nearby.empty()) {
                return candidate;
            }
        }
        return randomPointInBox(zone); // Fallback
    }
};
```

---

## Performance Targets

### **Before Spatial Systems (Current)**
- ✅ 5-10 entities: 60+ FPS
- ❌ 50+ entities: Performance degrades (no spatial culling)

### **After Spatial Systems (Goal)**
- ✅ 200+ creatures in world: 60+ FPS
- ✅ 50+ creatures visible: Efficient LOD rendering
- ✅ Spatial queries: <1ms per frame
- ✅ Octree updates: <0.5ms per frame

---

## Integration with Existing Systems

### **RenderSystem Enhancement**
```cpp
// Replace current RenderSystem with SpatialRenderSystem
world_->removeSystem<RenderSystem>();
auto* spatialRender = world_->addSystem<SpatialRenderSystem>(spatialManager_, cameraSystem_);
spatialRender->setupDefaultLODLevels();
```

### **ECS Inspector Integration**
```cpp
// Add spatial debugging to ECS Inspector
void ECSInspector::renderSpatialDebugInfo() {
    if (ImGui::CollapsingHeader("Spatial Systems")) {
        auto stats = renderSystem_->getLastFrameStats();
        ImGui::Text("Entities Considered: %zu", stats.entitiesConsidered);
        ImGui::Text("Frustum Culled: %zu", stats.frustumCulled);
        ImGui::Text("Distance Culled: %zu", stats.distanceCulled);
        ImGui::Text("LOD Reduced: %zu", stats.lodReduced);
        ImGui::Text("Rendered: %zu", stats.rendered);

        // Octree visualization
        if (ImGui::Button("Show Octree Bounds")) {
            showOctreeBounds_ = !showOctreeBounds_;
        }
    }
}
```

---

## Success Criteria

### **Week 1: Spatial Foundation**
- [ ] Octree implementation with entity insertion/removal
- [ ] Frustum culling working with camera system
- [ ] Basic LOD system reducing mesh detail by distance

### **Week 2: Creature Integration**
- [ ] CreatureBehaviorSystem using spatial queries for AI
- [ ] 50+ creatures spawned with efficient spatial updates
- [ ] Creature detection ranges working through spatial system

### **Week 3: Performance & Polish**
- [ ] 200+ creatures in world maintaining 60+ FPS
- [ ] Spatial debugging in ECS Inspector
- [ ] Performance profiling showing <2ms total spatial overhead

**This integrated approach gives us the spatial foundation needed for authentic Pokemon open world gameplay!**