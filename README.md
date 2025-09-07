# VulkanMon - Open Source Creature Collector Game Engine

An open-source game engine built with modern C++20 and Vulkan, designed to create a Pokemon-style 3rd person creature collector game with Python scripting capabilities.

## Project Vision

VulkanMon aims to be a high-performance, cross-platform game engine that combines the power of low-level graphics programming with the accessibility of Python scripting. The engine targets developers who want to create creature collection games with modern graphics and flexible gameplay systems.

## Architecture Overview

### Core Engine (C++20)
- **Vulkan Renderer**: Low-level graphics API for maximum performance
- **Physics Integration**: Bullet Physics for realistic interactions
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
- **Entity-Component System**: Flexible game object architecture
- **Battle System**: Turn-based combat with abilities and stats
- **Creature Management**: Catching, training, and evolving creatures
- **Save System**: Persistent game state and player progress
- **World Management**: Dynamic loading of game areas

### Developer Tools
- **Asset Pipeline**: Automated processing of 3D models, textures, audio
- **Debug UI**: Dear ImGui integration for development tools
- **Hot Reloading**: Runtime script updates during development
- **Performance Profiling**: Built-in timing and memory analysis

## Technology Stack

### Core Dependencies
- **Graphics**: Vulkan SDK, GLFW (windowing), GLM (math)
- **3D Models**: Assimp for model loading and processing
- **Physics**: Bullet Physics for collision and dynamics
- **Audio**: OpenAL Soft for 3D audio processing
- **Build System**: CMake with vcpkg/Conan for dependency management

### Development Tools
- **Languages**: C++20 (core), Cython (scripting interface), Python (game logic)
- **Version Control**: Git with GitHub Actions for CI/CD
- **Documentation**: Doxygen for code documentation
- **Testing**: Google Test for unit testing

## Development Roadmap

### Phase 1: Foundation (Months 1-3)
- [x] Project setup and build system
- [ ] Vulkan hello triangle
- [ ] Basic resource management
- [ ] Logging and debugging systems

### Phase 2: Core Rendering (Months 4-6)
- [ ] 3D model loading and rendering
- [ ] Camera system and controls
- [ ] Basic lighting and materials
- [ ] Texture management

### Phase 3: Advanced Graphics (Months 7-9)
- [ ] PBR material system
- [ ] Skeletal animation
- [ ] Instanced rendering
- [ ] Post-processing effects

### Phase 4: Game Systems (Months 10-12)
- [ ] Physics integration
- [ ] Audio system
- [ ] Cython scripting layer
- [ ] Basic gameplay framework

### Phase 5: Creature Systems (Months 13-15)
- [ ] Creature data structures
- [ ] Battle system
- [ ] AI behavior trees
- [ ] Save/load functionality

### Phase 6: Polish & Release (Months 16-18)
- [ ] UI systems
- [ ] Performance optimization
- [ ] Documentation and tutorials
- [ ] Community tools and modding support

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

# Install dependencies (using vcpkg)
vcpkg install glfw3 glm assimp bullet3 openal-soft

# Build the project
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build .

# Run the demo
./vulkanmon
```

## Contributing

We welcome contributions from developers of all skill levels! Here's how you can help:

### Areas of Contribution
- **Core Engine**: C++ systems programming, Vulkan expertise
- **Game Logic**: Python/Cython scripting for gameplay features
- **Art & Assets**: 3D models, textures, animations, audio
- **Documentation**: Tutorials, API docs, examples
- **Testing**: Unit tests, integration tests, performance testing

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