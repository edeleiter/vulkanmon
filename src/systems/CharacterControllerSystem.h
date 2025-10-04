#pragma once

#include "../core/System.h"
#include "../core/EntityManager.h"
#include "../components/CharacterControllerComponent.h"
#include "../components/Transform.h"
#include "../components/RigidBodyComponent.h"
#include <memory>

namespace VulkanMon {

// Forward declarations
class PhysicsSystem;

/**
 * CharacterControllerSystem
 *
 * Processes CharacterControllerComponent to provide generic character movement
 * for any game type. Integrates with PhysicsSystem for ground detection and
 * physics-based movement.
 *
 * Key Features:
 * - Ground detection using raycasting
 * - Movement state management (grounded, airborne, swimming, etc.)
 * - Physics integration without coupling to game-specific logic
 * - Performance optimization for large numbers of characters
 */
class CharacterControllerSystem : public SystemBase {
public:
    CharacterControllerSystem() = default;
    ~CharacterControllerSystem() = default;

    // System interface
    void update(float deltaTime, EntityManager& entityManager) override;

    // Physics system integration
    void setPhysicsSystem(std::shared_ptr<PhysicsSystem> physicsSystem) {
        physicsSystem_ = physicsSystem;
    }

    // Ground detection configuration
    void setGroundDetectionEnabled(bool enabled) { groundDetectionEnabled_ = enabled; }
    void setGroundLayerMask(uint32_t layerMask) { groundLayerMask_ = layerMask; }

    // Performance settings
    void setUpdateFrequency(float frequency) { updateFrequency_ = frequency; }
    void setMaxProcessingDistance(float distance) { maxProcessingDistance_ = distance; }

private:
    // Core processing methods
    void processCharacterController(EntityID entity,
                                  CharacterControllerComponent& controller,
                                  Transform& transform,
                                  EntityManager& entityManager,
                                  float deltaTime);

    void updateGroundDetection(EntityID entity,
                             CharacterControllerComponent& controller,
                             const Transform& transform);

    void updateMovementState(CharacterControllerComponent& controller, float deltaTime);

    void applyMovement(EntityID entity,
                      CharacterControllerComponent& controller,
                      Transform& transform,
                      EntityManager& entityManager,
                      float deltaTime);

    void applyPhysicsForces(EntityID entity,
                           const CharacterControllerComponent& controller,
                           EntityManager& entityManager);

    // Ground detection helpers
    struct GroundInfo {
        bool isGrounded = false;
        glm::vec3 groundPoint{0.0f};
        glm::vec3 groundNormal{0.0f, 1.0f, 0.0f};
        float distance = 0.0f;
        float slopeAngle = 0.0f;
    };

    GroundInfo performGroundCheck(const glm::vec3& position,
                                 const CharacterControllerComponent& controller);

    bool isPointOnGround(const glm::vec3& point,
                        const CharacterControllerComponent& controller);

    // Movement calculation helpers
    glm::vec3 calculateDesiredVelocity(const CharacterControllerComponent& controller);

    glm::vec3 applyAcceleration(const glm::vec3& currentVelocity,
                               const glm::vec3& targetVelocity,
                               float acceleration,
                               float deltaTime);

    void handleJumping(CharacterControllerComponent& controller, float deltaTime);

    void applyGravity(CharacterControllerComponent& controller, float deltaTime);

    // Balance and stability
    void applyAutoBalance(EntityID entity,
                         const CharacterControllerComponent& controller,
                         EntityManager& entityManager);

    // Performance optimization
    bool shouldProcessEntity(EntityID entity, const Transform& transform);

    // Configuration
    std::shared_ptr<PhysicsSystem> physicsSystem_;
    bool groundDetectionEnabled_ = true;
    uint32_t groundLayerMask_ = 0xFFFFFFFF; // Default to all layers

    // Performance settings
    float updateFrequency_ = 60.0f;  // Updates per second
    float maxProcessingDistance_ = 100.0f; // Max distance to process characters
    float lastUpdateTime_ = 0.0f;

    // Statistics (for debugging/profiling)
    struct SystemStats {
        int charactersProcessed = 0;
        int groundChecksPerformed = 0;
        float averageProcessingTime = 0.0f;
        int charactersSkippedDistance = 0;
    } stats_;

public:
    // Debug/profiling interface
    const SystemStats& getStats() const { return stats_; }
    void resetStats() { stats_ = SystemStats{}; }
};

} // namespace VulkanMon