#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/SpatialComponent.h"
#include "../spatial/LayerMask.h"
#include "../spatial/SpatialManager.h"
#include "../utils/Logger.h"
#include <memory>
#include <random>

namespace VulkanMon {

// Forward declarations
class SpatialSystem;

struct GrassComponent {
    VKMON_COMPONENT(GrassComponent)

    // Visual properties
    float rustleIntensity = 0.0f;       // Current rustle strength (0.0 - 1.0)
    float rustleRadius = 3.0f;          // How close entities need to be to rustle grass

    // Spawning properties
    float spawnChance = 0.001f;         // Base chance per frame to spawn creature (very low)
    float lastSpawnTime = 0.0f;         // Time since last spawn
    float minSpawnInterval = 5.0f;      // Minimum time between spawns
    bool canSpawn = true;               // Whether this grass patch can spawn creatures

    // Visual effects
    glm::vec3 rustleDirection = glm::vec3(0.0f); // Direction of movement causing rustle
    float rustleDecayRate = 2.0f;       // How fast rustle fades (per second)

    // Grass type affects spawn rates and creature types
    enum class GrassType : uint8_t {
        SHORT_GRASS,    // Common creatures, low spawn rate
        TALL_GRASS,     // Uncommon creatures, higher spawn rate
        FLOWER_PATCH,   // Special creatures, very low spawn rate
        DENSE_BRUSH     // Aggressive creatures, medium spawn rate
    } type = GrassType::SHORT_GRASS;

    GrassComponent() = default;
    GrassComponent(GrassType t, float radius = 3.0f)
        : type(t), rustleRadius(radius) {

        // Set spawn rates based on grass type
        switch (type) {
            case GrassType::SHORT_GRASS:
                spawnChance = 0.0005f;
                break;
            case GrassType::TALL_GRASS:
                spawnChance = 0.002f;
                break;
            case GrassType::FLOWER_PATCH:
                spawnChance = 0.0001f;
                break;
            case GrassType::DENSE_BRUSH:
                spawnChance = 0.001f;
                break;
        }
    }
};

class GrassSystem : public System<Transform, SpatialComponent, GrassComponent> {
private:
    std::shared_ptr<SpatialSystem> spatialSystem_;
    std::mt19937 rng_;

    // Performance tracking
    struct GrassStats {
        size_t grassPatchesProcessed = 0;
        size_t rustleEventsTriggered = 0;
        size_t spawnsAttempted = 0;
        size_t creaturesSpawned = 0;
        float updateTimeMs = 0.0f;
    } frameStats_;

public:
    GrassSystem(std::shared_ptr<SpatialSystem> spatialSystem)
        : spatialSystem_(spatialSystem), rng_(std::random_device{}()) {
        VKMON_INFO("GrassSystem initialized");
    }

    void update(float deltaTime, EntityManager& entityManager) override {
        if (!spatialSystem_) {
            VKMON_WARNING("GrassSystem: No spatial system available");
            return;
        }

        auto start = std::chrono::high_resolution_clock::now();
        frameStats_ = GrassStats{};

        // Get all grass patches
        auto& transforms = entityManager.getAllComponents<Transform>();
        auto& spatialComponents = entityManager.getAllComponents<SpatialComponent>();
        auto& grassComponents = entityManager.getAllComponents<GrassComponent>();
        auto& entityIds = entityManager.getEntitiesWithComponent<Transform>();

        frameStats_.grassPatchesProcessed = entityIds.size();

        for (size_t i = 0; i < entityIds.size(); ++i) {
            EntityID entity = entityIds[i];

            // Check if entity has all required components
            if (entityManager.hasComponent<SpatialComponent>(entity) &&
                entityManager.hasComponent<GrassComponent>(entity)) {

                auto& transform = transforms[i];
                auto& spatial = entityManager.getComponent<SpatialComponent>(entity);
                auto& grass = entityManager.getComponent<GrassComponent>(entity);

                updateGrassPatch(entity, transform, spatial, grass, deltaTime, entityManager);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        frameStats_.updateTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Log performance occasionally
        static int frameCount = 0;
        if (++frameCount >= 600) { // Every ~10 seconds at 60 FPS
            logPerformanceStats();
            frameCount = 0;
        }
    }

    void render(VulkanRenderer& renderer, EntityManager& entityManager) override {
        // GrassSystem doesn't render anything directly
        // Rendering would be handled by a separate grass rendering system
    }

    // Get system statistics
    const GrassStats& getFrameStats() const { return frameStats_; }

private:
    void updateGrassPatch(EntityID entity, const Transform& transform, const SpatialComponent& spatial,
                         GrassComponent& grass, float deltaTime, EntityManager& entityManager) {

        // Update timers
        grass.lastSpawnTime += deltaTime;

        // Check for entities moving through grass
        auto nearbyEntities = spatialSystem_->queryRadius(
            transform.position,
            grass.rustleRadius,
            LayerMask::Creatures | LayerMask::Player
        );

        // Calculate rustle intensity based on nearby movement
        float maxRustleThisFrame = 0.0f;
        glm::vec3 averageDirection(0.0f);
        int movingEntities = 0;

        for (EntityID nearbyEntity : nearbyEntities) {
            if (nearbyEntity == entity) continue; // Skip self

            // Calculate rustle intensity based on distance
            // Note: In a real implementation, you'd get the actual entity position
            // For now, we simulate based on detection
            float distance = grass.rustleRadius * 0.5f; // Simulate distance
            float intensity = 1.0f - (distance / grass.rustleRadius);
            intensity = glm::max(0.0f, intensity);

            if (intensity > maxRustleThisFrame) {
                maxRustleThisFrame = intensity;
            }

            // Simulate movement direction (in real implementation, use velocity)
            glm::vec3 direction = glm::normalize(glm::vec3(
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f,
                0.0f,
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f
            ));
            averageDirection += direction;
            movingEntities++;

            frameStats_.rustleEventsTriggered++;
        }

        // Update rustle properties
        if (movingEntities > 0) {
            grass.rustleIntensity = glm::max(grass.rustleIntensity, maxRustleThisFrame);
            grass.rustleDirection = averageDirection / static_cast<float>(movingEntities);

            // Check for creature spawning
            if (shouldSpawnCreature(grass, maxRustleThisFrame)) {
                frameStats_.spawnsAttempted++;
                if (spawnWildCreature(transform.position, grass, entityManager)) {
                    frameStats_.creaturesSpawned++;
                }
            }
        }

        // Decay rustle over time
        grass.rustleIntensity *= (1.0f - deltaTime * grass.rustleDecayRate);
        grass.rustleIntensity = glm::max(0.0f, grass.rustleIntensity);
    }

    bool shouldSpawnCreature(const GrassComponent& grass, float rustleIntensity) {
        // Don't spawn if recently spawned
        if (grass.lastSpawnTime < grass.minSpawnInterval) {
            return false;
        }

        // Don't spawn if disabled
        if (!grass.canSpawn) {
            return false;
        }

        // Calculate spawn chance based on rustle intensity and grass type
        float effectiveSpawnChance = grass.spawnChance * (1.0f + rustleIntensity * 2.0f);

        // Roll for spawn
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng_) < effectiveSpawnChance;
    }

    bool spawnWildCreature(const glm::vec3& position, GrassComponent& grass, EntityManager& entityManager) {
        // In a real implementation, this would create a new creature entity
        // For this example, we'll just log the spawn and reset the timer

        #ifdef DEBUG_VERBOSE
        VKMON_DEBUG("Wild creature spawned in " + getGrassTypeName(grass.type) +
                   " at position (" + std::to_string(position.x) + ", " +
                   std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
        #endif

        // Reset spawn timer
        grass.lastSpawnTime = 0.0f;

        // Temporarily disable spawning for this patch
        grass.canSpawn = false;

        // Re-enable spawning after some time (would be handled by a timer system)
        // For now, just immediately re-enable for demonstration
        std::uniform_real_distribution<float> dist(5.0f, 15.0f);
        grass.minSpawnInterval = dist(rng_); // Random interval between spawns

        return true;
    }

    const char* getGrassTypeName(GrassComponent::GrassType type) const {
        switch (type) {
            case GrassComponent::GrassType::SHORT_GRASS: return "Short Grass";
            case GrassComponent::GrassType::TALL_GRASS: return "Tall Grass";
            case GrassComponent::GrassType::FLOWER_PATCH: return "Flower Patch";
            case GrassComponent::GrassType::DENSE_BRUSH: return "Dense Brush";
            default: return "Unknown";
        }
    }

    void logPerformanceStats() {
        VKMON_INFO("GrassSystem Performance:");
        VKMON_INFO("  Grass patches processed: " + std::to_string(frameStats_.grassPatchesProcessed));
        VKMON_INFO("  Rustle events triggered: " + std::to_string(frameStats_.rustleEventsTriggered));
        VKMON_INFO("  Spawn attempts: " + std::to_string(frameStats_.spawnsAttempted));
        VKMON_INFO("  Creatures spawned: " + std::to_string(frameStats_.creaturesSpawned));
        VKMON_INFO("  Update time: " + std::to_string(frameStats_.updateTimeMs) + "ms");
    }
};

} // namespace VulkanMon