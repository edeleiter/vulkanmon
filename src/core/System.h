#pragma once

#include "Entity.h"
#include <tuple>
#include <functional>

namespace VulkanMon {

// Forward declarations
class EntityManager;
class VulkanRenderer;

// Base class for all systems
class SystemBase {
public:
    virtual ~SystemBase() = default;

    // Called every frame to update system logic
    virtual void update(float deltaTime, EntityManager& entityManager) = 0;

    // Called during render phase (optional - not all systems need to render)
    virtual void render(VulkanRenderer& renderer, EntityManager& entityManager) {}

    // Called when system is first added to world (optional)
    virtual void initialize(EntityManager& entityManager) {}

    // Called when system is removed from world (optional)
    virtual void shutdown(EntityManager& entityManager) {}
};

// System template is in SystemImpl.h to avoid circular dependencies

} // namespace VulkanMon