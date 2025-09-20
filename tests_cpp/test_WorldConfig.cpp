#include <catch2/catch_test_macros.hpp>
#include "../src/spatial/WorldConfig.h"
#include "../src/spatial/SpatialManager.h"
#include <stdexcept>

using namespace VulkanMon;

TEST_CASE("WorldConfig construction and validation", "[WorldConfig]") {
    SECTION("Default constructor creates valid config") {
        WorldConfig config;

        REQUIRE(config.minBounds.x == -10.0f);
        REQUIRE(config.minBounds.y == -5.0f);
        REQUIRE(config.minBounds.z == -10.0f);
        REQUIRE(config.maxBounds.x == 10.0f);
        REQUIRE(config.maxBounds.y == 10.0f);
        REQUIRE(config.maxBounds.z == 10.0f);
        REQUIRE(config.maxOctreeDepth == 8);
        REQUIRE(config.minNodeSize == 1.0f);
        REQUIRE(config.name == "Default");

        REQUIRE_NOTHROW(config.validate());
    }

    SECTION("Parameterized constructor with valid bounds") {
        glm::vec3 min(-5.0f, -2.0f, -5.0f);
        glm::vec3 max(5.0f, 8.0f, 5.0f);

        WorldConfig config(min, max, "Test World");

        REQUIRE(config.minBounds == min);
        REQUIRE(config.maxBounds == max);
        REQUIRE(config.name == "Test World");
        REQUIRE_NOTHROW(config.validate());
    }

    SECTION("Invalid bounds throw exceptions") {
        // Min X >= Max X
        REQUIRE_THROWS_AS(
            WorldConfig(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(5.0f, 10.0f, 10.0f)),
            std::invalid_argument
        );

        // Min Y > Max Y
        REQUIRE_THROWS_AS(
            WorldConfig(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(10.0f, 5.0f, 10.0f)),
            std::invalid_argument
        );

        // Min Z > Max Z
        REQUIRE_THROWS_AS(
            WorldConfig(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(10.0f, 10.0f, 5.0f)),
            std::invalid_argument
        );
    }

    SECTION("Edge case validation") {
        WorldConfig config;

        // Test octree depth validation
        config.maxOctreeDepth = 0;
        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);
        config.maxOctreeDepth = 21;
        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);
        config.maxOctreeDepth = 8; // Reset to valid

        // Test node size validation
        config.minNodeSize = 0.0f;
        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);
        config.minNodeSize = 1.0f; // Reset to valid
    }

    SECTION("Utility methods") {
        WorldConfig testConfig(glm::vec3(-10.0f, -5.0f, -20.0f), glm::vec3(10.0f, 15.0f, 20.0f));

        // Size calculation
        glm::vec3 size = testConfig.getSize();
        REQUIRE(size.x == 20.0f);
        REQUIRE(size.y == 20.0f);
        REQUIRE(size.z == 40.0f);

        // Center calculation
        glm::vec3 center = testConfig.getCenter();
        REQUIRE(center.x == 0.0f);
        REQUIRE(center.y == 5.0f);
        REQUIRE(center.z == 0.0f);

        // Volume calculation
        float volume = testConfig.getVolume();
        REQUIRE(volume == 16000.0f); // 20*20*40
    }

    SECTION("Factory methods") {
        // Test world factory
        WorldConfig testWorld = WorldConfig::createTestWorld();
        REQUIRE(testWorld.name == "Test World");
        REQUIRE_NOTHROW(testWorld.validate());

        // Pokemon world factory
        WorldConfig pokemonWorld = WorldConfig::createPokemonWorld();
        REQUIRE(pokemonWorld.name == "Pokemon World");
        REQUIRE_NOTHROW(pokemonWorld.validate());
        REQUIRE(pokemonWorld.getVolume() > testWorld.getVolume() * 10);
    }
}

TEST_CASE("WorldConfig BoundingBox integration", "[WorldConfig][Integration]") {
    SECTION("getBoundingBox creates correct BoundingBox") {
        glm::vec3 min(-5.0f, -2.0f, -8.0f);
        glm::vec3 max(15.0f, 12.0f, 8.0f);
        WorldConfig config(min, max);

        BoundingBox bbox = config.getBoundingBox();

        REQUIRE(bbox.min == min);
        REQUIRE(bbox.max == max);

        // Test BoundingBox functionality
        REQUIRE(bbox.contains(glm::vec3(0.0f, 5.0f, 0.0f)));
        REQUIRE_FALSE(bbox.contains(glm::vec3(20.0f, 5.0f, 0.0f)));

        glm::vec3 center = bbox.getCenter();
        REQUIRE(center.x == 5.0f);
        REQUIRE(center.y == 5.0f);
        REQUIRE(center.z == 0.0f);
    }
}