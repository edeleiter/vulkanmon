#pragma once

#include "../core/Entity.h"
#include "../core/System.h"
#include "../components/RigidBodyComponent.h"
#include "../components/CollisionComponent.h"
#include "../components/CreaturePhysicsComponent.h"
#include "../components/Transform.h"
#include <glm/glm.hpp>
#include <memory>

namespace VulkanMon {

// Forward declarations
class EntityManager;
class SpatialSystem;
class VulkanRenderer;

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
    void update(float deltaTime, EntityManager& entityManager) override;
    void initialize(EntityManager& entityManager) override;
    void shutdown(EntityManager& entityManager) override;

    // Physics-specific initialization with gravity parameter
    void initialize(const glm::vec3& gravity = glm::vec3(0.0f, -9.81f, 0.0f));

    // Clean shutdown of physics system
    void shutdown();

    // Set reference to spatial system for collision detection optimization
    void setSpatialSystem(SpatialSystem* spatial) { spatialSystem_ = spatial; }

    // =============================================================================
    // CORE SIMULATION LOOP
    // =============================================================================

    // Update all physics entities (called every frame)
    void update(EntityManager& entityManager, float deltaTime);

    // Fixed timestep physics update for stability (called at fixed intervals)
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

private:
    // =============================================================================
    // INTERNAL PHYSICS SIMULATION
    // =============================================================================

    // Core physics steps
    void integrateRigidBodies(EntityManager& entityManager, float deltaTime);
    void detectCollisions(EntityManager& entityManager);
    void resolveCollisions(EntityManager& entityManager, float deltaTime);
    void updateTransforms(EntityManager& entityManager, float deltaTime);
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

    // System state
    bool initialized_{false};                      // System initialization status
    bool collisionEnabled_{true};                  // Global collision detection toggle
    bool debugDraw_{false};                        // Debug visualization toggle

    // System references
    SpatialSystem* spatialSystem_{nullptr};        // Spatial partitioning system reference

    // Performance tracking
    PhysicsStats stats_;                           // Current frame statistics

    // Collision filtering matrix (layer pairs that can collide)
    uint32_t collisionMatrix_[32];                 // Bit matrix for layer collision rules

    // Frame timing
    float accumulator_{0.0f};                      // Fixed timestep accumulator
    static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f; // 60 Hz physics
};

} // namespace VulkanMon