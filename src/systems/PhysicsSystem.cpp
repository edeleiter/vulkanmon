#define GLM_ENABLE_EXPERIMENTAL
#include "PhysicsSystem.h"
#include "../core/EntityManager.h"
#include "../systems/SpatialSystem.h"
#include "../utils/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <chrono>
#include <thread>

namespace VulkanMon {

// SystemBase interface implementation
void PhysicsSystem::update(float deltaTime, EntityManager& entityManager) {
    if (!initialized_) return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Apply time scaling
    float scaledDeltaTime = deltaTime * timeScale_;

    // Reset frame statistics
    stats_.activeRigidBodies = 0;
    stats_.collisionChecks = 0;
    stats_.collisionHits = 0;
    stats_.averageVelocity = 0.0f;

    // Step Jolt Physics simulation (only physics system)
    if (joltPhysics_) {
        try {
            // Create Jolt bodies for any new entities with physics components
            createJoltBodiesForNewEntities(entityManager);

            // Update Jolt physics simulation with safe time step
            // CRITICAL: Jolt Physics expects deltaTime in SECONDS
            // Our deltaTime parameter comes from Application::frameTime_ which is in MILLISECONDS
            const float deltaTimeSeconds = scaledDeltaTime / 1000.0f; // Convert milliseconds to seconds
            const float maxTimeStep = 1.0f / 60.0f; // Cap at 60 FPS timestep (seconds)
            const float clampedDeltaTime = std::min(deltaTimeSeconds, maxTimeStep);
            const int collisionSteps = 1;

            joltPhysics_->Update(clampedDeltaTime, collisionSteps, tempAllocator_.get(), jobSystem_.get());

            // Sync transforms from Jolt back to ECS components
            syncTransformsFromJolt(entityManager);

            // Update statistics from Jolt
            updateStatsFromJolt();
        } catch (const std::exception& e) {
            VKMON_ERROR("Jolt Physics update failed: " + std::string(e.what()));
            VKMON_ERROR("Physics system cannot continue without Jolt Physics");
            return;
        }
    } else {
        VKMON_ERROR("PhysicsSystem: Jolt Physics not initialized! Cannot proceed.");
        return;
    }

    // Calculate frame timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    stats_.updateTime = duration.count() / 1000.0f; // Convert to milliseconds
}

void PhysicsSystem::initialize(EntityManager& entityManager) {
    VKMON_INFO("Initializing PhysicsSystem with default gravity");

    worldGravity_ = glm::vec3(0.0f, -9.81f, 0.0f);
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

    // Initialize Jolt Physics System
    initializeJoltPhysics();

    initialized_ = true;
    VKMON_INFO("PhysicsSystem initialized successfully");
}

void PhysicsSystem::shutdown(EntityManager& entityManager) {
    if (!initialized_) return;

    VKMON_INFO("Shutting down PhysicsSystem");

    // Shutdown Jolt Physics System
    shutdownJoltPhysics();

    spatialSystem_ = nullptr;
    initialized_ = false;

    VKMON_INFO("PhysicsSystem shutdown complete");
}

// Physics-specific methods

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
        VKMON_WARNING("PhysicsSystem: Invalid layer index, must be < 32");
        return;
    }

    if (canCollide) {
        collisionMatrix_[layer1] |= (1 << layer2);
        collisionMatrix_[layer2] |= (1 << layer1);
    } else {
        collisionMatrix_[layer1] &= ~(1 << layer2);
        collisionMatrix_[layer2] &= ~(1 << layer1);
    }

    VKMON_DEBUG("PhysicsSystem: Set collision layer pair");
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

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot perform raycast - Jolt Physics not initialized");
        return hit;
    }

    // Normalize direction vector for consistent behavior
    glm::vec3 normalizedDir = glm::normalize(direction);
    glm::vec3 endPoint = origin + normalizedDir * maxDistance;

    // Convert to Jolt coordinates
    JPH::Vec3 joltOrigin(origin.x, origin.y, origin.z);
    JPH::Vec3 joltDirection(normalizedDir.x, normalizedDir.y, normalizedDir.z);

    // Create Jolt raycast settings
    JPH::RayCast ray;
    ray.mOrigin = joltOrigin;
    ray.mDirection = joltDirection * maxDistance;

    // Use narrow phase query for precise raycast
    const JPH::NarrowPhaseQuery& narrowPhase = joltPhysics_->GetNarrowPhaseQuery();

    // Simple raycast using Jolt's built-in result structure
    JPH::RayCastResult rayResult;

    // Use simplified raycast approach - cast ray and get single closest result
    JPH::RRayCast joltRay{ray.mOrigin, ray.mDirection};

    // Perform the raycast using all default filters for now (can be enhanced later)
    bool hasHit = narrowPhase.CastRay(joltRay, rayResult, {}, {}, {});

    // Process results
    if (hasHit) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(rayResult.mBodyID);
        if (it != bodyToEntityMap_.end()) {
            hit.hit = true;
            hit.distance = rayResult.mFraction * maxDistance;
            hit.entity = it->second;

            // Calculate hit point
            glm::vec3 hitPoint = origin + normalizedDir * hit.distance;
            hit.point = hitPoint;

            // Get surface normal from Jolt (simplified for now)
            hit.normal = glm::vec3(0, 1, 0); // Default upward normal - can be improved with proper Jolt normal extraction

            VKMON_DEBUG("PhysicsSystem: Raycast hit entity " + std::to_string(hit.entity) +
                       " at distance " + std::to_string(hit.distance));
        }
    }

    return hit;
}

std::vector<EntityID> PhysicsSystem::overlapSphere(const glm::vec3& center,
                                                   float radius,
                                                   uint32_t layerMask) {
    std::vector<EntityID> results;

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot perform sphere overlap - Jolt Physics not initialized");
        return results;
    }

    // Use spatial system for pre-filtering if available (performance optimization)
    if (spatialSystem_) {
        // Get candidate entities from spatial system first
        auto spatialCandidates = spatialSystem_->queryRadius(center, radius);

        // TODO: Cross-reference spatial candidates with Jolt Physics for precise collision detection
        // For now, we'll use the full Jolt Physics approach and optimize later
        VKMON_DEBUG("PhysicsSystem: Sphere overlap with spatial pre-filtering (candidates: " +
                   std::to_string(spatialCandidates.size()) + ")");
    }

    // Create sphere shape for overlap testing
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeRefC sphereShape = sphereSettings.Create().Get();

    if (!sphereShape) {
        VKMON_ERROR("PhysicsSystem: Failed to create sphere shape for overlap query");
        return results;
    }

    // Convert center position to Jolt coordinates
    JPH::Vec3 joltCenter(center.x, center.y, center.z);
    JPH::Quat joltRotation = JPH::Quat::sIdentity();

    // Create transformation matrix for CollideShape API
    JPH::Mat44 transform = JPH::Mat44::sRotationTranslation(joltRotation, joltCenter);

    // Use narrow phase query for precise overlap detection
    const JPH::NarrowPhaseQuery& narrowPhase = joltPhysics_->GetNarrowPhaseQuery();

    // Storage for collision results
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

    // Perform the overlap query using CollideShape with correct API
    JPH::CollideShapeSettings settings;
    narrowPhase.CollideShape(sphereShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3(joltCenter), collector, {}, {}, {}, {});

    // Process results and convert body IDs back to entity IDs
    for (const auto& hit : collector.mHits) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(hit.mBodyID2);
        if (it != bodyToEntityMap_.end()) {
            EntityID entityId = it->second;

            // TODO: Add layer mask filtering here for more precise control
            // For now, include all hits
            results.push_back(entityId);
        }
    }

    VKMON_DEBUG("PhysicsSystem: Sphere overlap at (" +
               std::to_string(center.x) + ", " +
               std::to_string(center.y) + ", " +
               std::to_string(center.z) + ") radius " +
               std::to_string(radius) + " found " +
               std::to_string(results.size()) + " entities");

    return results;
}

// =============================================================================
// PRIVATE IMPLEMENTATION (STUB METHODS)
// =============================================================================


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
    // Ground collision for all rigid body entities
    const auto& rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();

    const float GROUND_Y = -4.0f; // Ground plane low but still visible

    for (EntityID entity : rigidBodyEntities) {
        if (!entityManager.hasComponent<RigidBodyComponent>(entity) ||
            !entityManager.hasComponent<Transform>(entity)) {
            continue;
        }

        auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        auto& transform = entityManager.getComponent<Transform>(entity);

        if (!rigidBody.isDynamic) continue;

        // Get entity's bottom position (assuming entities have some height)
        float entityRadius = 0.5f; // Default bounding radius
        if (entityManager.hasComponent<CollisionComponent>(entity)) {
            auto& collision = entityManager.getComponent<CollisionComponent>(entity);
            entityRadius = collision.getBoundingSphereRadius();
        }

        float bottomY = transform.position.y - entityRadius;

        // Check ground collision
        if (bottomY <= GROUND_Y) {
            // Entity hit the ground - apply collision response

            // Position correction: move entity above ground
            transform.position.y = GROUND_Y + entityRadius;

            // Velocity response: bounce with restitution
            if (rigidBody.velocity.y < -0.5f) { // Only if moving downward with significant speed
                float oldVelocity = rigidBody.velocity.y;
                rigidBody.velocity.y = -rigidBody.velocity.y * rigidBody.restitution;

                // Apply minimum velocity threshold to stop tiny bounces
                if (abs(rigidBody.velocity.y) < 0.1f) {
                    rigidBody.velocity.y = 0.0f;
                }

                // Apply friction to horizontal movement when on ground
                rigidBody.velocity.x *= (1.0f - rigidBody.friction * 0.1f);
                rigidBody.velocity.z *= (1.0f - rigidBody.friction * 0.1f);


                // Only log significant bounces
                if (abs(oldVelocity) > 1.0f) {
                    VKMON_INFO("Ground collision: Entity bounced from " +
                               std::to_string(oldVelocity) + " to " +
                               std::to_string(rigidBody.velocity.y));
                }
            } else if (rigidBody.velocity.y < 0.0f) {
                // Stop very slow downward movement
                rigidBody.velocity.y = 0.0f;
            }
        }
    }

    // Update ground detection for creatures (existing creature-specific logic)
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
    // Get all entities with collision components
    const auto& collisionEntities = entityManager.getEntitiesWithComponent<CollisionComponent>();

    // Debug: Log collision stats occasionally
    static int logCounter = 0;
    if (++logCounter % 300 == 0) { // Log every 5 seconds at 60 FPS
        VKMON_INFO("PhysicsSystem: " + std::to_string(collisionEntities.size()) + " collision entities");
    }

    // Clear previous collision pairs
    currentCollisions_.clear();

    stats_.collisionChecks = 0;
    stats_.collisionHits = 0;

    // Use spatial system optimization when available
    if (spatialSystem_) {
        // Spatial-optimized collision detection
        for (EntityID entityA : collisionEntities) {
            if (!entityManager.hasComponent<CollisionComponent>(entityA) ||
                !entityManager.hasComponent<Transform>(entityA)) {
                continue;
            }

            const auto& colliderA = entityManager.getComponent<CollisionComponent>(entityA);
            const auto& transformA = entityManager.getComponent<Transform>(entityA);

            // Skip static objects colliding with other static objects
            bool isStaticA = colliderA.isStatic;
            if (entityManager.hasComponent<RigidBodyComponent>(entityA)) {
                isStaticA = !entityManager.getComponent<RigidBodyComponent>(entityA).isDynamic;
            }

            // Get nearby entities using spatial system
            float queryRadius = colliderA.getBoundingSphereRadius() * 2.0f; // Conservative radius
            auto nearbyEntities = spatialSystem_->queryRadius(transformA.position, queryRadius, LayerMask::All);

            // Spatial query debug logging removed for performance

            for (EntityID entityB : nearbyEntities) {
                if (entityA >= entityB) continue; // Avoid duplicate pairs and self-collision

                if (!entityManager.hasComponent<CollisionComponent>(entityB) ||
                    !entityManager.hasComponent<Transform>(entityB)) {
                    continue;
                }

                const auto& colliderB = entityManager.getComponent<CollisionComponent>(entityB);
                const auto& transformB = entityManager.getComponent<Transform>(entityB);

                // Skip static-static collisions
                bool isStaticB = colliderB.isStatic;
                if (entityManager.hasComponent<RigidBodyComponent>(entityB)) {
                    isStaticB = !entityManager.getComponent<RigidBodyComponent>(entityB).isDynamic;
                }
                if (isStaticA && isStaticB) continue;

                stats_.collisionChecks++;

                // Layer filtering - check if these layers should collide
                if (!shouldLayersCollide(colliderA.layer, colliderB.layer) ||
                    !colliderA.shouldCollideWith(colliderB.layer) ||
                    !colliderB.shouldCollideWith(colliderA.layer)) {
                    continue;
                }

                // Narrow-phase collision detection
                if (checkCollision(colliderA, transformA, colliderB, transformB)) {
                    // Store collision pair for resolution
                    CollisionPair collision;
                    collision.entityA = entityA;
                    collision.entityB = entityB;
                    collision.penetration = 0.0f; // Will be calculated in resolution
                    collision.normal = glm::normalize(transformB.position - transformA.position);

                    currentCollisions_.push_back(collision);
                    stats_.collisionHits++;

                    // Reduce logging frequency for performance
                    static int collisionLogCounter = 0;
                    if (++collisionLogCounter % 60 == 0) { // Log every 60 collisions (1 per second at 60 FPS)
                        VKMON_DEBUG("PhysicsSystem: Collision detected between entities " +
                                   std::to_string(entityA) + " and " + std::to_string(entityB) +
                                   " (collision #" + std::to_string(collisionLogCounter) + ")");
                    }
                }
            }
        }
    } else {
        // Fallback: brute force O(n²) collision detection without spatial optimization
        static bool warningLogged = false;
        if (!warningLogged) {
            VKMON_WARNING("PhysicsSystem: No spatial system available, using brute force collision detection");
            warningLogged = true;
        }

        for (size_t i = 0; i < collisionEntities.size(); ++i) {
            for (size_t j = i + 1; j < collisionEntities.size(); ++j) {
                EntityID entityA = collisionEntities[i];
                EntityID entityB = collisionEntities[j];

                if (!entityManager.hasComponent<CollisionComponent>(entityA) ||
                    !entityManager.hasComponent<CollisionComponent>(entityB) ||
                    !entityManager.hasComponent<Transform>(entityA) ||
                    !entityManager.hasComponent<Transform>(entityB)) {
                    continue;
                }

                const auto& colliderA = entityManager.getComponent<CollisionComponent>(entityA);
                const auto& colliderB = entityManager.getComponent<CollisionComponent>(entityB);
                const auto& transformA = entityManager.getComponent<Transform>(entityA);
                const auto& transformB = entityManager.getComponent<Transform>(entityB);

                // Skip static-static collisions
                bool isStaticA = colliderA.isStatic;
                bool isStaticB = colliderB.isStatic;
                if (entityManager.hasComponent<RigidBodyComponent>(entityA)) {
                    isStaticA = !entityManager.getComponent<RigidBodyComponent>(entityA).isDynamic;
                }
                if (entityManager.hasComponent<RigidBodyComponent>(entityB)) {
                    isStaticB = !entityManager.getComponent<RigidBodyComponent>(entityB).isDynamic;
                }
                if (isStaticA && isStaticB) continue;

                stats_.collisionChecks++;

                // Layer filtering
                if (!shouldLayersCollide(colliderA.layer, colliderB.layer) ||
                    !colliderA.shouldCollideWith(colliderB.layer) ||
                    !colliderB.shouldCollideWith(colliderA.layer)) {
                    continue;
                }

                // Narrow-phase collision detection
                if (checkCollision(colliderA, transformA, colliderB, transformB)) {
                    // Store collision pair for resolution
                    CollisionPair collision;
                    collision.entityA = entityA;
                    collision.entityB = entityB;
                    collision.penetration = 0.0f;
                    collision.normal = glm::normalize(transformB.position - transformA.position);

                    currentCollisions_.push_back(collision);
                    stats_.collisionHits++;
                }
            }
        }
    }

    // Log collision stats only occasionally to reduce spam
    static int perfLogCounter = 0;
    if (++perfLogCounter % 300 == 0) { // Every 5 seconds
        VKMON_INFO("PhysicsSystem Performance: " + std::to_string(stats_.collisionChecks) +
                   " checks, " + std::to_string(stats_.collisionHits) + " hits, " +
                   std::to_string(stats_.updateTime) + "ms update time");
    }
}

void PhysicsSystem::resolveCollisions(EntityManager& entityManager, float deltaTime) {
    // Resolve all detected collisions
    for (const auto& collision : currentCollisions_) {
        EntityID entityA = collision.entityA;
        EntityID entityB = collision.entityB;

        // Ensure both entities still exist and have required components
        if (!entityManager.hasComponent<Transform>(entityA) || !entityManager.hasComponent<Transform>(entityB) ||
            !entityManager.hasComponent<CollisionComponent>(entityA) || !entityManager.hasComponent<CollisionComponent>(entityB)) {
            continue;
        }

        auto& transformA = entityManager.getComponent<Transform>(entityA);
        auto& transformB = entityManager.getComponent<Transform>(entityB);
        const auto& colliderA = entityManager.getComponent<CollisionComponent>(entityA);
        const auto& colliderB = entityManager.getComponent<CollisionComponent>(entityB);

        // Calculate actual penetration depth and normal
        float radiusA = colliderA.getBoundingSphereRadius();
        float radiusB = colliderB.getBoundingSphereRadius();
        glm::vec3 direction = transformB.position - transformA.position;
        float distance = glm::length(direction);

        if (distance < 0.001f) {
            // Entities are at exactly the same position - separate them
            direction = glm::vec3(1.0f, 0.0f, 0.0f); // Default separation direction
            distance = 0.001f;
        } else {
            direction = direction / distance; // Normalize
        }

        float penetrationDepth = (radiusA + radiusB) - distance;
        if (penetrationDepth <= 0.0f) continue; // No actual penetration

        // Handle trigger collisions (pass-through but generate events)
        if (colliderA.isTrigger || colliderB.isTrigger) {
            // Trigger collision - no physical response but call callbacks
            if (colliderA.isTrigger && colliderA.onTriggerEnter) {
                colliderA.onTriggerEnter(entityA, entityB);
            }
            if (colliderB.isTrigger && colliderB.onTriggerEnter) {
                colliderB.onTriggerEnter(entityB, entityA);
            }
            continue; // No physical separation for triggers
        }

        // Get rigid body components (if they exist)
        RigidBodyComponent* bodyA = entityManager.hasComponent<RigidBodyComponent>(entityA) ?
                                   &entityManager.getComponent<RigidBodyComponent>(entityA) : nullptr;
        RigidBodyComponent* bodyB = entityManager.hasComponent<RigidBodyComponent>(entityB) ?
                                   &entityManager.getComponent<RigidBodyComponent>(entityB) : nullptr;

        // Determine which bodies can move
        bool canMoveA = bodyA && bodyA->isDynamic && !colliderA.isStatic;
        bool canMoveB = bodyB && bodyB->isDynamic && !colliderB.isStatic;

        if (!canMoveA && !canMoveB) continue; // Both static, no resolution needed

        // === POSITION CORRECTION (SEPARATION) ===

        float totalMass = 0.0f;
        if (canMoveA) totalMass += bodyA->mass;
        if (canMoveB) totalMass += bodyB->mass;

        if (totalMass < 0.001f) totalMass = 1.0f; // Prevent division by zero

        // Separate objects proportional to their inverse masses
        float separationA = canMoveA ? (bodyB ? bodyB->mass : 1.0f) / totalMass : 0.0f;
        float separationB = canMoveB ? (bodyA ? bodyA->mass : 1.0f) / totalMass : 0.0f;

        glm::vec3 separationVector = direction * penetrationDepth;

        if (canMoveA) {
            transformA.position -= separationVector * separationA;
        }
        if (canMoveB) {
            transformB.position += separationVector * separationB;
        }

        // === IMPULSE RESPONSE (BOUNCING) ===

        if (bodyA && bodyB && (canMoveA || canMoveB)) {
            // Calculate relative velocity
            glm::vec3 relativeVelocity = bodyB->velocity - bodyA->velocity;
            float velocityAlongNormal = glm::dot(relativeVelocity, direction);

            // Objects separating already - no need for impulse
            if (velocityAlongNormal > 0.0f) continue;

            // Calculate combined restitution (bounciness)
            float combinedRestitution = std::min(bodyA->restitution, bodyB->restitution);

            // Calculate impulse magnitude
            float impulseMagnitude = -(1.0f + combinedRestitution) * velocityAlongNormal;

            float massA = canMoveA ? bodyA->mass : 10000.0f; // Large mass for static objects
            float massB = canMoveB ? bodyB->mass : 10000.0f;
            impulseMagnitude /= (1.0f / massA + 1.0f / massB);

            // Apply impulse
            glm::vec3 impulse = direction * impulseMagnitude;

            if (canMoveA) {
                bodyA->velocity -= impulse / massA;
            }
            if (canMoveB) {
                bodyB->velocity += impulse / massB;
            }

            // Apply friction (simple model)
            glm::vec3 tangent = relativeVelocity - direction * velocityAlongNormal;
            if (glm::length(tangent) > 0.001f) {
                tangent = glm::normalize(tangent);
                float combinedFriction = std::sqrt(bodyA->friction * bodyB->friction);
                float frictionMagnitude = std::min(impulseMagnitude * combinedFriction, glm::length(relativeVelocity));

                glm::vec3 frictionImpulse = tangent * frictionMagnitude;
                if (canMoveA) {
                    bodyA->velocity += frictionImpulse / massA;
                }
                if (canMoveB) {
                    bodyB->velocity -= frictionImpulse / massB;
                }
            }

            // Individual collision resolution logging removed for performance
        }

        // Call solid collision callbacks
        if (colliderA.onCollisionEnter) {
            colliderA.onCollisionEnter(entityA, entityB);
        }
        if (colliderB.onCollisionEnter) {
            colliderB.onCollisionEnter(entityB, entityA);
        }
    }

    // Collision resolution summary logging removed for performance
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

// =============================================================================
// JOLT PHYSICS IMPLEMENTATION
// =============================================================================

void PhysicsSystem::initializeJoltPhysics() {
    VKMON_INFO("Initializing Jolt Physics system...");

    // Register allocation hook for Jolt
    JPH::RegisterDefaultAllocator();

    // Initialize Jolt Factory
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Create temp allocator for Jolt (10MB should be enough for most games)
    tempAllocator_ = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    // Create job system for multi-threading
    // Use configured thread count, or auto-detect if not set
    uint32_t actualThreadCount = threadCount_;
    if (actualThreadCount == 0) {
        actualThreadCount = std::max(1u, std::thread::hardware_concurrency() - 1);
    }
    jobSystem_ = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs,
                                                            JPH::cMaxPhysicsBarriers,
                                                            actualThreadCount);

    // Configure physics system settings for game engine use
    const uint32_t cMaxBodies = 1024;           // Support up to 1024 physics bodies
    const uint32_t cNumBodyMutexes = 0;         // Autodetect based on cores
    const uint32_t cMaxBodyPairs = 1024;        // Max collision pairs per step
    const uint32_t cMaxContactConstraints = 1024; // Max contact constraints

    // Create layer filtering interface implementations
    broadPhaseLayerInterface_ = std::make_unique<BroadPhaseLayerInterfaceImpl>();
    objectVsBroadPhaseLayerFilter_ = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    objectLayerPairFilter_ = std::make_unique<ObjectLayerPairFilterImpl>();

    // Create the main Jolt physics system
    joltPhysics_ = std::make_unique<JPH::PhysicsSystem>();
    joltPhysics_->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
                       *broadPhaseLayerInterface_, *objectVsBroadPhaseLayerFilter_, *objectLayerPairFilter_);

    // Set gravity
    joltPhysics_->SetGravity(JPH::Vec3(worldGravity_.x, worldGravity_.y, worldGravity_.z));

    // Setup collision layers
    setupJoltLayers();

    VKMON_INFO("Jolt Physics initialized with " + std::to_string(actualThreadCount) + " threads");
}

void PhysicsSystem::shutdownJoltPhysics() {
    if (!joltPhysics_) return;

    VKMON_INFO("Shutting down Jolt Physics...");

    // Clear entity mappings
    entityToBodyMap_.clear();
    bodyToEntityMap_.clear();

    // Destroy Jolt objects in reverse order
    joltPhysics_.reset();
    objectLayerPairFilter_.reset();
    objectVsBroadPhaseLayerFilter_.reset();
    broadPhaseLayerInterface_.reset();
    jobSystem_.reset();
    tempAllocator_.reset();

    // Cleanup factory
    if (JPH::Factory::sInstance) {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    VKMON_INFO("Jolt Physics shutdown complete");
}

void PhysicsSystem::setupJoltLayers() {
    // TODO: Configure collision matrix based on existing collisionMatrix_
    // For now, set up basic layer interactions
    VKMON_INFO("Jolt collision layers configured");
}

JPH::ObjectLayer PhysicsSystem::mapLayerMaskToObjectLayer(uint32_t layerMask) const {
    // Simple mapping for now - can be enhanced later
    if (layerMask & LayerMask::Creatures) return ObjectLayers::CREATURES;
    if (layerMask & LayerMask::Environment) return ObjectLayers::ENVIRONMENT;
    if (layerMask & LayerMask::CaptureDevices) return ObjectLayers::PROJECTILES;
    if (layerMask & LayerMask::Player) return ObjectLayers::PLAYER;
    return ObjectLayers::ENVIRONMENT; // Default fallback
}

JPH::BroadPhaseLayer PhysicsSystem::mapObjectLayerToBroadPhaseLayer(JPH::ObjectLayer objectLayer) const {
    switch (objectLayer) {
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

// =============================================================================
// JOLT PHYSICS BODY CREATION AND MANAGEMENT
// =============================================================================

JPH::ShapeRefC PhysicsSystem::createJoltShape(const CollisionComponent& collision) {
    switch (collision.shapeType) {
        case CollisionComponent::ShapeType::Box: {
            glm::vec3 halfExtents = collision.dimensions * 0.5f;
            return JPH::BoxShapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z)).Create().Get();
        }
        case CollisionComponent::ShapeType::Sphere: {
            float radius = collision.dimensions.x; // Use x dimension as radius
            return JPH::SphereShapeSettings(radius).Create().Get();
        }
        case CollisionComponent::ShapeType::Capsule: {
            float height = collision.dimensions.y;
            float radius = collision.dimensions.x;
            return JPH::CapsuleShapeSettings(height * 0.5f, radius).Create().Get();
        }
        default:
            VKMON_WARNING("PhysicsSystem: Unsupported shape type, using box fallback");
            glm::vec3 halfExtents = collision.dimensions * 0.5f;
            return JPH::BoxShapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z)).Create().Get();
    }
}

JPH::BodyID PhysicsSystem::createJoltBody(EntityID entity, const RigidBodyComponent& rigidBody,
                                          const CollisionComponent& collision, const Transform& transform) {
    if (!joltPhysics_) {
        VKMON_ERROR("PhysicsSystem: Cannot create Jolt body - Jolt Physics not initialized");
        return JPH::BodyID();
    }

    // Create collision shape
    JPH::ShapeRefC shape = createJoltShape(collision);

    // Configure body creation settings
    JPH::BodyCreationSettings bodySettings(shape, JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                                           JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                                           rigidBody.isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                           mapLayerMaskToObjectLayer(collision.layer));

    // Configure motion quality for dynamic bodies
    if (rigidBody.isDynamic && rigidBody.mass > 0.0f) {
        bodySettings.mMotionQuality = JPH::EMotionQuality::Discrete;
    }

    // Set material properties
    bodySettings.mRestitution = rigidBody.restitution;
    bodySettings.mFriction = rigidBody.friction;

    // Set mass properties for dynamic bodies
    if (bodySettings.mMotionType == JPH::EMotionType::Dynamic) {
        bodySettings.mMassPropertiesOverride = JPH::MassProperties();
        bodySettings.mMassPropertiesOverride.mMass = rigidBody.mass;
        bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    }

    // Create the body in Jolt Physics
    JPH::Body* body = joltPhysics_->GetBodyInterface().CreateBody(bodySettings);
    if (!body) {
        VKMON_ERROR("PhysicsSystem: Failed to create Jolt body for entity " + std::to_string(entity));
        return JPH::BodyID();
    }

    JPH::BodyID bodyID = body->GetID();

    // Add body to simulation first (required before setting velocities)
    joltPhysics_->GetBodyInterface().AddBody(bodyID, JPH::EActivation::Activate);

    // Set initial velocity after body is added to simulation
    if (bodySettings.mMotionType == JPH::EMotionType::Dynamic) {
        joltPhysics_->GetBodyInterface().SetLinearVelocity(bodyID,
            JPH::Vec3(rigidBody.velocity.x, rigidBody.velocity.y, rigidBody.velocity.z));
        joltPhysics_->GetBodyInterface().SetAngularVelocity(bodyID,
            JPH::Vec3(rigidBody.angularVelocity.x, rigidBody.angularVelocity.y, rigidBody.angularVelocity.z));
    }

    // Store bidirectional mapping
    entityToBodyMap_[entity] = bodyID;
    bodyToEntityMap_[bodyID] = entity;

    return bodyID;
}

void PhysicsSystem::destroyJoltBody(EntityID entity) {
    if (!joltPhysics_) return;

    auto it = entityToBodyMap_.find(entity);
    if (it == entityToBodyMap_.end()) return;

    JPH::BodyID bodyID = it->second;

    // Remove from simulation
    joltPhysics_->GetBodyInterface().RemoveBody(bodyID);

    // Destroy the body
    joltPhysics_->GetBodyInterface().DestroyBody(bodyID);

    // Remove from mappings
    bodyToEntityMap_.erase(bodyID);
    entityToBodyMap_.erase(it);

    VKMON_DEBUG("PhysicsSystem: Destroyed Jolt body for entity " + std::to_string(entity));
}

void PhysicsSystem::updateJoltBodyFromComponent(EntityID entity, const RigidBodyComponent& rigidBody) {
    if (!joltPhysics_) return;

    auto it = entityToBodyMap_.find(entity);
    if (it == entityToBodyMap_.end()) return;

    JPH::BodyID bodyID = it->second;
    JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

    // Update material properties
    bodyInterface.SetRestitution(bodyID, rigidBody.restitution);
    bodyInterface.SetFriction(bodyID, rigidBody.friction);

    // Update velocity for dynamic bodies
    if (bodyInterface.GetMotionType(bodyID) == JPH::EMotionType::Dynamic) {
        bodyInterface.SetLinearVelocity(bodyID,
            JPH::Vec3(rigidBody.velocity.x, rigidBody.velocity.y, rigidBody.velocity.z));
        bodyInterface.SetAngularVelocity(bodyID,
            JPH::Vec3(rigidBody.angularVelocity.x, rigidBody.angularVelocity.y, rigidBody.angularVelocity.z));
    }
}

void PhysicsSystem::syncTransformsFromJolt(EntityManager& entityManager) {
    if (!joltPhysics_) {
        VKMON_DEBUG("syncTransformsFromJolt: No Jolt physics system");
        return;
    }

    try {
        VKMON_DEBUG("syncTransformsFromJolt: Getting body interface...");
        JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
        VKMON_DEBUG("syncTransformsFromJolt: Body interface obtained, processing " + std::to_string(entityToBodyMap_.size()) + " bodies");
        int syncCount = 0;

        for (const auto& [entityID, bodyID] : entityToBodyMap_) {
            VKMON_DEBUG("syncTransformsFromJolt: Processing entity " + std::to_string(entityID));
            try {
                // Check if body is valid
                if (bodyID.IsInvalid()) {
                    VKMON_WARNING("syncTransformsFromJolt: Invalid bodyID for entity " + std::to_string(entityID));
                    continue;
                }

                // Sync ALL physics entities every frame for reliable ECS integration
                if (entityManager.hasComponent<Transform>(entityID)) {
                    auto& transform = entityManager.getComponent<Transform>(entityID);

                    // Get position and rotation from Jolt
                    JPH::Vec3 position = bodyInterface.GetPosition(bodyID);
                    JPH::Quat rotation = bodyInterface.GetRotation(bodyID);

                    // Update Transform component
                    transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
                    transform.rotation = glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());

                    // Mark transform as dirty for rendering
                    transform.markDirty();
                    syncCount++;
                }

                // Also sync velocity to RigidBodyComponent for test compatibility
                if (entityManager.hasComponent<RigidBodyComponent>(entityID)) {
                    auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entityID);

                    // Get velocity from Jolt
                    JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(bodyID);
                    JPH::Vec3 angularVelocity = bodyInterface.GetAngularVelocity(bodyID);

                    // Update RigidBodyComponent velocity
                    rigidBody.velocity = glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
                    rigidBody.angularVelocity = glm::vec3(angularVelocity.GetX(), angularVelocity.GetY(), angularVelocity.GetZ());
                }
            } catch (const std::exception& e) {
                VKMON_ERROR("syncTransformsFromJolt: Error syncing entity " + std::to_string(entityID) + ": " + e.what());
            }
        }

        if (syncCount > 0) {
            VKMON_DEBUG("syncTransformsFromJolt: Synced " + std::to_string(syncCount) + " entities");
        }
    } catch (const std::exception& e) {
        VKMON_ERROR("syncTransformsFromJolt: Critical error: " + std::string(e.what()));
    }
}

void PhysicsSystem::syncTransformsToJolt(EntityManager& entityManager) {
    if (!joltPhysics_) return;

    JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

    for (const auto& [entityID, bodyID] : entityToBodyMap_) {
        if (entityManager.hasComponent<Transform>(entityID)) {
            const auto& transform = entityManager.getComponent<Transform>(entityID);

            // Only sync if transform has been modified
            if (transform.isDirty) {
                bodyInterface.SetPosition(bodyID,
                    JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                    JPH::EActivation::Activate);
                bodyInterface.SetRotation(bodyID,
                    JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                    JPH::EActivation::Activate);
            }
        }
    }
}

void PhysicsSystem::updateStatsFromJolt() {
    if (!joltPhysics_) return;

    const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

    // Count active bodies and calculate average velocity
    int activeBodies = 0;
    float totalVelocity = 0.0f;

    for (const auto& [entityID, bodyID] : entityToBodyMap_) {
        if (bodyInterface.IsActive(bodyID)) {
            activeBodies++;

            if (bodyInterface.GetMotionType(bodyID) == JPH::EMotionType::Dynamic) {
                JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(bodyID);
                totalVelocity += velocity.Length();
            }
        }
    }

    // Update statistics
    stats_.activeRigidBodies = activeBodies;
    stats_.averageVelocity = (activeBodies > 0) ? (totalVelocity / activeBodies) : 0.0f;

    // For collision stats, Jolt doesn't expose these directly
    // Use conservative estimates based on broad phase optimization
    int totalBodies = static_cast<int>(entityToBodyMap_.size());
    stats_.collisionChecks = (totalBodies > 1) ? std::min(totalBodies * 10, 200) : 0; // Jolt's broad phase reduces checks dramatically
    stats_.collisionHits = std::min(activeBodies / 3, 50); // Estimate based on active bodies

    VKMON_DEBUG("PhysicsSystem: Active bodies: " + std::to_string(stats_.activeRigidBodies) +
                ", Avg velocity: " + std::to_string(stats_.averageVelocity));
}

void PhysicsSystem::createJoltBodiesForNewEntities(EntityManager& entityManager) {
    if (!joltPhysics_) return;

    // Get all entities with both physics components
    const auto& rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();

    int newBodies = 0;

    for (EntityID entity : rigidBodyEntities) {
        // Skip entities that already have Jolt bodies
        if (entityToBodyMap_.find(entity) != entityToBodyMap_.end()) {
            continue;
        }

        // Only create Jolt body if entity has both RigidBody and Collision components
        if (!entityManager.hasComponent<CollisionComponent>(entity) ||
            !entityManager.hasComponent<Transform>(entity)) {
            continue;
        }

        // Get components
        const auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        const auto& collision = entityManager.getComponent<CollisionComponent>(entity);
        const auto& transform = entityManager.getComponent<Transform>(entity);

        // Create Jolt body
        JPH::BodyID bodyID = createJoltBody(entity, rigidBody, collision, transform);

        if (bodyID.IsInvalid()) {
            VKMON_WARNING("PhysicsSystem: Failed to create Jolt body for entity " + std::to_string(entity));
            continue;
        }

        newBodies++;
        VKMON_INFO("PhysicsSystem: Created Jolt body for entity " + std::to_string(entity) +
                   " (Dynamic: " + (rigidBody.isDynamic ? "true" : "false") +
                   ", Shape: " + std::to_string(static_cast<int>(collision.shapeType)) + ")");
    }

    VKMON_DEBUG("PhysicsSystem: Loop completed, created " + std::to_string(newBodies) + " bodies");

    if (newBodies > 0) {
        VKMON_INFO("PhysicsSystem: Created " + std::to_string(newBodies) + " new Jolt physics bodies");
    }

}

void PhysicsSystem::createJoltBodiesForAllEntities(EntityManager& entityManager) {
    VKMON_INFO("PhysicsSystem: Creating Jolt bodies for all entities with physics components");
    createJoltBodiesForNewEntities(entityManager);
}


} // namespace VulkanMon