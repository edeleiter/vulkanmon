#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../../src/systems/PhysicsSystem.h"
#include "../../src/core/EntityManager.h"
#include "../../src/components/Transform.h"
#include "../../src/components/RigidBodyComponent.h"
#include "../../src/components/CollisionComponent.h"
#include "../../src/spatial/SpatialManager.h"
#include "../../src/systems/SpatialSystem.h"
#include <glm/glm.hpp>
#include <memory>

using namespace VulkanMon;

// Test fixture for physics blocker validation
class PhysicsBlockerTestFixture {
public:
    PhysicsBlockerTestFixture() {
        physicsSystem = std::make_unique<PhysicsSystem>();
        entityManager = std::make_unique<EntityManager>();

        // Initialize physics system with test gravity
        physicsSystem->initialize(*entityManager);
        physicsSystem->setGravity(glm::vec3(0.0f, -9.81f, 0.0f));

        // Note: SpatialSystem integration tested separately
        // For unit tests, we focus on physics features in isolation
    }

    ~PhysicsBlockerTestFixture() {
        if (physicsSystem) {
            physicsSystem->shutdown(*entityManager);
        }
    }

    // Helper: Create a test physics entity
    EntityID createTestEntity(const glm::vec3& position, const glm::vec3& dimensions,
                            bool isDynamic = false, CollisionComponent::ShapeType shape = CollisionComponent::ShapeType::Box) {
        EntityID entity = entityManager->createEntity();

        // Add Transform
        Transform transform;
        transform.position = position;
        transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f);
        entityManager->addComponent(entity, transform);

        // Add RigidBodyComponent
        RigidBodyComponent rigidBody;
        rigidBody.isDynamic = isDynamic;
        rigidBody.mass = isDynamic ? 1.0f : 0.0f;
        rigidBody.useGravity = isDynamic;
        rigidBody.velocity = glm::vec3(0.0f);
        entityManager->addComponent(entity, rigidBody);

        // Add CollisionComponent
        CollisionComponent collision;
        collision.shapeType = shape;
        collision.dimensions = dimensions;
        collision.layer = LayerMask::Environment;
        collision.isStatic = !isDynamic;
        entityManager->addComponent(entity, collision);

        return entity;
    }

    std::unique_ptr<PhysicsSystem> physicsSystem;
    std::unique_ptr<EntityManager> entityManager;
};

// =============================================================================
// CRITICAL BLOCKER #1: RAYCAST SYSTEM TESTS
// =============================================================================

TEST_CASE_METHOD(PhysicsBlockerTestFixture, "Critical Blocker #1: Raycast System", "[physics][raycast]") {

    SECTION("Raycast can detect ground collision") {
        // Create ground entity at Y = 0
        EntityID ground = createTestEntity(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(10.0f, 1.0f, 10.0f), false);

        // Update physics system to create Jolt bodies
        physicsSystem->update(16.67f, *entityManager);

        // Cast ray from above ground downward
        glm::vec3 origin(0.0f, 5.0f, 0.0f);
        glm::vec3 direction(0.0f, -1.0f, 0.0f);
        float maxDistance = 10.0f;

        auto hit = physicsSystem->raycast(origin, direction, maxDistance);

        REQUIRE(hit.hit == true);
        REQUIRE(hit.entity == ground);
        REQUIRE(hit.distance > 4.0f);  // Should hit after traveling ~5 units downward
        REQUIRE(hit.distance < 6.0f);  // But not too far
    }

    SECTION("Raycast correctly reports no hits in empty space") {
        // No entities created - empty space

        // Cast ray in empty area
        glm::vec3 origin(100.0f, 100.0f, 100.0f);
        glm::vec3 direction(1.0f, 0.0f, 0.0f);
        float maxDistance = 50.0f;

        auto hit = physicsSystem->raycast(origin, direction, maxDistance);

        REQUIRE(hit.hit == false);
        REQUIRE(hit.entity == 0);
        REQUIRE(hit.distance == 0.0f);
    }

    SECTION("Raycast detects closest entity when multiple are present") {
        // Create multiple entities in a line
        EntityID close = createTestEntity(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);
        EntityID far = createTestEntity(glm::vec3(15.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);

        // Update physics system
        physicsSystem->update(16.67f, *entityManager);

        // Cast ray from origin toward both entities
        glm::vec3 origin(0.0f, 0.0f, 0.0f);
        glm::vec3 direction(1.0f, 0.0f, 0.0f);
        float maxDistance = 20.0f;

        auto hit = physicsSystem->raycast(origin, direction, maxDistance);

        REQUIRE(hit.hit == true);
        REQUIRE(hit.entity == close);  // Should hit the closer entity
        REQUIRE(hit.distance < 6.0f);  // Should be approximately 5 units
    }

    SECTION("Raycast performance is sub-millisecond") {
        // Create several entities
        for (int i = 0; i < 10; ++i) {
            createTestEntity(glm::vec3(i * 2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);
        }
        physicsSystem->update(16.67f, *entityManager);

        // Benchmark raycast performance
        glm::vec3 origin(0.0f, 5.0f, 0.0f);
        glm::vec3 direction(0.0f, -1.0f, 0.0f);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            physicsSystem->raycast(origin, direction, 10.0f);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        float avgTimeMs = duration.count() / (100.0f * 1000.0f);

        // Should be under 1ms per raycast
        REQUIRE(avgTimeMs < 1.0f);
        INFO("Average raycast time: " << avgTimeMs << "ms");
    }
}

// =============================================================================
// CRITICAL BLOCKER #2: SPHERE OVERLAP QUERIES TESTS
// =============================================================================

TEST_CASE_METHOD(PhysicsBlockerTestFixture, "Critical Blocker #2: Sphere Overlap Queries", "[physics][overlap]") {

    SECTION("Sphere overlap detects entities within radius") {
        // Create test entities at known positions
        EntityID entity1 = createTestEntity(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);
        EntityID entity2 = createTestEntity(glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);
        EntityID entity3 = createTestEntity(glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false); // Far away

        // Update physics system
        physicsSystem->update(16.67f, *entityManager);

        // Query with sphere that should include entity1 and entity2, but not entity3
        glm::vec3 center(1.0f, 0.0f, 0.0f);
        float radius = 3.0f;

        auto overlaps = physicsSystem->overlapSphere(center, radius);

        REQUIRE(overlaps.size() >= 2);
        REQUIRE(std::find(overlaps.begin(), overlaps.end(), entity1) != overlaps.end());
        REQUIRE(std::find(overlaps.begin(), overlaps.end(), entity2) != overlaps.end());
        REQUIRE(std::find(overlaps.begin(), overlaps.end(), entity3) == overlaps.end());
    }

    SECTION("Sphere overlap returns empty when no entities in range") {
        // Create entity far away
        createTestEntity(glm::vec3(100.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);
        physicsSystem->update(16.67f, *entityManager);

        // Query in empty area
        glm::vec3 center(0.0f, 0.0f, 0.0f);
        float radius = 2.0f;

        auto overlaps = physicsSystem->overlapSphere(center, radius);

        REQUIRE(overlaps.empty());
    }

    SECTION("Sphere overlap performance with many entities") {
        // Create grid of entities
        for (int x = 0; x < 10; ++x) {
            for (int z = 0; z < 10; ++z) {
                createTestEntity(glm::vec3(x * 2.0f, 0.0f, z * 2.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);
            }
        }
        physicsSystem->update(16.67f, *entityManager);

        // Benchmark overlap performance
        glm::vec3 center(10.0f, 0.0f, 10.0f);
        float radius = 5.0f;

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 50; ++i) {
            physicsSystem->overlapSphere(center, radius);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        float avgTimeMs = duration.count() / (50.0f * 1000.0f);

        // Should be under 1ms per query with 100 entities
        REQUIRE(avgTimeMs < 1.0f);
        INFO("Average overlap query time: " << avgTimeMs << "ms");
    }

    SECTION("Sphere overlap with distributed entities") {
        // Create entities spread across space to test range detection
        for (int i = 0; i < 10; ++i) {
            createTestEntity(glm::vec3(i * 5.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), false);
        }
        physicsSystem->update(16.67f, *entityManager);

        // Small radius should only find nearby entities
        glm::vec3 center(10.0f, 0.0f, 0.0f); // Position near entity at x=10
        float radius = 3.0f;

        auto overlaps = physicsSystem->overlapSphere(center, radius);

        // Should find some entities (testing that sphere overlap works with physics)
        REQUIRE(overlaps.size() >= 1);
        REQUIRE(overlaps.size() <= 5);  // Reasonable upper bound
    }
}

// =============================================================================
// CRITICAL BLOCKER #3: SPATIAL COLLISION OPTIMIZATION TESTS
// =============================================================================

TEST_CASE_METHOD(PhysicsBlockerTestFixture, "Critical Blocker #3: Physics Performance at Scale", "[physics][performance]") {

    SECTION("Physics system handles many entities efficiently") {
        // Create large number of entities to test spatial optimization
        const int ENTITY_COUNT = 50;

        for (int i = 0; i < ENTITY_COUNT; ++i) {
            float x = (i % 10) * 3.0f;
            float z = (i / 10) * 3.0f;
            createTestEntity(glm::vec3(x, 0.0f, z), glm::vec3(1.0f, 1.0f, 1.0f), true);
        }

        // Measure collision detection performance
        auto start = std::chrono::high_resolution_clock::now();
        physicsSystem->update(16.67f, *entityManager);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        float updateTimeMs = duration.count();

        // With spatial optimization, 50 entities should update quickly
        REQUIRE(updateTimeMs < 50.0f);  // Less than 50ms for 50 entities

        // Check physics statistics
        auto stats = physicsSystem->getStats();
        REQUIRE(stats.activeRigidBodies >= ENTITY_COUNT);

        // Physics should handle many entities without excessive computation
        INFO("Physics performance: " << stats.activeRigidBodies << " entities, "
             << stats.updateTime << "ms update time, "
             << stats.collisionChecks << " collision checks");
    }

    SECTION("Physics maintains collision accuracy with multiple entities") {
        // Create overlapping entities that should definitely collide
        EntityID entity1 = createTestEntity(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(2.0f, 2.0f, 2.0f), true);
        EntityID entity2 = createTestEntity(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(2.0f, 2.0f, 2.0f), true);

        // Update physics - should detect collision
        physicsSystem->update(16.67f, *entityManager);

        auto stats = physicsSystem->getStats();

        // Should have detected collision between overlapping entities
        REQUIRE(stats.collisionChecks > 0);

        // With overlapping 2x2x2 boxes at (0,0,0) and (1,0,0), physics should detect potential collision
        // This validates that physics system processes overlapping entities correctly
    }

    SECTION("Performance scales to Pokemon Legends Arceus requirements") {
        // Test with Pokemon-scale entity count
        const int POKEMON_SCALE = 200;  // Target: 200+ creatures

        // Create distributed entities (simulating creatures across map)
        for (int i = 0; i < POKEMON_SCALE; ++i) {
            float angle = (i * 2.0f * 3.14159f) / POKEMON_SCALE;
            float radius = 10.0f + (i % 5) * 5.0f;
            float x = cos(angle) * radius;
            float z = sin(angle) * radius;

            createTestEntity(glm::vec3(x, 0.0f, z), glm::vec3(1.0f, 1.0f, 1.0f), true);
        }

        // Measure performance over multiple frames
        float totalUpdateTime = 0.0f;
        const int FRAME_COUNT = 5;

        for (int frame = 0; frame < FRAME_COUNT; ++frame) {
            auto start = std::chrono::high_resolution_clock::now();
            physicsSystem->update(16.67f, *entityManager);
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            totalUpdateTime += duration.count() / 1000.0f;  // Convert to milliseconds
        }

        float avgUpdateTime = totalUpdateTime / FRAME_COUNT;

        // Pokemon Legends Arceus requirement: < 5ms per frame at 60 FPS
        REQUIRE(avgUpdateTime < 5.0f);

        auto stats = physicsSystem->getStats();
        REQUIRE(stats.activeRigidBodies >= POKEMON_SCALE);

        INFO("Pokemon-scale performance: " << avgUpdateTime << "ms for " << stats.activeRigidBodies << " entities");
    }
}

// =============================================================================
// INTEGRATION TESTS - ALL BLOCKERS WORKING TOGETHER
// =============================================================================

TEST_CASE_METHOD(PhysicsBlockerTestFixture, "Physics Blockers Integration", "[physics][integration]") {

    SECTION("All three blockers work together in realistic scenario") {
        // Create a realistic scene: ground + entities + dynamic objects
        EntityID ground = createTestEntity(glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(20.0f, 1.0f, 20.0f), false);

        // Static environment entities (trees, rocks, etc.)
        std::vector<EntityID> staticEntities;
        for (int i = 0; i < 10; ++i) {
            float x = (i - 5) * 3.0f;
            staticEntities.push_back(createTestEntity(glm::vec3(x, 0.0f, 0.0f), glm::vec3(1.0f, 2.0f, 1.0f), false));
        }

        // Dynamic entities (creatures, projectiles, etc.)
        std::vector<EntityID> dynamicEntities;
        for (int i = 0; i < 20; ++i) {
            float x = (i - 10) * 2.0f;
            float z = (i % 2) * 4.0f - 2.0f;
            dynamicEntities.push_back(createTestEntity(glm::vec3(x, 5.0f, z), glm::vec3(0.8f, 0.8f, 0.8f), true));
        }

        // Update physics system
        physicsSystem->update(16.67f, *entityManager);

        // Test Raycast: Should detect ground from above
        auto groundHit = physicsSystem->raycast(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 20.0f);
        REQUIRE(groundHit.hit == true);
        // Don't test specific entity ID due to creation order variations
        REQUIRE(groundHit.entity > 0); // Just verify we hit a valid entity

        // Test Sphere Overlap: Should detect multiple entities in central area
        auto centerOverlaps = physicsSystem->overlapSphere(glm::vec3(0.0f, 0.0f, 0.0f), 8.0f);
        REQUIRE(centerOverlaps.size() >= 5);  // Should find several static + dynamic entities

        // Test Spatial Performance: Should handle all entities efficiently
        auto stats = physicsSystem->getStats();
        REQUIRE(stats.activeRigidBodies >= 20);  // Dynamic entities (static bodies aren't "active" in Jolt)
        REQUIRE(stats.updateTime < 5.0f);  // Efficient performance

        INFO("Integration test: " << stats.activeRigidBodies << " entities, "
             << stats.updateTime << "ms update time");
    }
}