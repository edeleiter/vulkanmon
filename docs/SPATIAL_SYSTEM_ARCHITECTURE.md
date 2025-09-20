# Spatial System Architecture

## Overview

VulkanMon's spatial system is designed for Pokemon Legends: Arceus-style open world gameplay, supporting hundreds of creatures with efficient spatial queries for detection, interaction, and rendering.

## Core Components

### SpatialManager
- **Primary Interface**: Main entry point for all spatial operations
- **Entity Storage**: Manages entity positions and layer assignments
- **Cache Management**: Handles query caching for performance optimization
- **World Bounds**: Operates within configurable 3D world boundaries

### OctreeNode
- **Hierarchical Partitioning**: Recursive spatial subdivision for O(log n) queries
- **Dynamic Subdivision**: Automatically subdivides when MAX_ENTITIES_PER_NODE (8) is exceeded
- **Pokemon Optimized**: Tuned for creature detection radii (5-15 units)
- **Cache-Friendly**: Leaf nodes store entity lists for efficient iteration

### SpatialComponent (ECS Integration)
- **Per-Entity Data**: Detection radius, territory bounds, home position
- **Query Throttling**: Prevents excessive spatial queries (configurable intervals)
- **Layer System**: Supports filtering by entity types (Creatures, NPCs, Items, etc.)

## Performance Characteristics

### Query Performance
- **Radius Queries**: O(log n) average, O(n) worst case
- **Frustum Queries**: Optimized for camera-based rendering culling
- **Batch Queries**: Supports processing 64+ creatures per frame
- **Cache Hit Rate**: ~80% for typical Pokemon movement patterns

### Memory Usage
- **Per Entity**: ~24 bytes baseline + component overhead
- **Octree Nodes**: Lazy allocation, minimal memory when sparse
- **Query Cache**: LRU eviction, configurable size limits

## Pokemon-Specific Features

### Creature Detection
```cpp
// Find creatures within interaction radius
auto nearbyCreatures = spatialManager.findCreaturesInRadius(playerPos, 10.0f);

// Find visible creatures for rendering
auto visibleCreatures = spatialManager.findVisibleCreatures(cameraFrustum);
```

### Territory Management
- **Home Position Tracking**: Creatures remember spawn/territory center
- **Boundary Detection**: Efficient queries for territorial behavior
- **Migration Support**: Handles seasonal movement patterns

### Battle Triggers
- **Proximity Detection**: Player-creature interaction zones
- **Line of Sight**: Integration with visibility/frustum queries
- **Aggression Radius**: Configurable per creature type

## Integration Points

### ECS Systems
- **SpatialSystem**: Updates entity positions in spatial manager
- **CreatureDetectionSystem**: Uses spatial queries for AI behavior
- **RenderSystem**: Frustum culling via spatial queries

### Performance Monitoring
- **Query Statistics**: Average query time, entities returned
- **Subdivision Metrics**: Octree depth, node count
- **Cache Performance**: Hit/miss ratios, invalidation frequency

## Configuration

### World Bounds
- **Default**: (-50, -50, -50) to (50, 50, 50) units
- **Pokemon Scale**: Supports large open world areas
- **Configurable**: Runtime adjustment via WorldConfig

### Performance Tuning
- **MAX_ENTITIES_PER_NODE**: 8 (optimal for creature interactions)
- **MAX_DEPTH**: 10 levels (prevents excessive subdivision)
- **Query Cache Size**: Adaptive based on entity count

## Future Enhancements

### Planned Features
- **Multi-Threaded Queries**: Parallel spatial operations
- **Dynamic LOD**: Distance-based detail reduction
- **Persistence**: Save/load spatial state for large worlds
- **Networking**: Distributed spatial management for multiplayer

### Pokemon-Specific Roadmap
- **Weather Integration**: Spatial queries by weather zones
- **Time of Day**: Temporal creature distribution patterns
- **Biome Awareness**: Creature spawning by terrain type