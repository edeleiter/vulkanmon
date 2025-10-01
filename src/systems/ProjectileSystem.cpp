#include "ProjectileSystem.h"
#include "PhysicsSystem.h"
#include "MaterialSystem.h"
#include "../utils/Logger.h"
#include "../spatial/LayerMask.h"
#include "../components/SpatialComponent.h"
#include <chrono>

namespace VulkanMon {

ProjectileSystem::ProjectileSystem(CameraSystem* cameraSystem, MaterialSystem* materialSystem)
    : cameraSystem_(cameraSystem)
    , materialSystem_(materialSystem)
    , materialManager_(nullptr)
{
    if (!cameraSystem_) {
        Logger::getInstance().log(LogLevel::ERROR_LEVEL, "ProjectileSystem created with null CameraSystem!");
        throw std::invalid_argument("CameraSystem cannot be null");
    }

    if (!materialSystem_) {
        Logger::getInstance().log(LogLevel::WARNING_LEVEL, "ProjectileSystem created with null MaterialSystem - will use default materials");
    } else {
        // Create material manager if MaterialSystem is available
        materialManager_ = std::make_unique<ProjectileMaterialManager>(*materialSystem_);
    }

    Logger::getInstance().log(LogLevel::INFO_LEVEL, "ProjectileSystem initialized successfully");
}

void ProjectileSystem::update(float deltaTime, EntityManager& entityManager) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Get all entities with ProjectileComponent
    auto projectileEntities = entityManager.getEntitiesWithComponent<ProjectileComponent>();

    // Filter to only those that also have Transform
    std::vector<EntityID> entities;
    for (EntityID entity : projectileEntities) {
        if (entityManager.hasComponent<Transform>(entity)) {
            entities.push_back(entity);
        }
    }

    stats_.activeProjectiles = entities.size();

    // Update each projectile
    for (EntityID entity : entities) {
        auto& projectile = entityManager.getComponent<ProjectileComponent>(entity);
        auto& transform = entityManager.getComponent<Transform>(entity);

        // Update projectile physics and movement
        updateProjectilePhysics(entity, projectile, transform, deltaTime);

        // Update projectile internal state
        projectile.update(deltaTime, transform.position);

        // Check for lifetime expiration and cleanup
        updateProjectileLifetime(entity, projectile, entityManager, deltaTime);

        // Handle collision detection if enabled
        if (projectile.enableCollisionDetection) {
            handleProjectileCollisions(entity, projectile, transform, entityManager);
        }
    }

    // Update performance statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    stats_.lastFrameUpdateTime = duration.count() / 1000.0f; // Convert to milliseconds

    updateTimeAccumulator_ += stats_.lastFrameUpdateTime;
    updateSampleCount_++;
    stats_.averageUpdateTime = updateTimeAccumulator_ / updateSampleCount_;
}

EntityID ProjectileSystem::spawnProjectileFromMouse(
    float mouseX, float mouseY, float screenWidth, float screenHeight,
    EntityManager& entityManager,
    ProjectileComponent::Type projectileType,
    float speed)
{
    // Get camera ray from mouse coordinates
    auto ray = cameraSystem_->screenToWorldRay(mouseX, mouseY, screenWidth, screenHeight, entityManager);

    // Note: CameraSystem::Ray is returned directly, not as optional
    // If ray calculation fails, direction will be zero vector

    // Get camera position as spawn point
    EntityID cameraEntity = cameraSystem_->getActiveCameraEntity();
    if (cameraEntity == INVALID_ENTITY) {
        Logger::getInstance().log(LogLevel::WARNING_LEVEL, "No active camera for projectile spawn");
        return INVALID_ENTITY;
    }

    auto& cameraTransform = entityManager.getComponent<Transform>(cameraEntity);
    glm::vec3 spawnPosition = cameraTransform.position + ray.direction * 2.0f; // Spawn slightly in front of camera

    // Create projectile component based on type
    ProjectileComponent projectileComponent;
    ProjectileMaterials::Type materialType = ProjectileMaterials::Type::DEFAULT_PROJECTILE;

    switch (projectileType) {
        case ProjectileComponent::Type::BULLET:
            projectileComponent = ProjectileComponent::createBullet(speed > 0 ? speed : 100.0f);
            materialType = ProjectileMaterials::Type::TRACER_BULLET;
            break;
        case ProjectileComponent::Type::GRENADE:
            projectileComponent = ProjectileComponent::createBallisticProjectile(speed > 0 ? speed : 20.0f);
            materialType = ProjectileMaterials::Type::EXPLOSIVE_GRENADE;
            break;
        case ProjectileComponent::Type::ARROW:
            projectileComponent = ProjectileComponent::createBallisticProjectile(speed > 0 ? speed : 25.0f);
            materialType = ProjectileMaterials::Type::PHYSICS_OBJECT;
            break;
        case ProjectileComponent::Type::MAGIC_SPELL:
            projectileComponent = ProjectileComponent::createMagicSpell(speed > 0 ? speed : 15.0f);
            materialType = ProjectileMaterials::Type::MAGIC_SPELL;
            break;
        case ProjectileComponent::Type::POKEBALL:
            projectileComponent = ProjectileComponent::createPokeball(speed > 0 ? speed : 12.0f);
            materialType = ProjectileMaterials::Type::POKEBALL_RED;
            break;
        case ProjectileComponent::Type::PHYSICS_OBJECT:
        default:
            projectileComponent = ProjectileComponent::createPhysicsObject(1.0f, 0.6f);
            materialType = ProjectileMaterials::Type::DEFAULT_PROJECTILE;
            break;
    }

    return spawnCustomProjectile(spawnPosition, ray.direction, projectileComponent, materialType, entityManager);
}

EntityID ProjectileSystem::spawnProjectile(
    const glm::vec3& position,
    const glm::vec3& direction,
    EntityManager& entityManager,
    ProjectileComponent::Type projectileType,
    float speed)
{
    // Create projectile component based on type (same logic as mouse spawn)
    ProjectileComponent projectileComponent;
    ProjectileMaterials::Type materialType = ProjectileMaterials::Type::DEFAULT_PROJECTILE;

    switch (projectileType) {
        case ProjectileComponent::Type::BULLET:
            projectileComponent = ProjectileComponent::createBullet(speed > 0 ? speed : 100.0f);
            materialType = ProjectileMaterials::Type::TRACER_BULLET;
            break;
        case ProjectileComponent::Type::GRENADE:
            projectileComponent = ProjectileComponent::createBallisticProjectile(speed > 0 ? speed : 20.0f);
            materialType = ProjectileMaterials::Type::EXPLOSIVE_GRENADE;
            break;
        case ProjectileComponent::Type::ARROW:
            projectileComponent = ProjectileComponent::createBallisticProjectile(speed > 0 ? speed : 25.0f);
            materialType = ProjectileMaterials::Type::PHYSICS_OBJECT;
            break;
        case ProjectileComponent::Type::MAGIC_SPELL:
            projectileComponent = ProjectileComponent::createMagicSpell(speed > 0 ? speed : 15.0f);
            materialType = ProjectileMaterials::Type::MAGIC_SPELL;
            break;
        case ProjectileComponent::Type::POKEBALL:
            projectileComponent = ProjectileComponent::createPokeball(speed > 0 ? speed : 12.0f);
            materialType = ProjectileMaterials::Type::POKEBALL_RED;
            break;
        case ProjectileComponent::Type::PHYSICS_OBJECT:
        default:
            projectileComponent = ProjectileComponent::createPhysicsObject(1.0f, 0.6f);
            materialType = ProjectileMaterials::Type::DEFAULT_PROJECTILE;
            break;
    }

    return spawnCustomProjectile(position, direction, projectileComponent, materialType, entityManager);
}

EntityID ProjectileSystem::spawnCustomProjectile(
    const glm::vec3& position,
    const glm::vec3& direction,
    const ProjectileComponent& projectileComponent,
    ProjectileMaterials::Type materialType,
    EntityManager& entityManager)
{
    EntityID entity = createProjectileEntity(position, direction, projectileComponent, materialType, entityManager);

    if (entity != INVALID_ENTITY) {
        stats_.projectilesSpawned++;
        logProjectileSpawn(entity, projectileComponent, position, direction);
    }

    return entity;
}

std::vector<EntityID> ProjectileSystem::getActiveProjectiles(EntityManager& entityManager) {
    auto projectileEntities = entityManager.getEntitiesWithComponent<ProjectileComponent>();
    std::vector<EntityID> result;
    for (EntityID entity : projectileEntities) {
        if (entityManager.hasComponent<Transform>(entity)) {
            result.push_back(entity);
        }
    }
    return result;
}

size_t ProjectileSystem::getProjectileCount(EntityManager& entityManager) {
    return getActiveProjectiles(entityManager).size();
}

void ProjectileSystem::destroyProjectile(EntityID entity, EntityManager& entityManager) {
    if (entityManager.hasComponent<ProjectileComponent>(entity)) {
        auto& projectile = entityManager.getComponent<ProjectileComponent>(entity);
        logProjectileDestroy(entity, projectile, "Manual destruction");
        stats_.projectilesDestroyed++;
    }

    entityManager.destroyEntity(entity);
}

void ProjectileSystem::destroyAllProjectiles(EntityManager& entityManager) {
    auto projectiles = getActiveProjectiles(entityManager);
    for (EntityID entity : projectiles) {
        destroyProjectile(entity, entityManager);
    }

    Logger::getInstance().log(LogLevel::INFO_LEVEL,
        "Destroyed all projectiles (count: " + std::to_string(projectiles.size()) + ")");
}

void ProjectileSystem::setPhysicsSystem(PhysicsSystem* physicsSystem) {
    physicsSystem_ = physicsSystem;
    Logger::getInstance().log(LogLevel::INFO_LEVEL,
        "ProjectileSystem: PhysicsSystem " + std::string(physicsSystem ? "connected" : "disconnected"));
}

void ProjectileSystem::setPhysicsEnabled(bool enabled) {
    physicsEnabled_ = enabled;
    Logger::getInstance().log(LogLevel::INFO_LEVEL,
        "ProjectileSystem: Physics " + std::string(enabled ? "enabled" : "disabled"));
}

void ProjectileSystem::setMaterialSystem(MaterialSystem* materialSystem) {
    materialSystem_ = materialSystem;

    if (materialSystem_) {
        // Create or recreate material manager with new MaterialSystem
        materialManager_ = std::make_unique<ProjectileMaterialManager>(*materialSystem_);
        Logger::getInstance().log(LogLevel::INFO_LEVEL, "ProjectileSystem: MaterialSystem connected and material manager created");
    } else {
        // Clear material manager if MaterialSystem is disconnected
        materialManager_.reset();
        Logger::getInstance().log(LogLevel::INFO_LEVEL, "ProjectileSystem: MaterialSystem disconnected");
    }
}

void ProjectileSystem::setCollisionCallback(CollisionCallback callback) {
    collisionCallback_ = callback;
}

ProjectileSystem::PerformanceStats ProjectileSystem::getPerformanceStats() const {
    return stats_;
}

void ProjectileSystem::resetPerformanceStats() {
    stats_ = {};
    updateTimeAccumulator_ = 0.0f;
    updateSampleCount_ = 0;
}

// Private helper methods

void ProjectileSystem::updateProjectilePhysics(EntityID entity, ProjectileComponent& projectile, Transform& transform, float deltaTime) {
    // Apply gravity if enabled
    if (projectile.affectedByGravity && projectile.gravityScale > 0.0f) {
        const float gravity = 9.81f * projectile.gravityScale;
        projectile.currentVelocity.y -= gravity * deltaTime;
    }

    // Apply drag
    projectile.applyDrag(deltaTime);

    // Update position based on velocity
    glm::vec3 movement = projectile.currentVelocity * deltaTime;
    transform.position += movement;

    // If using physics system, sync with physics body
    // TODO: Add proper physics synchronization when needed
    // if (physicsEnabled_ && physicsSystem_) {
    //     // Physics system will handle position updates
    //     // We just need to read back the physics position
    // }
}

void ProjectileSystem::updateProjectileLifetime(EntityID entity, ProjectileComponent& projectile, EntityManager& entityManager, float deltaTime) {
    if (projectile.shouldDestroy()) {
        std::string reason = "Lifetime expired";
        if (projectile.hasTimeLimit && projectile.timeSinceSpawn >= projectile.timeToLive) {
            reason = "Time limit reached (" + std::to_string(projectile.timeSinceSpawn) + "s)";
        } else if (projectile.hasDistanceLimit && projectile.distanceTraveled >= projectile.maxDistance) {
            reason = "Distance limit reached (" + std::to_string(projectile.distanceTraveled) + "m)";
        }

        logProjectileDestroy(entity, projectile, reason);
        stats_.projectilesDestroyed++;
        entityManager.destroyEntity(entity);
    }
}

void ProjectileSystem::handleProjectileCollisions(EntityID entity, ProjectileComponent& projectile, Transform& transform, EntityManager& entityManager) {
    // Simplified collision detection - in a real implementation this would integrate with physics system
    // For now, just implement basic ground collision

    const float groundHeight = 0.0f;
    if (transform.position.y <= groundHeight && projectile.currentVelocity.y < 0.0f) {
        // Ground collision detected
        glm::vec3 groundNormal(0.0f, 1.0f, 0.0f);
        float impactSpeed = glm::length(projectile.currentVelocity);

        // Call collision callback if set
        if (collisionCallback_) {
            collisionCallback_(entity, INVALID_ENTITY,
                glm::vec3(transform.position.x, groundHeight, transform.position.z),
                groundNormal, impactSpeed);
        }

        // Handle collision response
        projectile.handleCollision(groundNormal, impactSpeed);

        // Adjust position to ground level
        transform.position.y = groundHeight;

        // Destroy if configured to destroy on any collision
        if (projectile.destroyOnAnyCollision || projectile.collisionResponse == ProjectileComponent::CollisionResponse::DESTROY) {
            logProjectileDestroy(entity, projectile, "Ground collision");
            stats_.projectilesDestroyed++;
            entityManager.destroyEntity(entity);
        }
    }
}

EntityID ProjectileSystem::createProjectileEntity(
    const glm::vec3& position,
    const glm::vec3& direction,
    const ProjectileComponent& projectileComponent,
    ProjectileMaterials::Type materialType,
    EntityManager& entityManager)
{
    EntityID entity = entityManager.createEntity();

    // Add Transform component
    Transform transform;
    transform.position = position;
    transform.scale = glm::vec3(0.1f); // Small projectile size
    entityManager.addComponent(entity, transform);

    // Add ProjectileComponent (copy and initialize)
    ProjectileComponent projectile = projectileComponent;
    projectile.initialize(position, direction);
    entityManager.addComponent(entity, projectile);

    // Add Renderable component for visual representation
    Renderable renderable;
    renderable.meshPath = "projectile_sphere.obj";  // SHORT PATH - assets/models/ prefix added automatically
    renderable.materialId = static_cast<uint32_t>(materialType);
    renderable.isVisible = true;
    entityManager.addComponent(entity, renderable);

    // Add SpatialComponent so projectile is found by frustum culling
    SpatialComponent spatial;
    spatial.spatialLayers = LayerMask::CaptureDevices;  // Projectiles use CaptureDevices layer
    spatial.boundingRadius = 0.2f;  // Small radius for projectile
    spatial.behavior = SpatialBehavior::DYNAMIC;
    entityManager.addComponent(entity, spatial);

    // Add physics if enabled
    if (physicsEnabled_) {
        setupProjectilePhysics(entity, projectile, entityManager);
    }

    return entity;
}

void ProjectileSystem::setupProjectilePhysics(EntityID entity, const ProjectileComponent& projectile, EntityManager& entityManager) {
    if (!physicsSystem_) {
        return;
    }

    // Add RigidBodyComponent for physics integration
    RigidBodyComponent rigidBody;
    rigidBody.mass = projectile.mass;
    rigidBody.isDynamic = true;
    rigidBody.useGravity = projectile.affectedByGravity;
    rigidBody.velocity = projectile.currentVelocity;
    entityManager.addComponent(entity, rigidBody);

    // Add CollisionComponent
    CollisionComponent collision;
    collision.shapeType = CollisionComponent::ShapeType::Sphere;
    collision.dimensions = glm::vec3(0.05f); // Small sphere radius
    collision.layer = LayerMask::CaptureDevices;
    collision.collidesWith = projectile.collisionMask;
    entityManager.addComponent(entity, collision);
}

void ProjectileSystem::logProjectileSpawn(EntityID entity, const ProjectileComponent& projectile, const glm::vec3& position, const glm::vec3& direction) {
    if (!debugLogging_) return;

    std::string typeStr;
    switch (projectile.type) {
        case ProjectileComponent::Type::BULLET: typeStr = "BULLET"; break;
        case ProjectileComponent::Type::GRENADE: typeStr = "GRENADE"; break;
        case ProjectileComponent::Type::ARROW: typeStr = "ARROW"; break;
        case ProjectileComponent::Type::MAGIC_SPELL: typeStr = "MAGIC_SPELL"; break;
        case ProjectileComponent::Type::POKEBALL: typeStr = "POKEBALL"; break;
        case ProjectileComponent::Type::PHYSICS_OBJECT: typeStr = "PHYSICS_OBJECT"; break;
        default: typeStr = "CUSTOM"; break;
    }

    Logger::getInstance().log(LogLevel::INFO_LEVEL,
        "Projectile spawned - Entity: " + std::to_string(entity) +
        ", Type: " + typeStr +
        ", Position: (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")" +
        ", Speed: " + std::to_string(glm::length(projectile.initialVelocity)) + "m/s");
}

void ProjectileSystem::logProjectileDestroy(EntityID entity, const ProjectileComponent& projectile, const std::string& reason) {
    if (!debugLogging_) return;

    Logger::getInstance().log(LogLevel::INFO_LEVEL,
        "Projectile destroyed - Entity: " + std::to_string(entity) +
        ", Reason: " + reason +
        ", Lifetime: " + std::to_string(projectile.timeSinceSpawn) + "s" +
        ", Distance: " + std::to_string(projectile.distanceTraveled) + "m");
}

} // namespace VulkanMon