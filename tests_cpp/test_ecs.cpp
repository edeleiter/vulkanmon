#include <catch2/catch_test_macros.hpp>
#include "../src/core/EntityManager.h"
#include "../src/core/World.h"
#include "../src/core/SystemImpl.h"
#include "../src/components/Transform.h"
#include "../src/components/Renderable.h"

using namespace VulkanMon;

// Simple test component
struct TestComponent {
    VKMON_COMPONENT(TestComponent)
    int value = 0;
    TestComponent() = default;
    TestComponent(int v) : value(v) {}
};

TEST_CASE("[ECS] Entity creation and destruction", "[ECS][Entity]") {
    EntityManager manager;

    SECTION("Create valid entities") {
        EntityID entity1 = manager.createEntity();
        EntityID entity2 = manager.createEntity();

        REQUIRE(entity1 != INVALID_ENTITY);
        REQUIRE(entity2 != INVALID_ENTITY);
        REQUIRE(entity1 != entity2);
    }

    SECTION("Entity ID reuse after destruction") {
        EntityID entity1 = manager.createEntity();
        EntityID originalId = entity1;

        manager.destroyEntity(entity1);

        // Next entity should reuse the ID
        EntityID entity2 = manager.createEntity();
        REQUIRE(entity2 == originalId);
    }
}

TEST_CASE("[ECS] Component add/remove/get operations", "[ECS][Component]") {
    EntityManager manager;
    EntityID entity = manager.createEntity();

    SECTION("Add and get component") {
        TestComponent comp(42);
        manager.addComponent(entity, comp);

        REQUIRE(manager.hasComponent<TestComponent>(entity));
        REQUIRE(manager.getComponent<TestComponent>(entity).value == 42);
    }

    SECTION("Remove component") {
        TestComponent comp(123);
        manager.addComponent(entity, comp);
        REQUIRE(manager.hasComponent<TestComponent>(entity));

        manager.removeComponent<TestComponent>(entity);
        REQUIRE_FALSE(manager.hasComponent<TestComponent>(entity));
    }

    SECTION("Component count tracking") {
        REQUIRE(manager.getComponentCount<TestComponent>() == 0);

        manager.addComponent(entity, TestComponent(1));
        REQUIRE(manager.getComponentCount<TestComponent>() == 1);

        EntityID entity2 = manager.createEntity();
        manager.addComponent(entity2, TestComponent(2));
        REQUIRE(manager.getComponentCount<TestComponent>() == 2);

        manager.removeComponent<TestComponent>(entity);
        REQUIRE(manager.getComponentCount<TestComponent>() == 1);
    }

    SECTION("Multiple component types") {
        Transform transform;
        transform.setPosition(glm::vec3(1.0f, 2.0f, 3.0f));

        Renderable renderable("test.obj", "test.png");

        manager.addComponent(entity, transform);
        manager.addComponent(entity, renderable);

        REQUIRE(manager.hasComponent<Transform>(entity));
        REQUIRE(manager.hasComponent<Renderable>(entity));

        REQUIRE(manager.getComponent<Transform>(entity).position == glm::vec3(1.0f, 2.0f, 3.0f));
        REQUIRE(manager.getComponent<Renderable>(entity).meshPath == "test.obj");
    }
}

TEST_CASE("[ECS] Transform component functionality", "[ECS][Transform]") {
    Transform transform;

    SECTION("Default values") {
        REQUIRE(transform.position == glm::vec3(0.0f));
        REQUIRE(transform.scale == glm::vec3(1.0f));
    }

    SECTION("Position operations") {
        glm::vec3 newPos(1.0f, 2.0f, 3.0f);
        transform.setPosition(newPos);

        REQUIRE(transform.position == newPos);
        REQUIRE(transform.isDirty == true);
    }

    SECTION("Scale operations") {
        transform.setScale(2.0f);
        REQUIRE(transform.scale == glm::vec3(2.0f));

        transform.setScale(glm::vec3(1.0f, 2.0f, 3.0f));
        REQUIRE(transform.scale == glm::vec3(1.0f, 2.0f, 3.0f));
    }

    SECTION("Model matrix calculation") {
        transform.setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        transform.setScale(2.0f);

        const glm::mat4& matrix = transform.getModelMatrix();

        // Test that position is correctly encoded in matrix
        glm::vec3 matrixPos = glm::vec3(matrix[3]);
        REQUIRE(glm::distance(matrixPos, glm::vec3(5.0f, 0.0f, 0.0f)) < 0.001f);
    }

    SECTION("Direction vectors") {
        // Default rotation should give standard coordinate system
        glm::vec3 forward = transform.getForward();
        glm::vec3 right = transform.getRight();
        glm::vec3 up = transform.getUp();

        REQUIRE(glm::distance(forward, glm::vec3(0.0f, 0.0f, -1.0f)) < 0.001f);
        REQUIRE(glm::distance(right, glm::vec3(1.0f, 0.0f, 0.0f)) < 0.001f);
        REQUIRE(glm::distance(up, glm::vec3(0.0f, 1.0f, 0.0f)) < 0.001f);
    }
}

TEST_CASE("[ECS] Renderable component functionality", "[ECS][Renderable]") {
    SECTION("Constructor and defaults") {
        Renderable renderable;
        REQUIRE(renderable.isVisible == true);
        REQUIRE(renderable.castsShadow == true);
        REQUIRE(renderable.receivesShadow == true);
        REQUIRE(renderable.renderLayer == 0);
    }

    SECTION("Constructor with parameters") {
        Renderable renderable("mesh.obj", "texture.png", 5);
        REQUIRE(renderable.meshPath == "mesh.obj");
        REQUIRE(renderable.texturePath == "texture.png");
        REQUIRE(renderable.materialId == 5);
    }

    SECTION("Render distance check") {
        Renderable renderable;
        renderable.lodDistance = 50.0f;

        REQUIRE(renderable.shouldRender(25.0f) == true);  // Within range
        REQUIRE(renderable.shouldRender(75.0f) == false); // Out of range

        renderable.setVisible(false);
        REQUIRE(renderable.shouldRender(25.0f) == false); // Not visible
    }
}

TEST_CASE("[ECS] World integration test", "[ECS][World]") {
    World world;

    SECTION("Entity and component operations through world") {
        EntityID entity = world.createEntity();
        REQUIRE(entity != INVALID_ENTITY);

        Transform transform;
        transform.setPosition(glm::vec3(10.0f, 20.0f, 30.0f));
        world.addComponent(entity, transform);

        REQUIRE(world.hasComponent<Transform>(entity));
        REQUIRE(world.getComponent<Transform>(entity).position == glm::vec3(10.0f, 20.0f, 30.0f));

        world.destroyEntity(entity);
        REQUIRE_FALSE(world.hasComponent<Transform>(entity));
    }

    SECTION("Component count tracking") {
        REQUIRE(world.getComponentCount<Transform>() == 0);

        EntityID entity1 = world.createEntity();
        EntityID entity2 = world.createEntity();

        world.addComponent(entity1, Transform{});
        world.addComponent(entity2, Transform{});

        REQUIRE(world.getComponentCount<Transform>() == 2);

        world.destroyEntity(entity1);
        REQUIRE(world.getComponentCount<Transform>() == 1);
    }
}

// Simple test system for integration testing
class TestRenderSystem : public System<Transform, Renderable> {
public:
    int updateCallCount = 0;
    int renderCallCount = 0;

    void update(float deltaTime, EntityManager& entityManager) override {
        updateCallCount++;

        // Process all entities with both Transform and Renderable
        auto& transforms = entityManager.getAllComponents<Transform>();
        auto& renderables = entityManager.getAllComponents<Renderable>();
        auto& entityIds = entityManager.getEntitiesWithComponent<Transform>();

        for (size_t i = 0; i < transforms.size(); ++i) {
            EntityID entity = entityIds[i];
            if (hasRequiredComponents(entity, entityManager)) {
                // This entity has both components - process it
                Transform& transform = transforms[i];
                Renderable& renderable = entityManager.getComponent<Renderable>(entity);

                // Simple test logic: move object slightly
                if (renderable.isVisible) {
                    transform.setPosition(transform.position + glm::vec3(0.1f * deltaTime, 0.0f, 0.0f));
                }
            }
        }
    }

    void render(VulkanRenderer& renderer, EntityManager& entityManager) override {
        renderCallCount++;
    }
};

TEST_CASE("[ECS] System integration test", "[ECS][System]") {
    World world;

    SECTION("Add and update systems") {
        auto* testSystem = world.addSystem<TestRenderSystem>();
        REQUIRE(testSystem != nullptr);
        REQUIRE(world.hasSystem<TestRenderSystem>());
        REQUIRE(world.getSystemCount() == 1);

        // Create test entity
        EntityID entity = world.createEntity();
        Transform transform;
        transform.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        world.addComponent(entity, transform);
        world.addComponent(entity, Renderable("test.obj"));

        // Update world - should call system update
        world.update(1.0f);
        REQUIRE(testSystem->updateCallCount == 1);

        // Check that system moved the entity
        const Transform& updatedTransform = world.getComponent<Transform>(entity);
        REQUIRE(updatedTransform.position.x > 0.0f);
    }
}

TEST_CASE("[ECS] Multi-Object Rendering Support", "[ECS][MultiObject]") {
    World world;

    SECTION("Diverse renderable components") {
        // Test that ECS supports multiple different renderable components
        // as implemented in Phase 6.1 Multi-Object ECS Rendering

        // Create entities with different mesh types
        EntityID cubeEntity = world.createEntity();
        EntityID sphereEntity = world.createEntity();
        EntityID pyramidEntity = world.createEntity();
        EntityID planeEntity = world.createEntity();

        // Add diverse renderable components
        Renderable cubeRenderable("test_cube.obj", "default", 0);  // Default material
        Renderable sphereRenderable("sphere.obj", "default", 1);    // Gold material
        Renderable pyramidRenderable("pyramid.obj", "default", 2);  // Ruby material
        Renderable planeRenderable("plane.obj", "default", 4);      // Emerald material

        world.addComponent(cubeEntity, cubeRenderable);
        world.addComponent(sphereEntity, sphereRenderable);
        world.addComponent(pyramidEntity, pyramidRenderable);
        world.addComponent(planeEntity, planeRenderable);

        // Verify components were added correctly
        REQUIRE(world.hasComponent<Renderable>(cubeEntity));
        REQUIRE(world.hasComponent<Renderable>(sphereEntity));
        REQUIRE(world.hasComponent<Renderable>(pyramidEntity));
        REQUIRE(world.hasComponent<Renderable>(planeEntity));

        // Verify mesh paths are different
        const auto& cubeRend = world.getComponent<Renderable>(cubeEntity);
        const auto& sphereRend = world.getComponent<Renderable>(sphereEntity);
        const auto& pyramidRend = world.getComponent<Renderable>(pyramidEntity);
        const auto& planeRend = world.getComponent<Renderable>(planeEntity);

        REQUIRE(cubeRend.meshPath != sphereRend.meshPath);
        REQUIRE(sphereRend.meshPath != pyramidRend.meshPath);
        REQUIRE(pyramidRend.meshPath != planeRend.meshPath);

        // Verify material IDs are different
        REQUIRE(cubeRend.materialId != sphereRend.materialId);
        REQUIRE(sphereRend.materialId != pyramidRend.materialId);
        REQUIRE(pyramidRend.materialId != planeRend.materialId);

        INFO("Multi-object ECS rendering supports:");
        INFO("- Different mesh paths per entity");
        INFO("- Different material IDs per entity");
        INFO("- Independent component management");
    }

    SECTION("Multi-object transform diversity") {
        // Test that multiple objects can have different transforms
        EntityID entity1 = world.createEntity();
        EntityID entity2 = world.createEntity();
        EntityID entity3 = world.createEntity();

        Transform transform1, transform2, transform3;
        transform1.setPosition(glm::vec3(-3.0f, 0.0f, 0.0f));  // Left
        transform2.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));   // Center
        transform3.setPosition(glm::vec3(3.0f, 0.0f, 0.0f));   // Right

        transform1.setScale(glm::vec3(1.0f));
        transform2.setScale(glm::vec3(0.8f));
        transform3.setScale(glm::vec3(1.2f));

        world.addComponent(entity1, transform1);
        world.addComponent(entity2, transform2);
        world.addComponent(entity3, transform3);

        // Verify positions are different
        const auto& t1 = world.getComponent<Transform>(entity1);
        const auto& t2 = world.getComponent<Transform>(entity2);
        const auto& t3 = world.getComponent<Transform>(entity3);

        REQUIRE(t1.position.x != t2.position.x);
        REQUIRE(t2.position.x != t3.position.x);
        REQUIRE(t1.scale != t2.scale);
        REQUIRE(t2.scale != t3.scale);

        INFO("Multi-object transforms support:");
        INFO("- Independent positioning");
        INFO("- Independent scaling");
        INFO("- Spatial diversity for visual distinction");
    }

    SECTION("Renderable component material assignments") {
        // Test the material system integration from Phase 6.1
        std::vector<uint32_t> materialIds = {0, 1, 2, 3, 4}; // Default, Gold, Ruby, Chrome, Emerald
        std::vector<std::string> meshPaths = {
            "test_cube.obj",
            "sphere.obj",
            "pyramid.obj",
            "sphere.obj",   // Second sphere instance
            "plane.obj"
        };

        REQUIRE(materialIds.size() == 5);
        REQUIRE(meshPaths.size() == 5);

        // Create entities and assign diverse materials
        std::vector<EntityID> entities;
        for (size_t i = 0; i < materialIds.size(); ++i) {
            EntityID entity = world.createEntity();
            entities.push_back(entity);

            Renderable renderable(meshPaths[i], "default", materialIds[i]);
            world.addComponent(entity, renderable);
        }

        // Verify material diversity
        for (size_t i = 0; i < entities.size(); ++i) {
            const auto& renderable = world.getComponent<Renderable>(entities[i]);
            REQUIRE(renderable.materialId == materialIds[i]);
            REQUIRE(renderable.meshPath == meshPaths[i]);
        }

        INFO("Material assignment verification:");
        INFO("- 5 different materials (0=Default, 1=Gold, 2=Ruby, 3=Chrome, 4=Emerald)");
        INFO("- 4 different mesh types (cube, sphere, pyramid, plane)");
        INFO("- Material-mesh combinations create visual diversity");
    }
}

// Phase 6.1 Multi-Object ECS Rendering Tests Complete:
// - Multi-object component management
// - Diverse mesh path support
// - Material assignment verification
// - Transform diversity for spatial separation