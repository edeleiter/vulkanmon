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
            // Create Jolt bodies for any new entities with physics components
            createJoltBodiesForNewEntities(entityManager);

            // Update Jolt physics simulation with safe time step
            // CRITICAL: Jolt Physics expects deltaTime in SECONDS
            // Our deltaTime parameter comes from Application::frameTime_ which is in MILLISECONDS
            const float deltaTimeSeconds = scaledDeltaTime / 1000.0f; // Convert milliseconds to seconds
            const float maxTimeStep = 1.0f / 60.0f; // Cap at 60 FPS timestep (seconds)
            const float clampedDeltaTime = std::min(deltaTimeSeconds, maxTimeStep);
            const int collisionSteps = 1;

            joltPhysics_->Update(clampedDeltaTime, collisionSteps, tempAllocator_.get(), jobSystem_.get());

            // Sync transforms from Jolt back to ECS components
            syncTransformsFromJolt(entityManager);

            // Update statistics from Jolt
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

    // Use narrow phase query for precise raycast
    const JPH::NarrowPhaseQuery& narrowPhase = joltPhysics_->GetNarrowPhaseQuery();

    // Simple raycast using Jolt's built-in result structure
    JPH::RayCastResult rayResult;

    // Use simplified raycast approach - cast ray and get single closest result
    JPH::RRayCast joltRay{ray.mOrigin, ray.mDirection};

    // Perform the raycast using all default filters for now (can be enhanced later)
    bool hasHit = narrowPhase.CastRay(joltRay, rayResult, {}, {}, {});

    // Process results
    if (hasHit) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(rayResult.mBodyID);
        if (it != bodyToEntityMap_.end()) {
            hit.hit = true;
            hit.distance = rayResult.mFraction * maxDistance;
            hit.entity = it->second;

            // Calculate hit point
            glm::vec3 hitPoint = origin + normalizedDir * hit.distance;
            hit.point = hitPoint;

            // Get surface normal from Jolt (simplified for now)
            hit.normal = glm::vec3(0, 1, 0); // Default upward normal - can be improved with proper Jolt normal extraction

            VKMON_DEBUG("PhysicsSystem: Raycast hit entity " + std::to_string(hit.entity) +
                       " at distance " + std::to_string(hit.distance));
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

    // Storage for collision results
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

    // Perform the overlap query using CollideShape with correct API
    JPH::CollideShapeSettings settings;
    narrowPhase.CollideShape(sphereShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3(joltCenter), collector, {}, {}, {}, {});

    // Process results and convert body IDs back to entity IDs
    for (const auto& hit : collector.mHits) {
        // Find the entity ID from the body ID
        auto it = bodyToEntityMap_.find(hit.mBodyID2);
        if (it != bodyToEntityMap_.end()) {
            EntityID entityId = it->second;

            // TODO: Add layer mask filtering here for more precise control
            // For now, include all hits
            results.push_back(entityId);
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
    // Simple mapping for now - can be enhanced later
    if (layerMask & LayerMask::Creatures) return ObjectLayers::CREATURES;
    if (layerMask & LayerMask::Environment) return ObjectLayers::ENVIRONMENT;
    if (layerMask & LayerMask::CaptureDevices) return ObjectLayers::PROJECTILES;
    if (layerMask & LayerMask::Player) return ObjectLayers::PLAYER;
    return ObjectLayers::ENVIRONMENT; // Default fallback
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

    // Configure body creation settings
    JPH::BodyCreationSettings bodySettings(shape, JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                                           JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                                           rigidBody.isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                           mapLayerMaskToObjectLayer(collision.layer));

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
        VKMON_INFO("PhysicsSystem: Created Jolt body for entity " + std::to_string(entity) +
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


} // namespace VulkanMon