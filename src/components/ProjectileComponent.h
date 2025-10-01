#pragma once

#include "../core/Entity.h"
#include "../utils/Logger.h"
#include <glm/glm.hpp>
#include <chrono>
#include <array>

namespace VulkanMon {

/**
 * ProjectileComponent
 *
 * Generic projectile physics and behavior for VulkanMon engine.
 * Supports bullets, grenades, arrows, magic spells, physics objects, and any projectile-based gameplay.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clean interface with sensible defaults for all projectile types
 * - Engine-Generic: Configurable for FPS bullets, RPG magic, physics puzzles, Pokemon Pokeballs
 * - Performance-aware: Efficient lifetime management and collision response
 */
struct ProjectileComponent {
    VKMON_COMPONENT(ProjectileComponent)

    // =============================================================================
    // PROJECTILE TYPE AND BEHAVIOR
    // =============================================================================

    enum class Type {
        BULLET,          // High-speed projectiles (bullets, lasers)
        GRENADE,         // Arcing projectiles with explosion/area effect
        ARROW,           // Ballistic projectiles with gravity
        MAGIC_SPELL,     // Magical projectiles with special effects
        PHYSICS_OBJECT,  // Generic physics objects (balls, boxes)
        POKEBALL,        // Pokemon capture projectiles
        CUSTOM           // User-defined behavior
    } type = Type::PHYSICS_OBJECT;

    // =============================================================================
    // PHYSICS PROPERTIES
    // =============================================================================

    // Initial velocity and direction
    glm::vec3 initialVelocity{0.0f, 0.0f, -10.0f}; // Launch velocity in m/s
    glm::vec3 currentVelocity{0.0f};                // Current velocity (updated by physics)

    // Physics parameters
    float mass = 0.1f;                              // Mass in kg (affects trajectory)
    float gravityScale = 1.0f;                      // Gravity multiplier (0=no gravity, 1=normal, >1=heavy)
    float dragCoefficient = 0.01f;                  // Air resistance (0=no drag, higher=more drag)
    float bounciness = 0.3f;                        // Collision restitution (0=stick, 1=perfect bounce)

    // =============================================================================
    // LIFETIME MANAGEMENT
    // =============================================================================

    // Time-based lifetime
    float timeToLive = 10.0f;                       // Maximum lifetime in seconds
    float timeSinceSpawn = 0.0f;                    // Time since projectile was created
    bool hasTimeLimit = true;                       // Whether to use time-based cleanup

    // Distance-based lifetime
    float maxDistance = 100.0f;                     // Maximum travel distance
    float distanceTraveled = 0.0f;                  // Distance traveled since spawn
    bool hasDistanceLimit = false;                  // Whether to use distance-based cleanup
    glm::vec3 spawnPosition{0.0f};                  // Original spawn position for distance calc

    // =============================================================================
    // COLLISION RESPONSE
    // =============================================================================

    enum class CollisionResponse {
        DESTROY,         // Projectile is destroyed on impact
        BOUNCE,          // Projectile bounces off surfaces
        STICK,           // Projectile sticks to surfaces
        PASS_THROUGH,    // Projectile passes through (ghost mode)
        CUSTOM_CALLBACK  // Custom collision handling
    } collisionResponse = CollisionResponse::BOUNCE;

    // Collision filtering
    uint32_t collisionMask = 0xFFFFFFFF;            // What layers this projectile can hit
    bool affectedByGravity = true;                  // Whether gravity affects this projectile
    bool trackTrajectory = false;                   // Whether to record trajectory points

    // Impact behavior
    float impactDamage = 10.0f;                     // Damage dealt on impact (if applicable)
    float explosionRadius = 0.0f;                   // Area of effect radius (0=no explosion)
    bool destroyOnAnyCollision = false;             // Destroy on any collision vs specific targets

    // =============================================================================
    // TRAJECTORY TRACKING
    // =============================================================================

    struct TrajectoryPoint {
        glm::vec3 position;
        float timestamp;
    };

    static constexpr size_t MAX_TRAJECTORY_POINTS = 100;
    std::array<TrajectoryPoint, MAX_TRAJECTORY_POINTS> trajectoryPoints{};
    size_t trajectoryPointCount = 0;
    float trajectoryRecordInterval = 0.1f;          // Seconds between trajectory recordings
    float timeSinceLastRecord = 0.0f;

    // =============================================================================
    // PERFORMANCE OPTIMIZATIONS
    // =============================================================================

    // Update frequency control
    bool useHighPrecisionPhysics = false;          // Use full physics vs simplified trajectory
    float simulationTimeStep = 1.0f/60.0f;         // Physics time step for trajectory calculation
    bool enableCollisionDetection = true;          // Whether to check for collisions

    // Culling and optimization
    bool isVisible = true;                          // Whether projectile should be rendered
    float cullDistance = 200.0f;                   // Distance beyond which projectile is culled
    bool enableDistanceCulling = true;              // Whether to use distance-based culling

    // =============================================================================
    // PRESET CONSTRUCTORS FOR COMMON PROJECTILE TYPES
    // =============================================================================

    // Default constructor
    ProjectileComponent() = default;

    // High-speed bullet (FPS games)
    static ProjectileComponent createBullet(float speed = 100.0f, float lifetime = 5.0f) {
        ProjectileComponent projectile;
        projectile.type = Type::BULLET;
        projectile.initialVelocity = {0.0f, 0.0f, -speed};
        projectile.timeToLive = lifetime;
        projectile.mass = 0.01f;                    // Very light
        projectile.gravityScale = 0.1f;             // Minimal gravity effect
        projectile.dragCoefficient = 0.001f;        // Minimal air resistance
        projectile.collisionResponse = CollisionResponse::DESTROY;
        projectile.destroyOnAnyCollision = true;
        projectile.useHighPrecisionPhysics = true;
        return projectile;
    }

    // Ballistic arrow/grenade (RPG games)
    static ProjectileComponent createBallisticProjectile(float speed = 20.0f, float arc = 1.0f) {
        ProjectileComponent projectile;
        projectile.type = Type::ARROW;
        projectile.initialVelocity = {0.0f, arc * 5.0f, -speed};
        projectile.timeToLive = 15.0f;
        projectile.mass = 0.1f;
        projectile.gravityScale = 1.0f;             // Full gravity
        projectile.dragCoefficient = 0.02f;         // Some air resistance
        projectile.collisionResponse = CollisionResponse::STICK;
        projectile.trackTrajectory = true;
        return projectile;
    }

    // Physics object (puzzle games, general physics)
    static ProjectileComponent createPhysicsObject(float mass = 1.0f, float bounce = 0.6f) {
        ProjectileComponent projectile = {};  // CRITICAL FIX: Zero-initialize to avoid garbage data
        projectile.type = Type::PHYSICS_OBJECT;
        projectile.initialVelocity = {0.0f, 2.0f, -5.0f};
        projectile.timeToLive = 30.0f;
        projectile.mass = mass;
        projectile.gravityScale = 1.0f;
        projectile.dragCoefficient = 0.01f;
        projectile.bounciness = bounce;
        projectile.collisionResponse = CollisionResponse::BOUNCE;
        projectile.hasDistanceLimit = false;
        projectile.timeSinceSpawn = 0.0f;  // Explicitly ensure timer starts at zero
        projectile.distanceTraveled = 0.0f;
        return projectile;
    }

    // Magic spell (fantasy games)
    static ProjectileComponent createMagicSpell(float speed = 15.0f, float lifetime = 8.0f) {
        ProjectileComponent projectile;
        projectile.type = Type::MAGIC_SPELL;
        projectile.initialVelocity = {0.0f, 0.0f, -speed};
        projectile.timeToLive = lifetime;
        projectile.mass = 0.05f;                    // Very light
        projectile.gravityScale = 0.0f;             // No gravity (magical)
        projectile.dragCoefficient = 0.0f;          // No air resistance
        projectile.collisionResponse = CollisionResponse::DESTROY;
        projectile.trackTrajectory = true;
        projectile.explosionRadius = 2.0f;          // Area effect
        return projectile;
    }

    // Pokeball (Pokemon-style games)
    static ProjectileComponent createPokeball(float throwForce = 12.0f) {
        ProjectileComponent projectile;
        projectile.type = Type::POKEBALL;
        projectile.initialVelocity = {0.0f, 3.0f, -throwForce};
        projectile.timeToLive = 20.0f;
        projectile.mass = 0.2f;                     // Pokeball weight
        projectile.gravityScale = 1.0f;
        projectile.dragCoefficient = 0.015f;
        projectile.bounciness = 0.4f;
        projectile.collisionResponse = CollisionResponse::BOUNCE;
        projectile.trackTrajectory = true;
        projectile.destroyOnAnyCollision = false;   // Bounces multiple times
        return projectile;
    }

    // =============================================================================
    // LIFECYCLE MANAGEMENT
    // =============================================================================

    // Initialize projectile at spawn time
    void initialize(const glm::vec3& position, const glm::vec3& direction, float speed = -1.0f) {
        spawnPosition = position;
        timeSinceSpawn = 0.0f;
        distanceTraveled = 0.0f;
        trajectoryPointCount = 0;
        timeSinceLastRecord = 0.0f;

        // Set initial velocity based on direction and speed
        if (speed > 0.0f) {
            currentVelocity = glm::normalize(direction) * speed;
            initialVelocity = currentVelocity;
        } else {
            currentVelocity = initialVelocity;
        }

        // Record initial trajectory point
        if (trackTrajectory) {
            recordTrajectoryPoint(position, 0.0f);
        }
    }

    // Update projectile state each frame
    void update(float deltaTime, const glm::vec3& currentPosition) {
        timeSinceSpawn += deltaTime;
        timeSinceLastRecord += deltaTime;

        // Update distance traveled
        if (trajectoryPointCount > 0) {
            glm::vec3 movement = currentPosition - trajectoryPoints[trajectoryPointCount - 1].position;
            distanceTraveled += glm::length(movement);
        }

        // Record trajectory point if needed
        if (trackTrajectory && timeSinceLastRecord >= trajectoryRecordInterval) {
            recordTrajectoryPoint(currentPosition, timeSinceSpawn);
            timeSinceLastRecord = 0.0f;
        }
    }

    // Check if projectile should be destroyed
    bool shouldDestroy() const {
        // Time-based destruction
        if (hasTimeLimit && timeSinceSpawn >= timeToLive) {
            return true;
        }

        // Distance-based destruction
        if (hasDistanceLimit && distanceTraveled >= maxDistance) {
            return true;
        }

        return false;
    }

    // =============================================================================
    // TRAJECTORY AND PHYSICS UTILITIES
    // =============================================================================

    // Record a trajectory point
    void recordTrajectoryPoint(const glm::vec3& position, float timestamp) {
        if (trajectoryPointCount < MAX_TRAJECTORY_POINTS) {
            trajectoryPoints[trajectoryPointCount] = {position, timestamp};
            trajectoryPointCount++;
        }
    }

    // Get predicted impact point using ballistic trajectory
    glm::vec3 predictImpactPoint(const glm::vec3& startPos, float groundHeight = 0.0f) const {
        if (!affectedByGravity || gravityScale <= 0.0f) {
            // Linear trajectory for non-gravity projectiles
            float timeToGround = (startPos.y - groundHeight) / -currentVelocity.y;
            if (timeToGround <= 0.0f) timeToGround = timeToLive;

            return startPos + currentVelocity * timeToGround;
        }

        // Ballistic trajectory calculation
        float gravity = 9.81f * gravityScale;
        float vx = currentVelocity.x;
        float vy = currentVelocity.y;
        float vz = currentVelocity.z;

        // Solve quadratic equation for time to hit ground
        float a = -0.5f * gravity;
        float b = vy;
        float c = startPos.y - groundHeight;

        float discriminant = b * b - 4 * a * c;
        if (discriminant < 0.0f) {
            // No ground intersection, use max time
            float t = timeToLive;
            return {startPos.x + vx * t, groundHeight, startPos.z + vz * t};
        }

        float t = (-b - sqrt(discriminant)) / (2 * a);
        if (t < 0.0f) t = (-b + sqrt(discriminant)) / (2 * a);

        return {
            startPos.x + vx * t,
            groundHeight,
            startPos.z + vz * t
        };
    }

    // Apply drag force to velocity
    void applyDrag(float deltaTime) {
        if (dragCoefficient > 0.0f) {
            float speed = glm::length(currentVelocity);
            if (speed > 0.0f) {
                glm::vec3 dragForce = -currentVelocity * dragCoefficient * speed;
                currentVelocity += dragForce * deltaTime / mass;
            }
        }
    }

    // Get current trajectory info for debugging
    struct TrajectoryInfo {
        float timeInFlight;
        float distanceTraveled;
        float currentSpeed;
        glm::vec3 currentDirection;
        glm::vec3 predictedLanding;
    };

    TrajectoryInfo getTrajectoryInfo(const glm::vec3& currentPosition) const {
        TrajectoryInfo info;
        info.timeInFlight = timeSinceSpawn;
        info.distanceTraveled = distanceTraveled;
        info.currentSpeed = glm::length(currentVelocity);
        info.currentDirection = glm::length(currentVelocity) > 0.0f ?
                               glm::normalize(currentVelocity) : glm::vec3(0.0f);
        info.predictedLanding = predictImpactPoint(currentPosition);
        return info;
    }

    // =============================================================================
    // COLLISION UTILITIES
    // =============================================================================

    // Handle collision with surface
    void handleCollision(const glm::vec3& normal, float impactSpeed) {
        switch (collisionResponse) {
            case CollisionResponse::DESTROY:
                // Projectile will be destroyed by the system
                break;

            case CollisionResponse::BOUNCE:
                // Reflect velocity off surface
                currentVelocity = glm::reflect(currentVelocity, normal) * bounciness;
                break;

            case CollisionResponse::STICK:
                // Stop all movement
                currentVelocity = glm::vec3(0.0f);
                break;

            case CollisionResponse::PASS_THROUGH:
                // Continue unchanged
                break;

            case CollisionResponse::CUSTOM_CALLBACK:
                // Custom handling will be done by the system
                break;
        }
    }

    // Check if this projectile should collide with a specific layer
    bool shouldCollideWith(uint32_t targetLayer) const {
        return (collisionMask & targetLayer) != 0;
    }

    // Set collision layers this projectile can hit
    void setCollisionMask(uint32_t mask) {
        collisionMask = mask;
    }

    // Add a layer to collision mask
    void addCollisionLayer(uint32_t layer) {
        collisionMask |= layer;
    }

    // Remove a layer from collision mask
    void removeCollisionLayer(uint32_t layer) {
        collisionMask &= ~layer;
    }
};

} // namespace VulkanMon