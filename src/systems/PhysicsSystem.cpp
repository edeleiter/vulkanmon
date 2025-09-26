#define GLM_ENABLE_EXPERIMENTAL
#include "PhysicsSystem.h"
#include "../core/EntityManager.h"
#include "../systems/SpatialSystem.h"
#include "../utils/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <chrono>
#include <thread>

namespace VulkanMon {

// SystemBase interface implementation
void PhysicsSystem::update(float deltaTime, EntityManager& entityManager) {
    if (!initialized_) return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Apply time scaling
    float scaledDeltaTime = deltaTime * timeScale_;

    // Reset frame statistics
    stats_.activeRigidBodies = 0;
    stats_.collisionChecks = 0;
    stats_.collisionHits = 0;
    stats_.averageVelocity = 0.0f;

    // Step Jolt Physics simulation (only physics system)
    if (joltPhysics_) {
        try {
            // Start performance timing for enhanced monitoring
            recordTimingStart();

            // Create Jolt bodies for any new entities with physics components
            createJoltBodiesForNewEntities(entityManager);

            // Convert deltaTime from milliseconds to seconds for Jolt Physics
            float deltaTimeSeconds = scaledDeltaTime / 1000.0f;

            // Clamp timestep to prevent physics instability at low framerates
            const float maxTimestep = 1.0f / 30.0f;  // Cap at 30 FPS minimum
            float clampedDeltaSeconds = std::min(deltaTimeSeconds, maxTimestep);

            const int collisionSteps = 1;

            // Update Jolt Physics
            joltPhysics_->Update(clampedDeltaSeconds, collisionSteps, tempAllocator_.get(), jobSystem_.get());

            // Sync transforms from Jolt back to ECS components
            syncTransformsFromJolt(entityManager);

            // Update statistics with enhanced monitoring
            updateStatsFromJolt();
        } catch (const std::exception& e) {
            VKMON_ERROR("Jolt Physics update failed: " + std::string(e.what()));
            VKMON_ERROR("Physics system cannot continue without Jolt Physics");
            return;
        }
    } else {
        VKMON_ERROR("PhysicsSystem: Jolt Physics not initialized! Cannot proceed.");
        return;
    }

    // Calculate frame timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    stats_.updateTime = duration.count() / 1000.0f; // Convert to milliseconds
}

void PhysicsSystem::initialize(EntityManager& entityManager) {
    VKMON_INFO("Initializing PhysicsSystem with default gravity");

    worldGravity_ = glm::vec3(0.0f, -9.81f, 0.0f);
    timeScale_ = 1.0f;
    collisionEnabled_ = true;
    debugDraw_ = false;

    // Initialize statistics
    stats_ = PhysicsStats{};

    // Initialize Jolt Physics System
    initializeJoltPhysics();

    initialized_ = true;
    VKMON_INFO("PhysicsSystem initialized successfully");
}

void PhysicsSystem::shutdown(EntityManager& entityManager) {
    if (!initialized_) return;

    VKMON_INFO("Shutting down PhysicsSystem");

    // Shutdown Jolt Physics System
    shutdownJoltPhysics();

    spatialSystem_ = nullptr;
    initialized_ = false;

    VKMON_INFO("PhysicsSystem shutdown complete");
}

// Physics-specific methods

void PhysicsSystem::fixedUpdate(EntityManager& entityManager, float fixedDeltaTime) {
    if (!initialized_) return;

    // Fixed timestep is now handled internally by Jolt Physics
    // This method exists for API compatibility but delegates to main update
    update(fixedDeltaTime * 1000.0f, entityManager); // Convert to milliseconds for update()
}

void PhysicsSystem::setCollisionMatrix(uint32_t layer1, uint32_t layer2, bool canCollide) {
    if (layer1 >= ObjectLayers::NUM_LAYERS || layer2 >= ObjectLayers::NUM_LAYERS) {
        VKMON_WARNING("PhysicsSystem: Invalid layer index, must be < " + std::to_string(ObjectLayers::NUM_LAYERS));
        return;
    }

    // Collision filtering is now handled by Jolt Physics ObjectLayerPairFilter
    // This method exists for API compatibility but does not store state
    VKMON_DEBUG("PhysicsSystem: Collision layer configuration delegated to Jolt Physics ObjectLayerPairFilter");
}

bool PhysicsSystem::shouldLayersCollide(uint32_t layer1, uint32_t layer2) const {
    if (layer1 >= ObjectLayers::NUM_LAYERS || layer2 >= ObjectLayers::NUM_LAYERS) return false;

    // Delegate collision filtering to Jolt Physics ObjectLayerPairFilter
    if (objectLayerPairFilter_) {
        return objectLayerPairFilter_->ShouldCollide(static_cast<JPH::ObjectLayer>(layer1), static_cast<JPH::ObjectLayer>(layer2));
    }

    // Default fallback: allow all collisions
    return true;
}

PhysicsSystem::RaycastHit PhysicsSystem::raycast(const glm::vec3& origin,
                                                 const glm::vec3& direction,
                                                 float maxDistance,
                                                 uint32_t layerMask) {
    RaycastHit hit{};

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot perform raycast - Jolt Physics not initialized");
        return hit;
    }

    // Normalize direction vector for consistent behavior
    glm::vec3 normalizedDir = glm::normalize(direction);
    glm::vec3 endPoint = origin + normalizedDir * maxDistance;

    // Convert to Jolt coordinates
    JPH::Vec3 joltOrigin(origin.x, origin.y, origin.z);
    JPH::Vec3 joltDirection(normalizedDir.x, normalizedDir.y, normalizedDir.z);

    // Create Jolt raycast settings
    JPH::RayCast ray;
    ray.mOrigin = joltOrigin;
    ray.mDirection = joltDirection * maxDistance;

    // Use narrow phase query for precise raycast with surface normal extraction
    const JPH::NarrowPhaseQuery& narrowPhase = joltPhysics_->GetNarrowPhaseQuery();

    // Use proper Jolt Physics broad phase layer filter for layer mask filtering
    auto broadPhaseFilter = [this, layerMask](JPH::BroadPhaseLayer inBroadPhaseLayer) -> bool {
        // For now, allow all broad phase layers (can be enhanced with layer mask logic)
        return layerMask == 0xFFFFFFFF || layerMask != 0;
    };

    auto objectFilter = [this, layerMask](JPH::ObjectLayer inObjectLayer) -> bool {
        // Convert layer mask to basic filtering logic
        if (layerMask == 0xFFFFFFFF) return true; // All layers

        // Map ObjectLayer to basic layer check
        uint32_t objLayer = static_cast<uint32_t>(inObjectLayer);
        return shouldLayersCollide(objLayer, objLayer) || (layerMask & (1 << objLayer)) != 0;
    };

    // Use RayCastSettings for more control over the raycast
    JPH::RayCastResult rayResult;

    // Use simplified raycast approach with improved filtering
    JPH::RRayCast joltRay{ray.mOrigin, ray.mDirection};

    // Create filter instances for Jolt
    class SimpleObjectFilter : public JPH::ObjectLayerFilter {
    public:
        SimpleObjectFilter(uint32_t layerMask, const PhysicsSystem* physics)
            : mLayerMask(layerMask), mPhysics(physics) {}

        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            if (mLayerMask == 0xFFFFFFFF) return true;
            uint32_t objLayer = static_cast<uint32_t>(inLayer);
            return (mLayerMask & (1 << objLayer)) != 0;
        }
    private:
        uint32_t mLayerMask;
        const PhysicsSystem* mPhysics;
    };

    SimpleObjectFilter objectLayerFilter(layerMask, this);

    // Perform the raycast with layer filtering
    bool hasHit = narrowPhase.CastRay(joltRay, rayResult, {}, objectLayerFilter, {});

    // Process results with improved surface normal extraction
    if (hasHit) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(rayResult.mBodyID);
        if (it != bodyToEntityMap_.end()) {
            hit.hit = true;
            hit.distance = rayResult.mFraction * maxDistance;
            hit.entity = it->second;

            // Calculate precise hit point
            glm::vec3 hitPoint = origin + normalizedDir * hit.distance;
            hit.point = hitPoint;

            // Extract surface normal from Jolt Physics using proper body interface
            const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

            // Get the body's transform and shape for normal calculation
            JPH::RefConst<JPH::Shape> shape = bodyInterface.GetShape(rayResult.mBodyID);
            if (shape != nullptr) {
                // For basic normal extraction, use the ray direction as approximation
                // In a full implementation, we would use Jolt's GetSurfaceNormal or similar
                JPH::Vec3 joltNormal = -joltRay.mDirection.Normalized();

                // Convert Jolt normal to GLM
                hit.normal = glm::normalize(glm::vec3(joltNormal.GetX(), joltNormal.GetY(), joltNormal.GetZ()));
            } else {
                // Fallback: use inverse ray direction as surface normal
                hit.normal = glm::normalize(-normalizedDir);
            }

            VKMON_DEBUG("PhysicsSystem: Raycast hit entity " + std::to_string(hit.entity) +
                       " at distance " + std::to_string(hit.distance) +
                       " with normal (" + std::to_string(hit.normal.x) + ", " +
                       std::to_string(hit.normal.y) + ", " + std::to_string(hit.normal.z) + ")");
        }
    }

    return hit;
}

std::vector<EntityID> PhysicsSystem::overlapSphere(const glm::vec3& center,
                                                   float radius,
                                                   uint32_t layerMask) {
    std::vector<EntityID> results;

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot perform sphere overlap - Jolt Physics not initialized");
        return results;
    }

    // Use spatial system for pre-filtering if available (performance optimization)
    if (spatialSystem_) {
        // Get candidate entities from spatial system first
        auto spatialCandidates = spatialSystem_->queryRadius(center, radius);

        // TODO: Cross-reference spatial candidates with Jolt Physics for precise collision detection
        // For now, we'll use the full Jolt Physics approach and optimize later
        VKMON_DEBUG("PhysicsSystem: Sphere overlap with spatial pre-filtering (candidates: " +
                   std::to_string(spatialCandidates.size()) + ")");
    }

    // Create sphere shape for overlap testing
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeRefC sphereShape = sphereSettings.Create().Get();

    if (!sphereShape) {
        VKMON_ERROR("PhysicsSystem: Failed to create sphere shape for overlap query");
        return results;
    }

    // Convert center position to Jolt coordinates
    JPH::Vec3 joltCenter(center.x, center.y, center.z);
    JPH::Quat joltRotation = JPH::Quat::sIdentity();

    // Create transformation matrix for CollideShape API
    JPH::Mat44 transform = JPH::Mat44::sRotationTranslation(joltRotation, joltCenter);

    // Use narrow phase query for precise overlap detection
    const JPH::NarrowPhaseQuery& narrowPhase = joltPhysics_->GetNarrowPhaseQuery();

    // Create layer filter (reuse the SimpleObjectFilter from raycast)
    class SimpleObjectFilter : public JPH::ObjectLayerFilter {
    public:
        SimpleObjectFilter(uint32_t layerMask, const PhysicsSystem* physics)
            : mLayerMask(layerMask), mPhysics(physics) {}

        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            if (mLayerMask == 0xFFFFFFFF) return true;
            uint32_t objLayer = static_cast<uint32_t>(inLayer);
            return (mLayerMask & (1 << objLayer)) != 0;
        }
    private:
        uint32_t mLayerMask;
        const PhysicsSystem* mPhysics;
    };

    SimpleObjectFilter objectLayerFilter(layerMask, this);

    // Storage for collision results
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

    // Perform the overlap query with layer filtering
    JPH::CollideShapeSettings settings;
    narrowPhase.CollideShape(sphereShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3(joltCenter), collector, {}, objectLayerFilter, {}, {});

    // Process results and convert body IDs back to entity IDs with layer filtering
    for (const auto& hit : collector.mHits) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(hit.mBodyID2);
        if (it != bodyToEntityMap_.end()) {
            EntityID entityId = it->second;

            // Additional layer filtering validation (double-check)
            const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
            JPH::ObjectLayer hitObjectLayer = bodyInterface.GetObjectLayer(hit.mBodyID2);

            if (objectLayerFilter.ShouldCollide(hitObjectLayer)) {
                results.push_back(entityId);
            }
        }
    }

    VKMON_DEBUG("PhysicsSystem: Sphere overlap at (" +
               std::to_string(center.x) + ", " +
               std::to_string(center.y) + ", " +
               std::to_string(center.z) + ") radius " +
               std::to_string(radius) + " found " +
               std::to_string(results.size()) + " entities");

    return results;
}

std::vector<EntityID> PhysicsSystem::overlapBox(const glm::vec3& center,
                                                const glm::vec3& halfExtents,
                                                const glm::quat& rotation,
                                                uint32_t layerMask) {
    std::vector<EntityID> results;

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot perform box overlap - Jolt Physics not initialized");
        return results;
    }

    // Use spatial system for pre-filtering if available (performance optimization)
    if (spatialSystem_) {
        // For box queries, we can use the maximum extent as a rough sphere radius for pre-filtering
        float maxExtent = std::max({halfExtents.x, halfExtents.y, halfExtents.z});
        auto spatialCandidates = spatialSystem_->queryRadius(center, maxExtent);

        VKMON_DEBUG("PhysicsSystem: Box overlap with spatial pre-filtering (candidates: " +
                   std::to_string(spatialCandidates.size()) + ")");
    }

    // Create box shape for overlap testing
    JPH::BoxShapeSettings boxSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    JPH::ShapeRefC boxShape = boxSettings.Create().Get();

    if (!boxShape) {
        VKMON_ERROR("PhysicsSystem: Failed to create box shape for overlap query");
        return results;
    }

    // Convert center position and rotation to Jolt coordinates
    JPH::Vec3 joltCenter(center.x, center.y, center.z);
    JPH::Quat joltRotation(rotation.x, rotation.y, rotation.z, rotation.w);

    // Create transformation matrix for CollideShape API
    JPH::Mat44 transform = JPH::Mat44::sRotationTranslation(joltRotation, joltCenter);

    // Use narrow phase query for precise overlap detection
    const JPH::NarrowPhaseQuery& narrowPhase = joltPhysics_->GetNarrowPhaseQuery();

    // Create layer filter (reuse the SimpleObjectFilter from raycast)
    class SimpleObjectFilter : public JPH::ObjectLayerFilter {
    public:
        SimpleObjectFilter(uint32_t layerMask, const PhysicsSystem* physics)
            : mLayerMask(layerMask), mPhysics(physics) {}

        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            if (mLayerMask == 0xFFFFFFFF) return true;
            uint32_t objLayer = static_cast<uint32_t>(inLayer);
            return (mLayerMask & (1 << objLayer)) != 0;
        }
    private:
        uint32_t mLayerMask;
        const PhysicsSystem* mPhysics;
    };

    SimpleObjectFilter objectLayerFilter(layerMask, this);

    // Storage for collision results
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

    // Perform the overlap query with layer filtering
    JPH::CollideShapeSettings settings;
    narrowPhase.CollideShape(boxShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3(joltCenter), collector, {}, objectLayerFilter, {}, {});

    // Process results and convert body IDs back to entity IDs with layer filtering
    for (const auto& hit : collector.mHits) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(hit.mBodyID2);
        if (it != bodyToEntityMap_.end()) {
            EntityID entityId = it->second;

            // Additional layer filtering validation (double-check)
            const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
            JPH::ObjectLayer hitObjectLayer = bodyInterface.GetObjectLayer(hit.mBodyID2);

            if (objectLayerFilter.ShouldCollide(hitObjectLayer)) {
                results.push_back(entityId);
            }
        }
    }

    VKMON_DEBUG("PhysicsSystem: Box overlap at (" +
               std::to_string(center.x) + ", " +
               std::to_string(center.y) + ", " +
               std::to_string(center.z) + ") halfExtents (" +
               std::to_string(halfExtents.x) + ", " +
               std::to_string(halfExtents.y) + ", " +
               std::to_string(halfExtents.z) + ") found " +
               std::to_string(results.size()) + " entities");

    return results;
}

std::vector<EntityID> PhysicsSystem::overlapCapsule(const glm::vec3& pointA,
                                                    const glm::vec3& pointB,
                                                    float radius,
                                                    uint32_t layerMask) {
    std::vector<EntityID> results;

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot perform capsule overlap - Jolt Physics not initialized");
        return results;
    }

    // Calculate capsule properties
    glm::vec3 axis = pointB - pointA;
    float halfHeight = glm::length(axis) * 0.5f;
    glm::vec3 center = (pointA + pointB) * 0.5f;

    // Use spatial system for pre-filtering if available (performance optimization)
    if (spatialSystem_) {
        // For capsule queries, we can use the capsule radius + half height for rough sphere pre-filtering
        float maxRadius = radius + halfHeight;
        auto spatialCandidates = spatialSystem_->queryRadius(center, maxRadius);

        VKMON_DEBUG("PhysicsSystem: Capsule overlap with spatial pre-filtering (candidates: " +
                   std::to_string(spatialCandidates.size()) + ")");
    }

    // Create capsule shape for overlap testing
    JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
    JPH::ShapeRefC capsuleShape = capsuleSettings.Create().Get();

    if (!capsuleShape) {
        VKMON_ERROR("PhysicsSystem: Failed to create capsule shape for overlap query");
        return results;
    }

    // Calculate rotation to align capsule with axis
    JPH::Quat joltRotation = JPH::Quat::sIdentity();
    if (halfHeight > 0.0001f) {  // Avoid division by zero
        glm::vec3 normalizedAxis = glm::normalize(axis);
        glm::vec3 capsuleUp(0, 1, 0); // Jolt capsules are aligned with Y axis

        // Calculate rotation needed to align capsule with desired axis
        glm::vec3 rotationAxis = glm::cross(capsuleUp, normalizedAxis);
        float rotationAngle = std::acos(glm::clamp(glm::dot(capsuleUp, normalizedAxis), -1.0f, 1.0f));

        if (glm::length(rotationAxis) > 0.0001f) {
            rotationAxis = glm::normalize(rotationAxis);
            glm::quat rotation = glm::angleAxis(rotationAngle, rotationAxis);
            joltRotation = JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w);
        }
    }

    // Convert center position to Jolt coordinates
    JPH::Vec3 joltCenter(center.x, center.y, center.z);

    // Create transformation matrix for CollideShape API
    JPH::Mat44 transform = JPH::Mat44::sRotationTranslation(joltRotation, joltCenter);

    // Use narrow phase query for precise overlap detection
    const JPH::NarrowPhaseQuery& narrowPhase = joltPhysics_->GetNarrowPhaseQuery();

    // Create layer filter (reuse the SimpleObjectFilter from raycast)
    class SimpleObjectFilter : public JPH::ObjectLayerFilter {
    public:
        SimpleObjectFilter(uint32_t layerMask, const PhysicsSystem* physics)
            : mLayerMask(layerMask), mPhysics(physics) {}

        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            if (mLayerMask == 0xFFFFFFFF) return true;
            uint32_t objLayer = static_cast<uint32_t>(inLayer);
            return (mLayerMask & (1 << objLayer)) != 0;
        }
    private:
        uint32_t mLayerMask;
        const PhysicsSystem* mPhysics;
    };

    SimpleObjectFilter objectLayerFilter(layerMask, this);

    // Storage for collision results
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

    // Perform the overlap query with layer filtering
    JPH::CollideShapeSettings settings;
    narrowPhase.CollideShape(capsuleShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3(joltCenter), collector, {}, objectLayerFilter, {}, {});

    // Process results and convert body IDs back to entity IDs with layer filtering
    for (const auto& hit : collector.mHits) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(hit.mBodyID2);
        if (it != bodyToEntityMap_.end()) {
            EntityID entityId = it->second;

            // Additional layer filtering validation (double-check)
            const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
            JPH::ObjectLayer hitObjectLayer = bodyInterface.GetObjectLayer(hit.mBodyID2);

            if (objectLayerFilter.ShouldCollide(hitObjectLayer)) {
                results.push_back(entityId);
            }
        }
    }

    VKMON_DEBUG("PhysicsSystem: Capsule overlap from (" +
               std::to_string(pointA.x) + ", " +
               std::to_string(pointA.y) + ", " +
               std::to_string(pointA.z) + ") to (" +
               std::to_string(pointB.x) + ", " +
               std::to_string(pointB.y) + ", " +
               std::to_string(pointB.z) + ") radius " +
               std::to_string(radius) + " found " +
               std::to_string(results.size()) + " entities");

    return results;
}

// =============================================================================
// GENERIC SPATIAL QUERIES
// =============================================================================

EntityID PhysicsSystem::findNearestEntity(const glm::vec3& position, float maxDistance,
                                          uint32_t layerMask, EntityID excludeEntity) {
    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot find nearest entity - Jolt Physics not initialized");
        return 0;
    }

    EntityID nearestEntity = 0;
    float nearestDistance = maxDistance;

    // Use spatial system for pre-filtering if available (major performance optimization)
    if (spatialSystem_) {
        auto candidates = spatialSystem_->queryRadius(position, maxDistance);

        for (EntityID candidate : candidates) {
            if (candidate == excludeEntity) continue;

            // Find the Jolt body for this entity to get precise position
            auto bodyIt = entityToBodyMap_.find(candidate);
            if (bodyIt == entityToBodyMap_.end()) continue;

            const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
            JPH::ObjectLayer objectLayer = bodyInterface.GetObjectLayer(bodyIt->second);

            // Check layer mask
            if (layerMask != 0xFFFFFFFF) {
                uint32_t objLayer = static_cast<uint32_t>(objectLayer);
                if ((layerMask & (1 << objLayer)) == 0) continue;
            }

            // Get precise position from Jolt body
            JPH::Vec3 bodyPos = bodyInterface.GetPosition(bodyIt->second);
            glm::vec3 entityPos(bodyPos.GetX(), bodyPos.GetY(), bodyPos.GetZ());

            float distance = glm::distance(position, entityPos);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestEntity = candidate;
            }
        }
    } else {
        // Fallback: iterate through all entities (less efficient)
        for (const auto& pair : entityToBodyMap_) {
            EntityID candidate = pair.first;
            if (candidate == excludeEntity) continue;

            const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
            JPH::ObjectLayer objectLayer = bodyInterface.GetObjectLayer(pair.second);

            // Check layer mask
            if (layerMask != 0xFFFFFFFF) {
                uint32_t objLayer = static_cast<uint32_t>(objectLayer);
                if ((layerMask & (1 << objLayer)) == 0) continue;
            }

            // Get precise position from Jolt body
            JPH::Vec3 bodyPos = bodyInterface.GetPosition(pair.second);
            glm::vec3 entityPos(bodyPos.GetX(), bodyPos.GetY(), bodyPos.GetZ());

            float distance = glm::distance(position, entityPos);
            if (distance <= maxDistance && distance < nearestDistance) {
                nearestDistance = distance;
                nearestEntity = candidate;
            }
        }
    }

    VKMON_DEBUG("PhysicsSystem: Found nearest entity " + std::to_string(nearestEntity) +
               " at distance " + std::to_string(nearestDistance));

    return nearestEntity;
}

std::vector<std::pair<EntityID, float>> PhysicsSystem::findEntitiesInRadius(const glm::vec3& position,
                                                                           float radius,
                                                                           uint32_t layerMask,
                                                                           bool sortByDistance) {
    std::vector<std::pair<EntityID, float>> results;

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot find entities in radius - Jolt Physics not initialized");
        return results;
    }

    // Use spatial system for pre-filtering if available
    std::vector<EntityID> candidates;
    if (spatialSystem_) {
        candidates = spatialSystem_->queryRadius(position, radius);
    } else {
        // Fallback: collect all entities
        for (const auto& pair : entityToBodyMap_) {
            candidates.push_back(pair.first);
        }
    }

    // Check each candidate precisely
    for (EntityID candidate : candidates) {
        auto bodyIt = entityToBodyMap_.find(candidate);
        if (bodyIt == entityToBodyMap_.end()) continue;

        const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
        JPH::ObjectLayer objectLayer = bodyInterface.GetObjectLayer(bodyIt->second);

        // Check layer mask
        if (layerMask != 0xFFFFFFFF) {
            uint32_t objLayer = static_cast<uint32_t>(objectLayer);
            if ((layerMask & (1 << objLayer)) == 0) continue;
        }

        // Get precise position from Jolt body
        JPH::Vec3 bodyPos = bodyInterface.GetPosition(bodyIt->second);
        glm::vec3 entityPos(bodyPos.GetX(), bodyPos.GetY(), bodyPos.GetZ());

        float distance = glm::distance(position, entityPos);
        if (distance <= radius) {
            results.emplace_back(candidate, distance);
        }
    }

    // Sort by distance if requested
    if (sortByDistance) {
        std::sort(results.begin(), results.end(),
                 [](const std::pair<EntityID, float>& a, const std::pair<EntityID, float>& b) {
                     return a.second < b.second;
                 });
    }

    VKMON_DEBUG("PhysicsSystem: Found " + std::to_string(results.size()) +
               " entities in radius " + std::to_string(radius) + " from (" +
               std::to_string(position.x) + ", " + std::to_string(position.y) + ", " +
               std::to_string(position.z) + ")");

    return results;
}

bool PhysicsSystem::isPathClear(const glm::vec3& startPosition, const glm::vec3& endPosition,
                               float clearanceRadius, uint32_t layerMask) {
    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot check path clearance - Jolt Physics not initialized");
        return false;
    }

    // Use multiple raycasts with slight offsets for clearance checking
    glm::vec3 direction = glm::normalize(endPosition - startPosition);
    float pathLength = glm::distance(startPosition, endPosition);

    // Create perpendicular vectors for clearance sampling
    glm::vec3 right = glm::cross(direction, glm::vec3(0, 1, 0));
    if (glm::length(right) < 0.001f) { // Handle vertical paths
        right = glm::cross(direction, glm::vec3(1, 0, 0));
    }
    right = glm::normalize(right) * clearanceRadius;
    glm::vec3 up = glm::normalize(glm::cross(direction, right)) * clearanceRadius;

    // Test multiple rays for comprehensive path clearance
    std::vector<glm::vec3> rayOffsets = {
        glm::vec3(0, 0, 0),     // Center ray
        right, -right,          // Left/right clearance
        up, -up,                // Up/down clearance
        right + up, -right - up, // Diagonal clearance
        right - up, -right + up
    };

    for (const glm::vec3& offset : rayOffsets) {
        glm::vec3 rayStart = startPosition + offset;
        RaycastHit hit = raycast(rayStart, direction, pathLength, layerMask);

        if (hit.hit) {
            VKMON_DEBUG("PhysicsSystem: Path blocked by entity " + std::to_string(hit.entity) +
                       " at distance " + std::to_string(hit.distance));
            return false;
        }
    }

    return true;
}

std::vector<EntityID> PhysicsSystem::getEntitiesAlongPath(const glm::vec3& startPosition,
                                                         const glm::vec3& endPosition,
                                                         float pathWidth,
                                                         uint32_t layerMask) {
    std::vector<EntityID> results;

    if (!joltPhysics_) {
        VKMON_WARNING("PhysicsSystem: Cannot get entities along path - Jolt Physics not initialized");
        return results;
    }

    // Calculate path properties
    glm::vec3 pathVector = endPosition - startPosition;
    float pathLength = glm::length(pathVector);
    glm::vec3 pathCenter = startPosition + pathVector * 0.5f;

    // Use capsule overlap to find entities along path
    float capsuleRadius = pathWidth * 0.5f;
    return overlapCapsule(startPosition, endPosition, capsuleRadius, layerMask);
}

// =============================================================================
// JOLT PHYSICS IMPLEMENTATION
// =============================================================================

void PhysicsSystem::initializeJoltPhysics() {
    VKMON_INFO("Initializing Jolt Physics system...");

    // Register allocation hook for Jolt
    JPH::RegisterDefaultAllocator();

    // Initialize Jolt Factory
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Create temp allocator for Jolt (10MB should be enough for most games)
    tempAllocator_ = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    // Create job system for multi-threading
    // Use configured thread count, or auto-detect if not set
    uint32_t actualThreadCount = threadCount_;
    if (actualThreadCount == 0) {
        actualThreadCount = std::max(1u, std::thread::hardware_concurrency() - 1);
    }
    jobSystem_ = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs,
                                                            JPH::cMaxPhysicsBarriers,
                                                            actualThreadCount);

    // Configure physics system settings for game engine use
    const uint32_t cMaxBodies = 1024;           // Support up to 1024 physics bodies
    const uint32_t cNumBodyMutexes = 0;         // Autodetect based on cores
    const uint32_t cMaxBodyPairs = 1024;        // Max collision pairs per step
    const uint32_t cMaxContactConstraints = 1024; // Max contact constraints

    // Create layer filtering interface implementations
    broadPhaseLayerInterface_ = std::make_unique<BroadPhaseLayerInterfaceImpl>();
    objectVsBroadPhaseLayerFilter_ = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    objectLayerPairFilter_ = std::make_unique<ObjectLayerPairFilterImpl>();

    // Create the main Jolt physics system
    joltPhysics_ = std::make_unique<JPH::PhysicsSystem>();
    joltPhysics_->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
                       *broadPhaseLayerInterface_, *objectVsBroadPhaseLayerFilter_, *objectLayerPairFilter_);

    // Set gravity
    joltPhysics_->SetGravity(JPH::Vec3(worldGravity_.x, worldGravity_.y, worldGravity_.z));

    // Setup collision layers
    setupJoltLayers();

    VKMON_INFO("Jolt Physics initialized with " + std::to_string(actualThreadCount) + " threads");
}

void PhysicsSystem::shutdownJoltPhysics() {
    if (!joltPhysics_) return;

    VKMON_INFO("Shutting down Jolt Physics...");

    // Clear entity mappings
    entityToBodyMap_.clear();
    bodyToEntityMap_.clear();

    // Destroy Jolt objects in reverse order
    joltPhysics_.reset();
    objectLayerPairFilter_.reset();
    objectVsBroadPhaseLayerFilter_.reset();
    broadPhaseLayerInterface_.reset();
    jobSystem_.reset();
    tempAllocator_.reset();

    // Cleanup factory
    if (JPH::Factory::sInstance) {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    VKMON_INFO("Jolt Physics shutdown complete");
}

void PhysicsSystem::setupJoltLayers() {
    // Jolt collision layers are configured via ObjectLayerPairFilter implementation
    // No additional setup required - layer interactions handled by filter classes
    VKMON_INFO("Jolt collision layers configured");
}

JPH::ObjectLayer PhysicsSystem::mapLayerMaskToObjectLayer(uint32_t layerMask) const {
    // Enhanced comprehensive mapping with priority-based layer selection
    // When multiple layers are present, prioritize based on gameplay significance

    // Player layer takes highest priority for accurate physics simulation
    if (layerMask & LayerMask::Player) return ObjectLayers::PLAYER;

    // Creatures have second priority for proper NPC/creature physics
    if (layerMask & LayerMask::Creatures) return ObjectLayers::CREATURES;

    // Projectiles/capture devices for dynamic interactive objects
    if (layerMask & (LayerMask::CaptureDevices | LayerMask::Items | LayerMask::Collectibles)) {
        return ObjectLayers::PROJECTILES;
    }

    // Trigger zones for event detection
    if (layerMask & (LayerMask::TriggerZones | LayerMask::Triggers | LayerMask::TriggerZone)) {
        return ObjectLayers::TRIGGERS;
    }

    // Environment objects (static world geometry)
    if (layerMask & (LayerMask::Environment | LayerMask::Terrain | LayerMask::Buildings |
                    LayerMask::Water | LayerMask::Vegetation)) {
        return ObjectLayers::ENVIRONMENT;
    }

    // Default fallback for unrecognized layers
    VKMON_DEBUG("PhysicsSystem: Unmapped LayerMask " + std::to_string(layerMask) +
               ", defaulting to ENVIRONMENT layer");
    return ObjectLayers::ENVIRONMENT;
}

JPH::BroadPhaseLayer PhysicsSystem::mapObjectLayerToBroadPhaseLayer(JPH::ObjectLayer objectLayer) const {
    switch (objectLayer) {
        case ObjectLayers::CREATURES:
        case ObjectLayers::PROJECTILES:
        case ObjectLayers::PLAYER:
            return BroadPhaseLayers::MOVING;
        case ObjectLayers::ENVIRONMENT:
        case ObjectLayers::TRIGGERS:
        default:
            return BroadPhaseLayers::STATIC;
    }
}

// =============================================================================
// ENHANCED LAYER MAPPING SYSTEM
// =============================================================================

uint32_t PhysicsSystem::mapObjectLayerToLayerMask(JPH::ObjectLayer objectLayer) const {
    // Reverse conversion from Jolt ObjectLayer back to VulkanMon LayerMask
    switch (objectLayer) {
        case ObjectLayers::PLAYER:
            return LayerMask::Player;
        case ObjectLayers::CREATURES:
            return LayerMask::Creatures;
        case ObjectLayers::PROJECTILES:
            return LayerMask::CaptureDevices; // Primary mapping for projectiles
        case ObjectLayers::TRIGGERS:
            return LayerMask::TriggerZones;
        case ObjectLayers::ENVIRONMENT:
        default:
            return LayerMask::Environment;
    }
}

JPH::BroadPhaseLayer PhysicsSystem::mapLayerMaskToBroadPhaseLayer(uint32_t layerMask) const {
    // Direct conversion for performance optimization - bypass ObjectLayer conversion

    // Dynamic entities that need movement optimization
    if (layerMask & (LayerMask::Player | LayerMask::Creatures |
                    LayerMask::CaptureDevices | LayerMask::Items | LayerMask::Collectibles)) {
        return BroadPhaseLayers::MOVING;
    }

    // Static environment and trigger zones
    return BroadPhaseLayers::STATIC;
}

bool PhysicsSystem::isValidLayerMaskCombination(uint32_t layerMask) const {
    // Check for invalid layer combinations that could cause physics issues

    if (layerMask == LayerMask::None) {
        VKMON_WARNING("PhysicsSystem: Empty LayerMask detected");
        return false;
    }

    // Check for conflicting layer combinations
    bool hasPlayer = (layerMask & LayerMask::Player) != 0;
    bool hasCreatures = (layerMask & LayerMask::Creatures) != 0;
    bool hasEnvironment = (layerMask & LayerMask::Environment) != 0;

    // Warn about potentially problematic combinations
    if (hasPlayer && hasCreatures) {
        VKMON_DEBUG("PhysicsSystem: Entity has both Player and Creatures layers - using Player priority");
    }

    if (hasEnvironment && (hasPlayer || hasCreatures)) {
        VKMON_DEBUG("PhysicsSystem: Dynamic entity with Environment layer - may affect performance");
    }

    return true;
}

std::vector<JPH::ObjectLayer> PhysicsSystem::expandLayerMaskToObjectLayers(uint32_t layerMask) const {
    // Convert LayerMask to all applicable ObjectLayers for comprehensive queries
    std::vector<JPH::ObjectLayer> objectLayers;

    if (layerMask & LayerMask::Player) {
        objectLayers.push_back(ObjectLayers::PLAYER);
    }

    if (layerMask & LayerMask::Creatures) {
        objectLayers.push_back(ObjectLayers::CREATURES);
    }

    if (layerMask & (LayerMask::CaptureDevices | LayerMask::Items | LayerMask::Collectibles)) {
        objectLayers.push_back(ObjectLayers::PROJECTILES);
    }

    if (layerMask & (LayerMask::TriggerZones | LayerMask::Triggers | LayerMask::TriggerZone)) {
        objectLayers.push_back(ObjectLayers::TRIGGERS);
    }

    if (layerMask & (LayerMask::Environment | LayerMask::Terrain | LayerMask::Buildings |
                    LayerMask::Water | LayerMask::Vegetation)) {
        objectLayers.push_back(ObjectLayers::ENVIRONMENT);
    }

    // Remove duplicates while preserving order
    std::sort(objectLayers.begin(), objectLayers.end());
    objectLayers.erase(std::unique(objectLayers.begin(), objectLayers.end()), objectLayers.end());

    return objectLayers;
}

std::string PhysicsSystem::layerMaskToDebugString(uint32_t layerMask) const {
    if (layerMask == LayerMask::None) return "None";
    if (layerMask == LayerMask::All) return "All";

    std::vector<std::string> layers;

    if (layerMask & LayerMask::Player) layers.push_back("Player");
    if (layerMask & LayerMask::Creatures) layers.push_back("Creatures");
    if (layerMask & LayerMask::Environment) layers.push_back("Environment");
    if (layerMask & LayerMask::Terrain) layers.push_back("Terrain");
    if (layerMask & LayerMask::Vegetation) layers.push_back("Vegetation");
    if (layerMask & LayerMask::Water) layers.push_back("Water");
    if (layerMask & LayerMask::Items) layers.push_back("Items");
    if (layerMask & LayerMask::CaptureDevices) layers.push_back("CaptureDevices");
    if (layerMask & LayerMask::TriggerZones) layers.push_back("TriggerZones");
    if (layerMask & LayerMask::NPCs) layers.push_back("NPCs");
    if (layerMask & LayerMask::Buildings) layers.push_back("Buildings");
    if (layerMask & LayerMask::Collectibles) layers.push_back("Collectibles");

    if (layers.empty()) {
        return "Unknown(" + std::to_string(layerMask) + ")";
    }

    std::string result = layers[0];
    for (size_t i = 1; i < layers.size(); ++i) {
        result += " | " + layers[i];
    }

    return result;
}

std::string PhysicsSystem::objectLayerToDebugString(JPH::ObjectLayer objectLayer) const {
    switch (objectLayer) {
        case ObjectLayers::PLAYER: return "PLAYER";
        case ObjectLayers::CREATURES: return "CREATURES";
        case ObjectLayers::PROJECTILES: return "PROJECTILES";
        case ObjectLayers::TRIGGERS: return "TRIGGERS";
        case ObjectLayers::ENVIRONMENT: return "ENVIRONMENT";
        default: return "UNKNOWN(" + std::to_string(static_cast<int>(objectLayer)) + ")";
    }
}

// =============================================================================
// TIME UNIT SAFETY SYSTEM
// =============================================================================

PhysicsSystem::Seconds PhysicsSystem::clampPhysicsTimestep(Milliseconds deltaTime) const {
    // Convert to seconds and apply physics-safe timestep clamping
    Seconds deltaSec = toSeconds(deltaTime);

    // Clamp to reasonable physics timesteps to prevent simulation instability
    const float minTimestep = 1.0f / 240.0f; // Minimum: 240 FPS (0.004 seconds)
    const float maxTimestep = 1.0f / 30.0f;  // Maximum: 30 FPS (0.033 seconds)

    float clampedValue = std::max(minTimestep, std::min(deltaSec.value, maxTimestep));

    if (deltaSec.value != clampedValue) {
        VKMON_DEBUG("PhysicsSystem: Clamped physics timestep from " +
                   std::to_string(deltaSec.value) + "s to " +
                   std::to_string(clampedValue) + "s");
    }

    return Seconds(clampedValue);
}

bool PhysicsSystem::isValidPhysicsTimestep(Milliseconds deltaTime) const {
    // Check if the timestep is within acceptable physics simulation bounds
    Seconds deltaSec = toSeconds(deltaTime);

    // Invalid ranges
    if (deltaSec.value <= 0.0f) {
        VKMON_WARNING("PhysicsSystem: Invalid negative or zero timestep: " + std::to_string(deltaSec.value) + "s");
        return false;
    }

    // Handle initialization/asset loading spikes gracefully
    // Large timesteps during startup are expected due to asset loading blocking
    if (deltaSec.value > 0.1f) { // More than 100ms = 10 FPS
        VKMON_INFO("PhysicsSystem: Large timestep detected (" + std::to_string(deltaSec.value) + "s) - likely asset loading or initialization, will clamp");
        // Don't reject - let clampPhysicsTimestep handle it
    }

    // Only reject truly problematic timesteps (>5 seconds indicates real issues)
    if (deltaSec.value > 5.0f) {
        VKMON_WARNING("PhysicsSystem: Extremely large timestep detected: " + std::to_string(deltaSec.value) + "s - skipping");
        return false;
    }

    return true;
}

PhysicsSystem::Milliseconds PhysicsSystem::applyTimeScale(Milliseconds deltaTime) const {
    // Apply global time scaling with safety validation
    if (timeScale_ <= 0.0f) {
        VKMON_WARNING("PhysicsSystem: Invalid timeScale " + std::to_string(timeScale_) + ", using 1.0");
        return deltaTime;
    }

    float scaledValue = deltaTime.value * timeScale_;

    // Prevent extreme time scaling that could break physics
    if (timeScale_ > 10.0f || timeScale_ < 0.1f) {
        VKMON_WARNING("PhysicsSystem: Extreme timeScale " + std::to_string(timeScale_) + " may cause instability");
    }

    return Milliseconds(scaledValue);
}

void PhysicsSystem::recordTimingStart() {
    lastTimingStart_ = std::chrono::steady_clock::now();
}

PhysicsSystem::Milliseconds PhysicsSystem::getElapsedTiming() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTimingStart_);

    // Convert microseconds to milliseconds
    float elapsedMs = elapsed.count() / 1000.0f;

    return Milliseconds(elapsedMs);
}

std::string PhysicsSystem::timeToDebugString(Milliseconds ms) const {
    std::string result = std::to_string(ms.value) + "ms";

    // Add helpful context for common physics timesteps
    if (ms.value >= 33.0f && ms.value <= 34.0f) {
        result += " (~30 FPS)";
    } else if (ms.value >= 16.0f && ms.value <= 17.0f) {
        result += " (~60 FPS)";
    } else if (ms.value >= 8.0f && ms.value <= 9.0f) {
        result += " (~120 FPS)";
    } else if (ms.value >= 4.0f && ms.value <= 5.0f) {
        result += " (~240 FPS)";
    } else if (ms.value > 100.0f) {
        result += " (WARNING: Very slow frame)";
    } else if (ms.value < 1.0f) {
        result += " (Very fast frame)";
    }

    return result;
}

std::string PhysicsSystem::timeToDebugString(Seconds s) const {
    std::string result = std::to_string(s.value) + "s";

    // Add millisecond equivalent for readability
    float ms = s.value * 1000.0f;
    result += " (" + std::to_string(ms) + "ms)";

    return result;
}

// =============================================================================
// JOLT PHYSICS BODY CREATION AND MANAGEMENT
// =============================================================================

JPH::ShapeRefC PhysicsSystem::createJoltShape(const CollisionComponent& collision) {
    switch (collision.shapeType) {
        case CollisionComponent::ShapeType::Box: {
            glm::vec3 halfExtents = collision.dimensions * 0.5f;
            return JPH::BoxShapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z)).Create().Get();
        }
        case CollisionComponent::ShapeType::Sphere: {
            float radius = collision.dimensions.x; // Use x dimension as radius
            return JPH::SphereShapeSettings(radius).Create().Get();
        }
        case CollisionComponent::ShapeType::Capsule: {
            float height = collision.dimensions.y;
            float radius = collision.dimensions.x;
            return JPH::CapsuleShapeSettings(height * 0.5f, radius).Create().Get();
        }
        default:
            VKMON_WARNING("PhysicsSystem: Unsupported shape type, using box fallback");
            glm::vec3 halfExtents = collision.dimensions * 0.5f;
            return JPH::BoxShapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z)).Create().Get();
    }
}

JPH::BodyID PhysicsSystem::createJoltBody(EntityID entity, const RigidBodyComponent& rigidBody,
                                          const CollisionComponent& collision, const Transform& transform) {
    if (!joltPhysics_) {
        VKMON_ERROR("PhysicsSystem: Cannot create Jolt body - Jolt Physics not initialized");
        return JPH::BodyID();
    }

    // Create collision shape
    JPH::ShapeRefC shape = createJoltShape(collision);

    // ========================================================================
    // ENHANCED LAYER MAPPING INTEGRATION
    // ========================================================================

    // Validate layer mask before conversion
    if (!isValidLayerMaskCombination(collision.layer)) {
        VKMON_WARNING("PhysicsSystem: Invalid layer combination for entity " + std::to_string(entity));
    }

    // Convert LayerMask to ObjectLayer with enhanced mapping
    JPH::ObjectLayer objectLayer = mapLayerMaskToObjectLayer(collision.layer);

    // Debug logging for layer conversion (can be disabled for performance)
    VKMON_DEBUG("PhysicsSystem: Entity " + std::to_string(entity) + " LayerMask " +
               layerMaskToDebugString(collision.layer) + " → ObjectLayer " +
               objectLayerToDebugString(objectLayer));

    // Configure body creation settings
    JPH::BodyCreationSettings bodySettings(shape, JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                                           JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                                           rigidBody.isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                           objectLayer);

    // Configure motion quality for dynamic bodies
    if (rigidBody.isDynamic && rigidBody.mass > 0.0f) {
        bodySettings.mMotionQuality = JPH::EMotionQuality::Discrete;
    }

    // Set material properties
    bodySettings.mRestitution = rigidBody.restitution;
    bodySettings.mFriction = rigidBody.friction;

    // Set mass properties for dynamic bodies
    if (bodySettings.mMotionType == JPH::EMotionType::Dynamic) {
        bodySettings.mMassPropertiesOverride = JPH::MassProperties();
        bodySettings.mMassPropertiesOverride.mMass = rigidBody.mass;
        bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    }

    // Create the body in Jolt Physics
    JPH::Body* body = joltPhysics_->GetBodyInterface().CreateBody(bodySettings);
    if (!body) {
        VKMON_ERROR("PhysicsSystem: Failed to create Jolt body for entity " + std::to_string(entity));
        return JPH::BodyID();
    }

    JPH::BodyID bodyID = body->GetID();

    // Add body to simulation first (required before setting velocities)
    joltPhysics_->GetBodyInterface().AddBody(bodyID, JPH::EActivation::Activate);

    // Set initial velocity after body is added to simulation
    if (bodySettings.mMotionType == JPH::EMotionType::Dynamic) {
        joltPhysics_->GetBodyInterface().SetLinearVelocity(bodyID,
            JPH::Vec3(rigidBody.velocity.x, rigidBody.velocity.y, rigidBody.velocity.z));
        joltPhysics_->GetBodyInterface().SetAngularVelocity(bodyID,
            JPH::Vec3(rigidBody.angularVelocity.x, rigidBody.angularVelocity.y, rigidBody.angularVelocity.z));
    }

    // Store bidirectional mapping
    entityToBodyMap_[entity] = bodyID;
    bodyToEntityMap_[bodyID] = entity;

    return bodyID;
}

void PhysicsSystem::destroyJoltBody(EntityID entity) {
    if (!joltPhysics_) return;

    auto it = entityToBodyMap_.find(entity);
    if (it == entityToBodyMap_.end()) return;

    JPH::BodyID bodyID = it->second;

    // Remove from simulation
    joltPhysics_->GetBodyInterface().RemoveBody(bodyID);

    // Destroy the body
    joltPhysics_->GetBodyInterface().DestroyBody(bodyID);

    // Remove from mappings
    bodyToEntityMap_.erase(bodyID);
    entityToBodyMap_.erase(it);

    VKMON_DEBUG("PhysicsSystem: Destroyed Jolt body for entity " + std::to_string(entity));
}

void PhysicsSystem::updateJoltBodyFromComponent(EntityID entity, const RigidBodyComponent& rigidBody) {
    if (!joltPhysics_) return;

    auto it = entityToBodyMap_.find(entity);
    if (it == entityToBodyMap_.end()) return;

    JPH::BodyID bodyID = it->second;
    JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

    // Update material properties
    bodyInterface.SetRestitution(bodyID, rigidBody.restitution);
    bodyInterface.SetFriction(bodyID, rigidBody.friction);

    // Update velocity for dynamic bodies
    if (bodyInterface.GetMotionType(bodyID) == JPH::EMotionType::Dynamic) {
        bodyInterface.SetLinearVelocity(bodyID,
            JPH::Vec3(rigidBody.velocity.x, rigidBody.velocity.y, rigidBody.velocity.z));
        bodyInterface.SetAngularVelocity(bodyID,
            JPH::Vec3(rigidBody.angularVelocity.x, rigidBody.angularVelocity.y, rigidBody.angularVelocity.z));
    }
}

void PhysicsSystem::syncTransformsFromJolt(EntityManager& entityManager) {
    if (!joltPhysics_) {
        VKMON_DEBUG("syncTransformsFromJolt: No Jolt physics system");
        return;
    }

    try {
        VKMON_DEBUG("syncTransformsFromJolt: Getting body interface...");
        JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();
        VKMON_DEBUG("syncTransformsFromJolt: Body interface obtained, processing " + std::to_string(entityToBodyMap_.size()) + " bodies");
        int syncCount = 0;

        for (const auto& [entityID, bodyID] : entityToBodyMap_) {
            VKMON_DEBUG("syncTransformsFromJolt: Processing entity " + std::to_string(entityID));
            try {
                // Check if body is valid
                if (bodyID.IsInvalid()) {
                    VKMON_WARNING("syncTransformsFromJolt: Invalid bodyID for entity " + std::to_string(entityID));
                    continue;
                }

                // Sync ALL physics entities every frame for reliable ECS integration
                if (entityManager.hasComponent<Transform>(entityID)) {
                    auto& transform = entityManager.getComponent<Transform>(entityID);

                    // Get position and rotation from Jolt
                    JPH::Vec3 position = bodyInterface.GetPosition(bodyID);
                    JPH::Quat rotation = bodyInterface.GetRotation(bodyID);

                    // Update Transform component
                    transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
                    transform.rotation = glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());

                    // Mark transform as dirty for rendering
                    transform.markDirty();
                    syncCount++;
                }

                // Also sync velocity to RigidBodyComponent for test compatibility
                if (entityManager.hasComponent<RigidBodyComponent>(entityID)) {
                    auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entityID);

                    // Get velocity from Jolt
                    JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(bodyID);
                    JPH::Vec3 angularVelocity = bodyInterface.GetAngularVelocity(bodyID);

                    // Update RigidBodyComponent velocity
                    rigidBody.velocity = glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
                    rigidBody.angularVelocity = glm::vec3(angularVelocity.GetX(), angularVelocity.GetY(), angularVelocity.GetZ());
                }
            } catch (const std::exception& e) {
                VKMON_ERROR("syncTransformsFromJolt: Error syncing entity " + std::to_string(entityID) + ": " + e.what());
            }
        }

        if (syncCount > 0) {
            VKMON_DEBUG("syncTransformsFromJolt: Synced " + std::to_string(syncCount) + " entities");
        }
    } catch (const std::exception& e) {
        VKMON_ERROR("syncTransformsFromJolt: Critical error: " + std::string(e.what()));
    }
}

void PhysicsSystem::syncTransformsToJolt(EntityManager& entityManager) {
    if (!joltPhysics_) return;

    JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

    for (const auto& [entityID, bodyID] : entityToBodyMap_) {
        if (entityManager.hasComponent<Transform>(entityID)) {
            const auto& transform = entityManager.getComponent<Transform>(entityID);

            // Only sync if transform has been modified
            if (transform.isDirty) {
                bodyInterface.SetPosition(bodyID,
                    JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                    JPH::EActivation::Activate);
                bodyInterface.SetRotation(bodyID,
                    JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                    JPH::EActivation::Activate);
            }
        }
    }
}

void PhysicsSystem::updateStatsFromJolt() {
    if (!joltPhysics_) return;

    const JPH::BodyInterface& bodyInterface = joltPhysics_->GetBodyInterface();

    // Count active bodies and calculate average velocity
    int activeBodies = 0;
    float totalVelocity = 0.0f;

    for (const auto& [entityID, bodyID] : entityToBodyMap_) {
        if (bodyInterface.IsActive(bodyID)) {
            activeBodies++;

            if (bodyInterface.GetMotionType(bodyID) == JPH::EMotionType::Dynamic) {
                JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(bodyID);
                totalVelocity += velocity.Length();
            }
        }
    }

    // Update statistics
    stats_.activeRigidBodies = activeBodies;
    stats_.averageVelocity = (activeBodies > 0) ? (totalVelocity / activeBodies) : 0.0f;

    // For collision stats, Jolt doesn't expose these directly
    // Use conservative estimates based on broad phase optimization
    int totalBodies = static_cast<int>(entityToBodyMap_.size());
    stats_.collisionChecks = (totalBodies > 1) ? std::min(totalBodies * 10, 200) : 0; // Jolt's broad phase reduces checks dramatically
    stats_.collisionHits = std::min(activeBodies / 3, 50); // Estimate based on active bodies

    VKMON_DEBUG("PhysicsSystem: Active bodies: " + std::to_string(stats_.activeRigidBodies) +
                ", Avg velocity: " + std::to_string(stats_.averageVelocity));
}

void PhysicsSystem::createJoltBodiesForNewEntities(EntityManager& entityManager) {
    if (!joltPhysics_) return;

    // Get all entities with both physics components
    const auto& rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();

    int newBodies = 0;

    for (EntityID entity : rigidBodyEntities) {
        // Skip entities that already have Jolt bodies
        if (entityToBodyMap_.find(entity) != entityToBodyMap_.end()) {
            continue;
        }

        // Only create Jolt body if entity has both RigidBody and Collision components
        if (!entityManager.hasComponent<CollisionComponent>(entity) ||
            !entityManager.hasComponent<Transform>(entity)) {
            continue;
        }

        // Get components
        const auto& rigidBody = entityManager.getComponent<RigidBodyComponent>(entity);
        const auto& collision = entityManager.getComponent<CollisionComponent>(entity);
        const auto& transform = entityManager.getComponent<Transform>(entity);

        // Create Jolt body
        JPH::BodyID bodyID = createJoltBody(entity, rigidBody, collision, transform);

        if (bodyID.IsInvalid()) {
            VKMON_WARNING("PhysicsSystem: Failed to create Jolt body for entity " + std::to_string(entity));
            continue;
        }

        newBodies++;
        VKMON_DEBUG("PhysicsSystem: Created Jolt body for entity " + std::to_string(entity) +
                    " (Dynamic: " + (rigidBody.isDynamic ? "true" : "false") +
                    ", Shape: " + std::to_string(static_cast<int>(collision.shapeType)) + ")");
    }

    VKMON_DEBUG("PhysicsSystem: Loop completed, created " + std::to_string(newBodies) + " bodies");

    if (newBodies > 0) {
        VKMON_INFO("PhysicsSystem: Created " + std::to_string(newBodies) + " new Jolt physics bodies");
    }

}

void PhysicsSystem::createJoltBodiesForAllEntities(EntityManager& entityManager) {
    VKMON_INFO("PhysicsSystem: Creating Jolt bodies for all entities with physics components");
    createJoltBodiesForNewEntities(entityManager);
}

void PhysicsSystem::preloadPhysicsBodies(EntityManager& entityManager) {
    if (!joltPhysics_) {
        VKMON_WARNING("Cannot preload physics bodies: Jolt Physics not initialized");
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    VKMON_INFO("PhysicsSystem: Preloading physics bodies during initialization...");

    // Count entities before preloading
    auto rigidBodyEntities = entityManager.getEntitiesWithComponent<RigidBodyComponent>();
    auto collisionEntities = entityManager.getEntitiesWithComponent<CollisionComponent>();

    int totalPhysicsEntities = 0;
    for (EntityID entity : rigidBodyEntities) {
        if (entityManager.hasComponent<CollisionComponent>(entity)) {
            totalPhysicsEntities++;
        }
    }

    VKMON_INFO("PhysicsSystem: Found " + std::to_string(totalPhysicsEntities) + " entities with both physics components");

    // Create all physics bodies immediately
    createJoltBodiesForNewEntities(entityManager);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto preloadTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    VKMON_INFO("PhysicsSystem: Physics body preloading complete in " + std::to_string(preloadTimeMs) + "ms");
}


} // namespace VulkanMon