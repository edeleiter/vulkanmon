# Physics Integration with Bullet Physics
## Week 3: Building a Game-Ready Physics System

## Overview
Bullet Physics integration is crucial for Pokémon Legends: Arceus-style gameplay. We need accurate Pokéball throwing, creature movement, terrain collision, and environmental interactions. This guide covers a production-ready integration that's performant and maintainable.

## Architecture Design

### Core Physics Components

```cpp
// include/physics/PhysicsSystem.h
#pragma once
#include "ecs/System.h"
#include <btBulletDynamicsCommon.h>
#include <memory>
#include <unordered_map>

namespace VulkanMon {

// Physics body types
enum class BodyType {
    Static,    // Terrain, buildings
    Dynamic,   // Pokeballs, items
    Kinematic, // Player, creatures (code-controlled movement)
    Ghost      // Triggers, detection zones
};

// Collision groups for filtering
namespace CollisionGroup {
    constexpr uint16_t None = 0;
    constexpr uint16_t Terrain = 1 << 0;
    constexpr uint16_t Player = 1 << 1;
    constexpr uint16_t Creature = 1 << 2;
    constexpr uint16_t Pokeball = 1 << 3;
    constexpr uint16_t Grass = 1 << 4;
    constexpr uint16_t Water = 1 << 5;
    constexpr uint16_t Item = 1 << 6;
    constexpr uint16_t Trigger = 1 << 7;
    constexpr uint16_t All = 0xFFFF;
}

// Physics component for ECS
class PhysicsComponent : public Component {
public:
    BodyType bodyType = BodyType::Static;
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;  // Bounciness
    float linearDamping = 0.1f;
    float angularDamping = 0.1f;
    
    uint16_t collisionGroup = CollisionGroup::None;
    uint16_t collisionMask = CollisionGroup::All;
    
    // Shape parameters
    enum ShapeType {
        Box, Sphere, Capsule, Mesh, Terrain
    } shapeType = ShapeType::Box;
    
    glm::vec3 shapeSize{1.0f};  // Box: half-extents, Sphere: x=radius, Capsule: x=radius, y=height
    
    // Runtime data (managed by PhysicsSystem)
    btRigidBody* rigidBody = nullptr;
    btCollisionShape* collisionShape = nullptr;
    
    // Callbacks
    std::function<void(EntityId other)> onCollisionEnter;
    std::function<void(EntityId other)> onCollisionExit;
    std::function<void(EntityId other)> onTriggerEnter;
    std::function<void(EntityId other)> onTriggerExit;
};

// Character controller for player/creatures
class CharacterController : public Component {
public:
    float moveSpeed = 5.0f;
    float jumpHeight = 2.0f;
    float stepHeight = 0.5f;
    float capsuleRadius = 0.5f;
    float capsuleHeight = 1.8f;
    
    // Movement input
    glm::vec3 moveDirection{0.0f};
    bool jumpRequested = false;
    
    // State
    bool isGrounded = false;
    glm::vec3 groundNormal{0, 1, 0};
    float groundDistance = 0.0f;
    
    // Internal
    btKinematicCharacterController* controller = nullptr;
};

// Projectile component for Pokeballs
class ProjectileComponent : public Component {
public:
    float launchSpeed = 20.0f;
    float gravityScale = 1.0f;
    float airDrag = 0.1f;
    float spinRate = 5.0f;  // Visual spin
    
    // Trajectory prediction
    std::vector<glm::vec3> predictedPath;
    float trajectoryTime = 3.0f;
    
    // State
    bool hasHit = false;
    EntityId hitEntity = InvalidEntity;
    glm::vec3 hitPoint;
    glm::vec3 hitNormal;
};

class PhysicsSystem : public System {
public:
    PhysicsSystem();
    ~PhysicsSystem();
    
    void initialize() override;
    void update(float deltaTime) override;
    void fixedUpdate(float fixedDeltaTime) override;
    
    // Component management
    void onComponentAdded(EntityId entity, ComponentId component) override;
    void onComponentRemoved(EntityId entity, ComponentId component) override;
    
    // Terrain
    void setTerrain(const std::vector<float>& heightmap, 
                     int width, int height, 
                     float scale, float maxHeight);
    
    // Queries
    struct RaycastHit {
        bool hit;
        EntityId entity;
        glm::vec3 point;
        glm::vec3 normal;
        float distance;
    };
    
    RaycastHit raycast(const glm::vec3& from, const glm::vec3& to,
                       uint16_t collisionMask = CollisionGroup::All);
    
    std::vector<EntityId> overlapSphere(const glm::vec3& center, float radius,
                                        uint16_t collisionMask = CollisionGroup::All);
    
    // Pokeball throwing
    void launchProjectile(EntityId projectile, 
                          const glm::vec3& origin,
                          const glm::vec3& direction,
                          float speed);
    
    std::vector<glm::vec3> predictTrajectory(const glm::vec3& origin,
                                             const glm::vec3& velocity,
                                             float timeStep = 0.1f,
                                             float maxTime = 3.0f);
    
    // Debug
    void setDebugDrawEnabled(bool enabled) { m_debugDrawEnabled = enabled; }
    void debugDraw(class DebugRenderer* renderer);
    
private:
    // Bullet world
    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfig;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btDbvtBroadphase> m_broadphase;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_world;
    std::unique_ptr<btIDebugDraw> m_debugDrawer;
    
    // Entity mapping
    std::unordered_map<EntityId, btRigidBody*> m_entityBodies;
    std::unordered_map<const btRigidBody*, EntityId> m_bodyEntities;
    std::unordered_map<EntityId, btKinematicCharacterController*> m_characterControllers;
    
    // Shape cache
    std::unordered_map<size_t, std::shared_ptr<btCollisionShape>> m_shapeCache;
    
    // Collision tracking
    struct CollisionPair {
        EntityId a, b;
        bool operator<(const CollisionPair& other) const {
            return std::tie(a, b) < std::tie(other.a, other.b);
        }
    };
    std::set<CollisionPair> m_currentCollisions;
    std::set<CollisionPair> m_previousCollisions;
    
    // Settings
    float m_fixedTimeStep = 1.0f / 60.0f;
    int m_maxSubSteps = 3;
    float m_accumulator = 0.0f;
    bool m_debugDrawEnabled = false;
    
    // Helper methods
    btCollisionShape* createCollisionShape(const PhysicsComponent& comp);
    void updateTransformFromPhysics(EntityId entity, btRigidBody* body);
    void updatePhysicsFromTransform(EntityId entity, btRigidBody* body);
    void processCollisions();
    void updateCharacterControllers(float deltaTime);
};

} // namespace VulkanMon
```

### Implementation

```cpp
// src/physics/PhysicsSystem.cpp
#include "physics/PhysicsSystem.h"
#include "ecs/Transform.h"
#include "spatial/SpatialSystem.h"
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

namespace VulkanMon {

// Custom motion state to sync with ECS
class ECSMotionState : public btMotionState {
public:
    ECSMotionState(EntityId entity, Transform* transform)
        : m_entity(entity), m_transform(transform) {}
    
    void getWorldTransform(btTransform& worldTrans) const override {
        if (m_transform) {
            worldTrans.setOrigin(btVector3(
                m_transform->position.x,
                m_transform->position.y,
                m_transform->position.z
            ));
            worldTrans.setRotation(btQuaternion(
                m_transform->rotation.x,
                m_transform->rotation.y,
                m_transform->rotation.z,
                m_transform->rotation.w
            ));
        }
    }
    
    void setWorldTransform(const btTransform& worldTrans) override {
        if (m_transform) {
            const btVector3& pos = worldTrans.getOrigin();
            const btQuaternion& rot = worldTrans.getRotation();
            
            m_transform->position = glm::vec3(pos.x(), pos.y(), pos.z());
            m_transform->rotation = glm::quat(rot.w(), rot.x(), rot.y(), rot.z());
        }
    }
    
private:
    EntityId m_entity;
    Transform* m_transform;
};

PhysicsSystem::PhysicsSystem() {
    initialize();
}

void PhysicsSystem::initialize() {
    // Setup Bullet world
    m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());
    m_broadphase = std::make_unique<btDbvtBroadphase>();
    m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    
    m_world = std::make_unique<btDiscreteDynamicsWorld>(
        m_dispatcher.get(), m_broadphase.get(), 
        m_solver.get(), m_collisionConfig.get()
    );
    
    // Set gravity (Pokemon world gravity might be different!)
    m_world->setGravity(btVector3(0, -9.81f, 0));
    
    // Enable continuous collision detection for fast objects (Pokeballs)
    m_world->getSolverInfo().m_solverMode |= 
        SOLVER_USE_2_FRICTION_DIRECTIONS |
        SOLVER_ENABLE_FRICTION_DIRECTION_CACHING;
}

void PhysicsSystem::onComponentAdded(EntityId entity, ComponentId component) {
    if (component == ComponentId::Physics) {
        auto* physics = ECS::getInstance().getComponent<PhysicsComponent>(entity);
        auto* transform = ECS::getInstance().getComponent<Transform>(entity);
        
        if (!physics || !transform) return;
        
        // Create collision shape
        physics->collisionShape = createCollisionShape(*physics);
        
        // Calculate inertia
        btVector3 localInertia(0, 0, 0);
        if (physics->bodyType == BodyType::Dynamic && physics->mass > 0) {
            physics->collisionShape->calculateLocalInertia(physics->mass, localInertia);
        }
        
        // Create motion state
        auto* motionState = new ECSMotionState(entity, transform);
        
        // Create rigid body
        btRigidBody::btRigidBodyConstructionInfo rbInfo(
            physics->bodyType == BodyType::Dynamic ? physics->mass : 0,
            motionState,
            physics->collisionShape,
            localInertia
        );
        
        rbInfo.m_friction = physics->friction;
        rbInfo.m_restitution = physics->restitution;
        rbInfo.m_linearDamping = physics->linearDamping;
        rbInfo.m_angularDamping = physics->angularDamping;
        
        physics->rigidBody = new btRigidBody(rbInfo);
        physics->rigidBody->setUserPointer(reinterpret_cast<void*>(entity));
        
        // Set collision flags
        if (physics->bodyType == BodyType::Kinematic) {
            physics->rigidBody->setCollisionFlags(
                physics->rigidBody->getCollisionFlags() | 
                btCollisionObject::CF_KINEMATIC_OBJECT
            );
            physics->rigidBody->setActivationState(DISABLE_DEACTIVATION);
        } else if (physics->bodyType == BodyType::Ghost) {
            physics->rigidBody->setCollisionFlags(
                physics->rigidBody->getCollisionFlags() | 
                btCollisionObject::CF_NO_CONTACT_RESPONSE
            );
        }
        
        // Add to world with collision filtering
        m_world->addRigidBody(
            physics->rigidBody,
            physics->collisionGroup,
            physics->collisionMask
        );
        
        // Track mapping
        m_entityBodies[entity] = physics->rigidBody;
        m_bodyEntities[physics->rigidBody] = entity;
    }
    else if (component == ComponentId::CharacterController) {
        auto* character = ECS::getInstance().getComponent<CharacterController>(entity);
        auto* transform = ECS::getInstance().getComponent<Transform>(entity);
        
        if (!character || !transform) return;
        
        // Create capsule shape for character
        auto* capsule = new btCapsuleShape(
            character->capsuleRadius, 
            character->capsuleHeight
        );
        
        // Create ghost object for character
        auto* ghostObject = new btPairCachingGhostObject();
        ghostObject->setWorldTransform(btTransform(
            btQuaternion::getIdentity(),
            btVector3(transform->position.x, transform->position.y, transform->position.z)
        ));
        ghostObject->setCollisionShape(capsule);
        ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
        
        // Create character controller
        character->controller = new btKinematicCharacterController(
            ghostObject, capsule, character->stepHeight
        );
        
        character->controller->setJumpSpeed(
            sqrtf(2.0f * 9.81f * character->jumpHeight)
        );
        character->controller->setMaxSlope(btRadians(45.0f));
        character->controller->setGravity(btVector3(0, -9.81f * 2, 0));
        
        // Add to world
        m_world->addCollisionObject(
            ghostObject,
            CollisionGroup::Player,
            CollisionGroup::All
        );
        m_world->addAction(character->controller);
        
        m_characterControllers[entity] = character->controller;
    }
}

void PhysicsSystem::fixedUpdate(float fixedDeltaTime) {
    // Update character controllers first
    updateCharacterControllers(fixedDeltaTime);
    
    // Step physics simulation
    m_world->stepSimulation(fixedDeltaTime, 0);
    
    // Update transforms from physics
    for (const auto& [entity, body] : m_entityBodies) {
        if (body->isActive() && !body->isStaticOrKinematicObject()) {
            updateTransformFromPhysics(entity, body);
        }
    }
    
    // Process collisions
    processCollisions();
}

void PhysicsSystem::updateCharacterControllers(float deltaTime) {
    for (const auto& [entity, controller] : m_characterControllers) {
        auto* character = ECS::getInstance().getComponent<CharacterController>(entity);
        if (!character) continue;
        
        // Calculate walk direction
        btVector3 walkDir(
            character->moveDirection.x,
            0,
            character->moveDirection.z
        );
        walkDir.normalize();
        walkDir *= character->moveSpeed * deltaTime;
        
        // Apply movement
        controller->setWalkDirection(walkDir);
        
        // Handle jumping
        if (character->jumpRequested && controller->canJump()) {
            controller->jump();
            character->jumpRequested = false;
        }
        
        // Update grounded state
        character->isGrounded = controller->onGround();
        
        // Sync transform
        auto* transform = ECS::getInstance().getComponent<Transform>(entity);
        if (transform) {
            const btTransform& worldTrans = 
                controller->getGhostObject()->getWorldTransform();
            const btVector3& pos = worldTrans.getOrigin();
            transform->position = glm::vec3(pos.x(), pos.y(), pos.z());
        }
    }
}

void PhysicsSystem::launchProjectile(EntityId projectile,
                                     const glm::vec3& origin,
                                     const glm::vec3& direction,
                                     float speed) {
    auto* physics = ECS::getInstance().getComponent<PhysicsComponent>(projectile);
    auto* proj = ECS::getInstance().getComponent<ProjectileComponent>(projectile);
    
    if (!physics || !physics->rigidBody || !proj) return;
    
    // Set initial position
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(origin.x, origin.y, origin.z));
    physics->rigidBody->setWorldTransform(transform);
    
    // Apply launch velocity
    glm::vec3 velocity = glm::normalize(direction) * speed;
    physics->rigidBody->setLinearVelocity(
        btVector3(velocity.x, velocity.y, velocity.z)
    );
    
    // Add spin for realism
    glm::vec3 right = glm::cross(direction, glm::vec3(0, 1, 0));
    physics->rigidBody->setAngularVelocity(
        btVector3(right.x * proj->spinRate, 
                  right.y * proj->spinRate, 
                  right.z * proj->spinRate)
    );
    
    // Enable CCD for fast projectile
    physics->rigidBody->setCcdMotionThreshold(0.1f);
    physics->rigidBody->setCcdSweptSphereRadius(0.05f);
    
    // Predict trajectory
    proj->predictedPath = predictTrajectory(origin, velocity);
}

std::vector<glm::vec3> PhysicsSystem::predictTrajectory(
    const glm::vec3& origin,
    const glm::vec3& velocity,
    float timeStep, float maxTime) {
    
    std::vector<glm::vec3> path;
    
    glm::vec3 pos = origin;
    glm::vec3 vel = velocity;
    glm::vec3 gravity(0, -9.81f, 0);
    
    for (float t = 0; t < maxTime; t += timeStep) {
        path.push_back(pos);
        
        // Simple ballistic prediction
        vel += gravity * timeStep;
        pos += vel * timeStep;
        
        // Check for ground collision
        RaycastHit hit = raycast(
            path.back(), pos,
            CollisionGroup::Terrain | CollisionGroup::Creature
        );
        
        if (hit.hit) {
            path.push_back(hit.point);
            break;
        }
    }
    
    return path;
}

void PhysicsSystem::processCollisions() {
    // Get all collision pairs from Bullet
    std::set<CollisionPair> newCollisions;
    
    int numManifolds = m_dispatcher->getNumManifolds();
    for (int i = 0; i < numManifolds; ++i) {
        btPersistentManifold* manifold = m_dispatcher->getManifoldByIndexInternal(i);
        
        if (manifold->getNumContacts() > 0) {
            const btRigidBody* body0 = static_cast<const btRigidBody*>(manifold->getBody0());
            const btRigidBody* body1 = static_cast<const btRigidBody*>(manifold->getBody1());
            
            auto it0 = m_bodyEntities.find(body0);
            auto it1 = m_bodyEntities.find(body1);
            
            if (it0 != m_bodyEntities.end() && it1 != m_bodyEntities.end()) {
                CollisionPair pair{it0->second, it1->second};
                if (pair.b < pair.a) std::swap(pair.a, pair.b);
                newCollisions.insert(pair);
            }
        }
    }
    
    // Find new collisions (enter)
    for (const auto& pair : newCollisions) {
        if (m_previousCollisions.find(pair) == m_previousCollisions.end()) {
            // Collision enter
            auto* physicsA = ECS::getInstance().getComponent<PhysicsComponent>(pair.a);
            auto* physicsB = ECS::getInstance().getComponent<PhysicsComponent>(pair.b);
            
            if (physicsA && physicsA->onCollisionEnter) {
                physicsA->onCollisionEnter(pair.b);
            }
            if (physicsB && physicsB->onCollisionEnter) {
                physicsB->onCollisionEnter(pair.a);
            }
            
            // Special handling for pokeball hits
            auto* projA = ECS::getInstance().getComponent<ProjectileComponent>(pair.a);
            if (projA && !projA->hasHit) {
                projA->hasHit = true;
                projA->hitEntity = pair.b;
                // Get hit point from manifold
                handlePokeballHit(pair.a, pair.b);
            }
        }
    }
    
    // Find ended collisions (exit)
    for (const auto& pair : m_previousCollisions) {
        if (newCollisions.find(pair) == newCollisions.end()) {
            // Collision exit
            auto* physicsA = ECS::getInstance().getComponent<PhysicsComponent>(pair.a);
            auto* physicsB = ECS::getInstance().getComponent<PhysicsComponent>(pair.b);
            
            if (physicsA && physicsA->onCollisionExit) {
                physicsA->onCollisionExit(pair.b);
            }
            if (physicsB && physicsB->onCollisionExit) {
                physicsB->onCollisionExit(pair.a);
            }
        }
    }
    
    m_previousCollisions = std::move(newCollisions);
}

PhysicsSystem::RaycastHit PhysicsSystem::raycast(
    const glm::vec3& from, const glm::vec3& to, uint16_t collisionMask) {
    
    btVector3 btFrom(from.x, from.y, from.z);
    btVector3 btTo(to.x, to.y, to.z);
    
    btCollisionWorld::ClosestRayResultCallback callback(btFrom, btTo);
    callback.m_collisionFilterMask = collisionMask;
    
    m_world->rayTest(btFrom, btTo, callback);
    
    RaycastHit hit;
    hit.hit = callback.hasHit();
    
    if (hit.hit) {
        const btVector3& point = callback.m_hitPointWorld;
        const btVector3& normal = callback.m_hitNormalWorld;
        
        hit.point = glm::vec3(point.x(), point.y(), point.z());
        hit.normal = glm::vec3(normal.x(), normal.y(), normal.z());
        hit.distance = glm::distance(from, hit.point);
        
        // Get entity from rigid body
        const btRigidBody* body = btRigidBody::upcast(callback.m_collisionObject);
        if (body) {
            auto it = m_bodyEntities.find(body);
            if (it != m_bodyEntities.end()) {
                hit.entity = it->second;
            }
        }
    }
    
    return hit;
}

} // namespace VulkanMon
```

## Pokeball Throwing System

```cpp
// src/game/PokeballSystem.cpp
class PokeballSystem : public System {
public:
    struct ThrowSettings {
        float minSpeed = 10.0f;
        float maxSpeed = 30.0f;
        float aimAssistRadius = 2.0f;
        float captureRadius = 1.5f;
        bool showTrajectory = true;
    } settings;
    
    void throwPokeball(const glm::vec3& origin, 
                       const glm::vec3& direction,
                       float chargeAmount) {
        // Create pokeball entity
        auto pokeball = ECS::getInstance().createEntity();
        
        // Add components
        auto* transform = addComponent<Transform>(pokeball);
        transform->position = origin;
        transform->scale = glm::vec3(0.1f);  // Pokeball size
        
        auto* physics = addComponent<PhysicsComponent>(pokeball);
        physics->bodyType = BodyType::Dynamic;
        physics->shapeType = PhysicsComponent::Sphere;
        physics->shapeSize = glm::vec3(0.1f);
        physics->mass = 0.15f;  // 150g pokeball
        physics->restitution = 0.4f;  // Some bounce
        physics->friction = 0.8f;
        physics->collisionGroup = CollisionGroup::Pokeball;
        physics->collisionMask = CollisionGroup::All;
        
        auto* projectile = addComponent<ProjectileComponent>(pokeball);
        projectile->launchSpeed = glm::mix(
            settings.minSpeed, 
            settings.maxSpeed, 
            chargeAmount
        );
        
        // Apply aim assist
        glm::vec3 adjustedDirection = applyAimAssist(origin, direction);
        
        // Launch
        m_physicsSystem->launchProjectile(
            pokeball, origin, adjustedDirection, 
            projectile->launchSpeed
        );
        
        // Set up collision callback
        physics->onCollisionEnter = [this, pokeball](EntityId other) {
            handlePokeballCollision(pokeball, other);
        };
    }
    
private:
    glm::vec3 applyAimAssist(const glm::vec3& origin, 
                             const glm::vec3& direction) {
        // Find nearby creatures
        auto targets = m_spatialSystem->queryRadius(
            origin + direction * 20.0f,  // Look ahead
            settings.aimAssistRadius,
            LayerMask::Creatures
        );
        
        if (targets.empty()) return direction;
        
        // Find best target
        float bestScore = -1.0f;
        glm::vec3 bestDirection = direction;
        
        for (const auto& target : targets) {
            glm::vec3 toTarget = target.position - origin;
            float distance = glm::length(toTarget);
            glm::vec3 targetDir = toTarget / distance;
            
            float alignment = glm::dot(direction, targetDir);
            if (alignment > 0.7f) {  // Within 45 degrees
                float score = alignment / (1.0f + distance * 0.1f);
                if (score > bestScore) {
                    bestScore = score;
                    // Blend towards target
                    bestDirection = glm::normalize(
                        direction * 0.7f + targetDir * 0.3f
                    );
                }
            }
        }
        
        return bestDirection;
    }
    
    void handlePokeballCollision(EntityId pokeball, EntityId other) {
        auto* creature = getComponent<CreatureComponent>(other);
        if (!creature) {
            // Hit terrain or non-creature
            startPokeballReturn(pokeball);
            return;
        }
        
        // Check if creature can be caught
        if (creature->isCatchable && !creature->isOwned) {
            startCaptureSequence(pokeball, other);
        } else {
            // Bounce off
            startPokeballReturn(pokeball);
        }
    }
};
```

## Testing Framework

```cpp
// tests_cpp/physics/test_PhysicsSystem.cpp
TEST_CASE("Physics System Integration", "[physics]") {
    VulkanMon::PhysicsSystem physics;
    
    SECTION("Gravity and collision") {
        // Create ground
        auto ground = createEntity();
        auto* groundPhys = addComponent<PhysicsComponent>(ground);
        groundPhys->bodyType = BodyType::Static;
        groundPhys->shapeType = PhysicsComponent::Box;
        groundPhys->shapeSize = glm::vec3(50, 1, 50);
        
        // Drop a sphere
        auto sphere = createEntity();
        auto* sphereTrans = addComponent<Transform>(sphere);
        sphereTrans->position = glm::vec3(0, 10, 0);
        
        auto* spherePhys = addComponent<PhysicsComponent>(sphere);
        spherePhys->bodyType = BodyType::Dynamic;
        spherePhys->shapeType = PhysicsComponent::Sphere;
        spherePhys->shapeSize = glm::vec3(0.5f);
        spherePhys->mass = 1.0f;
        
        // Simulate for 2 seconds
        for (int i = 0; i < 120; ++i) {
            physics.fixedUpdate(1.0f / 60.0f);
        }
        
        // Should have fallen and stopped on ground
        CHECK(sphereTrans->position.y == Approx(1.5f).margin(0.1f));
    }
    
    SECTION("Projectile trajectory") {
        auto projectile = createEntity();
        
        glm::vec3 origin(0, 1, 0);
        glm::vec3 direction(1, 0.5f, 0);
        float speed = 20.0f;
        
        auto trajectory = physics.predictTrajectory(
            origin, glm::normalize(direction) * speed
        );
        
        REQUIRE(trajectory.size() > 10);
        
        // Should form a parabolic arc
        float maxHeight = 0;
        for (const auto& point : trajectory) {
            maxHeight = glm::max(maxHeight, point.y);
        }
        CHECK(maxHeight > origin.y);
        CHECK(trajectory.back().y <= origin.y);
    }
    
    SECTION("Character controller movement") {
        auto player = createEntity();
        auto* character = addComponent<CharacterController>(player);
        auto* transform = addComponent<Transform>(player);
        transform->position = glm::vec3(0, 2, 0);
        
        // Move forward
        character->moveDirection = glm::vec3(1, 0, 0);
        
        for (int i = 0; i < 60; ++i) {
            physics.fixedUpdate(1.0f / 60.0f);
        }
        
        // Should have moved
        CHECK(transform->position.x > 4.0f);
        CHECK(character->isGrounded == true);
    }
}
```

## Performance Benchmarks

```cpp
// tests_cpp/physics/bench_PhysicsSystem.cpp
static void BM_Physics_1000_Dynamic_Bodies(benchmark::State& state) {
    VulkanMon::PhysicsSystem physics;
    
    // Create ground
    auto ground = createGroundPlane();
    
    // Create 1000 falling spheres
    std::vector<EntityId> spheres;
    for (int i = 0; i < 1000; ++i) {
        auto sphere = createDynamicSphere(
            glm::vec3(i % 10, 10 + i / 10, i % 10)
        );
        spheres.push_back(sphere);
    }
    
    for (auto _ : state) {
        physics.fixedUpdate(1.0f / 60.0f);
    }
    
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_Physics_1000_Dynamic_Bodies);
```

## Integration Example: Grass Physics

```cpp
// src/game/GrassPhysics.cpp
class GrassPhysicsSystem : public System {
public:
    void createGrassPatch(const glm::vec3& position, float radius) {
        auto grass = createEntity();
        
        // Visual representation
        auto* transform = addComponent<Transform>(grass);
        transform->position = position;
        
        // Physics trigger zone
        auto* physics = addComponent<PhysicsComponent>(grass);
        physics->bodyType = BodyType::Ghost;  // Trigger
        physics->shapeType = PhysicsComponent::Sphere;
        physics->shapeSize = glm::vec3(radius);
        physics->collisionGroup = CollisionGroup::Grass;
        
        // Grass behavior
        auto* grassComp = addComponent<GrassComponent>(grass);
        grassComp->rustleDecay = 2.0f;
        
        // Setup callbacks
        physics->onTriggerEnter = [grassComp](EntityId other) {
            // Start rustling
            grassComp->rustleIntensity = 1.0f;
            grassComp->lastDisturbance = getCurrentTime();
            
            // Chance to spawn Pokemon
            if (random() < 0.1f) {
                spawnWildPokemon(grassComp->position);
            }
        };
        
        physics->onTriggerExit = [grassComp](EntityId other) {
            // Gradually stop rustling
            grassComp->rustleIntensity *= 0.5f;
        };
    }
};
```

## Debug Visualization

```cpp
void PhysicsSystem::debugDraw(DebugRenderer* renderer) {
    if (!m_debugDrawEnabled) return;
    
    // Draw all collision shapes
    for (const auto& [entity, body] : m_entityBodies) {
        btTransform transform;
        body->getMotionState()->getWorldTransform(transform);
        
        const btVector3& pos = transform.getOrigin();
        const btQuaternion& rot = transform.getRotation();
        
        glm::vec3 position(pos.x(), pos.y(), pos.z());
        glm::quat rotation(rot.w(), rot.x(), rot.y(), rot.z());
        
        // Color based on body type
        glm::vec4 color;
        if (body->isStaticObject()) {
            color = glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);  // Gray
        } else if (body->isKinematicObject()) {
            color = glm::vec4(0, 0, 1, 0.5f);  // Blue
        } else {
            color = glm::vec4(0, 1, 0, 0.5f);  // Green
        }
        
        // Draw shape
        btCollisionShape* shape = body->getCollisionShape();
        if (shape->getShapeType() == BOX_SHAPE_PROXYTYPE) {
            btBoxShape* box = static_cast<btBoxShape*>(shape);
            btVector3 halfExtents = box->getHalfExtentsWithMargin();
            renderer->drawWireBox(
                position, 
                glm::vec3(halfExtents.x(), halfExtents.y(), halfExtents.z()) * 2.0f,
                color, rotation
            );
        } else if (shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE) {
            btSphereShape* sphere = static_cast<btSphereShape*>(shape);
            renderer->drawWireSphere(position, sphere->getRadius(), color);
        }
        
        // Draw velocity vector
        if (!body->isStaticObject()) {
            const btVector3& vel = body->getLinearVelocity();
            glm::vec3 velocity(vel.x(), vel.y(), vel.z());
            if (glm::length(velocity) > 0.1f) {
                renderer->drawArrow(
                    position, 
                    position + glm::normalize(velocity),
                    glm::vec4(1, 1, 0, 1)
                );
            }
        }
    }
    
    // Draw contact points
    int numManifolds = m_dispatcher->getNumManifolds();
    for (int i = 0; i < numManifolds; ++i) {
        btPersistentManifold* manifold = m_dispatcher->getManifoldByIndexInternal(