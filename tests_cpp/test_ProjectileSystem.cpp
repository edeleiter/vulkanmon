/*
 * VulkanMon ProjectileSystem Unit Tests
 *
 * Comprehensive tests for the ProjectileSystem focusing on:
 * - ProjectileComponent factory methods and initialization
 * - Projectile spawning and entity creation
 * - Projectile physics updates and lifetime management
 * - Component integration (Transform, Renderable, Spatial)
 * - Performance and cleanup
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/systems/ProjectileSystem.h"
#include "../src/components/ProjectileComponent.h"
#include "../src/components/Transform.h"
#include "../src/components/Renderable.h"
#include "../src/components/SpatialComponent.h"
#include "../src/core/World.h"
#include "../src/systems/CameraSystem.h"
#include "../src/systems/MaterialSystem.h"

using namespace VulkanMon;
using Catch::Approx;

TEST_CASE("ProjectileComponent Factory Methods", "[Projectile][Component]") {
    SECTION("createBullet factory") {
        auto bullet = ProjectileComponent::createBullet(100.0f);

        REQUIRE(bullet.type == ProjectileComponent::Type::BULLET);
        REQUIRE(bullet.gravityScale == Approx(0.1f)); // Minimal gravity for bullets
        REQUIRE(bullet.timeToLive == 5.0f);
        REQUIRE(bullet.hasTimeLimit == true);
        REQUIRE(bullet.timeSinceSpawn == 0.0f);
        REQUIRE(bullet.collisionResponse == ProjectileComponent::CollisionResponse::DESTROY);
    }

    SECTION("createBallisticProjectile factory") {
        auto arrow = ProjectileComponent::createBallisticProjectile(20.0f);

        REQUIRE(arrow.type == ProjectileComponent::Type::ARROW); // Creates ARROW type, not GRENADE
        REQUIRE(arrow.gravityScale == 1.0f);
        REQUIRE(arrow.hasTimeLimit == true);
        REQUIRE(arrow.collisionResponse == ProjectileComponent::CollisionResponse::STICK);
    }

    SECTION("createMagicSpell factory") {
        auto spell = ProjectileComponent::createMagicSpell(15.0f);

        REQUIRE(spell.type == ProjectileComponent::Type::MAGIC_SPELL);
        REQUIRE(spell.gravityScale == 0.0f); // Magic has no gravity
        REQUIRE(spell.hasTimeLimit == true);
        REQUIRE(spell.explosionRadius == Approx(2.0f));
    }

    SECTION("createPokeball factory") {
        auto pokeball = ProjectileComponent::createPokeball(12.0f);

        REQUIRE(pokeball.type == ProjectileComponent::Type::POKEBALL);
        REQUIRE(pokeball.gravityScale == 1.0f);
        REQUIRE(pokeball.mass == 0.2f);
        REQUIRE(pokeball.collisionResponse == ProjectileComponent::CollisionResponse::BOUNCE);
    }

    SECTION("createPhysicsObject factory") {
        auto physics = ProjectileComponent::createPhysicsObject(2.0f, 0.8f);

        REQUIRE(physics.type == ProjectileComponent::Type::PHYSICS_OBJECT);
        REQUIRE(physics.mass == 2.0f);
        REQUIRE(physics.bounciness == 0.8f);
        REQUIRE(physics.gravityScale == 1.0f);
        REQUIRE(physics.timeSinceSpawn == 0.0f); // Zero-initialized
    }
}

TEST_CASE("ProjectileComponent Initialization", "[Projectile][Component]") {
    SECTION("initialize sets spawn position") {
        ProjectileComponent projectile = ProjectileComponent::createBullet(50.0f);
        glm::vec3 position(1.0f, 2.0f, 3.0f);
        glm::vec3 direction(0.0f, 0.0f, -1.0f);

        projectile.initialize(position, direction, 50.0f);

        REQUIRE(projectile.spawnPosition.x == Approx(1.0f));
        REQUIRE(projectile.spawnPosition.y == Approx(2.0f));
        REQUIRE(projectile.spawnPosition.z == Approx(3.0f));
        REQUIRE(projectile.timeSinceSpawn == 0.0f);
        REQUIRE(projectile.distanceTraveled == 0.0f);
    }

    SECTION("initialize with explicit speed sets velocity") {
        ProjectileComponent projectile = ProjectileComponent::createPhysicsObject();
        glm::vec3 position(0.0f, 0.0f, 0.0f);
        glm::vec3 direction(1.0f, 0.0f, 0.0f);

        projectile.initialize(position, direction, 25.0f);

        // Velocity should be normalized direction * speed
        REQUIRE(glm::length(projectile.currentVelocity) == Approx(25.0f));
        REQUIRE(projectile.currentVelocity.x == Approx(25.0f));
    }
}

TEST_CASE("ProjectileComponent Update", "[Projectile][Component]") {
    SECTION("update increments timeSinceSpawn") {
        ProjectileComponent projectile = ProjectileComponent::createBullet();
        glm::vec3 position(0.0f, 0.0f, 0.0f);
        glm::vec3 direction(0.0f, 0.0f, -1.0f);
        projectile.initialize(position, direction);

        projectile.update(0.016f, position); // ~60 FPS frame
        REQUIRE(projectile.timeSinceSpawn == Approx(0.016f));

        projectile.update(0.016f, position);
        REQUIRE(projectile.timeSinceSpawn == Approx(0.032f));
    }

    SECTION("update tracks distance with trajectory enabled") {
        ProjectileComponent projectile = ProjectileComponent::createBallisticProjectile();
        projectile.trackTrajectory = true;
        glm::vec3 startPos(0.0f, 0.0f, 0.0f);
        projectile.initialize(startPos, glm::vec3(0.0f, 0.0f, -1.0f), 10.0f);

        glm::vec3 newPos(0.0f, 0.0f, -5.0f);
        projectile.update(0.016f, newPos);

        // Should track the 5 unit movement
        REQUIRE(projectile.distanceTraveled >= 4.9f);
        REQUIRE(projectile.distanceTraveled <= 5.1f);
    }
}

TEST_CASE("ProjectileComponent Lifetime Management", "[Projectile][Component]") {
    SECTION("shouldDestroy returns false when alive") {
        ProjectileComponent projectile = ProjectileComponent::createBullet();
        projectile.initialize(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));

        REQUIRE(projectile.shouldDestroy() == false);
    }

    SECTION("shouldDestroy returns true when time limit exceeded") {
        ProjectileComponent projectile = ProjectileComponent::createBullet();
        projectile.timeToLive = 1.0f;
        projectile.hasTimeLimit = true;
        projectile.initialize(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));

        // Simulate 1.5 seconds passing
        for (int i = 0; i < 100; ++i) {
            projectile.update(0.015f, glm::vec3(0.0f));
        }

        REQUIRE(projectile.shouldDestroy() == true);
    }

    SECTION("shouldDestroy returns true when distance limit exceeded") {
        ProjectileComponent projectile = ProjectileComponent::createBullet();
        projectile.maxDistance = 10.0f;
        projectile.hasDistanceLimit = true;
        projectile.trackTrajectory = true; // Need trajectory tracking for distance
        glm::vec3 startPos(0.0f, 0.0f, 0.0f);
        projectile.initialize(startPos, glm::vec3(0.0f, 0.0f, -1.0f));

        // Simulate traveling 15 units
        glm::vec3 currentPos(0.0f);
        for (int i = 0; i < 10; ++i) {
            currentPos.z -= 1.5f;
            projectile.update(0.016f, currentPos);
        }

        REQUIRE(projectile.shouldDestroy() == true);
    }
}

TEST_CASE("ProjectileComponent Physics", "[Projectile][Component]") {
    SECTION("applyDrag reduces velocity") {
        ProjectileComponent projectile = ProjectileComponent::createPhysicsObject();
        projectile.dragCoefficient = 0.5f;
        projectile.currentVelocity = glm::vec3(10.0f, 0.0f, 0.0f);

        float initialSpeed = glm::length(projectile.currentVelocity);
        projectile.applyDrag(0.1f);
        float finalSpeed = glm::length(projectile.currentVelocity);

        REQUIRE(finalSpeed < initialSpeed);
    }

    SECTION("handleCollision with bounce updates velocity") {
        ProjectileComponent projectile = ProjectileComponent::createPhysicsObject(1.0f, 0.8f);
        projectile.currentVelocity = glm::vec3(0.0f, -10.0f, 0.0f); // Falling down
        projectile.collisionResponse = ProjectileComponent::CollisionResponse::BOUNCE;

        glm::vec3 groundNormal(0.0f, 1.0f, 0.0f);
        projectile.handleCollision(groundNormal, 10.0f);

        // Should bounce upward (y component positive)
        REQUIRE(projectile.currentVelocity.y > 0.0f);
    }

    SECTION("handleCollision with stick stops velocity") {
        ProjectileComponent projectile = ProjectileComponent::createBullet();
        projectile.currentVelocity = glm::vec3(5.0f, 0.0f, 0.0f);
        projectile.collisionResponse = ProjectileComponent::CollisionResponse::STICK;

        projectile.handleCollision(glm::vec3(1.0f, 0.0f, 0.0f), 5.0f);

        REQUIRE(glm::length(projectile.currentVelocity) == Approx(0.0f));
    }
}

TEST_CASE("ProjectileSystem Spawning", "[Projectile][System]") {
    SECTION("spawnProjectile creates entity with components") {
        World world;
        auto& entityManager = world.getEntityManager();

        // Create minimal camera for CameraSystem
        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        cameraTransform.position = glm::vec3(0.0f, 5.0f, 10.0f);
        entityManager.addComponent(cameraEntity, cameraTransform);

        Camera camera;
        camera.aspectRatio = 16.0f / 9.0f;
        entityManager.addComponent(cameraEntity, camera);

        // Create ProjectileSystem (without MaterialSystem for unit test)
        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        glm::vec3 spawnPos(0.0f, 1.0f, 0.0f);
        glm::vec3 direction(0.0f, 0.0f, -1.0f);

        EntityID projectileEntity = projectileSystem->spawnProjectile(
            spawnPos, direction, entityManager,
            ProjectileComponent::Type::BULLET, 50.0f);

        REQUIRE(projectileEntity != INVALID_ENTITY);
        REQUIRE(entityManager.hasComponent<Transform>(projectileEntity));
        REQUIRE(entityManager.hasComponent<ProjectileComponent>(projectileEntity));
        REQUIRE(entityManager.hasComponent<Renderable>(projectileEntity));
        REQUIRE(entityManager.hasComponent<SpatialComponent>(projectileEntity));
    }

    SECTION("spawned projectile has correct Transform") {
        World world;
        auto& entityManager = world.getEntityManager();

        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        cameraTransform.position = glm::vec3(0.0f, 5.0f, 10.0f);
        entityManager.addComponent(cameraEntity, cameraTransform);

        Camera camera;
        camera.aspectRatio = 16.0f / 9.0f;
        entityManager.addComponent(cameraEntity, camera);

        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        glm::vec3 spawnPos(5.0f, 10.0f, -3.0f);
        glm::vec3 direction(0.0f, -1.0f, 0.0f);

        EntityID projectileEntity = projectileSystem->spawnProjectile(
            spawnPos, direction, entityManager,
            ProjectileComponent::Type::ARROW, 20.0f);

        auto& transform = entityManager.getComponent<Transform>(projectileEntity);
        REQUIRE(transform.position.x == Approx(5.0f));
        REQUIRE(transform.position.y == Approx(10.0f));
        REQUIRE(transform.position.z == Approx(-3.0f));
        REQUIRE(transform.scale.x == Approx(0.1f)); // Small projectile
    }

    SECTION("spawned projectile has correct SpatialComponent") {
        World world;
        auto& entityManager = world.getEntityManager();

        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        entityManager.addComponent(cameraEntity, cameraTransform);
        Camera camera;
        entityManager.addComponent(cameraEntity, camera);

        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        EntityID projectileEntity = projectileSystem->spawnProjectile(
            glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), entityManager,
            ProjectileComponent::Type::POKEBALL);

        auto& spatial = entityManager.getComponent<SpatialComponent>(projectileEntity);
        REQUIRE(spatial.spatialLayers == LayerMask::CaptureDevices);
        REQUIRE(spatial.boundingRadius == Approx(0.2f));
        REQUIRE(spatial.behavior == SpatialBehavior::DYNAMIC);
    }
}

TEST_CASE("ProjectileSystem Update and Cleanup", "[Projectile][System]") {
    SECTION("update processes active projectiles") {
        World world;
        auto& entityManager = world.getEntityManager();

        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        entityManager.addComponent(cameraEntity, cameraTransform);
        Camera camera;
        entityManager.addComponent(cameraEntity, camera);

        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        EntityID projectile = projectileSystem->spawnProjectile(
            glm::vec3(0.0f, 10.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            entityManager,
            ProjectileComponent::Type::BULLET);

        // Update projectile for one frame
        projectileSystem->update(0.016f, entityManager);

        // Projectile should still exist and have updated timeSinceSpawn
        REQUIRE(entityManager.hasComponent<ProjectileComponent>(projectile));
        auto& component = entityManager.getComponent<ProjectileComponent>(projectile);
        REQUIRE(component.timeSinceSpawn > 0.0f);
    }

    SECTION("update destroys expired projectiles") {
        World world;
        auto& entityManager = world.getEntityManager();

        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        entityManager.addComponent(cameraEntity, cameraTransform);
        Camera camera;
        entityManager.addComponent(cameraEntity, camera);

        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        // Create bullet with very short lifetime
        EntityID projectile = projectileSystem->spawnProjectile(
            glm::vec3(0.0f, 10.0f, 0.0f), // Spawn above ground
            glm::vec3(0.0f, 0.0f, -1.0f),
            entityManager,
            ProjectileComponent::Type::BULLET);

        auto& component = entityManager.getComponent<ProjectileComponent>(projectile);
        component.timeToLive = 0.05f; // 50ms lifetime

        // Update for longer than lifetime (4 frames at 60fps = 64ms)
        for (int i = 0; i < 4; ++i) {
            projectileSystem->update(0.016f, entityManager);
        }

        // Projectile should be destroyed
        REQUIRE_FALSE(entityManager.hasComponent<ProjectileComponent>(projectile));
    }

    SECTION("getProjectileCount returns correct count") {
        World world;
        auto& entityManager = world.getEntityManager();

        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        entityManager.addComponent(cameraEntity, cameraTransform);
        Camera camera;
        entityManager.addComponent(cameraEntity, camera);

        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        REQUIRE(projectileSystem->getProjectileCount(entityManager) == 0);

        projectileSystem->spawnProjectile(glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                         entityManager, ProjectileComponent::Type::BULLET);
        REQUIRE(projectileSystem->getProjectileCount(entityManager) == 1);

        projectileSystem->spawnProjectile(glm::vec3(1.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                         entityManager, ProjectileComponent::Type::ARROW);
        REQUIRE(projectileSystem->getProjectileCount(entityManager) == 2);
    }

    SECTION("destroyAllProjectiles clears all projectiles") {
        World world;
        auto& entityManager = world.getEntityManager();

        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        entityManager.addComponent(cameraEntity, cameraTransform);
        Camera camera;
        entityManager.addComponent(cameraEntity, camera);

        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        // Spawn 5 projectiles above ground
        for (int i = 0; i < 5; ++i) {
            projectileSystem->spawnProjectile(
                glm::vec3(static_cast<float>(i), 5.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, -1.0f),
                entityManager,
                ProjectileComponent::Type::BULLET);
        }

        REQUIRE(projectileSystem->getProjectileCount(entityManager) == 5);

        projectileSystem->destroyAllProjectiles(entityManager);

        REQUIRE(projectileSystem->getProjectileCount(entityManager) == 0);
    }
}

TEST_CASE("ProjectileSystem Performance", "[Projectile][System][Performance]") {
    SECTION("performance stats track update time") {
        World world;
        auto& entityManager = world.getEntityManager();

        auto cameraSystem = std::make_unique<CameraSystem>();
        EntityID cameraEntity = entityManager.createEntity();
        Transform cameraTransform;
        entityManager.addComponent(cameraEntity, cameraTransform);
        Camera camera;
        entityManager.addComponent(cameraEntity, camera);

        auto projectileSystem = std::make_unique<ProjectileSystem>(cameraSystem.get(), nullptr);

        // Spawn 10 projectiles above ground
        for (int i = 0; i < 10; ++i) {
            projectileSystem->spawnProjectile(
                glm::vec3(static_cast<float>(i), 5.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, -1.0f),
                entityManager,
                ProjectileComponent::Type::BULLET);
        }

        projectileSystem->update(0.016f, entityManager);

        auto stats = projectileSystem->getPerformanceStats();
        REQUIRE(stats.activeProjectiles == 10);
        REQUIRE(stats.projectilesSpawned == 10);
        REQUIRE(stats.lastFrameUpdateTime >= 0.0f);
    }
}
