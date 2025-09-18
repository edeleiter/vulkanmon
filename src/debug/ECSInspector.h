#pragma once

#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"
#include "../components/Camera.h"
#include "../components/SpatialComponent.h"
#include <imgui.h>
#include <string>
#include <vector>

/**
 * VulkanMon ECS Inspector - Real-time Entity Component Debug Interface
 *
 * Professional Unity/Unreal-style inspector for live ECS visualization and editing.
 * Provides immediate visual feedback for all component changes.
 *
 * Features:
 * - Real-time entity list with selection
 * - Live Transform editing (position/rotation/scale)
 * - Material/mesh switching with asset browser
 * - Entity creation/deletion with templates
 * - System performance profiler
 * - Component add/remove interface
 *
 * Design Philosophy:
 * - "Simple is Powerful": Intuitive interface focused on core workflows
 * - "Test, Test, Test": Immediate visual feedback validates changes
 * - "Document Often": Self-documenting UI with tooltips and help
 */

namespace VulkanMon::Debug {

class ECSInspector {
public:
    /**
     * Create ECS Inspector with World reference
     * @param world ECS World instance for entity/component access
     */
    explicit ECSInspector(World* world);

    /**
     * Render complete ECS inspector interface
     * Call once per frame in ImGui context
     *
     * @param deltaTime Frame time in milliseconds for performance tracking
     */
    void render(float deltaTime = 0.0f);

    /**
     * Check if inspector is enabled
     * @return true if inspector should render
     */
    bool isEnabled() const { return enabled_; }

    /**
     * Enable/disable inspector rendering
     * @param enabled true to show inspector
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /**
     * Get currently selected entity
     * @return Selected entity ID or INVALID_ENTITY if none selected
     */
    EntityID getSelectedEntity() const { return selectedEntity_; }

    /**
     * Set selected entity (programmatic selection)
     * @param entityId Entity to select or INVALID_ENTITY to clear
     */
    void setSelectedEntity(EntityID entityId) { selectedEntity_ = entityId; }

private:
    // Core inspector state
    World* world_;
    bool enabled_ = true;
    EntityID selectedEntity_ = INVALID_ENTITY;

    // UI state
    std::string entityFilter_;
    bool showOnlyVisible_ = false;
    bool showPerformanceProfiler_ = true;
    bool showComponentHelp_ = false;

    // Entity creation templates
    enum class EntityTemplate {
        CUBE,
        SPHERE,
        PYRAMID,
        PLANE,
        CAMERA,
        EMPTY
    };

    // Component editing state
    bool transformExpanded_ = true;
    bool renderableExpanded_ = true;
    bool cameraExpanded_ = true;
    bool spatialExpanded_ = true;

    // Performance tracking
    struct PerformanceData {
        float frameTime = 0.0f;
        size_t totalEntities = 0;
        size_t visibleEntities = 0;
        size_t culledEntities = 0;
        float renderSystemTime = 0.0f;
        float cameraSystemTime = 0.0f;
    } performanceData_;

    // Material and mesh asset lists
    std::vector<std::string> availableMeshes_ = {
        "cube.obj", "sphere.obj", "pyramid.obj", "plane.obj"
    };

    // Material names for ImGui combo (fixed size for safe access)
    static constexpr size_t MATERIAL_COUNT = 5;
    const char* materialNames_[MATERIAL_COUNT] = {
        "Default", "Gold", "Ruby", "Chrome", "Emerald"
    };

    // =================================================================
    // Main inspector panels
    // =================================================================

    /**
     * Render entity list panel with filtering and selection
     */
    void renderEntityList();

    /**
     * Render component inspector panel for selected entity
     */
    void renderComponentInspector();

    /**
     * Render system performance profiler panel
     */
    void renderPerformanceProfiler();

    /**
     * Render entity creation and scene operations panel
     */
    void renderSceneOperations();

    // =================================================================
    // Component-specific editors
    // =================================================================

    /**
     * Render Transform component editor with live feedback
     * @param transform Transform component to edit
     * @return true if component was modified
     */
    bool renderTransformEditor(Transform& transform);

    /**
     * Render Renderable component editor with asset browser
     * @param renderable Renderable component to edit
     * @return true if component was modified
     */
    bool renderRenderableEditor(Renderable& renderable);

    /**
     * Render Camera component editor with projection settings
     * @param camera Camera component to edit
     * @return true if component was modified
     */
    bool renderCameraEditor(Camera& camera);

    /**
     * Render SpatialComponent editor with spatial properties
     * @param spatial SpatialComponent to edit
     * @return true if component was modified
     */
    bool renderSpatialEditor(SpatialComponent& spatial);

    // =================================================================
    // Entity operations and templates
    // =================================================================

    /**
     * Create entity from template
     * @param templateType Type of entity to create
     * @param position World position for new entity
     * @return Created entity ID
     */
    EntityID createEntityFromTemplate(EntityTemplate templateType, const glm::vec3& position = glm::vec3(0.0f));

    /**
     * Delete selected entity with confirmation
     */
    void deleteSelectedEntity();

    /**
     * Duplicate entity with all its components
     * @param sourceEntity Entity to duplicate
     * @return Created entity ID
     */
    EntityID duplicateEntity(EntityID sourceEntity);

    /**
     * Render component addition interface for selected entity
     */
    void renderComponentAddition();

    // =================================================================
    // Helper utilities
    // =================================================================

    /**
     * Check if entity should be shown based on filters
     * @param entityId Entity to check
     * @return true if entity passes filters
     */
    bool shouldShowEntity(EntityID entityId) const;

    /**
     * Format entity label for display in lists
     * @param entityId Entity to format
     * @return Display string for entity
     */
    std::string formatEntityLabel(EntityID entityId) const;

    /**
     * Update performance data from systems
     * @param deltaTime Frame delta time in milliseconds
     */
    void updatePerformanceData(float deltaTime);

    /**
     * Render help tooltip for UI element
     * @param text Help text to display
     */
    void renderHelpTooltip(const char* text) const;

    /**
     * Apply syntax highlighting to numeric input fields
     * @param label Field label
     * @param value Current value reference
     * @param speed Drag speed
     * @param min Minimum value
     * @param max Maximum value
     * @return true if value was modified
     */
    template<typename T>
    bool renderHighlightedDragFloat(const char* label, T& value, float speed = 0.1f, float min = -100.0f, float max = 100.0f);
};

} // namespace VulkanMon::Debug