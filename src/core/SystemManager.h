#pragma once

#include "System.h"
#include "EntityManager.h"
#include "../rendering/VulkanRenderer.h"
#include <memory>
#include <vector>
#include <typeindex>
#include <unordered_map>
#include <chrono>
#include <string>

namespace VulkanMon {

// System performance data
struct SystemPerformanceData {
    std::string name;
    float updateTime = 0.0f; // in milliseconds
    float renderTime = 0.0f; // in milliseconds
    bool isActive = true;
};

class SystemManager {
private:
    std::vector<std::unique_ptr<SystemBase>> systems;
    std::unordered_map<std::type_index, SystemBase*> systemLookup;
    std::unordered_map<std::type_index, SystemPerformanceData> performanceData;

public:
    // Add a system to the manager
    template<typename T, typename... Args>
    T* addSystem(Args&&... args) {
        static_assert(std::is_base_of_v<SystemBase, T>, "T must inherit from SystemBase");

        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* rawPtr = system.get();

        systems.push_back(std::move(system));
        auto typeIndex = std::type_index(typeid(T));
        systemLookup[typeIndex] = rawPtr;

        // Initialize performance data
        SystemPerformanceData perfData;
        perfData.name = typeid(T).name();
        performanceData[typeIndex] = perfData;

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
            auto start = std::chrono::high_resolution_clock::now();
            system->update(deltaTime, entityManager);
            auto end = std::chrono::high_resolution_clock::now();

            // Update performance data
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            float timeMs = duration.count() / 1000.0f; // Convert to milliseconds

            // Find the system type and update performance data
            for (auto& [typeIndex, systemPtr] : systemLookup) {
                if (systemPtr == system.get()) {
                    performanceData[typeIndex].updateTime = timeMs;
                    break;
                }
            }
        }
    }

    // Render all systems
    void renderSystems(VulkanRenderer& renderer, EntityManager& entityManager) {
        for (auto& system : systems) {
            auto start = std::chrono::high_resolution_clock::now();
            system->render(renderer, entityManager);
            auto end = std::chrono::high_resolution_clock::now();

            // Update performance data
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            float timeMs = duration.count() / 1000.0f; // Convert to milliseconds

            // Find the system type and update performance data
            for (auto& [typeIndex, systemPtr] : systemLookup) {
                if (systemPtr == system.get()) {
                    performanceData[typeIndex].renderTime = timeMs;
                    break;
                }
            }
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

    // Get performance data for all systems
    const std::unordered_map<std::type_index, SystemPerformanceData>& getPerformanceData() const {
        return performanceData;
    }

    // Get performance data for specific system
    template<typename T>
    const SystemPerformanceData* getSystemPerformanceData() const {
        auto it = performanceData.find(std::type_index(typeid(T)));
        return (it != performanceData.end()) ? &it->second : nullptr;
    }
};

} // namespace VulkanMon