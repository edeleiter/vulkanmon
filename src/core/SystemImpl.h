#pragma once

#include "System.h"
#include "EntityManager.h"
#include <tuple>
#include <functional>

namespace VulkanMon {

// Helper template for systems that work with specific component types
template<typename... ComponentTypes>
class System : public SystemBase {
protected:
    // Helper to check if entity has all required components
    bool hasRequiredComponents(EntityID entity, EntityManager& entityManager) {
        return (entityManager.hasComponent<ComponentTypes>(entity) && ...);
    }

    // Helper to get components (returns tuple of references)
    std::tuple<ComponentTypes&...> getComponents(EntityID entity, EntityManager& entityManager) {
        return std::make_tuple(std::ref(entityManager.getComponent<ComponentTypes>(entity))...);
    }
};

} // namespace VulkanMon