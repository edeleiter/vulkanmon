# GitHub Workflows Documentation

This document provides comprehensive documentation for all GitHub Actions workflows in the VulkanMon project.

## Overview

VulkanMon uses GitHub Actions for automated cross-platform building, testing, and CI/CD. All workflows are located in `.github/workflows/` and follow the project's "Simple is Powerful" philosophy.

---

## build-and-test.yml

**File**: `.github/workflows/build-and-test.yml`  
**Purpose**: Cross-platform build and test automation for Windows and Linux  
**Triggers**: Push to `main`/`develop` branches, PRs to `main`  

### Architecture

- **Matrix Strategy**: Parallel builds on `ubuntu-latest` and `windows-latest`
- **Conditional Dependencies**: Linux system packages only install on Ubuntu
- **Platform-Specific Presets**: Uses CMakePresets.json for consistent builds
- **vcpkg Integration**: Cross-platform C++ dependency management

### Matrix Configuration

```yaml
strategy:
  matrix:
    include:
      - os: ubuntu-latest
        preset: ci-x64-linux
        test_exe: "./vulkanmon_tests"
      - os: windows-latest
        preset: ci-x64-windows
        test_exe: "Debug/vulkanmon_tests.exe"
```

### Step-by-Step Breakdown

#### Step 1: Checkout Repository
- **Action**: `actions/checkout@v4`
- **Purpose**: Downloads source code including vcpkg submodule
- **Platforms**: Both Linux and Windows

#### Step 2: Install System Dependencies (Linux Only)
- **Condition**: `if: matrix.os == 'ubuntu-latest'`
- **Purpose**: Install build tools and system libraries
- **Dependencies Installed**:
  - `build-essential` - GCC compiler, make, etc.
  - `cmake` - CMake build system
  - `ninja-build` - Ninja generator (faster than make)
  - `curl`, `zip`, `unzip`, `tar` - Archive utilities for vcpkg
  - `pkg-config` - Library configuration tool
  - **X11/GLFW Libraries**:
    - `libx11-dev` - X11 windowing system headers
    - `libxinerama-dev` - Multi-monitor support for GLFW
    - `libxcursor-dev` - Cursor management for GLFW
    - `libxi-dev` - Input extension for GLFW
    - `libxrandr-dev` - Display management for GLFW
    - `xorg-dev` - X11 development libraries
    - `libglu1-mesa-dev` - OpenGL utilities (Mesa implementation)

#### Step 3: Install Vulkan SDK (Linux Only)
- **Condition**: `if: matrix.os == 'ubuntu-latest'`
- **Purpose**: Install Vulkan development environment using Ubuntu system packages
- **Rationale**: More reliable than LunarG repository for CI environments
- **Dependencies Installed**:
  - `libvulkan-dev` - Vulkan headers and loader library
  - `vulkan-tools` - vulkaninfo, validation tools
  - `vulkan-utility-libraries-dev` - Vulkan utility libraries
  - `spirv-tools` - SPIR-V binary utilities
  - `glslang-tools` - GLSL to SPIR-V compiler (glslangValidator)
  - `mesa-vulkan-drivers` - Open-source Vulkan drivers
- **Environment Variables Set**:
  - `VULKAN_SDK=/usr` - For vcpkg compatibility
  - `VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d` - Vulkan validation layers

#### Step 4: Setup vcpkg
- **Action**: `lukka/run-vcpkg@v11`
- **Purpose**: Set up cross-platform C++ package manager
- **Configuration**:
  - `vcpkgDirectory`: Uses repo's vcpkg submodule
  - `vcpkgJsonGlob`: Uses `vcpkg.json` dependency manifest
- **Dependencies Managed**: GLFW, GLM, Assimp, STB, Catch2, Vulkan (Windows only)

#### Step 5: Configure and Build
- **Action**: `lukka/run-cmake@v10`
- **Purpose**: Use platform-specific presets defined in CMakePresets.json
- **Platform Presets**:
  - **Linux**: `ci-x64-linux` (Ninja generator)
  - **Windows**: `ci-x64-windows` (Visual Studio 2022)
- **Configuration**: `configurePreset: '${{ matrix.preset }}'`
- **Build**: `buildPreset: '${{ matrix.preset }}'`

#### Step 6: Execute Unit Test Suite
- **Working Directory**: `${{ github.workspace }}/build/tests_cpp`
- **Purpose**: Run comprehensive C++ unit tests using Catch2 framework
- **Test Coverage**: 10 test cases, 59 assertions
- **Platform-Specific Execution**:
  - **Windows**: `Debug/vulkanmon_tests.exe` (Visual Studio output structure)
  - **Linux**: `./vulkanmon_tests` (direct executable)
- **Catch2 Options**: `--reporter=console::out=-::colour-mode=ansi`
- **Error Handling**: Lists directory contents and exits with code 1 if executable not found

#### Step 7: Archive Build Artifacts
- **Action**: `actions/upload-artifact@v4`
- **Condition**: `if: always()` (runs even if previous steps fail)
- **Purpose**: Upload test executables, main application, and build logs for debugging
- **Artifact Names**: Platform-specific (`${{ matrix.os }}-test-results`)
- **Files Uploaded**:
  - `vulkanmon_tests*` - Test executables
  - `Debug/vulkanmon*` - Main application
  - `*.log` - Build logs
- **Retention**: 7 days

#### Step 8: Generate Build Summary
- **Condition**: `if: always()` (always runs to provide build information)
- **Purpose**: Create summary report visible in GitHub Actions UI
- **Information Included**:
  - Build Type (Debug)
  - Platform (ubuntu-latest/windows-latest)
  - CMake Preset used
  - Dependency management (vcpkg)
  - Test framework (Catch2)
  - Expected test counts

### Dependencies Overview

#### vcpkg-Managed Dependencies
- **GLFW3** - Cross-platform windowing and input
- **GLM** - OpenGL Mathematics library
- **Assimp** - 3D model loading library
- **STB** - Image loading library
- **Catch2** - C++ unit testing framework
- **Vulkan** - Graphics API (Windows only, Linux uses system packages)

#### Linux System Dependencies
- **Build Tools**: GCC, CMake, Ninja, pkg-config
- **X11/Windowing**: Complete X11 development stack for GLFW
- **Vulkan SDK**: Headers, tools, validation layers, SPIR-V compiler
- **Graphics Drivers**: Mesa Vulkan drivers for software rendering

#### Windows Dependencies
- **Visual Studio 2022** - Pre-installed on windows-latest runners
- **vcpkg** - Handles all C++ dependencies including Vulkan SDK

### Platform Differences

| Aspect | Linux (Ubuntu) | Windows |
|--------|----------------|---------|
| **Build System** | Ninja | Visual Studio 2022 |
| **Vulkan SDK** | System packages | vcpkg |
| **Dependencies** | apt + vcpkg hybrid | vcpkg only |
| **Test Executable** | `vulkanmon_tests` | `Debug/vulkanmon_tests.exe` |
| **Build Output** | Direct to build/ | Build/Debug/ structure |

### Troubleshooting

#### Common Issues
1. **vcpkg Bootstrap Failures**: Check vcpkg submodule initialization
2. **Vulkan SDK Not Found**: Verify environment variables are set correctly
3. **Test Executable Missing**: Check CMake configuration and build success
4. **Artifact Upload Failures**: Ensure paths don't contain relative components (`.` or `..`)

#### Debug Information
- Build logs are uploaded as artifacts for failed builds
- Test executables are preserved for manual testing
- GitHub Actions summary provides build configuration details

---

## Future Workflow Plans

### Planned Additions
1. **Release Workflow** - Automated releases with version tagging
2. **Performance Testing** - Benchmark regression detection
3. **Code Coverage** - Test coverage reporting
4. **Documentation Generation** - Automated API documentation
5. **Asset Validation** - Texture and model file validation
6. **Security Scanning** - Dependency vulnerability scanning

### Enhancement Ideas
1. **Matrix Expansion**: Add Release builds, different compiler versions
2. **Caching Optimization**: Cache vcpkg packages between runs
3. **Parallel Testing**: Split test suite for faster execution
4. **Cross-Platform Assets**: Test asset loading on different platforms
5. **GPU Testing**: Integration with GPU-enabled runners

---

## Maintenance Notes

### When to Update This Document
- Adding new GitHub workflows
- Modifying existing workflow steps
- Changing dependency management strategy
- Platform-specific requirement changes
- CMake preset modifications

### Version History
- **Initial Version**: Cross-platform build and test automation established
- **Documentation Created**: Comprehensive workflow documentation added

---

## Related Documentation
- [Cross-Platform Build Guide](CROSS_PLATFORM_BUILD_GUIDE.md) - Local development setup
- [Technical Notes](TECHNICAL_NOTES.md) - Implementation fixes and decisions
- `CMakePresets.json` - Platform-specific build configurations
- `vcpkg.json` - Dependency manifest and version control