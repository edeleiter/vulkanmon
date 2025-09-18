#pragma once

#include "../core/SystemImpl.h"
#include "../components/Transform.h"
#include "../components/SpatialComponent.h"
#include "../spatial/LayerMask.h"
#include "../spatial/SpatialManager.h"
#include "../systems/SpatialSystem.h"
#include "../utils/Logger.h"
#include <memory>
#include <random>

namespace VulkanMon {

enum class CreatureState : uint8_t {
    IDLE = 0,
    WANDERING = 1,
    ALERT = 2,
    FLEEING = 3,
    AGGRESSIVE = 4
};

struct CreatureComponent {
    VKMON_COMPONENT(CreatureComponent)

    CreatureState state = CreatureState::IDLE;
    EntityID targetEntity = INVALID_ENTITY;

    // Detection parameters
    float detectionRadius = 15.0f;      // How far creature can detect others
    float alertRadius = 25.0f;          // Radius at which creature becomes alert
    float fleeRadius = 8.0f;            // Radius at which creature starts fleeing

    // Behavior parameters
    float wanderSpeed = 2.0f;
    float alertSpeed = 4.0f;
    float fleeSpeed = 8.0f;

    // Timing
    float lastDetectionCheck = 0.0f;
    float detectionCheckInterval = 0.2f; // Check every 200ms
    float alertDuration = 5.0f;         // Stay alert for 5 seconds
    float alertTimer = 0.0f;

    // Creature type for specific behaviors
    enum class CreatureType : uint8_t {
        PEACEFUL,    // Runs away from player
        NEUTRAL,     // Ignores player unless provoked
        AGGRESSIVE   // Approaches player when detected
    } type = CreatureType::PEACEFUL;

    CreatureComponent() = default;
    CreatureComponent(CreatureType t, float detection = 15.0f)
        : type(t), detectionRadius(detection) {}
};

class CreatureDetectionSystem : public System<Transform, SpatialComponent, CreatureComponent> {
private:
    SpatialSystem* spatialSystem_ = nullptr;

    // Performance tracking
    struct DetectionStats {
        size_t creaturesProcessed = 0;
        size_t detectionsTriggered = 0;
        size_t stateChanges = 0;
        float updateTimeMs = 0.0f;
    } frameStats_;

    // Modern C++11 random generation (thread-safe, high quality)
    mutable std::mt19937 randomGenerator_;
    mutable std::uniform_real_distribution<float> randomDistribution_;

public:
    CreatureDetectionSystem()
        : randomGenerator_(std::random_device{}()), randomDistribution_(0.0f, 1.0f) {
        VKMON_INFO("CreatureDetectionSystem initialized");
    }

    void setSpatialSystem(SpatialSystem* spatialSystem) {
        spatialSystem_ = spatialSystem;
    }

    void update(float deltaTime, EntityManager& entityManager) override {
        if (!spatialSystem_) {
            VKMON_WARNING("CreatureDetectionSystem: No spatial system available");
            return;
        }

        auto start = std::chrono::high_resolution_clock::now();
        frameStats_ = DetectionStats{};

        // Get all creatures
        auto& transforms = entityManager.getAllComponents<Transform>();
        auto& spatialComponents = entityManager.getAllComponents<SpatialComponent>();
        auto& creatureComponents = entityManager.getAllComponents<CreatureComponent>();
        auto& entityIds = entityManager.getEntitiesWithComponent<Transform>();

        frameStats_.creaturesProcessed = entityIds.size();

        for (size_t i = 0; i < entityIds.size(); ++i) {
            EntityID entity = entityIds[i];

            // Check if entity has all required components
            if (entityManager.hasComponent<SpatialComponent>(entity) &&
                entityManager.hasComponent<CreatureComponent>(entity)) {

                auto& transform = transforms[i];
                auto& spatial = entityManager.getComponent<SpatialComponent>(entity);
                auto& creature = entityManager.getComponent<CreatureComponent>(entity);

                updateCreatureBehavior(entity, transform, spatial, creature, deltaTime, entityManager);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        frameStats_.updateTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Log performance occasionally
        static int frameCount = 0;
        if (++frameCount >= 300) { // Every ~5 seconds at 60 FPS
            logPerformanceStats();
            frameCount = 0;
        }
    }

    void render(VulkanRenderer& renderer, EntityManager& entityManager) override {
        // CreatureDetectionSystem doesn't render anything
    }

    // Get system statistics
    const DetectionStats& getFrameStats() const { return frameStats_; }

private:
    void updateCreatureBehavior(EntityID entity, const Transform& transform,
                               const SpatialComponent& spatial, CreatureComponent& creature, float deltaTime, EntityManager& entityManager) {

        // Update timing
        creature.lastDetectionCheck += deltaTime;
        creature.alertTimer += deltaTime;

        // Only check detection periodically for performance
        if (creature.lastDetectionCheck >= creature.detectionCheckInterval) {
            creature.lastDetectionCheck = 0.0f;

            // Query for players and other creatures in detection radius
            auto nearbyEntities = spatialSystem_->queryRadius(
                transform.position,
                creature.detectionRadius,
                LayerMask::Player | LayerMask::Creatures
            );

            processDetectedEntities(entity, transform, creature, nearbyEntities, entityManager);
        }

        // Update creature state based on current conditions
        updateCreatureState(entity, transform, creature, deltaTime);
    }

    void processDetectedEntities(EntityID entity, const Transform& transform,
                                CreatureComponent& creature, const std::vector<EntityID>& nearbyEntities, EntityManager& entityManager) {

        EntityID closestPlayer = INVALID_ENTITY;
        float closestPlayerDistance = creature.detectionRadius;

        // Find the closest player
        for (EntityID nearbyEntity : nearbyEntities) {
            if (nearbyEntity == entity) continue; // Skip self

            // For now, treat any entity with Player layer as a player
            // Since we already have nearbyEntities from spatial query, just check if it's a player
            auto playersInRadius = spatialSystem_->queryRadius(transform.position, creature.detectionRadius, LayerMask::Player);
            for (EntityID playerEntity : playersInRadius) {
                if (playerEntity == nearbyEntity) {
                    // Get player's actual Transform component for distance calculation
                    if (entityManager.hasComponent<Transform>(playerEntity)) {
                        auto& playerTransform = entityManager.getComponent<Transform>(playerEntity);
                        float distance = glm::distance(transform.position, playerTransform.position);
                        if (distance < closestPlayerDistance) {
                            closestPlayer = nearbyEntity;
                            closestPlayerDistance = distance;
                        }
                    }
                    break; // Found the player match
                }
            }
        }

        // React to closest player if found
        if (closestPlayer != INVALID_ENTITY) {
            frameStats_.detectionsTriggered++;
            handlePlayerDetection(entity, creature, closestPlayer, closestPlayerDistance);
        }
    }

    void handlePlayerDetection(EntityID entity, CreatureComponent& creature,
                              EntityID player, float distance) {

        CreatureState oldState = creature.state;

        switch (creature.type) {
            case CreatureComponent::CreatureType::PEACEFUL:
                if (distance <= creature.fleeRadius) {
                    creature.state = CreatureState::FLEEING;
                    creature.targetEntity = player;
                } else if (distance <= creature.alertRadius) {
                    creature.state = CreatureState::ALERT;
                    creature.targetEntity = player;
                    creature.alertTimer = 0.0f;
                }
                break;

            case CreatureComponent::CreatureType::NEUTRAL:
                if (distance <= creature.alertRadius) {
                    creature.state = CreatureState::ALERT;
                    creature.targetEntity = player;
                    creature.alertTimer = 0.0f;
                }
                break;

            case CreatureComponent::CreatureType::AGGRESSIVE:
                if (distance <= creature.detectionRadius) {
                    creature.state = CreatureState::AGGRESSIVE;
                    creature.targetEntity = player;
                }
                break;
        }

        if (oldState != creature.state) {
            frameStats_.stateChanges++;

            #ifdef DEBUG_VERBOSE
            VKMON_DEBUG("Creature " + std::to_string(static_cast<uint32_t>(entity)) +
                       " state changed from " + std::to_string(static_cast<int>(oldState)) +
                       " to " + std::to_string(static_cast<int>(creature.state)));
            #endif
        }
    }

    void updateCreatureState(EntityID entity, const Transform& transform,
                            CreatureComponent& creature, float deltaTime) {

        switch (creature.state) {
            case CreatureState::IDLE:
                // Occasionally switch to wandering
                if (randomDistribution_(randomGenerator_) < 0.01f) { // 1% chance per frame
                    creature.state = CreatureState::WANDERING;
                    frameStats_.stateChanges++;
                }
                break;

            case CreatureState::WANDERING:
                // Wander for a while, then return to idle
                if (randomDistribution_(randomGenerator_) < 0.005f) { // 0.5% chance per frame
                    creature.state = CreatureState::IDLE;
                    frameStats_.stateChanges++;
                }
                break;

            case CreatureState::ALERT:
                // Return to normal behavior after alert duration
                if (creature.alertTimer >= creature.alertDuration) {
                    creature.state = CreatureState::WANDERING;
                    creature.targetEntity = INVALID_ENTITY;
                    frameStats_.stateChanges++;
                }
                break;

            case CreatureState::FLEEING:
                // Stop fleeing after some time or distance
                if (creature.alertTimer >= creature.alertDuration * 0.5f) {
                    creature.state = CreatureState::ALERT;
                    frameStats_.stateChanges++;
                }
                break;

            case CreatureState::AGGRESSIVE:
                // Stay aggressive until target is out of range
                // This would be handled by the detection check above
                break;
        }
    }

    void logPerformanceStats() {
        VKMON_INFO("CreatureDetectionSystem Performance:");
        VKMON_INFO("  Creatures processed: " + std::to_string(frameStats_.creaturesProcessed));
        VKMON_INFO("  Detections triggered: " + std::to_string(frameStats_.detectionsTriggered));
        VKMON_INFO("  State changes: " + std::to_string(frameStats_.stateChanges));
        VKMON_INFO("  Update time: " + std::to_string(frameStats_.updateTimeMs) + "ms");
    }
};

} // namespace VulkanMon