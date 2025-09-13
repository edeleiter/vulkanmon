#pragma once

#include "System.h"
#include "EntityManager.h"
#include "../rendering/VulkanRenderer.h"
#include <memory>
#include <vector>
#include <typeindex>
#include <unordered_map>

namespace VulkanMon {

class SystemManager {
private:
    std::vector<std::unique_ptr<SystemBase>> systems;
    std::unordered_map<std::type_index, SystemBase*> systemLookup;

public:
    // Add a system to the manager
    template<typename T, typename... Args>
    T* addSystem(Args&&... args) {
        static_assert(std::is_base_of_v<SystemBase, T>, "T must inherit from SystemBase");

        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* rawPtr = system.get();

        systems.push_back(std::move(system));
        systemLookup[std::type_index(typeid(T))] = rawPtr;

        return rawPtr;
    }

    // Get a system by type
    template<typename T>
    T* getSystem() {
        auto it = systemLookup.find(std::type_index(typeid(T)));
        if (it != systemLookup.end()) {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }

    // Remove a system
    template<typename T>
    void removeSystem() {
        auto typeIndex = std::type_index(typeid(T));
        auto it = systemLookup.find(typeIndex);
        if (it != systemLookup.end()) {
            // Find the system in the vector and remove it
            SystemBase* systemPtr = it->second;
            auto systemIt = std::find_if(systems.begin(), systems.end(),
                [systemPtr](const std::unique_ptr<SystemBase>& ptr) {
                    return ptr.get() == systemPtr;
                });

            if (systemIt != systems.end()) {
                systems.erase(systemIt);
            }

            systemLookup.erase(it);
        }
    }

    // Initialize all systems
    void initializeSystems(EntityManager& entityManager) {
        for (auto& system : systems) {
            system->initialize(entityManager);
        }
    }

    // Update all systems
    void updateSystems(float deltaTime, EntityManager& entityManager) {
        for (auto& system : systems) {
            system->update(deltaTime, entityManager);
        }
    }

    // Render all systems
    void renderSystems(VulkanRenderer& renderer, EntityManager& entityManager) {
        for (auto& system : systems) {
            system->render(renderer, entityManager);
        }
    }

    // Shutdown all systems
    void shutdownSystems(EntityManager& entityManager) {
        // Shutdown in reverse order
        for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
            (*it)->shutdown(entityManager);
        }
    }

    // Get system count
    size_t getSystemCount() const {
        return systems.size();
    }

    // Check if system exists
    template<typename T>
    bool hasSystem() const {
        return systemLookup.find(std::type_index(typeid(T))) != systemLookup.end();
    }
};

} // namespace VulkanMon