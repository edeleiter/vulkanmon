#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../../src/systems/PhysicsSystem.h"
#include "../../src/core/EntityManager.h"
#include "../../src/components/RigidBodyComponent.h"
#include "../../src/components/CollisionComponent.h"
#include "../../src/components/CreaturePhysicsComponent.h"
#include "../../src/components/Transform.h"

using namespace VulkanMon;

// =============================================================================
// PHYSICS SYSTEM TESTS
// =============================================================================

TEST_CASE("PhysicsSystem Basic Functionality", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    SECTION("Physics system initialization") {
        glm::vec3 customGravity(0.0f, -12.0f, 0.0f);
        physicsSystem.initialize(customGravity);

        REQUIRE(physicsSystem.getGravity() == customGravity);
        REQUIRE(physicsSystem.getTimeScale() == 1.0f);
        REQUIRE(physicsSystem.isCollisionEnabled() == true);
        REQUIRE_FALSE(physicsSystem.isDebugDrawEnabled());

        // Test shutdown
        physicsSystem.shutdown();
    }

    SECTION("Physics configuration") {
        physicsSystem.initialize();

        // Test gravity modification
        glm::vec3 newGravity(1.0f, -5.0f, 2.0f);
        physicsSystem.setGravity(newGravity);
        REQUIRE(physicsSystem.getGravity() == newGravity);

        // Test time scale
        physicsSystem.setTimeScale(0.5f);
        REQUIRE(physicsSystem.getTimeScale() == Catch::Approx(0.5f));

        // Test collision toggle
        physicsSystem.enableCollisionDetection(false);
        REQUIRE_FALSE(physicsSystem.isCollisionEnabled());

        physicsSystem.shutdown();
    }

    SECTION("Collision layer matrix") {
        physicsSystem.initialize();

        // Test layer collision setup
        uint32_t layer1 = 1;
        uint32_t layer2 = 2;

        // Initially, all layers should collide
        REQUIRE(physicsSystem.shouldLayersCollide(layer1, layer2));

        // Disable collision between layers
        physicsSystem.setCollisionMatrix(layer1, layer2, false);
        REQUIRE_FALSE(physicsSystem.shouldLayersCollide(layer1, layer2));

        // Re-enable collision
        physicsSystem.setCollisionMatrix(layer1, layer2, true);
        REQUIRE(physicsSystem.shouldLayersCollide(layer1, layer2));

        physicsSystem.shutdown();
    }
}

TEST_CASE("PhysicsSystem ECS Integration", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize();

    SECTION("Physics update with no entities") {
        // Should handle empty entity manager gracefully
        physicsSystem.update(entityManager, 0.016f); // 60 FPS

        auto stats = physicsSystem.getStats();
        REQUIRE(stats.activeRigidBodies == 0);
        REQUIRE(stats.collisionChecks == 0);
        REQUIRE(stats.updateTime >= 0.0f);
    }

    SECTION("Physics update with rigid body entities") {
        // Create test entity with physics components
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, 10.0f, 0.0f);
        transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f);
        entityManager.addComponent(entity, transform);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.mass = 5.0f;
        rigidBody.useGravity = true;
        rigidBody.velocity = glm::vec3(0.0f);
        entityManager.addComponent(entity, rigidBody);

        // Run physics update
        physicsSystem.update(entityManager, 0.016f);

        auto stats = physicsSystem.getStats();
        REQUIRE(stats.activeRigidBodies == 1);

        // Check that physics update processed the entity
        auto& updatedRigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        // The physics system should have processed this rigid body
        // Since we apply and then clear forces, and integrate velocity,
        // we should see some change. Let's verify forces were applied
        // by running multiple frames to see accumulated effect

        float initialY = updatedRigidBody.velocity.y;

        // Run a few more physics updates to see gravity accumulate
        for (int i = 0; i < 5; ++i) {
            physicsSystem.update(entityManager, 0.016f);
        }

        auto& finalRigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        REQUIRE(finalRigidBody.velocity.y < initialY); // Should be falling more
    }

    SECTION("Physics update with creature physics") {
        // Create test entity with creature physics
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        entityManager.addComponent(entity, transform);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.mass = 2.0f;
        entityManager.addComponent(entity, rigidBody);

        CreaturePhysicsComponent creaturePhysics;
        creaturePhysics.moveSpeed = 8.0f;
        creaturePhysics.isGrounded = true;
        creaturePhysics.setMovementInput(glm::vec3(1.0f, 0.0f, 0.0f), false, false);
        entityManager.addComponent(entity, creaturePhysics);

        // Run physics update
        physicsSystem.update(entityManager, 0.016f);

        // Check that movement forces were applied
        auto& updatedRigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        REQUIRE(glm::length(updatedRigidBody.force) > 0.0f);
    }

    SECTION("Collision detection with multiple entities") {
        // Create two entities with collision components
        EntityID entity1 = entityManager.createEntity();
        EntityID entity2 = entityManager.createEntity();

        // Entity 1
        Transform transform1;
        transform1.position = glm::vec3(0.0f, 0.0f, 0.0f);
        entityManager.addComponent(entity1, transform1);

        CollisionComponent collision1 = CollisionComponent::createCreature(1.0f, 2.0f);
        entityManager.addComponent(entity1, collision1);

        // Entity 2
        Transform transform2;
        transform2.position = glm::vec3(1.5f, 0.0f, 0.0f); // Close to entity1
        entityManager.addComponent(entity2, transform2);

        CollisionComponent collision2 = CollisionComponent::createCreature(1.0f, 2.0f);
        entityManager.addComponent(entity2, collision2);

        // Run physics update
        physicsSystem.update(entityManager, 0.016f);

        auto stats = physicsSystem.getStats();
        REQUIRE(stats.collisionChecks > 0); // Should have checked for collisions
    }

    physicsSystem.shutdown();
}

TEST_CASE("PhysicsSystem Performance", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize();

    SECTION("Performance with many rigid bodies") {
        // Create multiple physics entities
        const int NUM_ENTITIES = 50;

        for (int i = 0; i < NUM_ENTITIES; ++i) {
            EntityID entity = entityManager.createEntity();

            Transform transform;
            transform.position = glm::vec3(i * 2.0f, 0.0f, 0.0f);
            entityManager.addComponent(entity, transform);

            RigidBodyComponent rigidBody;
            rigidBody.isDynamic = true;
            rigidBody.mass = 1.0f + i * 0.1f;
            rigidBody.useGravity = true;
            entityManager.addComponent(entity, rigidBody);
        }

        // Measure physics update performance
        auto startTime = std::chrono::high_resolution_clock::now();
        physicsSystem.update(entityManager, 0.016f);
        auto endTime = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        float updateTimeMs = duration.count() / 1000.0f;

        auto stats = physicsSystem.getStats();
        REQUIRE(stats.activeRigidBodies == NUM_ENTITIES);
        REQUIRE(stats.updateTime >= 0.0f);
        REQUIRE(updateTimeMs < 10.0f); // Should complete within 10ms for 50 entities

        // Verify statistics are reasonable
        REQUIRE(stats.averageVelocity >= 0.0f);
        REQUIRE(stats.collisionChecks >= 0);
    }

    physicsSystem.shutdown();
}

TEST_CASE("PhysicsSystem Raycast and Queries", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;

    physicsSystem.initialize();

    SECTION("Raycast functionality") {
        glm::vec3 origin(0.0f, 0.0f, 0.0f);
        glm::vec3 direction(1.0f, 0.0f, 0.0f);
        float maxDistance = 10.0f;

        auto hit = physicsSystem.raycast(origin, direction, maxDistance);

        // Raycast is stub implementation for now
        REQUIRE_FALSE(hit.hit);
        REQUIRE(hit.entity == 0);
        REQUIRE(hit.distance == 0.0f);
    }

    SECTION("Sphere overlap query") {
        glm::vec3 center(0.0f, 0.0f, 0.0f);
        float radius = 5.0f;

        auto results = physicsSystem.overlapSphere(center, radius);

        // Overlap is stub implementation for now
        REQUIRE(results.empty());
    }

    physicsSystem.shutdown();
}

TEST_CASE("PhysicsSystem Fixed Timestep", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize();

    SECTION("Fixed timestep accumulation") {
        // Create test entity
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        entityManager.addComponent(entity, transform);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.mass = 1.0f;
        rigidBody.useGravity = true;
        entityManager.addComponent(entity, rigidBody);

        // Run multiple fixed updates
        for (int i = 0; i < 5; ++i) {
            physicsSystem.fixedUpdate(entityManager, 0.016f);
        }

        // Should complete without errors
        auto stats = physicsSystem.getStats();
        REQUIRE(stats.updateTime >= 0.0f);
    }

    physicsSystem.shutdown();
}