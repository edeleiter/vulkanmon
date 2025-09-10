# VulkanMon Build, Deploy & CI/CD Strategy

## Current State Analysis

### Build System Status
- **Local Development**: Windows + vcpkg (included in repo but unused)
- **CMakeLists.txt**: Uses system `find_package()` calls (no vcpkg integration)
- **CI Environment**: Attempting to use system packages on Ubuntu
- **Package Manager**: vcpkg directory exists but isn't integrated

### Key Issues Identified
1. **Inconsistent Dependency Management**: Local vs CI environments use different package sources
2. **Platform Fragmentation**: Windows (vcpkg) vs Linux (apt) vs macOS (brew)
3. **Missing vcpkg Integration**: vcpkg is present but not used by CMake
4. **No Release Pipeline**: Only debug builds, no deployment automation
5. **Limited Platform Coverage**: Only Ubuntu CI, no Windows/macOS testing

## Recommended Strategy: Multi-Platform vcpkg Integration

### Why vcpkg?
- **Cross-Platform Consistency**: Same dependencies across Windows/Linux/macOS
- **Version Pinning**: Reproducible builds with exact dependency versions
- **CI Integration**: GitHub Actions has excellent vcpkg support
- **C++ Ecosystem**: Best-in-class C++ package manager
- **Already Present**: vcpkg directory exists, just needs integration

## Implementation Plan

### Phase 1: vcpkg Integration Foundation
**Priority: Critical** | **Timeline: 1-2 days**

1. **Create vcpkg Manifest** (`vcpkg.json`)
   - Define all dependencies with version constraints
   - Replace system `find_package()` calls
   - Enable consistent cross-platform builds

2. **Update CMakeLists.txt**
   - Remove system package dependencies
   - Add vcpkg toolchain integration
   - Ensure builds work on Windows/Linux/macOS

3. **Fix CI Pipeline**
   - Integrate vcpkg with GitHub Actions
   - Use `run-vcpkg` action for dependency management
   - Remove system package installation

### Phase 2: Linux Development Environment Setup
**Priority: Critical** | **Timeline: 1 day**

1. **Linux Development Prerequisites**
   - Vulkan SDK installation on Linux
   - vcpkg bootstrap for Linux
   - Development toolchain setup (GCC/Clang)
   - VSCode/CLion configuration for Linux

2. **Cross-Platform vcpkg Configuration**
   - Ensure all dependencies have Linux support
   - Configure triplets for x64-linux
   - Test compilation on both Windows and Linux

3. **Development Workflow Validation**
   - Hot reload works on Linux
   - Shader compilation works on Linux  
   - All interactive controls work on Linux

### Phase 3: Multi-Platform CI Matrix  
**Priority: High** | **Timeline: 2-3 days**

1. **Expand CI Matrix**
   ```yaml
   strategy:
     matrix:
       os: [ubuntu-latest, windows-latest]
       build_type: [Debug, Release]
   ```

2. **Platform-Specific Optimizations**
   - Windows: Use MSVC + vcpkg
   - Linux: Use GCC/Clang + vcpkg
   - Future: macOS with MoltenVK

3. **Artifact Management**
   - Build artifacts per platform
   - Automated packaging (zip/tar)
   - Version tagging and releases

### Phase 3: Advanced CI/CD Pipeline
**Priority: Medium** | **Timeline: 3-4 days**

1. **Quality Gates**
   - Code coverage reporting
   - Static analysis (cppcheck, clang-tidy)
   - Performance regression tests
   - Security scanning

2. **Release Automation**
   - Semantic versioning
   - Automated changelog generation
   - GitHub Releases with binaries
   - Docker containerization

3. **Development Workflow**
   - Pre-commit hooks
   - Branch protection rules
   - Auto-merge for passing PRs
   - Notification integrations

### Phase 4: Deployment & Distribution
**Priority: Low** | **Timeline: 4-5 days**

1. **Distribution Channels**
   - Steam integration preparation
   - itch.io automated uploads
   - Self-hosted download portal

2. **Asset Pipeline**
   - Automated texture optimization
   - Shader compilation validation
   - Asset bundling and compression

3. **Monitoring & Analytics**
   - Crash reporting integration
   - Performance metrics collection
   - Usage analytics framework

## Technical Implementation Details

### vcpkg.json Structure
```json
{
  "name": "vulkanmon",
  "version": "0.1.0",
  "dependencies": [
    "vulkan",
    "glfw3",
    "glm",
    "assimp",
    "stb",
    "catch2"
  ],
  "builtin-baseline": "latest"
}
```

### CMake Integration
```cmake
# Enable vcpkg integration
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

# Dependencies will be automatically found
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Stb REQUIRED)
```

### CI Pipeline Structure
```yaml
jobs:
  build-test:
    strategy:
      matrix:
        include:
          - os: ubuntu-latest
            compiler: gcc
          - os: windows-latest  
            compiler: msvc
          - os: macos-latest
            compiler: clang
    
    steps:
    - uses: actions/checkout@v4
    - uses: lukka/run-vcpkg@v11
      with:
        vcpkgDirectory: '${{ github.workspace }}/vcpkg'
        vcpkgJsonGlob: 'vcpkg.json'
    - uses: lukka/run-cmake@v10
      with:
        configurePreset: 'ci-${{ matrix.os }}'
```

## Success Metrics

### Build Reliability
- **Target**: 100% build success rate across all platforms
- **Current**: ~70% (Linux only, dependency issues)

### Development Velocity  
- **Target**: <5 minute CI pipeline duration
- **Current**: ~8 minutes (single platform)

### Platform Coverage
- **Target**: Windows, Linux, macOS support
- **Current**: Linux CI only

### Release Automation
- **Target**: Fully automated releases with artifacts
- **Current**: Manual builds only

## Risk Mitigation

### Dependency Conflicts
- Pin all dependency versions in vcpkg.json
- Use vcpkg baseline commits for reproducibility
- Regular dependency audits and updates

### Build Environment Drift
- Containerized builds using Docker
- Matrix testing across OS versions
- Documented development environment setup

### CI Cost Management
- Optimize cache strategies
- Use matrix conditions to reduce redundant builds
- Monitor GitHub Actions usage

## Linux Development Environment Setup

### Prerequisites for Linux Development

#### 1. Vulkan SDK on Linux
```bash
# Ubuntu/Debian
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
sudo apt update
sudo apt install vulkan-sdk

# Arch Linux
sudo pacman -S vulkan-devel vulkan-tools vulkan-validation-layers

# Fedora
sudo dnf install vulkan-devel vulkan-tools vulkan-validation-layers
```

#### 2. vcpkg Bootstrap on Linux
```bash
cd /path/to/vulkanmon
./vcpkg/bootstrap-vcpkg.sh
```

#### 3. Development Tools
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake ninja-build gdb

# Arch Linux  
sudo pacman -S base-devel cmake ninja gdb

# Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake ninja-build gdb
```

#### 4. Graphics Drivers
```bash
# NVIDIA (proprietary)
sudo apt install nvidia-driver-535 nvidia-utils-535

# AMD (open source)
sudo apt install mesa-vulkan-drivers

# Intel (open source) 
sudo apt install mesa-vulkan-drivers intel-media-va-driver
```

### Cross-Platform Development Workflow

#### 1. VSCode Configuration
Create `.vscode/settings.json`:
```json
{
  "cmake.configureArgs": [
    "-DCMAKE_TOOLCHAIN_FILE=${workspaceFolder}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  ],
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.debugConfig": {
    "cwd": "${workspaceFolder}"
  }
}
```

#### 2. Build Commands (Cross-Platform)
```bash
# Windows (PowerShell)
mkdir build; cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --parallel

# Linux/macOS  
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --parallel $(nproc)
```

#### 3. Platform-Specific Considerations

**Linux-Specific:**
- X11/Wayland display server compatibility
- OpenGL fallback for older systems
- Mesa driver optimization flags
- Audio system (ALSA/PulseAudio/PipeWire)

**Windows-Specific:**  
- DirectX 12 interop potential
- Windows SDK dependency management
- Visual Studio debugger integration

## Next Steps

1. **Immediate**: Create vcpkg.json manifest with Linux compatibility validation
2. **Day 1**: Complete Phase 1 (vcpkg integration) + test Linux build
3. **Day 2**: Complete Phase 2 (Linux dev environment setup)  
4. **Week 1**: Implement Phase 3 (cross-platform CI)
5. **Month 1**: Phases 4+ based on project priorities

This strategy prioritizes getting you productive on both Windows and Linux immediately, while ensuring consistent builds across all future platforms.