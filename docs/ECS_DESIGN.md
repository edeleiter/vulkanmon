# Entity Component System (ECS) Design

## Design Philosophy: Simple is Powerful

Our ECS follows three core principles:
1. **Entities** are unique IDs (simple integers)
2. **Components** are pure data structures
3. **Systems** contain all logic and operate on components

## Core Architecture

### Entity (32-bit ID)
```cpp
using EntityID = uint32_t;
static constexpr EntityID INVALID_ENTITY = 0;
```

### Component Interface
```cpp
struct ComponentBase {
    static inline uint32_t nextTypeId = 1;
    template<typename T>
    static uint32_t getTypeId() {
        static uint32_t typeId = nextTypeId++;
        return typeId;
    }
};
```

### System Base Class
```cpp
class SystemBase {
public:
    virtual ~SystemBase() = default;
    virtual void update(float deltaTime) = 0;
    virtual void render(VulkanRenderer& renderer) {}
};
```

## Component Types for Pokemon Game

### Core Components
- **Transform**: Position, rotation, scale
- **Renderable**: Mesh, material, visibility
- **Collider**: Collision bounds, physics properties
- **Animation**: Current animation state, blend weights
- **Audio**: Sound sources, 3D positioning

### Pokemon-Specific Components
- **Creature**: Species, stats, moves, AI state
- **Player**: Input handling, inventory, progression
- **Interactable**: Can be picked up, talked to, etc.
- **Mount**: Rideable creatures with special movement

## Memory Layout Strategy

### Sparse Set Approach
- Fast iteration over components
- Efficient entity-component mapping
- Cache-friendly memory access patterns
- O(1) component add/remove/access

### Component Storage
```cpp
template<typename T>
class ComponentArray {
private:
    std::vector<T> components;           // Dense array of components
    std::vector<EntityID> entityIds;     // Entity ID for each component
    std::unordered_map<EntityID, size_t> entityToIndex; // Entity -> index mapping
};
```

## System Design Patterns

### Update Systems (per frame)
- **MovementSystem**: Updates Transform based on velocity/input
- **AnimationSystem**: Updates animation state and bone transforms
- **PhysicsSystem**: Collision detection and response
- **AISystem**: Creature behavior and decision making

### Render Systems (per frame)
- **RenderSystem**: Submits renderable objects to renderer
- **UISystem**: Renders game interface elements
- **DebugRenderSystem**: Visualization for debugging

### Event Systems (as needed)
- **InputSystem**: Processes user input into component changes
- **AudioSystem**: Manages 3D audio positioning
- **ScriptSystem**: Executes gameplay scripts and triggers

## Integration with Existing Systems

### Phase 1: Migrate Current Objects
1. **Camera** becomes Entity with Transform + Camera components
2. **Lighting** becomes Entities with Transform + Light components
3. **Rendered objects** become Entities with Transform + Renderable components

### Phase 2: Add Game Objects
1. **Player character** with Transform + Player + Renderable + Collider
2. **Environment objects** with Transform + Renderable + optional Collider
3. **Creatures** with full component suite for Pokemon gameplay

## Performance Considerations

### Memory Efficiency
- Components stored in contiguous arrays
- Systems iterate only over relevant components
- No virtual function calls in tight loops

### Threading Preparation
- Systems designed for parallel execution
- Component dependencies clearly defined
- Read-only vs write access patterns documented

## Testing Strategy

### Unit Tests
- Component add/remove/access operations
- System update logic in isolation
- Entity lifecycle management

### Integration Tests
- Multiple systems working together
- Component dependencies resolved correctly
- Performance benchmarks for large entity counts

## Implementation Plan

1. **Core ECS Framework** (Entity manager, component arrays, system base)
2. **Basic Components** (Transform, Renderable)
3. **Basic Systems** (Movement, Render)
4. **Migration** (Camera, lighting to ECS)
5. **Testing** (Unit tests + visual validation)
6. **Documentation** (Usage examples + performance notes)

This design enables **scalable Pokemon gameplay** while maintaining **engine simplicity**.