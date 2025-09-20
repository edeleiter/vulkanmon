#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../../src/spatial/SpatialManager.h"
#include "../../src/components/SpatialComponent.h"
#include "../../src/systems/SpatialSystem.h"
#include "../../src/core/World.h"
#include "../../src/components/Transform.h"
#include <random>

using namespace VulkanMon;

TEST_CASE("SpatialSystem Grid Operations", "[spatial]") {
    // Create world with spatial system
    BoundingBox worldBounds(glm::vec3(-100, -100, -100), glm::vec3(100, 100, 100));
    World world;
    auto spatialSystem = world.addSystem<SpatialSystem>(worldBounds);

    SECTION("Basic radius query") {
        // Create test entities
        auto e1 = world.createEntity();
        auto e2 = world.createEntity();
        auto e3 = world.createEntity();

        // Add Transform and SpatialComponent to each entity
        Transform t1;
        t1.position = glm::vec3(0, 0, 0);
        world.addComponent(e1, t1);
        SpatialComponent s1;
        s1.boundingRadius = 1.0f;
        s1.markDirty();
        world.addComponent(e1, s1);

        Transform t2;
        t2.position = glm::vec3(5, 0, 0);
        world.addComponent(e2, t2);
        SpatialComponent s2;
        s2.boundingRadius = 1.0f;
        s2.markDirty();
        world.addComponent(e2, s2);

        Transform t3;
        t3.position = glm::vec3(20, 0, 0);
        world.addComponent(e3, t3);
        SpatialComponent s3;
        s3.boundingRadius = 1.0f;
        s3.markDirty();
        world.addComponent(e3, s3);

        // Update spatial system to register entities
        spatialSystem->update(0.016f, world.getEntityManager());

        // Query with radius 10 from origin
        auto results = spatialSystem->queryRadius(glm::vec3(0, 0, 0), 10.0f);

        REQUIRE(results.size() >= 2);

        // Verify that e1 and e2 are found (within radius), e3 is not
        bool foundE1 = false, foundE2 = false, foundE3 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
            if (entity == e2) foundE2 = true;
            if (entity == e3) foundE3 = true;
        }

        CHECK(foundE1);
        CHECK(foundE2);
        CHECK_FALSE(foundE3);
    }

    SECTION("Nearest entity query") {
        auto e1 = world.createEntity();
        auto e2 = world.createEntity();

        Transform t1;
        t1.position = glm::vec3(10, 0, 0);
        world.addComponent(e1, t1);
        SpatialComponent s1;
        s1.boundingRadius = 2.0f;
        s1.markDirty();
        world.addComponent(e1, s1);

        Transform t2;
        t2.position = glm::vec3(50, 0, 0);
        world.addComponent(e2, t2);
        SpatialComponent s2;
        s2.boundingRadius = 2.0f;
        s2.markDirty();
        world.addComponent(e2, s2);

        spatialSystem->update(0.016f, world.getEntityManager());

        // Find nearest to origin
        auto nearest = spatialSystem->findNearestEntity(glm::vec3(0, 0, 0), 100.0f);

        REQUIRE(nearest != EntityID(0)); // Valid entity found
        CHECK(nearest == e1); // e1 should be closer than e2
    }

    SECTION("Bounding box query") {
        auto e1 = world.createEntity();
        auto e2 = world.createEntity();

        Transform t1;
        t1.position = glm::vec3(5, 5, 5);
        world.addComponent(e1, t1);
        SpatialComponent s1;
        s1.boundingRadius = 1.0f;
        s1.markDirty();
        world.addComponent(e1, s1);

        Transform t2;
        t2.position = glm::vec3(15, 15, 15);
        world.addComponent(e2, t2);
        SpatialComponent s2;
        s2.boundingRadius = 1.0f;
        s2.markDirty();
        world.addComponent(e2, s2);

        spatialSystem->update(0.016f, world.getEntityManager());

        // Query region that should contain only e1
        BoundingBox queryRegion(glm::vec3(0, 0, 0), glm::vec3(10, 10, 10));
        auto results = spatialSystem->queryRegion(queryRegion);

        REQUIRE(results.size() >= 1);

        bool foundE1 = false, foundE2 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
            if (entity == e2) foundE2 = true;
        }

        CHECK(foundE1);
        CHECK_FALSE(foundE2);
    }

    SECTION("Layer mask filtering") {
        auto e1 = world.createEntity();
        Transform t1;
        t1.position = glm::vec3(0, 0, 0);
        world.addComponent(e1, t1);
        SpatialComponent s1;
        s1.boundingRadius = 1.0f;
        s1.spatialLayers = LayerMask::Player;
        s1.markDirty();
        world.addComponent(e1, s1);

        auto e2 = world.createEntity();
        Transform t2;
        t2.position = glm::vec3(1, 0, 0);
        world.addComponent(e2, t2);
        SpatialComponent s2;
        s2.boundingRadius = 1.0f;
        s2.spatialLayers = LayerMask::Creatures;
        s2.markDirty();
        world.addComponent(e2, s2);

        spatialSystem->update(0.016f, world.getEntityManager());

        // Query only creatures
        auto results = spatialSystem->queryRadius(glm::vec3(0, 0, 0), 10.0f, LayerMask::Creatures);
        REQUIRE(results.size() == 1);

        bool foundE1 = false, foundE2 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
            if (entity == e2) foundE2 = true;
        }

        CHECK_FALSE(foundE1); // Player should not be found
        CHECK(foundE2); // Creature should be found

        // Query multiple layers
        auto multiResults = spatialSystem->queryRadius(glm::vec3(0, 0, 0), 10.0f, LayerMask::Player | LayerMask::Creatures);
        REQUIRE(multiResults.size() == 2);
    }
}

TEST_CASE("SpatialComponent Utility Functions", "[spatial]") {
    SECTION("Home position tracking") {
        SpatialComponent spatial(5.0f);

        glm::vec3 homePos(10, 0, 5);
        spatial.setHomePosition(homePos);

        CHECK(spatial.homePosition == homePos);
        CHECK(spatial.isNearHome(glm::vec3(12, 1, 6), 3.0f));
        CHECK_FALSE(spatial.isNearHome(glm::vec3(20, 0, 5), 3.0f));
    }

    SECTION("Query throttling") {
        SpatialComponent spatial(2.0f);

        // First query should not be throttled
        CHECK_FALSE(spatial.shouldThrottleQueries(0.016f, 10.0f));

        // Immediate next query should be throttled
        CHECK(spatial.shouldThrottleQueries(0.016f, 10.0f));

        // After enough time, should not be throttled
        CHECK_FALSE(spatial.shouldThrottleQueries(0.5f, 10.0f));
    }

    SECTION("World bounds calculation") {
        SpatialComponent spatial(3.0f);
        glm::vec3 worldPos(10, 5, -2);

        BoundingBox bounds = spatial.getWorldBounds(worldPos);

        CHECK(bounds.min == glm::vec3(7, 2, -5));
        CHECK(bounds.max == glm::vec3(13, 8, 1));
        CHECK(bounds.contains(worldPos));
    }
}

TEST_CASE("SpatialManager Octree Operations", "[spatial]") {
    BoundingBox worldBounds(glm::vec3(-50, -50, -50), glm::vec3(50, 50, 50));
    SpatialManager spatialManager(worldBounds);

    SECTION("Entity lifecycle management") {
        EntityID e1 = EntityID(1);
        EntityID e2 = EntityID(2);

        glm::vec3 pos1(10, 10, 10);
        glm::vec3 pos2(-15, 5, 0);

        // Add entities
        spatialManager.addEntity(e1, pos1);
        spatialManager.addEntity(e2, pos2);

        // Verify they can be found
        auto results = spatialManager.queryRadius(pos1, 5.0f);
        bool foundE1 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
        }
        CHECK(foundE1);

        // Update position
        glm::vec3 newPos1(20, 20, 20);
        spatialManager.updateEntity(e1, newPos1);

        // Verify old position no longer finds it
        results = spatialManager.queryRadius(pos1, 5.0f);
        foundE1 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
        }
        CHECK_FALSE(foundE1);

        // Verify new position finds it
        results = spatialManager.queryRadius(newPos1, 5.0f);
        foundE1 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
        }
        CHECK(foundE1);

        // Remove entity
        spatialManager.removeEntity(e1);

        results = spatialManager.queryRadius(newPos1, 5.0f);
        foundE1 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
        }
        CHECK_FALSE(foundE1);
    }

    SECTION("Pokemon-specific queries") {
        EntityID creature1 = EntityID(10);
        EntityID creature2 = EntityID(11);
        EntityID creature3 = EntityID(12);

        spatialManager.addEntity(creature1, glm::vec3(0, 0, 0), LayerMask::Creatures);
        spatialManager.addEntity(creature2, glm::vec3(8, 0, 0), LayerMask::Creatures);
        spatialManager.addEntity(creature3, glm::vec3(25, 0, 0), LayerMask::Creatures);

        // Test creature radius search
        auto nearbyCreatures = spatialManager.findCreaturesInRadius(glm::vec3(0, 0, 0), 15.0f);

        REQUIRE(nearbyCreatures.size() >= 2);

        bool found1 = false, found2 = false, found3 = false;
        for (auto entity : nearbyCreatures) {
            if (entity == creature1) found1 = true;
            if (entity == creature2) found2 = true;
            if (entity == creature3) found3 = true;
        }

        CHECK(found1);
        CHECK(found2);
        CHECK_FALSE(found3); // Should be outside radius
    }

    SECTION("Performance statistics") {
        // Add several entities
        for (int i = 0; i < 50; ++i) {
            EntityID entity = EntityID(100 + i);
            glm::vec3 pos(i * 2.0f, 0, 0);
            spatialManager.addEntity(entity, pos);
        }

        // Perform some queries to generate stats
        spatialManager.queryRadius(glm::vec3(0, 0, 0), 20.0f);
        spatialManager.queryRadius(glm::vec3(30, 0, 0), 15.0f);

        auto stats = spatialManager.getPerformanceStats();
        CHECK(stats.totalQueries >= 2);
        CHECK(stats.totalEntitiesReturned > 0);
        CHECK(stats.lastQueryTimeMs >= 0.0f);
    }
}

TEST_CASE("Frustum Culling", "[spatial]") {
    BoundingBox worldBounds(glm::vec3(-100, -100, -100), glm::vec3(100, 100, 100));
    SpatialManager spatialManager(worldBounds);

    SECTION("Basic frustum query") {
        // Add entities at various positions
        EntityID e1 = EntityID(1);
        EntityID e2 = EntityID(2);
        EntityID e3 = EntityID(3);

        spatialManager.addEntity(e1, glm::vec3(0, 0, -10));   // In front
        spatialManager.addEntity(e2, glm::vec3(0, 0, 10));    // Behind
        spatialManager.addEntity(e3, glm::vec3(100, 0, -10)); // Far to the side

        // Create a simple frustum (looking down negative Z)
        Frustum frustum;
        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
        frustum.updateFromMatrix(proj * view);

        auto results = spatialManager.queryFrustum(frustum);

        // Should find at least the entity in front
        REQUIRE(results.size() >= 1);

        bool foundE1 = false;
        for (auto entity : results) {
            if (entity == e1) foundE1 = true;
        }
        CHECK(foundE1);
    }
}