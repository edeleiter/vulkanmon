# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Claude Project Instructions

You are an expert at everything game development - especially custom Vulkan game engines and building/designing games like Pokemon Legends: Arceus.

## Project Overview

VulkanMon is a modern C++20 game engine with Vulkan graphics, designed as a foundation for creating Pokemon-style 3D creature collector games. The project follows three core principles: "Simple is Powerful", "Test, Test, Test", and "Document Often".

## Build System

### Build Commands
```bash
# Initial setup from root directory
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .

# Quick rebuild after source changes
cmake --build .

# Run the application
Debug/vulkanmon.exe  # Windows
./Debug/vulkanmon    # Linux/macOS
```

### Dependencies
- **Package Manager**: vcpkg (included in repo, pre-configured)
- **Build System**: CMake 3.20+ with C++20 support
- **Graphics**: Vulkan SDK 1.3+, GLFW (windowing), GLM (math)
- **Assets**: Assimp for 3D model loading, STB for image loading

### Testing
```bash
# Run Python integration test suite
cd tests
pip install -r requirements.txt
python -m pytest

# Run specific test categories
python -m pytest -m build        # Build system tests
python -m pytest -m vulkan       # Vulkan runtime tests
python -m pytest -m integration  # End-to-end tests

# Run C++ unit tests (Sweet Spot approach)
cd build/tests_cpp
Debug/vulkanmon_tests.exe        # All C++ unit tests

# Run specific C++ test categories
Debug/vulkanmon_tests.exe "[Logger]"   # Logger unit tests
Debug/vulkanmon_tests.exe "[Camera]"   # Camera unit tests
Debug/vulkanmon_tests.exe "[LightingSystem]"   # LightingSystem unit tests (pending implementation)
Debug/vulkanmon_tests.exe "[MaterialSystem]"   # MaterialSystem unit tests (pending implementation)
```

## Architecture

### Core Systems
- **ResourceManager** (`src/ResourceManager.h/.cpp`): RAII wrappers for Vulkan resources (buffers, images, memory)
- **Logger** (`src/Logger.h/.cpp`): Thread-safe logging system with multiple outputs and performance tracking
- **AssetManager** (`src/AssetManager.h/.cpp`): Texture loading, caching, and asset discovery
- **ModelLoader** (`src/ModelLoader.h/.cpp`): Assimp-based 3D model loading with material support
- **Camera** (`src/Camera.h/.cpp`): Interactive WASD camera controls with mouse look
- **LightingSystem** (`src/LightingSystem.h/.cpp`): Directional and ambient lighting with Vulkan descriptor management
- **MaterialSystem** (`src/MaterialSystem.h/.cpp`): Material property management with Phong lighting support

### Main Application
- **Entry Point**: `src/main.cpp` - Complete Vulkan 3D renderer
- **Shaders**: `shaders/` directory with GLSL vertex/fragment shaders
- **Assets**: `assets/` directory containing textures and 3D models

### Current Phase Status
- ✅ **Phase 1 COMPLETE**: Vulkan foundation with hello triangle
- ✅ **Phase 2 COMPLETE**: 3D graphics with MVP matrices, depth testing
- ✅ **Phase 2.5 COMPLETE**: Interactive development (WASD camera, hot shader reload)
- ✅ **Phase 3 COMPLETE**: Core engine systems (logging, assets, model loading)
- ✅ **Phase 3.1 COMPLETE**: C++ Unit Testing Framework (Sweet Spot approach)
- ✅ **Phase 4.1 COMPLETE**: MaterialSystem implementation and integration
- ✅ **Phase 4.2 COMPLETE**: Lighting system refinement and material-lighting integration
- 🚧 **Phase 5.1 IN PROGRESS**: Testing framework expansion and code architecture refactoring

### Phase 4.1 Milestone: MaterialSystem Integration (COMPLETE)

**Achievement**: Successfully implemented complete material property system with real-time switching capabilities.

**Key Accomplishments:**
- **MaterialSystem Architecture**: Complete material management with Vulkan descriptor handling
- **Material Property Support**: Ambient, diffuse, specular, and shininess properties with proper GPU alignment
- **Real-time Material Switching**: Interactive controls for cycling through material presets
- **Automatic Shader Compilation**: CMake integration for automatic GLSL to SPIR-V compilation
- **Material Presets**: Five realistic material types (Default, Gold, Ruby, Chrome, Emerald)

**Interactive Controls:**
- **M Key**: Cycle through material presets with immediate visual feedback
- **5/6 Keys**: Adjust material shininess for specular highlight control
- **R Key**: Hot reload shaders during development

**Technical Foundation:**
- MaterialData structure with vec4 alignment for Vulkan compatibility
- Direct uniform buffer approach for reliable GPU data transfer
- Integrated with existing descriptor set management
- Comprehensive logging and error handling

### Phase 4.2 Milestone: Lighting System Refinement (COMPLETE)

**Achievement**: Successfully implemented complete material-lighting integration with realistic rendering.

**Key Accomplishments:**
- **Material-Lighting Integration**: Fixed fragment shader to properly combine material properties with lighting
- **Blinn-Phong Specular Model**: Implemented more realistic specular highlights than standard Phong
- **Calibrated Lighting Presets**: Normalized light directions and optimized intensities for 4 lighting scenarios
- **Enhanced Interactive Controls**: Improved shininess adjustment with clear visual feedback
- **Realistic Material Response**: Materials now show distinct characteristics under lighting

**Interactive Controls:**
- **M Key**: Cycle through 5 material presets (Default, Gold, Ruby, Chrome, Emerald)
- **5/6 Keys**: Adjust material shininess (±10.0 increments) with console feedback
- **1/2 Keys**: Adjust directional light intensity
- **3 Key**: Cycle through 4 lighting presets (Default, Daylight, Sunset, Moonlight)
- **4 Key**: Toggle ambient lighting on/off
- **L Key**: Print detailed lighting debug information
- **R Key**: Hot reload shaders during development

**Technical Achievements:**
- Fixed material multiplication issue in fragment shader
- Implemented Blinn-Phong specular calculations for better performance and realism
- Normalized all directional light vectors with proper GLM integration
- Enhanced console feedback for real-time parameter adjustment
- Verified all material presets show distinct visual characteristics

### Phase 5.1 Next Steps: Testing Framework Expansion and Architecture Refactoring

**Current Priority**: Expand testing coverage and refactor main.cpp architecture for better scalability.

**Phase 5.1 Goals:**
1. **Complete Unit Test Coverage**: Add missing LightingSystem and MaterialSystem tests
2. **Architecture Refactoring**: Extract Application, VulkanRenderer, InputHandler classes from main.cpp
3. **Resource Management Consistency**: Migrate remaining Vulkan resources to ResourceManager
4. **Integration Test Enhancement**: Add material-lighting validation tests

**Phase 5.2 Planning:**
1. **Scene Management System**: Implement scene graph and multi-object rendering
2. **Performance Monitoring**: Add GPU performance counters and profiling tools
3. **Enhanced Asset Management**: Asset dependency tracking and hot-reloading

**Success Criteria:**
- 100% unit test coverage for all core systems
- main.cpp reduced from 1,641 lines to manageable application entry point
- All Vulkan resources managed through ResourceManager RAII system
- Clear separation of concerns between rendering, input, and application logic

**Detailed Planning:**
See [docs/PHASE_5_PLAN.md](docs/PHASE_5_PLAN.md) for comprehensive Phase 5 development strategy, implementation timeline, and architectural decisions.

## Development Workflow

### Code Conventions
- **Modern C++20**: Use RAII, smart pointers, `auto`, `constexpr`
- **Vulkan Best Practices**: Proper synchronization, efficient descriptor management
- **Error Handling**: Exceptions for unrecoverable errors, detailed error messages
- **Memory Management**: No raw `new`/`delete`, use ResourceManager for Vulkan objects
- **Documentation Style**: No emojis in code, documentation, or comments - focus on clear, professional text

### Testing Requirements
Before committing code:
1. **Build Test**: Code must compile cleanly
2. **Unit Tests**: C++ tests pass (Logger, Camera, LightingSystem when implemented)
3. **Console Validation**: Application runs without errors
4. **Visual Validation**: Rendering output matches expectations
5. **Performance Check**: Maintains 60+ FPS

### Hot Reloading
- **Shader Reloading**: Press 'R' key during runtime to reload shaders from `shaders/` directory
- **Interactive Camera**: WASD + mouse for movement, QE for up/down

## Asset Organization

### Directory Structure
```
assets/
├── textures/          # .jpg, .png image files
├── models/           # .obj, .fbx, .gltf 3D models
└── shaders/          # .vert, .frag GLSL shaders (compiled to .spv)
```

### Supported Formats
- **Textures**: JPG, PNG (via STB)
- **3D Models**: 40+ formats via Assimp (.obj, .fbx, .gltf, .dae, etc.)
- **Shaders**: GLSL (compiled to SPIR-V)

## Common Development Tasks

### Adding New 3D Models
1. Place model files in `assets/models/`
2. Use ModelLoader class to load at runtime
3. Models automatically processed for materials and textures

### Shader Development
1. Edit GLSL files in `shaders/` directory
2. Use hot reload (R key) for rapid iteration
3. Shaders automatically compiled to SPIR-V on build

### Performance Monitoring
- Logger system provides automatic performance tracking
- Frame time and render statistics logged during development
- Use Vulkan validation layers for debugging

## Important Notes

### Resource Management
- **Never** use raw Vulkan handles directly
- **Always** use ResourceManager wrappers for automatic cleanup
- **All** Vulkan resources are RAII-managed

### Development Philosophy
- **Build incrementally**: Get features working before optimization
- **Test frequently**: Validate changes immediately after implementation  
- **Document decisions**: Update progress files and code comments

### Platform Support
- **Primary**: Windows with MSVC 2019+
- **Future**: Linux (GCC/Clang), macOS (Clang + MoltenVK)
- **GPU Support**: NVIDIA and AMD cards with Vulkan 1.3+