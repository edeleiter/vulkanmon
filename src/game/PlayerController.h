#pragma once

#include "../core/Entity.h"
#include "../core/EntityManager.h"
#include "../components/Transform.h"
#include "../components/CharacterControllerComponent.h"
#include "../components/Camera.h"
#include "../components/CameraFollowComponent.h"
#include "../components/Renderable.h"
#include "../spatial/LayerMask.h"
#include <glm/glm.hpp>

namespace VulkanMon {

/**
 * PlayerController
 *
 * Pokemon-specific player character factory and control system.
 * Creates player entity with third-person camera for Pokemon Legends: Arceus style gameplay.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clean player setup with sensible Pokemon defaults
 * - Game-Specific: Pokemon-focused movement speeds and camera positioning
 * - ECS-Based: Uses generic CharacterControllerComponent + game-specific configuration
 *
 * Features:
 * - Player entity creation with Transform, CharacterController, Renderable (optional)
 * - Third-person camera entity with follow behavior
 * - Pokemon-appropriate movement speeds (walk, sprint, dodge)
 * - Layer mask configuration for Pokemon world interaction
 *
 * @example Basic Player Setup
 * @code
 * auto [playerEntity, cameraEntity] = PlayerController::createPlayer(world, entityManager);
 * PlayerController::setInputVector(playerEntity, inputVector, entityManager);
 * @endcode
 */
class PlayerController {
public:
    // =============================================================================
    // PLAYER ENTITY CREATION
    // =============================================================================

    /**
     * @brief Create player entity with character controller and third-person camera
     * @param entityManager ECS entity manager
     * @return Pair of (playerEntity, cameraEntity)
     */
    static std::pair<EntityID, EntityID> createPlayer(EntityManager& entityManager) {
        // Create player entity
        EntityID playerEntity = entityManager.createEntity();

        // Add Transform component at origin
        Transform playerTransform;
        playerTransform.position = glm::vec3(0.0f, 1.0f, 0.0f); // Start above ground
        playerTransform.scale = glm::vec3(1.0f);
        entityManager.addComponent(playerEntity, playerTransform);

        // Add CharacterController with Pokemon-appropriate settings
        auto characterController = CharacterControllerComponent::createHumanoid(5.0f); // 5 m/s walk speed
        characterController.sprintMultiplier = 1.8f;  // Pokemon sprint speed
        characterController.jumpForce = 7.0f;         // Lower jump for Pokemon style
        characterController.groundCheckDistance = 0.15f;
        characterController.slopeLimit = 45.0f;       // Pokemon can climb moderate slopes
        entityManager.addComponent(playerEntity, characterController);

        // Add Renderable so player is visible (temporary sphere model)
        Renderable playerRenderable;
        playerRenderable.meshPath = "sphere.obj";      // Use sphere as temporary player model
        playerRenderable.texturePath = "default";
        playerRenderable.materialId = 2;               // Ruby material (red for visibility)
        playerRenderable.isVisible = true;
        entityManager.addComponent(playerEntity, playerRenderable);

        // Create third-person camera entity
        EntityID cameraEntity = createPlayerCamera(entityManager, playerEntity);

        return {playerEntity, cameraEntity};
    }

    /**
     * @brief Create player camera with third-person follow behavior
     * @param entityManager ECS entity manager
     * @param targetEntity Player entity to follow
     * @return Camera entity ID
     */
    static EntityID createPlayerCamera(EntityManager& entityManager, EntityID targetEntity) {
        EntityID cameraEntity = entityManager.createEntity();

        // Add Transform (initial position will be set by follow system)
        Transform cameraTransform;
        cameraTransform.position = glm::vec3(0.0f, 5.0f, 10.0f); // Start behind and above player
        entityManager.addComponent(cameraEntity, cameraTransform);

        // Add Camera component (perspective)
        Camera camera;
        camera.setPerspective(75.0f, 16.0f / 9.0f, 0.1f, 200.0f);
        camera.isActive = true;
        camera.priority = 100; // Highest priority camera
        entityManager.addComponent(cameraEntity, camera);

        // Add CameraFollow component (Pokemon Legends: Arceus style)
        auto cameraFollow = CameraFollowComponent::createThirdPerson(8.0f, 4.0f); // 8m back, 4m up
        cameraFollow.targetEntity = targetEntity;
        cameraFollow.lookAtOffset = glm::vec3(0.0f, 1.2f, 0.0f); // Look at player's upper body
        cameraFollow.positionSmoothness = 0.12f;  // Smooth camera movement
        cameraFollow.rotationSmoothness = 0.18f;  // Smooth camera rotation
        cameraFollow.followRotation = false;      // Camera doesn't rotate with player (free look)
        entityManager.addComponent(cameraEntity, cameraFollow);

        return cameraEntity;
    }

    // =============================================================================
    // INPUT HANDLING
    // =============================================================================

    /**
     * @brief Set player movement input vector (called from InputHandler)
     * @param playerEntity Player entity ID
     * @param inputVector Movement direction in world space (normalized)
     * @param entityManager ECS entity manager
     * @param sprinting Whether player is sprinting
     */
    static void setInputVector(EntityID playerEntity, const glm::vec3& inputVector,
                              EntityManager& entityManager, bool sprinting = false) {
        if (!entityManager.hasComponent<CharacterControllerComponent>(playerEntity)) {
            return;
        }

        auto& controller = entityManager.getComponent<CharacterControllerComponent>(playerEntity);
        controller.inputDirection = inputVector;
        controller.wantsToSprint = sprinting;
    }

    /**
     * @brief Trigger player jump (called from InputHandler on jump input)
     * @param playerEntity Player entity ID
     * @param entityManager ECS entity manager
     */
    static void jump(EntityID playerEntity, EntityManager& entityManager) {
        if (!entityManager.hasComponent<CharacterControllerComponent>(playerEntity)) {
            return;
        }

        auto& controller = entityManager.getComponent<CharacterControllerComponent>(playerEntity);
        controller.wantsToJump = true;
    }

    /**
     * @brief Set player dodge/roll input (Pokemon Legends: Arceus has dodge roll)
     * @param playerEntity Player entity ID
     * @param dodgeDirection Direction to dodge (normalized)
     * @param entityManager ECS entity manager
     */
    static void dodge(EntityID playerEntity, const glm::vec3& dodgeDirection,
                     EntityManager& entityManager) {
        if (!entityManager.hasComponent<CharacterControllerComponent>(playerEntity)) {
            return;
        }

        auto& controller = entityManager.getComponent<CharacterControllerComponent>(playerEntity);
        // Dodge implementation - set high velocity burst in dodge direction
        // This will be refined when we add animation system
        controller.velocity = dodgeDirection * 15.0f; // Fast dodge velocity
    }

    // =============================================================================
    // PLAYER STATE QUERIES
    // =============================================================================

    /**
     * @brief Get player position in world space
     * @param playerEntity Player entity ID
     * @param entityManager ECS entity manager
     * @return Player position vector
     */
    static glm::vec3 getPlayerPosition(EntityID playerEntity, EntityManager& entityManager) {
        if (!entityManager.hasComponent<Transform>(playerEntity)) {
            return glm::vec3(0.0f);
        }
        return entityManager.getComponent<Transform>(playerEntity).position;
    }

    /**
     * @brief Get player forward direction
     * @param playerEntity Player entity ID
     * @param entityManager ECS entity manager
     * @return Player forward vector (normalized)
     */
    static glm::vec3 getPlayerForward(EntityID playerEntity, EntityManager& entityManager) {
        if (!entityManager.hasComponent<Transform>(playerEntity)) {
            return glm::vec3(0.0f, 0.0f, -1.0f);
        }
        return entityManager.getComponent<Transform>(playerEntity).getForward();
    }

    /**
     * @brief Check if player is grounded
     * @param playerEntity Player entity ID
     * @param entityManager ECS entity manager
     * @return True if player is on ground
     */
    static bool isPlayerGrounded(EntityID playerEntity, EntityManager& entityManager) {
        if (!entityManager.hasComponent<CharacterControllerComponent>(playerEntity)) {
            return false;
        }
        return entityManager.getComponent<CharacterControllerComponent>(playerEntity).isGrounded;
    }

    /**
     * @brief Check if player is sprinting
     * @param playerEntity Player entity ID
     * @param entityManager ECS entity manager
     * @return True if player is sprinting
     */
    static bool isPlayerSprinting(EntityID playerEntity, EntityManager& entityManager) {
        if (!entityManager.hasComponent<CharacterControllerComponent>(playerEntity)) {
            return false;
        }
        return entityManager.getComponent<CharacterControllerComponent>(playerEntity).wantsToSprint;
    }

    // =============================================================================
    // POKEMON-SPECIFIC CONFIGURATION
    // =============================================================================

    /**
     * @brief Configure player for Pokemon world interaction
     * @details Sets up layer masks, collision settings, and Pokemon-specific behaviors
     * @param playerEntity Player entity ID
     * @param entityManager ECS entity manager
     */
    static void configurePokemonInteraction(EntityID playerEntity, EntityManager& entityManager) {
        // This will be expanded when we add collision and interaction systems
        // For now, just ensure player entity is on Player layer
        // (Future: Add collision component, interaction component, etc.)
    }
};

} // namespace VulkanMon
