#include "ECSInspector.h"
#include "../utils/Logger.h"
#include "../systems/SpatialSystem.h"
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace VulkanMon::Debug {

ECSInspector::ECSInspector(World* world) : world_(world) {
    if (!world_) {
        VKMON_ERROR("ECSInspector created with null World pointer!");
        return;
    }

    VKMON_INFO("ECS Inspector initialized successfully");
}

void ECSInspector::render(float deltaTime) {
    if (!enabled_ || !world_) return;

    // Update performance data with actual frame time
    updatePerformanceData(deltaTime);

    // Main inspector layout with docking
    if (ImGui::Begin("ECS Inspector", &enabled_)) {
        // Inspector controls
        ImGui::Checkbox("Performance Profiler", &showPerformanceProfiler_);
        ImGui::SameLine();
        ImGui::Checkbox("Component Help", &showComponentHelp_);

        ImGui::Separator();

        // Split layout: Entity list on left, Component inspector on right
        ImGui::BeginChild("EntityListPane", ImVec2(300, 0), true);
        renderEntityList();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("ComponentPane", ImVec2(0, 0), true);
        renderComponentInspector();
        ImGui::EndChild();
    }
    ImGui::End();

    // Performance profiler in separate window
    if (showPerformanceProfiler_) {
        renderPerformanceProfiler();
    }

    // Scene operations panel
    if (ImGui::Begin("Scene Operations", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        renderSceneOperations();
    }
    ImGui::End();
}

void ECSInspector::renderEntityList() {
    ImGui::Text("Entity List");

    // Filter controls
    ImGui::SetNextItemWidth(-50);
    static char filterBuffer[256] = "";
    if (ImGui::InputText("##Filter", filterBuffer, sizeof(filterBuffer))) {
        entityFilter_ = std::string(filterBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        entityFilter_.clear();
    }

    ImGui::Checkbox("Visible Only", &showOnlyVisible_);
    if (showComponentHelp_) {
        renderHelpTooltip("Show only entities with visible Renderable components");
    }

    ImGui::Separator();

    // Entity creation buttons
    ImGui::Text("Create Entity:");
    if (ImGui::Button("Cube")) {
        EntityID newEntity = createEntityFromTemplate(EntityTemplate::CUBE);
        selectedEntity_ = newEntity;
    }
    ImGui::SameLine();
    if (ImGui::Button("Sphere")) {
        EntityID newEntity = createEntityFromTemplate(EntityTemplate::SPHERE);
        selectedEntity_ = newEntity;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pyramid")) {
        EntityID newEntity = createEntityFromTemplate(EntityTemplate::PYRAMID);
        selectedEntity_ = newEntity;
    }

    if (ImGui::Button("Plane")) {
        EntityID newEntity = createEntityFromTemplate(EntityTemplate::PLANE);
        selectedEntity_ = newEntity;
    }
    ImGui::SameLine();
    if (ImGui::Button("Camera")) {
        EntityID newEntity = createEntityFromTemplate(EntityTemplate::CAMERA);
        selectedEntity_ = newEntity;
    }
    ImGui::SameLine();
    if (ImGui::Button("Empty")) {
        EntityID newEntity = createEntityFromTemplate(EntityTemplate::EMPTY);
        selectedEntity_ = newEntity;
    }

    ImGui::Separator();

    // Entity list
    if (ImGui::BeginListBox("##Entities", ImVec2(-1, -1))) {
        auto& entityManager = world_->getEntityManager();
        auto entities = entityManager.getEntitiesWithComponent<Transform>();

        for (EntityID entityId : entities) {
            if (!shouldShowEntity(entityId)) continue;

            std::string label = formatEntityLabel(entityId);
            bool isSelected = (selectedEntity_ == entityId);

            if (ImGui::Selectable(label.c_str(), isSelected)) {
                selectedEntity_ = entityId;
            }

            // Right-click context menu
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup(("EntityContext##" + std::to_string(entityId)).c_str());
            }

            if (ImGui::BeginPopup(("EntityContext##" + std::to_string(entityId)).c_str())) {
                if (ImGui::MenuItem("Select")) {
                    selectedEntity_ = entityId;
                }
                if (ImGui::MenuItem("Duplicate")) {
                    EntityID duplicatedEntity = duplicateEntity(entityId);
                    if (duplicatedEntity != INVALID_ENTITY) {
                        selectedEntity_ = duplicatedEntity;
                        VKMON_INFO("Duplicated entity " + std::to_string(entityId) + " to entity " + std::to_string(duplicatedEntity));
                    } else {
                        VKMON_WARNING("Failed to duplicate entity " + std::to_string(entityId));
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del")) {
                    if (selectedEntity_ == entityId) {
                        deleteSelectedEntity();
                    } else {
                        world_->destroyEntity(entityId);
                        VKMON_INFO("Deleted Entity " + std::to_string(entityId));
                    }
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndListBox();
    }
}

void ECSInspector::renderComponentInspector() {
    if (selectedEntity_ == INVALID_ENTITY) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No entity selected");
        ImGui::TextWrapped("Select an entity from the list to inspect its components.");
        return;
    }

    auto& entityManager = world_->getEntityManager();

    // Verify entity still exists
    if (!entityManager.hasComponent<Transform>(selectedEntity_)) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Selected entity no longer exists");
        selectedEntity_ = INVALID_ENTITY;
        return;
    }

    ImGui::Text("Entity ID: %u", selectedEntity_);
    ImGui::Separator();

    // Transform Component (always present)
    if (entityManager.hasComponent<Transform>(selectedEntity_)) {
        if (ImGui::CollapsingHeader("Transform", transformExpanded_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            auto& transform = entityManager.getComponent<Transform>(selectedEntity_);
            if (renderTransformEditor(transform)) {
                // Transform was modified - changes are immediately visible via dirty flag system
            }
        }
    }

    // Renderable Component
    if (entityManager.hasComponent<Renderable>(selectedEntity_)) {
        if (ImGui::CollapsingHeader("Renderable", renderableExpanded_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            auto& renderable = entityManager.getComponent<Renderable>(selectedEntity_);
            if (renderRenderableEditor(renderable)) {
                // Renderable was modified - changes are immediately visible
            }
        }
    }

    // Camera Component
    if (entityManager.hasComponent<Camera>(selectedEntity_)) {
        if (ImGui::CollapsingHeader("Camera", cameraExpanded_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            auto& camera = entityManager.getComponent<Camera>(selectedEntity_);
            if (renderCameraEditor(camera)) {
                // Camera was modified
            }
        }
    }

    // SpatialComponent - spatial properties and optimization
    if (entityManager.hasComponent<SpatialComponent>(selectedEntity_)) {
        if (ImGui::CollapsingHeader("Spatial Component", spatialExpanded_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            auto& spatial = entityManager.getComponent<SpatialComponent>(selectedEntity_);
            if (renderSpatialEditor(spatial)) {
                // Spatial component was modified - mark for spatial system update
                spatial.markDirty();
            }
        }
    }

    ImGui::Separator();
    renderComponentAddition();
}

bool ECSInspector::renderTransformEditor(Transform& transform) {
    bool modified = false;

    // Position editing with immediate feedback
    glm::vec3 position = transform.position;
    if (ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1f, -100.0f, 100.0f)) {
        transform.setPosition(position);
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Entity position in world space. Changes are immediately visible.");
    }

    // Rotation editing (Euler angles for UI simplicity)
    glm::vec3 eulerDegrees = glm::degrees(glm::eulerAngles(transform.rotation));
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerDegrees), 1.0f, -180.0f, 180.0f)) {
        transform.setRotationEuler(glm::radians(eulerDegrees.x), glm::radians(eulerDegrees.y), glm::radians(eulerDegrees.z));
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Entity rotation in degrees (X=Pitch, Y=Yaw, Z=Roll)");
    }

    // Scale editing
    glm::vec3 scale = transform.scale;
    if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1f, 0.1f, 10.0f)) {
        transform.setScale(scale);
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Entity scale factors. 1.0 = original size, 2.0 = double size");
    }

    // Quick preset buttons
    if (ImGui::Button("Reset##Transform")) {
        transform.setPosition(glm::vec3(0.0f));
        transform.setRotation(glm::quat(1, 0, 0, 0));
        transform.setScale(glm::vec3(1.0f));
        modified = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Identity##Transform")) {
        transform.setPosition(glm::vec3(0.0f));
        transform.setRotation(glm::quat(1, 0, 0, 0));
        transform.setScale(glm::vec3(1.0f));
        modified = true;
    }

    return modified;
}

bool ECSInspector::renderRenderableEditor(Renderable& renderable) {
    bool modified = false;

    // Mesh path selector with dropdown
    if (ImGui::BeginCombo("Mesh", renderable.meshPath.c_str())) {
        for (const auto& mesh : availableMeshes_) {
            bool selected = (renderable.meshPath == mesh);
            if (ImGui::Selectable(mesh.c_str(), selected)) {
                renderable.meshPath = mesh;
                modified = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("3D model file to render. Changes take effect immediately.");
    }

    // Material ID with preset names (bounds-checked)
    int currentMaterial = static_cast<int>(renderable.materialId);
    if (ImGui::Combo("Material", &currentMaterial, materialNames_, MATERIAL_COUNT)) {
        // Ensure the selected material is within valid bounds
        if (currentMaterial >= 0 && currentMaterial < static_cast<int>(MATERIAL_COUNT)) {
            renderable.materialId = static_cast<uint32_t>(currentMaterial);
            modified = true;
        }
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Material preset affecting color and lighting response");
    }

    // Visibility and rendering flags
    if (ImGui::Checkbox("Visible", &renderable.isVisible)) {
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Whether this entity is rendered. Invisible entities are culled.");
    }

    if (ImGui::Checkbox("Casts Shadow", &renderable.castsShadow)) {
        modified = true;
    }
    if (ImGui::Checkbox("Receives Shadow", &renderable.receivesShadow)) {
        modified = true;
    }

    // LOD settings
    if (ImGui::DragFloat("LOD Distance", &renderable.lodDistance, 1.0f, 1.0f, 500.0f)) {
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Distance at which Level of Detail switching occurs");
    }

    int renderLayer = static_cast<int>(renderable.renderLayer);
    if (ImGui::DragInt("Render Layer", &renderLayer, 1, 0, 10)) {
        renderable.renderLayer = static_cast<uint32_t>(renderLayer);
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Rendering layer for sorting. Lower numbers render first.");
    }

    return modified;
}

bool ECSInspector::renderCameraEditor(Camera& camera) {
    bool modified = false;

    // Camera type selection
    const char* cameraTypes[] = { "Perspective", "Orthographic" };
    int currentType = static_cast<int>(camera.type);
    if (ImGui::Combo("Type", &currentType, cameraTypes, 2)) {
        camera.type = static_cast<Camera::Type>(currentType);
        camera.updateProjectionMatrix();
        modified = true;
    }

    // Common properties
    float fov = camera.fov;
    if (ImGui::DragFloat("FOV", &fov, 1.0f, 10.0f, 179.0f)) {
        camera.fov = fov;
        camera.updateProjectionMatrix();
        modified = true;
    }

    float nearPlane = camera.nearPlane;
    float farPlane = camera.farPlane;
    if (ImGui::DragFloat("Near Plane", &nearPlane, 0.01f, 0.01f, 10.0f)) {
        camera.nearPlane = nearPlane;
        camera.updateProjectionMatrix();
        modified = true;
    }
    if (ImGui::DragFloat("Far Plane", &farPlane, 1.0f, nearPlane + 1.0f, 1000.0f)) {
        camera.farPlane = farPlane;
        camera.updateProjectionMatrix();
        modified = true;
    }

    // Camera state
    bool isActive = camera.isActive;
    if (ImGui::Checkbox("Active", &isActive)) {
        camera.isActive = isActive;
        modified = true;
    }

    int priority = camera.priority;
    if (ImGui::DragInt("Priority", &priority, 1, 0, 100)) {
        camera.priority = priority;
        modified = true;
    }

    return modified;
}

bool ECSInspector::renderSpatialEditor(SpatialComponent& spatial) {
    bool modified = false;

    // Bounding radius with live preview
    float boundingRadius = spatial.boundingRadius;
    if (ImGui::SliderFloat("Bounding Radius", &boundingRadius, 0.1f, 50.0f)) {
        spatial.setBoundingRadius(boundingRadius);
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Radius for collision detection and spatial queries. Larger = more inclusive spatial queries.");
    }

    // Detection radius for Pokemon encounters
    float detectionRadius = spatial.detectionRadius;
    if (ImGui::SliderFloat("Detection Radius", &detectionRadius, 1.0f, 100.0f)) {
        spatial.detectionRadius = detectionRadius;
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("How far this entity can detect other entities. Used for entity interaction and AI systems.");
    }

    // Territory radius for entity behavior
    float territoryRadius = spatial.territoryRadius;
    if (ImGui::SliderFloat("Territory Radius", &territoryRadius, 5.0f, 200.0f)) {
        spatial.territoryRadius = territoryRadius;
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Territory size for entities with area-based behavior. Defines movement bounds and interaction area.");
    }

    // Spatial behavior selection
    const char* behaviors[] = { "Static", "Dynamic", "Occasional" };
    int currentBehavior = static_cast<int>(spatial.behavior);
    if (ImGui::Combo("Behavior", &currentBehavior, behaviors, 3)) {
        spatial.behavior = static_cast<SpatialBehavior>(currentBehavior);
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Static: Never moves (buildings, trees). Dynamic: Moves frequently (creatures). Occasional: Moves rarely (NPCs).");
    }

    // Home position editor
    glm::vec3 homePosition = spatial.homePosition;
    if (ImGui::DragFloat3("Home Position", &homePosition.x, 0.1f)) {
        spatial.setHomePosition(homePosition);
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Original spawn position. Used for territorial behavior and returning home.");
    }

    // Performance info (read-only)
    ImGui::Separator();
    ImGui::Text("Performance Info:");
    ImGui::Text("Query Count: %u", spatial.spatialQueryCount);
    ImGui::Text("Time Since Last Query: %.2fs", spatial.timeSinceLastQuery);

    // Query throttling control
    ImGui::Separator();
    ImGui::Text("Query Throttling:");
    bool needsUpdate = spatial.needsSpatialUpdate;
    if (ImGui::Checkbox("Needs Spatial Update", &needsUpdate)) {
        spatial.needsSpatialUpdate = needsUpdate;
        modified = true;
    }
    if (showComponentHelp_ && ImGui::IsItemHovered()) {
        renderHelpTooltip("Forces spatial system update on next frame. Automatically set when entity moves.");
    }

    // Distance checks
    ImGui::Separator();
    ImGui::Text("Spatial Status:");
    // Note: This would require current position to calculate, could be added as enhancement
    ImGui::Text("Is Visible: %s", spatial.isVisible ? "Yes" : "No");
    if (spatial.lastCullingDistance > 0.0f) {
        ImGui::Text("Last Culling Distance: %.2f", spatial.lastCullingDistance);
    }

    return modified;
}

EntityID ECSInspector::createEntityFromTemplate(EntityTemplate templateType, const glm::vec3& position) {
    EntityID newEntity = world_->createEntity();

    // All entities get Transform component
    Transform transform;
    transform.setPosition(position);
    world_->addComponent<Transform>(newEntity, transform);

    switch (templateType) {
        case EntityTemplate::CUBE:
            {
                Renderable renderable;
                renderable.meshPath = "cube.obj";
                renderable.materialId = 0; // Default material
                world_->addComponent<Renderable>(newEntity, renderable);
                break;
            }
        case EntityTemplate::SPHERE:
            {
                Renderable renderable;
                renderable.meshPath = "sphere.obj";
                renderable.materialId = 1; // Gold material
                world_->addComponent<Renderable>(newEntity, renderable);
                break;
            }
        case EntityTemplate::PYRAMID:
            {
                Renderable renderable;
                renderable.meshPath = "pyramid.obj";
                renderable.materialId = 2; // Ruby material
                world_->addComponent<Renderable>(newEntity, renderable);
                break;
            }
        case EntityTemplate::PLANE:
            {
                Renderable renderable;
                renderable.meshPath = "plane.obj";
                renderable.materialId = 4; // Emerald material
                world_->addComponent<Renderable>(newEntity, renderable);
                break;
            }
        case EntityTemplate::CAMERA:
            {
                Camera camera;
                camera.setPerspective(45.0f, 16.0f/9.0f, 0.1f, 100.0f);
                camera.isActive = false; // Don't immediately override main camera
                world_->addComponent<Camera>(newEntity, camera);
                break;
            }
        case EntityTemplate::EMPTY:
            // Only Transform component
            break;
    }

    VKMON_INFO("Created entity " + std::to_string(newEntity) + " from template");
    return newEntity;
}

void ECSInspector::deleteSelectedEntity() {
    if (selectedEntity_ == INVALID_ENTITY) return;

    world_->destroyEntity(selectedEntity_);
    VKMON_INFO("Deleted Entity " + std::to_string(selectedEntity_));
    selectedEntity_ = INVALID_ENTITY;
}

EntityID ECSInspector::duplicateEntity(EntityID sourceEntity) {
    if (sourceEntity == INVALID_ENTITY) {
        VKMON_WARNING("Cannot duplicate invalid entity");
        return INVALID_ENTITY;
    }

    auto& entityManager = world_->getEntityManager();

    // Create new entity
    EntityID newEntity = world_->createEntity();

    // Duplicate Transform component if it exists
    if (entityManager.hasComponent<Transform>(sourceEntity)) {
        Transform sourceTransform = entityManager.getComponent<Transform>(sourceEntity);
        // Offset position slightly to avoid exact overlap
        sourceTransform.setPosition(sourceTransform.position + glm::vec3(1.0f, 0.0f, 0.0f));
        world_->addComponent<Transform>(newEntity, sourceTransform);
    }

    // Duplicate Renderable component if it exists
    if (entityManager.hasComponent<Renderable>(sourceEntity)) {
        Renderable sourceRenderable = entityManager.getComponent<Renderable>(sourceEntity);
        world_->addComponent<Renderable>(newEntity, sourceRenderable);
    }

    // Duplicate Camera component if it exists
    if (entityManager.hasComponent<Camera>(sourceEntity)) {
        Camera sourceCamera = entityManager.getComponent<Camera>(sourceEntity);
        // Set duplicated camera as inactive to avoid conflicts
        sourceCamera.isActive = false;
        sourceCamera.priority = sourceCamera.priority - 1; // Lower priority
        world_->addComponent<Camera>(newEntity, sourceCamera);
    }

    return newEntity;
}

void ECSInspector::renderComponentAddition() {
    ImGui::Text("Add Component:");

    auto& entityManager = world_->getEntityManager();

    // Renderable component
    if (!entityManager.hasComponent<Renderable>(selectedEntity_)) {
        if (ImGui::Button("Add Renderable")) {
            Renderable renderable;
            renderable.meshPath = "cube.obj";
            renderable.materialId = 0;
            world_->addComponent<Renderable>(selectedEntity_, renderable);
            VKMON_INFO("Added Renderable component to Entity " + std::to_string(selectedEntity_));
        }
        ImGui::SameLine();
    }

    // Camera component
    if (!entityManager.hasComponent<Camera>(selectedEntity_)) {
        if (ImGui::Button("Add Camera")) {
            Camera camera;
            camera.setPerspective(45.0f, 16.0f/9.0f, 0.1f, 100.0f);
            camera.isActive = false;
            world_->addComponent<Camera>(selectedEntity_, camera);
            VKMON_INFO("Added Camera component to Entity " + std::to_string(selectedEntity_));
        }
        ImGui::SameLine();
    }

    // SpatialComponent
    if (!entityManager.hasComponent<SpatialComponent>(selectedEntity_)) {
        if (ImGui::Button("Add Spatial")) {
            SpatialComponent spatial;
            spatial.boundingRadius = 1.0f;
            spatial.behavior = SpatialBehavior::DYNAMIC;
            spatial.needsSpatialUpdate = true;
            world_->addComponent<SpatialComponent>(selectedEntity_, spatial);
            VKMON_INFO("Added SpatialComponent to Entity " + std::to_string(selectedEntity_));
        }
    }
}

void ECSInspector::renderPerformanceProfiler() {
    if (ImGui::Begin("Performance Profiler", &showPerformanceProfiler_)) {
        ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", performanceData_.frameTime, 1000.0f / performanceData_.frameTime);

        ImGui::Separator();

        // Entity statistics
        ImGui::Text("Entity Statistics:");
        ImGui::Indent();
        ImGui::Text("Total Entities: %zu", performanceData_.totalEntities);
        ImGui::Text("Visible Entities: %zu", performanceData_.visibleEntities);
        ImGui::Text("Culled Entities: %zu", performanceData_.culledEntities);
        ImGui::Unindent();

        ImGui::Separator();

        // System performance
        if (ImGui::BeginTable("Systems", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("System");
            ImGui::TableSetupColumn("Update (ms)");
            ImGui::TableSetupColumn("Render (ms)");
            ImGui::TableSetupColumn("Total (ms)");
            ImGui::TableHeadersRow();

            // Display all systems dynamically
            auto& systemManager = world_->getSystemManager();
            const auto& perfData = systemManager.getPerformanceData();

            for (const auto& [typeIndex, systemPerfData] : perfData) {
                std::string systemName = systemPerfData.name;

                // Clean up the system name (remove namespace and class decorations)
                size_t classPos = systemName.find("class ");
                if (classPos != std::string::npos) {
                    systemName = systemName.substr(classPos + 6);
                }
                size_t namespacePos = systemName.find("VulkanMon::");
                if (namespacePos != std::string::npos) {
                    systemName = systemName.substr(namespacePos + 11);
                }

                float totalTime = systemPerfData.updateTime + systemPerfData.renderTime;

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", systemName.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.3f", systemPerfData.updateTime);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", systemPerfData.renderTime);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", totalTime);
            }

            ImGui::EndTable();
        }

        // Spatial System Performance (if available)
        if (world_->hasSystem<SpatialSystem>()) {
            ImGui::Separator();
            ImGui::Text("Spatial System Debug:");
            ImGui::Indent();

            auto* spatialSystem = world_->getSystem<SpatialSystem>();
            if (spatialSystem) {
                // Get spatial statistics
                int nodeCount = 0, maxDepth = 0, totalEntities = 0;
                spatialSystem->getSpatialStatistics(nodeCount, maxDepth, totalEntities);

                ImGui::Text("Octree Nodes: %d", nodeCount);
                ImGui::Text("Max Depth: %d", maxDepth);
                ImGui::Text("Spatial Entities: %d", totalEntities);

                // Get performance stats
                const auto& spatialStats = spatialSystem->getSpatialPerformanceStats();
                ImGui::Text("Spatial Queries: %zu", spatialStats.totalQueries);
                ImGui::Text("Avg Query Time: %.3f ms", spatialStats.averageQueryTimeMs);
                ImGui::Text("Last Query Time: %.3f ms", spatialStats.lastQueryTimeMs);
                ImGui::Text("Entities Returned: %zu", spatialStats.totalEntitiesReturned);

                // Frame stats
                const auto& frameStats = spatialSystem->getFrameStats();
                ImGui::Text("Frame Entities Tracked: %zu", frameStats.entitiesTracked);
                ImGui::Text("Frame Entities Updated: %zu", frameStats.entitiesUpdated);

                // World bounds
                const auto& worldBounds = spatialSystem->getWorldBounds();
                ImGui::Text("World Bounds: (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)",
                           worldBounds.min.x, worldBounds.min.y, worldBounds.min.z,
                           worldBounds.max.x, worldBounds.max.y, worldBounds.max.z);
            }
            ImGui::Unindent();
        }

        // Memory usage estimate
        ImGui::Separator();
        ImGui::Text("Memory Usage (Estimated):");
        ImGui::Indent();
        size_t componentMemory = performanceData_.totalEntities * (sizeof(Transform) + sizeof(Renderable));
        ImGui::Text("Components: %.2f KB", componentMemory / 1024.0f);
        ImGui::Unindent();
    }
    ImGui::End();
}

void ECSInspector::renderSceneOperations() {
    ImGui::Text("Scene Operations");
    ImGui::Separator();

    // Bulk operations
    if (ImGui::Button("Select All Visible")) {
        // Find first visible entity
        auto& entityManager = world_->getEntityManager();
        auto entities = entityManager.getEntitiesWithComponent<Transform>();

        for (EntityID entityId : entities) {
            if (entityManager.hasComponent<Renderable>(entityId)) {
                auto& renderable = entityManager.getComponent<Renderable>(entityId);
                if (renderable.isVisible) {
                    selectedEntity_ = entityId;
                    break;
                }
            }
        }
    }

    if (ImGui::Button("Hide All")) {
        auto& entityManager = world_->getEntityManager();
        auto entities = entityManager.getEntitiesWithComponent<Renderable>();

        for (EntityID entityId : entities) {
            auto& renderable = entityManager.getComponent<Renderable>(entityId);
            renderable.isVisible = false;
        }
        VKMON_INFO("Hidden all renderable entities");
    }

    ImGui::SameLine();
    if (ImGui::Button("Show All")) {
        auto& entityManager = world_->getEntityManager();
        auto entities = entityManager.getEntitiesWithComponent<Renderable>();

        for (EntityID entityId : entities) {
            auto& renderable = entityManager.getComponent<Renderable>(entityId);
            renderable.isVisible = true;
        }
        VKMON_INFO("Showed all renderable entities");
    }

    ImGui::Separator();

    // Statistics
    auto& entityManager = world_->getEntityManager();
    size_t totalEntities = entityManager.getEntitiesWithComponent<Transform>().size();
    size_t renderableEntities = entityManager.getEntitiesWithComponent<Renderable>().size();
    size_t cameraEntities = entityManager.getEntitiesWithComponent<Camera>().size();

    ImGui::Text("Scene Statistics:");
    ImGui::Indent();
    ImGui::Text("Total Entities: %zu", totalEntities);
    ImGui::Text("With Renderable: %zu", renderableEntities);
    ImGui::Text("With Camera: %zu", cameraEntities);
    ImGui::Unindent();
}

bool ECSInspector::shouldShowEntity(EntityID entityId) const {
    auto& entityManager = world_->getEntityManager();

    // Apply visibility filter
    if (showOnlyVisible_ && entityManager.hasComponent<Renderable>(entityId)) {
        auto& renderable = entityManager.getComponent<Renderable>(entityId);
        if (!renderable.isVisible) return false;
    }

    // Apply text filter
    if (!entityFilter_.empty()) {
        std::string entityLabel = formatEntityLabel(entityId);
        std::string lowerFilter = entityFilter_;
        std::string lowerLabel = entityLabel;

        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
        std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(), ::tolower);

        if (lowerLabel.find(lowerFilter) == std::string::npos) {
            return false;
        }
    }

    return true;
}

std::string ECSInspector::formatEntityLabel(EntityID entityId) const {
    std::stringstream ss;
    ss << "Entity " << entityId;

    auto& entityManager = world_->getEntityManager();

    // Add component indicators
    std::vector<std::string> components;
    if (entityManager.hasComponent<Transform>(entityId)) components.push_back("T");
    if (entityManager.hasComponent<Renderable>(entityId)) components.push_back("R");
    if (entityManager.hasComponent<Camera>(entityId)) components.push_back("C");

    if (!components.empty()) {
        ss << " [";
        for (size_t i = 0; i < components.size(); ++i) {
            if (i > 0) ss << ",";
            ss << components[i];
        }
        ss << "]";
    }

    // Add mesh name if available
    if (entityManager.hasComponent<Renderable>(entityId)) {
        auto& renderable = entityManager.getComponent<Renderable>(entityId);
        std::string meshName = renderable.meshPath;
        if (!meshName.empty()) {
            // Extract filename without extension
            size_t lastSlash = meshName.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                meshName = meshName.substr(lastSlash + 1);
            }
            size_t lastDot = meshName.find_last_of('.');
            if (lastDot != std::string::npos) {
                meshName = meshName.substr(0, lastDot);
            }
            ss << " (" << meshName << ")";
        }
    }

    return ss.str();
}

void ECSInspector::updatePerformanceData(float deltaTime) {
    performanceData_.frameTime = deltaTime;

    if (!world_) return;

    auto& entityManager = world_->getEntityManager();
    performanceData_.totalEntities = entityManager.getEntitiesWithComponent<Transform>().size();

    // Count visible entities
    performanceData_.visibleEntities = 0;
    auto renderableEntities = entityManager.getEntitiesWithComponent<Renderable>();
    for (EntityID entityId : renderableEntities) {
        auto& renderable = entityManager.getComponent<Renderable>(entityId);
        if (renderable.isVisible) {
            performanceData_.visibleEntities++;
        }
    }

    performanceData_.culledEntities = renderableEntities.size() - performanceData_.visibleEntities;

    // Get actual system timing data
    auto& systemManager = world_->getSystemManager();
    const auto& perfData = systemManager.getPerformanceData();

    performanceData_.renderSystemTime = 0.0f;
    performanceData_.cameraSystemTime = 0.0f;

    // Aggregate timing data from all systems
    for (const auto& [typeIndex, systemPerfData] : perfData) {
        std::string systemName = systemPerfData.name;
        float totalTime = systemPerfData.updateTime + systemPerfData.renderTime;

        // Classify systems by name (crude but functional)
        if (systemName.find("Render") != std::string::npos || systemName.find("render") != std::string::npos) {
            performanceData_.renderSystemTime += totalTime;
        } else if (systemName.find("Camera") != std::string::npos || systemName.find("camera") != std::string::npos) {
            performanceData_.cameraSystemTime += totalTime;
        }
    }
}

void ECSInspector::renderHelpTooltip(const char* text) const {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

} // namespace VulkanMon::Debug