#include "CharacterControllerSystem.h"
#include "PhysicsSystem.h"
#include "../components/Transform.h"
#include "../components/CharacterControllerComponent.h"
#include "../components/RigidBodyComponent.h"
#include "../utils/Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace VulkanMon {

void CharacterControllerSystem::update(float deltaTime, EntityManager& entityManager) {
    stats_.charactersProcessed = 0;
    stats_.groundChecksPerformed = 0;
    stats_.charactersSkippedDistance = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Get all entities with CharacterControllerComponent and Transform
    auto entities = entityManager.getEntitiesWithComponent<CharacterControllerComponent>();

    static bool loggedOnce = false;
    if (!loggedOnce && !entities.empty()) {
        VKMON_INFO("CharacterControllerSystem: Processing " + std::to_string(entities.size()) + " entities");
        loggedOnce = true;
    }

    for (EntityID entity : entities) {
        if (!entityManager.hasComponent<Transform>(entity)) {
            continue; // Skip entities without Transform
        }

        auto& controller = entityManager.getComponent<CharacterControllerComponent>(entity);
        auto& transform = entityManager.getComponent<Transform>(entity);

        // Performance optimization: skip entities too far away
        if (!shouldProcessEntity(entity, transform)) {
            stats_.charactersSkippedDistance++;
            continue;
        }

        // Process this character controller
        processCharacterController(entity, controller, transform, entityManager, deltaTime);
        stats_.charactersProcessed++;
    }

    // Update statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    stats_.averageProcessingTime = duration.count() / 1000.0f; // Convert to milliseconds
}

void CharacterControllerSystem::processCharacterController(
    EntityID entity,
    CharacterControllerComponent& controller,
    Transform& transform,
    EntityManager& entityManager,
    float deltaTime)
{
    // deltaTime is already in seconds (World::update converts from ms)
    float deltaSeconds = deltaTime;

    // Update timing values
    controller.updateTiming(deltaSeconds);

    // Ground detection
    if (groundDetectionEnabled_) {
        updateGroundDetection(entity, controller, transform);
    }

    // Update movement state
    updateMovementState(controller, deltaSeconds);

    // Handle jumping
    if (controller.wantsToJump && controller.canJump()) {
        handleJumping(controller, deltaSeconds);
        controller.wantsToJump = false; // Consume jump input
    }

    // Apply gravity
    if (controller.useGravity) {
        applyGravity(controller, deltaSeconds);
    }

    // Apply movement based on input
    applyMovement(entity, controller, transform, entityManager, deltaSeconds);

    // Apply physics forces if entity has RigidBody
    if (entityManager.hasComponent<RigidBodyComponent>(entity)) {
        applyPhysicsForces(entity, controller, entityManager);
    }

    // Auto-balance to keep character upright
    if (controller.autoBalance) {
        applyAutoBalance(entity, controller, entityManager);
    }
}

void CharacterControllerSystem::updateGroundDetection(
    EntityID entity,
    CharacterControllerComponent& controller,
    const Transform& transform)
{
    GroundInfo groundInfo = performGroundCheck(transform.position, controller);

    controller.isGrounded = groundInfo.isGrounded;
    controller.groundNormal = groundInfo.groundNormal;
    controller.groundDistance = groundInfo.distance;

    stats_.groundChecksPerformed++;

    // Reset vertical velocity when landing
    if (controller.isGrounded && controller.previousState != CharacterControllerComponent::MovementState::Grounded) {
        if (controller.velocity.y < 0.0f) {
            controller.velocity.y = 0.0f; // Stop falling when we hit ground
        }
    }
}

CharacterControllerSystem::GroundInfo CharacterControllerSystem::performGroundCheck(
    const glm::vec3& position,
    const CharacterControllerComponent& controller)
{
    GroundInfo info;

    // Simple ground check: raycast downward
    // TODO: Integrate with PhysicsSystem for proper raycasting when available

    // For now, use simple Y-axis ground plane matching scene ground at y=-2.0
    const float groundPlaneY = -2.0f;
    float distanceToGround = position.y - groundPlaneY;

    info.distance = distanceToGround;
    info.isGrounded = (distanceToGround <= controller.groundCheckDistance);
    info.groundPoint = glm::vec3(position.x, groundPlaneY, position.z);
    info.groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    info.slopeAngle = 0.0f; // Flat ground

    return info;
}

void CharacterControllerSystem::updateMovementState(
    CharacterControllerComponent& controller,
    float deltaTime)
{
    // Determine movement state based on conditions
    CharacterControllerComponent::MovementState newState = controller.currentState;

    if (controller.isInWater) {
        newState = CharacterControllerComponent::MovementState::Swimming;
    } else if (controller.isOnClimbableSurface) {
        newState = CharacterControllerComponent::MovementState::Climbing;
    } else if (controller.isGrounded) {
        newState = CharacterControllerComponent::MovementState::Grounded;
    } else {
        newState = CharacterControllerComponent::MovementState::Airborne;
    }

    controller.transitionToState(newState);
}

void CharacterControllerSystem::applyMovement(
    EntityID entity,
    CharacterControllerComponent& controller,
    Transform& transform,
    EntityManager& entityManager,
    float deltaTime)
{
    // Calculate desired velocity from input
    glm::vec3 desiredVelocity = calculateDesiredVelocity(controller);

    // Apply acceleration/deceleration
    float accel = (glm::length(controller.inputDirection) > 0.01f)
                  ? controller.acceleration
                  : controller.deceleration;

    controller.velocity = applyAcceleration(
        controller.velocity,
        desiredVelocity,
        accel,
        deltaTime
    );

    // Clamp to max speed
    float maxSpeed = (controller.currentState == CharacterControllerComponent::MovementState::Grounded)
                     ? controller.maxGroundSpeed
                     : controller.maxAirSpeed;

    float currentSpeed = glm::length(controller.velocity);
    if (currentSpeed > maxSpeed) {
        controller.velocity = glm::normalize(controller.velocity) * maxSpeed;
    }

    // Apply velocity to transform
    transform.position += controller.velocity * deltaTime;

    // Keep character above ground (ground plane at Y=-2.0)
    const float groundPlaneY = -2.0f;
    if (controller.isGrounded && transform.position.y < groundPlaneY) {
        transform.position.y = groundPlaneY;
        controller.velocity.y = 0.0f;
    }
}

void CharacterControllerSystem::applyPhysicsForces(
    EntityID entity,
    const CharacterControllerComponent& controller,
    EntityManager& entityManager)
{
    if (!entityManager.hasComponent<RigidBodyComponent>(entity)) {
        return;
    }

    auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);

    // Sync velocity with physics
    rigidBody.velocity = controller.velocity;
}

void CharacterControllerSystem::handleJumping(
    CharacterControllerComponent& controller,
    float deltaTime)
{
    // Apply upward impulse
    controller.velocity.y = controller.jumpForce;
    controller.timeSinceJump = 0.0f;
    controller.isGrounded = false;

    VKMON_INFO("Character jumped with force: " + std::to_string(controller.jumpForce));
}

void CharacterControllerSystem::applyGravity(
    CharacterControllerComponent& controller,
    float deltaTime)
{
    if (controller.isGrounded && controller.velocity.y <= 0.0f) {
        // Don't apply gravity when grounded and not jumping
        controller.velocity.y = 0.0f;
        return;
    }

    // Get gravity vector
    glm::vec3 gravity = controller.getGravityVector();

    // Apply gravity to vertical velocity
    controller.velocity += gravity * deltaTime;

    // Clamp fall speed
    if (controller.velocity.y < -controller.maxFallSpeed) {
        controller.velocity.y = -controller.maxFallSpeed;
    }
}

glm::vec3 CharacterControllerSystem::calculateDesiredVelocity(
    const CharacterControllerComponent& controller)
{
    if (glm::length(controller.inputDirection) < 0.01f) {
        return glm::vec3(0.0f); // No input, stop moving
    }

    // Get normalized input direction
    glm::vec3 moveDir = glm::normalize(controller.inputDirection);

    // Calculate speed based on state
    float speed = controller.getEffectiveSpeed();

    // Calculate horizontal velocity (XZ plane)
    glm::vec3 desiredVelocity = moveDir * speed;

    // Preserve vertical velocity
    desiredVelocity.y = controller.velocity.y;

    return desiredVelocity;
}

glm::vec3 CharacterControllerSystem::applyAcceleration(
    const glm::vec3& currentVelocity,
    const glm::vec3& targetVelocity,
    float acceleration,
    float deltaTime)
{
    // Separate horizontal and vertical components
    glm::vec3 currentHorizontal(currentVelocity.x, 0.0f, currentVelocity.z);
    glm::vec3 targetHorizontal(targetVelocity.x, 0.0f, targetVelocity.z);

    // Interpolate horizontal velocity
    glm::vec3 newHorizontal = glm::mix(
        currentHorizontal,
        targetHorizontal,
        std::min(1.0f, acceleration * deltaTime)
    );

    // Keep vertical velocity from target (gravity/jump is applied separately)
    return glm::vec3(newHorizontal.x, targetVelocity.y, newHorizontal.z);
}

void CharacterControllerSystem::applyAutoBalance(
    EntityID entity,
    const CharacterControllerComponent& controller,
    EntityManager& entityManager)
{
    // Auto-balance keeps character upright
    // This is a simplified version - full implementation would use physics constraints

    auto& transform = entityManager.getComponent<Transform>(entity);

    // Reset rotation to upright (identity or slight tilt based on movement)
    // For now, just ensure character stays upright
    transform.rotation = glm::quat(glm::vec3(0.0f)); // Identity rotation (upright)
}

bool CharacterControllerSystem::shouldProcessEntity(
    EntityID entity,
    const Transform& transform)
{
    // TODO: Implement camera-based culling when camera system is available
    // For now, always process entities within max distance

    // Simple distance check (could be optimized with spatial queries)
    float distanceSquared = glm::dot(transform.position, transform.position);
    float maxDistSquared = maxProcessingDistance_ * maxProcessingDistance_;

    return distanceSquared <= maxDistSquared;
}

} // namespace VulkanMon
