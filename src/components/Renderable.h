#pragma once

#include "../core/Entity.h"
#include <string>
#include <cstdint>

namespace VulkanMon {

struct Renderable {
    VKMON_COMPONENT(Renderable)

    // Asset references (will integrate with AssetManager)
    std::string meshPath;
    std::string texturePath;

    // Material properties (will integrate with MaterialSystem)
    uint32_t materialId = 0;

    // Rendering flags
    bool isVisible = true;
    bool castsShadow = true;
    bool receivesShadow = true;

    // Level of detail
    float lodDistance = 500.0f;  // Distance at which to switch LOD
    uint32_t currentLod = 0;     // Current LOD level (0 = highest quality)

    // Rendering layer (for sorting/culling)
    uint32_t renderLayer = 0;    // 0 = default layer

    // Constructor for easy creation
    Renderable() = default;

    Renderable(const std::string& mesh, const std::string& texture = "", uint32_t material = 0)
        : meshPath(mesh), texturePath(texture), materialId(material) {}

    // Helper methods
    void setVisible(bool visible) { isVisible = visible; }
    void setMaterial(uint32_t material) { materialId = material; }
    void setRenderLayer(uint32_t layer) { renderLayer = layer; }

    // Check if should be rendered based on distance
    bool shouldRender(float distanceToCamera) const {
        return isVisible && distanceToCamera <= lodDistance;
    }
};

} // namespace VulkanMon