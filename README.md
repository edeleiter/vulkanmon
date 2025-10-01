# VulkanMon - Open Source Creature Collector Game Engine

An open-source game engine built with modern C++20 and Vulkan, designed to create a Pokemon-style 3rd person creature collector game with Python scripting capabilities.

## Project Vision

VulkanMon aims to be a high-performance, cross-platform game engine that combines the power of low-level graphics programming with the accessibility of Python scripting. The engine targets developers who want to create creature collection games with modern graphics and flexible gameplay systems.

## Architecture Overview

### Core Engine (C++20)
- **Vulkan Renderer**: Low-level graphics API for maximum performance
- **Physics Integration**: Professional Jolt Physics engine with multi-threading
- **Audio System**: OpenAL for 3D positioned audio
- **Resource Management**: Efficient asset loading, caching, and streaming
- **Memory Management**: RAII-based with smart pointers
- **Cross-Platform Support**: Windows, Linux, macOS

### Scripting Layer (Cython)
- **Game Logic**: Creature behaviors, battle mechanics, AI decision trees
- **Data Management**: Stats, abilities, progression systems
- **UI Systems**: Menu logic, inventory management
- **Modding Support**: Easy content creation for community

## Key Features

### Rendering System
- **Modern Vulkan Pipeline**: Physically-based rendering (PBR)
- **Animation System**: Skeletal animation with blending
- **Instanced Rendering**: Efficient rendering of multiple creatures
- **Level-of-Detail (LOD)**: Performance optimization for distant objects
- **Material System**: Flexible shader-based materials

### Game Systems
- **Entity-Component System**: Flexible game object architecture with Transform, Renderable, Camera components
- **Render System**: Efficient sorting, culling, and batch rendering of game objects
- **Camera System**: Multi-camera support with priority-based activation
- **Battle System**: Turn-based combat with abilities and stats (planned)
- **Creature Management**: Catching, training, and evolving creatures (planned)
- **Save System**: Persistent game state and player progress (planned)
- **World Management**: Dynamic loading of game areas (planned)

### Developer Tools
- **Asset Pipeline**: Automated processing of 3D models, textures, audio
- **Debug UI**: Dear ImGui integration for development tools
- **Hot Reloading**: Runtime script updates during development
- **Performance Profiling**: Built-in timing and memory analysis

## Technology Stack

### Core Dependencies
- **Graphics**: Vulkan SDK, GLFW (windowing), GLM (math)
- **3D Models**: Assimp for model loading and processing
- **Physics**: Jolt Physics for collision and dynamics
- **Audio**: OpenAL Soft for 3D audio processing
- **Build System**: CMake with vcpkg/Conan for dependency management

### Development Tools
- **Languages**: C++20 (core), Cython (scripting interface), Python (game logic)
- **Architecture**: Entity Component System (ECS) for scalable game object management
- **Version Control**: Git with GitHub Actions for CI/CD
- **Documentation**: Doxygen for code documentation
- **Testing**: Comprehensive C++ unit testing with Catch2 framework (127 test cases, 1930 assertions)

## Development Roadmap

### Phase 1: Foundation COMPLETE!
- [x] Project setup and build system with vcpkg dependency management
- [x] **Vulkan hello triangle** - Beautiful RGB gradient triangle rendering!
- [x] **Modern testing framework** - Comprehensive C++ unit tests with Catch2
- [x] **Documentation and philosophy** - Core tenants established

### Phase 2: 3D Graphics Foundation COMPLETE!
- [x] **Texture loading and sampling** - Full texture support with STB integration
- [x] **Uniform Buffer Objects (UBOs)** - Complete 3D transformation pipeline
- [x] **Depth testing and depth buffer** - Proper 3D rendering with depth sorting  
- [x] **3D model rendering** - Assimp-based model loading with material support
- [x] **Camera system** - Interactive WASD movement and mouse look controls
- [x] **Model-View-Projection matrices** - Full 3D transformation pipeline

### Phase 2.5: Interactive Development COMPLETE!
- [x] **Hot shader reloading** - Press 'R' to reload shaders during development
- [x] **Interactive camera controls** - Smooth WASD movement with mouse look
- [x] **Performance monitoring** - Logger system with frame timing

### Phase 3: Core Engine Systems COMPLETE!
- [x] **ResourceManager** - RAII-based Vulkan resource management
- [x] **Logger System** - Thread-safe logging with console/file output
- [x] **AssetManager** - Texture caching and asset discovery
- [x] **ModelLoader** - Assimp integration for 40+ 3D model formats
- [x] **Camera System** - Complete interactive 3D camera implementation

### Phase 3.1: Testing Framework COMPLETE!
- [x] **C++ Unit Testing** - Sweet Spot approach with 100% core system coverage
- [x] **Integration Testing** - Python-based build and runtime validation
- [x] **Visual Validation** - Real hardware rendering verification

### Phase 4.1: MaterialSystem COMPLETE!
- [x] **Material Management** - Complete material property system
- [x] **Real-time Material Switching** - Interactive controls with 5 presets
- [x] **Vulkan Integration** - Proper descriptor management and GPU alignment
- [x] **Material Presets** - Realistic Gold, Ruby, Chrome, Emerald materials

### Phase 4.2: Lighting Integration COMPLETE!
- [x] **LightingSystem** - Directional and ambient lighting with descriptor management
- [x] **Material-Lighting Integration** - Fixed shader calculations for realistic rendering
- [x] **Blinn-Phong Specular** - Advanced specular highlighting model
- [x] **Interactive Lighting Controls** - 4 lighting presets with real-time adjustment
- [x] **Realistic Material Response** - Materials show distinct characteristics under lighting

### Phase 5.1: Testing & Architecture COMPLETE!
- [x] **Complete Unit Test Coverage** - Add LightingSystem and MaterialSystem tests
- [x] **Architecture Refactoring** - Extract Application, VulkanRenderer, InputHandler classes
- [x] **Resource Management Consistency** - Migrate all resources to ResourceManager
- [x] **Cross-Platform Build System** - vcpkg integration with multi-platform CI

### Phase 5.2: Architecture & ECS Foundation COMPLETE!
- [x] **Clean Application Architecture** - Separated concerns with modular design
- [x] **Advanced Testing Framework** - 100% unit test coverage (1930 assertions across 127 tests)
- [x] **Cross-Platform Compatibility** - Windows + Linux build verification
- [x] **Main.cpp Refactoring** - Clean, maintainable application entry point

### Phase 6.1: Entity Component System COMPLETE!
- [x] **Core ECS Framework** - Entity, Component, System architecture
- [x] **Transform & Renderable Components** - Foundation for game objects
- [x] **Camera Component System** - ECS-based camera management
- [x] **RenderSystem Implementation** - Sorting, culling, render command generation
- [x] **Comprehensive ECS Testing** - 48 assertions across 6 test cases
- [x] **Pokemon Game Foundation** - Architecture supports creature collection gameplay

### Phase 6.2: ECS-VulkanRenderer Integration COMPLETE!
- [x] **Multi-Object Rendering** - 5 entities with diverse models (cube, sphere, pyramid, plane)
- [x] **Model Caching System** - Efficient asset loading with automatic caching
- [x] **ECS Render Pipeline** - beginECSFrame → renderECSObject → endECSFrame workflow
- [x] **Performance Optimization** - 60+ FPS maintained with multiple objects

### Phase 6.3: ECS Inspector - Real-time Debug Interface COMPLETE!
- [x] **Professional ImGui Interface** - Unity/Unreal-style inspector with live editing
- [x] **Real-time Component Editing** - Transform, Renderable, Camera components
- [x] **Entity Management** - Creation, deletion, templates (Cube, Sphere, Pyramid, Plane, Camera)
- [x] **Performance Profiler** - System monitoring with sub-millisecond overhead
- [x] **Live Material/Mesh Switching** - Immediate visual feedback

### Phase 6.4: Window Management COMPLETE!
- [x] **Professional Window Resize** - Smooth swapchain recreation without freezing
- [x] **Dynamic Viewport Scaling** - 3D scene scales properly to new dimensions
- [x] **ImGui Integration** - Interface scales correctly with window size updates

### Phase 7: Advanced Graphics
- [ ] **PBR Material System** - Physically-based rendering materials
- [ ] **Multi-Light Support** - Point lights, spot lights, shadows
- [ ] **Skeletal Animation** - Character animation system
- [ ] **Post-processing Effects** - Bloom, tone mapping, anti-aliasing

### Phase 7.1: Jolt Physics Integration COMPLETE!
- [x] **Professional Physics Engine** - Jolt Physics with multi-threading and AAA-quality collision
- [x] **Physics Demo Scene** - Falling spheres, boxes, capsules with realistic physics behaviors
- [x] **ECS Physics Components** - RigidBodyComponent, CollisionComponent, CreaturePhysicsComponent
- [x] **Layer-Based Collision** - Comprehensive collision filtering for Pokemon-style gameplay
- [x] **Performance Optimized** - 1300+ FPS with 22+ physics entities and spatial queries

### Phase 8: Game Systems Foundation
- [x] **Physics Integration** - Jolt Physics integration
- [ ] **Audio System** - 3D positioned audio with OpenAL
- [ ] **Multi-threading & Job System** - Parallel processing for performance
- [ ] **Asset Streaming** - Dynamic loading for large worlds

### Phase 9: Pokemon Game Systems
- [ ] **Creature Framework** - Core creature data structures and components
- [ ] **Battle System** - Turn-based combat mechanics with ECS integration
- [ ] **AI Behavior System** - Decision trees and creature AI using ECS
- [ ] **Collection Mechanics** - Catching, training, and evolution systems
- [ ] **Open World Systems** - Large seamless world streaming
- [ ] **Player Interaction** - Mount/ride system, stealth mechanics, Pokeball physics

## Getting Started

### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- Vulkan SDK 1.3+
- CMake 3.20+
- Python 3.8+ (for scripting)

### Quick Start
```bash
# Clone the repository
git clone https://github.com/yourusername/vulkanmon.git
cd vulkanmon

# Build the project (dependencies managed by vcpkg)
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .

# Run the hello triangle demo
Debug/vulkanmon.exe

# Run the C++ unit test suite
cd build/tests_cpp
Debug/vulkanmon_tests.exe
```

## Development Philosophy

VulkanMon follows three core tenants:

### **"Simple is Powerful"**
We favor clear, straightforward solutions over complex ones. Code should be easy to understand, maintain, and extend.

### **"Test, Test, Test"**  
Confidence comes from thorough testing at every level - from unit tests to visual validation on real hardware. We have comprehensive C++ unit tests covering all core engine systems!

### **"Document Often"**
Good documentation is as important as good code. We document the "why" not just the "what".

*See [DEVELOPMENT_PHILOSOPHY.md](DEVELOPMENT_PHILOSOPHY.md) for detailed guidelines.*

## Contributing

We welcome contributions from developers of all skill levels! Here's how you can help:

### Areas of Contribution
- **Core Engine**: C++ systems programming, Vulkan expertise
- **Game Logic**: Python/Cython scripting for gameplay features
- **Art & Assets**: 3D models, textures, animations, audio
- **Documentation**: Tutorials, API docs, examples
- **Testing**: Complete C++ unit test coverage for all engine systems

### Development Guidelines
- Follow modern C++20 best practices
- Use RAII for resource management
- Write comprehensive tests for new features
- Document public APIs with Doxygen
- Follow the existing code style and conventions

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Vulkan Tutorial (vulkan-tutorial.com) for foundational graphics programming knowledge
- Open source game engines like Godot and Bevy for architectural inspiration
- The Vulkan and game development communities for their invaluable resources

## Contact

- Project Lead: [Your Name](mailto:your.email@domain.com)
- Discord: [Community Server Link]
- Issues: [GitHub Issues](https://github.com/yourusername/vulkanmon/issues)

---

**Note**: This project is in active development. The roadmap and features may change as the project evolves. We encourage early feedback and contributions to help shape the engine's direction.