#pragma once

#include "../core/Entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VulkanMon {

/**
 * CharacterControllerComponent
 *
 * Generic character movement system for any game type.
 * Handles ground detection, movement states, and physics parameters
 * that work for players, NPCs, creatures, vehicles, etc.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clear movement parameters with sensible defaults
 * - Game-Agnostic: Works for Pokemon, platformers, racing games, space games
 * - Performance-aware: Efficient ground detection and state management
 */
struct CharacterControllerComponent {
    VKMON_COMPONENT(CharacterControllerComponent)

    // =============================================================================
    // MOVEMENT PARAMETERS
    // =============================================================================

    // Base movement speeds (units per second)
    float moveSpeed = 5.0f;           // Normal movement speed
    float sprintMultiplier = 2.0f;    // Speed multiplier when sprinting
    float acceleration = 20.0f;       // How quickly we reach target speed
    float deceleration = 25.0f;       // How quickly we stop moving

    // Jumping and falling
    float jumpForce = 8.0f;           // Upward force for jumping
    float maxFallSpeed = 20.0f;       // Terminal velocity
    float airControl = 0.3f;          // Movement control while airborne (0-1)

    // Movement constraints
    float maxGroundSpeed = 15.0f;     // Maximum movement speed on ground
    float maxAirSpeed = 10.0f;        // Maximum movement speed in air

    // =============================================================================
    // GROUND DETECTION
    // =============================================================================

    // Ground detection parameters
    float groundCheckDistance = 0.6f; // How far to raycast downward for ground
    float groundCheckRadius = 0.3f;   // Radius for ground detection capsule
    float slopeLimit = 45.0f;         // Maximum walkable slope in degrees
    float stepHeight = 0.4f;          // Maximum step height to auto-climb

    // Ground state
    bool isGrounded = false;          // Currently touching walkable ground
    glm::vec3 groundNormal{0.0f, 1.0f, 0.0f}; // Surface normal of current ground
    float groundDistance = 0.0f;      // Distance to ground below

    // =============================================================================
    // MOVEMENT STATE
    // =============================================================================

    enum class MovementState {
        Grounded,    // Standing/walking on solid ground
        Airborne,    // In the air (jumping/falling)
        Swimming,    // In water volume
        Climbing,    // On climbable surface (walls/ceilings)
        Custom       // Game-specific state
    };

    MovementState currentState = MovementState::Grounded;
    MovementState previousState = MovementState::Grounded;

    // Current movement input and velocity
    glm::vec3 inputDirection{0.0f};   // Desired movement direction (normalized)
    glm::vec3 velocity{0.0f};         // Current velocity vector
    bool wantsToJump = false;         // Jump input received this frame
    bool wantsToSprint = false;       // Sprint input active

    // =============================================================================
    // ENVIRONMENTAL INTERACTION
    // =============================================================================

    // Gravity and physics
    bool useGravity = true;           // Affected by gravity
    float gravityMultiplier = 1.0f;   // Gravity strength modifier
    glm::vec3 customGravity{0.0f};    // Custom gravity vector (overrides world gravity if non-zero)

    // Environmental state flags
    bool isInWater = false;           // Currently in water volume
    bool isOnClimbableSurface = false; // Currently touching climbable surface
    float waterMovementMultiplier = 0.5f; // Speed multiplier when in water
    float landMovementMultiplier = 1.0f;  // Speed multiplier on land

    // =============================================================================
    // TIMING AND COOLDOWNS
    // =============================================================================

    // Jump timing
    float timeSinceGrounded = 0.0f;   // Time since last ground contact
    float timeSinceJump = 0.0f;       // Time since last jump
    float jumpCooldown = 0.2f;        // Minimum time between jumps
    float coyoteTime = 0.15f;         // Grace period for jumping after leaving ground

    // State timing
    float timeInCurrentState = 0.0f;  // How long in current movement state
    float minStateTime = 0.1f;        // Minimum time before state changes (prevents flickering)

    // =============================================================================
    // BALANCE AND STABILITY
    // =============================================================================

    // Auto-balance system (prevents characters from tipping over)
    bool autoBalance = true;          // Automatically keep character upright
    float stabilizingForce = 50.0f;   // Force to keep character upright
    float balanceTolerance = 30.0f;   // Maximum tilt angle before correction (degrees)

    // =============================================================================
    // FACTORY METHODS (Common Character Types)
    // =============================================================================

    // Standard humanoid character (players, NPCs)
    static CharacterControllerComponent createHumanoid(float speed = 5.0f) {
        CharacterControllerComponent controller;
        controller.moveSpeed = speed;
        controller.jumpForce = 8.0f;
        controller.acceleration = 20.0f;
        controller.deceleration = 25.0f;
        controller.airControl = 0.3f;
        controller.slopeLimit = 45.0f;
        return controller;
    }

    // Fast, agile character (speedsters, scouts)
    static CharacterControllerComponent createAgile(float speed = 8.0f) {
        CharacterControllerComponent controller;
        controller.moveSpeed = speed;
        controller.sprintMultiplier = 2.5f;
        controller.jumpForce = 12.0f;
        controller.acceleration = 30.0f;
        controller.deceleration = 20.0f;
        controller.airControl = 0.7f;
        controller.slopeLimit = 60.0f;
        return controller;
    }

    // Heavy, stable character (tanks, large creatures)
    static CharacterControllerComponent createHeavy(float speed = 3.0f) {
        CharacterControllerComponent controller;
        controller.moveSpeed = speed;
        controller.sprintMultiplier = 1.3f;
        controller.jumpForce = 5.0f;
        controller.acceleration = 10.0f;
        controller.deceleration = 15.0f;
        controller.airControl = 0.1f;
        controller.stabilizingForce = 100.0f;
        controller.slopeLimit = 30.0f;
        return controller;
    }

    // Flying character (birds, drones, aircraft)
    static CharacterControllerComponent createFlying(float speed = 7.0f) {
        CharacterControllerComponent controller;
        controller.moveSpeed = speed;
        controller.useGravity = false;
        controller.currentState = MovementState::Airborne;
        controller.airControl = 0.9f;
        controller.acceleration = 15.0f;
        controller.deceleration = 15.0f;
        controller.autoBalance = false; // Flying objects manage their own orientation
        return controller;
    }

    // Swimming character (fish, submarines, water vehicles)
    static CharacterControllerComponent createSwimming(float speed = 4.0f) {
        CharacterControllerComponent controller;
        controller.moveSpeed = speed;
        controller.waterMovementMultiplier = 1.5f;  // Faster in water
        controller.landMovementMultiplier = 0.3f;   // Slow on land
        controller.useGravity = false;  // Buoyancy handles vertical movement
        controller.airControl = 0.8f;   // Good 3D control in water
        return controller;
    }

    // Ground vehicle (cars, tanks, mechs)
    static CharacterControllerComponent createVehicle(float speed = 12.0f) {
        CharacterControllerComponent controller;
        controller.moveSpeed = speed;
        controller.sprintMultiplier = 1.8f;
        controller.jumpForce = 0.0f;    // Vehicles don't jump
        controller.acceleration = 8.0f;
        controller.deceleration = 12.0f;
        controller.airControl = 0.0f;   // No air control
        controller.stabilizingForce = 200.0f; // Very stable
        controller.slopeLimit = 25.0f;  // Limited climbing ability
        return controller;
    }

    // =============================================================================
    // UTILITY METHODS
    // =============================================================================

    // Get effective movement speed based on current conditions
    float getEffectiveSpeed() const {
        float baseSpeed = moveSpeed * (wantsToSprint ? sprintMultiplier : 1.0f);

        if (isInWater) {
            baseSpeed *= waterMovementMultiplier;
        } else {
            baseSpeed *= landMovementMultiplier;
        }

        // Apply state-based limits
        switch (currentState) {
            case MovementState::Grounded:
                return std::min(baseSpeed, maxGroundSpeed);
            case MovementState::Airborne:
                return std::min(baseSpeed * airControl, maxAirSpeed);
            case MovementState::Swimming:
                return baseSpeed; // Already modified by water multiplier
            default:
                return baseSpeed;
        }
    }

    // Check if character can jump based on current conditions
    bool canJump() const {
        return (isGrounded || timeSinceGrounded < coyoteTime) &&
               timeSinceJump >= jumpCooldown &&
               jumpForce > 0.0f;
    }

    // Get current gravity vector
    glm::vec3 getGravityVector() const {
        if (!useGravity) return glm::vec3{0.0f};

        if (glm::length(customGravity) > 0.0f) {
            return customGravity * gravityMultiplier;
        }

        // Use world gravity (will be provided by PhysicsSystem)
        return glm::vec3{0.0f, -9.81f, 0.0f} * gravityMultiplier;
    }

    // Check if slope is walkable
    bool isSlopeWalkable(float slopeAngle) const {
        return slopeAngle <= slopeLimit;
    }

    // Update timing values (called by CharacterControllerSystem)
    void updateTiming(float deltaTime) {
        timeSinceGrounded += deltaTime;
        timeSinceJump += deltaTime;
        timeInCurrentState += deltaTime;

        if (isGrounded) {
            timeSinceGrounded = 0.0f;
        }
    }

    // Transition to new movement state
    void transitionToState(MovementState newState) {
        if (newState != currentState && timeInCurrentState >= minStateTime) {
            previousState = currentState;
            currentState = newState;
            timeInCurrentState = 0.0f;
        }
    }
};

} // namespace VulkanMon