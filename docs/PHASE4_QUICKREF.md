# Phase 4 Quick Reference Guide

## Phase 4 Overview
**Theme**: Advanced Graphics & Rendering Pipeline  
**Goal**: Transform VulkanMon into a visually impressive rendering engine  
**Duration**: 6-8 weeks  

## Development Setup

### Branch Structure
- **Main Development**: `lighting-system` (for Steps 32-34)  
- **Material System**: `material-system` (for Steps 35-37)
- **Advanced Rendering**: `advanced-rendering` (for Steps 38-40)
- **Performance**: `performance-optimization` (for Steps 41-43)

### Hot Development Workflow
1. **Make shader changes** in `shaders/` directory
2. **Press R key** to hot reload shaders
3. **Use arrow keys** to test light direction (Step 32+)
4. **Use WASD** for camera positioning
5. **Check logs** for performance data

## Phase 4 Step Overview

### Lighting System (Weeks 1-2)
- **Step 32**: Basic directional lighting (sun/moon) - **START HERE**
- **Step 33**: Point lights with attenuation  
- **Step 34**: Spot lights with cone controls

### Material System (Weeks 3-4)  
- **Step 35**: PBR material foundation
- **Step 36**: Multiple texture support (normal, specular maps)
- **Step 37**: Advanced material properties (emission, transparency)

### Advanced Rendering (Weeks 5-6)
- **Step 38**: Shadow mapping
- **Step 39**: HDR and post-processing
- **Step 40**: Multi-pass rendering foundation

### Performance & Polish (Weeks 7-8)
- **Step 41**: Frustum and occlusion culling  
- **Step 42**: Level of Detail (LOD) system
- **Step 43**: GPU profiling and optimization

## Key Files for Phase 4

### New Core Systems
```
src/
├── LightingSystem.h/.cpp      # Light management (Step 32-34)
├── MaterialSystem.h/.cpp      # PBR materials (Step 35-37)  
├── RenderPipeline.h/.cpp      # Multi-pass rendering (Step 38-40)
├── ShadowSystem.h/.cpp        # Shadow mapping (Step 38)
├── PostProcessor.h/.cpp       # HDR, tone mapping (Step 39)
└── SceneManager.h/.cpp        # Culling, LOD (Step 41-43)
```

### Enhanced Systems
- **AssetManager**: Extended for material textures
- **ResourceManager**: New resource types (render targets, shadow maps)
- **ModelLoader**: Material property extraction  
- **Logger**: Performance profiling integration

## Testing Strategy

### Running Phase 4 Tests
```bash
cd build/tests_cpp/Debug

# All tests
./vulkanmon_tests.exe

# Phase 4 specific
./vulkanmon_tests.exe "[Lighting]"      # Lighting math tests
./vulkanmon_tests.exe "[Materials]"     # Material property tests
./vulkanmon_tests.exe "[Performance]"   # Graphics performance tests
./vulkanmon_tests.exe "[Visual]"        # Visual regression tests
```

### Test Categories Added per Step
- **Step 32**: `[Lighting][DirectionalLight]` - Directional light math
- **Step 33**: `[Lighting][PointLight]` - Point light attenuation  
- **Step 35**: `[Materials][PBR]` - PBR material calculations
- **Step 38**: `[Shadows][ShadowMapping]` - Shadow map validation
- **All Steps**: `[Performance][Graphics]` - Frame rate regression tests

## Development Controls

### Runtime Controls (Step 32+)
- **Arrow Keys**: Rotate directional light direction
- **R Key**: Hot reload shaders (existing)
- **WASD**: Camera movement (existing)
- **QE**: Camera up/down movement (existing)

### Future Controls (Later Steps)
- **Number Keys**: Switch between point lights (Step 33)
- **F Key**: Toggle flashlight spot light (Step 34)  
- **M Key**: Cycle through material presets (Step 35)
- **P Key**: Toggle post-processing effects (Step 39)

## Performance Targets

### Frame Rate Requirements
- **60+ FPS**: With basic directional lighting (Step 32)
- **60+ FPS**: With multiple point lights (Step 33-34)
- **55+ FPS**: With PBR materials (Step 35-37)
- **50+ FPS**: With shadow mapping (Step 38)
- **60+ FPS**: After optimization (Step 41-43)

### Memory Targets
- **< 100MB**: GPU memory usage for basic lighting
- **< 200MB**: With full material and shadow systems
- **< 500MB**: Maximum for complete advanced rendering

## Success Milestones

### Visual Quality Checkpoints
- **Step 32**: Clear light/shadow definition on textured cube
- **Step 34**: Multiple colored lights illuminating scene  
- **Step 37**: Realistic metal, plastic, wood materials
- **Step 38**: Dynamic shadows cast by objects
- **Step 39**: Enhanced contrast, bloom effects
- **Step 43**: Professional-grade visual quality

### Technical Checkpoints
- **Architecture**: Modular systems easy to extend
- **Performance**: Consistent 60+ FPS maintained
- **Developer Experience**: Hot reloading works for all shaders
- **Testing**: Comprehensive test coverage for graphics features

## Common Issues & Solutions

### Build Issues
- **Shader compilation fails**: Check GLSL syntax in `shaders/` directory
- **Linking errors**: Ensure all new `.cpp` files added to CMakeLists.txt
- **Vulkan validation**: Check Vulkan SDK installation and validation layers

### Runtime Issues  
- **Black screen**: Check uniform buffer alignment and binding
- **Performance drops**: Use Logger performance monitoring to identify bottlenecks
- **Hot reloading fails**: Ensure shader files exist and are writable

### Visual Issues
- **Too bright/dark**: Adjust light intensity and ambient lighting values
- **Incorrect normals**: Verify normal calculation and transformation
- **Z-fighting**: Ensure depth testing enabled and proper depth buffer format

## Next Steps After Phase 4

Phase 4 prepares for:
- **Scene Management**: Large world streaming
- **Animation System**: Character and object animation  
- **Particle Effects**: Environmental and magic effects
- **Audio Integration**: Spatial audio system
- **Game Logic**: Creature behavior and interactions

---

**Remember**: Follow VulkanMon's philosophy - "Simple is Powerful", "Test, Test, Test", "Document Often"

*Phase 4 will transform VulkanMon into a production-ready rendering engine!*