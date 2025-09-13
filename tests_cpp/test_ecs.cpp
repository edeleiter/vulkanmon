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