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
}

TEST_CASE("WorldConfig validation edge cases", "[WorldConfig]") {
    WorldConfig config;

    SECTION("Invalid octree depth") {
        config.maxOctreeDepth = 0;
        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);

        config.maxOctreeDepth = 21;
        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);

        config.maxOctreeDepth = 1;
        REQUIRE_NOTHROW(config.validate());

        config.maxOctreeDepth = 20;
        REQUIRE_NOTHROW(config.validate());
    }

    SECTION("Invalid node size") {
        config.minNodeSize = 0.0f;
        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);

        config.minNodeSize = -1.0f;
        REQUIRE_THROWS_AS(config.validate(), std::invalid_argument);

        config.minNodeSize = 0.001f;
        REQUIRE_NOTHROW(config.validate());
    }
}

TEST_CASE("WorldConfig utility methods", "[WorldConfig]") {
    WorldConfig config(glm::vec3(-10.0f, -5.0f, -20.0f), glm::vec3(10.0f, 15.0f, 20.0f));

    SECTION("Size calculation") {
        glm::vec3 size = config.getSize();
        REQUIRE(size.x == 20.0f);
        REQUIRE(size.y == 20.0f);
        REQUIRE(size.z == 40.0f);
    }

    SECTION("Center calculation") {
        glm::vec3 center = config.getCenter();
        REQUIRE(center.x == 0.0f);
        REQUIRE(center.y == 5.0f);
        REQUIRE(center.z == 0.0f);
    }

    SECTION("Volume calculation") {
        float volume = config.getVolume();
        REQUIRE(volume == 20.0f * 20.0f * 40.0f);
    }
}

TEST_CASE("WorldConfig factory methods", "[WorldConfig]") {
    SECTION("createTestWorld") {
        WorldConfig config = WorldConfig::createTestWorld();

        REQUIRE(config.name == "Test World");
        REQUIRE(config.minBounds == glm::vec3(-10.0f, -5.0f, -10.0f));
        REQUIRE(config.maxBounds == glm::vec3(10.0f, 10.0f, 10.0f));
        REQUIRE_NOTHROW(config.validate());
    }

    SECTION("createPokemonWorld") {
        WorldConfig config = WorldConfig::createPokemonWorld();

        REQUIRE(config.name == "Pokemon World");
        REQUIRE(config.minBounds == glm::vec3(-100.0f, -10.0f, -100.0f));
        REQUIRE(config.maxBounds == glm::vec3(100.0f, 50.0f, 100.0f));
        REQUIRE_NOTHROW(config.validate());

        // Verify it's much larger than test world
        float pokemonVolume = config.getVolume();
        float testVolume = WorldConfig::createTestWorld().getVolume();
        REQUIRE(pokemonVolume > testVolume * 10);
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