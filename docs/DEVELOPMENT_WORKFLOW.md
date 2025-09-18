# VulkanMon Development Workflow & Tooling

## � Developer Onboarding

### Quick Start (5 Minutes)
```bash
# 1. Clone with submodules
git clone --recursive https://github.com/your-org/vulkanmon.git
cd vulkanmon

# 2. Bootstrap vcpkg
./vcpkg/bootstrap-vcpkg.sh    # Linux/macOS
./vcpkg/bootstrap-vcpkg.bat   # Windows

# 3. Build and run
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
Debug/vulkanmon.exe
```

### Prerequisites
- **Vulkan SDK 1.3+** - Download from LunarG
- **CMake 3.20+** - Cross-platform build system
- **C++20 Compiler** - MSVC 2019+, GCC 10+, or Clang 12+
- **Git** - Version control with submodule support

---

## � Development Environment Setup

### Recommended IDEs

#### Visual Studio (Windows)
```json
// .vscode/settings.json - VSCode configuration
{
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=${workspaceFolder}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    ],
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "files.associations": {
        "*.vert": "glsl",
        "*.frag": "glsl",
        "*.comp": "glsl"
    },
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}

// .vscode/tasks.json - Build tasks
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build VulkanMon",
            "type": "shell",
            "command": "cmake",
            "args": ["--build", "build"],
            "group": "build",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "panel": "new"
            }
        },
        {
            "label": "Run Tests",
            "type": "shell",
            "command": "./build/tests_cpp/Debug/vulkanmon_tests.exe",
            "group": "test",
            "dependsOn": "Build VulkanMon"
        }
    ]
}
```

#### CLion (Cross-Platform)
```cmake
# CLion configuration in CMakePresets.json
{
    "version": 3,
    "configurePresets": [
        {
            "name": "default",
            "displayName": "Default Config",
            "description": "Default build using vcpkg",
            "binaryDir": "${sourceDir}/build",
            "toolchainFile": "${sourceDir}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        }
    ]
}
```

### Editor Extensions
- **GLSL Language Support** - Shader syntax highlighting
- **CMake Tools** - Build system integration
- **GitLens** - Enhanced Git functionality
- **Vulkan Memory Allocator** - VMA debugging support

---

## � Build Workflow

### Build Configurations

#### Debug Build (Development)
```bash
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

**Debug Features:**
- Vulkan validation layers enabled
- Debug symbols and assertions
- ImGui inspector available
- Slower performance for development

#### Release Build (Performance)
```bash
mkdir build-release && cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

**Release Features:**
- Optimized performance (60+ FPS)
- Validation layers disabled
- Minimal debug overhead
- Suitable for Pokemon gameplay testing

#### RelWithDebInfo (Profiling)
```bash
mkdir build-profile && cd build-profile
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo \
         -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

**Profiling Features:**
- Optimized with debug symbols
- Perfect for performance analysis
- Profiler-friendly stack traces
- Balanced performance and debuggability

### Incremental Build Optimization
```bash
# Quick rebuild after code changes (30 seconds)
cd build
cmake --build . --parallel $(nproc)

# Shader-only rebuild (5 seconds)
cmake --build . --target shader_compilation

# Test-only rebuild (15 seconds)
cmake --build . --target vulkanmon_tests
```

---

## � Development Features

### Hot Reload System

#### Shader Hot Reload
**Trigger**: Press `R` key during runtime
**Files**: All shaders in `shaders/` directory automatically recompiled

```cpp
// src/core/InputHandler.cpp - Hot reload implementation
void InputHandler::onKeyPress(int key, int action) {
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        Logger::info("Hot reloading shaders...");
        if (application->getRenderer()->reloadShaders()) {
            Logger::info("Shaders reloaded successfully");
        } else {
            Logger::error("Shader reload failed");
        }
    }
}
```

#### Asset Hot Reload (Future)
```bash
# Monitor asset directory for changes
# Automatically reload textures and models when modified
inotifywait -m -r -e modify assets/ | while read file; do
    echo "Reloading asset: $file"
    # Signal VulkanMon to reload specific asset
done
```

### Interactive Development Controls

#### Camera Controls
```cpp
// Runtime camera manipulation
WASD Keys       → Move camera position
QE Keys         → Vertical movement (up/down)
Mouse Movement  → Look around (first-person style)
Left Shift      → Speed boost (2x movement speed)
TAB Key         → Toggle camera/UI mode
```

#### Material & Lighting Controls
```cpp
// Real-time material and lighting adjustment
M Key           → Cycle material presets (Default, Gold, Ruby, Chrome, Emerald)
5/6 Keys        → Adjust material shininess (±10.0)
1/2 Keys        → Adjust directional light intensity
3 Key           → Cycle lighting presets (Default, Daylight, Sunset, Moonlight)
4 Key           → Toggle ambient lighting
L Key           → Print detailed lighting debug information
```

#### Debug Interface
```cpp
// ECS Inspector and debug tools
I Key           → Toggle ECS Inspector on/off
                 - Entity browser with real-time filtering
                 - Component editor (Transform, Renderable, Camera)
                 - Performance profiler
                 - System execution monitoring
```

### Live Entity Editing
**Feature**: ECS Inspector allows real-time entity modification without recompilation

```cpp
// Example: Live Transform editing
// 1. Press I to open ECS Inspector
// 2. Select entity from list
// 3. Modify Transform component:
//    - Position: Drag sliders for X/Y/Z
//    - Rotation: Adjust Euler angles
//    - Scale: Uniform or per-axis scaling
// 4. Changes apply immediately to 3D scene
```

---

## � Testing Workflow

### Test-Driven Development Cycle

#### 1. Write Test First
```cpp
// tests_cpp/test_NewFeature.cpp
TEST_CASE("New Pokemon feature", "[Pokemon][NewFeature]") {
    // Arrange
    World world;
    PokemonSystem pokemonSystem;

    // Act
    auto result = pokemonSystem.processNewFeature();

    // Assert
    REQUIRE(result.isValid());
    REQUIRE(result.pokemonCount > 0);
}
```

#### 2. Implement Minimal Code
```cpp
// src/systems/PokemonSystem.cpp
PokemonResult PokemonSystem::processNewFeature() {
    // Minimal implementation to make test pass
    return PokemonResult{true, 1};
}
```

#### 3. Refactor and Expand
```cpp
// Expand implementation with full Pokemon logic
PokemonResult PokemonSystem::processNewFeature() {
    // Full implementation with error handling
    // Performance optimization
    // Integration with spatial system
}
```

### Testing Commands
```bash
# Run all tests (102 test cases, 1724 assertions)
cd build/tests_cpp
Debug/vulkanmon_tests.exe

# Run specific test categories
Debug/vulkanmon_tests.exe "[Logger]"           # Logger-specific tests
Debug/vulkanmon_tests.exe "[ECS]"              # ECS framework tests
Debug/vulkanmon_tests.exe "[SpatialSystem]"    # Spatial partitioning tests
Debug/vulkanmon_tests.exe "[Performance]"      # Performance benchmarks
Debug/vulkanmon_tests.exe "[Pokemon]"          # Pokemon gameplay tests

# Verbose output with execution times
Debug/vulkanmon_tests.exe -s -d yes

# Run specific test case
Debug/vulkanmon_tests.exe "Logger thread safety"
```

### Performance Testing
```bash
# Benchmark critical systems
Debug/vulkanmon_tests.exe "[Performance]" --benchmark-samples 10

# Memory leak detection (Windows)
Debug/vulkanmon_tests.exe --detect-memory-leaks

# Continuous performance monitoring
while true; do
    Debug/vulkanmon_tests.exe "[Performance]" >> performance_log.txt
    sleep 300  # Every 5 minutes
done
```

---

## � Debugging Tools

### Vulkan Debugging

#### Validation Layers (Debug Builds)
```cpp
// Automatic validation layer setup in debug builds
#ifdef DEBUG
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Enable debug messenger for detailed error reports
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;
#endif
```

#### RenderDoc Integration
```bash
# Launch with RenderDoc for GPU debugging
renderdoc --capture vulkanmon.exe

# Or capture from within application (F12 key)
# Provides frame-by-frame GPU analysis
# Shader debugging and performance profiling
```

### Performance Profiling

#### Built-in Performance Monitoring
```cpp
// Logger automatically tracks performance metrics
Logger::info("Frame time: {:.2f}ms", frameTimeMs);
Logger::info("Entities processed: {}", entityCount);
Logger::info("Spatial queries: {} in {:.3f}ms", queryCount, queryTimeMs);

// ECS Inspector shows real-time performance
// - System execution times
// - Entity counts by component type
// - Memory usage statistics
```

#### External Profiling Tools
```bash
# Intel VTune (CPU profiling)
vtune -collect hotspots -- vulkanmon.exe

# NVIDIA Nsight Graphics (GPU profiling)
nsight-graphics vulkanmon.exe

# AMD Radeon GPU Profiler
RadeonGPUProfiler.exe --captureApplication vulkanmon.exe
```

### Memory Debugging
```cpp
// RAII compliance ensures automatic cleanup
class ResourceManager {
    ~ResourceManager() {
        // All Vulkan resources automatically cleaned up
        for (auto& buffer : buffers) {
            vkDestroyBuffer(device, buffer.buffer, nullptr);
            vkFreeMemory(device, buffer.memory, nullptr);
        }
    }
};

// Memory leak detection in tests
TEST_CASE("Memory leak detection", "[Memory]") {
    size_t initialMemory = getCurrentMemoryUsage();

    {
        // Create and destroy many objects
        for (int i = 0; i < 1000; ++i) {
            auto buffer = resourceManager->createBuffer(1024);
            // Automatic cleanup when buffer goes out of scope
        }
    }

    size_t finalMemory = getCurrentMemoryUsage();
    REQUIRE(finalMemory == initialMemory); // No leaks
}
```

---

## � Asset Workflow

### Asset Organization
```
assets/
├── models/                 # 3D models for Pokemon world
│   ├── creatures/         # Pokemon creature models (.fbx, .obj)
│   ├── environment/       # Trees, rocks, buildings
│   ├── items/            # Pokeballs, berries, tools
│   └── test/             # Development test models
├── textures/              # Image files for materials
│   ├── creatures/        # Pokemon textures and normal maps
│   ├── environment/      # World textures (grass, stone, water)
│   ├── ui/              # Interface icons and backgrounds
│   └── test/            # Development test textures
└── shaders/              # GLSL shader source files
    ├── shader.vert       # Main vertex shader
    ├── shader.frag       # Main fragment shader
    └── debug/           # Debug visualization shaders
```

### Asset Pipeline
```bash
# 1. Add new model to assets/models/
cp new_pokemon.fbx assets/models/creatures/

# 2. Test loading in application
# Application automatically discovers new assets
# ModelLoader supports 40+ file formats via Assimp

# 3. Optimize for production
# Convert large textures to optimal formats
# Generate mipmaps for better performance
# Compress models if needed
```

### Shader Development Workflow
```bash
# 1. Edit GLSL files in shaders/ directory
nano shaders/shader.frag

# 2. Automatic compilation via CMake
cmake --build . --target shader_compilation

# 3. Hot reload in running application
# Press R key to reload all shaders instantly

# 4. Debug shader issues
# Vulkan validation layers provide shader errors
# Use shader debugging tools (RenderDoc, Nsight)
```

---

## � Version Control Workflow

### Git Branching Strategy
```bash
# Main development branch
git checkout main

# Feature development
git checkout -b feature/pokemon-battles
# Implement Pokemon battle system
git add . && git commit -m "Implement basic Pokemon battle mechanics"
git push origin feature/pokemon-battles

# Create pull request for review
gh pr create --title "Pokemon Battle System" --body "Implements core battle mechanics with turn-based combat"

# After review and merge
git checkout main
git pull origin main
git branch -d feature/pokemon-battles
```

### Commit Message Convention
```bash
# Format: type(scope): description
git commit -m "feat(pokemon): add creature detection system"
git commit -m "fix(vulkan): resolve swapchain recreation bug"
git commit -m "test(spatial): add performance benchmarks"
git commit -m "docs(readme): update build instructions"
git commit -m "refactor(ecs): simplify component storage"
```

### Submodule Management
```bash
# Update vcpkg to latest
cd vcpkg
git pull origin master
cd ..
git add vcpkg
git commit -m "chore(deps): update vcpkg to latest baseline"

# Initialize vcpkg for new developers
git submodule update --init --recursive
```

---

## � Deployment Workflow

### Build Artifacts
```bash
# Release build for distribution
mkdir build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Package for distribution
cpack .

# Generated files:
# - vulkanmon.exe (main executable)
# - assets/ (game assets)
# - shaders/ (compiled shaders)
# - README.txt (quick start guide)
```

### Continuous Integration
```yaml
# .github/workflows/ci.yml
name: CI

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [windows-latest, ubuntu-latest]

    runs-on: ${{ matrix.os }}

    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive

    - name: Setup Vulkan SDK
      uses: humbletim/setup-vulkan-sdk@v1.2.0
      with:
        vulkan-query-version: 1.3.268.0
        vulkan-components: Vulkan-Headers, Vulkan-Loader
        vulkan-use-cache: true

    - name: Build
      run: |
        mkdir build
        cd build
        cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
        cmake --build .

    - name: Test
      run: |
        cd build/tests_cpp
        ctest --output-on-failure

    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: vulkanmon-${{ matrix.os }}
        path: |
          build/Debug/vulkanmon*
          assets/
          shaders/
```

---

## � Performance Monitoring

### Development Metrics
```bash
# Build performance
time cmake --build .                # ~30 seconds incremental
time cmake --build . --parallel 8   # ~15 seconds with parallelism

# Runtime performance (60 FPS target)
./vulkanmon.exe --benchmark          # Built-in performance mode
# Expected output:
# Frame time: 16.67ms (60 FPS)
# ECS update: 0.8ms
# Spatial queries: 0.3ms
# Vulkan commands: 2.1ms
# Present: 1.2ms
```

### Automated Performance Regression Testing
```bash
# Performance CI job
./build/tests_cpp/Debug/vulkanmon_tests.exe "[Performance]" --reporter json:out=perf.json

# Compare against baseline
python scripts/compare_performance.py perf.json baseline.json
# Alert if performance degrades >10%
```

---

## � Development Best Practices

### Code Style Guidelines
```cpp
// Follow modern C++20 practices
class PokemonSystem {
public:
    // Use auto for type deduction
    auto findNearbyPokemon(const glm::vec3& position) -> std::vector<EntityID>;

    // Use const correctness
    [[nodiscard]] bool isPlayerInBattleRange(EntityID player) const;

    // Use RAII for resource management
    std::unique_ptr<BattleArena> createBattleArena();

private:
    // Use member initializer lists
    PokemonConfig config_{};
    std::shared_ptr<SpatialSystem> spatialSystem_{};
};
```

### Error Handling Strategy
```cpp
// Use exceptions for unrecoverable errors
if (!vulkanDevice) {
    throw std::runtime_error("Failed to create Vulkan device");
}

// Use return codes for expected failures
[[nodiscard]] bool PokemonSystem::attemptCapture(EntityID pokemon) {
    if (!isValidPokemon(pokemon)) {
        Logger::warning("Invalid Pokemon entity for capture");
        return false;
    }
    // Capture logic...
    return true;
}

// Use logging for debugging
Logger::debug("Processing {} Pokemon in detection range", pokemonCount);
```

### Testing Philosophy
```cpp
// Test behavior, not implementation
TEST_CASE("Pokemon should flee when player approaches too quickly") {
    // Setup scenario
    auto [player, pokemon] = createPlayerAndPokemon();

    // Simulate rapid approach
    movePlayerTowards(player, pokemon, /* speed */ 10.0f);

    // Verify behavior
    auto pokemonState = world.getComponent<Creature>(pokemon).state;
    REQUIRE(pokemonState == CreatureState::Fleeing);
}
```

The development workflow provides comprehensive tooling for efficient Pokemon Legends: Arceus-style game development with professional debugging, testing, and deployment capabilities.