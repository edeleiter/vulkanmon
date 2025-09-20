#pragma once

#include <glm/glm.hpp>
#include <string>
#include <stdexcept>

namespace VulkanMon {

struct BoundingBox;

struct WorldConfig {
    glm::vec3 minBounds;
    glm::vec3 maxBounds;
    int maxOctreeDepth = 8;
    float minNodeSize = 1.0f;
    std::string name = "Default";

    WorldConfig()
        : minBounds(-10.0f, -5.0f, -10.0f)
        , maxBounds(10.0f, 10.0f, 10.0f) {}

    WorldConfig(const glm::vec3& min, const glm::vec3& max, const std::string& worldName = "Default")
        : minBounds(min), maxBounds(max), name(worldName) {
        validate();
    }

    BoundingBox getBoundingBox() const;

    void validate() const {
        if (minBounds.x >= maxBounds.x ||
            minBounds.y >= maxBounds.y ||
            minBounds.z >= maxBounds.z) {
            throw std::invalid_argument("WorldConfig: minBounds must be less than maxBounds in all dimensions");
        }

        if (maxOctreeDepth < 1 || maxOctreeDepth > 20) {
            throw std::invalid_argument("WorldConfig: maxOctreeDepth must be between 1 and 20");
        }

        if (minNodeSize <= 0.0f) {
            throw std::invalid_argument("WorldConfig: minNodeSize must be positive");
        }
    }

    glm::vec3 getSize() const {
        return maxBounds - minBounds;
    }

    glm::vec3 getCenter() const {
        return (minBounds + maxBounds) * 0.5f;
    }

    float getVolume() const {
        glm::vec3 size = getSize();
        return size.x * size.y * size.z;
    }

    static WorldConfig createPokemonWorld() {
        return WorldConfig(
            glm::vec3(-100.0f, -10.0f, -100.0f),
            glm::vec3(100.0f, 50.0f, 100.0f),
            "Pokemon World"
        );
    }

    static WorldConfig createTestWorld() {
        return WorldConfig(
            glm::vec3(-30.0f, -5.0f, -30.0f),  // Expanded for 10x10x10 cube formation
            glm::vec3(30.0f, 35.0f, 30.0f),   // Y expanded to 35 to accommodate cube 0-27 + buffer
            "Test World"
        );
    }
};

}