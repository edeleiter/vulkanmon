#include "World.h"
#include "../systems/CameraSystem.h"
#include "../systems/SpatialSystem.h"
#include "../systems/RenderSystem.h"
#include "../systems/CreatureRenderSystem.h"
#include "../game/CreatureDetectionSystem.h"
#include "../utils/Logger.h"

namespace VulkanMon {

void World::connectSystems() {
    // Get all systems with validation
    auto* cameraSystem = getSystem<CameraSystem>();
    auto* spatialSystem = getSystem<SpatialSystem>();
    auto* renderSystem = getSystem<RenderSystem>();
    auto* creatureRenderSystem = getSystem<CreatureRenderSystem>();
    auto* creatureDetectionSystem = getSystem<CreatureDetectionSystem>();

    // Validate required core systems exist
    if (!cameraSystem) {
        VKMON_ERROR("World::connectSystems: CameraSystem not initialized - cannot connect system dependencies");
        return;
    }
    if (!spatialSystem) {
        VKMON_ERROR("World::connectSystems: SpatialSystem not initialized - cannot connect system dependencies");
        return;
    }

    // Connect system dependencies
    if (renderSystem) {
        renderSystem->setCameraSystem(cameraSystem);
        renderSystem->setSpatialSystem(spatialSystem);
    }

    if (creatureRenderSystem) {
        creatureRenderSystem->setCameraSystem(cameraSystem);
        creatureRenderSystem->setSpatialSystem(spatialSystem);
    }

    if (creatureDetectionSystem) {
        creatureDetectionSystem->setSpatialSystem(spatialSystem);
    }

    VKMON_INFO("World: All system dependencies connected successfully");
}

} // namespace VulkanMon