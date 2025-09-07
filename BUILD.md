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

A window titled "VulkanMon - Hello Triangle" will appear showing:
- **Phase 1 Complete**: RGB gradient triangle (Red top, Blue bottom-right, Green bottom-left)  
- **Phase 2 Partial**: Additional horizontal gradient overlay from texture coordinates
- The triangle now has both vertex color gradients AND texture coordinate gradients combined

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

### If application crashes or segfaults
- Current implementation has texture loading code commented out due to stability issues
- If you uncommented texture loading functions, recomment them in `src/main.cpp`
- Look for lines containing `createTextureImage()` and `createDescriptorSetLayout()` - they should be commented out
- The working implementation only uses texture coordinates for gradient effects, not actual texture loading

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
✅ **Phase 1 COMPLETE**: Vulkan Hello Triangle with vertex buffers - ALL 16 STEPS DONE!  
🚧 **Phase 2 IN PROGRESS**: Step 22 (Texture System) partially implemented
- ✅ Texture coordinates added to vertex structure
- ✅ Shaders updated to handle texture coordinates  
- ✅ Visual gradient effect using texture coordinates working
- ❌ Actual texture loading/sampling not implemented (commented out due to segfaults)
🎊 **Current Visual**: RGB gradient triangle with texture coordinate-based gradient overlay

---
*Keep this file updated as we add more build steps and features!*