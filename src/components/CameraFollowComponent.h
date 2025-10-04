#pragma once

#include "../core/Entity.h"
#include <glm/glm.hpp>

namespace VulkanMon {

/**
 * CameraFollowComponent
 *
 * Generic camera follow behavior component for third-person games.
 * Supports smooth camera tracking, offset positioning, and rotation following.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clean third-person camera behavior without game-specific coupling
 * - Engine-Focused: Works for any game type (Pokemon, action-adventure, racing, etc.)
 * - Performance-Optimized: Lightweight component with efficient smoothing calculations
 *
 * Common Use Cases:
 * - Third-person character cameras (Pokemon Legends: Arceus style)
 * - Vehicle follow cameras (racing games, flight simulators)
 * - Cinematic tracking shots (cutscenes, camera animations)
 * - Top-down follow cameras (strategy games, MOBAs)
 *
 * @example Basic Third-Person Camera
 * @code
 * auto cameraFollow = CameraFollowComponent::createThirdPerson();
 * cameraFollow.targetEntity = playerEntity;
 * entityManager.addComponent(cameraEntity, cameraFollow);
 * @endcode
 */
struct CameraFollowComponent {
    VKMON_COMPONENT(CameraFollowComponent)

    // =============================================================================
    // TARGET TRACKING
    // =============================================================================

    /// Entity to follow (0 = no target)
    EntityID targetEntity = 0;

    /// Whether to enable follow behavior
    bool enabled = true;

    /// Whether to follow target's rotation (true = orbit camera, false = fixed angle)
    bool followRotation = false;

    // =============================================================================
    // CAMERA OFFSET POSITIONING
    // =============================================================================

    /// Camera offset from target in target's local space (back, up, right)
    /// Example: (-5, 2, 0) = 5 units behind, 2 units up, 0 units right
    glm::vec3 offset{-5.0f, 2.0f, 0.0f};

    /// Look-at offset from target position (where camera points)
    /// Example: (0, 1.5, 0) = look at target's head height
    glm::vec3 lookAtOffset{0.0f, 1.5f, 0.0f};

    // =============================================================================
    // SMOOTHING PARAMETERS
    // =============================================================================

    /// Position smoothing factor (0 = instant, 1 = very slow)
    /// Higher values = smoother but more laggy camera
    float positionSmoothness = 0.1f;

    /// Rotation smoothing factor (0 = instant, 1 = very slow)
    float rotationSmoothness = 0.15f;

    /// Minimum distance to maintain from target
    float minDistance = 2.0f;

    /// Maximum distance to maintain from target
    float maxDistance = 20.0f;

    // =============================================================================
    // CAMERA BEHAVIOR MODES
    // =============================================================================

    enum class FollowMode {
        /// Standard third-person follow (maintains offset relative to target)
        ThirdPerson,

        /// Orbit camera (rotates around target, maintains distance)
        Orbit,

        /// Top-down camera (follows XZ position, maintains fixed Y offset)
        TopDown,

        /// First-person camera (matches target position with small offset)
        FirstPerson,

        /// Free camera (no following, manual control only)
        Free,

        /// Custom behavior (handled by game-specific code)
        Custom
    };

    FollowMode mode = FollowMode::ThirdPerson;

    // =============================================================================
    // COLLISION AVOIDANCE
    // =============================================================================

    /// Enable collision detection to prevent camera clipping through walls
    bool collisionEnabled = false;

    /// Layer mask for collision detection (what objects can block camera)
    uint32_t collisionLayerMask = 0xFFFFFFFF;

    /// Collision detection radius (how thick is the camera collision sphere)
    float collisionRadius = 0.3f;

    // =============================================================================
    // FACTORY METHODS - COMMON CAMERA PRESETS
    // =============================================================================

    /**
     * @brief Create third-person action camera (Pokemon Legends: Arceus style)
     * @param distance Distance behind target (default: 5.0f)
     * @param height Height above target (default: 2.0f)
     * @return Configured CameraFollowComponent
     */
    static CameraFollowComponent createThirdPerson(float distance = 5.0f, float height = 2.0f) {
        CameraFollowComponent follow;
        follow.mode = FollowMode::ThirdPerson;
        follow.offset = glm::vec3(-distance, height, 0.0f);
        follow.lookAtOffset = glm::vec3(0.0f, 1.5f, 0.0f);
        follow.positionSmoothness = 0.1f;
        follow.rotationSmoothness = 0.15f;
        follow.followRotation = false;
        return follow;
    }

    /**
     * @brief Create orbit camera (rotates around target)
     * @param radius Distance from target (default: 8.0f)
     * @param height Height above target (default: 3.0f)
     * @return Configured CameraFollowComponent
     */
    static CameraFollowComponent createOrbit(float radius = 8.0f, float height = 3.0f) {
        CameraFollowComponent follow;
        follow.mode = FollowMode::Orbit;
        follow.offset = glm::vec3(-radius, height, 0.0f);
        follow.lookAtOffset = glm::vec3(0.0f, 1.0f, 0.0f);
        follow.positionSmoothness = 0.05f;
        follow.rotationSmoothness = 0.1f;
        follow.followRotation = true;
        follow.minDistance = 2.0f;
        follow.maxDistance = 20.0f;
        return follow;
    }

    /**
     * @brief Create top-down strategy camera
     * @param height Height above target (default: 15.0f)
     * @param angle Pitch angle in degrees (default: 45.0f)
     * @return Configured CameraFollowComponent
     */
    static CameraFollowComponent createTopDown(float height = 15.0f, float angle = 45.0f) {
        CameraFollowComponent follow;
        follow.mode = FollowMode::TopDown;
        follow.offset = glm::vec3(0.0f, height, 0.0f);
        follow.lookAtOffset = glm::vec3(0.0f, 0.0f, 0.0f);
        follow.positionSmoothness = 0.2f;
        follow.rotationSmoothness = 0.3f;
        follow.followRotation = false;
        return follow;
    }

    /**
     * @brief Create first-person camera (matches target view)
     * @param eyeHeight Height offset for eye position (default: 1.7f)
     * @return Configured CameraFollowComponent
     */
    static CameraFollowComponent createFirstPerson(float eyeHeight = 1.7f) {
        CameraFollowComponent follow;
        follow.mode = FollowMode::FirstPerson;
        follow.offset = glm::vec3(0.0f, eyeHeight, 0.0f);
        follow.lookAtOffset = glm::vec3(0.0f, 0.0f, 1.0f); // Look forward
        follow.positionSmoothness = 0.0f; // Instant position following
        follow.rotationSmoothness = 0.05f; // Slight rotation smoothing
        follow.followRotation = true;
        return follow;
    }

    /**
     * @brief Create free camera (no following, manual control only)
     * @return Configured CameraFollowComponent
     */
    static CameraFollowComponent createFree() {
        CameraFollowComponent follow;
        follow.mode = FollowMode::Free;
        follow.enabled = false;
        follow.targetEntity = 0;
        return follow;
    }

    // =============================================================================
    // HELPER METHODS
    // =============================================================================

    /// Check if camera has valid target
    bool hasTarget() const { return targetEntity != 0; }

    /// Set new target entity
    void setTarget(EntityID entity) { targetEntity = entity; }

    /// Clear target (stop following)
    void clearTarget() { targetEntity = 0; }

    /// Enable/disable follow behavior
    void setEnabled(bool enable) { enabled = enable; }

    /// Check if follow is active
    bool isActive() const { return enabled && hasTarget(); }
};

} // namespace VulkanMon
