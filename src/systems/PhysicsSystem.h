#pragma once

#include "../core/Entity.h"
#include "../core/System.h"
#include "../components/RigidBodyComponent.h"
#include "../components/CollisionComponent.h"
#include "../components/CreaturePhysicsComponent.h"
#include "../components/Transform.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <thread>

// Jolt Physics includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>

// Forward declare Jolt types to avoid naming conflicts

namespace VulkanMon {

// Forward declarations
class EntityManager;
class SpatialSystem;
class VulkanRenderer;

// =============================================================================
// JOLT PHYSICS LAYER DEFINITIONS
// =============================================================================

// Object layers - define collision groups for different entity types
namespace ObjectLayers {
    static constexpr JPH::ObjectLayer CREATURES = 0;
    static constexpr JPH::ObjectLayer ENVIRONMENT = 1;
    static constexpr JPH::ObjectLayer PROJECTILES = 2;
    static constexpr JPH::ObjectLayer PLAYER = 3;
    static constexpr JPH::ObjectLayer TRIGGERS = 4;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 5;
}

// Broad phase layers - higher level collision filtering
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer MOVING = JPH::BroadPhaseLayer(0);
    static constexpr JPH::BroadPhaseLayer STATIC = JPH::BroadPhaseLayer(1);
    static const JPH::uint NUM_LAYERS = 2;
}

// =============================================================================
// JOLT PHYSICS INTERFACE IMPLEMENTATIONS
// =============================================================================

// BroadPhaseLayerInterface implementation
class BroadPhaseLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        switch (inLayer) {
            case ObjectLayers::CREATURES:
            case ObjectLayers::PROJECTILES:
            case ObjectLayers::PLAYER:
                return BroadPhaseLayers::MOVING;
            case ObjectLayers::ENVIRONMENT:
            case ObjectLayers::TRIGGERS:
            default:
                return BroadPhaseLayers::STATIC;
        }
    }
};

// Object vs broad phase layer filter
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        // General game engine broad phase filtering for performance optimization
        switch (inLayer1) {
            case ObjectLayers::CREATURES:
            case ObjectLayers::PLAYER:
            case ObjectLayers::PROJECTILES:
                // Dynamic objects need to check collisions with both static and dynamic layers
                return true;

            case ObjectLayers::ENVIRONMENT:
                // Static environment only needs to check collisions with moving objects
                return inLayer2 == BroadPhaseLayers::MOVING;

            case ObjectLayers::TRIGGERS:
                // Trigger zones only detect moving objects passing through them
                return inLayer2 == BroadPhaseLayers::MOVING;

            default:
                return true;
        }
    }
};

// Object layer pair filter
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        // General game engine collision layer rules
        // Make collision checks symmetric by ordering layers
        JPH::ObjectLayer layer1 = std::min(inObject1, inObject2);
        JPH::ObjectLayer layer2 = std::max(inObject1, inObject2);

        // Dynamic entities (creatures, NPCs, AI characters) collision rules
        if (layer1 == ObjectLayers::CREATURES && layer2 == ObjectLayers::CREATURES) {
            // Dynamic entities can collide with each other (AI interactions, character blocking)
            return true;
        }

        if (layer1 == ObjectLayers::CREATURES && layer2 == ObjectLayers::ENVIRONMENT) {
            // Dynamic entities collide with static world geometry (terrain, buildings, obstacles)
            return true;
        }

        if (layer1 == ObjectLayers::CREATURES && layer2 == ObjectLayers::PROJECTILES) {
            // Projectiles can hit dynamic entities (bullets, thrown objects, etc.)
            return true;
        }

        if (layer1 == ObjectLayers::CREATURES && layer2 == ObjectLayers::PLAYER) {
            // Player can physically interact with dynamic entities
            return true;
        }

        if (layer1 == ObjectLayers::CREATURES && layer2 == ObjectLayers::TRIGGERS) {
            // Dynamic entities can trigger zone detection (switches, pressure plates, etc.)
            return true;
        }

        // Static environment collision rules
        if (layer1 == ObjectLayers::ENVIRONMENT && layer2 == ObjectLayers::ENVIRONMENT) {
            // Static environment pieces don't collide with each other (optimization)
            return false;
        }

        if (layer1 == ObjectLayers::ENVIRONMENT && layer2 == ObjectLayers::PROJECTILES) {
            // Projectiles collide with world geometry (bullets hitting walls, bouncing objects)
            return true;
        }

        if (layer1 == ObjectLayers::ENVIRONMENT && layer2 == ObjectLayers::PLAYER) {
            // Player collides with static world for movement constraints
            return true;
        }

        if (layer1 == ObjectLayers::ENVIRONMENT && layer2 == ObjectLayers::TRIGGERS) {
            // Static environment doesn't interact with trigger zones (optimization)
            return false;
        }

        // Projectile collision rules
        if (layer1 == ObjectLayers::PROJECTILES && layer2 == ObjectLayers::PROJECTILES) {
            // Projectiles can collide with each other (bullet deflection, object interactions)
            return true;
        }

        if (layer1 == ObjectLayers::PROJECTILES && layer2 == ObjectLayers::PLAYER) {
            // Player can interact with projectiles (catching, deflecting, taking damage)
            return true;
        }

        if (layer1 == ObjectLayers::PROJECTILES && layer2 == ObjectLayers::TRIGGERS) {
            // Projectiles can activate trigger zones (shooting switches, target detection)
            return true;
        }

        // Player collision rules
        if (layer1 == ObjectLayers::PLAYER && layer2 == ObjectLayers::PLAYER) {
            // Multiple players don't physically collide (prevents player blocking in multiplayer)
            return false;
        }

        if (layer1 == ObjectLayers::PLAYER && layer2 == ObjectLayers::TRIGGERS) {
            // Player activates trigger zones (doors, switches, area detection)
            return true;
        }

        // Trigger zone rules
        if (layer1 == ObjectLayers::TRIGGERS && layer2 == ObjectLayers::TRIGGERS) {
            // Trigger zones don't interact with each other (optimization)
            return false;
        }

        // Default to allowing collision for extensibility
        return true;
    }
};

/**
 * PhysicsSystem
 *
 * Manages physics simulation for all entities with physics components.
 * Integrates with ECS architecture and spatial partitioning for performance.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clear physics simulation loop with minimal complexity
 * - ECS Integration: Works seamlessly with existing Transform and Spatial components
 * - Performance-aware: Spatial partitioning integration for collision culling
 */
class PhysicsSystem : public SystemBase {
public:
    // =============================================================================
    // CONSTRUCTOR AND INITIALIZATION
    // =============================================================================

    PhysicsSystem() = default;
    ~PhysicsSystem() = default;

    // SystemBase interface implementation
    // @param deltaTime Frame time in MILLISECONDS (from Application::frameTime_)
    void update(float deltaTime, EntityManager& entityManager) override;
    void initialize(EntityManager& entityManager) override;
    void shutdown(EntityManager& entityManager) override;


    // Set reference to spatial system for collision detection optimization
    void setSpatialSystem(SpatialSystem* spatial) { spatialSystem_ = spatial; }

    // =============================================================================
    // CORE SIMULATION LOOP
    // =============================================================================


    // Fixed timestep physics update for stability (called at fixed intervals)
    // @param fixedDeltaTime Fixed timestep in MILLISECONDS (converted internally to seconds for Jolt)
    void fixedUpdate(EntityManager& entityManager, float fixedDeltaTime);

    // =============================================================================
    // PHYSICS CONFIGURATION
    // =============================================================================

    // World physics parameters
    void setGravity(const glm::vec3& gravity) { worldGravity_ = gravity; }
    glm::vec3 getGravity() const { return worldGravity_; }

    // Global physics scaling
    void setTimeScale(float scale) { timeScale_ = scale; }
    float getTimeScale() const { return timeScale_; }

    // Physics solver iterations for accuracy vs performance
    void setIterations(int velocityIterations, int positionIterations) {
        velocityIterations_ = velocityIterations;
        positionIterations_ = positionIterations;
    }

    // Thread configuration for Jolt Physics multithreading
    void setThreadCount(uint32_t threadCount) {
        threadCount_ = threadCount;
        if (threadCount_ == 0) {
            threadCount_ = std::max(1u, std::thread::hardware_concurrency() - 1);
        }
    }
    uint32_t getThreadCount() const { return threadCount_; }

    // =============================================================================
    // COLLISION DETECTION AND RESPONSE
    // =============================================================================

    // Enable/disable collision detection
    void enableCollisionDetection(bool enabled) { collisionEnabled_ = enabled; }
    bool isCollisionEnabled() const { return collisionEnabled_; }

    // Collision layer management
    void setCollisionMatrix(uint32_t layer1, uint32_t layer2, bool canCollide);
    bool shouldLayersCollide(uint32_t layer1, uint32_t layer2) const;

    // =============================================================================
    // PHYSICS QUERIES
    // =============================================================================

    // Ray casting for projectiles, line-of-sight checks
    struct RaycastHit {
        EntityID entity = 0;
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        float distance = 0.0f;
        bool hit = false;
    };

    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& direction,
                      float maxDistance = 100.0f, uint32_t layerMask = 0xFFFFFFFF);

    // Sphere overlap for area detection
    std::vector<EntityID> overlapSphere(const glm::vec3& center, float radius,
                                       uint32_t layerMask = 0xFFFFFFFF);

    // =============================================================================
    // PERFORMANCE AND DEBUGGING
    // =============================================================================

    // Get physics performance statistics
    struct PhysicsStats {
        float updateTime = 0.0f;        // Last update time in milliseconds
        int activeRigidBodies = 0;      // Number of dynamic physics bodies
        int collisionChecks = 0;        // Collision pairs checked this frame
        int collisionHits = 0;          // Actual collisions detected
        float averageVelocity = 0.0f;   // Average entity velocity for performance tuning
    };

    PhysicsStats getStats() const { return stats_; }

    // Enable debug visualization (requires debug renderer)
    void setDebugDraw(bool enabled) { debugDraw_ = enabled; }
    bool isDebugDrawEnabled() const { return debugDraw_; }

    // =============================================================================
    // JOLT PHYSICS INTEGRATION METHODS
    // =============================================================================

private:
    // =============================================================================
    // COLLISION DATA STRUCTURES
    // =============================================================================

    // Collision pair for tracking detected collisions
    struct CollisionPair {
        EntityID entityA = 0;
        EntityID entityB = 0;
        glm::vec3 normal{0.0f, 1.0f, 0.0f};  // Collision normal (A to B)
        float penetration = 0.0f;             // Penetration depth
    };

    // =============================================================================
    // JOLT PHYSICS INTERNAL METHODS
    // =============================================================================

    // Jolt system lifecycle
    void initializeJoltPhysics();
    void shutdownJoltPhysics();

    // Jolt layer configuration
    void setupJoltLayers();

    // Jolt body management
    JPH::BodyID createJoltBody(EntityID entity, const RigidBodyComponent& rigidBody,
                               const CollisionComponent& collision, const Transform& transform);
    void destroyJoltBody(EntityID entity);
    void updateJoltBodyFromComponent(EntityID entity, const RigidBodyComponent& rigidBody);

    // Jolt shape creation
    JPH::ShapeRefC createJoltShape(const CollisionComponent& collision);

    // Transform synchronization
    void syncTransformsFromJolt(EntityManager& entityManager);
    void syncTransformsToJolt(EntityManager& entityManager);

    // Jolt statistics update
    void updateStatsFromJolt();

    // Automatic Jolt body management
    void createJoltBodiesForNewEntities(EntityManager& entityManager);
    void createJoltBodiesForAllEntities(EntityManager& entityManager);

    // Body state management
    void activateJoltBody(EntityID entity);
    void deactivateJoltBody(EntityID entity);
    bool isJoltBodyActive(EntityID entity) const;

    // Jolt utility methods
    JPH::ObjectLayer mapLayerMaskToObjectLayer(uint32_t layerMask) const;
    JPH::BroadPhaseLayer mapObjectLayerToBroadPhaseLayer(JPH::ObjectLayer objectLayer) const;

    // =============================================================================
    // INTERNAL PHYSICS SIMULATION (LEGACY - TO BE REPLACED)
    // =============================================================================

    // Core physics steps
    void detectCollisions(EntityManager& entityManager);
    void resolveCollisions(EntityManager& entityManager, float deltaTime);
    void updateCreaturePhysics(EntityManager& entityManager, float deltaTime);

    // Collision detection helpers
    bool checkCollision(const CollisionComponent& collider1, const Transform& transform1,
                       const CollisionComponent& collider2, const Transform& transform2);

    void resolveCollision(EntityID entity1, EntityID entity2,
                         RigidBodyComponent& body1, RigidBodyComponent& body2,
                         const glm::vec3& collisionNormal, float penetration);

    // Ground detection for creatures
    void updateGroundDetection(EntityManager& entityManager);

    // Apply environmental forces (gravity, drag, etc.)
    void applyEnvironmentalForces(EntityManager& entityManager, float deltaTime);

    // =============================================================================
    // MEMBER VARIABLES
    // =============================================================================

    // World configuration
    glm::vec3 worldGravity_{0.0f, -9.81f, 0.0f};  // World gravity vector
    float timeScale_{1.0f};                        // Global time scaling factor

    // Solver configuration
    int velocityIterations_{8};                    // Velocity constraint solver iterations
    int positionIterations_{3};                    // Position constraint solver iterations
    uint32_t threadCount_{0};                      // Physics thread count (0 = auto-detect)

    // System state
    bool initialized_{false};                      // System initialization status
    bool collisionEnabled_{true};                  // Global collision detection toggle
    bool debugDraw_{false};                        // Debug visualization toggle

    // System references
    SpatialSystem* spatialSystem_{nullptr};        // Spatial partitioning system reference

    // =============================================================================
    // JOLT PHYSICS SYSTEM
    // =============================================================================

    // Core Jolt Physics objects
    std::unique_ptr<JPH::PhysicsSystem> joltPhysics_;
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator_;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem_;

    // Jolt interface implementations
    std::unique_ptr<BroadPhaseLayerInterfaceImpl> broadPhaseLayerInterface_;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> objectVsBroadPhaseLayerFilter_;
    std::unique_ptr<ObjectLayerPairFilterImpl> objectLayerPairFilter_;

    // Entity mapping for Jolt integration
    std::unordered_map<EntityID, JPH::BodyID> entityToBodyMap_;  // Entity -> Jolt Body mapping
    std::unordered_map<JPH::BodyID, EntityID> bodyToEntityMap_;  // Jolt Body -> Entity mapping

    // Performance tracking
    PhysicsStats stats_;                           // Current frame statistics

    // Collision filtering matrix (layer pairs that can collide)
    uint32_t collisionMatrix_[32];                 // Bit matrix for layer collision rules

    // Collision detection state
    std::vector<CollisionPair> currentCollisions_; // Current frame collision pairs

    // Frame timing
    float accumulator_{0.0f};                      // Fixed timestep accumulator
    static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f; // 60 Hz physics
};

} // namespace VulkanMon