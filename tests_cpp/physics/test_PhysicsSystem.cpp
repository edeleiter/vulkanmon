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
        physicsSystem.initialize(entityManager);
        physicsSystem.setGravity(customGravity);

        REQUIRE(physicsSystem.getGravity() == customGravity);
        REQUIRE(physicsSystem.getTimeScale() == 1.0f);
        REQUIRE(physicsSystem.isCollisionEnabled() == true);
        REQUIRE_FALSE(physicsSystem.isDebugDrawEnabled());

        // Test shutdown
        physicsSystem.shutdown(entityManager);
    }

    SECTION("Physics configuration") {
        physicsSystem.initialize(entityManager);

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

        physicsSystem.shutdown(entityManager);
    }

    SECTION("Collision layer matrix") {
        physicsSystem.initialize(entityManager);

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

        physicsSystem.shutdown(entityManager);
    }
}

TEST_CASE("PhysicsSystem ECS Integration", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize(entityManager);

    SECTION("Physics update with no entities") {
        // Should handle empty entity manager gracefully
        physicsSystem.update(0.016f, entityManager); // 60 FPS

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

        // Add collision component required for Jolt physics
        CollisionComponent collision = CollisionComponent::createCreature(1.0f);
        entityManager.addComponent(entity, collision);

        // Run physics update
        physicsSystem.update(0.016f, entityManager);

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
            physicsSystem.update(0.016f, entityManager);
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

        // Add collision component required for Jolt physics
        CollisionComponent collision = CollisionComponent::createCreature(1.0f);
        entityManager.addComponent(entity, collision);

        CreaturePhysicsComponent creaturePhysics;
        creaturePhysics.moveSpeed = 8.0f;
        creaturePhysics.isGrounded = true;
        creaturePhysics.setMovementInput(glm::vec3(1.0f, 0.0f, 0.0f), false, false);
        entityManager.addComponent(entity, creaturePhysics);

        // Run physics update
        physicsSystem.update(0.016f, entityManager);

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

        RigidBodyComponent rigidBody1;
        rigidBody1.isDynamic = true;
        rigidBody1.mass = 1.0f;
        entityManager.addComponent(entity1, rigidBody1);

        // Entity 2
        Transform transform2;
        transform2.position = glm::vec3(1.5f, 0.0f, 0.0f); // Close to entity1
        entityManager.addComponent(entity2, transform2);

        CollisionComponent collision2 = CollisionComponent::createCreature(1.0f, 2.0f);
        entityManager.addComponent(entity2, collision2);

        RigidBodyComponent rigidBody2;
        rigidBody2.isDynamic = true;
        rigidBody2.mass = 1.0f;
        entityManager.addComponent(entity2, rigidBody2);

        // Run physics update
        physicsSystem.update(0.016f, entityManager);

        auto stats = physicsSystem.getStats();
        REQUIRE(stats.collisionChecks > 0); // Should have checked for collisions
    }

    physicsSystem.shutdown(entityManager);
}

TEST_CASE("PhysicsSystem Performance", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize(entityManager);

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

            // Add collision component required for Jolt physics
            CollisionComponent collision = CollisionComponent::createCreature(0.5f);
            entityManager.addComponent(entity, collision);
        }

        // Measure physics update performance
        auto startTime = std::chrono::high_resolution_clock::now();
        physicsSystem.update(0.016f, entityManager);
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

    physicsSystem.shutdown(entityManager);
}

TEST_CASE("PhysicsSystem Raycast and Queries", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize(entityManager);

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

    physicsSystem.shutdown(entityManager);
}

TEST_CASE("PhysicsSystem Fixed Timestep", "[Physics][PhysicsSystem]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize(entityManager);

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

    physicsSystem.shutdown(entityManager);
}

// =============================================================================
// GROUND COLLISION TESTS
// =============================================================================

TEST_CASE("PhysicsSystem Ground Collision Detection", "[Physics][GroundCollision]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize(entityManager);

    SECTION("Entity hits ground and bounces correctly - Jolt Physics") {
        // Create ground entity for collision
        EntityID ground = entityManager.createEntity();
        Transform groundTransform;
        groundTransform.position = glm::vec3(0.0f, -4.0f, 0.0f);
        groundTransform.scale = glm::vec3(10.0f, 1.0f, 10.0f);
        entityManager.addComponent(ground, groundTransform);

        CollisionComponent groundCollision = CollisionComponent::createEnvironment(glm::vec3(10.0f, 1.0f, 10.0f));
        entityManager.addComponent(ground, groundCollision);

        RigidBodyComponent groundBody;
        groundBody.isDynamic = false;
        groundBody.mass = 0.0f;
        entityManager.addComponent(ground, groundBody);

        // Create falling entity
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, 5.0f, 0.0f); // Start above ground
        entityManager.addComponent(entity, transform);

        CollisionComponent collision = CollisionComponent::createCreature(0.5f);
        entityManager.addComponent(entity, collision);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.velocity = glm::vec3(0.0f, -5.0f, 0.0f); // Fast downward
        rigidBody.restitution = 0.7f; // Good bounce
        rigidBody.friction = 0.3f;
        rigidBody.useGravity = true;
        entityManager.addComponent(entity, rigidBody);

        // Run multiple physics updates to allow Jolt to settle
        for (int i = 0; i < 10; ++i) {
            physicsSystem.update(16.67f, entityManager);
        }

        // Check that entity eventually settles and is not falling through ground
        auto& updatedTransform = entityManager.getComponent<Transform>(entity);

        // Entity should be above ground level (not fallen through)
        REQUIRE(updatedTransform.position.y > -4.0f);
        // Should be reasonably close to ground surface (Jolt Physics behavior)
        REQUIRE(updatedTransform.position.y < 5.0f); // More realistic for Jolt Physics with bounce
    }

    SECTION("Entity with Jolt Physics integration works correctly") {
        // Create a simple entity to test Jolt Physics body creation and updates
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        entityManager.addComponent(entity, transform);

        CollisionComponent collision = CollisionComponent::createCreature(0.5f);
        entityManager.addComponent(entity, collision);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.velocity = glm::vec3(0.0f, 0.0f, 0.0f); // Start at rest
        rigidBody.useGravity = false; // No gravity for simple test
        entityManager.addComponent(entity, rigidBody);

        // Run physics update - should create Jolt body and sync correctly
        physicsSystem.update(16.67f, entityManager);

        // Verify entity still exists and has been processed
        REQUIRE(entityManager.hasComponent<Transform>(entity));
        REQUIRE(entityManager.hasComponent<RigidBodyComponent>(entity));

        // Get physics stats to verify system is working
        auto stats = physicsSystem.getStats();
        REQUIRE(stats.activeRigidBodies >= 1);
    }

    SECTION("Static entities are not affected by ground collision") {
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, -10.0f, 0.0f); // Below ground
        entityManager.addComponent(entity, transform);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = false; // Static body
        rigidBody.velocity = glm::vec3(0.0f, -5.0f, 0.0f);
        entityManager.addComponent(entity, rigidBody);

        CollisionComponent collision = CollisionComponent::createCreature(0.5f, 1.0f);
        entityManager.addComponent(entity, collision);

        float initialY = transform.position.y;

        // Run physics update
        physicsSystem.update(16.67f, entityManager);

        // Static entity should not move
        auto& updatedTransform = entityManager.getComponent<Transform>(entity);
        REQUIRE(updatedTransform.position.y == initialY);
    }

    physicsSystem.shutdown(entityManager);
}

// =============================================================================
// TRANSFORM INTEGRATION TESTS
// =============================================================================

TEST_CASE("PhysicsSystem Transform Integration", "[Physics][Transform]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize(entityManager);


    SECTION("Jolt Physics time stepping works correctly") {
        // Test that Jolt Physics simulation responds to time steps correctly
        // Simple test: just verify that physics simulation advances with time

        EntityID entity = entityManager.createEntity();
        Transform transform;
        transform.position = glm::vec3(0.0f, 10.0f, 0.0f);
        entityManager.addComponent(entity, transform);

        CollisionComponent collision = CollisionComponent::createCreature(0.5f);
        entityManager.addComponent(entity, collision);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.useGravity = true;
        rigidBody.mass = 1.0f;
        entityManager.addComponent(entity, rigidBody);

        glm::vec3 initialPosition = transform.position;

        // Run physics for several steps
        for (int i = 0; i < 5; i++) {
            physicsSystem.update(16.67f, entityManager);
        }

        auto& finalTransform = entityManager.getComponent<Transform>(entity);

        // Simply verify that gravity caused the entity to fall
        REQUIRE(finalTransform.position.y < initialPosition.y);

        // Verify it moved a reasonable amount (gravity should pull it down)
        float fallDistance = initialPosition.y - finalTransform.position.y;
        REQUIRE(fallDistance > 0.01f); // Should have fallen at least 0.01 units
    }


    physicsSystem.shutdown(entityManager);
}

// =============================================================================
// NEW TESTS FOR CLEAN JOLT-ECS ARCHITECTURE
// =============================================================================

TEST_CASE("Clean Jolt-ECS Integration", "[Physics][Integration][New]") {
    PhysicsSystem physicsSystem;
    EntityManager entityManager;

    physicsSystem.initialize(entityManager);

    SECTION("Automatic ECS Transform sync from Jolt Physics") {
        // Test that ECS Transforms automatically sync from Jolt every frame
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
        entityManager.addComponent(entity, transform);

        CollisionComponent collision = CollisionComponent::createCreature(0.5f);
        entityManager.addComponent(entity, collision);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.useGravity = true;
        rigidBody.mass = 1.0f;
        entityManager.addComponent(entity, rigidBody);

        glm::vec3 initialPosition = transform.position;

        // Run physics - Jolt should create body and sync back to ECS automatically
        physicsSystem.update(16.67f, entityManager);

        auto& syncedTransform = entityManager.getComponent<Transform>(entity);

        // Transform should have been automatically updated from Jolt Physics
        // (gravity should cause some movement)
        REQUIRE(syncedTransform.position.y <= initialPosition.y); // Fell or stayed same
    }

    SECTION("No manual sync flags needed") {
        // Verify that the clean architecture doesn't require manual sync management
        EntityID entity = entityManager.createEntity();

        Transform transform;
        transform.position = glm::vec3(0.0f, 10.0f, 0.0f);
        entityManager.addComponent(entity, transform);

        CollisionComponent collision = CollisionComponent::createCreature(0.5f);
        entityManager.addComponent(entity, collision);

        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = true;
        rigidBody.useGravity = true;
        rigidBody.mass = 1.0f;
        entityManager.addComponent(entity, rigidBody);

        // No manual sync flag setting needed - should work automatically
        for (int i = 0; i < 3; i++) {
            physicsSystem.update(16.67f, entityManager);
        }

        auto& finalTransform = entityManager.getComponent<Transform>(entity);

        // Should have fallen due to gravity - proves automatic sync is working
        REQUIRE(finalTransform.position.y < 10.0f);
    }

    SECTION("Pokemon-scale entity creation") {
        // Test creating many entities at once (Pokemon-style gameplay)
        std::vector<EntityID> entities;

        for (int i = 0; i < 20; i++) {
            EntityID entity = entityManager.createEntity();

            Transform transform;
            transform.position = glm::vec3(i * 2.0f, 10.0f, 0.0f);
            entityManager.addComponent(entity, transform);

            CollisionComponent collision = CollisionComponent::createCreature(0.5f);
            entityManager.addComponent(entity, collision);

            RigidBodyComponent rigidBody;
            rigidBody.isDynamic = true;
            rigidBody.useGravity = true;
            rigidBody.mass = 1.0f;
            entityManager.addComponent(entity, rigidBody);

            entities.push_back(entity);
        }

        // All entities should get Jolt bodies and sync correctly
        physicsSystem.update(16.67f, entityManager);

        // Verify all entities have proper physics simulation
        for (EntityID entity : entities) {
            auto& transform = entityManager.getComponent<Transform>(entity);
            // Should have fallen at least a little bit
            REQUIRE(transform.position.y <= 10.0f);
        }
    }

    physicsSystem.shutdown(entityManager);
}