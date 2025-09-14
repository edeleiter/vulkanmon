# Multi-Descriptor Set Pipeline Refactor

**Date:** 2025-09-14
**Purpose:** Restructure VulkanRenderer to support per-material descriptor sets for efficient multi-material rendering

## Current Architecture Analysis

### Single Descriptor Set Approach (Current)
```
Descriptor Set 0:
├── Binding 0: UBO (view, projection, camera) - GLOBAL, per-frame
├── Binding 1: Texture sampler - GLOBAL, rarely changes
├── Binding 2: Lighting data - GLOBAL, per-frame
└── Binding 3: Material data - PER-OBJECT, changes frequently
```

**Problems:**
1. **Performance:** Entire descriptor set must be recreated/rebound for each object with different materials
2. **Memory Waste:** Global data (UBO, lighting, texture) duplicated across material descriptor sets
3. **Limited Scalability:** Adding per-material textures/resources requires complete descriptor set recreation

### Shader Current Binding Layout
```glsl
// triangle.vert
layout(binding = 0) uniform UniformBufferObject { ... } ubo;
layout(binding = 2) uniform LightingData { ... } lighting;

// triangle.frag
layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform LightingData { ... } lighting;
layout(binding = 3) uniform MaterialData { ... } material;
```

## New Architecture Design

### Multi-Descriptor Set Approach (Target)
```
Descriptor Set 0 (Global):
├── Binding 0: UBO (view, projection, camera)
├── Binding 1: Texture sampler
└── Binding 2: Lighting data

Descriptor Set 1 (Per-Material):
└── Binding 0: Material data (rebased for clarity)
```

**Benefits:**
1. **Performance:** Global descriptor set bound once per frame, only material sets change per object
2. **Memory Efficiency:** No duplication of global data across material descriptor sets
3. **Scalability:** Easy to extend per-material descriptor set with textures, normal maps, etc.
4. **Cache Efficiency:** Better Vulkan driver descriptor set caching

### New Shader Binding Layout
```glsl
// triangle.vert
layout(set = 0, binding = 0) uniform UniformBufferObject { ... } ubo;
layout(set = 0, binding = 2) uniform LightingData { ... } lighting;

// triangle.frag
layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform LightingData { ... } lighting;
layout(set = 1, binding = 0) uniform MaterialData { ... } material;
```

## Implementation Plan

### Phase 1: Infrastructure Setup
1. **Add new descriptor set layouts to VulkanRenderer**
   - `globalDescriptorSetLayout_` (UBO, texture, lighting)
   - `materialDescriptorSetLayout_` (material data only)

2. **Add new descriptor pools and sets**
   - `globalDescriptorPool_` and `globalDescriptorSet_`
   - Material descriptor sets handled by MaterialSystem

3. **Update pipeline layout**
   - Change from single descriptor set layout to array of two layouts
   - Ensure proper binding order: `[globalLayout, materialLayout]`

### Phase 2: MaterialSystem Integration
1. **Update MaterialSystem descriptor set creation**
   - Use new `materialDescriptorSetLayout_` from VulkanRenderer
   - Create descriptor sets with only binding 0 (material data)
   - Remove references to old combined descriptor set approach

2. **Add MaterialSystem-VulkanRenderer communication**
   - VulkanRenderer provides `materialDescriptorSetLayout_` to MaterialSystem
   - MaterialSystem uses renderer's descriptor set layout for consistency

### Phase 3: Shader Updates
1. **Update vertex shader (triangle.vert)**
   - Change `layout(binding = 0)` to `layout(set = 0, binding = 0)` for UBO
   - Change `layout(binding = 2)` to `layout(set = 0, binding = 2)` for lighting

2. **Update fragment shader (triangle.frag)**
   - Change `layout(binding = 1)` to `layout(set = 0, binding = 1)` for texture
   - Change `layout(binding = 2)` to `layout(set = 0, binding = 2)` for lighting
   - Change `layout(binding = 3)` to `layout(set = 1, binding = 0)` for material

3. **Recompile shaders**
   - Ensure CMake shader compilation picks up changes
   - Test shader compilation with new binding syntax

### Phase 4: Rendering Pipeline Updates
1. **Update beginECSFrame()**
   - Bind global descriptor set (set 0) once per frame
   - Remove old single descriptor set binding

2. **Update renderECSObject()**
   - Bind material-specific descriptor set (set 1) per object
   - Use MaterialSystem to get appropriate descriptor set for materialId

3. **Update legacy rendering methods**
   - Ensure backward compatibility during transition
   - Update any other rendering paths that use descriptor sets

### Phase 5: Cleanup and Testing
1. **Remove legacy descriptor set code**
   - Remove `descriptorSetLayout_`, `descriptorPool_`, `descriptorSet_`
   - Clean up old `createDescriptorSetLayout()` method
   - Remove redundant descriptor set creation code

2. **Performance validation**
   - Measure frame time before/after refactor
   - Verify no performance regression
   - Test with multiple materials in scene

3. **Integration testing**
   - ECS Inspector material switching
   - Multi-object rendering with different materials
   - Hot shader reload functionality

## Technical Considerations

### Vulkan Pipeline Layout Requirements
```cpp
// Pipeline layout must specify both descriptor set layouts in correct order
VkDescriptorSetLayout layouts[] = {globalDescriptorSetLayout_, materialDescriptorSetLayout_};
pipelineLayoutInfo.setLayoutCount = 2;
pipelineLayoutInfo.pSetLayouts = layouts;
```

### Descriptor Set Binding Commands
```cpp
// Bind global descriptor set (once per frame)
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                       pipelineLayout_, 0, 1, &globalDescriptorSet_, 0, nullptr);

// Bind material descriptor set (per object)
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                       pipelineLayout_, 1, 1, &materialDescriptorSet, 0, nullptr);
```

### Memory Layout Considerations
- UBO, lighting, and texture data remain in global descriptor set
- Only material data moves to per-material descriptor sets
- No changes to uniform buffer layouts or data structures
- Push constants (model matrix) remain unchanged

## Rollback Plan
If issues arise during implementation:
1. Keep legacy descriptor set code until new system is fully tested
2. Use feature flag to switch between old/new descriptor set approaches
3. Maintain shader variants for both binding layouts during transition
4. Full rollback possible by reverting shader changes and re-enabling legacy code

## Success Criteria
- [ ] Multi-material rendering works correctly
- [ ] Performance equal to or better than current implementation
- [ ] ECS Inspector material switching functional
- [ ] All existing functionality preserved
- [ ] Clean code architecture with no legacy artifacts
- [ ] Comprehensive test coverage

## File Modifications Required

### Core Implementation Files
- `src/rendering/VulkanRenderer.h` - Add new descriptor set layouts
- `src/rendering/VulkanRenderer.cpp` - Implement new descriptor set creation/binding
- `src/systems/MaterialSystem.h/.cpp` - Update for new descriptor set layout

### Shader Files
- `shaders/triangle.vert` - Update binding layouts with set specifiers
- `shaders/triangle.frag` - Update binding layouts with set specifiers

### Build System
- `CMakeLists.txt` - Ensure shader recompilation triggered by changes

This refactor will establish VulkanMon as having professional-grade material system architecture suitable for complex game rendering pipelines.