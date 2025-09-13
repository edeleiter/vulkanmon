# 🧪 Vulkanmon ECS Inspector – TODO List

This document outlines the steps to implement a minimal ImGui-based ECS inspector for the Vulkanmon engine. The goal is to visualize entities and components in real time, with basic editing capabilities.

---

## ✅ Setup & Integration

- [ ] Integrate ImGui into Vulkanmon's rendering loop
  - [ ] Hook ImGui frame lifecycle into your main loop (`ImGui::NewFrame()`, `ImGui::Render()`)
  - [ ] Ensure Vulkan render pass supports ImGui draw calls
  - [ ] Add font and style configuration (optional)

- [ ] Create a dedicated `ECSInspectorSystem` or `DebugUIManager`
  - [ ] Register it as a system in your ECS tick loop
  - [ ] Ensure it runs after all logic systems but before rendering

---

## 🧠 Entity Visualization

- [ ] Query all active entities from `EntityManager`
- [ ] Display entity list in ImGui (`ImGui::Begin("ECS Inspector")`)
  - [ ] Show entity ID or name (if available)
  - [ ] Allow selection of an entity

- [ ] Add filtering options
  - [ ] Filter by component type
  - [ ] Search by entity ID or tag

---

## 🧩 Component Inspection

- [ ] For selected entity, list all attached components
  - [ ] Use your component registry to enumerate types
  - [ ] Display component name and data

- [ ] Render component fields using ImGui widgets
  - [ ] `TransformComponent`: sliders for position, rotation, scale
  - [ ] `VelocityComponent`: drag floats
  - [ ] `HealthComponent`: progress bar or numeric input

- [ ] Allow live editing of component values
  - [ ] Update ECS storage on widget interaction
  - [ ] Mark dirty components for system updates (if needed)

---

## 🧰 System Controls (Optional)

- [ ] List all registered systems
  - [ ] Show system name and tick status
  - [ ] Toggle system on/off via checkbox

- [ ] Display system execution time (profiling)
  - [ ] Use high-resolution timer around each system tick
  - [ ] Show results in ImGui table

---

## 🧪 Debugging & UX Polish

- [ ] Highlight entities with missing or invalid components
- [ ] Add collapsible sections per component type
- [ ] Support multi-entity selection (optional)
- [ ] Add "Spawn Entity" and "Delete Entity" buttons
- [ ] Add "Reset Component" button for default values

---

## 🛠️ Future Extensions

- [ ] Scene graph visualization (parent-child relationships)
- [ ] Save/load ECS state to JSON or binary
- [ ] Drag-and-drop component editing
- [ ] Scripting integration (Python/Lua)
- [ ] Asset preview (textures, meshes, audio)

---

## 📦 File Structure Suggestions
src/ ├── ecs/ │   ├── EntityManager.hpp │   ├── ComponentRegistry.hpp │   └── SystemManager.hpp ├── debug/ │   ├── ECSInspector.hpp │   └── ECSInspector.cpp

---

## 🧠 Notes

- Use ImGui’s `TreeNode`, `Table`, and `InputFloat3` widgets for clean layout
- Consider adding a `ComponentReflection` interface to simplify UI binding
- Keep inspector decoupled from core ECS logic—treat it as a debug overlay
