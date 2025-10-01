#pragma once

#include "../core/SystemImpl.h"
#include "../components/ProjectileComponent.h"
#include "../components/Transform.h"
#include "../components/RigidBodyComponent.h"
#include "../components/Renderable.h"
#include "../core/EntityManager.h"
#include "../systems/CameraSystem.h"
#include "../materials/ProjectileMaterials.h"
#include <memory>
#include <vector>
#include <functional>

namespace VulkanMon {

// Forward declarations
class PhysicsSystem;
class MaterialSystem;

/**
 * ProjectileSystem
 *
 * ECS system for managing projectile entity lifecycle, physics integration, and collision handling.
 * Handles projectile spawning, trajectory simulation, lifetime management, and cleanup.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clean API for projectile spawning and management
 * - Engine-Generic: Works for bullets, grenades, magic spells, physics objects
 * - Performance-aware: Efficient batch processing and optimized cleanup
 */
class ProjectileSystem : public System<ProjectileComponent, Transform> {
public:
    /**
     * Create ProjectileSystem with required dependencies
     * @param cameraSystem Camera system for ray projection
     * @param materialSystem Material system for projectile visuals
     */
    ProjectileSystem(CameraSystem* cameraSystem, MaterialSystem* materialSystem);

    /**
     * Update all projectiles - called every frame
     * @param deltaTime Time since last frame in seconds
     * @param entityManager ECS entity manager
     */
    void update(float deltaTime, EntityManager& entityManager) override;

    // =========================================================================
    // PROJECTILE SPAWNING API
    // =========================================================================

    /**
     * Spawn projectile from mouse click using camera ray projection
     * @param mouseX Mouse X coordinate in screen space
     * @param mouseY Mouse Y coordinate in screen space
     * @param screenWidth Window width in pixels
     * @param screenHeight Window height in pixels
     * @param entityManager ECS entity manager
     * @param projectileType Type of projectile to spawn
     * @param speed Initial speed in m/s (overrides component default if > 0)
     * @return Entity ID of spawned projectile
     */
    EntityID spawnProjectileFromMouse(
        float mouseX, float mouseY, float screenWidth, float screenHeight,
        EntityManager& entityManager,
        ProjectileComponent::Type projectileType = ProjectileComponent::Type::PHYSICS_OBJECT,
        float speed = -1.0f
    );

    /**
     * Spawn projectile with explicit position and direction
     * @param position World space spawn position
     * @param direction World space direction (will be normalized)
     * @param entityManager ECS entity manager
     * @param projectileType Type of projectile to spawn
     * @param speed Initial speed in m/s (overrides component default if > 0)
     * @return Entity ID of spawned projectile
     */
    EntityID spawnProjectile(
        const glm::vec3& position,
        const glm::vec3& direction,
        EntityManager& entityManager,
        ProjectileComponent::Type projectileType = ProjectileComponent::Type::PHYSICS_OBJECT,
        float speed = -1.0f
    );

    /**
     * Spawn projectile with custom component configuration
     * @param position World space spawn position
     * @param direction World space direction (will be normalized)
     * @param projectileComponent Custom projectile configuration
     * @param materialType Material type for visual appearance
     * @param entityManager ECS entity manager
     * @return Entity ID of spawned projectile
     */
    EntityID spawnCustomProjectile(
        const glm::vec3& position,
        const glm::vec3& direction,
        const ProjectileComponent& projectileComponent,
        ProjectileMaterials::Type materialType,
        EntityManager& entityManager
    );

    // =========================================================================
    // PROJECTILE MANAGEMENT
    // =========================================================================

    /**
     * Get all active projectile entities
     * @param entityManager ECS entity manager
     * @return Vector of projectile entity IDs
     */
    std::vector<EntityID> getActiveProjectiles(EntityManager& entityManager);

    /**
     * Get projectile count
     * @param entityManager ECS entity manager
     * @return Number of active projectiles
     */
    size_t getProjectileCount(EntityManager& entityManager);

    /**
     * Destroy projectile immediately
     * @param entity Projectile entity to destroy
     * @param entityManager ECS entity manager
     */
    void destroyProjectile(EntityID entity, EntityManager& entityManager);

    /**
     * Destroy all projectiles
     * @param entityManager ECS entity manager
     */
    void destroyAllProjectiles(EntityManager& entityManager);

    // =========================================================================
    // PHYSICS INTEGRATION
    // =========================================================================

    /**
     * Set physics system for physics-based projectiles
     * @param physicsSystem Pointer to physics system (can be nullptr to disable physics)
     */
    void setPhysicsSystem(PhysicsSystem* physicsSystem);

    /**
     * Enable/disable physics integration for new projectiles
     * @param enabled Whether to create physics bodies for projectiles
     */
    void setPhysicsEnabled(bool enabled);

    /**
     * Set material system for projectile visuals (can be set after construction)
     * @param materialSystem Pointer to material system (can be nullptr)
     */
    void setMaterialSystem(MaterialSystem* materialSystem);

    // =========================================================================
    // COLLISION CALLBACKS
    // =========================================================================

    /**
     * Collision callback function type
     * @param projectileEntity The projectile that collided
     * @param hitEntity The entity that was hit (may be INVALID_ENTITY for terrain)
     * @param impactPoint World space collision point
     * @param impactNormal Surface normal at collision point
     * @param impactSpeed Speed at time of impact
     */
    using CollisionCallback = std::function<void(
        EntityID projectileEntity,
        EntityID hitEntity,
        const glm::vec3& impactPoint,
        const glm::vec3& impactNormal,
        float impactSpeed
    )>;

    /**
     * Register callback for projectile collisions
     * @param callback Function to call when projectiles collide
     */
    void setCollisionCallback(CollisionCallback callback);

    // =========================================================================
    // PERFORMANCE MONITORING
    // =========================================================================

    /**
     * Get performance statistics
     */
    struct PerformanceStats {
        size_t activeProjectiles;
        size_t projectilesSpawned;
        size_t projectilesDestroyed;
        float averageUpdateTime;
        float lastFrameUpdateTime;
    };

    PerformanceStats getPerformanceStats() const;

    /**
     * Reset performance statistics
     */
    void resetPerformanceStats();

private:
    // System dependencies
    CameraSystem* cameraSystem_;
    MaterialSystem* materialSystem_;
    PhysicsSystem* physicsSystem_ = nullptr;
    std::unique_ptr<ProjectileMaterialManager> materialManager_;

    // Configuration
    bool physicsEnabled_ = true;
    bool debugLogging_ = true;

    // Collision handling
    CollisionCallback collisionCallback_;

    // Performance tracking
    mutable PerformanceStats stats_ = {};
    mutable float updateTimeAccumulator_ = 0.0f;
    mutable size_t updateSampleCount_ = 0;

    // Private helper methods
    void updateProjectilePhysics(EntityID entity, ProjectileComponent& projectile, Transform& transform, float deltaTime);
    void updateProjectileLifetime(EntityID entity, ProjectileComponent& projectile, EntityManager& entityManager, float deltaTime);
    void handleProjectileCollisions(EntityID entity, ProjectileComponent& projectile, Transform& transform, EntityManager& entityManager);

    EntityID createProjectileEntity(
        const glm::vec3& position,
        const glm::vec3& direction,
        const ProjectileComponent& projectileComponent,
        ProjectileMaterials::Type materialType,
        EntityManager& entityManager
    );

    void setupProjectilePhysics(EntityID entity, const ProjectileComponent& projectile, EntityManager& entityManager);
    void logProjectileSpawn(EntityID entity, const ProjectileComponent& projectile, const glm::vec3& position, const glm::vec3& direction);
    void logProjectileDestroy(EntityID entity, const ProjectileComponent& projectile, const std::string& reason);
};

} // namespace VulkanMon