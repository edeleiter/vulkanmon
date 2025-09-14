#include <catch2/catch_test_macros.hpp>
#include "../src/debug/ECSInspector.h"
#include "../src/core/World.h"
#include "../src/components/Transform.h"
#include "../src/components/Renderable.h"
#include "../src/components/Camera.h"
#include <memory>

using namespace VulkanMon::Debug;
using namespace VulkanMon;

// =============================================================================
// Basic ECS Inspector Functionality Tests
// =============================================================================

TEST_CASE("ECS Inspector Basic Construction", "[ECSInspector][Basic]") {
    SECTION("Valid construction with World") {
        World world;
        ECSInspector inspector(&world);

        REQUIRE(inspector.isEnabled() == true);  // Default enabled
        REQUIRE(inspector.getSelectedEntity() == INVALID_ENTITY);  // No selection initially
    }

    SECTION("Null World handling") {
        // Test that inspector can handle null World gracefully
        ECSInspector inspector(nullptr);
        REQUIRE(inspector.isEnabled() == true);
        REQUIRE(inspector.getSelectedEntity() == INVALID_ENTITY);
    }
}

TEST_CASE("ECS Inspector State Management", "[ECSInspector][State]") {
    World world;
    ECSInspector inspector(&world);

    SECTION("Toggle functionality") {
        REQUIRE(inspector.isEnabled() == true);

        inspector.setEnabled(false);
        REQUIRE(inspector.isEnabled() == false);

        inspector.setEnabled(true);
        REQUIRE(inspector.isEnabled() == true);
    }

    SECTION("Entity selection state") {
        // Test initial state
        REQUIRE(inspector.getSelectedEntity() == INVALID_ENTITY);

        // Create test entity and select it
        EntityID testEntity = world.createEntity();
        inspector.setSelectedEntity(testEntity);
        REQUIRE(inspector.getSelectedEntity() == testEntity);

        // Clear selection
        inspector.setSelectedEntity(INVALID_ENTITY);
        REQUIRE(inspector.getSelectedEntity() == INVALID_ENTITY);
    }

    SECTION("Invalid entity selection handling") {
        // Test selecting non-existent entity
        EntityID fakeEntity = static_cast<EntityID>(999999);
        inspector.setSelectedEntity(fakeEntity);
        REQUIRE(inspector.getSelectedEntity() == fakeEntity);  // Should store the value
    }
}

// =============================================================================
// Entity Management Testing
// =============================================================================

TEST_CASE("ECS Inspector Entity Operations", "[ECSInspector][Entity]") {
    World world;
    ECSInspector inspector(&world);

    SECTION("Entity selection with valid entities") {
        // Create test entities
        EntityID entity1 = world.createEntity();
        Transform transform1;
        transform1.position = glm::vec3(1.0f, 0.0f, 0.0f);
        world.addComponent(entity1, transform1);

        EntityID entity2 = world.createEntity();
        Transform transform2;
        Renderable renderable2;
        renderable2.meshPath = "sphere.obj";
        renderable2.materialId = 1;
        world.addComponent(entity2, transform2);
        world.addComponent(entity2, renderable2);

        // Test selection
        inspector.setSelectedEntity(entity1);
        REQUIRE(inspector.getSelectedEntity() == entity1);

        inspector.setSelectedEntity(entity2);
        REQUIRE(inspector.getSelectedEntity() == entity2);
    }

    SECTION("Selection persistence") {
        EntityID entity = world.createEntity();
        Transform transform;
        world.addComponent(entity, transform);

        inspector.setSelectedEntity(entity);

        // Selection should persist across multiple calls
        REQUIRE(inspector.getSelectedEntity() == entity);
        REQUIRE(inspector.getSelectedEntity() == entity);
    }
}

// =============================================================================
// Component Validation Testing
// =============================================================================

TEST_CASE("ECS Inspector Component Validation", "[ECSInspector][Components]") {
    World world;
    ECSInspector inspector(&world);

    SECTION("Transform component validation") {
        EntityID entity = world.createEntity();
        Transform transform;
        transform.position = glm::vec3(5.0f, 10.0f, -3.0f);
        world.addComponent(entity, transform);

        REQUIRE(world.hasComponent<Transform>(entity));
        const auto& storedTransform = world.getComponent<Transform>(entity);

        // Validate initial values
        REQUIRE(storedTransform.position.x == 5.0f);
        REQUIRE(storedTransform.position.y == 10.0f);
        REQUIRE(storedTransform.position.z == -3.0f);
    }

    SECTION("Renderable component validation") {
        EntityID entity = world.createEntity();
        Transform transform;
        Renderable renderable;
        renderable.meshPath = "pyramid.obj";
        renderable.materialId = 2;
        renderable.isVisible = true;

        world.addComponent(entity, transform);
        world.addComponent(entity, renderable);

        REQUIRE(world.hasComponent<Renderable>(entity));
        const auto& storedRenderable = world.getComponent<Renderable>(entity);

        // Validate initial values
        REQUIRE(storedRenderable.meshPath == "pyramid.obj");
        REQUIRE(storedRenderable.materialId == 2);
        REQUIRE(storedRenderable.isVisible == true);
    }

    SECTION("Camera component validation") {
        EntityID entity = world.createEntity();
        Transform transform;
        VulkanMon::Camera camera;
        camera.type = VulkanMon::Camera::Type::PERSPECTIVE;
        camera.fov = 45.0f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 100.0f;
        camera.isActive = true;

        world.addComponent(entity, transform);
        world.addComponent(entity, camera);

        REQUIRE(world.hasComponent<VulkanMon::Camera>(entity));
        const auto& storedCamera = world.getComponent<VulkanMon::Camera>(entity);

        // Validate initial values
        REQUIRE(storedCamera.type == VulkanMon::Camera::Type::PERSPECTIVE);
        REQUIRE(storedCamera.fov == 45.0f);
        REQUIRE(storedCamera.nearPlane == 0.1f);
        REQUIRE(storedCamera.farPlane == 100.0f);
        REQUIRE(storedCamera.isActive == true);
    }
}

// =============================================================================
// Component Modification Testing
// =============================================================================

TEST_CASE("ECS Inspector Component Modification", "[ECSInspector][Modification]") {
    World world;
    ECSInspector inspector(&world);

    SECTION("Transform modification") {
        EntityID entity = world.createEntity();
        Transform transform;
        transform.position = glm::vec3(0.0f);
        world.addComponent(entity, transform);

        // Modify transform
        auto& modifiableTransform = world.getComponent<Transform>(entity);
        modifiableTransform.position = glm::vec3(10.0f, 20.0f, 30.0f);
        modifiableTransform.setRotationEuler(45.0f, 90.0f, 0.0f);  // Use Euler angle setter
        modifiableTransform.scale = glm::vec3(2.0f, 2.0f, 2.0f);

        // Validate modifications
        const auto& validatedTransform = world.getComponent<Transform>(entity);
        REQUIRE(validatedTransform.position.x == 10.0f);
        REQUIRE(validatedTransform.scale.x == 2.0f);
        // Note: Quaternion rotation can't be easily validated without conversion
    }

    SECTION("Renderable modification") {
        EntityID entity = world.createEntity();
        Transform transform;
        Renderable renderable;
        renderable.meshPath = "test_cube.obj";
        world.addComponent(entity, transform);
        world.addComponent(entity, renderable);

        // Modify renderable
        auto& modifiableRenderable = world.getComponent<Renderable>(entity);
        modifiableRenderable.meshPath = "plane.obj";
        modifiableRenderable.materialId = 4;
        modifiableRenderable.isVisible = false;

        // Validate modifications
        const auto& validatedRenderable = world.getComponent<Renderable>(entity);
        REQUIRE(validatedRenderable.meshPath == "plane.obj");
        REQUIRE(validatedRenderable.materialId == 4);
        REQUIRE(validatedRenderable.isVisible == false);
    }
}

// =============================================================================
// Entity Creation and Management
// =============================================================================

TEST_CASE("ECS Inspector Entity Management", "[ECSInspector][Management]") {
    World world;
    ECSInspector inspector(&world);

    SECTION("Entity creation validation") {
        // Create entities with different components
        EntityID cubeEntity = world.createEntity();
        Transform transform1;
        Renderable renderable1;
        renderable1.meshPath = "test_cube.obj";
        renderable1.materialId = 0;
        world.addComponent(cubeEntity, transform1);
        world.addComponent(cubeEntity, renderable1);

        EntityID sphereEntity = world.createEntity();
        Transform transform2;
        Renderable renderable2;
        renderable2.meshPath = "sphere.obj";
        renderable2.materialId = 1;
        world.addComponent(sphereEntity, transform2);
        world.addComponent(sphereEntity, renderable2);

        EntityID cameraEntity = world.createEntity();
        Transform transform3;
        VulkanMon::Camera camera;
        camera.type = VulkanMon::Camera::Type::PERSPECTIVE;
        world.addComponent(cameraEntity, transform3);
        world.addComponent(cameraEntity, camera);

        // Validate entities have correct components
        REQUIRE(world.hasComponent<Transform>(cubeEntity));
        REQUIRE(world.hasComponent<Renderable>(cubeEntity));

        REQUIRE(world.hasComponent<Transform>(sphereEntity));
        REQUIRE(world.hasComponent<Renderable>(sphereEntity));

        REQUIRE(world.hasComponent<Transform>(cameraEntity));
        REQUIRE(world.hasComponent<VulkanMon::Camera>(cameraEntity));
    }

    SECTION("Entity deletion validation") {
        EntityID entity = world.createEntity();
        Transform transform;
        world.addComponent(entity, transform);

        REQUIRE(world.hasComponent<Transform>(entity));

        world.destroyEntity(entity);

        // Component should no longer be accessible
        REQUIRE_FALSE(world.hasComponent<Transform>(entity));
    }
}

// =============================================================================
// Error Handling and Edge Cases
// =============================================================================

TEST_CASE("ECS Inspector Error Handling", "[ECSInspector][ErrorHandling]") {
    SECTION("Null World handling") {
        ECSInspector inspector(nullptr);

        // Should not crash with null world
        REQUIRE(inspector.isEnabled() == true);
        REQUIRE(inspector.getSelectedEntity() == INVALID_ENTITY);

        // Setting selection should work (stored locally)
        inspector.setSelectedEntity(static_cast<EntityID>(123));
        REQUIRE(inspector.getSelectedEntity() == static_cast<EntityID>(123));
    }

    SECTION("Invalid entity operations") {
        World world;
        ECSInspector inspector(&world);

        // Test selection of invalid entities
        EntityID invalidEntity = static_cast<EntityID>(999999);
        inspector.setSelectedEntity(invalidEntity);
        REQUIRE(inspector.getSelectedEntity() == invalidEntity);

        // Test clearing invalid selections
        inspector.setSelectedEntity(INVALID_ENTITY);
        REQUIRE(inspector.getSelectedEntity() == INVALID_ENTITY);
    }

    SECTION("Component access with deleted entities") {
        World world;
        ECSInspector inspector(&world);

        EntityID entity = world.createEntity();
        Transform transform;
        world.addComponent(entity, transform);

        // Verify component exists
        REQUIRE(world.hasComponent<Transform>(entity));

        // Destroy entity
        world.destroyEntity(entity);

        // Component access should fail gracefully
        REQUIRE_FALSE(world.hasComponent<Transform>(entity));
    }
}

// =============================================================================
// Performance and Statistics
// =============================================================================

TEST_CASE("ECS Inspector Statistics", "[ECSInspector][Statistics]") {
    World world;
    ECSInspector inspector(&world);

    SECTION("Component counting") {
        // Create entities with different component combinations
        for (int i = 0; i < 10; ++i) {
            EntityID entity = world.createEntity();
            Transform transform;
            world.addComponent(entity, transform);

            if (i % 2 == 0) {
                Renderable renderable;
                world.addComponent(entity, renderable);
            }

            if (i % 3 == 0) {
                VulkanMon::Camera camera;
                world.addComponent(entity, camera);
            }
        }

        // Validate component counts
        REQUIRE(world.getComponentCount<Transform>() == 10);  // All entities have Transform
        REQUIRE(world.getComponentCount<Renderable>() == 5);  // Half have Renderable
        REQUIRE(world.getComponentCount<VulkanMon::Camera>() == 4);      // Every third has Camera (0,3,6,9)
    }

    SECTION("Empty world handling") {
        REQUIRE(world.getComponentCount<Transform>() == 0);
        REQUIRE(world.getComponentCount<Renderable>() == 0);
        REQUIRE(world.getComponentCount<VulkanMon::Camera>() == 0);
    }
}