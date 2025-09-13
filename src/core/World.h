#pragma once

#include "EntityManager.h"
#include "SystemManager.h"
#include "../rendering/VulkanRenderer.h"

namespace VulkanMon {

class World {
private:
    EntityManager entityManager;
    SystemManager systemManager;

public:
    World() = default;
    ~World() = default;

    // Non-copyable but movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = default;
    World& operator=(World&&) = default;

    // Initialize the world and all systems
    void initialize() {
        systemManager.initializeSystems(entityManager);
    }

    // Update the world (called every frame)
    void update(float deltaTime) {
        systemManager.updateSystems(deltaTime, entityManager);
    }

    // Render the world
    void render(VulkanRenderer& renderer) {
        systemManager.renderSystems(renderer, entityManager);
    }

    // Shutdown the world and all systems
    void shutdown() {
        systemManager.shutdownSystems(entityManager);
    }

    // Entity management
    EntityID createEntity() {
        return entityManager.createEntity();
    }

    void destroyEntity(EntityID entity) {
        entityManager.destroyEntity(entity);
    }

    template<typename T>
    void addComponent(EntityID entity, const T& component) {
        entityManager.addComponent(entity, component);
    }

    template<typename T>
    void removeComponent(EntityID entity) {
        entityManager.removeComponent<T>(entity);
    }

    template<typename T>
    T& getComponent(EntityID entity) {
        return entityManager.getComponent<T>(entity);
    }

    template<typename T>
    const T& getComponent(EntityID entity) const {
        return entityManager.getComponent<T>(entity);
    }

    template<typename T>
    bool hasComponent(EntityID entity) const {
        return entityManager.hasComponent<T>(entity);
    }

    // System management
    template<typename T, typename... Args>
    T* addSystem(Args&&... args) {
        return systemManager.addSystem<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    T* getSystem() {
        return systemManager.getSystem<T>();
    }

    template<typename T>
    void removeSystem() {
        systemManager.removeSystem<T>();
    }

    template<typename T>
    bool hasSystem() const {
        return systemManager.hasSystem<T>();
    }

    // Direct access to managers (for advanced use)
    EntityManager& getEntityManager() { return entityManager; }
    const EntityManager& getEntityManager() const { return entityManager; }

    SystemManager& getSystemManager() { return systemManager; }
    const SystemManager& getSystemManager() const { return systemManager; }

    // Stats and debugging
    template<typename T>
    size_t getComponentCount() const {
        return entityManager.getComponentCount<T>();
    }

    size_t getSystemCount() const {
        return systemManager.getSystemCount();
    }
};

} // namespace VulkanMon