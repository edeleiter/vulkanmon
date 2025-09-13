#include <catch2/catch_test_macros.hpp>
#include "../src/core/World.h"
#include "../src/components/Transform.h"
#include "../src/components/Renderable.h"
#include "../src/components/VulkanMon::Camera.h"
#include "../src/systems/RenderSystem.h"
#include "../src/systems/VulkanMon::CameraSystem.h"

using namespace VulkanMon;

TEST_CASE("[ECS Integration] Complete system integration test", "[ECS][Integration]") {
    World world;

    SECTION("VulkanMon::Camera and render system integration") {
        // Add systems to world
        auto* cameraSystem = world.addSystem<VulkanMon::CameraSystem>();
        auto* renderSystem = world.addSystem<RenderSystem>();

        // Connect systems
        renderSystem->setVulkanMon::CameraSystem(cameraSystem);

        // Initialize world
        world.initialize();

        // Create camera entity
        EntityID cameraEntity = world.createEntity();

        Transform cameraTransform;
        cameraTransform.setPosition(glm::vec3(0.0f, 5.0f, 10.0f));

        VulkanMon::VulkanMon::Camera camera;
        camera.setPerspective(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
        camera.setActive(true);
        camera.setPriority(1);

        world.addComponent(cameraEntity, cameraTransform);
        world.addComponent(cameraEntity, camera);

        // Create some renderable entities
        for (int i = 0; i < 3; ++i) {
            EntityID entity = world.createEntity();

            Transform transform;
            transform.setPosition(glm::vec3(i * 5.0f, 0.0f, 0.0f));

            Renderable renderable("test" + std::to_string(i) + ".obj", "test.png");

            world.addComponent(entity, transform);
            world.addComponent(entity, renderable);
        }

        // Update world
        world.update(0.016f);

        // Verify systems are working
        REQUIRE(cameraSystem->getActiveVulkanMon::CameraEntity() == cameraEntity);

        VulkanMon::Camera* activeVulkanMon::Camera = cameraSystem->getActiveVulkanMon::Camera(world.getEntityManager());
        REQUIRE(activeVulkanMon::Camera != nullptr);
        REQUIRE(activeVulkanMon::Camera->isActive == true);

        glm::vec3 camPos = cameraSystem->getActiveVulkanMon::CameraPosition(world.getEntityManager());
        REQUIRE(camPos == glm::vec3(0.0f, 5.0f, 10.0f));

        // Test render system statistics
        REQUIRE(renderSystem->getMaxRenderDistance() > 0.0f);

        world.shutdown();
    }

    SECTION("Multiple cameras with priority system") {
        auto* cameraSystem = world.addSystem<VulkanMon::CameraSystem>();
        world.initialize();

        // Create low priority camera
        EntityID lowPriorityVulkanMon::Camera = world.createEntity();
        Transform lowTransform;
        lowTransform.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        VulkanMon::Camera lowVulkanMon::Camera;
        lowVulkanMon::Camera.setActive(true);
        lowVulkanMon::Camera.setPriority(1);

        world.addComponent(lowPriorityVulkanMon::Camera, lowTransform);
        world.addComponent(lowPriorityVulkanMon::Camera, lowVulkanMon::Camera);

        // Create high priority camera
        EntityID highPriorityVulkanMon::Camera = world.createEntity();
        Transform highTransform;
        highTransform.setPosition(glm::vec3(10.0f, 10.0f, 10.0f));
        VulkanMon::Camera highVulkanMon::Camera;
        highVulkanMon::Camera.setActive(true);
        highVulkanMon::Camera.setPriority(10);

        world.addComponent(highPriorityVulkanMon::Camera, highTransform);
        world.addComponent(highPriorityVulkanMon::Camera, highVulkanMon::Camera);

        // Update to process cameras
        world.update(0.016f);

        // High priority camera should be active
        REQUIRE(cameraSystem->getActiveVulkanMon::CameraEntity() == highPriorityVulkanMon::Camera);

        glm::vec3 activeCamPos = cameraSystem->getActiveVulkanMon::CameraPosition(world.getEntityManager());
        REQUIRE(activeCamPos == glm::vec3(10.0f, 10.0f, 10.0f));

        // Disable high priority camera
        VulkanMon::Camera& highVulkanMon::CameraRef = world.getComponent<VulkanMon::Camera>(highPriorityVulkanMon::Camera);
        highVulkanMon::CameraRef.setActive(false);

        // Update again
        world.update(0.016f);

        // Low priority camera should now be active
        REQUIRE(cameraSystem->getActiveVulkanMon::CameraEntity() == lowPriorityVulkanMon::Camera);

        world.shutdown();
    }
}

TEST_CASE("[ECS Integration] VulkanMon::Camera component functionality", "[ECS][VulkanMon::Camera]") {
    SECTION("Perspective camera matrix calculation") {
        VulkanMon::VulkanMon::Camera camera;
        camera.setPerspective(90.0f, 1.0f, 1.0f, 100.0f);

        // Update matrices
        camera.updateProjectionMatrix();
        camera.updateViewMatrix(
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        const glm::mat4& viewProjMatrix = camera.getViewProjectionMatrix();

        // Matrix should not be identity (has been calculated)
        REQUIRE(viewProjMatrix != glm::mat4(1.0f));
    }

    SECTION("Orthographic camera setup") {
        VulkanMon::VulkanMon::Camera camera;
        camera.setOrthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);

        REQUIRE(camera.type == VulkanMon::Camera::Type::ORTHOGRAPHIC);
        REQUIRE(camera.orthoLeft == -10.0f);
        REQUIRE(camera.orthoRight == 10.0f);
    }

    SECTION("VulkanMon::Camera priority and active state") {
        VulkanMon::Camera camera1;
        camera1.setPriority(5);
        camera1.setActive(true);

        VulkanMon::Camera camera2;
        camera2.setPriority(10);
        camera2.setActive(false);

        REQUIRE(camera1.priority == 5);
        REQUIRE(camera1.shouldRender() == true);
        REQUIRE(camera2.priority == 10);
        REQUIRE(camera2.shouldRender() == false);
    }
}

TEST_CASE("[ECS Integration] Pokemon game scenario test", "[ECS][Pokemon]") {
    World world;

    SECTION("Pokemon world with creatures and environment") {
        // Add systems
        auto* cameraSystem = world.addSystem<VulkanMon::CameraSystem>();
        auto* renderSystem = world.addSystem<RenderSystem>();
        renderSystem->setVulkanMon::CameraSystem(cameraSystem);

        world.initialize();

        // Create player camera
        EntityID playerVulkanMon::Camera = world.createEntity();
        Transform cameraTransform;
        cameraTransform.setPosition(glm::vec3(0.0f, 2.0f, 5.0f));

        VulkanMon::VulkanMon::Camera camera;
        camera.setPerspective(75.0f, 16.0f/9.0f, 0.1f, 1000.0f);
        camera.setActive(true);
        camera.setPriority(100); // Player camera has highest priority

        world.addComponent(playerVulkanMon::Camera, cameraTransform);
        world.addComponent(playerVulkanMon::Camera, camera);

        // Create Pokemon creatures
        std::vector<EntityID> pokemon;
        std::vector<std::string> pokemonNames = {"pikachu", "charizard", "blastoise"};

        for (size_t i = 0; i < pokemonNames.size(); ++i) {
            EntityID creature = world.createEntity();

            Transform transform;
            transform.setPosition(glm::vec3(i * 10.0f - 10.0f, 0.0f, -5.0f));
            transform.setScale(1.5f);

            Renderable renderable(pokemonNames[i] + ".obj", pokemonNames[i] + "_texture.png", 1);
            renderable.setRenderLayer(1); // Creature layer

            world.addComponent(creature, transform);
            world.addComponent(creature, renderable);

            pokemon.push_back(creature);
        }

        // Create environment objects (trees, rocks)
        for (int i = 0; i < 10; ++i) {
            EntityID envObject = world.createEntity();

            Transform transform;
            transform.setPosition(glm::vec3(
                (i % 5) * 15.0f - 30.0f,
                0.0f,
                (i / 5) * 20.0f - 20.0f
            ));

            std::string objectType = (i % 2 == 0) ? "tree" : "rock";
            Renderable renderable(objectType + ".obj", objectType + "_texture.png", 0);
            renderable.setRenderLayer(0); // Environment layer

            world.addComponent(envObject, transform);
            world.addComponent(envObject, renderable);
        }

        // Simulate game frames
        for (int frame = 0; frame < 5; ++frame) {
            world.update(0.016f); // ~60 FPS

            // Verify camera is active
            REQUIRE(cameraSystem->getActiveVulkanMon::CameraEntity() == playerVulkanMon::Camera);

            // Move camera (simulate player movement)
            Transform& camTransform = world.getComponent<Transform>(playerVulkanMon::Camera);
            camTransform.setPosition(camTransform.position + glm::vec3(1.0f, 0.0f, 0.0f));
        }

        // Verify final camera position
        Transform& finalCamTransform = world.getComponent<Transform>(playerVulkanMon::Camera);
        REQUIRE(finalCamTransform.position.x > 0.0f); // VulkanMon::Camera moved

        // Verify we have the expected number of entities
        REQUIRE(world.getComponentCount<Transform>() == 1 + 3 + 10); // VulkanMon::Camera + Pokemon + Environment
        REQUIRE(world.getComponentCount<Renderable>() == 3 + 10); // Pokemon + Environment (camera has no renderable)
        REQUIRE(world.getComponentCount<VulkanMon::Camera>() == 1); // Just the player camera

        world.shutdown();
    }
}