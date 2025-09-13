#pragma once

#include "Entity.h"
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace VulkanMon {

// Base class for type-erased component storage
class ComponentArrayBase {
public:
    virtual ~ComponentArrayBase() = default;
    virtual void removeEntity(EntityID entity) = 0;
};

// Templated component storage using sparse set approach
template<typename T>
class ComponentArray : public ComponentArrayBase {
private:
    // Dense array of components for cache-friendly iteration
    std::vector<T> components;

    // Entity ID for each component (same indexing as components)
    std::vector<EntityID> entityIds;

    // Fast lookup: entity -> index in dense arrays
    std::unordered_map<EntityID, size_t> entityToIndex;

public:
    // Add component to entity
    void addComponent(EntityID entity, const T& component) {
        if (hasComponent(entity)) {
            throw std::runtime_error("Entity already has this component type");
        }

        // Add to dense arrays
        size_t newIndex = components.size();
        components.push_back(component);
        entityIds.push_back(entity);

        // Update lookup map
        entityToIndex[entity] = newIndex;
    }

    // Remove component from entity
    void removeComponent(EntityID entity) {
        if (!hasComponent(entity)) {
            return; // Silently ignore if component doesn't exist
        }

        size_t indexToRemove = entityToIndex[entity];
        size_t lastIndex = components.size() - 1;

        // Move last element to the removed position (swap and pop)
        if (indexToRemove != lastIndex) {
            components[indexToRemove] = std::move(components[lastIndex]);
            entityIds[indexToRemove] = entityIds[lastIndex];

            // Update the moved entity's index
            entityToIndex[entityIds[indexToRemove]] = indexToRemove;
        }

        // Remove last element
        components.pop_back();
        entityIds.pop_back();
        entityToIndex.erase(entity);
    }

    // Get component reference
    T& getComponent(EntityID entity) {
        auto it = entityToIndex.find(entity);
        if (it == entityToIndex.end()) {
            throw std::runtime_error("Entity does not have this component type");
        }
        return components[it->second];
    }

    // Get component reference (const version)
    const T& getComponent(EntityID entity) const {
        auto it = entityToIndex.find(entity);
        if (it == entityToIndex.end()) {
            throw std::runtime_error("Entity does not have this component type");
        }
        return components[it->second];
    }

    // Check if entity has component
    bool hasComponent(EntityID entity) const {
        return entityToIndex.find(entity) != entityToIndex.end();
    }

    // Get all components (for system iteration)
    std::vector<T>& getAllComponents() { return components; }
    const std::vector<T>& getAllComponents() const { return components; }

    // Get all entity IDs (for system iteration)
    const std::vector<EntityID>& getAllEntityIds() const { return entityIds; }

    // Get component count
    size_t getComponentCount() const { return components.size(); }

    // Remove entity from this component array (implements base class method)
    void removeEntity(EntityID entity) override {
        removeComponent(entity);
    }
};

} // namespace VulkanMon