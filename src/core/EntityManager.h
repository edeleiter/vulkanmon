#pragma once

#include "Entity.h"
#include "ComponentArray.h"
#include <memory>
#include <unordered_map>
#include <queue>
#include <typeindex>

namespace VulkanMon {

class EntityManager {
private:
    // Next entity ID to assign
    EntityID nextEntityId = 1; // Start at 1 since 0 is INVALID_ENTITY

    // Reusable entity IDs (from destroyed entities)
    std::queue<EntityID> availableEntityIds;

    // Component arrays, indexed by component type
    std::unordered_map<std::type_index, std::unique_ptr<ComponentArrayBase>> componentArrays;

    // Get or create component array for type T
    template<typename T>
    ComponentArray<T>* getComponentArray() {
        std::type_index typeIndex = std::type_index(typeid(T));

        auto it = componentArrays.find(typeIndex);
        if (it == componentArrays.end()) {
            // Create new component array for this type
            auto newArray = std::make_unique<ComponentArray<T>>();
            ComponentArray<T>* rawPtr = newArray.get();
            componentArrays[typeIndex] = std::move(newArray);
            return rawPtr;
        }

        return static_cast<ComponentArray<T>*>(it->second.get());
    }

public:
    // Create new entity
    EntityID createEntity() {
        EntityID newEntity;

        if (!availableEntityIds.empty()) {
            // Reuse previously destroyed entity ID
            newEntity = availableEntityIds.front();
            availableEntityIds.pop();
        } else {
            // Create new entity ID
            newEntity = nextEntityId++;
        }

        return newEntity;
    }

    // Destroy entity and all its components
    void destroyEntity(EntityID entity) {
        if (entity == INVALID_ENTITY) {
            return;
        }

        // Remove entity from all component arrays
        for (auto& [typeIndex, componentArray] : componentArrays) {
            componentArray->removeEntity(entity);
        }

        // Mark entity ID as available for reuse
        availableEntityIds.push(entity);
    }

    // Add component to entity
    template<typename T>
    void addComponent(EntityID entity, const T& component) {
        getComponentArray<T>()->addComponent(entity, component);
    }

    // Remove component from entity
    template<typename T>
    void removeComponent(EntityID entity) {
        getComponentArray<T>()->removeComponent(entity);
    }

    // Get component from entity
    template<typename T>
    T& getComponent(EntityID entity) {
        return getComponentArray<T>()->getComponent(entity);
    }

    // Get component from entity (const version)
    template<typename T>
    const T& getComponent(EntityID entity) const {
        return getComponentArray<T>()->getComponent(entity);
    }

    // Check if entity has component
    template<typename T>
    bool hasComponent(EntityID entity) const {
        auto typeIndex = std::type_index(typeid(T));
        auto it = componentArrays.find(typeIndex);
        if (it == componentArrays.end()) {
            return false;
        }
        return static_cast<ComponentArray<T>*>(it->second.get())->hasComponent(entity);
    }

    // Get all components of type T (for system iteration)
    template<typename T>
    std::vector<T>& getAllComponents() {
        return getComponentArray<T>()->getAllComponents();
    }

    // Get all entity IDs that have component T
    template<typename T>
    const std::vector<EntityID>& getEntitiesWithComponent() {
        return getComponentArray<T>()->getAllEntityIds();
    }

    // Get component count for type T
    template<typename T>
    size_t getComponentCount() const {
        auto typeIndex = std::type_index(typeid(T));
        auto it = componentArrays.find(typeIndex);
        if (it == componentArrays.end()) {
            return 0;
        }
        return static_cast<ComponentArray<T>*>(it->second.get())->getComponentCount();
    }
};

} // namespace VulkanMon