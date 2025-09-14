#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"
#include "../systems/RenderSystem.h"
#include "../utils/Logger.h"

namespace VulkanMon {

// Example demonstrating ECS usage for Pokemon-style game
class ECSExample {
public:
    static void demonstrateECS() {
        VKMON_INFO("=== ECS Framework Demonstration ===");

        // Create the world
        World world;

        // Add the render system
        auto* renderSystem = world.addSystem<RenderSystem>();
        VKMON_INFO("Added RenderSystem to world");

        // Initialize world
        world.initialize();

        // Create example entities representing Pokemon game objects

        // 1. Create a Pokemon creature (Pikachu)
        EntityID pikachu = world.createEntity();

        Transform pikachuTransform;
        pikachuTransform.setPosition(glm::vec3(10.0f, 0.0f, 0.0f));
        pikachuTransform.setScale(1.5f); // Slightly larger

        Renderable pikachuRenderable("pikachu.obj", "pikachu_texture.png", 1);
        pikachuRenderable.setRenderLayer(1); // Creature layer

        world.addComponent(pikachu, pikachuTransform);
        world.addComponent(pikachu, pikachuRenderable);

        VKMON_INFO("Created Pikachu entity at position (10, 0, 0)");

        // 2. Create environment objects (trees)
        for (int i = 0; i < 5; ++i) {
            EntityID tree = world.createEntity();

            Transform treeTransform;
            treeTransform.setPosition(glm::vec3(i * 20.0f, 0.0f, -10.0f));
            treeTransform.setScale(glm::vec3(2.0f, 3.0f, 2.0f)); // Tall trees

            Renderable treeRenderable("tree.obj", "tree_texture.png", 2);
            treeRenderable.setRenderLayer(0); // Environment layer

            world.addComponent(tree, treeTransform);
            world.addComponent(tree, treeRenderable);
        }
        VKMON_INFO("Created 5 tree entities");

        // 3. Create grass patches (lots of them for performance testing)
        for (int x = -10; x <= 10; x += 5) {
            for (int z = -10; z <= 10; z += 5) {
                EntityID grass = world.createEntity();

                Transform grassTransform;
                grassTransform.setPosition(glm::vec3(x, -1.0f, z));
                grassTransform.setScale(0.5f); // Small grass patches

                Renderable grassRenderable("grass.obj", "grass_texture.png", 0);
                grassRenderable.setRenderLayer(0); // Environment layer
                grassRenderable.lodDistance = 50.0f; // Closer LOD for small objects

                world.addComponent(grass, grassTransform);
                world.addComponent(grass, grassRenderable);
            }
        }
        VKMON_INFO("Created grass patch entities");

        // 4. Create a hidden Pokemon (testing visibility)
        EntityID hiddenPokemon = world.createEntity();
        Transform hiddenTransform;
        hiddenTransform.setPosition(glm::vec3(-20.0f, 0.0f, 0.0f));

        Renderable hiddenRenderable("pokemon.obj", "pokemon_texture.png", 1);
        hiddenRenderable.setVisible(false); // This one is hidden

        world.addComponent(hiddenPokemon, hiddenTransform);
        world.addComponent(hiddenPokemon, hiddenRenderable);
        VKMON_INFO("Created hidden Pokemon entity");

        // Simulate a few frames of gameplay
        VKMON_INFO("Simulating gameplay frames...");

        for (int frame = 0; frame < 3; ++frame) {
            float deltaTime = 0.016f; // ~60 FPS

            VKMON_INFO("Frame " + std::to_string(frame + 1) + ":");

            // Update world (all systems)
            world.update(deltaTime);

            // Note: We can't actually render without a VulkanRenderer instance,
            // but we can test the render system's logic

            // Show statistics
            VKMON_INFO("  - Total entities: " + std::to_string(
                world.getComponentCount<Transform>()));
            VKMON_INFO("  - Renderable entities: " + std::to_string(
                world.getComponentCount<Renderable>()));
        }

        // Demonstrate component access
        VKMON_INFO("=== Component Access Example ===");

        // Move Pikachu
        Transform& pikachuTransformRef = world.getComponent<Transform>(pikachu);
        glm::vec3 oldPos = pikachuTransformRef.position;
        pikachuTransformRef.setPosition(oldPos + glm::vec3(5.0f, 0.0f, 0.0f));

        VKMON_INFO("Moved Pikachu from (" +
                  std::to_string(oldPos.x) + ", " + std::to_string(oldPos.y) + ", " + std::to_string(oldPos.z) +
                  ") to (" +
                  std::to_string(pikachuTransformRef.position.x) + ", " +
                  std::to_string(pikachuTransformRef.position.y) + ", " +
                  std::to_string(pikachuTransformRef.position.z) + ")");

        // Test entity destruction
        world.destroyEntity(hiddenPokemon);
        VKMON_INFO("Destroyed hidden Pokemon entity");

        // Cleanup
        world.shutdown();
        VKMON_INFO("=== ECS Demonstration Complete ===");
    }

    // Example of creating a basic Pokemon creature
    static EntityID createPokemon(World& world, const std::string& species,
                                 const glm::vec3& position, float scale = 1.0f) {
        EntityID pokemon = world.createEntity();

        Transform transform;
        transform.setPosition(position);
        transform.setScale(scale);

        Renderable renderable(species + ".obj", species + "_texture.png", 1);
        renderable.setRenderLayer(1); // Creature layer

        world.addComponent(pokemon, transform);
        world.addComponent(pokemon, renderable);

        return pokemon;
    }
};

} // namespace VulkanMon