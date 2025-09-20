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
#include <cassert>

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
    SpatialSystem* spatialSystem_ = nullptr;  // Non-owning, lifetime managed by World

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
        VKMON_INFO("CreatureDetectionSystem: SpatialSystem reference updated");
    }

    void update(float deltaTime, EntityManager& entityManager) override {
        // Debug safety assertion (zero cost in release builds)
        assert(spatialSystem_ && "SpatialSystem must be set before creature detection");

        if (!spatialSystem_) {
            VKMON_WARNING("CreatureDetectionSystem: No spatial system available");
            return;
        }

        auto start = std::chrono::high_resolution_clock::now();
        frameStats_ = DetectionStats{};

        // PERFORMANCE OPTIMIZATION: Batch all spatial queries instead of per-creature queries
        auto creatureEntities = collectCreatureEntities(entityManager);
        frameStats_.creaturesProcessed = creatureEntities.size();

        if (creatureEntities.empty()) {
            frameStats_.updateTimeMs = 0.0f;
            return;
        }

        // Build batched spatial queries for all creatures that need detection checks
        std::vector<SpatialManager::BatchedRadiusQuery> batchedQueries;
        std::vector<std::pair<EntityID, CreatureComponent*>> queriedCreatures;

        prepareBatchedQueries(creatureEntities, entityManager, deltaTime, batchedQueries, queriedCreatures);

        // Execute all spatial queries in a single batch operation (eliminates mutex overhead)
        auto spatialResults = spatialSystem_->getSpatialManager()->queryRadiusBatch(batchedQueries);

        // Process detection results for each creature
        processBatchedDetectionResults(spatialResults, queriedCreatures, entityManager);

        // Update all creature states (non-spatial logic)
        updateAllCreatureStates(creatureEntities, entityManager, deltaTime);

        auto end = std::chrono::high_resolution_clock::now();
        frameStats_.updateTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        // Log performance occasionally - reduced frequency to every 10 seconds
        static int frameCount = 0;
        if (++frameCount >= 1800) { // Every ~10 seconds at 180 FPS
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
    // PERFORMANCE OPTIMIZATION: Batched processing methods

    std::vector<EntityID> collectCreatureEntities(EntityManager& entityManager) {
        std::vector<EntityID> creatures;
        auto& entityIds = entityManager.getEntitiesWithComponent<Transform>();

        for (EntityID entity : entityIds) {
            if (entityManager.hasComponent<SpatialComponent>(entity) &&
                entityManager.hasComponent<CreatureComponent>(entity)) {
                creatures.push_back(entity);
            }
        }

        return creatures;
    }

    void prepareBatchedQueries(const std::vector<EntityID>& creatures, EntityManager& entityManager,
                              float deltaTime, std::vector<SpatialManager::BatchedRadiusQuery>& batchedQueries,
                              std::vector<std::pair<EntityID, CreatureComponent*>>& queriedCreatures) {

        // Safety check: prevent division by zero in temporal spreading
        if (creatures.empty()) {
            return;
        }

        // TEMPORAL SPREADING OPTIMIZATION: Limit creatures processed per frame to eliminate frame time spikes
        static size_t frameOffset = 0;
        const size_t MAX_CREATURES_PER_FRAME = 64;  // Process max 64 creatures per frame (1024/16 = 64 per frame cycle)

        frameOffset = (frameOffset + MAX_CREATURES_PER_FRAME) % creatures.size();
        size_t endOffset = std::min(frameOffset + MAX_CREATURES_PER_FRAME, creatures.size());

        for (size_t i = frameOffset; i < endOffset; ++i) {
            EntityID entity = creatures[i];
            auto& transform = entityManager.getComponent<Transform>(entity);
            auto& creature = entityManager.getComponent<CreatureComponent>(entity);

            // Update timing first
            creature.lastDetectionCheck += deltaTime;
            creature.alertTimer += deltaTime;

            // Only add to batch if creature needs detection check
            if (creature.lastDetectionCheck >= creature.detectionCheckInterval) {
                creature.lastDetectionCheck = 0.0f;

                SpatialManager::BatchedRadiusQuery query;
                query.sourceEntity = entity;
                query.center = transform.position;
                query.radius = creature.detectionRadius;
                query.layerMask = LayerMask::Player | LayerMask::Creatures;

                batchedQueries.push_back(query);
                queriedCreatures.push_back({entity, &creature});
            }
        }
    }

    void processBatchedDetectionResults(const std::vector<SpatialManager::BatchedQueryResult>& spatialResults,
                                       const std::vector<std::pair<EntityID, CreatureComponent*>>& queriedCreatures,
                                       EntityManager& entityManager) {

        for (size_t i = 0; i < spatialResults.size(); ++i) {
            const auto& result = spatialResults[i];
            EntityID entity = queriedCreatures[i].first;
            CreatureComponent* creature = queriedCreatures[i].second;

            auto& transform = entityManager.getComponent<Transform>(entity);
            processDetectedEntities(entity, transform, *creature, result.nearbyEntities, entityManager);
        }
    }

    void updateAllCreatureStates(const std::vector<EntityID>& creatures, EntityManager& entityManager, float deltaTime) {
        for (EntityID entity : creatures) {
            auto& transform = entityManager.getComponent<Transform>(entity);
            auto& creature = entityManager.getComponent<CreatureComponent>(entity);
            updateCreatureState(entity, transform, creature, deltaTime);
        }
    }

    void processDetectedEntities(EntityID entity, const Transform& transform,
                                CreatureComponent& creature, const std::vector<EntityID>& nearbyEntities, EntityManager& entityManager) {

        EntityID closestPlayer = INVALID_ENTITY;
        float closestPlayerDistance = creature.detectionRadius;

        // PERFORMANCE OPTIMIZATION: Spatial query already filtered by layer mask!
        // Since we queried with LayerMask::Player | LayerMask::Creatures,
        // we only need to check which ones are players (much faster!)
        for (EntityID nearbyEntity : nearbyEntities) {
            if (nearbyEntity == entity) continue; // Skip self

            // FAST PATH: Use spatial layer cache (already loaded by spatial query)
            // Only check entities that are in Player layer - spatial query pre-filtered these!
            static EntityID cachedPlayerEntity = INVALID_ENTITY;
            static glm::vec3 cachedPlayerPos;

            // Simple check: is this the known player entity?
            if (nearbyEntity == cachedPlayerEntity ||
                (cachedPlayerEntity == INVALID_ENTITY &&
                 entityManager.hasComponent<SpatialComponent>(nearbyEntity))) {

                if (nearbyEntity != cachedPlayerEntity) {
                    auto& spatial = entityManager.getComponent<SpatialComponent>(nearbyEntity);
                    if ((spatial.spatialLayers & LayerMask::Player) != LayerMask::None) {
                        cachedPlayerEntity = nearbyEntity;
                        auto& playerTransform = entityManager.getComponent<Transform>(nearbyEntity);
                        cachedPlayerPos = playerTransform.position;
                    } else {
                        continue; // Not a player
                    }
                }

                float distance = glm::distance(transform.position, cachedPlayerPos);
                if (distance < closestPlayerDistance) {
                    closestPlayer = cachedPlayerEntity;
                    closestPlayerDistance = distance;
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
                // Target: ~10% chance per second to start wandering
                {
                    float wanderChancePerSecond = 0.1f;
                    float wanderChanceThisFrame = 1.0f - std::pow(1.0f - wanderChancePerSecond, deltaTime);
                    if (randomDistribution_(randomGenerator_) < wanderChanceThisFrame) {
                        creature.state = CreatureState::WANDERING;
                        frameStats_.stateChanges++;
                    }
                }
                break;

            case CreatureState::WANDERING:
                // Target: ~5% chance per second to return to idle
                {
                    float idleChancePerSecond = 0.05f;
                    float idleChanceThisFrame = 1.0f - std::pow(1.0f - idleChancePerSecond, deltaTime);
                    if (randomDistribution_(randomGenerator_) < idleChanceThisFrame) {
                        creature.state = CreatureState::IDLE;
                        frameStats_.stateChanges++;
                    }
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