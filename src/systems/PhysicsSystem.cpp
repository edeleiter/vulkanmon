#define GLM_ENABLE_EXPERIMENTAL
#include "PhysicsSystem.h"
#include "../core/EntityManager.h"
#include "../systems/SpatialSystem.h"
#include "../utils/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <chrono>

namespace VulkanMon {

// SystemBase interface implementation
void PhysicsSystem::update(float deltaTime, EntityManager& entityManager) {
    // Delegate to our existing update method
    update(entityManager, deltaTime);
}

void PhysicsSystem::initialize(EntityManager& entityManager) {
    // Initialize with default gravity
    initialize();
}

void PhysicsSystem::shutdown(EntityManager& entityManager) {
    // Delegate to our existing shutdown method
    shutdown();
}

// Physics-specific methods
void PhysicsSystem::initialize(const glm::vec3& gravity) {
    Logger::getInstance().info("Initializing PhysicsSystem with gravity: " +
                             std::to_string(gravity.x) + ", " +
                             std::to_string(gravity.y) + ", " +
                             std::to_string(gravity.z));

    worldGravity_ = gravity;
    timeScale_ = 1.0f;
    collisionEnabled_ = true;
    debugDraw_ = false;
    accumulator_ = 0.0f;

    // Initialize collision matrix - all layers can collide by default
    for (int i = 0; i < 32; ++i) {
        collisionMatrix_[i] = 0xFFFFFFFF;
    }

    // Reset statistics
    stats_ = PhysicsStats{};

    initialized_ = true;
    Logger::getInstance().info("PhysicsSystem initialized successfully");
}

void PhysicsSystem::shutdown() {
    if (!initialized_) return;

    Logger::getInstance().info("Shutting down PhysicsSystem");

    spatialSystem_ = nullptr;
    initialized_ = false;

    Logger::getInstance().info("PhysicsSystem shutdown complete");
}

void PhysicsSystem::update(EntityManager& entityManager, float deltaTime) {
    if (!initialized_) return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Apply time scaling
    float scaledDeltaTime = deltaTime * timeScale_;

    // Reset frame statistics
    stats_.activeRigidBodies = 0;
    stats_.collisionChecks = 0;
    stats_.collisionHits = 0;
    stats_.averageVelocity = 0.0f;

    // Basic physics update - stub implementation
    integrateRigidBodies(entityManager, scaledDeltaTime);
    updateCreaturePhysics(entityManager, scaledDeltaTime);
    applyEnvironmentalForces(entityManager, scaledDeltaTime);
    updateGroundDetection(entityManager);

    // Collision detection and response
    if (collisionEnabled_) {
        detectCollisions(entityManager);
        resolveCollisions(entityManager, scaledDeltaTime);
    }

    // Update transforms with new physics data
    updateTransforms(entityManager, scaledDeltaTime);

    // Calculate frame timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    stats_.updateTime = duration.count() / 1000.0f; // Convert to milliseconds
}

void PhysicsSystem::fixedUpdate(EntityManager& entityManager, float fixedDeltaTime) {
    if (!initialized_) return;

    // Fixed timestep accumulator for stable physics - stub implementation
    accumulator_ += fixedDeltaTime * timeScale_;

    while (accumulator_ >= FIXED_TIMESTEP) {
        // Run fixed timestep physics for stability
        applyEnvironmentalForces(entityManager, FIXED_TIMESTEP);

        if (collisionEnabled_) {
            detectCollisions(entityManager);
            resolveCollisions(entityManager, FIXED_TIMESTEP);
        }

        accumulator_ -= FIXED_TIMESTEP;
    }
}

void PhysicsSystem::setCollisionMatrix(uint32_t layer1, uint32_t layer2, bool canCollide) {
    if (layer1 >= 32 || layer2 >= 32) {
        Logger::getInstance().warning("PhysicsSystem: Invalid layer index, must be < 32");
        return;
    }

    if (canCollide) {
        collisionMatrix_[layer1] |= (1 << layer2);
        collisionMatrix_[layer2] |= (1 << layer1);
    } else {
        collisionMatrix_[layer1] &= ~(1 << layer2);
        collisionMatrix_[layer2] &= ~(1 << layer1);
    }

    Logger::getInstance().debug("PhysicsSystem: Set collision layer pair");
}

bool PhysicsSystem::shouldLayersCollide(uint32_t layer1, uint32_t layer2) const {
    if (layer1 >= 32 || layer2 >= 32) return false;
    return (collisionMatrix_[layer1] & (1 << layer2)) != 0;
}

PhysicsSystem::RaycastHit PhysicsSystem::raycast(const glm::vec3& origin,
                                                 const glm::vec3& direction,
                                                 float maxDistance,
                                                 uint32_t layerMask) {
    RaycastHit hit{};

    // Use spatial system for efficient ray casting if available
    if (spatialSystem_) {
        // Get entities along ray path from spatial system
        // This is a placeholder - actual implementation would use spatial queries
        Logger::getInstance().debug("PhysicsSystem: Raycast with spatial optimization not yet implemented");
    }

    // Fallback: Check all entities with collision components
    // TODO: Implement actual ray-sphere, ray-box intersection tests
    Logger::getInstance().debug("PhysicsSystem: Raycast performed");

    return hit;
}

std::vector<EntityID> PhysicsSystem::overlapSphere(const glm::vec3& center,
                                                   float radius,
                                                   uint32_t layerMask) {
    std::vector<EntityID> results;

    // Use spatial system for efficient overlap detection if available
    if (spatialSystem_) {
        // Leverage spatial partitioning for performance
        Logger::getInstance().debug("PhysicsSystem: Sphere overlap with spatial optimization not yet implemented");
    }

    Logger::getInstance().debug("PhysicsSystem: Sphere overlap performed");

    return results;
}

// =============================================================================
// PRIVATE IMPLEMENTATION (STUB METHODS)
// =============================================================================

void PhysicsSystem::integrateRigidBodies(EntityManager& entityManager, float deltaTime) {
    // Get all entities with RigidBodyComponent
    const auto& rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();

    // Convert deltaTime from milliseconds to seconds
    float deltaTimeSeconds = deltaTime / 1000.0f;

    for (EntityID entity : rigidBodyEntities) {
        if (!entityManager.hasComponent<RigidBodyComponent>(entity) ||
            !entityManager.hasComponent<Transform>(entity)) {
            continue;
        }

        auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        auto& transform = entityManager.getComponent<Transform>(entity);

        if (!rigidBody.isDynamic || rigidBody.mass <= 0.0f) continue;

        stats_.activeRigidBodies++;

        // Store initial velocity to detect changes
        glm::vec3 initialVelocity = rigidBody.velocity;
        glm::vec3 initialAngularVelocity = rigidBody.angularVelocity;

        // Apply forces to velocity (F = ma -> a = F/m)
        glm::vec3 acceleration = rigidBody.force / rigidBody.mass;
        rigidBody.velocity += acceleration * deltaTimeSeconds;

        // Apply linear damping (air resistance)
        rigidBody.velocity *= (1.0f - rigidBody.linearDamping * deltaTimeSeconds);

        // Apply angular damping
        rigidBody.angularVelocity *= (1.0f - rigidBody.angularDamping * deltaTimeSeconds);

        // Implement terminal velocity for realistic falling
        float speed = glm::length(rigidBody.velocity);
        float terminalVelocity = 30.0f; // Realistic terminal velocity (m/s)
        if (speed > terminalVelocity) {
            rigidBody.velocity = glm::normalize(rigidBody.velocity) * terminalVelocity;
        }

        // Check if physics properties changed and mark for synchronization
        if (glm::length(rigidBody.velocity - initialVelocity) > 0.001f ||
            glm::length(rigidBody.angularVelocity - initialAngularVelocity) > 0.001f) {
            rigidBody.needsSync = true;
        }

        // Accumulate average velocity for statistics
        stats_.averageVelocity += speed;

        // Clear frame forces after integration
        rigidBody.clearForces();
    }

    // Calculate average velocity
    if (stats_.activeRigidBodies > 0) {
        stats_.averageVelocity /= stats_.activeRigidBodies;
    }
}

void PhysicsSystem::updateCreaturePhysics(EntityManager& entityManager, float deltaTime) {
    // Get all entities with CreaturePhysicsComponent
    const auto& creatureEntities = entityManager.getEntitiesWithComponent<CreaturePhysicsComponent>();

    for (EntityID entity : creatureEntities) {
        if (!entityManager.hasComponent<CreaturePhysicsComponent>(entity) ||
            !entityManager.hasComponent<RigidBodyComponent>(entity) ||
            !entityManager.hasComponent<Transform>(entity)) {
            continue;
        }

        auto& creaturePhysics = entityManager.getComponent<CreaturePhysicsComponent>(entity);
        auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        auto& transform = entityManager.getComponent<Transform>(entity);

        // Update movement state timing
        creaturePhysics.updateMovementState(deltaTime);

        // Apply movement input as force
        if (glm::length(creaturePhysics.inputDirection) > 0.001f) {
            glm::vec3 movementForce = creaturePhysics.getMovementForce(rigidBody.mass);
            rigidBody.applyForce(movementForce);
        }

        // Handle jumping
        if (creaturePhysics.wantsToJump && creaturePhysics.canJump()) {
            glm::vec3 jumpVelocity = creaturePhysics.getJumpVelocity();
            rigidBody.applyImpulse(jumpVelocity);
        }

        // Clear single-frame inputs
        creaturePhysics.clearFrameInputs();
    }
}

void PhysicsSystem::applyEnvironmentalForces(EntityManager& entityManager, float deltaTime) {
    // Apply gravity to all dynamic rigid bodies
    const auto& rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();

    for (EntityID entity : rigidBodyEntities) {
        if (!entityManager.hasComponent<RigidBodyComponent>(entity)) continue;

        auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        if (!rigidBody.isDynamic || !rigidBody.useGravity) continue;

        // Apply gravity force
        glm::vec3 gravityForce = worldGravity_ * rigidBody.mass;
        rigidBody.applyForce(gravityForce);
    }
}

void PhysicsSystem::updateGroundDetection(EntityManager& entityManager) {
    // Update ground detection for creatures - stub implementation
    const auto& creatureEntities = entityManager.getEntitiesWithComponent<CreaturePhysicsComponent>();

    for (EntityID entity : creatureEntities) {
        if (!entityManager.hasComponent<CreaturePhysicsComponent>(entity) ||
            !entityManager.hasComponent<Transform>(entity)) {
            continue;
        }

        auto& creaturePhysics = entityManager.getComponent<CreaturePhysicsComponent>(entity);
        auto& transform = entityManager.getComponent<Transform>(entity);

        // Simple ground detection - assume ground at y=0
        bool wasGrounded = creaturePhysics.isGrounded;
        creaturePhysics.isGrounded = (transform.position.y <= creaturePhysics.groundSnapDistance);

        if (creaturePhysics.isGrounded) {
            creaturePhysics.groundNormal = glm::vec3(0, 1, 0);
        }
    }
}

void PhysicsSystem::detectCollisions(EntityManager& entityManager) {
    // Simple stub collision detection
    const auto& collisionEntities = entityManager.getEntitiesWithComponent<CollisionComponent>();

    stats_.collisionChecks = collisionEntities.size() * (collisionEntities.size() - 1) / 2;
    stats_.collisionHits = 0; // No actual collision detection yet
}

void PhysicsSystem::resolveCollisions(EntityManager& entityManager, float deltaTime) {
    // Collision resolution stub
    if (stats_.collisionHits > 0) {
        Logger::getInstance().debug("PhysicsSystem: Resolving collisions (stub)");
    }
}

void PhysicsSystem::updateTransforms(EntityManager& entityManager, float deltaTime) {
    // Update Transform components from RigidBody physics data
    const auto& rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();

    for (EntityID entity : rigidBodyEntities) {
        if (!entityManager.hasComponent<RigidBodyComponent>(entity) ||
            !entityManager.hasComponent<Transform>(entity)) {
            continue;
        }

        auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        auto& transform = entityManager.getComponent<Transform>(entity);

        if (!rigidBody.needsSync || !rigidBody.isDynamic) continue;

        // Convert deltaTime from milliseconds to seconds for physics calculations
        float deltaTimeSeconds = deltaTime / 1000.0f;

        // Integrate position from velocity using real deltaTime
        transform.position += rigidBody.velocity * deltaTimeSeconds;

        // Integrate rotation from angular velocity using real deltaTime
        if (!rigidBody.freezeRotation && glm::length(rigidBody.angularVelocity) > 0.001f) {
            float angle = glm::length(rigidBody.angularVelocity) * deltaTimeSeconds;
            glm::vec3 axis = glm::normalize(rigidBody.angularVelocity);
            glm::quat deltaRotation = glm::angleAxis(angle, axis);
            transform.rotation = deltaRotation * transform.rotation;
        }

        // Clear sync flag
        rigidBody.needsSync = false;
    }
}

bool PhysicsSystem::checkCollision(const CollisionComponent& collider1, const Transform& transform1,
                                  const CollisionComponent& collider2, const Transform& transform2) {
    // Simple sphere-sphere collision detection for now
    float radius1 = collider1.getBoundingSphereRadius();
    float radius2 = collider2.getBoundingSphereRadius();
    float distance = glm::distance(transform1.position, transform2.position);

    return distance < (radius1 + radius2);
}

void PhysicsSystem::resolveCollision(EntityID entity1, EntityID entity2,
                                    RigidBodyComponent& body1, RigidBodyComponent& body2,
                                    const glm::vec3& collisionNormal, float penetration) {
    // Simple collision resolution stub
    Logger::getInstance().debug("PhysicsSystem: Collision resolved (stub)");
}

} // namespace VulkanMon