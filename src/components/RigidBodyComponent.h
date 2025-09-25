#pragma once

#include "../core/Entity.h"
#include <glm/glm.hpp>

namespace VulkanMon {

/**
 * RigidBodyComponent
 *
 * Core physics properties for entities that move and respond to forces.
 * Integrates with Jolt Physics engine for realistic Pokemon gameplay.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clean interface with sensible defaults
 * - Pokemon-focused: Mass values and damping tuned for creature gameplay
 * - Performance-aware: Minimal memory footprint, cache-friendly layout
 */
struct RigidBodyComponent {
    VKMON_COMPONENT(RigidBodyComponent)

    // =============================================================================
    // PHYSICS PROPERTIES
    // =============================================================================

    // Core physics behavior
    bool isDynamic = true;           // Can move vs static (buildings, terrain)
    float mass = 1.0f;               // Mass in kg (affects acceleration, Pokemon weight)
    float restitution = 0.3f;        // Bounce factor (0=no bounce, 1=perfect bounce)
    float friction = 0.7f;           // Surface friction (0=ice, 1=sandpaper)

    // =============================================================================
    // CURRENT PHYSICS STATE
    // =============================================================================

    // Linear motion
    glm::vec3 velocity{0.0f};        // Linear velocity in m/s
    glm::vec3 force{0.0f};           // Accumulated forces for this frame

    // Angular motion
    glm::vec3 angularVelocity{0.0f}; // Angular velocity in rad/s
    glm::vec3 torque{0.0f};          // Accumulated torques for this frame

    // =============================================================================
    // JOLT PHYSICS INTEGRATION
    // =============================================================================

    uint32_t bodyID = 0;             // Jolt physics body identifier (0 = invalid)

    // =============================================================================
    // PHYSICS BEHAVIOR FLAGS
    // =============================================================================

    bool useGravity = true;          // Affected by gravity (disable for flying Pokemon)
    bool freezeRotation = false;     // Lock rotation (useful for upright creatures)
    bool isKinematic = false;        // Moved programmatically, not by forces

    // =============================================================================
    // PHYSICS MATERIAL PROPERTIES
    // =============================================================================

    float linearDamping = 0.1f;      // Air resistance for movement (0=no damping, 1=stops quickly)
    float angularDamping = 0.1f;     // Air resistance for rotation

    // =============================================================================
    // CONSTRUCTORS AND UTILITIES
    // =============================================================================

    // Default constructor - good for most Pokemon
    RigidBodyComponent() = default;

    // Pokemon-specific constructor
    RigidBodyComponent(float pokemonMass, bool canRotate = false)
        : mass(pokemonMass), freezeRotation(!canRotate) {
        // Pokemon-specific defaults
        restitution = 0.1f;          // Pokemon don't bounce much
        friction = 0.8f;             // Good grip on ground
        linearDamping = 0.3f;        // Creatures slow down naturally
    }

    // =============================================================================
    // ENGINE-FOCUSED FACTORY METHODS
    // =============================================================================

    // Dynamic physics object (affected by forces, gravity, collisions)
    static RigidBodyComponent dynamic(float mass = 1.0f) {
        RigidBodyComponent rb;
        rb.isDynamic = true;
        rb.mass = mass;
        rb.useGravity = true;
        rb.restitution = 0.3f;       // Moderate bounce
        rb.friction = 0.7f;          // Good surface grip
        rb.linearDamping = 0.1f;     // Natural air resistance
        rb.angularDamping = 0.05f;   // Natural rotation damping
        return rb;
    }

    // Kinematic object (moved programmatically, not affected by forces)
    static RigidBodyComponent kinematic() {
        RigidBodyComponent rb;
        rb.isDynamic = true;         // Can move
        rb.isKinematic = true;       // But controlled programmatically
        rb.mass = 1.0f;
        rb.useGravity = false;       // Not affected by physics forces
        rb.restitution = 0.0f;       // No bouncing
        rb.friction = 0.0f;          // Slides smoothly
        return rb;
    }

    // Static object (immovable environment like terrain, buildings)
    static RigidBodyComponent staticBody() {
        RigidBodyComponent rb;
        rb.isDynamic = false;        // Cannot move
        rb.mass = 0.0f;              // Infinite mass
        rb.useGravity = false;       // Not affected by gravity
        rb.restitution = 0.5f;       // Objects bounce off it
        rb.friction = 0.8f;          // High friction for realistic contact
        return rb;
    }

    // =============================================================================
    // GAME-SPECIFIC FACTORY METHODS (for reference/convenience)
    // =============================================================================

    // Pokeball constructor
    static RigidBodyComponent createPokeball() {
        RigidBodyComponent pokeball;
        pokeball.mass = 0.2f;        // Light sphere
        pokeball.restitution = 0.8f; // Very bouncy
        pokeball.friction = 0.3f;    // Low friction for rolling
        pokeball.linearDamping = 0.1f; // Rolls smoothly
        return pokeball;
    }

    // Static object constructor (terrain, buildings) - DEPRECATED: Use staticBody()
    static RigidBodyComponent createStatic() {
        return staticBody();
    }

    // =============================================================================
    // PHYSICS STATE MANAGEMENT
    // =============================================================================

    // Apply instantaneous force (for jumping, impacts)
    void applyImpulse(const glm::vec3& impulse) {
        if (isDynamic && mass > 0.0f) {
            velocity += impulse / mass;
        }
    }

    // Apply continuous force (for movement, wind)
    void applyForce(const glm::vec3& newForce) {
        force += newForce;
    }

    // Apply torque for rotation
    void applyTorque(const glm::vec3& newTorque) {
        if (!freezeRotation) {
            torque += newTorque;
        }
    }

    // Clear accumulated forces (called after physics step)
    void clearForces() {
        force = glm::vec3(0.0f);
        torque = glm::vec3(0.0f);
    }

    // Check if body has valid Jolt physics representation
    bool hasPhysicsBody() const {
        return bodyID != 0;
    }


    // =============================================================================
    // POKEMON GAMEPLAY UTILITIES
    // =============================================================================

    // Get kinetic energy (useful for impact damage calculations)
    float getKineticEnergy() const {
        float speed = glm::length(velocity);
        return 0.5f * mass * speed * speed;
    }

    // Check if moving fast enough for certain interactions
    bool isMovingFast(float threshold = 5.0f) const {
        return glm::length(velocity) > threshold;
    }

    // Get momentum for collision calculations
    glm::vec3 getMomentum() const {
        return velocity * mass;
    }

    // Set velocity with automatic mass scaling
    void setVelocity(const glm::vec3& newVelocity) {
        velocity = newVelocity;
    }

    // Add velocity (useful for conveyor belts, moving platforms)
    void addVelocity(const glm::vec3& deltaVelocity) {
        velocity += deltaVelocity;
    }

    // Instantly stop all movement
    void stop() {
        velocity = glm::vec3(0.0f);
        angularVelocity = glm::vec3(0.0f);
        clearForces();
    }
};

} // namespace VulkanMon