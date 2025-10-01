#pragma once

#include "../core/Entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VulkanMon {

/**
 * CreaturePhysicsComponent
 *
 * Pokemon-specific physics behaviors and movement constraints.
 * Handles ground detection, species-specific movement, and creature balance.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clear movement parameters with Pokemon-focused defaults
 * - Pokemon-specific: Flying, swimming, climbing capabilities per species
 * - Performance-aware: Efficient ground detection and movement state tracking
 */
struct CreaturePhysicsComponent {
    VKMON_COMPONENT(CreaturePhysicsComponent)

    // =============================================================================
    // GROUND DETECTION AND MOVEMENT
    // =============================================================================

    // Ground contact detection
    float groundSnapDistance = 0.5f;  // How far to check for ground below creature
    float groundCheckRadius = 0.3f;   // Radius for ground detection (wider = more stable)
    bool isGrounded = false;          // Currently touching walkable ground
    glm::vec3 groundNormal{0.0f, 1.0f, 0.0f}; // Surface normal of ground we're on

    // Slope and terrain constraints
    float slopeLimit = 45.0f;         // Maximum walkable slope in degrees
    float stepHeight = 0.3f;          // Maximum step height to auto-climb

    // =============================================================================
    // CREATURE MOVEMENT PARAMETERS
    // =============================================================================

    // Base movement speeds (meters per second)
    float moveSpeed = 5.0f;           // Normal walking/running speed
    float sprintMultiplier = 2.0f;    // Speed multiplier when sprinting
    float jumpForce = 8.0f;           // Upward force for jumping (Newtons)
    float airControl = 0.3f;          // How much control while airborne (0-1)

    // Movement constraints
    float maxWalkSpeed = 10.0f;       // Speed limit for ground movement
    float maxFallSpeed = 20.0f;       // Terminal velocity
    float maxSwimSpeed = 3.0f;        // Speed limit for swimming

    // =============================================================================
    // POKEMON-SPECIFIC MOVEMENT TYPES
    // =============================================================================

    // Species capabilities
    bool canFly = false;              // Can move in 3D space freely (birds, dragons)
    bool canSwim = false;             // Can move through water efficiently (water types)
    bool canClimb = false;            // Can walk on walls/ceilings (some bug types)
    bool canPhaseThrough = false;     // Can pass through solid objects (ghost types)

    // Flying parameters
    float flySpeed = 8.0f;            // Flying movement speed
    float hoverHeight = 2.0f;         // Preferred height above ground when flying
    bool preferGroundMovement = true; // Prefer walking over flying when possible

    // Swimming parameters
    float swimDepth = 1.0f;           // Preferred depth below water surface
    float buoyancy = 0.8f;           // How much water supports creature (0-1)

    // =============================================================================
    // MOVEMENT STATE TRACKING
    // =============================================================================

    // Current movement input and state
    glm::vec3 inputDirection{0.0f};   // Desired movement direction (normalized)
    bool wantsToJump = false;         // Jump input received this frame
    bool wantsToSprint = false;       // Sprint input active

    // Environmental state
    bool isSwimming = false;          // Currently in water
    bool isFlying = false;            // Currently flying/hovering
    bool isClimbing = false;          // Currently on wall/ceiling
    bool isInTallGrass = false;       // Currently in tall grass

    // Movement timing
    float timeSinceGrounded = 0.0f;   // Time since last ground contact
    float timeSinceJump = 0.0f;       // Time since last jump (prevents spam)
    float jumpCooldown = 0.5f;        // Minimum time between jumps

    // =============================================================================
    // BALANCE AND STABILITY
    // =============================================================================

    // Creature stability system
    float stabilizingForce = 50.0f;   // Force to keep creatures upright
    bool autoBalance = true;          // Automatically prevent tipping over
    float balanceTolerance = 30.0f;   // Maximum tilt angle before correction (degrees)

    // Center of mass and stability
    glm::vec3 centerOfMass{0.0f, 0.5f, 0.0f}; // Relative to transform position
    float stabilityRadius = 0.8f;     // Base of support for balance calculations

    // =============================================================================
    // CONSTRUCTORS FOR DIFFERENT POKEMON TYPES
    // =============================================================================

    // Default constructor - balanced land creature
    CreaturePhysicsComponent() = default;

    // Land-based Pokemon (most common)
    static CreaturePhysicsComponent createLandCreature(float speed = 5.0f, float jumpHeight = 1.5f) {
        CreaturePhysicsComponent creature;
        creature.moveSpeed = speed;
        creature.jumpForce = jumpHeight * 4.0f; // Approximate force for desired height
        creature.canFly = false;
        creature.canSwim = false;
        creature.preferGroundMovement = true;
        return creature;
    }

    // Flying Pokemon (birds, dragons, levitating types)
    static CreaturePhysicsComponent createFlyingCreature(float flySpeed = 8.0f, float groundSpeed = 4.0f) {
        CreaturePhysicsComponent creature;
        creature.moveSpeed = groundSpeed;
        creature.flySpeed = flySpeed;
        creature.canFly = true;
        creature.hoverHeight = 2.0f;
        creature.preferGroundMovement = false; // Prefer flying
        creature.airControl = 0.9f; // Excellent air control
        return creature;
    }

    // Water Pokemon (fish, aquatic types)
    static CreaturePhysicsComponent createWaterCreature(float swimSpeed = 6.0f, float landSpeed = 2.0f) {
        CreaturePhysicsComponent creature;
        creature.moveSpeed = landSpeed; // Slow on land
        creature.maxSwimSpeed = swimSpeed;
        creature.canSwim = true;
        creature.buoyancy = 0.9f; // Very buoyant
        creature.swimDepth = 1.5f;
        return creature;
    }

    // Fast, agile Pokemon (electric, normal types)
    static CreaturePhysicsComponent createAgileCreature(float speed = 8.0f) {
        CreaturePhysicsComponent creature;
        creature.moveSpeed = speed;
        creature.sprintMultiplier = 2.5f; // Very fast sprint
        creature.jumpForce = 10.0f; // High jumping
        creature.airControl = 0.7f; // Good air control
        creature.slopeLimit = 60.0f; // Can climb steep slopes
        return creature;
    }

    // Heavy, slow Pokemon (rock, steel types)
    static CreaturePhysicsComponent createHeavyCreature(float speed = 3.0f) {
        CreaturePhysicsComponent creature;
        creature.moveSpeed = speed;
        creature.sprintMultiplier = 1.5f; // Limited sprint
        creature.jumpForce = 4.0f; // Low jumping
        creature.stabilizingForce = 100.0f; // Very stable
        creature.slopeLimit = 30.0f; // Limited climbing
        return creature;
    }

    // =============================================================================
    // MOVEMENT STATE MANAGEMENT
    // =============================================================================

    // Set movement input (called by AI or player controller)
    void setMovementInput(const glm::vec3& direction, bool sprint = false, bool jump = false) {
        inputDirection = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(0.0f);
        wantsToSprint = sprint;

        // Handle jump with cooldown
        if (jump && canJump()) {
            wantsToJump = true;
            timeSinceJump = 0.0f;
        }
    }

    // Check if creature can jump right now
    bool canJump() const {
        return isGrounded && timeSinceJump >= jumpCooldown;
    }

    // Update movement state (called every frame)
    void updateMovementState(float deltaTime) {
        timeSinceJump += deltaTime;

        if (!isGrounded) {
            timeSinceGrounded += deltaTime;
        } else {
            timeSinceGrounded = 0.0f;
        }
    }

    // Calculate effective movement speed based on state
    float getEffectiveSpeed() const {
        float speed = moveSpeed;

        // Apply sprint multiplier
        if (wantsToSprint && isGrounded) {
            speed *= sprintMultiplier;
        }

        // Modify based on environment
        if (isSwimming) {
            speed = maxSwimSpeed;
        } else if (isFlying) {
            speed = flySpeed;
        } else if (isInTallGrass) {
            speed *= 0.7f; // Slower in tall grass
        }

        // Apply air control if airborne (but not when swimming or flying)
        if (!isGrounded && !isFlying && !isSwimming) {
            speed *= airControl;
        }

        return speed;
    }

    // Get jump velocity for physics system
    glm::vec3 getJumpVelocity() const {
        if (wantsToJump && canJump()) {
            return glm::vec3(0.0f, jumpForce, 0.0f);
        }
        return glm::vec3(0.0f);
    }

    // =============================================================================
    // ENVIRONMENTAL INTERACTION
    // =============================================================================

    // Enter/exit water
    void enterWater() {
        if (canSwim) {
            isSwimming = true;
        }
    }

    void exitWater() {
        isSwimming = false;
    }

    // Start/stop flying
    void startFlying() {
        if (canFly) {
            isFlying = true;
            isGrounded = false;
        }
    }

    void stopFlying() {
        isFlying = false;
    }

    // Enter/exit tall grass
    void enterTallGrass() {
        isInTallGrass = true;
    }

    void exitTallGrass() {
        isInTallGrass = false;
    }

    // =============================================================================
    // PHYSICS UTILITIES
    // =============================================================================

    // Calculate if slope is walkable
    bool isSlopeWalkable(const glm::vec3& surfaceNormal) const {
        float slope = glm::degrees(glm::acos(glm::dot(surfaceNormal, glm::vec3(0, 1, 0))));
        return slope <= slopeLimit;
    }

    // Get movement force to apply this frame
    glm::vec3 getMovementForce(float mass) const {
        if (glm::length(inputDirection) < 0.001f) {
            return glm::vec3(0.0f);
        }

        float force = getEffectiveSpeed() * mass;
        return inputDirection * force;
    }

    // Clear single-frame inputs (called after physics update)
    void clearFrameInputs() {
        wantsToJump = false;
    }

    // Get center of mass in world space
    glm::vec3 getWorldCenterOfMass(const glm::vec3& entityPosition) const {
        return entityPosition + centerOfMass;
    }

    // Check if creature is stable (not tipping over)
    bool isStable(const glm::quat& entityRotation) const {
        if (!autoBalance) return true;

        // Calculate tilt from upright
        glm::vec3 up = entityRotation * glm::vec3(0, 1, 0);
        float tiltAngle = glm::degrees(glm::acos(glm::dot(up, glm::vec3(0, 1, 0))));

        return tiltAngle <= balanceTolerance;
    }
};

} // namespace VulkanMon