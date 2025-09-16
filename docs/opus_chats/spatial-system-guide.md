# Spatial System Implementation Guide
## Week 1-2: Building a Production-Ready Spatial Partitioning System

## Overview
The spatial system is the backbone of any open-world game. For VulkanMon, we'll implement a hybrid approach: starting with a simple spatial grid for broad-phase culling, then adding an octree for detailed queries. This matches how Pokémon Legends: Arceus handles its world.

## Architecture Design

### Core Components

```cpp
// include/spatial/SpatialSystem.h
#pragma once
#include "ecs/System.h"
#include "ecs/Component.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace VulkanMon {

// Spatial component for any entity that exists in world space
class SpatialComponent : public Component {
public:
    glm::vec3 position{0.0f};
    float radius = 1.0f;  // Bounding sphere radius
    uint32_t spatialLayers = 0x01;  // Bit flags for filtering
    bool isStatic = false;  // Optimization hint
    
    // Cached grid coordinates (updated by system)
    int32_t gridX = 0, gridY = 0, gridZ = 0;
};

// Spatial query result
struct SpatialQueryResult {
    EntityId entity;
    float distance;
    glm::vec3 position;
};

// Main spatial system
class SpatialSystem : public System {
public:
    SpatialSystem(float cellSize = 10.0f);
    
    void update(float deltaTime) override;
    void onComponentAdded(EntityId entity, ComponentId component) override;
    void onComponentRemoved(EntityId entity, ComponentId component) override;
    
    // Query functions
    std::vector<SpatialQueryResult> queryRadius(
        const glm::vec3& center, 
        float radius,
        uint32_t layerMask = 0xFFFFFFFF
    );
    
    std::vector<SpatialQueryResult> queryBox(
        const glm::vec3& min,
        const glm::vec3& max,
        uint32_t layerMask = 0xFFFFFFFF
    );
    
    std::vector<SpatialQueryResult> queryFrustum(
        const glm::mat4& viewProjection,
        uint32_t layerMask = 0xFFFFFFFF
    );
    
    // Raycast for pokeball throwing
    std::optional<SpatialQueryResult> raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance = 1000.0f,
        uint32_t layerMask = 0xFFFFFFFF
    );
    
    // Debug visualization
    void debugDraw(class DebugRenderer* renderer);
    
private:
    struct GridCell {
        int32_t x, y, z;
        
        bool operator==(const GridCell& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    
    struct GridCellHash {
        std::size_t operator()(const GridCell& cell) const {
            // Morton encoding for spatial locality
            std::size_t h1 = std::hash<int32_t>{}(cell.x);
            std::size_t h2 = std::hash<int32_t>{}(cell.y);
            std::size_t h3 = std::hash<int32_t>{}(cell.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
    
    float m_cellSize;
    std::unordered_map<GridCell, std::vector<EntityId>, GridCellHash> m_grid;
    std::unordered_map<EntityId, GridCell> m_entityCells;
    
    // Optimization: separate static and dynamic entities
    std::unordered_set<EntityId> m_staticEntities;
    std::unordered_set<EntityId> m_dynamicEntities;
    
    // Helper functions
    GridCell worldToGrid(const glm::vec3& position) const;
    std::vector<GridCell> getOverlappingCells(const glm::vec3& center, float radius) const;
    void updateEntityCell(EntityId entity, const glm::vec3& oldPos, const glm::vec3& newPos);
};

} // namespace VulkanMon
```

### Implementation Details

```cpp
// src/spatial/SpatialSystem.cpp
#include "spatial/SpatialSystem.h"
#include "ecs/Transform.h"
#include <algorithm>
#include <execution>

namespace VulkanMon {

SpatialSystem::SpatialSystem(float cellSize) 
    : m_cellSize(cellSize) {
    // Reserve space for expected entity count
    m_entityCells.reserve(10000);
}

void SpatialSystem::update(float deltaTime) {
    // Update spatial positions from transform components
    auto& ecs = ECS::getInstance();
    
    // Parallel update for dynamic entities
    std::for_each(std::execution::par_unseq,
        m_dynamicEntities.begin(), m_dynamicEntities.end(),
        [this, &ecs](EntityId entity) {
            auto* transform = ecs.getComponent<Transform>(entity);
            auto* spatial = ecs.getComponent<SpatialComponent>(entity);
            
            if (transform && spatial) {
                glm::vec3 oldPos = spatial->position;
                spatial->position = transform->getWorldPosition();
                
                // Only update grid if position changed significantly
                if (glm::distance(oldPos, spatial->position) > 0.1f) {
                    updateEntityCell(entity, oldPos, spatial->position);
                }
            }
        }
    );
}

GridCell SpatialSystem::worldToGrid(const glm::vec3& position) const {
    return {
        static_cast<int32_t>(std::floor(position.x / m_cellSize)),
        static_cast<int32_t>(std::floor(position.y / m_cellSize)),
        static_cast<int32_t>(std::floor(position.z / m_cellSize))
    };
}

std::vector<GridCell> SpatialSystem::getOverlappingCells(
    const glm::vec3& center, float radius) const {
    
    std::vector<GridCell> cells;
    
    // Calculate grid bounds
    glm::vec3 min = center - glm::vec3(radius);
    glm::vec3 max = center + glm::vec3(radius);
    
    GridCell minCell = worldToGrid(min);
    GridCell maxCell = worldToGrid(max);
    
    // Collect all overlapping cells
    for (int32_t x = minCell.x; x <= maxCell.x; ++x) {
        for (int32_t y = minCell.y; y <= maxCell.y; ++y) {
            for (int32_t z = minCell.z; z <= maxCell.z; ++z) {
                cells.push_back({x, y, z});
            }
        }
    }
    
    return cells;
}

std::vector<SpatialQueryResult> SpatialSystem::queryRadius(
    const glm::vec3& center, float radius, uint32_t layerMask) {
    
    std::vector<SpatialQueryResult> results;
    auto cells = getOverlappingCells(center, radius);
    
    // Use a set to avoid duplicate entities
    std::unordered_set<EntityId> checked;
    
    for (const auto& cell : cells) {
        auto it = m_grid.find(cell);
        if (it != m_grid.end()) {
            for (EntityId entity : it->second) {
                // Skip if already checked
                if (!checked.insert(entity).second) continue;
                
                auto* spatial = ECS::getInstance().getComponent<SpatialComponent>(entity);
                if (!spatial || !(spatial->spatialLayers & layerMask)) continue;
                
                // Sphere-sphere intersection test
                float distance = glm::distance(center, spatial->position);
                if (distance <= radius + spatial->radius) {
                    results.push_back({entity, distance, spatial->position});
                }
            }
        }
    }
    
    // Sort by distance for convenience
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.distance < b.distance; });
    
    return results;
}

std::optional<SpatialQueryResult> SpatialSystem::raycast(
    const glm::vec3& origin, const glm::vec3& direction, 
    float maxDistance, uint32_t layerMask) {
    
    // DDA algorithm for grid traversal
    glm::vec3 normalizedDir = glm::normalize(direction);
    glm::vec3 currentPos = origin;
    
    float step = m_cellSize * 0.5f;  // Step at half cell size for accuracy
    float distanceTraveled = 0.0f;
    
    std::optional<SpatialQueryResult> closestHit;
    float closestDistance = maxDistance;
    
    while (distanceTraveled < maxDistance) {
        GridCell cell = worldToGrid(currentPos);
        
        auto it = m_grid.find(cell);
        if (it != m_grid.end()) {
            for (EntityId entity : it->second) {
                auto* spatial = ECS::getInstance().getComponent<SpatialComponent>(entity);
                if (!spatial || !(spatial->spatialLayers & layerMask)) continue;
                
                // Ray-sphere intersection
                glm::vec3 toSphere = spatial->position - origin;
                float projection = glm::dot(toSphere, normalizedDir);
                
                if (projection < 0) continue;  // Behind ray origin
                
                glm::vec3 closestPoint = origin + normalizedDir * projection;
                float distanceToCenter = glm::distance(closestPoint, spatial->position);
                
                if (distanceToCenter <= spatial->radius && projection < closestDistance) {
                    closestDistance = projection;
                    closestHit = {entity, projection, spatial->position};
                }
            }
        }
        
        currentPos += normalizedDir * step;
        distanceTraveled += step;
    }
    
    return closestHit;
}

void SpatialSystem::updateEntityCell(EntityId entity, 
    const glm::vec3& oldPos, const glm::vec3& newPos) {
    
    GridCell oldCell = worldToGrid(oldPos);
    GridCell newCell = worldToGrid(newPos);
    
    if (oldCell == newCell) return;  // No cell change
    
    // Remove from old cell
    auto oldIt = m_grid.find(oldCell);
    if (oldIt != m_grid.end()) {
        auto& entities = oldIt->second;
        entities.erase(
            std::remove(entities.begin(), entities.end(), entity),
            entities.end()
        );
        
        if (entities.empty()) {
            m_grid.erase(oldIt);
        }
    }
    
    // Add to new cell
    m_grid[newCell].push_back(entity);
    m_entityCells[entity] = newCell;
}

} // namespace VulkanMon
```

## Testing Strategy

### Unit Tests

```cpp
// tests_cpp/spatial/test_SpatialSystem.cpp
#include <catch2/catch.hpp>
#include "spatial/SpatialSystem.h"
#include "ecs/ECS.h"

TEST_CASE("SpatialSystem Grid Operations", "[spatial]") {
    VulkanMon::SpatialSystem spatial(10.0f);
    auto& ecs = VulkanMon::ECS::getInstance();
    
    SECTION("Basic radius query") {
        // Create test entities
        auto e1 = ecs.createEntity();
        auto e2 = ecs.createEntity();
        auto e3 = ecs.createEntity();
        
        auto* s1 = ecs.addComponent<VulkanMon::SpatialComponent>(e1);
        s1->position = glm::vec3(0, 0, 0);
        s1->radius = 1.0f;
        
        auto* s2 = ecs.addComponent<VulkanMon::SpatialComponent>(e2);
        s2->position = glm::vec3(5, 0, 0);
        s2->radius = 1.0f;
        
        auto* s3 = ecs.addComponent<VulkanMon::SpatialComponent>(e3);
        s3->position = glm::vec3(20, 0, 0);
        s3->radius = 1.0f;
        
        spatial.update(0.016f);
        
        // Query with radius 10 from origin
        auto results = spatial.queryRadius(glm::vec3(0, 0, 0), 10.0f);
        
        REQUIRE(results.size() == 2);
        CHECK(results[0].entity == e1);
        CHECK(results[1].entity == e2);
    }
    
    SECTION("Layer mask filtering") {
        auto e1 = ecs.createEntity();
        auto* s1 = ecs.addComponent<VulkanMon::SpatialComponent>(e1);
        s1->position = glm::vec3(0, 0, 0);
        s1->spatialLayers = 0x01;  // Layer 1
        
        auto e2 = ecs.createEntity();
        auto* s2 = ecs.addComponent<VulkanMon::SpatialComponent>(e2);
        s2->position = glm::vec3(1, 0, 0);
        s2->spatialLayers = 0x02;  // Layer 2
        
        spatial.update(0.016f);
        
        // Query only layer 1
        auto results = spatial.queryRadius(glm::vec3(0, 0, 0), 10.0f, 0x01);
        REQUIRE(results.size() == 1);
        CHECK(results[0].entity == e1);
    }
    
    SECTION("Raycast accuracy") {
        auto e1 = ecs.createEntity();
        auto* s1 = ecs.addComponent<VulkanMon::SpatialComponent>(e1);
        s1->position = glm::vec3(10, 0, 0);
        s1->radius = 2.0f;
        
        spatial.update(0.016f);
        
        // Cast ray toward entity
        auto hit = spatial.raycast(
            glm::vec3(0, 0, 0),
            glm::vec3(1, 0, 0),
            20.0f
        );
        
        REQUIRE(hit.has_value());
        CHECK(hit->entity == e1);
        CHECK(hit->distance == Approx(8.0f).epsilon(0.1f));
    }
}
```

### Performance Benchmarks

```cpp
// tests_cpp/spatial/bench_SpatialSystem.cpp
#include <benchmark/benchmark.h>
#include "spatial/SpatialSystem.h"

static void BM_SpatialQuery_1000_Entities(benchmark::State& state) {
    VulkanMon::SpatialSystem spatial(10.0f);
    auto& ecs = VulkanMon::ECS::getInstance();
    
    // Create 1000 entities randomly distributed
    std::vector<VulkanMon::EntityId> entities;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-500.0f, 500.0f);
    
    for (int i = 0; i < 1000; ++i) {
        auto e = ecs.createEntity();
        auto* s = ecs.addComponent<VulkanMon::SpatialComponent>(e);
        s->position = glm::vec3(dist(rng), dist(rng), dist(rng));
        s->radius = 1.0f;
        entities.push_back(e);
    }
    
    spatial.update(0.016f);
    
    for (auto _ : state) {
        auto results = spatial.queryRadius(glm::vec3(0, 0, 0), 50.0f);
        benchmark::DoNotOptimize(results);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SpatialQuery_1000_Entities);

static void BM_SpatialUpdate_Dynamic(benchmark::State& state) {
    VulkanMon::SpatialSystem spatial(10.0f);
    auto& ecs = VulkanMon::ECS::getInstance();
    
    // Create dynamic entities
    std::vector<VulkanMon::EntityId> entities;
    for (int i = 0; i < state.range(0); ++i) {
        auto e = ecs.createEntity();
        auto* s = ecs.addComponent<VulkanMon::SpatialComponent>(e);
        s->position = glm::vec3(i * 2.0f, 0, 0);
        s->isStatic = false;
        entities.push_back(e);
    }
    
    for (auto _ : state) {
        // Simulate movement
        for (auto e : entities) {
            auto* s = ecs.getComponent<VulkanMon::SpatialComponent>(e);
            s->position.x += 0.1f;
        }
        spatial.update(0.016f);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_SpatialUpdate_Dynamic)->Range(10, 1000);
```

## Integration with Game Systems

### Creature Detection System

```cpp
// src/game/CreatureDetection.cpp
class CreatureDetectionSystem : public System {
public:
    void update(float deltaTime) override {
        auto& spatial = getSpatialSystem();
        
        // For each creature, check for nearby players
        for (auto& [entity, creature] : m_creatures) {
            // Query for players within detection range
            auto nearbyEntities = spatial.queryRadius(
                creature->position,
                creature->detectionRadius,
                LayerMask::Player
            );
            
            if (!nearbyEntities.empty()) {
                // Trigger alert behavior
                creature->state = CreatureState::Alert;
                creature->targetEntity = nearbyEntities[0].entity;
            }
        }
    }
};
```

### Grass Interaction System

```cpp
// src/game/GrassSystem.cpp
class GrassSystem : public System {
public:
    void update(float deltaTime) override {
        auto& spatial = getSpatialSystem();
        
        // Check for entities moving through grass
        for (auto& [entity, grass] : m_grassPatches) {
            auto nearbyEntities = spatial.queryRadius(
                grass->position,
                grass->rustleRadius,
                LayerMask::Creatures | LayerMask::Player
            );
            
            for (const auto& result : nearbyEntities) {
                // Calculate rustle intensity based on distance
                float intensity = 1.0f - (result.distance / grass->rustleRadius);
                grass->rustleIntensity = glm::max(grass->rustleIntensity, intensity);
                
                // Chance to spawn wild Pokemon
                if (shouldSpawnCreature(intensity)) {
                    spawnWildCreature(grass->position);
                }
            }
            
            // Decay rustle over time
            grass->rustleIntensity *= (1.0f - deltaTime * 2.0f);
        }
    }
};
```

## Optimization Strategies

### 1. Hierarchical Spatial Structure

```cpp
class HierarchicalSpatialSystem {
    // Coarse grid for broad phase
    SpatialGrid m_coarseGrid{50.0f};  
    
    // Fine grid for detailed queries
    SpatialGrid m_fineGrid{5.0f};
    
    // Octree for very dense areas
    std::unique_ptr<Octree> m_octree;
    
    void update(float deltaTime) {
        // Update both grids in parallel
        std::thread t1([this, deltaTime]() { m_coarseGrid.update(deltaTime); });
        std::thread t2([this, deltaTime]() { m_fineGrid.update(deltaTime); });
        t1.join();
        t2.join();
    }
};
```

### 2. Spatial Layers

Define layers for different entity types:

```cpp
namespace LayerMask {
    constexpr uint32_t Player = 0x01;
    constexpr uint32_t Creatures = 0x02;
    constexpr uint32_t Terrain = 0x04;
    constexpr uint32_t Grass = 0x08;
    constexpr uint32_t Water = 0x10;
    constexpr uint32_t Items = 0x20;
    constexpr uint32_t Pokeballs = 0x40;
    constexpr uint32_t Triggers = 0x80;
}
```

### 3. Caching and Prediction

```cpp
class PredictiveSpatialCache {
    struct CacheEntry {
        glm::vec3 queryCenter;
        float queryRadius;
        std::vector<SpatialQueryResult> results;
        float timestamp;
    };
    
    std::unordered_map<size_t, CacheEntry> m_cache;
    
    std::vector<SpatialQueryResult> queryCached(
        const glm::vec3& center, float radius) {
        
        size_t hash = hashQuery(center, radius);
        auto it = m_cache.find(hash);
        
        if (it != m_cache.end() && 
            getCurrentTime() - it->second.timestamp < 0.1f) {
            return it->second.results;  // Use cached result
        }
        
        // Perform actual query and cache
        auto results = performQuery(center, radius);
        m_cache[hash] = {center, radius, results, getCurrentTime()};
        
        // Predictively cache nearby queries
        prefetchNearbyQueries(center, radius);
        
        return results;
    }
};
```

## Debug Visualization

```cpp
void SpatialSystem::debugDraw(DebugRenderer* renderer) {
    // Draw grid cells with entity counts
    for (const auto& [cell, entities] : m_grid) {
        glm::vec3 cellCenter = glm::vec3(
            cell.x * m_cellSize + m_cellSize * 0.5f,
            cell.y * m_cellSize + m_cellSize * 0.5f,
            cell.z * m_cellSize + m_cellSize * 0.5f
        );
        
        // Color based on entity density
        glm::vec4 color = getHeatmapColor(entities.size());
        renderer->drawWireBox(cellCenter, glm::vec3(m_cellSize), color);
        
        // Draw entity count
        renderer->drawText3D(cellCenter, std::to_string(entities.size()));
    }
    
    // Draw entity bounding spheres
    if (m_showBounds) {
        for (const auto& [entity, cell] : m_entityCells) {
            auto* spatial = ECS::getInstance().getComponent<SpatialComponent>(entity);
            if (spatial) {
                renderer->drawWireSphere(spatial->position, spatial->radius, 
                    glm::vec4(0, 1, 0, 0.5f));
            }
        }
    }
}
```

## Performance Targets

- **Grid Update**: < 1ms for 1000 dynamic entities
- **Radius Query**: < 0.5ms for 50-unit radius with 1000 entities
- **Raycast**: < 0.1ms for 100-unit ray
- **Memory Usage**: ~100 bytes per entity + grid overhead
- **Cache Hit Rate**: > 80% for repeated queries

## Week 1 Milestones

1. **Day 1-2**: Implement basic SpatialSystem and SpatialComponent
2. **Day 3**: Add radius query and unit tests
3. **Day 4**: Implement raycast and layer filtering
4. **Day 5**: Add performance benchmarks
5. **Day 6**: Integrate with existing ECS
6. **Day 7**: Debug visualization and profiling

## Week 2 Milestones

1. **Day 8-9**: Optimize with spatial caching
2. **Day 10**: Add hierarchical structure (coarse + fine grid)
3. **Day 11**: Implement frustum culling query
4. **Day 12**: Add predictive caching
5. **Day 13**: Performance tuning based on profiling
6. **Day 14**: Documentation and code review

## Common Pitfalls to Avoid

1. **Grid Size**: Too small = memory overhead, too large = poor culling
2. **Update Frequency**: Only update moving entities
3. **Memory Allocation**: Pre-allocate containers, use object pools
4. **Thread Safety**: Use read-write locks for concurrent access
5. **Floating Point**: Use epsilon comparisons for positions

## Next Steps

After completing the spatial system:
1. Integrate with physics for accurate collision
2. Use for LOD system (switch models based on distance)
3. Implement area-based spawning system
4. Add spatial audio queries
5. Create heat map visualization for optimization