#pragma once

#include "../../src/core/World.h"
#include "../../src/core/EntityManager.h"
#include "../../src/components/Transform.h"
#include "../../src/components/Renderable.h"
#include "../../src/components/Camera.h"
#include <unordered_map>
#include <memory>

using namespace VulkanMon;

/**
 * Mock World for ECS Inspector testing
 * Provides controlled entity/component environment without Vulkan dependencies
 */
class MockWorld : public World {
private:
    std::unique_ptr<EntityManager> entityManager_;

public:
    MockWorld() : entityManager_(std::make_unique<EntityManager>()) {}

    // Override World methods for testing
    EntityManager& getEntityManager() override {
        return *entityManager_;
    }

    const EntityManager& getEntityManager() const override {
        return *entityManager_;
    }

    // Test helper methods
    EntityID createTestEntity() {
        return entityManager_->createEntity();
    }

    EntityID createTestEntityWithTransform(const glm::vec3& position = glm::vec3(0.0f)) {
        EntityID entity = entityManager_->createEntity();
        Transform transform;
        transform.position = position;
        entityManager_->addComponent(entity, transform);
        return entity;
    }

    EntityID createTestEntityWithRenderable(const std::string& meshPath = "test_cube.obj", int materialId = 0) {
        EntityID entity = entityManager_->createEntity();
        Transform transform;
        Renderable renderable;
        renderable.meshPath = meshPath;
        renderable.materialId = materialId;
        renderable.isVisible = true;
        entityManager_->addComponent(entity, transform);
        entityManager_->addComponent(entity, renderable);
        return entity;
    }

    EntityID createTestCamera() {
        EntityID entity = entityManager_->createEntity();
        Transform transform;
        VulkanMon::Camera camera;
        camera.type = VulkanMon::Camera::Type::PERSPECTIVE;
        camera.fov = 45.0f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 100.0f;
        camera.isActive = true;
        entityManager_->addComponent(entity, transform);
        entityManager_->addComponent(entity, camera);
        return entity;
    }

    void destroyTestEntity(EntityID entity) {
        entityManager_->destroyEntity(entity);
    }

    // Helper methods for validation
    size_t getEntityCount() const {
        // Count entities by checking all component types
        size_t count = entityManager_->getComponentCount<Transform>();
        return count;
    }

    size_t getVisibleEntityCount() const {
        size_t count = 0;
        const auto& renderableEntities = entityManager_->getEntitiesWithComponent<Renderable>();
        for (EntityID entity : renderableEntities) {
            if (entityManager_->hasComponent<Renderable>(entity)) {
                const auto& renderable = entityManager_->getComponent<Renderable>(entity);
                if (renderable.isVisible) {
                    count++;
                }
            }
        }
        return count;
    }

    std::vector<EntityID> getAllEntities() const {
        // Get all entities with Transform (assuming all entities have Transform)
        return entityManager_->getEntitiesWithComponent<Transform>();
    }

    std::vector<EntityID> getEntitiesWithComponent(const std::string& componentName) const {
        if (componentName == "Transform") {
            return entityManager_->getEntitiesWithComponent<Transform>();
        } else if (componentName == "Renderable") {
            return entityManager_->getEntitiesWithComponent<Renderable>();
        } else if (componentName == "Camera") {
            return entityManager_->getEntitiesWithComponent<VulkanMon::Camera>();
        }
        return {};
    }
};