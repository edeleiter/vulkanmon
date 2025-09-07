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

### If build fails with missing extensions
- Update your graphics drivers
- Verify GPU supports Vulkan: `vulkaninfo --summary`

## Current Status
✅ Vulkan instance, surface, device, and swap chain working  
🚧 Next: Render pass and triangle rendering implementation

---
*Keep this file updated as we add more build steps and features!*