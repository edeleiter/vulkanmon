#pragma once

#include "../core/Entity.h"
#include "../spatial/LayerMask.h"
#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace VulkanMon {

/**
 * CollisionComponent
 *
 * Defines collision shape and interaction rules for physics entities.
 * Supports Pokemon-specific collision detection and layer-based filtering.
 *
 * Design Philosophy:
 * - Simple is Powerful: Clear shape types with minimal complexity
 * - Pokemon-focused: Capture mechanics, creature interactions, environmental collision
 * - Performance-aware: Layer filtering for efficient collision detection
 */
struct CollisionComponent {
    VKMON_COMPONENT(CollisionComponent)

    // =============================================================================
    // COLLISION SHAPE DEFINITION
    // =============================================================================

    // Collision shape types optimized for Pokemon gameplay
    enum class ShapeType : uint8_t {
        Box,        // Rectangular collision (buildings, platforms, crates)
        Sphere,     // Spherical collision (Pokeballs, simple round objects)
        Capsule,    // Pill-shaped collision (humanoid creatures, trees)
        Mesh,       // Complex mesh collision (detailed environment objects)
        Heightfield // Terrain collision (ground surfaces, landscapes)
    };

    ShapeType shapeType = ShapeType::Box;

    // Shape dimensions (interpretation depends on shapeType)
    // Box: half-extents (width/2, height/2, depth/2)
    // Sphere: radius in x component (y,z ignored)
    // Capsule: radius in x, height in y (z ignored)
    // Mesh/Heightfield: scaling factors
    glm::vec3 dimensions{1.0f};

    // Mesh path for complex collision shapes
    std::string meshPath;

    // Jolt Physics collision margin for numerical stability
    float convexRadius = 0.05f;

    // =============================================================================
    // COLLISION FILTERING SYSTEM
    // =============================================================================

    // Layer system for efficient collision detection
    uint32_t layer = LayerMask::Default;        // What layer this entity belongs to
    uint32_t collidesWith = LayerMask::All;     // What layers this entity collides with

    // =============================================================================
    // COLLISION BEHAVIOR
    // =============================================================================

    bool isTrigger = false;          // Generates events but doesn't block movement
    bool isStatic = false;           // Never moves (terrain, buildings)

    // =============================================================================
    // CREATURE COLLECTION GAME PROPERTIES
    // =============================================================================

    // Capture device mechanics (generic for creature collection games)
    float captureRadius = 0.0f;      // Radius for creature capture detection (0 = no capture)
    bool blocksCaptureDevices = true; // Can capture devices pass through this object?
    bool blocksCreatures = true;     // Can creatures pass through this object?

    // Backward compatibility
    bool blocksPokeballs = true;     // Alias for blocksCaptureDevices

    // Environmental interaction properties
    bool isWater = false;            // Water area for swimming creatures
    bool isVegetation = false;       // Vegetation area for creature encounters
    bool isTallGrass = false;        // Alias for isVegetation
    bool isClimbable = false;        // Can creatures climb this surface?

    // =============================================================================
    // COLLISION EVENT CALLBACKS
    // =============================================================================

    // Collision events (solid objects)
    std::function<void(EntityID self, EntityID other)> onCollisionEnter;
    std::function<void(EntityID self, EntityID other)> onCollisionExit;

    // Trigger events (pass-through detection zones)
    std::function<void(EntityID self, EntityID other)> onTriggerEnter;
    std::function<void(EntityID self, EntityID other)> onTriggerExit;

    // =============================================================================
    // CONSTRUCTORS FOR COMMON USE CASES
    // =============================================================================

    // Default constructor
    CollisionComponent() = default;

    // =============================================================================
    // ENGINE-FOCUSED FACTORY METHODS
    // =============================================================================

    // Spherical collision shape (radius)
    static CollisionComponent sphere(float radius, uint32_t layer = LayerMask::Default) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Sphere;
        collision.dimensions = glm::vec3(radius, 0.0f, 0.0f);
        collision.layer = layer;
        collision.collidesWith = LayerMask::All;
        return collision;
    }

    // Box collision shape (half-extents)
    static CollisionComponent box(glm::vec3 size, uint32_t layer = LayerMask::Default) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Box;
        collision.dimensions = size * 0.5f; // Convert size to half-extents
        collision.layer = layer;
        collision.collidesWith = LayerMask::All;
        return collision;
    }

    // Capsule collision shape (radius, height) - perfect for character controllers
    static CollisionComponent capsule(float radius, float height, uint32_t layer = LayerMask::Default) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Capsule;
        collision.dimensions = glm::vec3(radius, height, radius);
        collision.layer = layer;
        collision.collidesWith = LayerMask::All;
        return collision;
    }

    // Plane collision (infinite ground plane)
    static CollisionComponent plane(glm::vec3 normal = {0, 1, 0}, uint32_t layer = LayerMask::Environment) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Box;
        collision.dimensions = glm::vec3(1000.0f, 0.1f, 1000.0f); // Large thin box
        collision.layer = layer;
        collision.collidesWith = LayerMask::All;
        collision.isStatic = true;
        return collision;
    }

    // =============================================================================
    // GAME-SPECIFIC FACTORY METHODS (for reference/convenience)
    // =============================================================================

    // Pokemon creature collision - DEPRECATED: Use sphere() or capsule()
    static CollisionComponent createCreature(float radius, float height = 0.0f) {
        if (height > 0.0f) {
            auto collision = capsule(radius, height, LayerMask::Creatures);
            collision.collidesWith = LayerMask::All & ~LayerMask::Creatures; // Don't collide with other creatures by default
            return collision;
        } else {
            auto collision = sphere(radius, LayerMask::Creatures);
            collision.collidesWith = LayerMask::All & ~LayerMask::Creatures; // Don't collide with other creatures by default
            return collision;
        }
    }

    // Capture device collision (spheres, nets, capture tools)
    static CollisionComponent createCaptureDevice(float radius = 0.1f) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Sphere;
        collision.dimensions = glm::vec3(radius, 0.0f, 0.0f);
        collision.layer = LayerMask::CaptureDevices;
        collision.collidesWith = LayerMask::All;
        collision.captureRadius = radius * 2.0f; // Capture radius larger than collision
        return collision;
    }

    // Environment object collision (trees, rocks, buildings)
    static CollisionComponent createEnvironment(const glm::vec3& size) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Box;
        collision.dimensions = size * 0.5f; // Convert size to half-extents
        collision.layer = LayerMask::Environment;
        collision.collidesWith = LayerMask::All;
        collision.isStatic = true;
        return collision;
    }

    // Terrain/ground collision
    static CollisionComponent createTerrain(const std::string& heightfieldPath = "") {
        CollisionComponent collision;
        collision.shapeType = heightfieldPath.empty() ? ShapeType::Box : ShapeType::Heightfield;
        collision.meshPath = heightfieldPath;
        collision.dimensions = glm::vec3(100.0f, 1.0f, 100.0f); // Large flat terrain
        collision.layer = LayerMask::Terrain;
        collision.collidesWith = LayerMask::All;
        collision.isStatic = true;
        return collision;
    }

    // Water area for swimming
    static CollisionComponent createWater(const glm::vec3& size) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Box;
        collision.dimensions = size * 0.5f;
        collision.layer = LayerMask::Water;
        collision.collidesWith = LayerMask::Creatures | LayerMask::Player;
        collision.isTrigger = true; // Pass through but detect entry/exit
        collision.isWater = true;
        collision.blocksPokeballs = false; // Pokeballs can pass through water
        return collision;
    }

    // Tall grass for Pokemon encounters
    static CollisionComponent createTallGrass(const glm::vec3& size) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Box;
        collision.dimensions = size * 0.5f;
        collision.layer = LayerMask::Environment;
        collision.collidesWith = LayerMask::Player | LayerMask::Creatures;
        collision.isTrigger = true; // Pass through but detect entry
        collision.isTallGrass = true;
        collision.blocksPokeballs = false;
        collision.blocksCreatures = false;
        return collision;
    }

    // Trigger zone for general detection
    static CollisionComponent createTriggerZone(float radius, uint32_t detectionLayers = LayerMask::All) {
        CollisionComponent collision;
        collision.shapeType = ShapeType::Sphere;
        collision.dimensions = glm::vec3(radius, 0.0f, 0.0f);
        collision.layer = LayerMask::TriggerZone;
        collision.collidesWith = detectionLayers;
        collision.isTrigger = true;
        collision.blocksPokeballs = false;
        collision.blocksCreatures = false;
        return collision;
    }

    // =============================================================================
    // COLLISION UTILITIES
    // =============================================================================

    // Get volume for mass calculations
    float getVolume() const {
        switch (shapeType) {
            case ShapeType::Box:
                return 8.0f * dimensions.x * dimensions.y * dimensions.z; // 8 * half-extents
            case ShapeType::Sphere:
                return (4.0f / 3.0f) * 3.14159f * dimensions.x * dimensions.x * dimensions.x; // 4/3 * pi * r^3
            case ShapeType::Capsule: {
                float r = dimensions.x;
                float h = dimensions.y - 2.0f * r; // Cylinder height (total - 2 hemispheres)
                float sphereVol = (4.0f / 3.0f) * 3.14159f * r * r * r;
                float cylinderVol = 3.14159f * r * r * h;
                return sphereVol + cylinderVol;
            }
            case ShapeType::Mesh:
            case ShapeType::Heightfield:
            default:
                return 1.0f; // Cannot calculate for complex shapes
        }
    }

    // Get bounding sphere radius (useful for broad-phase collision)
    float getBoundingSphereRadius() const {
        switch (shapeType) {
            case ShapeType::Box:
                return glm::length(dimensions); // Distance to corner
            case ShapeType::Sphere:
                return dimensions.x;
            case ShapeType::Capsule:
                return std::max(dimensions.x, dimensions.y * 0.5f);
            case ShapeType::Mesh:
            case ShapeType::Heightfield:
            default:
                return glm::length(dimensions); // Approximation
        }
    }

    // Check if this collision affects specific entity types
    bool affectsCreatures() const {
        return blocksCreatures && (collidesWith & LayerMask::Creatures) != LayerMask::None;
    }

    bool affectsCaptureDevices() const {
        return blocksCaptureDevices && (collidesWith & LayerMask::CaptureDevices) != LayerMask::None;
    }

    bool affectsPlayer() const {
        return (collidesWith & LayerMask::Player) != LayerMask::None;
    }

    // Check if collision should occur between layers
    bool shouldCollideWith(uint32_t otherLayer) const {
        return (collidesWith & otherLayer) != LayerMask::None;
    }

    // Set up collision callbacks for creature capture interactions
    void setupCreatureCapture(std::function<void(EntityID, EntityID)> captureCallback) {
        if (layer == LayerMask::CaptureDevices && captureRadius > 0.0f) {
            onTriggerEnter = captureCallback;
        }
    }

    void setupWaterInteraction(std::function<void(EntityID, EntityID)> enterWater,
                              std::function<void(EntityID, EntityID)> exitWater) {
        if (isWater) {
            onTriggerEnter = enterWater;
            onTriggerExit = exitWater;
        }
    }

    void setupGrassEncounter(std::function<void(EntityID, EntityID)> encounterCallback) {
        if (isTallGrass) {
            onTriggerEnter = encounterCallback;
        }
    }
};

} // namespace VulkanMon