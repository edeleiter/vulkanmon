# VulkanMon Physics System Design Document

## Document Purpose
This document provides detailed design specifications for the VulkanMon Physics/Collision system, following our core development philosophies:
- **Simple is Powerful**: Clean, maintainable architecture
- **Test as we go**: Testable design with clear validation criteria
- **Document often**: Comprehensive design before implementation

## Executive Summary

The Physics System enables realistic Pokemon Legends: Arceus gameplay through:
- **Ground collision** for creatures walking on terrain
- **Projectile physics** for Pokeball throwing mechanics
- **Creature interactions** through collision detection
- **Environmental collision** with trees, rocks, and obstacles
- **Performance optimization** for hundreds of Pokemon entities

## Design Principles

### 1. ECS-First Architecture
The physics system integrates seamlessly with VulkanMon's existing ECS architecture:
- Physics components follow existing component patterns
- PhysicsSystem integrates with World and SystemManager
- No disruption to existing Transform, Spatial, or Render systems

### 2. Spatial System Integration
Leverage existing SpatialManager for performance:
- Use octree for broad-phase collision detection
- Maintain consistency between physics and spatial positions
- Batch operations for efficiency

### 3. Jolt Physics Integration
Modern C++ physics engine choice:
- High-performance multi-threading capabilities
- Designed for open-world games
- Modern C++17 API matching our C++20 codebase
- MIT license with no restrictions

## System Architecture

### Component Design

#### RigidBodyComponent
**Purpose**: Core physics properties for entities that move and collide
**File**: `src/components/RigidBodyComponent.h`

```cpp
struct RigidBodyComponent {
    VKMON_COMPONENT(RigidBodyComponent)

    // Physics properties
    bool isDynamic = true;           // Can move vs static
    float mass = 1.0f;               // Mass in kg (affects acceleration)
    float restitution = 0.3f;        // Bounce factor (0=no bounce, 1=perfect bounce)
    float friction = 0.7f;           // Surface friction (0=ice, 1=sandpaper)

    // Current physics state
    glm::vec3 velocity{0.0f};        // Linear velocity in m/s
    glm::vec3 angularVelocity{0.0f}; // Angular velocity in rad/s
    glm::vec3 force{0.0f};           // Accumulated forces for this frame
    glm::vec3 torque{0.0f};          // Accumulated torques for this frame

    // Jolt Physics integration
    uint32_t bodyID = 0;             // Jolt physics body identifier
    bool needsSync = false;          // Flag when Transform needs updating

    // Physics behavior flags
    bool useGravity = true;          // Affected by gravity
    bool freezeRotation = false;     // Lock rotation (useful for creatures)
    bool isKinematic = false;        // Moved programmatically, not by forces

    // Physics material properties
    float linearDamping = 0.1f;      // Air resistance for movement
    float angularDamping = 0.1f;     // Air resistance for rotation
};
```

**Design Rationale**:
- Mass affects how forces influence the entity (heavier Pokemon move slower)
- Restitution controls bounce (Pokeballs bounce, creatures don't)
- Friction prevents sliding (creatures grip ground, ice is slippery)
- Velocity tracking enables smooth interpolation for rendering
- Damping prevents unrealistic infinite motion

#### CollisionComponent
**Purpose**: Defines collision shape and interaction rules
**File**: `src/components/CollisionComponent.h`

```cpp
struct CollisionComponent {
    VKMON_COMPONENT(CollisionComponent)

    // Collision shape definition
    enum class ShapeType : uint8_t {
        Box,        // Rectangular collision (buildings, platforms)
        Sphere,     // Spherical collision (Pokeballs, simple creatures)
        Capsule,    // Pill-shaped collision (humanoid creatures)
        Mesh,       // Complex mesh collision (detailed environment)
        Heightfield // Terrain collision (ground surfaces)
    };

    ShapeType shapeType = ShapeType::Box;
    glm::vec3 dimensions{1.0f};      // Box: half-extents, Sphere: radius in x, Capsule: radius,height,radius
    std::string meshPath;            // For mesh collision shapes
    float convexRadius = 0.05f;      // Jolt's collision margin for stability

    // Layer-based collision filtering
    uint32_t layer = LayerMask::Default;        // What layer this entity is on
    uint32_t collidesWith = LayerMask::All;     // What layers this entity collides with

    // Collision behavior
    bool isTrigger = false;          // Generates events but doesn't block movement
    bool isStatic = false;           // Never moves (terrain, buildings)

    // Pokemon-specific collision properties
    float captureRadius = 0.0f;      // For Pokeball capture detection
    bool blocksPokeballs = true;     // Can Pokeballs pass through?
    bool blocksCreatures = true;     // Can creatures pass through?

    // Collision callbacks
    std::function<void(EntityID, EntityID)> onCollisionEnter;
    std::function<void(EntityID, EntityID)> onCollisionExit;
    std::function<void(EntityID, EntityID)> onTriggerEnter;
    std::function<void(EntityID, EntityID)> onTriggerExit;
};
```

**Design Rationale**:
- ShapeType covers all Pokemon gameplay needs (creatures, objects, terrain)
- Layer system enables flexible collision filtering (player vs creatures vs environment)
- Trigger zones support Pokemon detection and capture mechanics
- Callback functions integrate collision events with gameplay systems

#### CreaturePhysicsComponent
**Purpose**: Pokemon-specific physics behaviors and constraints
**File**: `src/components/CreaturePhysicsComponent.h`

```cpp
struct CreaturePhysicsComponent {
    VKMON_COMPONENT(CreaturePhysicsComponent)

    // Ground detection and movement
    float groundSnapDistance = 0.5f;  // How far to check for ground
    float groundCheckRadius = 0.3f;   // Radius for ground detection
    bool isGrounded = false;          // Currently touching ground
    glm::vec3 groundNormal{0.0f, 1.0f, 0.0f}; // Surface normal of ground
    float slopeLimit = 45.0f;         // Maximum walkable slope in degrees

    // Creature movement parameters
    float moveSpeed = 5.0f;           // Base movement speed in m/s
    float sprintMultiplier = 2.0f;    // Speed multiplier when sprinting
    float jumpForce = 8.0f;           // Upward force for jumping
    float airControl = 0.3f;          // How much control while airborne

    // Pokemon-specific movement types
    bool canFly = false;              // Can move in 3D space freely
    bool canSwim = false;             // Can move through water
    bool canClimb = false;            // Can walk on walls/ceilings

    // Movement state tracking
    glm::vec3 inputDirection{0.0f};   // Desired movement direction
    bool wantsToJump = false;         // Jump input received
    bool isSwimming = false;          // Currently in water
    bool isFlying = false;            // Currently flying

    // Physics constraints for creatures
    float maxWalkSpeed = 10.0f;       // Speed limit for walking
    float maxFallSpeed = 20.0f;       // Terminal velocity
    float stepHeight = 0.3f;          // Maximum step height to auto-climb

    // Balance and stability
    float stabilizingForce = 50.0f;   // Force to keep creatures upright
    bool autoBalance = true;          // Automatically prevent tipping over
};
```

**Design Rationale**:
- Ground detection enables realistic walking on uneven terrain
- Movement types support different Pokemon species (flying, swimming, etc.)
- Speed limits prevent unrealistic physics behavior
- Auto-balance keeps creatures upright and stable
- Input direction separates player intent from physics simulation

### System Design

#### PhysicsSystem Architecture
**Purpose**: Core ECS system managing all physics simulation
**File**: `src/systems/PhysicsSystem.h`

```cpp
class PhysicsSystem : public System<RigidBodyComponent, CollisionComponent> {
private:
    // Jolt Physics integration
    std::unique_ptr<JPH::PhysicsSystem> joltPhysics_;
    std::unique_ptr<JPH::BodyInterface> bodyInterface_;
    std::unique_ptr<JPH::JobSystem> jobSystem_;

    // ECS system integration
    SpatialSystem* spatialSystem_ = nullptr;      // Non-owning
    RenderSystem* renderSystem_ = nullptr;        // Non-owning

    // Physics world configuration
    BoundingBox worldBounds_;
    glm::vec3 gravity_{0.0f, -9.81f, 0.0f};      // Earth gravity
    float timeStep_ = 1.0f / 60.0f;               // Fixed 60Hz physics
    int velocitySteps_ = 8;                       // Jolt collision iterations
    int positionSteps_ = 3;                       // Jolt constraint iterations

    // Performance tracking
    struct PhysicsStats {
        size_t activeBodies = 0;
        size_t sleepingBodies = 0;
        float updateTimeMs = 0.0f;
        size_t collisionCount = 0;
    } frameStats_;

public:
    // System lifecycle
    void initialize(const BoundingBox& worldBounds) override;
    void update(float deltaTime, EntityManager& entityManager) override;
    void render(VulkanRenderer& renderer, EntityManager& entityManager) override;
    void shutdown() override;

    // ECS integration
    void onComponentAdded(EntityID entity, RigidBodyComponent& component);
    void onComponentAdded(EntityID entity, CollisionComponent& component);
    void onComponentRemoved(EntityID entity);

    // System dependencies
    void setSpatialSystem(SpatialSystem* spatialSystem);
    void setRenderSystem(RenderSystem* renderSystem);

    // Physics body management
    void createPhysicsBody(EntityID entity);
    void destroyPhysicsBody(EntityID entity);
    void updatePhysicsBody(EntityID entity);

    // Force and impulse application
    void applyForce(EntityID entity, const glm::vec3& force);
    void applyImpulse(EntityID entity, const glm::vec3& impulse);
    void applyTorque(EntityID entity, const glm::vec3& torque);

    // Collision queries
    std::vector<EntityID> getColliding(EntityID entity);
    bool castRay(const glm::vec3& origin, const glm::vec3& direction,
                 float maxDistance, EntityID& hitEntity, glm::vec3& hitPoint);
    bool testCollision(EntityID entity1, EntityID entity2);

    // Physics configuration
    void setGravity(const glm::vec3& gravity);
    void setTimeStep(float timeStep);

    // Performance and debugging
    const PhysicsStats& getFrameStats() const { return frameStats_; }
    void enableDebugDrawing(bool enabled);
};
```

**Design Rationale**:
- Fixed 60Hz timestep ensures consistent physics behavior
- Component callbacks integrate seamlessly with ECS lifecycle
- System dependencies follow VulkanMon's dependency injection pattern
- Force/impulse API supports gameplay mechanics (jumping, throwing)
- Ray casting enables line-of-sight and projectile collision
- Performance tracking maintains 60+ FPS requirement

### Integration Design

#### Transform Synchronization
**Challenge**: Keep Transform and RigidBody positions synchronized
**Solution**: Bidirectional synchronization with dirty flagging

```cpp
// Physics -> Transform (every frame)
void PhysicsSystem::syncPhysicsToTransform(EntityID entity) {
    auto& rigidBody = getComponent<RigidBodyComponent>(entity);
    auto& transform = getComponent<Transform>(entity);

    if (rigidBody.needsSync) {
        // Get position from Jolt Physics
        JPH::Vec3 joltPos = bodyInterface_->GetPosition(rigidBody.bodyID);
        transform.setPosition(glm::vec3(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ()));

        // Get rotation from Jolt Physics
        JPH::Quat joltRot = bodyInterface_->GetRotation(rigidBody.bodyID);
        transform.setRotation(glm::quat(joltRot.GetW(), joltRot.GetX(), joltRot.GetY(), joltRot.GetZ()));

        rigidBody.needsSync = false;
    }
}

// Transform -> Physics (when Transform changes)
void PhysicsSystem::syncTransformToPhysics(EntityID entity) {
    auto& rigidBody = getComponent<RigidBodyComponent>(entity);
    auto& transform = getComponent<Transform>(entity);

    if (transform.isDirty) {
        // Update Jolt Physics body position
        JPH::Vec3 joltPos(transform.position.x, transform.position.y, transform.position.z);
        bodyInterface_->SetPosition(rigidBody.bodyID, joltPos, JPH::EActivation::Activate);

        // Update Jolt Physics body rotation
        JPH::Quat joltRot(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
        bodyInterface_->SetRotation(rigidBody.bodyID, joltRot, JPH::EActivation::Activate);
    }
}
```

#### Spatial System Integration
**Challenge**: Maintain spatial partitioning accuracy with physics simulation
**Solution**: Update spatial system after physics step

```cpp
void PhysicsSystem::updateSpatialPositions() {
    for (auto& [entity, rigidBody] : getAllComponents<RigidBodyComponent>()) {
        if (rigidBody.needsSync && spatialSystem_ && hasComponent<SpatialComponent>(entity)) {
            auto& transform = getComponent<Transform>(entity);
            spatialSystem_->updateEntity(entity, transform.position);
        }
    }
}
```

### Performance Design

#### Level-of-Detail (LOD) System
**Purpose**: Reduce physics load for distant entities
**Implementation**: Distance-based physics complexity

```cpp
enum class PhysicsLOD : uint8_t {
    Full,      // Complete physics simulation
    Reduced,   // Simplified collision shapes
    Kinematic, // Position updates only
    Sleeping   // No updates
};

PhysicsLOD calculatePhysicsLOD(EntityID entity, const glm::vec3& cameraPos) {
    auto& transform = getComponent<Transform>(entity);
    float distance = glm::distance(transform.position, cameraPos);

    if (distance < 20.0f) return PhysicsLOD::Full;
    if (distance < 50.0f) return PhysicsLOD::Reduced;
    if (distance < 100.0f) return PhysicsLOD::Kinematic;
    return PhysicsLOD::Sleeping;
}
```

#### Batch Processing
**Purpose**: Minimize per-entity overhead
**Implementation**: Group similar operations

```cpp
void PhysicsSystem::updateBatch(float deltaTime) {
    // Collect all entities needing physics updates
    std::vector<EntityID> dynamicEntities;
    std::vector<EntityID> kinematicEntities;

    for (auto& [entity, rigidBody] : getAllComponents<RigidBodyComponent>()) {
        if (rigidBody.isDynamic && !bodyInterface_->IsDeactivated(rigidBody.bodyID)) {
            dynamicEntities.push_back(entity);
        } else if (rigidBody.isKinematic) {
            kinematicEntities.push_back(entity);
        }
    }

    // Process in batches
    updateDynamicBodies(dynamicEntities, deltaTime);
    updateKinematicBodies(kinematicEntities, deltaTime);
}
```

## Pokemon-Specific Features

### Pokeball Physics Design
**Requirements**: Realistic throwing, bouncing, and capture mechanics

```cpp
class PokeballSystem {
public:
    struct PokeballComponent {
        float throwForce = 15.0f;         // Initial throw force
        float captureRadius = 2.0f;       // Radius for creature capture
        EntityID targetCreature = INVALID_ENTITY;
        bool hasCollided = false;         // Hit something
        float lifetime = 10.0f;           // Despawn timer

        // Physics properties
        float mass = 0.2f;               // Light sphere
        float restitution = 0.8f;        // Very bouncy
        float friction = 0.3f;           // Low friction for rolling
    };

    EntityID throwPokeball(const glm::vec3& origin, const glm::vec3& direction, float force);
    void updatePokeballCapture(EntityID pokeball, float deltaTime);
    bool attemptCapture(EntityID pokeball, EntityID creature);
};
```

### Creature Ground Collision
**Requirements**: Smooth movement on uneven terrain

```cpp
void CreaturePhysicsSystem::updateGroundContact(EntityID entity, float deltaTime) {
    auto& creaturePhysics = getComponent<CreaturePhysicsComponent>(entity);
    auto& transform = getComponent<Transform>(entity);
    auto& rigidBody = getComponent<RigidBodyComponent>(entity);

    // Cast ray downward to find ground
    glm::vec3 rayStart = transform.position + glm::vec3(0, creaturePhysics.groundCheckRadius, 0);
    glm::vec3 rayDirection = glm::vec3(0, -1, 0);
    float rayDistance = creaturePhysics.groundSnapDistance + creaturePhysics.groundCheckRadius;

    EntityID hitEntity;
    glm::vec3 hitPoint;
    if (physicsSystem_->castRay(rayStart, rayDirection, rayDistance, hitEntity, hitPoint)) {
        // Calculate ground normal and slope
        creaturePhysics.groundNormal = calculateGroundNormal(hitPoint, hitEntity);
        float slope = glm::degrees(glm::acos(glm::dot(creaturePhysics.groundNormal, glm::vec3(0, 1, 0))));

        // Check if slope is walkable
        if (slope <= creaturePhysics.slopeLimit) {
            creaturePhysics.isGrounded = true;

            // Snap to ground if close enough
            float distanceToGround = transform.position.y - hitPoint.y;
            if (distanceToGround < creaturePhysics.groundSnapDistance && rigidBody.velocity.y <= 0) {
                transform.position.y = hitPoint.y;
                rigidBody.velocity.y = 0; // Stop falling
            }
        } else {
            creaturePhysics.isGrounded = false; // Too steep
        }
    } else {
        creaturePhysics.isGrounded = false; // No ground found
    }
}
```

## Testing Strategy

### Unit Tests Design
**File**: `tests_cpp/physics/test_PhysicsSystem.cpp`

```cpp
TEST_CASE("Physics System Integration", "[Physics][Integration]") {
    SECTION("RigidBody component lifecycle") {
        // Create entity with physics
        EntityID entity = createTestEntity();
        auto& rigidBody = addComponent<RigidBodyComponent>(entity);
        auto& collision = addComponent<CollisionComponent>(entity);

        REQUIRE(physicsSystem.hasPhysicsBody(entity));
        REQUIRE(rigidBody.bodyID != 0);

        // Remove components
        removeComponent<RigidBodyComponent>(entity);
        REQUIRE_FALSE(physicsSystem.hasPhysicsBody(entity));
    }

    SECTION("Gravity and falling") {
        auto entity = createEntityAtPosition(glm::vec3(0, 10, 0));
        auto& rigidBody = getComponent<RigidBodyComponent>(entity);

        // Should fall under gravity
        for (int i = 0; i < 120; ++i) { // 2 seconds at 60Hz
            physicsSystem.update(1.0f / 60.0f, entityManager);
        }

        auto& transform = getComponent<Transform>(entity);
        REQUIRE(transform.position.y < 5.0f); // Should have fallen
        REQUIRE(rigidBody.velocity.y < 0.0f); // Should be falling
    }

    SECTION("Ground collision") {
        // Create ground plane
        auto ground = createGroundEntity(glm::vec3(0, 0, 0), glm::vec3(10, 1, 10));

        // Create falling entity
        auto entity = createEntityAtPosition(glm::vec3(0, 5, 0));

        // Simulate until it hits ground
        for (int i = 0; i < 300; ++i) { // 5 seconds max
            physicsSystem.update(1.0f / 60.0f, entityManager);

            auto& transform = getComponent<Transform>(entity);
            if (transform.position.y <= 1.0f) break; // Hit ground
        }

        auto& transform = getComponent<Transform>(entity);
        REQUIRE(transform.position.y >= 0.9f); // Should be resting on ground
        REQUIRE(transform.position.y <= 1.1f); // Not embedded or floating
    }
}
```

### Performance Tests
**File**: `tests_cpp/physics/test_PhysicsPerformance.cpp`

```cpp
TEST_CASE("Physics Performance", "[Physics][Performance]") {
    SECTION("Hundreds of physics bodies") {
        // Create 500 dynamic physics entities
        std::vector<EntityID> entities;
        for (int i = 0; i < 500; ++i) {
            auto entity = createRandomPhysicsEntity();
            entities.push_back(entity);
        }

        // Measure physics update time
        auto start = std::chrono::high_resolution_clock::now();

        for (int frame = 0; frame < 60; ++frame) {
            physicsSystem.update(1.0f / 60.0f, entityManager);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<float, std::milli>(end - start);

        // Should maintain 60+ FPS (< 16.67ms per frame)
        float avgFrameTime = duration.count() / 60.0f;
        REQUIRE(avgFrameTime < 16.0f);

        // Check physics stats
        auto stats = physicsSystem.getFrameStats();
        REQUIRE(stats.activeBodies <= 500);
        REQUIRE(stats.updateTimeMs < 10.0f); // Physics should be under 10ms
    }
}
```

## Implementation Roadmap

### Phase 1: Foundation (Week 1)
1. Create physics components (RigidBody, Collision, CreaturePhysics)
2. Implement basic PhysicsSystem class structure
3. Basic Jolt Physics integration and initialization
4. Transform synchronization system
5. Unit tests for component lifecycle

### Phase 2: Core Collision (Week 2)
1. Collision shape creation (Box, Sphere, Capsule)
2. Basic collision detection and response
3. Ground collision for creatures
4. Layer-based collision filtering
5. Integration tests for collision

### Phase 3: Pokemon Features (Week 3)
1. Pokeball throwing mechanics
2. Creature ground detection and movement
3. Environmental collision (trees, rocks)
4. Spatial system integration
5. Performance optimization (LOD, batching)

### Phase 4: Polish and Testing (Week 4)
1. Physics debug visualization
2. Comprehensive test suite
3. Performance profiling and optimization
4. Documentation updates
5. Integration with existing creature systems

## Success Criteria

### Functional Requirements
- ✅ Creatures walk on terrain without falling through
- ✅ Pokeballs follow realistic trajectories and bounce
- ✅ Collision detection works between all entity types
- ✅ Physics simulation maintains 60+ FPS with 200+ entities
- ✅ Integration preserves existing ECS functionality

### Performance Requirements
- ✅ Physics update time < 10ms per frame
- ✅ Memory usage < 100MB for 500 physics entities
- ✅ No frame drops during heavy physics scenes
- ✅ Spatial system performance maintained

### Quality Requirements
- ✅ 100% test coverage for physics components
- ✅ Zero physics simulation instabilities
- ✅ Clean integration with existing systems
- ✅ Comprehensive error handling and recovery

This design document serves as our implementation blueprint, ensuring we maintain VulkanMon's quality standards while building the foundation for Pokemon Legends: Arceus gameplay.