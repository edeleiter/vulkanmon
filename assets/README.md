# VulkanMon Assets Directory

This directory contains all game assets organized by type for Phase 3 and beyond.

## Directory Structure

### `/models/`
- **Purpose**: 3D models and geometry data
- **Formats**: `.obj`, `.fbx`, `.gltf`, `.dae` (processed via Assimp)
- **Examples**: Creatures, environment objects, characters
- **Phase 3 Target**: Simple test models (cube, sphere, basic creatures)

### `/textures/`
- **Purpose**: Image files for materials and UI
- **Formats**: `.png`, `.jpg`, `.tga`, `.dds`
- **Categories**: Diffuse, normal, roughness, metallic, emission maps
- **Phase 3 Target**: Basic material textures, test patterns

### `/shaders/`
- **Purpose**: Extended shader library beyond basic triangle/cube
- **Formats**: `.vert`, `.frag`, `.comp`, `.geom`
- **Categories**: PBR materials, lighting, post-processing
- **Phase 3 Target**: Basic lighting shaders, material system

### `/audio/`
- **Purpose**: Sound effects and music (Phase 4)
- **Formats**: `.wav`, `.ogg`, `.mp3`
- **Categories**: SFX, ambient, music, voice
- **Phase 3 Target**: Placeholder for Phase 4 integration

### `/scenes/`
- **Purpose**: Scene description files and level data
- **Formats**: Custom scene format, JSON configs
- **Categories**: Levels, test scenes, creature environments
- **Phase 3 Target**: Simple test scenes for asset loading

## Asset Pipeline

### Current State (Phase 2.5)
- Procedural geometry (triangle, cube)
- Programmatic textures (checkered pattern)
- Basic vertex/fragment shaders

### Phase 3 Goals
- Assimp model loading from `/models/`
- Image texture loading from `/textures/`
- Expanded shader library in `/shaders/`
- Scene management system for `/scenes/`

## Development Guidelines

### Asset Naming
- Use descriptive names: `creature_basic_01.obj`, `grass_diffuse.png`
- Include version/variant suffixes: `_v1`, `_alt`, `_lod0`
- Maintain consistent naming across related assets

### File Organization
- Group related assets in subdirectories
- Keep test/development assets separate from production
- Document asset sources and licenses

### Performance Considerations
- Target reasonable file sizes for real-time rendering
- Consider LOD versions for complex models
- Optimize textures for GPU memory constraints

---

*Asset organization following VulkanMon's "Simple is Powerful" philosophy*