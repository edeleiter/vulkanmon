# VulkanMon Build Instructions

## Initial Build Setup
```bash
# From the vulkanmon root directory:
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

## Running the Application
```bash
# From the build directory:
Debug/vulkanmon.exe
```

## Expected Output
When you run the application, you should see console output like:
```
Vulkan instance created successfully!
Window surface created successfully!
Selected GPU: NVIDIA GeForce RTX 5070 Ti
Logical device created successfully!
Swap chain created successfully with 3 images!
```

A black window titled "VulkanMon - Hello Triangle" will appear and stay open until you close it.

## Quick Rebuild (after source changes)
```bash
# From build directory:
cmake --build .
Debug/vulkanmon.exe
```

## Clean Rebuild (if needed)
```bash
# From root directory:
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
Debug/vulkanmon.exe
```

## Troubleshooting

### If CMake can't find Vulkan
- Make sure Vulkan SDK is installed and `vulkaninfo` works
- Check that `VULKAN_SDK` environment variable is set

### If vcpkg dependencies fail
- Make sure vcpkg is properly bootstrapped: `cd vcpkg && ./bootstrap-vcpkg.sh`
- Reinstall dependencies: `cd vcpkg && ./vcpkg install glfw3 glm`

**Note**: Our vcpkg directory is optimized - we exclude build artifacts (~108MB) while keeping essential files for easy contribution setup.

### If build fails with missing extensions
- Update your graphics drivers
- Verify GPU supports Vulkan: `vulkaninfo --summary`

## Development Workflow

### Following Our Core Tenants

#### 🎯 **"Simple is Powerful"**
- Build incrementally - get each step working before moving to the next
- Prefer clear, explicit code over clever optimizations
- One feature at a time

#### 🧪 **"Test, Test, Test"**  
- Build and test after every change
- Validate console output at each step  
- Visual confirmation of rendering results
- Test on your actual target hardware

#### 📚 **"Document Often"**
- Update progress in PROGRESS.md
- Comment complex Vulkan concepts
- Keep build instructions current

### Testing Checklist

#### Automated Testing (Recommended)
```bash
# Modern pytest-based testing
cd tests
pip install -r requirements.txt
python -m pytest

# Or run specific test categories
python -m pytest -m build        # Build system tests
python -m pytest -m vulkan       # Vulkan runtime tests  
python -m pytest -m integration  # End-to-end tests

# For more details, see tests/README.md
```

#### Manual Testing
```bash
# 1. Clean build test
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .

# 2. Console output validation
Debug/vulkanmon.exe
# Should show all systems initializing successfully

# 3. Visual validation  
# Triangle should render with RGB gradient
# Red (top), Blue (bottom-right), Green (bottom-left)

# 4. Performance check
# Should run smoothly at 60+ FPS
```

See [TESTING.md](TESTING.md) for detailed testing strategy and philosophy.

## Current Status
✅ **COMPLETE**: Vulkan Hello Triangle with vertex buffers - ALL 16 STEPS DONE!  
🎊 **Achievement**: Beautiful RGB gradient triangle rendering  
🚀 **Ready**: Phase 2 - 3D Graphics Foundation

---
*Keep this file updated as we add more build steps and features!*