#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../../src/components/RigidBodyComponent.h"
#include "../../src/components/CollisionComponent.h"
#include "../../src/components/CreaturePhysicsComponent.h"
#include "../../src/spatial/LayerMask.h"

using namespace VulkanMon;

// =============================================================================
// RIGIDBTODY COMPONENT TESTS
// =============================================================================

TEST_CASE("RigidBodyComponent Basic Functionality", "[Physics][RigidBody]") {
    SECTION("Default constructor creates valid component") {
        RigidBodyComponent rigidBody;

        REQUIRE(rigidBody.isDynamic == true);
        REQUIRE(rigidBody.mass == 1.0f);
        REQUIRE(rigidBody.restitution == 0.3f);
        REQUIRE(rigidBody.friction == 0.7f);
        REQUIRE(rigidBody.useGravity == true);
        REQUIRE(rigidBody.bodyID == 0); // No physics body yet
        REQUIRE_FALSE(rigidBody.hasPhysicsBody());
    }

    SECTION("Pokemon constructor creates appropriate values") {
        RigidBodyComponent pokemon(25.0f, false); // Pikachu-sized, can't rotate

        REQUIRE(pokemon.mass == 25.0f);
        REQUIRE(pokemon.freezeRotation == true);
        REQUIRE(pokemon.restitution == 0.1f); // Pokemon don't bounce much
        REQUIRE(pokemon.friction == 0.8f); // Good grip
        REQUIRE(pokemon.linearDamping == 0.3f); // Natural slowdown
    }

    SECTION("Pokeball factory creates bouncy sphere") {
        auto pokeball = RigidBodyComponent::createPokeball();

        REQUIRE(pokeball.mass == 0.2f); // Light
        REQUIRE(pokeball.restitution == 0.8f); // Very bouncy
        REQUIRE(pokeball.friction == 0.3f); // Low friction for rolling
        REQUIRE(pokeball.linearDamping == 0.1f); // Rolls smoothly
    }

    SECTION("Static object factory creates immovable body") {
        auto staticBody = RigidBodyComponent::createStatic();

        REQUIRE_FALSE(staticBody.isDynamic);
        REQUIRE(staticBody.mass == 0.0f); // Infinite mass
        REQUIRE_FALSE(staticBody.useGravity);
    }
}

TEST_CASE("RigidBodyComponent Engine Factory Methods", "[Physics][RigidBody][Factory]") {
    SECTION("dynamic() factory creates movable physics body") {
        auto dynamic = RigidBodyComponent::dynamic(5.0f);

        REQUIRE(dynamic.isDynamic == true);
        REQUIRE(dynamic.mass == 5.0f);
        REQUIRE(dynamic.useGravity == true);
        REQUIRE(dynamic.restitution == Catch::Approx(0.3f));
        REQUIRE(dynamic.friction == Catch::Approx(0.7f));
        REQUIRE(dynamic.linearDamping == Catch::Approx(0.1f));
        REQUIRE(dynamic.angularDamping == Catch::Approx(0.05f));
    }

    SECTION("dynamic() factory uses default mass") {
        auto dynamic = RigidBodyComponent::dynamic();
        REQUIRE(dynamic.mass == 1.0f);
    }

    SECTION("kinematic() factory creates programmatically controlled body") {
        auto kinematic = RigidBodyComponent::kinematic();

        REQUIRE(kinematic.isDynamic == true);
        REQUIRE(kinematic.isKinematic == true);
        REQUIRE(kinematic.useGravity == false);
        REQUIRE(kinematic.restitution == 0.0f);
        REQUIRE(kinematic.friction == 0.0f);
        REQUIRE(kinematic.mass == 1.0f);
    }

    SECTION("staticBody() factory creates immovable environment") {
        auto staticBody = RigidBodyComponent::staticBody();

        REQUIRE(staticBody.isDynamic == false);
        REQUIRE(staticBody.mass == 0.0f);
        REQUIRE(staticBody.useGravity == false);
        REQUIRE(staticBody.restitution == Catch::Approx(0.5f));
        REQUIRE(staticBody.friction == Catch::Approx(0.8f));
    }
}

TEST_CASE("RigidBodyComponent Physics Calculations", "[Physics][RigidBody]") {
    RigidBodyComponent rigidBody;
    rigidBody.mass = 10.0f;
    rigidBody.velocity = glm::vec3(5.0f, 0.0f, 0.0f);

    SECTION("Kinetic energy calculation") {
        float ke = rigidBody.getKineticEnergy();
        REQUIRE(ke == Catch::Approx(125.0f)); // 0.5 * 10 * 5^2 = 125
    }

    SECTION("Momentum calculation") {
        glm::vec3 momentum = rigidBody.getMomentum();
        REQUIRE(momentum.x == Catch::Approx(50.0f)); // 10 * 5 = 50
        REQUIRE(momentum.y == Catch::Approx(0.0f));
        REQUIRE(momentum.z == Catch::Approx(0.0f));
    }

    SECTION("Movement speed detection") {
        REQUIRE(rigidBody.isMovingFast(3.0f)); // Faster than threshold
        REQUIRE_FALSE(rigidBody.isMovingFast(10.0f)); // Slower than threshold
    }

    SECTION("Impulse application") {
        glm::vec3 initialVelocity = rigidBody.velocity;
        rigidBody.applyImpulse(glm::vec3(20.0f, 0.0f, 0.0f));

        // Impulse should change velocity by impulse/mass
        REQUIRE(rigidBody.velocity.x == Catch::Approx(7.0f)); // 5 + 20/10 = 7
        // needsSync removed - automatic sync now
    }
}

// =============================================================================
// COLLISION COMPONENT TESTS
// =============================================================================

TEST_CASE("CollisionComponent Basic Functionality", "[Physics][Collision]") {
    SECTION("Default constructor creates valid component") {
        CollisionComponent collision;

        REQUIRE(collision.shapeType == CollisionComponent::ShapeType::Box);
        REQUIRE(collision.dimensions == glm::vec3(1.0f));
        REQUIRE(collision.layer == LayerMask::Default);
        REQUIRE(collision.collidesWith == LayerMask::All);
        REQUIRE_FALSE(collision.isTrigger);
        REQUIRE_FALSE(collision.isStatic);
    }

    SECTION("Creature factory creates appropriate collision") {
        auto creature = CollisionComponent::createCreature(1.5f, 2.0f);

        REQUIRE(creature.shapeType == CollisionComponent::ShapeType::Capsule);
        REQUIRE(creature.dimensions.x == Catch::Approx(1.5f)); // radius
        REQUIRE(creature.dimensions.y == Catch::Approx(2.0f)); // height
        REQUIRE(creature.layer == LayerMask::Creatures);
        REQUIRE((creature.collidesWith & LayerMask::Creatures) == LayerMask::None); // Don't collide with other creatures
    }

    SECTION("Capture device factory creates capture-enabled sphere") {
        auto captureDevice = CollisionComponent::createCaptureDevice(0.15f);

        REQUIRE(captureDevice.shapeType == CollisionComponent::ShapeType::Sphere);
        REQUIRE(captureDevice.dimensions.x == Catch::Approx(0.15f));
        REQUIRE(captureDevice.layer == LayerMask::CaptureDevices);
        REQUIRE(captureDevice.captureRadius == Catch::Approx(0.3f)); // 2x collision radius
    }

    SECTION("Environment factory creates static collision") {
        auto env = CollisionComponent::createEnvironment(glm::vec3(4.0f, 6.0f, 2.0f));

        REQUIRE(env.shapeType == CollisionComponent::ShapeType::Box);
        REQUIRE(env.dimensions == glm::vec3(2.0f, 3.0f, 1.0f)); // Half-extents
        REQUIRE(env.layer == LayerMask::Environment);
        REQUIRE(env.isStatic == true);
    }

    SECTION("Water factory creates trigger zone") {
        auto water = CollisionComponent::createWater(glm::vec3(10.0f, 2.0f, 10.0f));

        REQUIRE(water.isTrigger == true);
        REQUIRE(water.isWater == true);
        REQUIRE_FALSE(water.blocksPokeballs);
        REQUIRE(water.layer == LayerMask::Water);
    }
}

TEST_CASE("CollisionComponent Engine Factory Methods", "[Physics][Collision][Factory]") {
    SECTION("sphere() factory creates spherical collision") {
        auto sphere = CollisionComponent::sphere(2.5f, LayerMask::Creatures);

        REQUIRE(sphere.shapeType == CollisionComponent::ShapeType::Sphere);
        REQUIRE(sphere.dimensions.x == Catch::Approx(2.5f));
        REQUIRE(sphere.dimensions.y == 0.0f); // Unused for sphere
        REQUIRE(sphere.dimensions.z == 0.0f); // Unused for sphere
        REQUIRE(sphere.layer == LayerMask::Creatures);
        REQUIRE(sphere.collidesWith == LayerMask::All);
    }

    SECTION("sphere() factory uses default layer") {
        auto sphere = CollisionComponent::sphere(1.0f);
        REQUIRE(sphere.layer == LayerMask::Default);
    }

    SECTION("box() factory creates box collision with proper half-extents") {
        auto box = CollisionComponent::box(glm::vec3(4.0f, 6.0f, 8.0f), LayerMask::Environment);

        REQUIRE(box.shapeType == CollisionComponent::ShapeType::Box);
        REQUIRE(box.dimensions.x == Catch::Approx(2.0f)); // Half-extent
        REQUIRE(box.dimensions.y == Catch::Approx(3.0f)); // Half-extent
        REQUIRE(box.dimensions.z == Catch::Approx(4.0f)); // Half-extent
        REQUIRE(box.layer == LayerMask::Environment);
        REQUIRE(box.collidesWith == LayerMask::All);
    }

    SECTION("box() factory uses default layer") {
        auto box = CollisionComponent::box(glm::vec3(2.0f));
        REQUIRE(box.layer == LayerMask::Default);
    }

    SECTION("capsule() factory creates capsule collision") {
        auto capsule = CollisionComponent::capsule(1.5f, 3.0f, LayerMask::Creatures);

        REQUIRE(capsule.shapeType == CollisionComponent::ShapeType::Capsule);
        REQUIRE(capsule.dimensions.x == Catch::Approx(1.5f)); // radius
        REQUIRE(capsule.dimensions.y == Catch::Approx(3.0f)); // height
        REQUIRE(capsule.dimensions.z == Catch::Approx(1.5f)); // radius (repeated)
        REQUIRE(capsule.layer == LayerMask::Creatures);
        REQUIRE(capsule.collidesWith == LayerMask::All);
    }

    SECTION("capsule() factory uses default layer") {
        auto capsule = CollisionComponent::capsule(1.0f, 2.0f);
        REQUIRE(capsule.layer == LayerMask::Default);
    }

    SECTION("plane() factory creates large thin box for ground") {
        auto plane = CollisionComponent::plane(glm::vec3(0, 1, 0), LayerMask::Terrain);

        REQUIRE(plane.shapeType == CollisionComponent::ShapeType::Box);
        REQUIRE(plane.dimensions.x == Catch::Approx(1000.0f)); // Large ground plane
        REQUIRE(plane.dimensions.y == Catch::Approx(0.1f)); // Thin
        REQUIRE(plane.dimensions.z == Catch::Approx(1000.0f)); // Large ground plane
        REQUIRE(plane.layer == LayerMask::Terrain);
        REQUIRE(plane.collidesWith == LayerMask::All);
        REQUIRE(plane.isStatic == true);
    }

    SECTION("plane() factory uses default normal and layer") {
        auto plane = CollisionComponent::plane();
        REQUIRE(plane.layer == LayerMask::Environment);
        REQUIRE(plane.isStatic == true);
    }
}

TEST_CASE("CollisionComponent Volume and Bounds Calculations", "[Physics][Collision]") {
    SECTION("Box volume calculation") {
        CollisionComponent box;
        box.shapeType = CollisionComponent::ShapeType::Box;
        box.dimensions = glm::vec3(2.0f, 3.0f, 4.0f); // Half-extents

        float volume = box.getVolume();
        REQUIRE(volume == Catch::Approx(192.0f)); // 8 * 2 * 3 * 4 = 192
    }

    SECTION("Sphere volume calculation") {
        CollisionComponent sphere;
        sphere.shapeType = CollisionComponent::ShapeType::Sphere;
        sphere.dimensions = glm::vec3(3.0f, 0.0f, 0.0f); // radius = 3

        float volume = sphere.getVolume();
        float expected = (4.0f / 3.0f) * 3.14159f * 27.0f; // 4/3 * pi * r^3
        REQUIRE(volume == Catch::Approx(expected).epsilon(0.01f));
    }

    SECTION("Bounding sphere radius") {
        CollisionComponent box;
        box.shapeType = CollisionComponent::ShapeType::Box;
        box.dimensions = glm::vec3(3.0f, 4.0f, 5.0f);

        float radius = box.getBoundingSphereRadius();
        float expected = glm::length(glm::vec3(3.0f, 4.0f, 5.0f));
        REQUIRE(radius == Catch::Approx(expected));
    }
}

TEST_CASE("CollisionComponent Layer Filtering", "[Physics][Collision]") {
    CollisionComponent collision;
    collision.collidesWith = LayerMask::Creatures | LayerMask::Environment;

    SECTION("Layer collision detection") {
        REQUIRE(collision.shouldCollideWith(LayerMask::Creatures));
        REQUIRE(collision.shouldCollideWith(LayerMask::Environment));
        REQUIRE_FALSE(collision.shouldCollideWith(LayerMask::Water));
        REQUIRE_FALSE(collision.shouldCollideWith(LayerMask::CaptureDevices));
    }

    SECTION("Entity type interaction checks") {
        CollisionComponent captureDevice = CollisionComponent::createCaptureDevice();
        REQUIRE(captureDevice.affectsCreatures());
        REQUIRE(captureDevice.affectsPlayer());

        CollisionComponent water = CollisionComponent::createWater(glm::vec3(5.0f));
        REQUIRE_FALSE(water.affectsCaptureDevices());
    }
}

// =============================================================================
// CREATURE PHYSICS COMPONENT TESTS
// =============================================================================

TEST_CASE("CreaturePhysicsComponent Basic Functionality", "[Physics][CreaturePhysics]") {
    SECTION("Default constructor creates balanced land creature") {
        CreaturePhysicsComponent creature;

        REQUIRE(creature.moveSpeed == 5.0f);
        REQUIRE(creature.jumpForce == 8.0f);
        REQUIRE_FALSE(creature.canFly);
        REQUIRE_FALSE(creature.canSwim);
        REQUIRE(creature.autoBalance == true);
        REQUIRE_FALSE(creature.isGrounded);
    }

    SECTION("Flying creature factory") {
        auto flyer = CreaturePhysicsComponent::createFlyingCreature(12.0f, 6.0f);

        REQUIRE(flyer.flySpeed == 12.0f);
        REQUIRE(flyer.moveSpeed == 6.0f);
        REQUIRE(flyer.canFly == true);
        REQUIRE_FALSE(flyer.preferGroundMovement);
        REQUIRE(flyer.airControl == Catch::Approx(0.9f));
    }

    SECTION("Water creature factory") {
        auto swimmer = CreaturePhysicsComponent::createWaterCreature(8.0f, 3.0f);

        REQUIRE(swimmer.maxSwimSpeed == 8.0f);
        REQUIRE(swimmer.moveSpeed == 3.0f); // Slow on land
        REQUIRE(swimmer.canSwim == true);
        REQUIRE(swimmer.buoyancy == Catch::Approx(0.9f));
    }

    SECTION("Heavy creature factory") {
        auto heavy = CreaturePhysicsComponent::createHeavyCreature(2.0f);

        REQUIRE(heavy.moveSpeed == 2.0f);
        REQUIRE(heavy.sprintMultiplier == Catch::Approx(1.5f)); // Limited sprint
        REQUIRE(heavy.stabilizingForce == Catch::Approx(100.0f)); // Very stable
        REQUIRE(heavy.slopeLimit == Catch::Approx(30.0f)); // Limited climbing
    }
}

TEST_CASE("CreaturePhysicsComponent Movement Logic", "[Physics][CreaturePhysics]") {
    CreaturePhysicsComponent creature;
    creature.moveSpeed = 10.0f;
    creature.sprintMultiplier = 2.0f;

    SECTION("Movement input handling") {
        creature.setMovementInput(glm::vec3(1.0f, 0.0f, 0.5f), true, false);

        // Direction should be normalized
        float length = glm::length(creature.inputDirection);
        REQUIRE(length == Catch::Approx(1.0f));
        REQUIRE(creature.wantsToSprint == true);
    }

    SECTION("Effective speed calculation") {
        creature.isGrounded = true;
        creature.wantsToSprint = true;

        float speed = creature.getEffectiveSpeed();
        REQUIRE(speed == Catch::Approx(20.0f)); // 10 * 2 = 20
    }

    SECTION("Swimming speed override") {
        creature.isSwimming = true;
        creature.maxSwimSpeed = 6.0f;
        creature.wantsToSprint = true;

        float speed = creature.getEffectiveSpeed();
        REQUIRE(speed == Catch::Approx(6.0f)); // Swimming overrides sprint
    }

    SECTION("Air control limitation") {
        creature.isGrounded = false;
        creature.isFlying = false;
        creature.airControl = 0.5f;
        creature.wantsToSprint = false;

        float speed = creature.getEffectiveSpeed();
        REQUIRE(speed == Catch::Approx(5.0f)); // 10 * 0.5 = 5
    }
}

TEST_CASE("CreaturePhysicsComponent Jump Mechanics", "[Physics][CreaturePhysics]") {
    CreaturePhysicsComponent creature;
    creature.jumpForce = 12.0f;
    creature.jumpCooldown = 0.5f;

    SECTION("Jump availability when grounded") {
        creature.isGrounded = true;
        creature.timeSinceJump = 1.0f; // Past cooldown

        REQUIRE(creature.canJump());
    }

    SECTION("Jump blocked when airborne") {
        creature.isGrounded = false;
        creature.timeSinceJump = 1.0f;

        REQUIRE_FALSE(creature.canJump());
    }

    SECTION("Jump blocked during cooldown") {
        creature.isGrounded = true;
        creature.timeSinceJump = 0.2f; // Still in cooldown

        REQUIRE_FALSE(creature.canJump());
    }

    SECTION("Jump velocity generation") {
        creature.isGrounded = true;
        creature.timeSinceJump = 1.0f;
        creature.wantsToJump = true;

        glm::vec3 jumpVel = creature.getJumpVelocity();
        REQUIRE(jumpVel.y == Catch::Approx(12.0f));
        REQUIRE(jumpVel.x == Catch::Approx(0.0f));
        REQUIRE(jumpVel.z == Catch::Approx(0.0f));
    }
}

TEST_CASE("CreaturePhysicsComponent Slope Detection", "[Physics][CreaturePhysics]") {
    CreaturePhysicsComponent creature;
    creature.slopeLimit = 45.0f;

    SECTION("Flat ground is walkable") {
        glm::vec3 flatNormal(0.0f, 1.0f, 0.0f);
        REQUIRE(creature.isSlopeWalkable(flatNormal));
    }

    SECTION("Steep slope is not walkable") {
        // 60-degree slope (beyond 45-degree limit)
        float angle = glm::radians(60.0f);
        glm::vec3 steepNormal(glm::sin(angle), glm::cos(angle), 0.0f);
        REQUIRE_FALSE(creature.isSlopeWalkable(steepNormal));
    }

    SECTION("Moderate slope is walkable") {
        // 30-degree slope (within 45-degree limit)
        float angle = glm::radians(30.0f);
        glm::vec3 moderateNormal(glm::sin(angle), glm::cos(angle), 0.0f);
        REQUIRE(creature.isSlopeWalkable(moderateNormal));
    }
}

TEST_CASE("CreaturePhysicsComponent Environmental State", "[Physics][CreaturePhysics]") {
    CreaturePhysicsComponent creature;
    creature.canSwim = true;
    creature.canFly = true;

    SECTION("Water interaction") {
        REQUIRE_FALSE(creature.isSwimming);

        creature.enterWater();
        REQUIRE(creature.isSwimming);

        creature.exitWater();
        REQUIRE_FALSE(creature.isSwimming);
    }

    SECTION("Flying state management") {
        REQUIRE_FALSE(creature.isFlying);
        REQUIRE(creature.isGrounded == false); // Default state

        creature.startFlying();
        REQUIRE(creature.isFlying);
        REQUIRE_FALSE(creature.isGrounded);

        creature.stopFlying();
        REQUIRE_FALSE(creature.isFlying);
    }

    SECTION("Creature without swim ability cannot enter water") {
        CreaturePhysicsComponent landCreature;
        landCreature.canSwim = false;

        landCreature.enterWater();
        REQUIRE_FALSE(landCreature.isSwimming); // Should remain false
    }
}